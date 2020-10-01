ifeq ($(BOARD_HAVE_SAMSUNG_WIFI),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    macloader.c

LOCAL_SHARED_LIBRARIES := \
    liblog libutils

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_MODULE := macloader
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)

endif
