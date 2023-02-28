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

#include "VrApi.h"
#include "VrApi_Helpers.h"
#include "VrApi_SystemUtils.h"
#include "VrApi_Input.h"
#include "vr.h"

extern "C"
{
#include <render.h>
}

/*
================================================================================

System Clock Time

================================================================================
*/

double GetTimeInSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * 1e9 + now.tv_nsec) * 0.000000001;
}

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/
OpenGLExtensions_t glExtensions;

void EglInitExtensions() {
    const char* allExtensions = (const char*)glGetString(GL_EXTENSIONS);
    if (allExtensions != NULL) {
        glExtensions.multi_view = strstr(allExtensions, "GL_OVR_multiview2") &&
                                  strstr(allExtensions, "GL_OVR_multiview_multisampled_render_to_texture");

        glExtensions.EXT_texture_border_clamp =
                strstr(allExtensions, "GL_EXT_texture_border_clamp") ||
                strstr(allExtensions, "GL_OES_texture_border_clamp");
    }
}

static const char* EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

static const char* GlFrameBufferStatusString(GLenum status) {
switch (status) {
case GL_FRAMEBUFFER_UNDEFINED:
return "GL_FRAMEBUFFER_UNDEFINED";
case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
case GL_FRAMEBUFFER_UNSUPPORTED:
return "GL_FRAMEBUFFER_UNSUPPORTED";
case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
default:
return "unknown";
}
}


/*
================================================================================

ovrEgl

================================================================================
*/

