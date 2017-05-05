LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

APP_OPTIM := release

LOCAL_MODULE := main

SDL_PATH := ../SDL2

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include ../UniPCemu

LOCAL_CFLAGS := -DSDL2

ROOTPATH = ../UniPCemu

include ../UniPCemu/Makefile

# Add your application source files here...
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	$(patsubst %.o,../../../UniPCemu/%.c,$(OBJS))

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

#Check for empty profile target!
ifneq (,$(findstring profile,$(MAKECMDGOALS)))
ifeq ($(profile),)
$(error Please specify the NDK directory containing all NDK files, by specifying "profile=YOURPATHHERE"(without quotes). Replace YOURPATHHERE with the ndk directory path. This is usually the sources path. )
endif
endif

#Apply profile support!
ifneq (,$(profile))
$(info Profile build selected.)
LOCAL_CFLAGS := -pg -DNDK_PROFILE $(LOCAL_CFLAGS)
LOCAL_STATIC_LIBRARIES := $(LOCAL_STATIC_LIBRARIES) android-ndk-profiler
endif

include $(BUILD_SHARED_LIBRARY)

#Apply profile support!
ifneq (,$(profile))
$(call import-add-path,$(profile))
$(call import-module,android-ndk-profiler)
endif
