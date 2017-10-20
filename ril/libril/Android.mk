# Copyright 2006 The Android Open Source Project

ifeq ($(BOARD_PROVIDES_LIBRIL),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ril.cpp \
    ril_event.cpp\
    RilSocket.cpp \
    RilSapSocket.cpp \

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libbinder \
    libcutils \
    libhardware_legacy \
    librilutils \

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-c-nano-enable_malloc \

LOCAL_CFLAGS :=

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_MULTI_SIM
    LOCAL_CFLAGS += -DANDROID_SIM_COUNT_2
endif

ifneq ($(filter xmm6262 xmm6360,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS += -DMODEM_TYPE_XMM6262
endif
ifeq ($(BOARD_MODEM_TYPE),xmm6260)
LOCAL_CFLAGS += -DMODEM_TYPE_XMM6260
endif
ifneq ($(filter m7450 mdm9x35 ss333 tss310 xmm7260,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS += -DSAMSUNG_NEXT_GEN_MODEM
endif

ifeq ($(BOARD_MODEM_NEEDS_VIDEO_CALL_FIELD), true)
LOCAL_CFLAGS += -DNEEDS_VIDEO_CALL_FIELD
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += external/nanopb-c
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/../include

LOCAL_MODULE:= libril
LOCAL_CLANG := true
LOCAL_SANITIZE := integer

include $(BUILD_SHARED_LIBRARY)


# For RdoServD which needs a static library
# =========================================
ifneq ($(ANDROID_BIONIC_TRANSITION),)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
    ril.cpp

LOCAL_STATIC_LIBRARIES := \
    libutils_static \
    libcutils \
    librilutils_static \
    libprotobuf-c-nano-enable_malloc

LOCAL_CFLAGS :=

ifneq ($(filter xmm6262 xmm6360,$(BOARD_MODEM_TYPE)),)
LOCAL_CFLAGS += -DMODEM_TYPE_XMM6262
endif
ifeq ($(BOARD_MODEM_TYPE),xmm6260)
LOCAL_CFLAGS += -DMODEM_TYPE_XMM6260
endif

LOCAL_MODULE:= libril_static

include $(BUILD_STATIC_LIBRARY)
endif # ANDROID_BIONIC_TRANSITION
endif # BOARD_PROVIDES_LIBRIL
