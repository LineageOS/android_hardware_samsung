/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <vector>

#include <aidl/android/media/audio/common/AudioDevice.h>
#include <hardware/audio.h>
#include <media/AudioParameter.h>

namespace aidl::android::hardware::audio::core::legacy {

/*
 * Converts a list of devices into a legacy device type mask and the address of
 * the first device, the way AudioFlinger routes streams of HALs without audio
 * patch support.
 */
bool toLegacyDevices(const std::vector<::aidl::android::media::audio::common::AudioDevice>& devices,
                     audio_devices_t* types, std::string* address);

/* Returns the legacy key value pairs carrying the address of a device. */
::android::AudioParameter addressToParameters(audio_devices_t type, const std::string& address);

/* Sends key value pairs to the legacy device, logging failures. */
int setParameters(audio_hw_device_t* device, const ::android::AudioParameter& parameters);

/* Sends key value pairs to the legacy stream, logging failures. */
int setParameters(audio_stream* stream, const ::android::AudioParameter& parameters);

}  // namespace aidl::android::hardware::audio::core::legacy
