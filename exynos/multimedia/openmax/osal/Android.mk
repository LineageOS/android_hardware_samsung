LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
	SEC_OSAL_Android.cpp \
	SEC_OSAL_Event.c \
	SEC_OSAL_Queue.c \
	SEC_OSAL_ETC.c \
	SEC_OSAL_Mutex.c \
	SEC_OSAL_Thread.c \
	SEC_OSAL_Memory.c \
	SEC_OSAL_Semaphore.c \
	SEC_OSAL_Library.c \
	SEC_OSAL_Log.c

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE := libsecosal

LOCAL_CFLAGS :=

ifeq ($(TARGET_NEEDS_EXYNOS4_ENHANCEMENTS),true)
LOCAL_CFLAGS += -DEXYNOS4_ENHANCEMENTS
endif

ifeq ($(BOARD_USE_V4L2), false)
ifeq ($(BOARD_USE_S3D_SUPPORT), true)
LOCAL_CFLAGS += -DS3D_SUPPORT
endif
endif

LOCAL_STATIC_LIBRARIES :=
LOCAL_SHARED_LIBRARIES := libcutils libutils \
	libui \
	libhardware \
	libandroid_runtime \
	libbinder \
	libmedia

LOCAL_C_INCLUDES := $(SEC_OMX_INC)/khronos \
	$(SEC_OMX_INC)/sec \
	$(SEC_OMX_TOP)/osal \
	$(SEC_OMX_COMPONENT)/common \
	$(SEC_OMX_COMPONENT)/video/dec

include $(BUILD_STATIC_LIBRARY)