void ovrEgl_CreateContext(ovrEgl* egl, const ovrEgl* shareEgl) {
    if (egl->Display != 0) {
        return;
    }

    egl->Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ALOGV("        eglInitialize( Display, &MajorVersion, &MinorVersion )");
    eglInitialize(egl->Display, &egl->MajorVersion, &egl->MinorVersion);
    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted for our warp target.
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    if (eglGetConfigs(egl->Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE) {
        ALOGE("        eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint configAttribs[] = {
            EGL_RED_SIZE,
            8,
            EGL_GREEN_SIZE,
            8,
            EGL_BLUE_SIZE,
            8,
            EGL_ALPHA_SIZE,
            8, // need alpha for the multi-pass timewarp compositor
            EGL_DEPTH_SIZE,
            0,
            EGL_STENCIL_SIZE,
            0,
            EGL_SAMPLES,
            0,
            EGL_NONE};
    egl->Config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;

        eglGetConfigAttrib(egl->Display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
            continue;
        }

        // The pbuffer config also needs to be compatible with normal window rendering
        // so it can share textures with the window context.
        eglGetConfigAttrib(egl->Display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(egl->Display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            egl->Config = configs[i];
            break;
        }
    }
    if (egl->Config == 0) {
        ALOGE("        eglChooseConfig() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    ALOGV("        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )");
    egl->Context = eglCreateContext(
            egl->Display,
            egl->Config,
            (shareEgl != NULL) ? shareEgl->Context : EGL_NO_CONTEXT,
            contextAttribs);
    if (egl->Context == EGL_NO_CONTEXT) {
        ALOGE("        eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    ALOGV("        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )");
    egl->TinySurface = eglCreatePbufferSurface(egl->Display, egl->Config, surfaceAttribs);
    if (egl->TinySurface == EGL_NO_SURFACE) {
        ALOGE("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
    ALOGV("        eglMakeCurrent( Display, TinySurface, TinySurface, Context )");
    if (eglMakeCurrent(egl->Display, egl->TinySurface, egl->TinySurface, egl->Context) ==
        EGL_FALSE) {
        ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        eglDestroySurface(egl->Display, egl->TinySurface);
        eglDestroyContext(egl->Display, egl->Context);
        egl->Context = EGL_NO_CONTEXT;
        return;
    }
}

void ovrEgl_DestroyContext(ovrEgl* egl) {
    if (egl->Display != 0) {
        ALOGE("        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )");
        if (eglMakeCurrent(egl->Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) ==
            EGL_FALSE) {
            ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        }
    }
    if (egl->Context != EGL_NO_CONTEXT) {
        ALOGE("        eglDestroyContext( Display, Context )");
        if (eglDestroyContext(egl->Display, egl->Context) == EGL_FALSE) {
            ALOGE("        eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Context = EGL_NO_CONTEXT;
    }
    if (egl->TinySurface != EGL_NO_SURFACE) {
        ALOGE("        eglDestroySurface( Display, TinySurface )");
        if (eglDestroySurface(egl->Display, egl->TinySurface) == EGL_FALSE) {
            ALOGE("        eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
        }
        egl->TinySurface = EGL_NO_SURFACE;
    }
    if (egl->Display != 0) {
        ALOGE("        eglTerminate( Display )");
        if (eglTerminate(egl->Display) == EGL_FALSE) {
            ALOGE("        eglTerminate() failed: %s", EglErrorString(eglGetError()));
        }
        egl->Display = 0;
    }
}

void ovrEgl_Clear(ovrEgl* egl) {
    egl->MajorVersion = 0;
    egl->MinorVersion = 0;
    egl->Display = 0;
    egl->Config = 0;
    egl->TinySurface = EGL_NO_SURFACE;
    egl->MainSurface = EGL_NO_SURFACE;
    egl->Context = EGL_NO_CONTEXT;
}

/*
================================================================================

ovrProgram

================================================================================
*/


void ovrProgram_Clear(ovrProgram* program) {
    program->Program = 0;
    program->VertexShader = 0;
    program->FragmentShader = 0;
    memset(program->UniformLocation, 0, sizeof(program->UniformLocation));
    memset(program->UniformBinding, 0, sizeof(program->UniformBinding));
    memset(program->Textures, 0, sizeof(program->Textures));
}

GLint program_with_tex_id;

static int ovrProgram_Create(
        ovrProgram* program,
        const char* vertexSource,
        const char* fragmentSource,
        const int useMultiview) {
    GLint r;

    GL(program->VertexShader = glCreateShader(GL_VERTEX_SHADER));

    const char* vertexSources[3] = {
            programVersion,
            (useMultiview) ? "#define DISABLE_MULTIVIEW 0\n" : "#define DISABLE_MULTIVIEW 1\n",
            vertexSource};
    GL(glShaderSource(program->VertexShader, 3, vertexSources, 0));
    GL(glCompileShader(program->VertexShader));
    GL(glGetShaderiv(program->VertexShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(program->VertexShader, sizeof(msg), 0, msg));
        ALOGE("%s\n%s\n", vertexSource, msg);
        return false;
    }

    const char* fragmentSources[2] = {programVersion, fragmentSource};
    GL(program->FragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
    GL(glShaderSource(program->FragmentShader, 2, fragmentSources, 0));
    GL(glCompileShader(program->FragmentShader));
    GL(glGetShaderiv(program->FragmentShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(program->FragmentShader, sizeof(msg), 0, msg));
        ALOGE("%s\n%s\n", fragmentSource, msg);
        return false;
    }

    GL(program->Program = glCreateProgram());
    GL(glAttachShader(program->Program, program->VertexShader));
    GL(glAttachShader(program->Program, program->FragmentShader));
    program_with_tex_id = program->Program;

    // Bind the vertex attribute locations.
    for (int i = 0; i < sizeof(ProgramVertexAttributes) / sizeof(ProgramVertexAttributes[0]); i++) {
        GL(glBindAttribLocation(
                program->Program,
                ProgramVertexAttributes[i].location,
                ProgramVertexAttributes[i].name));
    }

    GL(glLinkProgram(program->Program));
    GL(glGetProgramiv(program->Program, GL_LINK_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetProgramInfoLog(program->Program, sizeof(msg), 0, msg));
        ALOGE("Linking program failed: %s\n", msg);
        return false;
    }

    int numBufferBindings = 0;

    // Get the uniform locations.
    memset(program->UniformLocation, -1, sizeof(program->UniformLocation));
    for (int i = 0; i < sizeof(ProgramUniforms) / sizeof(ProgramUniforms[0]); i++) {
        const int uniformIndex = ProgramUniforms[i].index;
        if (ProgramUniforms[i].type == UNIFORM_TYPE_BUFFER) {
            GL(program->UniformLocation[uniformIndex] =
                       glGetUniformBlockIndex(program->Program, ProgramUniforms[i].name));
            program->UniformBinding[uniformIndex] = numBufferBindings++;
            GL(glUniformBlockBinding(
                    program->Program,
                    program->UniformLocation[uniformIndex],
                    program->UniformBinding[uniformIndex]));
        } else {
            GL(program->UniformLocation[uniformIndex] =
                       glGetUniformLocation(program->Program, ProgramUniforms[i].name));
            program->UniformBinding[uniformIndex] = program->UniformLocation[uniformIndex];
        }
    }

    GL(glUseProgram(program->Program));

    // Get the texture locations.
    for (int i = 0; i < MAX_PROGRAM_TEXTURES; i++) {
        char name[32];
        sprintf(name, "Texture%i", i);
        program->Textures[i] = glGetUniformLocation(program->Program, name);
        if (program->Textures[i] != -1) {
            GL(glUniform1i(program->Textures[i], i));
        }
    }

    GL(glUseProgram(0));

    return true;
}

void ovrProgram_Destroy(ovrProgram* program) {
    if (program->Program != 0) {
        GL(glDeleteProgram(program->Program));
        program->Program = 0;
    }
    if (program->VertexShader != 0) {
        GL(glDeleteShader(program->VertexShader));
        program->VertexShader = 0;
    }
    if (program->FragmentShader != 0) {
        GL(glDeleteShader(program->FragmentShader));
        program->FragmentShader = 0;
    }
}


/*
================================================================================

ovrFramebuffer

================================================================================
*/

void ovrFramebuffer_Clear(ovrFramebuffer* frameBuffer) {
    frameBuffer->Width = 0;
    frameBuffer->Height = 0;
    frameBuffer->Multisamples = 0;
    frameBuffer->TextureSwapChainLength = 0;
    frameBuffer->TextureSwapChainIndex = 0;
    frameBuffer->UseMultiview = false;
    frameBuffer->ColorTextureSwapChain = NULL;
    frameBuffer->DepthBuffers = NULL;
    frameBuffer->FrameBuffers = NULL;
}

int ovrFramebuffer_Create(
        ovrFramebuffer* frameBuffer,
        const int useMultiview,
        const GLenum colorFormat,
        const int width,
        const int height,
        const int multisamples) {
    PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC glRenderbufferStorageMultisampleEXT =
            (PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC)eglGetProcAddress(
                    "glRenderbufferStorageMultisampleEXT");
    PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC glFramebufferTexture2DMultisampleEXT =
            (PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC)eglGetProcAddress(
                    "glFramebufferTexture2DMultisampleEXT");

    PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR =
            (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)eglGetProcAddress(
                    "glFramebufferTextureMultiviewOVR");
    PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR =
            (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)eglGetProcAddress(
                    "glFramebufferTextureMultisampleMultiviewOVR");

    frameBuffer->Width = width;
    frameBuffer->Height = height;
    frameBuffer->Multisamples = multisamples;
    frameBuffer->UseMultiview =
            (useMultiview && (glFramebufferTextureMultiviewOVR != NULL)) ? true : false;

    frameBuffer->ColorTextureSwapChain = vrapi_CreateTextureSwapChain3(
            frameBuffer->UseMultiview ? VRAPI_TEXTURE_TYPE_2D_ARRAY : VRAPI_TEXTURE_TYPE_2D,
            colorFormat,
            width,
            height,
            1,
            3);
    frameBuffer->TextureSwapChainLength =
            vrapi_GetTextureSwapChainLength(frameBuffer->ColorTextureSwapChain);
    frameBuffer->DepthBuffers =
            (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));
    frameBuffer->FrameBuffers =
            (GLuint*)malloc(frameBuffer->TextureSwapChainLength * sizeof(GLuint));

    ALOGV("        frameBuffer->UseMultiview = %d", frameBuffer->UseMultiview);

    for (int i = 0; i < frameBuffer->TextureSwapChainLength; i++) {
        // Create the color buffer texture.
        const GLuint colorTexture =
                vrapi_GetTextureSwapChainHandle(frameBuffer->ColorTextureSwapChain, i);
        GLenum colorTextureTarget = frameBuffer->UseMultiview ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D;
        GL(glBindTexture(colorTextureTarget, colorTexture));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
        GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        GL(glTexParameterfv(colorTextureTarget, GL_TEXTURE_BORDER_COLOR, borderColor));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(colorTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL(glBindTexture(colorTextureTarget, 0));

        if (frameBuffer->UseMultiview) {
            // Create the depth buffer texture.
            GL(glGenTextures(1, &frameBuffer->DepthBuffers[i]));
            GL(glBindTexture(GL_TEXTURE_2D_ARRAY, frameBuffer->DepthBuffers[i]));
            GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT24, width, height, 2));
            GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));

            // Create the frame buffer.
            GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
            GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
            if (multisamples > 1 && (glFramebufferTextureMultisampleMultiviewOVR != NULL)) {
                GL(glFramebufferTextureMultisampleMultiviewOVR(
                        GL_DRAW_FRAMEBUFFER,
                        GL_DEPTH_ATTACHMENT,
                        frameBuffer->DepthBuffers[i],
                        0 /* level */,
                        multisamples /* samples */,
                        0 /* baseViewIndex */,
                        2 /* numViews */));
                GL(glFramebufferTextureMultisampleMultiviewOVR(
                        GL_DRAW_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        colorTexture,
                        0 /* level */,
                        multisamples /* samples */,
                        0 /* baseViewIndex */,
                        2 /* numViews */));
            } else {
                GL(glFramebufferTextureMultiviewOVR(
                        GL_DRAW_FRAMEBUFFER,
                        GL_DEPTH_ATTACHMENT,
                        frameBuffer->DepthBuffers[i],
                        0 /* level */,
                        0 /* baseViewIndex */,
                        2 /* numViews */));
                GL(glFramebufferTextureMultiviewOVR(
                        GL_DRAW_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        colorTexture,
                        0 /* level */,
                        0 /* baseViewIndex */,
                        2 /* numViews */));
            }

            GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
            GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
            if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                ALOGE(
                        "Incomplete frame buffer object: %s",
                        GlFrameBufferStatusString(renderFramebufferStatus));
                return false;
            }
        } else {
            if (multisamples > 1 && glRenderbufferStorageMultisampleEXT != NULL &&
                glFramebufferTexture2DMultisampleEXT != NULL) {
                // Create multisampled depth buffer.
                GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
                GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
                GL(glRenderbufferStorageMultisampleEXT(
                        GL_RENDERBUFFER, multisamples, GL_DEPTH_COMPONENT24, width, height));
                GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

                // Create the frame buffer.
                // NOTE: glFramebufferTexture2DMultisampleEXT only works with GL_FRAMEBUFFER.
                GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
                GL(glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
                GL(glFramebufferTexture2DMultisampleEXT(
                        GL_FRAMEBUFFER,
                        GL_COLOR_ATTACHMENT0,
                        GL_TEXTURE_2D,
                        colorTexture,
                        0,
                        multisamples));
                GL(glFramebufferRenderbuffer(
                        GL_FRAMEBUFFER,
                        GL_DEPTH_ATTACHMENT,
                        GL_RENDERBUFFER,
                        frameBuffer->DepthBuffers[i]));
                GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER));
                GL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
                if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                    ALOGE(
                            "Incomplete frame buffer object: %s",
                            GlFrameBufferStatusString(renderFramebufferStatus));
                    return false;
                }
            } else {
                // Create depth buffer.
                GL(glGenRenderbuffers(1, &frameBuffer->DepthBuffers[i]));
                GL(glBindRenderbuffer(GL_RENDERBUFFER, frameBuffer->DepthBuffers[i]));
                GL(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height));
                GL(glBindRenderbuffer(GL_RENDERBUFFER, 0));

                // Create the frame buffer.
                GL(glGenFramebuffers(1, &frameBuffer->FrameBuffers[i]));
                GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[i]));
                GL(glFramebufferRenderbuffer(
                        GL_DRAW_FRAMEBUFFER,
                        GL_DEPTH_ATTACHMENT,
                        GL_RENDERBUFFER,
                        frameBuffer->DepthBuffers[i]));
                GL(glFramebufferTexture2D(
                        GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0));
                GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
                GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
                if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
                    ALOGE(
                            "Incomplete frame buffer object: %s",
                            GlFrameBufferStatusString(renderFramebufferStatus));
                    return false;
                }
            }
        }
    }

    return true;
}

