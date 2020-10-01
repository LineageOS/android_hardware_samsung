LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(filter-out exynos4,$(TARGET_BOARD_PLATFORM)),)
include   $(LOCAL_PATH)/exynos4/Android.mk
endif
