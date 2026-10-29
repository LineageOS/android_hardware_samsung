/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungVoice"

#include "voice/AudioCallSlot.h"

#include <android-base/logging.h>
#include <android/hardware/audio/7.0/IDevicesFactory.h>

namespace aidl::android::hardware::radio::samsung {

using ::android::sp;
using ::android::hardware::hidl_vec;
using ::android::hardware::audio::V7_0::IDevice;
using ::android::hardware::audio::V7_0::IDevicesFactory;
using ::android::hardware::audio::V7_0::IPrimaryDevice;
using ::android::hardware::audio::V7_0::ParameterValue;
using ::android::hardware::audio::V7_0::Result;

AudioCallSlot& AudioCallSlot::get() {
    static AudioCallSlot instance;
    return instance;
}

sp<IDevice> AudioCallSlot::device() {
    if (mDevice != nullptr) {
        return mDevice;
    }
    // Don't hold up the call list on the audio HAL.
    const auto factory = IDevicesFactory::tryGetService();
    if (factory == nullptr) {
        LOG(ERROR) << "Audio HAL not available";
        return nullptr;
    }
    const auto ret =
            factory->openPrimaryDevice([this](Result result, const sp<IPrimaryDevice>& device) {
                if (result == Result::OK) {
                    mDevice = device;
                } else {
                    LOG(ERROR) << "Failed to open the primary audio device: " << toString(result);
                }
            });
    if (!ret.isOk()) {
        LOG(ERROR) << "Failed to open the primary audio device: " << ret.description();
    }
    return mDevice;
}

bool AudioCallSlot::setParameters(const hidl_vec<ParameterValue>& parameters) {
    // The audio HAL may have restarted.
    for (int attempt = 0; attempt < 2; attempt++) {
        const auto device = this->device();
        if (device == nullptr) {
            return false;
        }
        const auto ret = device->setParameters({}, parameters);
        if (ret.isOk()) {
            if (ret != Result::OK) {
                LOG(ERROR) << "Failed to set " << parameters[0].key << ": "
                           << toString(Result(ret));
            }
            return ret == Result::OK;
        }
        LOG(WARNING) << "Audio HAL call failed: " << ret.description();
        mDevice = nullptr;
    }
    return false;
}

bool AudioCallSlot::set(int slot) {
    // Voice session IDs of VOICEMMODE1 and VOICEMMODE2, one per SIM.
    static constexpr const char* kVsid[] = {"297816064", "299651072"};
    std::lock_guard lock(mMutex);
    bool ok = true;
    if (mSlot != -1) {
        ok = setParameters({{"g_call_state", "1"}});
        for (const char* vsid : kVsid) {
            ok = setParameters({{"vsid", vsid}, {"call_state", "1"}}) && ok;
        }
    }
    // The audio HAL keeps the session pending and starts it once it enters IN_CALL.
    if (ok && slot >= 0) {
        ok = setParameters({{"g_call_sim_slot", slot == 0 ? "0x01" : "0x02"}}) &&
             setParameters({{"g_call_state", "2"}}) &&
             setParameters({{"vsid", kVsid[slot]}, {"call_state", "2"}});
    }
    if (ok) {
        LOG(INFO) << "CS voice session slot " << mSlot << " -> " << slot;
    }
    mSlot = ok ? slot : -2;
    return ok;
}

}  // namespace aidl::android::hardware::radio::samsung
