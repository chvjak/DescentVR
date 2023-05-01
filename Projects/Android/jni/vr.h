#ifndef vr_h
#define vr_h
/************************************************************************************

Filename    :   VrCubeWorld_NativeActivity.c
Content     :   This sample uses the Android NativeActivity class. This sample does
                not use the application framework.
                This sample only uses the VrApi.
Created     :   March, 2015
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#include <ctime>
extern "C"
{
#include <unistd.h>
#include <pthread.h>
}

#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif


#if !defined(GL_EXT_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)(
    GLenum target,
    GLsizei samples,
    GLenum internalformat,
    GLsizei width,
    GLsizei height);
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)(
    GLenum target,
    GLenum attachment,
    GLenum textarget,
    GLuint texture,
    GLint level,
    GLsizei samples);
#endif

#if !defined(GL_OVR_multiview)
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR = 0x9630;
static const int GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR = 0x9632;
static const int GL_MAX_VIEWS_OVR = 0x9631;
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#if !defined(GL_OVR_multiview_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLsizei samples,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"

#define DEBUG 1
#define OVR_LOG_TAG "Descent"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif

static const int CPU_LEVEL = 2;
static const int GPU_LEVEL = 3;
static const int NUM_MULTI_SAMPLES = 4;

//#define MULTI_THREADED 0

/*
================================================================================

System Clock Time

================================================================================
*/

double GetTimeInSeconds() ;

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

typedef struct {
    int multi_view; // GL_OVR_multiview, GL_OVR_multiview2
    int EXT_texture_border_clamp; // GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
} OpenGLExtensions_t;

extern OpenGLExtensions_t glExtensions;

void EglInitExtensions();

static const char* EglErrorString(const EGLint error);

static const char* GlFrameBufferStatusString(GLenum status) ;

#ifdef CHECK_GL_ERRORS

static const char* GlErrorString(GLenum error);

void GLCheckErrors(int line);

#define GL(func) \
    func;        \
    GLCheckErrors(__LINE__);

#else // CHECK_GL_ERRORS

#define GL(func) func;

#endif // CHECK_GL_ERRORS

/*
================================================================================

ovrEgl

================================================================================
*/

typedef struct {
    EGLint MajorVersion;
    EGLint MinorVersion;
    EGLDisplay Display;
    EGLConfig Config;
    EGLSurface TinySurface;
    EGLSurface MainSurface;
    EGLContext Context;
} ovrEgl;

void ovrEgl_Clear(ovrEgl* egl);
void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl) ;
void ovrEgl_DestroyContext(ovrEgl* egl);

/*
================================================================================

ovrProgram

================================================================================
*/

#define MAX_PROGRAM_UNIFORMS 8
#define MAX_PROGRAM_TEXTURES 8

