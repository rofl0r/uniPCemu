LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL2

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include ../UniPCemu

LOCAL_CFLAGS := -DSDL2

ROOTPATH = ../UniPCemu

include ../UniPCemu/Makefile

# Add your application source files here...
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	$(patsubst %.o,../../../UniPCemu/%.c,$(OBJS))

LOCAL_STATIC_LIBRARIES := SDL2_static

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

ifneq (,$(findstring profile,$(MAKECMDGOALS)))
LOCAL_CFLAGS := -pg -DNDK_PROFILE $(LOCAL_CFLAGS)
LOCAL_STATIC_LIBRARIES := $(LOCAL_STATIC_LIBRARIES) android-ndk-profiler
endif

include $(BUILD_SHARED_LIBRARY)
$(call import-module,SDL)LOCAL_PATH := $(call my-dir)
ifneq (,$(findstring profile,$(MAKECMDGOALS)))
$(call import-module,android-ndk-profiler)
endif