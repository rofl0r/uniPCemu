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

LOCAL_STATIC_LIBRARIES := SDL2_static

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

#Check for empty profile target!
ifneq (,$(findstring profile,$(MAKECMDGOALS)))
ifeq ($(profile),)
$(error Please specify the directory containing the android-ndk-profiler directory, which contains the profiler's Android.mk file, by specifying "profile=YOURPATHHERE"(without quotes). Replace YOURPATHHERE with the ndk profiler Android.mk directory path. This is usually the sources path(where android-ndk-profiler/Android.mk is located after relocation of the jni folder content). )
endif
endif

#Apply profile support!
ifneq (,$(profile))
LOCAL_CFLAGS := -pg -DNDK_PROFILE $(LOCAL_CFLAGS)
LOCAL_STATIC_LIBRARIES := $(LOCAL_STATIC_LIBRARIES) android-ndk-profiler
endif

include $(BUILD_SHARED_LIBRARY)
$(call import-module,SDL)
LOCAL_PATH := $(call my-dir)

#Apply profile support!
ifneq (,$(profile))
#Below commented our searched NDK_MODULE_PATH for the folder name specified and includes it's Android.mk file for us. We do this manually as specified by our variable!
$(call import-add-path,$(profile))
$(call import-module,android-ndk-profiler)
endif