typedef struct {
    GLuint Program;
    GLuint VertexShader;
    GLuint FragmentShader;
    // These will be -1 if not used by the program.
    GLint UniformLocation[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint UniformBinding[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint Textures[MAX_PROGRAM_TEXTURES]; // Texture%i
} ovrProgram;

typedef enum {
    UNIFORM_VIEW_ID,
    UNIFORM_SCENE_MATRICES,
} index_enum;

typedef enum {
    UNIFORM_TYPE_VECTOR4,
    UNIFORM_TYPE_MATRIX4X4,
    UNIFORM_TYPE_INT,
    UNIFORM_TYPE_BUFFER,
} type_enum;

typedef struct {
    index_enum index;
    type_enum type;
    const char* name;
} ovrUniform;

static ovrUniform ProgramUniforms[] = {
    {UNIFORM_VIEW_ID, UNIFORM_TYPE_INT, "ViewID"},
    {UNIFORM_SCENE_MATRICES, UNIFORM_TYPE_BUFFER, "SceneMatrices"},
};

void ovrProgram_Clear(ovrProgram* program);

static const char* programVersion = "#version 300 es\n";

static int ovrProgram_Create(
    ovrProgram* program,
    const char* vertexSource,
    const char* fragmentSource,
    const int useMultiview) ;

void ovrProgram_Destroy(ovrProgram* program);

static const char VERTEX_SHADER[] =
"#ifndef DISABLE_MULTIVIEW\n\
#define DISABLE_MULTIVIEW 0\n\
#endif\n\
#define NUM_VIEWS 2\n\
#if defined( GL_OVR_multiview2 ) && ! DISABLE_MULTIVIEW\n\
	#extension GL_OVR_multiview2 : enable\n\
	layout(num_views=NUM_VIEWS) in;\n\
	#define VIEW_ID gl_ViewID_OVR\n\
#else\n\
	uniform lowp int ViewID;\n\
	#define VIEW_ID ViewID\n\
#endif\n\
in vec3 vertexPosition;\n\
in vec3 vertexColor;\n\
in vec2 vTexCoords;\n\
uniform SceneMatrices\n\
{\n\
	uniform mat4 ViewMatrix[NUM_VIEWS];\n\
	uniform mat4 ProjectionMatrix[NUM_VIEWS];\n\
} sm;\n\
out vec2 aTexCoords;\n\
out vec3 aColors;\n\
 \n\
void main()\n\
{\n\
    gl_Position = sm.ProjectionMatrix[VIEW_ID] * ( sm.ViewMatrix[VIEW_ID] * ( vec4( vertexPosition, 1.0 ) ) );\n\
	aTexCoords = vTexCoords;\n\
	aColors = vertexColor;\n\
}\n";

static const char FRAGMENT_SHADER[] =
"in vec2 aTexCoords;\n\
in vec3 aColors;\n\
\n\
uniform sampler2D texSampler;\n\
\n\
out lowp vec4 outColor;\n\
void main()\n\
{\n\
    if(aTexCoords.x == -1.f && aTexCoords.y == -1.f)\
       outColor = vec4(aColors, 1.f);\n\
    else\
    {\
        vec4 sample1 = texture(texSampler, aTexCoords);\n\
        if (sample1.a == 0.0f) {\n\
            discard;\n\
        } else {\n\
            outColor = mix(sample1, vec4(aColors, 1.f), 0.5);\n\
        }\n\
    }\
}";

static const char* vertex_shader_with_tex_text =
        R"(#version 300 es
#extension GL_OVR_multiview2 : enable
layout(num_views=2) in;

in vec3 vPos;
in vec3 vColors;
in vec2 vTexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 aTexCoords;
out vec3 aColors;

void main()
{
    gl_Position =  projection * view * model * vec4(vPos * 0.4, 1.0) ;
    aTexCoords = vTexCoords;
    aColors = vColors;
})";

static const char* fragment_shader_with_tex_text =
        R"(#version 300 es
in vec3 aColors;
in vec2 aTexCoords;

uniform sampler2D texSampler;

out lowp vec4 outColor;
void main()
{
    vec4 sample1 = texture(texSampler, aTexCoords);
    if (sample1.a == 0.0f) {
        outColor = sample1;
    } else {
        outColor = mix(sample1, vec4(aColors, 1.f), 0.5);
    }
})";

/*
================================================================================

ovrFramebuffer

================================================================================
*/

typedef struct {
    int Width;
    int Height;
    int Multisamples;
    int TextureSwapChainLength;
    int TextureSwapChainIndex;
    int UseMultiview;
    ovrTextureSwapChain* ColorTextureSwapChain;
    GLuint* DepthBuffers;
    GLuint* FrameBuffers;
} ovrFramebuffer;

 void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer);
 int ovrFramebuffer_Create(
    ovrFramebuffer* frameBuffer,
    const int useMultiview,
    const GLenum colorFormat,
    const int width,
    const int height,
    const int multisamples) ;
 void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer) ;
 void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer) ;
 void ovrFramebuffer_SetNone();

 void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer) ;

 void ovrFramebuffer_Advance(ovrFramebuffer* frameBuffer) ;

