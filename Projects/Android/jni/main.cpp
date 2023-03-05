#include <ctime>

extern "C"
{
#include <unistd.h>
#include <pthread.h>
}

#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/norm.hpp>

#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"

#include "vr.h"

extern "C"
{
#include "globvars.h"
#include "ai.h"
#include "effects.h"
#include "laser.h"
#include "text.h"
#include "menu.h"
#include "switch.h"
#include "wall.h"
#include <game.h>
#include <gamefont.h>
#include <bm.h>
#include <texmerge.h>
#include <mission.h>
#include <gameseq.h>
#include <palette.h>
#include <timer.h>
#include <3d.h>
}

#include <fcntl.h>
#include <errno.h>

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* app, int32_t cmd) {
    ovrApp* appState = (ovrApp*)app->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            ALOGV("onStart()");
            ALOGV("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            ALOGV("onResume()");
            ALOGV("    APP_CMD_RESUME");
            appState->Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            ALOGV("onPause()");
            ALOGV("    APP_CMD_PAUSE");
            appState->Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            ALOGV("onStop()");
            ALOGV("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            ALOGV("onDestroy()");
            ALOGV("    APP_CMD_DESTROY");
            appState->NativeWindow = NULL;
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            ALOGV("surfaceCreated()");
            ALOGV("    APP_CMD_INIT_WINDOW");
            appState->NativeWindow = app->window;
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            ALOGV("surfaceDestroyed()");
            ALOGV("    APP_CMD_TERM_WINDOW");
            appState->NativeWindow = NULL;
            break;
        }
    }
}

/**
* This is the main entry point of a native application that is using
* android_native_app_glue.  It runs in its own thread, with its own
* event loop for receiving input events and doing other things.
*/
extern AAssetManager* Asset_manager;
extern "C" int allowed_to_fire_missile(void);

ovrVector3f cross_product(ovrVector3f *pF, ovrVector3f *pF1);

bool allowed_to_change_level();

ovrVector3f up, forward, right;
char dataPath[256];

bool next_level = false;
bool prev_level = false;

int level = 1;

int MAX_LEVEL = 10; // TODO: figure out max level

fix next_level_change_time = 0;

void android_main(struct android_app* app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

    Asset_manager = app->activity->assetManager;

    ovrJava java;
    java.Vm = app->activity->vm;
    java.Vm->AttachCurrentThread(&java.Env, NULL);
    java.ActivityObject = app->activity->clazz;

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);

    const ovrInitParms initParms = vrapi_DefaultInitParms(&java);
    int32_t initResult = vrapi_Initialize(&initParms);
    if (initResult != VRAPI_INITIALIZE_SUCCESS) {
        // If intialization failed, vrapi_* function calls will not be available.
        exit(0);
    }

    ovrApp appState;
    ovrApp_Clear(&appState);
    appState.Java = java;

    ovrEgl_CreateContext(&appState.Egl, NULL);

    EglInitExtensions();

    GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    appState.UseMultiview &= glExtensions.multi_view;

    ALOGV("AppState UseMultiview : %d", appState.UseMultiview);

    appState.CpuLevel = CPU_LEVEL;
    appState.GpuLevel = GPU_LEVEL;
    appState.MainThreadTid = gettid();

#if MULTI_THREADED
    ovrRenderThread_Create(
        &appState.RenderThread, &appState.Java, &appState.Egl, appState.UseMultiview);
    // Also set the renderer thread to SCHED_FIFO.
    appState.RenderThreadTid = ovrRenderThread_GetTid(&appState.RenderThread);
#else
    ovrRenderer_Create(&appState.Renderer, &java, appState.UseMultiview);
