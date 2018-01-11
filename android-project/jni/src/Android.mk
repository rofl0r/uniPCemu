LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

APP_OPTIM := release

LOCAL_MODULE := main

SDL_PATH := ../SDL2
SDL_net_PATH := ../SDL2_net

ROOTPATH = $(LOCAL_PATH)/../../../UniPCemu

LOCAL_C_INCLUDES := $(LOCAL_PATH)/$(SDL_PATH)/include $(ROOTPATH) $(ROOTPATH)/../commonemuframework

LOCAL_CFLAGS := -DSDL2 -DUNIPCEMU

PLATFORM = custom
include $(LOCAL_PATH)/../../../UniPCemu/Makefile

# Add your application source files here...
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	$(patsubst %.o,$(ROOTPATH)/%.c,$(OBJS))

LOCAL_SHARED_LIBRARIES := SDL2

ifeq (1,$(useSDL2_net))
LOCAL_SHARED_LIBRARIES := $(LOCAL_SHARED_LIBRARIES) SDL2_net
LOCAL_CFLAGS := $(LOCAL_CFLAGS) -DSDL2_NET
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES) $(LOCAL_PATH)/$(SDL_net_PATH)
endif


LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

#Profiler support block
ifneq (,$(findstring NDK_PROFILE,$(MAKECMDGOALS)))
IS_PROFILE = 1
endif

ifneq (,$(findstring NDK_LINEPROFILE,$(MAKECMDGOALS)))
#Profiling detected
IS_PROFILE = 1
endif

ifeq (,$(NDK_PROFILE))
#Make us equal for easy double support!
NDK_PROFILE = $(NDK_LINEPROFILE)
endif

#Check for empty profile target!
ifeq ($(IS_PROFILE),1)
ifeq ($(NDK_PROFILE),)
$(error Please specify the directory containing the android-ndk-profiler directory, which contains the profiler's Android.mk file, by specifying "NDK_PROFILE=YOURPATHHERE"(without quotes). Replace YOURPATHHERE with the ndk profiler Android.mk directory path. This is usually the sources path(where android-ndk-profiler/Android.mk is located after relocation of the jni folder content). Use NDK_LINEPROFILE instead to specify line-by-line profiling. )
endif
endif

#Apply profile support!
PROFILE_CFLAGS = -pg
ifneq (,$(NDK_LINEPROFILE))
#Enable line profiling!
PROFILE_CFLAGS := $(PROFILE_CFLAGS) -l
endif

#To apply the profiler data itself!
ifneq (,$(NDK_PROFILE))
LOCAL_CFLAGS := $(PROFILE_CFLAGS) -DNDK_PROFILE $(LOCAL_CFLAGS)
LOCAL_STATIC_LIBRARIES := $(LOCAL_STATIC_LIBRARIES) android-ndk-profiler
endif
#end of profiler support block

include $(BUILD_SHARED_LIBRARY)

#Apply profile support!
ifneq (,$(NDK_PROFILE))
#Below commented our searched NDK_MODULE_PATH for the folder name specified and includes it's Android.mk file for us. We do this manually as specified by our variable!
$(call import-add-path,$(NDK_PROFILE))
$(call import-module,android-ndk-profiler)
endif