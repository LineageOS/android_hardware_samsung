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

LOCAL_SRC_FILES := \
    BiometricsFingerprint.cpp \
    service.cpp

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libhardware \
    libhidlbase \
    libhidltransport \
    liblog \
    libutils \
    android.hardware.biometrics.fingerprint@2.1

ifeq ($(TARGET_SEC_FP_USES_PERCENTAGE_SAMPLES),true)
    LOCAL_CFLAGS += -DUSES_PERCENTAGE_SAMPLES
endif

LOCAL_MODULE := android.hardware.biometrics.fingerprint@2.1-service.samsung
LOCAL_INIT_RC := android.hardware.biometrics.fingerprint@2.1-service.samsung.rc
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)
