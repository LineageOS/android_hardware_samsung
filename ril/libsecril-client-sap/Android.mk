# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= \
    secril-client-sap.cpp

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libbinder \
    libcutils \
    libhardware_legacy \
    liblog

LOCAL_CFLAGS := 

LOCAL_MODULE:= libsecril-client-sap
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
