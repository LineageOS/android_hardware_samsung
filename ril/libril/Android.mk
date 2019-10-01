# Copyright 2006 The Android Open Source Project

ifeq ($(BOARD_PROVIDES_LIBRIL),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_VENDOR_MODULE := true

LOCAL_SRC_FILES:= \
    ril.cpp \
    ril_event.cpp\
    RilSapSocket.cpp \
    ril_service.cpp \
    sap_service.cpp

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libutils \
    libcutils \
    libhardware_legacy \
    librilutils \
    android.hardware.radio@1.0 \
    android.hardware.radio.deprecated@1.0 \
    libhidlbase

LOCAL_STATIC_LIBRARIES := \
    libprotobuf-c-nano-enable_malloc \

LOCAL_CFLAGS += -Wno-unused-parameter

ifeq ($(SIM_COUNT), 2)
    LOCAL_CFLAGS += -DANDROID_MULTI_SIM -DDSDA_RILD1
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

ifeq ($(BOARD_NEEDS_ROAMING_PROTOCOL_FIELD), true)
LOCAL_CFLAGS += -DNEEDS_ROAMING_PROTOCOL_FIELD
endif

ifeq ($(BOARD_NEEDS_IMS_TYPE_FIELD), true)
LOCAL_CFLAGS += -DNEEDS_IMS_TYPE_FIELD
endif

ifneq ($(DISABLE_RILD_OEM_HOOK),)
    LOCAL_CFLAGS += -DOEM_HOOK_DISABLED
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_C_INCLUDES += external/nanopb-c
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/../include

LOCAL_MODULE:= libril
LOCAL_SANITIZE := integer

include $(BUILD_SHARED_LIBRARY)

endif # BOARD_PROVIDES_LIBRIL
