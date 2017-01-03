LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

LOCAL_CFLAGS := -DSDL2

ROOTPATH = ../UniPCemu

include ../UniPCemu/Makefile

LOCAL_SRC_FILES := $(patsubst %.o,../../../UniPCemu/%.c,$(OBJS))

LOCAL_STATIC_LIBRARIES := SDL2_static

include $(BUILD_SHARED_LIBRARY)
$(call import-module,SDL)LOCAL_PATH := $(call my-dir)