void ovrFramebuffer_Destroy(ovrFramebuffer* frameBuffer) {
    GL(glDeleteFramebuffers(frameBuffer->TextureSwapChainLength, frameBuffer->FrameBuffers));
    if (frameBuffer->UseMultiview) {
        GL(glDeleteTextures(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers));
    } else {
        GL(glDeleteRenderbuffers(frameBuffer->TextureSwapChainLength, frameBuffer->DepthBuffers));
    }
    vrapi_DestroyTextureSwapChain(frameBuffer->ColorTextureSwapChain);

    free(frameBuffer->DepthBuffers);
    free(frameBuffer->FrameBuffers);

    ovrFramebuffer_Clear(frameBuffer);
}

void ovrFramebuffer_SetCurrent(ovrFramebuffer* frameBuffer) {
    GL(glBindFramebuffer(
            GL_DRAW_FRAMEBUFFER, frameBuffer->FrameBuffers[frameBuffer->TextureSwapChainIndex]));
}

void ovrFramebuffer_SetNone() {
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
}

void ovrFramebuffer_Resolve(ovrFramebuffer* frameBuffer) {
    // Discard the depth buffer, so the tiler won't need to write it back out to memory.
    const GLenum depthAttachment[1] = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);

    // We now let the resolve happen implicitly.
}