#endif

    app->userData = &appState;
    app->onAppCmd = app_handle_cmd;

    const double startTime = GetTimeInSeconds();

    // Game init
    {
        const int SCR_WIDTH = 800;
        const int SCR_HEIGHT = 600; // TODO: remove this buffers

        game_init_render_buffers(SM_800x600V15, SCR_WIDTH, SCR_HEIGHT, 1, 0, 1);

        int t = gr_init(SCR_WIDTH, SCR_HEIGHT);

        gr_use_palette_table((char *) "PALETTE.256");
        gamefont_init(); // must be loaded aftr pallette
        load_text();
        bm_init();
        g3_init();
        texmerge_init(100);        // 10 cache bitmaps

        init_game();
        set_detail_level_parameters(NUM_DETAIL_LEVELS - 2); // #define	NUM_DETAIL_LEVELS	6 , //	Note: Highest detail level (detail_level == NUM_DETAIL_LEVELS-1) is custom detail level.
        load_mission(0);
        StartNewGame(level); // Start on level 1
        gr_palette_apply(gr_palette);

        timer_init();
        reset_time();
        FrameTime = 0;			//make first frame zero

        Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;

        digi_init();
    }

    while (app->destroyRequested == 0) {
        // Read all pending events.
        for (;;) {
            int events;
            struct android_poll_source* source;
            const int timeoutMilliseconds =
                    (appState.Ovr == NULL && app->destroyRequested == 0) ? -1 : 0;
            if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void**)&source) < 0) {
                break;
            }

            // Process this event.
            if (source != NULL) {
                source->process(app, source);
            }

            ovrApp_HandleVrModeChanges(&appState);
        }

        // We must read from the event queue with regular frequency.
        ovrApp_HandleVrApiEvents(&appState);

        ovrApp_HandleInput(&appState);

        if (appState.Ovr == NULL) {
            continue;
        }

        // Create the scene if not yet created.
        // The scene is created here to be able to show a loading icon.
        if (!ovrScene_IsCreated(&appState.Scene)) {
#if MULTI_THREADED
            // Show a loading icon.
            ovrRenderThread_Submit(
                &appState.RenderThread,
                appState.Ovr,
                RENDER_LOADING_ICON,
                appState.FrameIndex,
                appState.DisplayTime,
                appState.SwapInterval,
                NULL,
                NULL,
                NULL);
#else
            // Show a loading icon.
            int frameFlags = 0;
            frameFlags |= VRAPI_FRAME_FLAG_FLUSH;

            ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
            blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

            ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
            iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;

            const ovrLayerHeader2* layers[] = {
                    &blackLayer.Header,
                    &iconLayer.Header,
            };

            ovrSubmitFrameDescription2 frameDesc = {0};
            frameDesc.Flags = frameFlags;
            frameDesc.SwapInterval = 1;
            frameDesc.FrameIndex = appState.FrameIndex;
            frameDesc.DisplayTime = appState.DisplayTime;
            frameDesc.LayerCount = 2;
            frameDesc.Layers = layers;

            vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
#endif

            // Create the scene.
            ovrScene_Create(&appState.Scene, appState.UseMultiview);
        }

        // This is the only place the frame index is incremented, right before
        // calling vrapi_GetPredictedDisplayTime().
        appState.FrameIndex++;

        // Get the HMD pose, predicted for the middle of the time period during which
        // the new eye images will be displayed. The number of frames predicted ahead
        // depends on the pipeline depth of the engine and the synthesis rate.
        // The better the prediction, the less black will be pulled in at the edges.
        const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
        const ovrTracking2 tracking = vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);

        appState.DisplayTime = predictedDisplayTime;

        {
            // update game state

            calc_frame_time();
            GameTime += FrameTime;

            do_ai_frame_all();
            object_move_all();
            do_special_effects();
            wall_frame_process();
            triggers_frame_process();
            update_object_seg(ConsoleObject);

            ConsoleObject->pos = (vms_vector) {fl2f(shipPosition.x), fl2f(shipPosition.y), fl2f(-shipPosition.z)};

            ovrMatrix4f mat = ovrMatrix4f_CreateFromQuaternion(&tracking.HeadPose.Pose.Orientation);

            glm::vec3 cameraFront(-mat.M[0][2], -mat.M[1][2], -mat.M[2][2]);
            glm::vec3 cameraUp(mat.M[0][1], mat.M[1][1], mat.M[2][1]);
            glm::vec3 cameraRight(mat.M[0][0], mat.M[1][0], mat.M[2][0]);

            forward = {cameraFront.x, cameraFront.y, cameraFront.z};
            up = {cameraUp.x, cameraUp.y, cameraUp.z};
            right = {cameraRight.x, cameraRight.y, cameraRight.z};

            glm::vec3 cameraNegFront(-mat.M[0][2], -mat.M[1][2], mat.M[2][2]); // -z
            glm::vec3 cameraNegUp(mat.M[0][1], mat.M[1][1], -mat.M[2][1]); // -z
            glm::vec3 cameraNegRight = -glm::cross(cameraNegFront, cameraNegUp);

            ConsoleObject->orient.fvec = { fl2f(cameraNegFront.x), fl2f(cameraNegFront.y), fl2f(cameraNegFront.z), };
            ConsoleObject->orient.uvec = { fl2f(cameraNegUp.x), fl2f(cameraNegUp.y), fl2f(cameraNegUp.z), };
            ConsoleObject->orient.rvec = { fl2f(cameraNegRight.x), fl2f(cameraNegRight.y), fl2f(cameraNegRight.z), };

            if (fire_secondary)
            {
                Secondary_weapon = HOMING_INDEX;
                Players[Player_num].secondary_ammo[Secondary_weapon] = 10;

                if (allowed_to_fire_missile())
                    do_missile_firing();
                fire_secondary = false;
            }

            if (fire_primary)
            {
                do_laser_firing_player();
                fire_primary = false;
            }

            if(next_level && allowed_to_change_level())
            {
                next_level = false;
                level = (level + 1) % MAX_LEVEL;
                next_level_change_time = GameTime + 1000;
                StartNewGame(level); // Start on level 1

                shipPosition.x = 0;
                shipPosition.y = 0;
                shipPosition.z = 0;
            }

            if(prev_level && allowed_to_change_level())
            {
                prev_level = false;
                level = (level - 1) % MAX_LEVEL;
                next_level_change_time = GameTime + 1000;
                StartNewGame(level); // Start on level 1

                shipPosition.x = 0;
                shipPosition.y = 0;
                shipPosition.z = 0;

                // seems levels have their starts in different points, e.g 2 and 3 are far off, but the rest - around 0,0,0 but still a bit off
            }


        }

