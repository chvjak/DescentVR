LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := descentvr

LOCAL_CFLAGS += -std=c99 -DANDROID_NDK -DDISABLE_IMPORTGL -DOGLES -DNASM -DNETWORK

DESCENT_PATH := $(LOCAL_PATH)/../DescentCore/src/

LOCAL_C_INCLUDES +=  $(DESCENT_PATH)/lib/ \
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

LOCAL_SRC_FILES := main.c \
                   vr.c \
                   $(FILE_LIST:$(LOCAL_PATH)/%=%)

LOCAL_LDLIBS := -lEGL -lOpenSLES -landroid -llog -ldl  -lGLESv3

LOCAL_LDFLAGS := -u ANativeActivity_onCreate -fuse-ld=bfd

LOCAL_STATIC_LIBRARIES := android_native_app_glue
LOCAL_SHARED_LIBRARIES := vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
$(call import-module,android/native_app_glue)
