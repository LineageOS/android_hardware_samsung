LOCAL_PATH:= $(call my-dir)

# Host static library
include $(CLEAR_VARS)
LOCAL_SRC_FILES := dtbimg.c
LOCAL_STATIC_LIBRARIES := libfdt
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libdtbimg
LOCAL_MODULE := libdtbimg
include $(BUILD_HOST_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := mkbootimg.c
LOCAL_STATIC_LIBRARIES := libdtbimg libfdt libcrypto_static

LOCAL_MODULE := mkdtbhbootimg

include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := unpackbootimg.c
LOCAL_MODULE := unpackdtbhbootimg
include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := mkdtbimg.c
LOCAL_STATIC_LIBRARIES := libdtbimg libfdt

LOCAL_MODULE := dtbhtoolExynos

include $(BUILD_HOST_EXECUTABLE)

# Target static library
include $(CLEAR_VARS)
LOCAL_SRC_FILES := dtbimg.c
LOCAL_STATIC_LIBRARIES := libfdt
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libdtbimg
LOCAL_MODULE := libdtbimg
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := mkbootimg.c
LOCAL_STATIC_LIBRARIES := libdtbimg libfdt libcrypto_static libcutils libc
LOCAL_MODULE := utility_mkdtbhbootimg
LOCAL_MODULE_STEM := mkdtbhbootimg
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_UNSTRIPPED_PATH := $(PRODUCT_OUT)/symbols/utilities
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := unpackbootimg.c
LOCAL_STATIC_LIBRARIES := libcutils libc
LOCAL_MODULE := utility_unpackdtbhbootimg
LOCAL_MODULE_STEM := unpackdtbhbootimg
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_UNSTRIPPED_PATH := $(PRODUCT_OUT)/symbols/utilities
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/utilities
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := unpackdtbhimg.c
LOCAL_MODULE := unpackdtbhimg
include $(BUILD_HOST_EXECUTABLE)

$(call dist-for-goals,dist_files,$(LOCAL_BUILT_MODULE))
