# Copyright (C) 2017 The LineageOS Project
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

ifeq ($(TARGET_AUDIOHAL_VARIANT),samsung)

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	audience.c \
	audio_hw.c \
	compress_offload.c \
	ril_interface.c \
	voice.c

# TODO: remove resampler if possible when AudioFlinger supports downsampling from 48 to 8
LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libaudioutils \
	libhardware \
	libprocessgroup \
	libtinyalsa \
	libtinycompress \
	libaudioroute \
	libdl \
	libsecril-client


LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/include \
	external/tinyalsa/include \
	external/tinycompress/include \
	hardware/libhardware/include \
	hardware/samsung/ril/libsecril-client \
	$(call include-path-for, audio-utils) \
	$(call include-path-for, audio-route) \
	$(call include-path-for, audio-effects)

LOCAL_CFLAGS := -Werror -Wall
LOCAL_CFLAGS += -DPREPROCESSING_ENABLED

LOCAL_MODULE := audio.primary.$(TARGET_BOOTLOADER_BOARD_NAME)

LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_MODULE_TAGS := optional

LOCAL_VENDOR_MODULE := true

include $(BUILD_SHARED_LIBRARY)

endif
