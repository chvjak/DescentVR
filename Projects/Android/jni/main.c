#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#include "vr.h"
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
extern int allowed_to_fire_missile(void);

ovrVector3f cross_product(ovrVector3f *pF, ovrVector3f *pF1);

void android_main(struct android_app* app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");


    Asset_manager = app->activity->assetManager;

    ovrJava java;
    java.Vm = app->activity->vm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
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
        StartNewGame(1); // Start on level 1
        gr_palette_apply(gr_palette);

        timer_init();
        reset_time();
        FrameTime = 0;			//make first frame zero

        Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;

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

        // Advance the simulation based on the elapsed time since start of loop till predicted
        // display time.
        ovrSimulation_Advance(&appState.Simulation, predictedDisplayTime - startTime);

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

            // TODO: switch to c++ otherwise can't use op() overloads
            // TODO: add pos offset from tracking
            ConsoleObject->pos = (vms_vector) {fl2f(shipPosition.x), fl2f(shipPosition.y), fl2f(-shipPosition.z)};

            ovrMatrix4f mat = ovrMatrix4f_CreateFromQuaternion(&tracking.HeadPose.Pose.Orientation);

            ovrVector3f up = { mat.M[1][0], mat.M[1][1], mat.M[1][2] };
            ovrVector3f forward =      { mat.M[2][0], mat.M[2][1], mat.M[2][2] };
            ovrVector3f right =      { mat.M[0][0], mat.M[0][1], mat.M[0][2] };
            //ovrVector3f right = cross_product(&forward, &up);

            // TODO: fix dependes on orieantation vs world e.g - if facing against original z - up-down flipped
            ConsoleObject->orient.fvec = (vms_vector){ fl2f(forward.x), fl2f(forward.y), fl2f(forward.z), };
            ConsoleObject->orient.uvec = (vms_vector){ fl2f(up.x), fl2f(up.y), fl2f(up.z), };
            ConsoleObject->orient.rvec = (vms_vector){ fl2f(right.x), fl2f(right.y), fl2f(right.z), };

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

    (*java.Vm)->DetachCurrentThread(java.Vm);
}

ovrVector3f cross_product(ovrVector3f *a, ovrVector3f *b) {
    ovrVector3f result = {a->y * b->z - a->z * b->y,
                        -(a->x * b->z - a->z * b->x),
                          a->x * b->y - a->y * b->x};
    return result;
}

