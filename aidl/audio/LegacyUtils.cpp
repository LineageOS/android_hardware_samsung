/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "AHAL_LegacyUtils"

#include "LegacyUtils.h"

#include <cstdlib>
#include <memory>

#include <android-base/logging.h>
#include <media/AidlConversionCppNdk.h>
#include <system/audio.h>

using aidl::android::media::audio::common::AudioDevice;

namespace aidl::android::hardware::audio::core::legacy {

bool toLegacyDevices(const std::vector<AudioDevice>& devices, audio_devices_t* types,
                     std::string* address) {
    *types = AUDIO_DEVICE_NONE;
    address->clear();
    for (size_t i = 0; i < devices.size(); i++) {
        audio_devices_t type;
        std::string deviceAddress;
        if (::aidl::android::aidl2legacy_AudioDevice_audio_device(
                    devices[i], &type, &deviceAddress) != ::android::OK) {
            LOG(ERROR) << __func__ << ": unable to convert " << devices[i].toString();
            return false;
        }
        *types = static_cast<audio_devices_t>(*types | type);
        // Like AudioFlinger, only pass the address of the first device
        if (i == 0) {
            *address = std::move(deviceAddress);
        }
    }
    return true;
}

::android::AudioParameter addressToParameters(audio_devices_t type, const std::string& address) {
    if (address.empty()) {
        return ::android::AudioParameter();
    }
    std::unique_ptr<char, decltype(&free)> keyValuePairs(
            audio_device_address_to_parameter(type, address.c_str()), &free);
    return ::android::AudioParameter(::android::String8(keyValuePairs.get()));
}

int setParameters(audio_hw_device_t* device, const ::android::AudioParameter& parameters) {
    const ::android::String8 keyValuePairs = parameters.toString();
    LOG(DEBUG) << __func__ << ": " << keyValuePairs.c_str();
    int ret = device->set_parameters(device, keyValuePairs.c_str());
    if (ret != 0) {
        LOG(ERROR) << __func__ << ": \"" << keyValuePairs.c_str() << "\" failed: " << ret;
    }
    return ret;
}

int setParameters(audio_stream* stream, const ::android::AudioParameter& parameters) {
    const ::android::String8 keyValuePairs = parameters.toString();
    LOG(DEBUG) << __func__ << ": " << keyValuePairs.c_str();
    int ret = stream->set_parameters(stream, keyValuePairs.c_str());
    if (ret != 0) {
        LOG(ERROR) << __func__ << ": \"" << keyValuePairs.c_str() << "\" failed: " << ret;
    }
    return ret;
}

}  // namespace aidl::android::hardware::audio::core::legacy