void ovrFramebuffer_Advance(ovrFramebuffer* frameBuffer) {
    // Advance to the next texture from the set.
    frameBuffer->TextureSwapChainIndex =
            (frameBuffer->TextureSwapChainIndex + 1) % frameBuffer->TextureSwapChainLength;
}

/*
================================================================================

ovrScene

================================================================================
*/

void ovrScene_Clear(ovrScene* scene) {
    scene->CreatedScene = false;
    scene->CreatedVAOs = false;
    scene->Random = 2;
    scene->SceneMatrices = 0;
    scene->InstanceTransformBuffer = 0;

    ovrProgram_Clear(&scene->Program);
}

int ovrScene_IsCreated(ovrScene* scene) {
    return scene->CreatedScene;
}

// Returns a random float in the range [0, 1].
static float ovrScene_RandomFloat(ovrScene* scene) {
    scene->Random = 1664525L * scene->Random + 1013904223L;
    unsigned int rf = 0x3F800000 | (scene->Random & 0x007FFFFF);
    return (*(float*)&rf) - 1.0f;
}

void ovrScene_Create(ovrScene* scene, int useMultiview) {
    ovrProgram_Create(&scene->Program, VERTEX_SHADER, FRAGMENT_SHADER, useMultiview);

    // Create the instance transform attribute buffer.
    GL(glGenBuffers(1, &scene->InstanceTransformBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, scene->InstanceTransformBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, NUM_INSTANCES * 4 * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    // Setup the scene matrices.
    GL(glGenBuffers(1, &scene->SceneMatrices));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, scene->SceneMatrices));
    GL(glBufferData(
            GL_UNIFORM_BUFFER,
            2 * sizeof(ovrMatrix4f) /* 2 view matrices */ +
            2 * sizeof(ovrMatrix4f) /* 2 projection matrices */,
            NULL,
            GL_STATIC_DRAW));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    scene->CreatedScene = true;

}

