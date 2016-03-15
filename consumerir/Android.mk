# Copyright (C) 2016 The Android Open Source Project
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

# HAL module implementation stored in
# hw/<POWERS_HARDWARE_MODULE_ID>.<ro.hardware>.so

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(IR_HAL_SUFFIX),)
IR_HAL_SUFFIX := $(TARGET_BOARD_PLATFORM)
endif

LOCAL_MODULE := consumerir.$(IR_HAL_SUFFIX)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := consumerir.c
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_MODULE_TAGS := optional

ifeq ($(BOARD_IR_HAS_ONE_FREQ_RANGE),true)
LOCAL_CFLAGS += -DUSE_ONE_FREQ_RANGE
endif

ifeq ($(BOARD_USES_MS_IR_SIGNAL),true)
LOCAL_CFLAGS += -DMS_IR_SIGNAL
endif

include $(BUILD_SHARED_LIBRARY)
