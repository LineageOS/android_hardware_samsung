/*
 * Copyright (C) 2020 The LineageOS Project
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

#define LOG_TAG "vibrator@1.3-samsung"

#include "Vibrator.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <hardware/hardware.h>
#include <hardware/vibrator.h>

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
static Return<Status> writeNode(const std::string& path, const T& value) {
    std::ofstream node(path);
    if (!node) {
        LOG(ERROR) << "Failed to open: " << path;
        return Status::UNKNOWN_ERROR;
    }

    LOG(DEBUG) << "writeNode node: " << path << " value: " << value;

    node << value << std::endl;
    if (!node) {
        LOG(ERROR) << "Failed to write: " << value;
        return Status::UNKNOWN_ERROR;
    }

    return Status::OK;
}

static bool nodeExists(const std::string& path) {
    std::ofstream f(path.c_str());
    return f.good();
}

Vibrator::Vibrator() {
    bool ok;

    ok = nodeExists(VIBRATOR_TIMEOUT_PATH);
    if (ok) {
        mIsTimedOutVibriator = true;
    }

    ok = nodeExists(VIBRATOR_INTENSITY_PATH);
    if (ok) {
        mhasTimedOutIntensity = true;
    }
}

// Methods from ::android::hardware::vibrator::V1_0::IVibrator follow.

Return<Status> Vibrator::on(uint32_t timeoutMs) {
    return activate(timeoutMs);
}

Return<Status> Vibrator::off() {
    return activate(0);
}

Return<bool> Vibrator::supportsAmplitudeControl() {
    return true;
}

Return<Status> Vibrator::setAmplitude(uint8_t amplitude) {
    uint32_t intensity;

    if (amplitude == 0) {
        return Status::BAD_VALUE;
    }

    LOG(DEBUG) << "setting amplitude: " << (uint32_t)amplitude;

    intensity = std::lround((amplitude - 1) * 10000.0 / 254.0);
    if (intensity > INTENSITY_MAX) {
        intensity = INTENSITY_MAX;
    }
    LOG(DEBUG) << "setting intensity: " << intensity;

    if (mhasTimedOutIntensity) {
        return writeNode(VIBRATOR_INTENSITY_PATH, intensity);
    }

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

// Private methods follow.

Return<void> Vibrator::perform(Effect effect, EffectStrength strength, perform_cb _hidl_cb) {
    Status status = Status::OK;
    uint8_t amplitude;
    uint32_t ms;

    LOG(DEBUG) << "perform effect: " << toString(effect)
               << ", strength: " << toString(strength);

    amplitude = strengthToAmplitude(strength, &status);
    if (status != Status::OK) {
        _hidl_cb(status, 0);
        return Void();
    }
    setAmplitude(amplitude);

    ms = effectToMs(effect, &status);
    if (status != Status::OK) {
        _hidl_cb(status, 0);
        return Void();
    }
    status = activate(ms);

    _hidl_cb(status, ms);

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

Status Vibrator::activate(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock{mMutex};
    if (!mIsTimedOutVibriator) {
        return Status::UNSUPPORTED_OPERATION;
    }

    return writeNode(VIBRATOR_TIMEOUT_PATH, timeoutMs);
}

uint8_t Vibrator::strengthToAmplitude(EffectStrength strength, Status* status) {
    *status = Status::OK;

    switch (strength) {
        case EffectStrength::LIGHT:
            return 78;
        case EffectStrength::MEDIUM:
            return 128;
        case EffectStrength::STRONG:
            return 204;
    }

    *status = Status::UNSUPPORTED_OPERATION;
    return 0;
}

uint32_t Vibrator::effectToMs(Effect effect, Status* status) {
    *status = Status::OK;

    switch (effect) {
        case Effect::CLICK:
            return 20;
        case Effect::DOUBLE_CLICK:
            return 25;
        case Effect::HEAVY_CLICK:
            return 30;
        case Effect::TICK:
        case Effect::TEXTURE_TICK:
        case Effect::THUD:
        case Effect::POP:
            return 15;
        case Effect::RINGTONE_1:
        case Effect::RINGTONE_2:
        case Effect::RINGTONE_3:
        case Effect::RINGTONE_4:
        case Effect::RINGTONE_5:
        case Effect::RINGTONE_6:
        case Effect::RINGTONE_7:
        case Effect::RINGTONE_8:
        case Effect::RINGTONE_9:
        case Effect::RINGTONE_10:
        case Effect::RINGTONE_11:
        case Effect::RINGTONE_12:
        case Effect::RINGTONE_13:
        case Effect::RINGTONE_14:
        case Effect::RINGTONE_15:
            return 300;
    }

    *status = Status::UNSUPPORTED_OPERATION;
    return 0;
}

}  // namespace implementation
}  // namespace V1_3
}  // namespace vibrator
}  // namespace hardware
}  // namespace android
