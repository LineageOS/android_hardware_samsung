LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    fingerprint.c

LOCAL_SHARED_LIBRARIES := \
    libhardware liblog

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE := fingerprint.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)