/*
================================================================================

ovrScene

 // Seems not needed
================================================================================
*/

#define NUM_INSTANCES 1
#define NUM_ROTATIONS 1

typedef struct {
    int CreatedScene;
    int CreatedVAOs;
    unsigned int Random;
    ovrProgram Program;
    GLuint SceneMatrices;
    ovrVector3f Rotations[NUM_ROTATIONS];
    ovrVector3f CubePositions[NUM_INSTANCES];
    int CubeRotations[NUM_INSTANCES];
} ovrScene;

void ovrScene_Clear(ovrScene* scene);

int ovrScene_IsCreated(ovrScene* scene);

void ovrScene_Create(ovrScene* scene, int useMultiview) ;

void ovrScene_Destroy(ovrScene* scene);

/*
================================================================================

ovrRenderer

================================================================================
*/

typedef struct {
    ovrFramebuffer FrameBuffer[VRAPI_FRAME_LAYER_EYE_MAX];
    int NumBuffers;
} ovrRenderer;

void ovrRenderer_Clear(ovrRenderer* renderer);

void ovrRenderer_Create(ovrRenderer* renderer, const ovrJava* java, const int useMultiview) ;

void ovrRenderer_Destroy(ovrRenderer* renderer);

ovrLayerProjection2 ovrRenderer_RenderFrame(
    ovrRenderer* renderer,
    const ovrJava* java,
    const ovrScene* scene,
    const ovrTracking2* tracking,
    ovrMobile* ovr) ;

/*
================================================================================

ovrRenderThread

================================================================================
*/

#if MULTI_THREADED

typedef enum { RENDER_FRAME, RENDER_LOADING_ICON } ovrRenderType;

typedef struct {
    JavaVM* JavaVm;
    jobject ActivityObject;
    const ovrEgl* ShareEgl;
    pthread_t Thread;
    int Tid;
    int UseMultiview;
    // Synchronization
    int Exit;
    int WorkAvailableFlag;
    int WorkDoneFlag;
    pthread_cond_t WorkAvailableCondition;
    pthread_cond_t WorkDoneCondition;
    pthread_mutex_t Mutex;
    // Latched data for rendering.
    ovrMobile* Ovr;
    ovrRenderType RenderType;
    long long FrameIndex;
    double DisplayTime;
    int SwapInterval;
    ovrScene* Scene;
    ovrSimulation Simulation;
    ovrTracking2 Tracking;
} ovrRenderThread;

