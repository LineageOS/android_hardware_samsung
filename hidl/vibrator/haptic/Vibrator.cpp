/*
 * Copyright (C) 2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.vibrator@1.3-service.samsung-haptic"

#include <log/log.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include <hardware/hardware.h>
#include <hardware/vibrator.h>

#include "Vibrator.h"

#include <cinttypes>
#include <cmath>
#include <fstream>
#include <iostream>

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_3 {
namespace implementation {

/*
 * Write value to path and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value << std::endl;
}

Vibrator::Vibrator() {
    mIntensity = 10000;
}

// SEC Haptic Engine
Return<Status> Vibrator::doHaptic(int timeout, int intensity, int freq, int overdrive) {
    std::string haptic =
            android::base::StringPrintf("4 %d %d %d %d", timeout, intensity, freq, overdrive);
    set("/sys/class/timed_output/vibrator/haptic_engine", haptic);
    set("/sys/class/timed_output/vibrator/enable", 1);

    return Status::OK;
}

// Methods from ::android::hardware::vibrator::V1_0::IVibrator follow.
Return<Status> Vibrator::on(uint32_t timeout_ms) {
    return doHaptic(timeout_ms, mIntensity, 0, 0);
}

Return<Status> Vibrator::off() {
    set("/sys/class/timed_output/vibrator/enable", 0);
    return Status::OK;
}

Return<bool> Vibrator::supportsAmplitudeControl() {
    return true;
}

Return<Status> Vibrator::setAmplitude(uint8_t amplitude) {
    if (amplitude == 0) {
        return Status::BAD_VALUE;
    }
    mIntensity = amplitude * 10000 / 255;
    return Status::OK;
}

Return<void> Vibrator::perform(V1_0::Effect effect, EffectStrength strength, perform_cb _hidl_cb) {
    return perform<decltype(effect)>(effect, strength, _hidl_cb);
}

// Methods from ::android::hardware::vibrator::V1_1::IVibrator follow.

Return<void> Vibrator::perform_1_1(V1_1::Effect_1_1 effect, EffectStrength strength,
                                   perform_cb _hidl_cb) {
    return perform<decltype(effect)>(effect, strength, _hidl_cb);
}

// Methods from ::android::hardware::vibrator::V1_2::IVibrator follow.

Return<void> Vibrator::perform_1_2(V1_2::Effect effect, EffectStrength strength,
                                   perform_cb _hidl_cb) {
    return perform<decltype(effect)>(effect, strength, _hidl_cb);
}

// Methods from ::android::hardware::vibrator::V1_3::IVibrator follow.

Return<bool> Vibrator::supportsExternalControl() {
    return true;
}

Return<Status> Vibrator::setExternalControl(bool enabled) {
    if (mEnabled) {
        LOG(WARNING) << "Setting external control while the vibrator is enabled is "
                        "unsupported!";
        return Status::UNSUPPORTED_OPERATION;
    }

    LOG(INFO) << "ExternalControl: " << mExternalControl << " -> " << enabled;
    mExternalControl = enabled;
    return Status::OK;
}

Return<void> Vibrator::perform_1_3(Effect effect, EffectStrength strength, perform_cb _hidl_cb) {
    return perform<decltype(effect)>(effect, strength, _hidl_cb);
}

// Private methods follow

Return<void> Vibrator::perform(Effect effect, EffectStrength strength, perform_cb _hidl_cb) {
    Status status = Status::OK;
    uint32_t timeMS;
    uint32_t intensity;

    switch (strength) {
        case EffectStrength::LIGHT:
            intensity = 1000;
            break;
        case EffectStrength::STRONG:
            intensity = 5000;
            break;
        default:
            intensity = 3000;
            break;
    }

    switch (effect) {
        case Effect::CLICK:
        case Effect::DOUBLE_CLICK:
            status = doHaptic(7, intensity, 2000, 1);
            timeMS = 7;
            break;
        default:
            status = Status::UNSUPPORTED_OPERATION;
            timeMS = 0;
            break;
    }

    _hidl_cb(status, timeMS);
    return Void();
}

template <typename T>
Return<void> Vibrator::perform(T effect, EffectStrength strength, perform_cb _hidl_cb) {
    auto validRange = hidl_enum_range<T>();
    if (effect < *validRange.begin() || effect > *std::prev(validRange.end())) {
        _hidl_cb(Status::UNSUPPORTED_OPERATION, 0);
        return Void();
    }

    return perform(static_cast<Effect>(effect), strength, _hidl_cb);
}

}  // namespace implementation
}  // namespace V1_3
}  // namespace vibrator
}  // namespace hardware
}  // namespace android
