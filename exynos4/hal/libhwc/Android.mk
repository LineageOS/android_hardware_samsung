# Copyright (C) 2010 Samsung Electronics Co.,LTD., for the ODROID code
# Copyright (C) 2012 The Android Open Source Project, for the samsung_slsi/exynos5 code
# Copyright (C) 2013 Paul Kocialkowski, for the V4L2 code
# Copyright (C) 2017 The NamelessRom Project, for making it work
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


LOCAL_PATH := $(call my-dir)

# HAL module implemenation stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SHARED_LIBRARIES := liblog libEGL libcutils libhardware libutils libsync libfimg

LOCAL_SRC_FILES := hwcomposer.cpp \
                   hwcomposer_vsync.cpp \
                   window.cpp \
                   utils.cpp \
                   v4l2.cpp

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include \
                    $(TOP)/system/core/libsync/include

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_CFLAGS:= -DLOG_TAG=\"hwcomposer\"
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)
