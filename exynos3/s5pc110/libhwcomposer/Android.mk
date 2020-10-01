# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)
# HAL module implemenation, not prelinked and stored in
# hw/<COPYPIX_HARDWARE_MODULE_ID>.<ro.product.board>.so

include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog libcutils libEGL libGLESv1_CM libhardware libhardware_legacy

LOCAL_CFLAGS := -DLOG_TAG=\"hwcomposer\"
ifeq ($(BOARD_USES_HDMI),true)
    LOCAL_CFLAGS += -DBOARD_USES_HDMI
endif

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := SecHWCUtils.cpp SecHWC.cpp

ifeq ($(BOARD_CUSTOM_VSYNC_IOCTL),true)
    LOCAL_CFLAGS += -DVSYNC_IOCTL
endif

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
