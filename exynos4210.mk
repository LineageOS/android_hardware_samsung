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

ifeq ($(TARGET_BOARD_PLATFORM),exynos4)
ifeq ($(TARGET_SOC),exynos4210)

include $(TARGET_HAL_PATH)/Android.mk
include $(SAM_ROOT)/exynos/multimedia/Android.mk
include $(SAM_ROOT)/exynos4/exynos4210/Android.mk

#NFC
ifeq ($(BOARD_HAVE_NFC),true)
include $(SAM_ROOT)/exynos4/nfc/Android.mk
endif

endif
endif
