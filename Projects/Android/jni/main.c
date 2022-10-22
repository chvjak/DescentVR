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

void game_loop(ovrApp* appState)
{
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

            // WTF?
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
                if (scene->Program.UniformLocation[UNIFORM_VIEW_ID] >=
                    0) // NOTE: will not be present when multiview path is enabled.
                {
                    GL(glUniform1i(scene->Program.UniformLocation[UNIFORM_VIEW_ID], eye));
                }

                // DRAW

                GL(glEnable(GL_SCISSOR_TEST));
                GL(glDepthMask(GL_TRUE));
                GL(glEnable(GL_DEPTH_TEST));
                GL(glDepthFunc(GL_LEQUAL));
                GL(glEnable(GL_CULL_FACE));
                GL(glCullFace(GL_BACK));
                GL(glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));
                GL(glScissor(0, 0, frameBuffer->Width, frameBuffer->Height));
                GL(glClearColor(0.125f, 0.0f, 0.125f, 1.0f));
                GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
                GL(glBindVertexArray(scene->Cube.VertexArrayObject));
                GL(glDrawElementsInstanced(
                    GL_TRIANGLES, scene->Cube.IndexCount, GL_UNSIGNED_SHORT, NULL, NUM_INSTANCES));
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

    game_loop(&appState);

#if MULTI_THREADED
    ovrRenderThread_Destroy(&appState.RenderThread);
#else
    ovrRenderer_Destroy(&appState.Renderer);
#endif

    ovrEgl_DestroyContext(&appState.Egl);

    vrapi_Shutdown();

    (*java.Vm)->DetachCurrentThread(java.Vm);
}
