#
# Copyright (C) 2019 The LineageOS Project
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

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_VENDOR_MODULE := true

LOCAL_MODULE := android.hardware.vibrator@1.0-service.samsung-haptic
LOCAL_INIT_RC := android.hardware.vibrator@1.0-service.samsung-haptic.rc
LOCAL_SRC_FILES := service.cpp \
    Vibrator.cpp

LOCAL_SHARED_LIBRARIES := \
    libbase \
    liblog \
    libdl \
    libhidlbase \
    libhidltransport \
    libhardware \
    libutils \
    android.hardware.vibrator@1.0

include $(BUILD_EXECUTABLE)
