#
# Copyright (C) 2020 The LineageOS Project
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
    Power.cpp \
    service.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    hardware/samsung/hidl/light/include

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libbinder \
    libhidlbase \
    libhidltransport \
    libutils \
    android.hardware.power@1.0 \
    vendor.lineage.power@1.0

LOCAL_STATIC_LIBRARIES := libc++fs

LOCAL_MODULE := android.hardware.power@1.0-service.exynos
LOCAL_INIT_RC := android.hardware.power@1.0-service.exynos.rc
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_OWNER := samsung
LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)