void ovrScene_Destroy(ovrScene* scene) {
    ovrProgram_Destroy(&scene->Program);
    GL(glDeleteBuffers(1, &scene->InstanceTransformBuffer));
    GL(glDeleteBuffers(1, &scene->SceneMatrices));
    scene->CreatedScene = false;
}

/*
================================================================================

ovrSimulation

================================================================================
*/


void ovrSimulation_Clear(ovrSimulation* simulation) {
    simulation->CurrentRotation.x = 0.0f;
    simulation->CurrentRotation.y = 0.0f;
    simulation->CurrentRotation.z = 0.0f;
}

void ovrSimulation_Advance(ovrSimulation* simulation, double elapsedDisplayTime) {
    // Update rotation.
    simulation->CurrentRotation.x = (float)(elapsedDisplayTime);
    simulation->CurrentRotation.y = (float)(elapsedDisplayTime);
    simulation->CurrentRotation.z = (float)(elapsedDisplayTime);
}

/*
================================================================================

ovrRenderer

================================================================================
*/

void ovrRenderer_Clear(ovrRenderer* renderer) {
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer_Clear(&renderer->FrameBuffer[eye]);
    }
    renderer->NumBuffers = VRAPI_FRAME_LAYER_EYE_MAX;
}

void
ovrRenderer_Create(ovrRenderer* renderer, const ovrJava* java, const int useMultiview) {
    renderer->NumBuffers = useMultiview ? 1 : VRAPI_FRAME_LAYER_EYE_MAX;

    // Create the frame buffers.
    for (int eye = 0; eye < renderer->NumBuffers; eye++) {
        ovrFramebuffer_Create(
                &renderer->FrameBuffer[eye],
                useMultiview,
                GL_SRGB8_ALPHA8,
                vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_WIDTH),
                vrapi_GetSystemPropertyInt(java, VRAPI_SYS_PROP_SUGGESTED_EYE_TEXTURE_HEIGHT),
                NUM_MULTI_SAMPLES);
    }
}

void ovrRenderer_Destroy(ovrRenderer* renderer) {
    for (int eye = 0; eye < renderer->NumBuffers; eye++) {
        ovrFramebuffer_Destroy(&renderer->FrameBuffer[eye]);
    }
}