void* RenderThreadFunction(void* parm) {
    ovrRenderThread* renderThread = (ovrRenderThread*)parm;
    renderThread->Tid = gettid();

    ovrJava java;
    java.Vm = renderThread->JavaVm;
    (*java.Vm)->AttachCurrentThread(java.Vm, &java.Env, NULL);
    java.ActivityObject = renderThread->ActivityObject;

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"OVR::Renderer", 0, 0, 0);

    ovrEgl egl;
    ovrEgl_CreateContext(&egl, renderThread->ShareEgl);

    GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));

    ovrRenderer renderer;
    ovrRenderer_Create(&renderer, &java, renderThread->UseMultiview);

    ovrScene* lastScene = NULL;

    for (;;) {
        // Signal work completed.
        pthread_mutex_lock(&renderThread->Mutex);
        renderThread->WorkDoneFlag = true;
        pthread_cond_signal(&renderThread->WorkDoneCondition);
        pthread_mutex_unlock(&renderThread->Mutex);

        // Wait for work.
        pthread_mutex_lock(&renderThread->Mutex);
        while (!renderThread->WorkAvailableFlag) {
            pthread_cond_wait(&renderThread->WorkAvailableCondition, &renderThread->Mutex);
        }
        renderThread->WorkAvailableFlag = false;
        pthread_mutex_unlock(&renderThread->Mutex);

        // Check for exit.
        if (renderThread->Exit) {
            break;
        }

        // Make sure the scene has VAOs created for this context.
        if (renderThread->Scene != NULL && renderThread->Scene != lastScene) {
            if (lastScene != NULL) {
                ovrScene_DestroyVAOs(lastScene);
            }
            ovrScene_CreateVAOs(renderThread->Scene);
            lastScene = renderThread->Scene;
        }

        // Render.
        ovrLayer_Union2 layers[ovrMaxLayerCount] = {0};
        int layerCount = 0;
        int frameFlags = 0;

        if (renderThread->RenderType == RENDER_FRAME) {
            ovrLayerProjection2 layer;
            layer = ovrRenderer_RenderFrame(
                &renderer,
                &java,
                renderThread->Scene,
                &renderThread->Simulation,
                &renderThread->Tracking,
                renderThread->Ovr);

            layers[layerCount++].Projection = layer;
        } else if (renderThread->RenderType == RENDER_LOADING_ICON) {
            ovrLayerProjection2 blackLayer = vrapi_DefaultLayerBlackProjection2();
            blackLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
            layers[layerCount++].Projection = blackLayer;

            ovrLayerLoadingIcon2 iconLayer = vrapi_DefaultLayerLoadingIcon2();
            iconLayer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_INHIBIT_SRGB_FRAMEBUFFER;
            layers[layerCount++].LoadingIcon = iconLayer;

            frameFlags |= VRAPI_FRAME_FLAG_FLUSH;
        }

        const ovrLayerHeader2* layerList[ovrMaxLayerCount] = {0};
        for (int i = 0; i < layerCount; i++) {
            layerList[i] = &layers[i].Header;
        }

        ovrSubmitFrameDescription2 frameDesc = {0};
        frameDesc.Flags = frameFlags;
        frameDesc.SwapInterval = renderThread->SwapInterval;
        frameDesc.FrameIndex = renderThread->FrameIndex;
        frameDesc.DisplayTime = renderThread->DisplayTime;
        frameDesc.LayerCount = layerCount;
        frameDesc.Layers = layerList;

        vrapi_SubmitFrame2(renderThread->Ovr, &frameDesc);
    }

    if (lastScene != NULL) {
        ovrScene_DestroyVAOs(lastScene);
    }

    ovrRenderer_Destroy(&renderer);
    ovrEgl_DestroyContext(&egl);

    (*java.Vm)->DetachCurrentThread(java.Vm);

    return NULL;
}

void ovrRenderThread_Clear(ovrRenderThread* renderThread) {
    renderThread->JavaVm = NULL;
    renderThread->ActivityObject = NULL;
    renderThread->ShareEgl = NULL;
    renderThread->Thread = 0;
    renderThread->Tid = 0;
    renderThread->UseMultiview = false;
    renderThread->Exit = false;
    renderThread->WorkAvailableFlag = false;
    renderThread->WorkDoneFlag = false;
    renderThread->Ovr = NULL;
    renderThread->RenderType = RENDER_FRAME;
    renderThread->FrameIndex = 1;
    renderThread->DisplayTime = 0;
    renderThread->SwapInterval = 1;
    renderThread->Scene = NULL;
    ovrSimulation_Clear(&renderThread->Simulation);
}

void ovrRenderThread_Create(
    ovrRenderThread* renderThread,
    const ovrJava* java,
    const ovrEgl* shareEgl,
    const int useMultiview) {
    renderThread->JavaVm = java->Vm;
    renderThread->ActivityObject = java->ActivityObject;
    renderThread->ShareEgl = shareEgl;
    renderThread->Thread = 0;
    renderThread->Tid = 0;
    renderThread->UseMultiview = useMultiview;
    renderThread->Exit = false;
    renderThread->WorkAvailableFlag = false;
    renderThread->WorkDoneFlag = false;
    pthread_cond_init(&renderThread->WorkAvailableCondition, NULL);
    pthread_cond_init(&renderThread->WorkDoneCondition, NULL);
    pthread_mutex_init(&renderThread->Mutex, NULL);

    const int createErr =
        pthread_create(&renderThread->Thread, NULL, RenderThreadFunction, renderThread);
    if (createErr != 0) {
        ALOGE("pthread_create returned %i", createErr);
    }
}

