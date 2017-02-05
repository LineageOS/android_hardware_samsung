FP_ROOT := $(call my-dir)

ifneq ($(TARGET_SEC_FP_HAL_VARIANT),)
include $(FP_ROOT)/$(TARGET_SEC_FP_HAL_VARIANT)/Android.mk
endif
