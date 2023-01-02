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
#include "3d.h"

void game_loop(ovrApp* appState)
{
    // game.c::game(){GameLoop()}
    // GameLoop(){game_render_frame(); object_move_all()}
    //    object_move_all() {object_move_one( i );}
    //    render.c::render_frame() should happen here
    //       render.c:: render_mine(int start_seg_num, fix eye_offset)
    //         render_segment(i){render_side(k); do_render_object(j)}
    //           render_side(){render_face()}
    //             render_face(){g3_draw_tmap()}
    //               g3_draw_tmap(int nv, g3s_point **pointlist, g3s_uvl *uvl_list, grs_bitmap *bm)
    //           do_render_object(j){draw_polygon_object() or other types like powerup or morh(wtf?)}
    //             draw_polygon_object(){draw_polygon_model(...,obj->rtype.pobj_info.model_num,..)}
    //               draw_polygon_model(){g3_draw_polygon_model(void *model_ptr, grs_bitmap **model_bitmaps)} - for animation
    //                 g3_draw_polygon_model(){g3_draw_tmap(); or recurse for to draw back to front}
    //                   g3_draw_tmap() - as for walls

    //        Segment_points[] - walls, SEE ALSO: segment, segments.h TODO: figure out where loaded
    //        Vertices[] - also walls?
    // objects - all sorts of stuff, rendered as bitmaps, or polygonal models, SEE ALSO: objects.h
    // segment->objects - objects in the sector
    // objects.h::object - big struct
    // polyobj.h:: polymodel Polygon_models[] - models
    // polyobj.h::g3s_point robot_points[]; - actual model data?


    /*
    typedef struct {
        int Width;
        int Height;
        int Multisamples;
        int TextureSwapChainLength;
        int TextureSwapChainIndex;
        bool UseMultiview;
        ovrTextureSwapChain* ColorTextureSwapChain;
        GLuint* DepthBuffers;
        GLuint* FrameBuffers;
    } ovrFramebuffer;
    */

    while(true)
    {
            // This is the only place the frame index is incremented, right before
            // calling vrapi_GetPredictedDisplayTime().
            appState->FrameIndex++;

            // Get the HMD pose, predicted for the middle of the time period during which
            // the new eye images will be displayed. The number of frames predicted ahead
            // depends on the pipeline depth of the engine and the synthesis rate.
            // The better the prediction, the less black will be pulled in at the edges.
            const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(appState->Ovr, appState->FrameIndex);
            const ovrTracking2 tracking = vrapi_GetPredictedTracking2(appState->Ovr, predictedDisplayTime);

            // shortcuts
            ovrRenderer* renderer = &appState->Renderer;
            const ovrJava* java = &appState->Java;
            const ovrScene* scene = &appState->Scene;
            const ovrSimulation* simulation = &appState->Simulation;
            ovrMobile* ovr = appState->Ovr;
            ovrLayerProjection2 worldLayer = vrapi_DefaultLayerProjection2();

            ovrMatrix4f rotationMatrices[NUM_ROTATIONS];
            for (int i = 0; i < NUM_ROTATIONS; i++) {
                rotationMatrices[i] = ovrMatrix4f_CreateRotation(
                    scene->Rotations[i].x * simulation->CurrentRotation.x,
                    scene->Rotations[i].y * simulation->CurrentRotation.y,
                    scene->Rotations[i].z * simulation->CurrentRotation.z);
            }

            // Update the instance transform attributes.
            GL(glBindBuffer(GL_ARRAY_BUFFER, scene->InstanceTransformBuffer));
            GL(ovrMatrix4f* cubeTransforms = (ovrMatrix4f*)glMapBufferRange(
                   GL_ARRAY_BUFFER,
                   0,
                   NUM_INSTANCES * sizeof(ovrMatrix4f),
                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

            for (int i = 0; i < NUM_INSTANCES; i++) {
                const int index = scene->CubeRotations[i];

                // Write in order in case the mapped buffer lives on write-combined memory.
                cubeTransforms[i].M[0][0] = rotationMatrices[index].M[0][0];
                cubeTransforms[i].M[0][1] = rotationMatrices[index].M[0][1];
                cubeTransforms[i].M[0][2] = rotationMatrices[index].M[0][2];
                cubeTransforms[i].M[0][3] = rotationMatrices[index].M[0][3];

                cubeTransforms[i].M[1][0] = rotationMatrices[index].M[1][0];
                cubeTransforms[i].M[1][1] = rotationMatrices[index].M[1][1];
                cubeTransforms[i].M[1][2] = rotationMatrices[index].M[1][2];
                cubeTransforms[i].M[1][3] = rotationMatrices[index].M[1][3];

                cubeTransforms[i].M[2][0] = rotationMatrices[index].M[2][0];
                cubeTransforms[i].M[2][1] = rotationMatrices[index].M[2][1];
                cubeTransforms[i].M[2][2] = rotationMatrices[index].M[2][2];
                cubeTransforms[i].M[2][3] = rotationMatrices[index].M[2][3];

                cubeTransforms[i].M[3][0] = scene->CubePositions[i].x;
                cubeTransforms[i].M[3][1] = scene->CubePositions[i].y;
                cubeTransforms[i].M[3][2] = scene->CubePositions[i].z;
                cubeTransforms[i].M[3][3] = 1.0f;
            }
            GL(glUnmapBuffer(GL_ARRAY_BUFFER));
            GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

            ovrTracking2 updatedTracking = tracking;

            ovrMatrix4f eyeViewMatrixTransposed[2];
            eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose(&updatedTracking.Eye[0].ViewMatrix);
            eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose(&updatedTracking.Eye[1].ViewMatrix);

            ovrMatrix4f projectionMatrixTransposed[2];
            projectionMatrixTransposed[0] = ovrMatrix4f_Transpose(&updatedTracking.Eye[0].ProjectionMatrix);
            projectionMatrixTransposed[1] = ovrMatrix4f_Transpose(&updatedTracking.Eye[1].ProjectionMatrix);

            // Update the scene matrices.
            GL(glBindBuffer(GL_UNIFORM_BUFFER, scene->SceneMatrices));
            GL(ovrMatrix4f* sceneMatrices = (ovrMatrix4f*)glMapBufferRange(
                   GL_UNIFORM_BUFFER,
                   0,
                   2 * sizeof(ovrMatrix4f) /* 2 view matrices */ +
                       2 * sizeof(ovrMatrix4f) /* 2 projection matrices */,
                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

            if (sceneMatrices != NULL) {
                memcpy((char*)sceneMatrices, &eyeViewMatrixTransposed, 2 * sizeof(ovrMatrix4f));
                memcpy(
                    (char*)sceneMatrices + 2 * sizeof(ovrMatrix4f),
                    &projectionMatrixTransposed,
                    2 * sizeof(ovrMatrix4f));
            }

            GL(glUnmapBuffer(GL_UNIFORM_BUFFER));
            GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));

            ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
            worldLayer.HeadPose = updatedTracking.HeadPose;
            for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
                ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[renderer->NumBuffers == 1 ? 0 : eye];
                worldLayer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
                worldLayer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;
                worldLayer.Textures[eye].TexCoordsFromTanAngles =
                    ovrMatrix4f_TanAngleMatrixFromProjection(&updatedTracking.Eye[eye].ProjectionMatrix);
            }
            worldLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

            // Render the eye images.
            for (int eye = 0; eye < renderer->NumBuffers; eye++) {

                // PRE DRAW

                // NOTE: In the non-mv case, latency can be further reduced by updating the sensor
                // prediction for each eye (updates orientation, not position)
                ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[eye];
                ovrFramebuffer_SetCurrent(frameBuffer);

                GL(glUseProgram(scene->Program.Program));
                GL(glBindBufferBase(
                    GL_UNIFORM_BUFFER,
                    scene->Program.UniformBinding[UNIFORM_SCENE_MATRICES],
                    scene->SceneMatrices));
                if (scene->Program.UniformLocation[UNIFORM_VIEW_ID] >= 0) // NOTE: will not be present when multiview path is enabled.
                {
                    GL(glUniform1i(scene->Program.UniformLocation[UNIFORM_VIEW_ID], eye));
                }

                GL(glEnable(GL_SCISSOR_TEST));
                GL(glDepthMask(GL_TRUE));
                GL(glEnable(GL_DEPTH_TEST));
                GL(glDepthFunc(GL_LEQUAL));
                GL(glEnable(GL_CULL_FACE));
                GL(glCullFace(GL_BACK));
                GL(glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));
                GL(glScissor(0, 0, frameBuffer->Width, frameBuffer->Height));

                // DRAW
                GL(glClearColor(0.125f, 0.0f, 0.125f, 1.0f));
                GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
                GL(glBindVertexArray(scene->Cube.VertexArrayObject));
                GL(glDrawElementsInstanced(GL_TRIANGLES, scene->Cube.IndexCount, GL_UNSIGNED_SHORT, NULL, NUM_INSTANCES));

                GL(glBindVertexArray(0));
                GL(glUseProgram(0));

                // POST DRAW

                ovrFramebuffer_Resolve(frameBuffer);
                ovrFramebuffer_Advance(frameBuffer);
            }

            ovrFramebuffer_SetNone();

            const ovrLayerHeader2* layers[] = {&worldLayer.Header};

            ovrSubmitFrameDescription2 frameDesc = {0};
            frameDesc.Flags = 0;
            frameDesc.SwapInterval = appState->SwapInterval;
            frameDesc.FrameIndex = appState->FrameIndex;
            frameDesc.DisplayTime = appState->DisplayTime;
            frameDesc.LayerCount = 1;
            frameDesc.Layers = layers;

            // Hand over the eye images to the time warp.
            vrapi_SubmitFrame2(appState->Ovr, &frameDesc);
        }
    }

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
void android_main(struct android_app* app) {
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

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
        const double predictedDisplayTime =
                vrapi_GetPredictedDisplayTime(appState.Ovr, appState.FrameIndex);
        const ovrTracking2 tracking =
                vrapi_GetPredictedTracking2(appState.Ovr, predictedDisplayTime);

        appState.DisplayTime = predictedDisplayTime;

        // Advance the simulation based on the elapsed time since start of loop till predicted
        // display time.
        ovrSimulation_Advance(&appState.Simulation, predictedDisplayTime - startTime);

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