ovrLayerProjection2 ovrRenderer_RenderFrame(
        ovrRenderer* renderer,
        const ovrJava* java,
        const ovrScene* scene,
        const ovrSimulation* simulation,
        const ovrTracking2* tracking,
        ovrMobile* ovr) {
    ovrMatrix4f rotationMatrices;
    rotationMatrices = ovrMatrix4f_CreateRotation(0,0,0);  // radians * time

    // Update the instance transform attributes.
    GL(glBindBuffer(GL_ARRAY_BUFFER, scene->InstanceTransformBuffer));
    ovrMatrix4f* modelMatrix = (ovrMatrix4f*)GL(glMapBufferRange(
            GL_ARRAY_BUFFER,
            0,
            1 * sizeof(ovrMatrix4f),
            GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

    for (int i = 0; i < NUM_INSTANCES; i++) {
        // Write in order in case the mapped buffer lives on write-combined memory.
        modelMatrix[i].M[0][0] = rotationMatrices.M[0][0];
        modelMatrix[i].M[0][1] = rotationMatrices.M[0][1];
        modelMatrix[i].M[0][2] = rotationMatrices.M[0][2];
        modelMatrix[i].M[0][3] = rotationMatrices.M[0][3];

        modelMatrix[i].M[1][0] = rotationMatrices.M[1][0];
        modelMatrix[i].M[1][1] = rotationMatrices.M[1][1];
        modelMatrix[i].M[1][2] = rotationMatrices.M[1][2];
        modelMatrix[i].M[1][3] = rotationMatrices.M[1][3];

        modelMatrix[i].M[2][0] = rotationMatrices.M[2][0];
        modelMatrix[i].M[2][1] = rotationMatrices.M[2][1];
        modelMatrix[i].M[2][2] = rotationMatrices.M[2][2];
        modelMatrix[i].M[2][3] = rotationMatrices.M[2][3];

        modelMatrix[i].M[3][0] = 0;
        modelMatrix[i].M[3][1] = 0;
        modelMatrix[i].M[3][2] = 0;
        modelMatrix[i].M[3][3] = 1.0f;
    }
    GL(glUnmapBuffer(GL_ARRAY_BUFFER));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    ovrTracking2 updatedTracking = *tracking;

    ovrMatrix4f translation = ovrMatrix4f_CreateTranslation(-shipPosition.x, -shipPosition.y, -shipPosition.z);

    ovrMatrix4f eye0 =  ovrMatrix4f_Multiply(&updatedTracking.Eye[0].ViewMatrix, &translation);
    ovrMatrix4f eye1 =  ovrMatrix4f_Multiply(&updatedTracking.Eye[1].ViewMatrix, &translation);

    ovrMatrix4f eyeViewMatrixTransposed[2];
    eyeViewMatrixTransposed[0] = ovrMatrix4f_Transpose(&eye0);
    eyeViewMatrixTransposed[1] = ovrMatrix4f_Transpose(&eye1);

    ovrMatrix4f projectionMatrixTransposed[2];
    projectionMatrixTransposed[0] = ovrMatrix4f_Transpose(&updatedTracking.Eye[0].ProjectionMatrix);
    projectionMatrixTransposed[1] = ovrMatrix4f_Transpose(&updatedTracking.Eye[1].ProjectionMatrix);

    // Update the scene matrices.
    GL(glBindBuffer(GL_UNIFORM_BUFFER, scene->SceneMatrices));
    ovrMatrix4f* sceneMatrices = (ovrMatrix4f*)GL(glMapBufferRange(GL_UNIFORM_BUFFER,
                                                                   0,
                                                                   2 * sizeof(ovrMatrix4f) /* 2 view matrices */ +
                                                                   2 * sizeof(ovrMatrix4f) /* 2 projection matrices */,
                                                                   GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)); // TODO: Why?

    if (sceneMatrices != NULL) {
        memcpy((char*)sceneMatrices, &eyeViewMatrixTransposed, 2 * sizeof(ovrMatrix4f));
        memcpy((char*)sceneMatrices + 2 * sizeof(ovrMatrix4f), &projectionMatrixTransposed,2 * sizeof(ovrMatrix4f));
    }

    GL(glUnmapBuffer(GL_UNIFORM_BUFFER)); // TODO: Why?
    GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    ovrLayerProjection2 layer = vrapi_DefaultLayerProjection2();
    layer.HeadPose = updatedTracking.HeadPose;
    for (int eye = 0; eye < VRAPI_FRAME_LAYER_EYE_MAX; eye++) {
        ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[renderer->NumBuffers == 1 ? 0 : eye];
        layer.Textures[eye].ColorSwapChain = frameBuffer->ColorTextureSwapChain;
        layer.Textures[eye].SwapChainIndex = frameBuffer->TextureSwapChainIndex;
        layer.Textures[eye].TexCoordsFromTanAngles = ovrMatrix4f_TanAngleMatrixFromProjection(&updatedTracking.Eye[eye].ProjectionMatrix);
    }
    layer.Header.Flags |= VRAPI_FRAME_LAYER_FLAG_CHROMATIC_ABERRATION_CORRECTION;

    // Render the eye images.
    for (int eye = 0; eye < renderer->NumBuffers; eye++) {
        // PRE DRAW

        // NOTE: In the non-mv case, latency can be further reduced by updating the sensor
        // prediction for each eye (updates orientation, not position)
        ovrFramebuffer* frameBuffer = &renderer->FrameBuffer[eye];
        ovrFramebuffer_SetCurrent(frameBuffer);

        GL(glUseProgram(scene->Program.Program)); // TODO: Why?
        GL(glBindBufferBase(
                GL_UNIFORM_BUFFER,
                scene->Program.UniformBinding[UNIFORM_SCENE_MATRICES],
                scene->SceneMatrices)); // TODO: Why?
        if (scene->Program.UniformLocation[UNIFORM_VIEW_ID] >= 0) // NOTE: will not be present when multiview path is enabled.
        {
            GL(glUniform1i(scene->Program.UniformLocation[UNIFORM_VIEW_ID], eye)); // projection?
        }

        GL(glEnable(GL_SCISSOR_TEST));
        GL(glDepthMask(GL_TRUE));
        GL(glEnable(GL_DEPTH_TEST));
        GL(glDepthFunc(GL_LEQUAL));
        //GL(glEnable(GL_CULL_FACE));
        //GL(glCullFace(GL_BACK));

        GL(glViewport(0, 0, frameBuffer->Width, frameBuffer->Height));
        GL(glScissor(0, 0, frameBuffer->Width, frameBuffer->Height));
        GL(glClearColor(0.125f, 0.0f, 0.125f, 1.0f));
        GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

        GL(render_frame(0));

        // POST DRAW
        GL(glUseProgram(0));

        ovrFramebuffer_Resolve(frameBuffer);
        ovrFramebuffer_Advance(frameBuffer);
    }

    ovrFramebuffer_SetNone();

    return layer;

}

/*
================================================================================

ovrRenderThread

================================================================================
*/


void ovrApp_Clear(ovrApp* app) {
    app->Java.Vm = NULL;
    app->Java.Env = NULL;
    app->Java.ActivityObject = NULL;
    app->NativeWindow = NULL;
    app->Resumed = false;
    app->Ovr = NULL;
    app->FrameIndex = 1;
    app->DisplayTime = 0;
    app->SwapInterval = 1;
    app->CpuLevel = 2;
    app->GpuLevel = 2;
    app->MainThreadTid = 0;
    app->RenderThreadTid = 0;
    app->UseMultiview = true;

    ovrEgl_Clear(&app->Egl);
    ovrScene_Clear(&app->Scene);
    ovrSimulation_Clear(&app->Simulation);
#if MULTI_THREADED
    ovrRenderThread_Clear(&app->RenderThread);
#else
    ovrRenderer_Clear(&app->Renderer);
#endif
}

void ovrApp_HandleVrModeChanges(ovrApp* app) {
    if (app->Resumed != false && app->NativeWindow != NULL) {
        if (app->Ovr == NULL) {
            ovrModeParms parms = vrapi_DefaultModeParms(&app->Java);
            // No need to reset the FLAG_FULLSCREEN window flag when using a View
            parms.Flags &= ~VRAPI_MODE_FLAG_RESET_WINDOW_FULLSCREEN;

            parms.Flags |= VRAPI_MODE_FLAG_FRONT_BUFFER_SRGB;
            parms.Flags |= VRAPI_MODE_FLAG_NATIVE_WINDOW;
            parms.Display = (size_t)app->Egl.Display;
            parms.WindowSurface = (size_t)app->NativeWindow;
            parms.ShareContext = (size_t)app->Egl.Context;

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            ALOGV("        vrapi_EnterVrMode()");

            app->Ovr = vrapi_EnterVrMode(&parms);

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            // If entering VR mode failed then the ANativeWindow was not valid.
            if (app->Ovr == NULL) {
                ALOGE("Invalid ANativeWindow!");
                app->NativeWindow = NULL;
            }

            // Set performance parameters once we have entered VR mode and have a valid ovrMobile.
            if (app->Ovr != NULL) {
                vrapi_SetClockLevels(app->Ovr, app->CpuLevel, app->GpuLevel);

                ALOGV("		vrapi_SetClockLevels( %d, %d )", app->CpuLevel, app->GpuLevel);

                vrapi_SetPerfThread(app->Ovr, VRAPI_PERF_THREAD_TYPE_MAIN, app->MainThreadTid);

                ALOGV("		vrapi_SetPerfThread( MAIN, %d )", app->MainThreadTid);

                vrapi_SetPerfThread(
                        app->Ovr, VRAPI_PERF_THREAD_TYPE_RENDERER, app->RenderThreadTid);

                ALOGV("		vrapi_SetPerfThread( RENDERER, %d )", app->RenderThreadTid);
            }
        }
    } else {
        if (app->Ovr != NULL) {
#if MULTI_THREADED
            // Make sure the renderer thread is no longer using the ovrMobile.
            ovrRenderThread_Wait(&app->RenderThread);
#endif
            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));

            ALOGV("        vrapi_LeaveVrMode()");

            vrapi_LeaveVrMode(app->Ovr);
            app->Ovr = NULL;

            ALOGV("        eglGetCurrentSurface( EGL_DRAW ) = %p", eglGetCurrentSurface(EGL_DRAW));
        }
    }
}