void ovrRenderThread_Destroy(ovrRenderThread* renderThread) {
    pthread_mutex_lock(&renderThread->Mutex);
    renderThread->Exit = true;
    renderThread->WorkAvailableFlag = true;
    pthread_cond_signal(&renderThread->WorkAvailableCondition);
    pthread_mutex_unlock(&renderThread->Mutex);

    pthread_join(renderThread->Thread, NULL);
    pthread_cond_destroy(&renderThread->WorkAvailableCondition);
    pthread_cond_destroy(&renderThread->WorkDoneCondition);
    pthread_mutex_destroy(&renderThread->Mutex);
}

void ovrRenderThread_Submit(
    ovrRenderThread* renderThread,
    ovrMobile* ovr,
    ovrRenderType type,
    long long frameIndex,
    double displayTime,
    int swapInterval,
    ovrScene* scene,
    const ovrSimulation* simulation,
    const ovrTracking2* tracking) {
    // Wait for the renderer thread to finish the last frame.
    pthread_mutex_lock(&renderThread->Mutex);
    while (!renderThread->WorkDoneFlag) {
        pthread_cond_wait(&renderThread->WorkDoneCondition, &renderThread->Mutex);
    }
    renderThread->WorkDoneFlag = false;
    // Latch the render data.
    renderThread->Ovr = ovr;
    renderThread->RenderType = type;
    renderThread->FrameIndex = frameIndex;
    renderThread->DisplayTime = displayTime;
    renderThread->SwapInterval = swapInterval;
    renderThread->Scene = scene;
    if (simulation != NULL) {
        renderThread->Simulation = *simulation;
    }
    if (tracking != NULL) {
        renderThread->Tracking = *tracking;
    }
    // Signal work is available.
    renderThread->WorkAvailableFlag = true;
    pthread_cond_signal(&renderThread->WorkAvailableCondition);
    pthread_mutex_unlock(&renderThread->Mutex);
}

void ovrRenderThread_Wait(ovrRenderThread* renderThread) {
    // Wait for the renderer thread to finish the last frame.
    pthread_mutex_lock(&renderThread->Mutex);
    while (!renderThread->WorkDoneFlag) {
        pthread_cond_wait(&renderThread->WorkDoneCondition, &renderThread->Mutex);
    }
    pthread_mutex_unlock(&renderThread->Mutex);
}

static int ovrRenderThread_GetTid(ovrRenderThread* renderThread) {
    ovrRenderThread_Wait(renderThread);
    return renderThread->Tid;
}

#endif // MULTI_THREADED

/*
================================================================================

ovrApp

================================================================================
*/

typedef struct {
    ovrJava Java;
    ovrEgl Egl;
    ANativeWindow* NativeWindow;
    int Resumed;
    ovrMobile* Ovr;
    ovrScene Scene;
    long long FrameIndex;
    double DisplayTime;
    int SwapInterval;
    int CpuLevel;
    int GpuLevel;
    int MainThreadTid;
    int RenderThreadTid;
    int GamePadBackButtonDown;
#if MULTI_THREADED
    ovrRenderThread RenderThread;
#else
    ovrRenderer Renderer;
#endif
    int UseMultiview;
} ovrApp;

void ovrApp_Clear(ovrApp* app) ;

void ovrApp_HandleVrModeChanges(ovrApp* app) ;

void ovrApp_HandleInput(ovrApp* app) ;

void ovrApp_HandleVrApiEvents(ovrApp* app);

#endif