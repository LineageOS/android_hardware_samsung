ifeq ($(BOARD_KERNEL_SEPARATED_DT),true)
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
	dtbtool.c \
	libfdt/fdt.c \
	libfdt/fdt_ro.c \
	libfdt/fdt_wip.c \
	libfdt/fdt_sw.c \
	libfdt/fdt_rw.c \
	libfdt/fdt_strerror.c \
	libfdt/fdt_empty_tree.c
LOCAL_CFLAGS += -Wall
LOCAL_C_INCLUDES := $(LOCAL_PATH)/libfdt
LOCAL_MODULE := dtbToolExynos
LOCAL_MODULE_TAGS := optional
include $(BUILD_HOST_EXECUTABLE)

endif