ovrInputStateTrackedRemote rightTrackedRemoteState_old;
ovrInputStateTrackedRemote rightTrackedRemoteState_new;

ovrInputStateTrackedRemote leftTrackedRemoteState_old;
ovrInputStateTrackedRemote leftTrackedRemoteState_new;

ovrVector3f vecmul(ovrVector3f f, float speed);

ovrVector3f vecadd(ovrVector3f f, ovrVector3f f1);

ovrVector3f shipPosition = {0, 0, 0};

int fire_secondary = false;
int fire_primary = false;

extern ovrVector3f up, forward, right;

void ovrApp_HandleInput(ovrApp * app )
{
    for ( int i = 0; ; i++ ) {
        ovrInputCapabilityHeader cap;
        ovrResult result = vrapi_EnumerateInputDevices(app->Ovr, i, &cap);
        if (result < 0) {
            break;
        }

        if (cap.Type == ovrControllerType_TrackedRemote) {
            ovrTracking remoteTracking;
            ovrInputStateTrackedRemote trackedRemoteState;
            trackedRemoteState.Header.ControllerType = ovrControllerType_TrackedRemote;
            result = vrapi_GetCurrentInputState(app->Ovr, cap.DeviceID, &trackedRemoteState.Header);

            if (result == ovrSuccess) {
                ovrInputTrackedRemoteCapabilities remoteCapabilities;
                remoteCapabilities.Header = cap;
                result = vrapi_GetInputDeviceCapabilities(app->Ovr, &remoteCapabilities.Header);

                result = vrapi_GetInputTrackingState(app->Ovr, cap.DeviceID, app->DisplayTime,&remoteTracking);

                if (remoteCapabilities.ControllerCapabilities & ovrControllerCaps_RightHand) {
                    rightTrackedRemoteState_new = trackedRemoteState;
                } else{
                    leftTrackedRemoteState_new = trackedRemoteState;
                }
            }
        }
    }

    ovrInputStateTrackedRemote *dominantTrackedRemoteState = &rightTrackedRemoteState_new ;
    ovrInputStateTrackedRemote *dominantTrackedRemoteStateOld = &rightTrackedRemoteState_old;
    ovrInputStateTrackedRemote *offHandTrackedRemoteState = &leftTrackedRemoteState_new;
    ovrInputStateTrackedRemote *offHandTrackedRemoteStateOld = &leftTrackedRemoteState_old;

    //Right-hand specific stuff
    {

        int rightJoyState = (rightTrackedRemoteState_new.Joystick.x > 0.7f ? 1 : 0);
        if (rightJoyState != (rightTrackedRemoteState_old.Joystick.x > 0.7f ? 1 : 0)) {
            // pitch up
            ALOGV("pitch up");
        }
        rightJoyState = (rightTrackedRemoteState_new.Joystick.x < -0.7f ? 1 : 0);
        if (rightJoyState != (rightTrackedRemoteState_old.Joystick.x < -0.7f ? 1 : 0)) {
            // pitch down
            ALOGV("pitch down");

        }
        rightJoyState = (rightTrackedRemoteState_new.Joystick.y < -0.7f ? 1 : 0);
        if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y < -0.7f ? 1 : 0)) {
            // yaw left
            ALOGV("yaw left");
        }
        rightJoyState = (rightTrackedRemoteState_new.Joystick.y > 0.7f ? 1 : 0);
        if (rightJoyState != (rightTrackedRemoteState_old.Joystick.y > 0.7f ? 1 : 0)) {
            // yaw right
            ALOGV("yaw right");
        }

    }

    //Left-hand specific stuff
    {

        int leftJoyState = (leftTrackedRemoteState_new.Joystick.x > 0.7f ? 1 : 0);
        if (leftJoyState != (leftTrackedRemoteState_old.Joystick.x > 0.7f ? 1 : 0)) {
            // roll left
            ALOGV("roll left");
        }

        leftJoyState = (leftTrackedRemoteState_new.Joystick.x < -0.7f ? 1 : 0);
        if (leftJoyState != (leftTrackedRemoteState_old.Joystick.x < -0.7f ? 1 : 0)) {
            // roll right
            ALOGV("roll right");
        }
        leftJoyState = (leftTrackedRemoteState_new.Joystick.y < -0.7f ? 1 : 0);

        const double predictedDisplayTime = vrapi_GetPredictedDisplayTime(app->Ovr, app->FrameIndex);
        const ovrTracking2 tracking = vrapi_GetPredictedTracking2(app->Ovr, predictedDisplayTime);

        ovrMatrix4f mat = ovrMatrix4f_CreateFromQuaternion(&tracking.HeadPose.Pose.Orientation);

        float speed = 0.5;
        if (leftJoyState != (leftTrackedRemoteState_old.Joystick.y > 0.7f ? 1 : 0)) {
            // move forward
            ALOGV("move forward");
            shipPosition = vecadd(shipPosition, vecmul(forward, speed));
        }
        leftJoyState = (leftTrackedRemoteState_new.Joystick.y > 0.7f ? 1 : 0);
        if (leftJoyState != (leftTrackedRemoteState_old.Joystick.y < -0.7f ? 1 : 0)) {
            // move backward
            ALOGV("move backward");
            shipPosition = vecadd(shipPosition, vecmul(forward, -speed));
        }
    }

    // fire
    if((dominantTrackedRemoteState->Buttons & ovrButton_Trigger))
    {
        ALOGV("fire primary");
        fire_primary = true;

    }

    if((offHandTrackedRemoteState->Buttons & ovrButton_Trigger))
    {
        ALOGV("fire secondary");
        fire_secondary = true;

    }



    // swap old / new
    // why?
    // ..

}

