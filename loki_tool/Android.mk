LOCAL_PATH := $(call my-dir)

# build static binary
include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wno-pointer-arith -Wno-unused-result -Wno-sign-compare
LOCAL_SRC_FILES := loki_flash.c loki_patch.c loki_find.c loki_unlok.c main.c
LOCAL_MODULE := loki_tool_static
LOCAL_MODULE_STEM := loki_tool
LOCAL_FORCE_STATIC_EXECUTABLE := true
include $(BUILD_EXECUTABLE)

# build host binary
include $(CLEAR_VARS)
LOCAL_CFLAGS := -Wno-pointer-arith -Wno-unused-result -Wno-sign-compare
LOCAL_SRC_FILES := loki_flash.c loki_patch.c loki_find.c loki_unlok.c main.c
LOCAL_MODULE := loki_tool
include $(BUILD_HOST_EXECUTABLE)