#if MULTI_THREADED
        // Render the eye images on a separate thread.
        ovrRenderThread_Submit(
            &appState.RenderThread,
            appState.Ovr,
            RENDER_FRAME,
            appState.FrameIndex,
            appState.DisplayTime,
            appState.SwapInterval,
            &appState.Scene,
            &appState.Simulation,
            &tracking);
#else

        // Render eye images and setup the primary layer using ovrTracking2.
        const ovrLayerProjection2 worldLayer = ovrRenderer_RenderFrame(
                &appState.Renderer,
                &appState.Java,
                &appState.Scene,
                &appState.Simulation,
                &tracking,
                appState.Ovr);

        const ovrLayerHeader2* layers[] = {&worldLayer.Header};

        ovrSubmitFrameDescription2 frameDesc = {0};
        frameDesc.Flags = 0;
        frameDesc.SwapInterval = appState.SwapInterval;
        frameDesc.FrameIndex = appState.FrameIndex;
        frameDesc.DisplayTime = appState.DisplayTime;
        frameDesc.LayerCount = 1;
        frameDesc.Layers = layers;

        // Hand over the eye images to the time warp.
        vrapi_SubmitFrame2(appState.Ovr, &frameDesc);
#endif
    }

#if MULTI_THREADED
    ovrRenderThread_Destroy(&appState.RenderThread);
#else
    ovrRenderer_Destroy(&appState.Renderer);
#endif

    ovrEgl_DestroyContext(&appState.Egl);

    vrapi_Shutdown();

    java.Vm->DetachCurrentThread();
}

bool allowed_to_change_level() {
    return (next_level_change_time < GameTime);

}

ovrVector3f cross_product(ovrVector3f *a, ovrVector3f *b) {
    ovrVector3f result = {a->y * b->z - a->z * b->y,
                          -(a->x * b->z - a->z * b->x),
                          a->x * b->y - a->y * b->x};
    return result;
}

extern "C"
{
void draw_with_texture(int nv, GLfloat* vertices, GLfloat* tex_coords, GLfloat* colors, GLint texture_slot_id);
void ogles_bm_bind_teximage_2d_with_max_alpha(grs_bitmap* bm, ubyte max_alpha);
bool g3_draw_bitmap_ogles(g3s_point *pos, fix width, fix height, grs_bitmap *bm) {
    g3s_point pnt;
    fix w, h;

    w = fixmul(width, Matrix_scale.x) * 2;
    h = fixmul(height, Matrix_scale.y) * 2;

    GLfloat x0f, y0f, x1f, y1f, zf;

    // Calculate OGLES coords
    x0f = f2fl(pos->x - w / 2);
    y0f = f2fl(pos->y - h / 2);
    x1f = f2fl(pos->x + w / 2);
    y1f = f2fl(pos->y + h / 2);
    zf = -f2fl(pos->z);

    // Looks like if cameraFront matches world-z - it's fine, than when it is 45 degree off - bitmap is perpendicular to what is needed and when it is 90 degree it is correct again
    // looks like the angle of compensation rotation grows at x2 rate

    glm::vec3 cameraFront(forward.x, forward.y, forward.z);
    glm::vec3 pos1(f2fl(pos->x), f2fl(pos->y), -f2fl(pos->z));
    glm::vec3 originalNormal(0.0f, 0.0f, 1.0f); // TODO: calculate normal, deal with 180 degree rotation
    float norm1 = glm::l2Norm(cameraFront);
    float norm2 = glm::l2Norm(originalNormal);
    float angle = acos(glm::dot(originalNormal, cameraFront) / norm1 / norm2);

    glm::vec3 axis = glm::cross(originalNormal, cameraFront);
    axis /= glm::l2Norm(axis);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos1);
    model = glm::rotate(model, angle, axis);
    model = glm::translate(model, -pos1);

    glm::vec4 verts[] = { glm::vec4(x1f, y0f, zf, 1), glm::vec4(x0f, y0f, zf, 1), glm::vec4(x0f, y1f, zf, 1), glm::vec4(x1f, y1f, zf, 1) };
    GLfloat vertices[4];

    for(int i = 0; i < 4; i++)
    {
        verts[i] = model * verts[i] ;

        vertices[i * 3] = verts[i].x;
        vertices[i * 3 + 1] = verts[i].y;
        vertices[i * 3 + 2] = verts[i].z;
    }

    // Draw
    GLfloat texCoords[] = { 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f };
    GLfloat colors[] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, };

    ogles_bm_bind_teximage_2d_with_max_alpha(bm, 128);
    draw_with_texture(4, vertices, texCoords, colors, bm->bm_ogles_tex_id);

    return 0;

}

}