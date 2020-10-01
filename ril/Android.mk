#
# Copyright (C) 2013 The Android Open-Source Project
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
#

RIL_PATH := $(call my-dir)

ifeq ($(BOARD_VENDOR),samsung)

# libril
ifeq ($(BOARD_PROVIDES_LIBRIL),true)
ifneq ($(filter m7450 mdm9x35 ss333 tss310 xmm6260 xmm6262 xmm6360 xmm7260,$(BOARD_MODEM_TYPE)),)
include $(RIL_PATH)/libril/Android.mk
endif
endif

# ril client
SECRIL_CLIENT_DIRS := libsecril-client libsecril-client-sap
include $(foreach client_dirs,$(SECRIL_CLIENT_DIRS),$(RIL_PATH)/$(client_dirs)/Android.mk)

endif