ovrVector3f vecadd(ovrVector3f f, ovrVector3f f1) {
    return (ovrVector3f){f.x + f1.x, f.y + f1.y, f.z + f1.z};
}

ovrVector3f vecmul(ovrVector3f f, float speed) {
    return (ovrVector3f){f.x * speed, f.y* speed, f.z * speed};
}


void ovrApp_HandleVrApiEvents(ovrApp* app) {
    ovrEventDataBuffer eventDataBuffer = {};

    // Poll for VrApi events
    for (;;) {
        ovrEventHeader* eventHeader = (ovrEventHeader*)(&eventDataBuffer);
        ovrResult res = vrapi_PollEvent(eventHeader);
        if (res != ovrSuccess) {
            break;
        }

        switch (eventHeader->EventType) {
            case VRAPI_EVENT_DATA_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_DATA_LOST");
                break;
            case VRAPI_EVENT_VISIBILITY_GAINED:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_GAINED");
                break;
            case VRAPI_EVENT_VISIBILITY_LOST:
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_VISIBILITY_LOST");
                break;
            case VRAPI_EVENT_FOCUS_GAINED:
                // FOCUS_GAINED is sent when the application is in the foreground and has
                // input focus. This may be due to a system overlay relinquishing focus
                // back to the application.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_GAINED");
                break;
            case VRAPI_EVENT_FOCUS_LOST:
                // FOCUS_LOST is sent when the application is no longer in the foreground and
                // therefore does not have input focus. This may be due to a system overlay taking
                // focus from the application. The application should take appropriate action when
                // this occurs.
                ALOGV("vrapi_PollEvent: Received VRAPI_EVENT_FOCUS_LOST");
                break;
            default:
                ALOGV("vrapi_PollEvent: Unknown event");
                break;
        }
    }
}

