#include <ctime>
#include <unistd.h>
#include <pthread.h>

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
#include "fireball.h"
#include "endlevel.h"

}

#include <fcntl.h>
#include <errno.h>

#include <fluidsynth.h>
#include <dirent.h>

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

glm::vec3 up, forward, right;
glm::vec3 shipPosition;
int fire_secondary;
int fire_primary;

bool next_level = false;
bool prev_level = false;

int level = 1;

int MAX_LEVEL = MAX_LEVELS_PER_MISSION;

fix next_level_change_time = 0;
bool allowed_to_change_level() {
    return (next_level_change_time < GameTime);
}

bool next_primary_weapon = false;
bool next_secondary_weapon = false;

fix next_weapon_change_time = 0;
bool allowed_to_change_weapon() {
    return (next_weapon_change_time < GameTime);
}

fix DELAY = F1_0;

int try_switch_level(int , int);
void try_switch_weapon(int secondary_flag);

void StartLevel(int level)
{
    StartNewGame(level);

    Players[Player_num].flags |= PLAYER_FLAGS_INVULNERABLE;
    Players[Player_num].primary_weapon_flags = 0xff;
    Players[Player_num].secondary_weapon_flags = 0xff;

    ConsoleObject->mtype.phys_info.flags &= ~(PF_WIGGLE);

    for (int i = 0; i<MAX_PRIMARY_WEAPONS; i++)
        Players[Player_num].primary_ammo[i] = Primary_ammo_max[i];

    for (int i = 0; i<MAX_SECONDARY_WEAPONS; i++)
        Players[Player_num].secondary_ammo[i] = Secondary_ammo_max[i];


    switch(level)
    {
        case 2:
            shipPosition.x = -40;
            shipPosition.y = -15;
            shipPosition.z = -830;
            break;

        case 3:
            shipPosition.x = -550;
            shipPosition.y = 313;
            shipPosition.z = -455;
            break;

        case 4:
            shipPosition.x = 110;
            shipPosition.y = -60;
            shipPosition.z = 200;
            break;


        case 5:
            shipPosition.x = 0;
            shipPosition.y = -100;
            shipPosition.z = -90;
            break;

        case 6:
            shipPosition.x = 0;
            shipPosition.y = -20;
            shipPosition.z = 0;
            break;

        case 7:
            shipPosition.x = -70;
            shipPosition.y = -20;
            shipPosition.z = -40;
            break;

        case 10:
            shipPosition.x = 0;
            shipPosition.y = 0;
            shipPosition.z = -180;
            break;
        case 17:
            shipPosition.x = 150;
            shipPosition.y = -120;
            shipPosition.z = -200;
            break;

        default:
            shipPosition.x = f2fl(Player_init->pos.x);
            shipPosition.y = f2fl(Player_init->pos.y);
            shipPosition.z = f2fl(Player_init->pos.x);

    }

}

fluid_settings_t* settings;
fluid_synth_t* synth;
fluid_player_t* midi_player;
fluid_audio_driver_t* adriver;

void CreateMidiPlayer()
{
    midi_player = new_fluid_player(synth);
    fluid_player_set_loop(midi_player, -1); // doesn't loop infinitely for some reason
}

void InitMusic(const char* soundfont_path_name)
{
    settings = new_fluid_settings();

    fluid_settings_setstr(settings, "player.reset-synth", "1");
    fluid_settings_setstr(settings, "synth.gain", "1.6");

    synth = new_fluid_synth(settings);
    fluid_synth_sfload(synth, soundfont_path_name, 1);
    adriver = new_fluid_audio_driver(settings, synth);

    CreateMidiPlayer();
}

extern "C"
void StartMusic(const char* MIDIFILE, int FILELEN)
{
    fluid_player_stop(midi_player);

    int status = fluid_player_get_status(midi_player);
    while (status == FLUID_PLAYER_PLAYING || status == FLUID_PLAYER_STOPPING)
    {
        status = fluid_player_get_status(midi_player);
    }
    sleep(1);

    delete_fluid_player	(midi_player);


    CreateMidiPlayer();

    fluid_player_add_mem(midi_player, MIDIFILE, FILELEN);
    fluid_player_play(midi_player);
}

extern int digi_volume;

void ls(const char * path) {
    DIR *d;
    struct dirent *dir;
    d = opendir(path);
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            ALOGV("%s\n", dir->d_name);
        }
        closedir(d);
    }
}
void android_main(struct android_app* app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");


    Asset_manager = app->activity->assetManager;

    ovrJava java;
    java.Vm = app->activity->vm;
    java.Vm->AttachCurrentThread(&java.Env, NULL);
    java.ActivityObject = app->activity->clazz;

    chdir("/sdcard/DescentVR");
    InitMusic("merlin_silver.sf2");

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
        const int SCR_WIDTH = 1440;
        const int SCR_HEIGHT = 1584;

        game_init_render_buffers(SM_800x600V15, SCR_WIDTH, SCR_HEIGHT, 1, 0, 1);

        int t = gr_init(SCR_WIDTH, SCR_HEIGHT);

        gr_use_palette_table((char *) "PALETTE.256");
        gamefont_init(); // must be loaded aftr pallette
        load_text();
        bm_init();
        g3_init();
        texmerge_init(1000);

        init_game();
        set_detail_level_parameters(NUM_DETAIL_LEVELS - 2); // #define	NUM_DETAIL_LEVELS	6 , //	Note: Highest detail level (detail_level == NUM_DETAIL_LEVELS-1) is custom detail level.
        load_mission(0);
        StartLevel(level);
        gr_palette_apply(gr_palette);

        timer_init();
        reset_time();
        FrameTime = 0;

        digi_init();
        digi_set_digi_volume( (32768)/2 );
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

            if (Endlevel_sequence) {
                do_endlevel_frame();
                do_special_effects();

                if(Endlevel_sequence == 4) // EL_STOPPED
                {
                    next_level = true;
                    Endlevel_sequence = 0;
                }
            }
            else {
                do_ai_frame_all();
                object_move_all();
                do_exploding_wall_frame();
                do_special_effects();
                wall_frame_process();
                triggers_frame_process();
                update_object_seg(ConsoleObject);
            }

            shipPosition = { f2fl(ConsoleObject->pos.x), f2fl(ConsoleObject->pos.y), -f2fl(ConsoleObject->pos.z) };

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

            if(next_level)
            {
                next_level = false;
                level = try_switch_level(level, level + 1);
            }

            if(prev_level)
            {
                prev_level = false;
                level = try_switch_level(level, level - 1);
            }

            if(next_primary_weapon) {
                next_primary_weapon = false;
                try_switch_weapon(0);
            }

            if(next_secondary_weapon) {
                next_secondary_weapon = false;
                try_switch_weapon(1);
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

int try_switch_level(int old_level, int level1) {
    if(allowed_to_change_level())
    {
        level1 = level1 % MAX_LEVEL;
        if (level1 == 0) level1 = 1;
        next_level_change_time = GameTime + DELAY * 2;

        ALOGV("StartLevel(%d);", level1);
        StartLevel(level1);

        return level1;
    }
    return old_level;
}

void try_switch_weapon(int secondary_flag) {
    if(allowed_to_change_weapon()) {
        next_weapon_change_time =  GameTime + DELAY;
        do_weapon_select(secondary_flag);
    }
}


