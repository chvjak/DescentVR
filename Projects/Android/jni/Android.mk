LOCAL_PATH := $(call my-dir)

FLUIDSYNTH_PATH := $(FLUIDSYNTH_PATH)/

include $(CLEAR_VARS)
LOCAL_MODULE := fluidsynth
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libfluidsynth.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := instpatch
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libinstpatch-1.0.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := gobject
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libgobject-2.0.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := gthread
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libgthread-2.0.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := glib
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libglib-2.0.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := sndfile
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libsndfile.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := oboe
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/liboboe.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := pcre
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libpcre.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := vorbisenc
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libvorbisenc.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := FLAC
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libFLAC.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := opus
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libopus.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := vorbis
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libvorbis.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := ogg
LOCAL_SRC_FILES := $(FLUIDSYNTH_PATH)/lib/arm64-v8a/libogg.so
include $(PREBUILT_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := omp
LOCAL_SRC_FILES := C:/Users/Dmytro/AppData/Local/Android/Sdk/ndk/22.1.7171670/toolchains/llvm/prebuilt/windows-x86_64/lib64/clang/11.0.5/lib/linux/aarch64/libomp.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := descentvr

LOCAL_CFLAGS += -DANDROID_NDK -DDISABLE_IMPORTGL -DOGLES -DNASM -DNETWORK

DESCENT_PATH := $(LOCAL_PATH)/../DescentCore/src/

GLM_PATH := $(LOCAL_PATH)/../../../../

LOCAL_C_INCLUDES += $(FLUIDSYNTH_PATH)/include/ \
                    $(GLM_PATH) \
                    $(DESCENT_PATH)/lib/ \
                    $(LOCAL_PATH)/
FILE_LIST := \
 $(wildcard $(DESCENT_PATH)2d/*.c) \
 $(wildcard $(DESCENT_PATH)2d/ogles/*.c) \
 $(wildcard $(DESCENT_PATH)3d/*.c) \
 $(wildcard $(DESCENT_PATH)3d/ogles/*.c) \
 $(wildcard $(DESCENT_PATH)cfile/*.c) \
 $(wildcard $(DESCENT_PATH)fix/*.c) \
 $(wildcard $(DESCENT_PATH)iff/*.c) \
 $(wildcard $(DESCENT_PATH)io/*.c) \
 $(wildcard $(DESCENT_PATH)main/*.c) \
 $(wildcard $(DESCENT_PATH)main/android/*.c) \
 $(wildcard $(DESCENT_PATH)mem/*.c) \
 $(wildcard $(DESCENT_PATH)misc/*.c) \
 $(wildcard $(DESCENT_PATH)texmap/*.c) \
 $(wildcard $(DESCENT_PATH)texmap/ogles/*.c) \
 $(wildcard $(DESCENT_PATH)ui/*.c) \
 $(wildcard $(DESCENT_PATH)vecmat/*.c) \
 $(wildcard $(DESCENT_PATH)java-glue/*.c)

LOCAL_SRC_FILES := main.cpp \
                   vr.cpp \
                   ogles3.cpp \
                   $(FILE_LIST:$(LOCAL_PATH)/%=%)

LOCAL_LDLIBS := -lEGL -lOpenSLES -landroid -llog -ldl  -lGLESv3

LOCAL_LDFLAGS := -u ANativeActivity_onCreate -fuse-ld=bfd

LOCAL_STATIC_LIBRARIES := android_native_app_glue
LOCAL_SHARED_LIBRARIES := vrapi fluidsynth omp instpatch gobject gthread glib sndfile oboe c++_shared pcre vorbisenc FLAC opus vorbis ogg

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,android/native_app_glue)
