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

ifeq ($(filter-out exynos4,$(TARGET_BOARD_PLATFORM)),)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES := liblog libutils libcutils
ifeq ($(BOARD_SUPPORT_SYSMMU),true)
LOCAL_SHARED_LIBRARIES+= libMali
endif

ifeq ($(BOARD_SUPPORT_SYSMMU),true)
LOCAL_CFLAGS+=-DBOARD_SUPPORT_SYSMMU
endif

ifeq ($(TARGET_SOC),exynos4210)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4210
endif

ifeq ($(TARGET_SOC),exynos4x12)
LOCAL_CFLAGS += -DSAMSUNG_EXYNOS4x12
endif

ifeq ($(BOARD_USE_V4L2),true)
LOCAL_CFLAGS += -DBOARD_USE_V4L2
endif

LOCAL_CFLAGS  += \
	-DDEFAULT_FB_NUM=$(DEFAULT_FB_NUM)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../include \
	framework/base/include

LOCAL_SRC_FILES := SecFimc.cpp

LOCAL_MODULE := libfimc
include $(BUILD_SHARED_LIBRARY)

endif
