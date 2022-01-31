/*
 * Copyright (C) 2021 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Vibrator.h"

#include <android-base/logging.h>

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

static std::map<Effect, int> CP_TRIGGER_EFFECTS {
    { Effect::CLICK, 10 },
    { Effect::DOUBLE_CLICK, 14 },
    { Effect::HEAVY_CLICK, 23 },
    { Effect::TEXTURE_TICK, 50 },
    { Effect::TICK, 50 }
};

/*
 * Write value to path and close file.
 */
template <typename T>
static ndk::ScopedAStatus writeNode(const std::string& path, const T& value) {
    std::ofstream node(path);
    if (!node) {
        LOG(ERROR) << "Failed to open: " << path;
        return ndk::ScopedAStatus::fromStatus(STATUS_UNKNOWN_ERROR);
    }

    LOG(DEBUG) << "writeNode node: " << path << " value: " << value;

    node << value << std::endl;
    if (!node) {
        LOG(ERROR) << "Failed to write: " << value;
        return ndk::ScopedAStatus::fromStatus(STATUS_UNKNOWN_ERROR);
    }

    return ndk::ScopedAStatus::ok();
}

static bool nodeExists(const std::string& path) {
    std::ofstream f(path.c_str());
    return f.good();
}

Vibrator::Vibrator() {
    mIsTimedOutVibrator = nodeExists(VIBRATOR_TIMEOUT_PATH);
    mHasTimedOutIntensity = nodeExists(VIBRATOR_INTENSITY_PATH);
    mHasTimedOutEffect = nodeExists(VIBRATOR_CP_TRIGGER_PATH);
}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t* _aidl_return) {
    *_aidl_return = IVibrator::CAP_ON_CALLBACK | IVibrator::CAP_PERFORM_CALLBACK |
                    IVibrator::CAP_EXTERNAL_CONTROL /*| IVibrator::CAP_COMPOSE_EFFECTS |
                    IVibrator::CAP_ALWAYS_ON_CONTROL*/;

    if (mHasTimedOutIntensity) {
        *_aidl_return = *_aidl_return | IVibrator::CAP_AMPLITUDE_CONTROL |
                        IVibrator::CAP_EXTERNAL_AMPLITUDE_CONTROL;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    return activate(0);
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs, const std::shared_ptr<IVibratorCallback>& callback) {
    ndk::ScopedAStatus status;

    if (mHasTimedOutEffect)
        writeNode(VIBRATOR_CP_TRIGGER_PATH, 0); // Clear all effects

    status = activate(timeoutMs);

    if (callback != nullptr) {
        std::thread([=] {
            LOG(DEBUG) << "Starting on on another thread";
            usleep(timeoutMs * 1000);
            LOG(DEBUG) << "Notifying on complete";
            if (!callback->onComplete().isOk()) {
                LOG(ERROR) << "Failed to call onComplete";
            }
        }).detach();
    }

    return status;
}

ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength strength, const std::shared_ptr<IVibratorCallback>& callback, int32_t* _aidl_return) {
    ndk::ScopedAStatus status;
    uint32_t amplitude = strengthToAmplitude(strength, &status);
    uint32_t ms = 1000;

    if (!status.isOk())
        return status;

    activate(0);
    setAmplitude(amplitude);

    if (mHasTimedOutEffect && CP_TRIGGER_EFFECTS.find(effect) != CP_TRIGGER_EFFECTS.end()) {
        writeNode(VIBRATOR_CP_TRIGGER_PATH, CP_TRIGGER_EFFECTS[effect]);
    } else {
        if (mHasTimedOutEffect)
            writeNode(VIBRATOR_CP_TRIGGER_PATH, 0); // Clear previous effect

        ms = effectToMs(effect, &status);

        if (!status.isOk())
            return status;
    }

    status = activate(ms);

    if (callback != nullptr) {
        std::thread([=] {
            LOG(DEBUG) << "Starting perform on another thread";
            usleep(ms * 1000);
            LOG(DEBUG) << "Notifying perform complete";
            callback->onComplete();
        }).detach();
    }

    *_aidl_return = ms;
    return status;
}

