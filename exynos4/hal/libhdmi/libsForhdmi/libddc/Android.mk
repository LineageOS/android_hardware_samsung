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

ifeq ($(BOARD_USES_HDMI),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_SRC_FILES := libddc.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../../../include

ifeq ($(BOARD_HDMI_DDC_CH), DDC_CH_I2C_7)
LOCAL_CFLAGS  += -DDDC_CH_I2C_7
endif

ifeq ($(BOARD_HDMI_DDC_CH), DDC_CH_I2C_1)
LOCAL_CFLAGS  += -DDDC_CH_I2C_1
endif

ifeq ($(BOARD_HDMI_DDC_CH), DDC_CH_I2C_2)
LOCAL_CFLAGS  += -DDDC_CH_I2C_2
endif

LOCAL_MODULE := libddc
include $(BUILD_SHARED_LIBRARY)

endif
