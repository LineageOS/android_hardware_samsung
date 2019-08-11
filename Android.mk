# Copyright (C) 2012 The Android Open Source Project
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

SAM_ROOT := $(call my-dir)

# Exynos 4
ifeq ($(TARGET_BOARD_PLATFORM),exynos4)
ifeq ($(TARGET_SOC),exynos4210)
include $(SAM_ROOT)/exynos4210.mk
endif
ifeq ($(TARGET_SOC),exynos4x12)
include $(SAM_ROOT)/exynos4x12.mk
endif
endif

# Exynos 3
ifeq ($(TARGET_BOARD_PLATFORM),s5pc110)
include $(SAM_ROOT)/s5pc110.mk
endif

# Wifi
ifeq ($(BOARD_HAVE_SAMSUNG_WIFI),true)
include $(SAM_ROOT)/macloader/Android.mk
include $(SAM_ROOT)/wifiloader/Android.mk
endif

# Loki
ifeq ($(strip $(TARGET_NEEDS_LOKI)),true)
include $(SAM_ROOT)/loki_tool/Android.mk
endif

ifeq ($(BOARD_VENDOR),samsung)
include $(SAM_ROOT)/AdvancedDisplay/Android.mk
include $(SAM_ROOT)/audio/Android.mk
include $(SAM_ROOT)/consumerir/Android.mk
include $(SAM_ROOT)/dtbhtool/Android.mk
include $(SAM_ROOT)/fingerprint/Android.mk
include $(SAM_ROOT)/hidl/Android.mk
include $(SAM_ROOT)/liblights/Android.mk
include $(SAM_ROOT)/modemloader/Android.mk
include $(SAM_ROOT)/power/Android.mk
include $(SAM_ROOT)/ril/Android.mk
endif