ndk::ScopedAStatus Vibrator::getSupportedEffects(std::vector<Effect>* _aidl_return) {
    *_aidl_return = {Effect::CLICK, Effect::DOUBLE_CLICK, Effect::HEAVY_CLICK,
                     Effect::TICK, Effect::TEXTURE_TICK, Effect::THUD, Effect::POP,
                     Effect::RINGTONE_1, Effect::RINGTONE_2, Effect::RINGTONE_3,
                     Effect::RINGTONE_4, Effect::RINGTONE_5, Effect::RINGTONE_6,
                     Effect::RINGTONE_7, Effect::RINGTONE_7, Effect::RINGTONE_8,
                     Effect::RINGTONE_9, Effect::RINGTONE_10, Effect::RINGTONE_11,
                     Effect::RINGTONE_12, Effect::RINGTONE_13, Effect::RINGTONE_14,
                     Effect::RINGTONE_15};
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
    uint32_t intensity;

    if (amplitude == 0) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    LOG(DEBUG) << "Setting amplitude: " << (uint32_t)amplitude;

    intensity = std::lround((amplitude - 1) * INTENSITY_MAX / 254.0);
    if (intensity > INTENSITY_MAX) {
        intensity = INTENSITY_MAX;
    }
    LOG(DEBUG) << "Setting intensity: " << intensity;

    if (mHasTimedOutIntensity) {
        return writeNode(VIBRATOR_INTENSITY_PATH, intensity);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setExternalControl(bool enabled) {
    if (mEnabled) {
        LOG(WARNING) << "Setting external control while the vibrator is enabled is "
                        "unsupported!";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    LOG(INFO) << "ExternalControl: " << mExternalControl << " -> " << enabled;
    mExternalControl = enabled;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getSupportedPrimitives(std::vector<CompositePrimitive>* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getPrimitiveDuration(CompositePrimitive /*primitive*/, int32_t* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::compose(const std::vector<CompositeEffect>& /*composite*/, const std::shared_ptr<IVibratorCallback>& /*callback*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(std::vector<Effect>* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::alwaysOnEnable(int32_t /*id*/, Effect /*effect*/, EffectStrength /*strength*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::alwaysOnDisable(int32_t /*id*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getResonantFrequency(float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getQFactor(float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getFrequencyResolution(float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getFrequencyMinimum(float* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getBandwidthAmplitudeMap(std::vector<float>* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getPwlePrimitiveDurationMax(int32_t* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getPwleCompositionSizeMax(int32_t* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::getSupportedBraking(std::vector<Braking>* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::composePwle(const std::vector<PrimitivePwle>& /*composite*/, const std::shared_ptr<IVibratorCallback>& /*callback*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::activate(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock{mMutex};
    if (!mIsTimedOutVibrator) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return writeNode(VIBRATOR_TIMEOUT_PATH, timeoutMs);
}

uint8_t Vibrator::strengthToAmplitude(EffectStrength strength, ndk::ScopedAStatus* status) {
    *status = ndk::ScopedAStatus::ok();

    switch (strength) {
        case EffectStrength::LIGHT:
            return 64;
        case EffectStrength::MEDIUM:
            return 128;
        case EffectStrength::STRONG:
            return 255;
    }

    *status = ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return 0;
}

uint32_t Vibrator::effectToMs(Effect effect, ndk::ScopedAStatus* status) {
    *status = ndk::ScopedAStatus::ok();
    switch (effect) {
        case Effect::CLICK:
            return 10;
        case Effect::DOUBLE_CLICK:
            return 15;
        case Effect::TICK:
        case Effect::TEXTURE_TICK:
        case Effect::THUD:
        case Effect::POP:
            return 5;
        case Effect::HEAVY_CLICK:
            return 10;
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
            return 30000;
    }
    *status = ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return 0;
}

} // namespace vibrator
} // namespace hardware
} // namespace android
} // namespace aidl
