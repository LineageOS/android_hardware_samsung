/*
 * Copyright (C) 2021-2023 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Vibrator.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <cmath>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>

#include <linux/input.h>

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

const std::string kVibratorPropPrefix = "ro.vendor.vibrator_hal.";
const std::string kVibratorPropDuration = "_duration";

static std::map<Effect, int> CP_TRIGGER_EFFECTS {
    { Effect::CLICK, 10 },
    { Effect::DOUBLE_CLICK, 14 },
    { Effect::HEAVY_CLICK, 23 },
    { Effect::TEXTURE_TICK, 50 },
    { Effect::TICK, 50 }
};

static std::map<Effect, short> FF_EFFECT_IDS {
    { Effect::CLICK, 1 },
    { Effect::DOUBLE_CLICK, 5 },
    { Effect::TICK, 41 },
    { Effect::HEAVY_CLICK, 14 },
    { Effect::TEXTURE_TICK, 41 }
};

#ifdef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
static std::map<EffectStrength, float> DURATION_AMPLITUDE = {
    { EffectStrength::LIGHT, DURATION_AMPLITUDE_LIGHT },
    { EffectStrength::MEDIUM, DURATION_AMPLITUDE_MEDIUM },
    { EffectStrength::STRONG, DURATION_AMPLITUDE_STRONG }
};
#endif

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

static int getIntProperty(const std::string& key, int def) {
    return ::android::base::GetIntProperty(kVibratorPropPrefix + key, def);
}

Vibrator::Vibrator() {
    mIsTimedOutVibrator = nodeExists(VIBRATOR_TIMEOUT_PATH);
    if (!mIsTimedOutVibrator) {
        for (const auto &file : std::filesystem::directory_iterator("/dev/input")) {
            auto fd = open(file.path().c_str(), O_RDWR);
            if (fd != -1) {
                char name[32];
                ioctl(fd, EVIOCGNAME(sizeof(name)), name);
                if (strcmp("sec_vibrator_inputff", name) == 0) {
                    mVibratorFd = fd;
                    mIsForceFeedbackVibrator = true;
                    writeNode("/sys/class/sec_vib_inputff/control/use_sep_index", 1);
                    break;
                }
                close(fd);
            }
        }
    }
    mHasTimedOutIntensity = nodeExists(VIBRATOR_INTENSITY_PATH);
    mHasTimedOutEffect = nodeExists(VIBRATOR_CP_TRIGGER_PATH);
}

ndk::ScopedAStatus Vibrator::getCapabilities(int32_t* _aidl_return) {
    *_aidl_return = IVibrator::CAP_ON_CALLBACK | IVibrator::CAP_PERFORM_CALLBACK |
                    IVibrator::CAP_EXTERNAL_CONTROL /*| IVibrator::CAP_COMPOSE_EFFECTS |
                    IVibrator::CAP_ALWAYS_ON_CONTROL*/;

#ifdef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
    *_aidl_return |= IVibrator::CAP_AMPLITUDE_CONTROL | IVibrator::CAP_EXTERNAL_AMPLITUDE_CONTROL;
#else
    if (mHasTimedOutIntensity)
        *_aidl_return |= IVibrator::CAP_AMPLITUDE_CONTROL | IVibrator::CAP_EXTERNAL_AMPLITUDE_CONTROL;
#endif

    if (mIsForceFeedbackVibrator)
        *_aidl_return |= IVibrator::CAP_AMPLITUDE_CONTROL;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    return activate(0);
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs, const std::shared_ptr<IVibratorCallback>& callback) {
    ndk::ScopedAStatus status;

    if (mHasTimedOutEffect)
        writeNode(VIBRATOR_CP_TRIGGER_PATH, 0); // Clear all effects

    if (mIsForceFeedbackVibrator)
        uploadFFEffect({0}, timeoutMs);

#ifdef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
    timeoutMs *= mDurationAmplitude;
#endif

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
    float amplitude = strengthToAmplitude(strength, &status);
    uint32_t ms = 1000;

    if (!status.isOk())
        return status;

    if (mIsTimedOutVibrator)
        activate(0);

    setAmplitude(amplitude);

    if (mHasTimedOutEffect && CP_TRIGGER_EFFECTS.find(effect) != CP_TRIGGER_EFFECTS.end()) {
        writeNode(VIBRATOR_CP_TRIGGER_PATH, CP_TRIGGER_EFFECTS[effect]);
    } else if (mIsForceFeedbackVibrator) {
        if (FF_EFFECT_IDS.find(effect) == FF_EFFECT_IDS.end())
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        uploadFFEffect({0, FF_EFFECT_IDS.at(effect)}, 0);
    } else {
        if (mHasTimedOutEffect)
            writeNode(VIBRATOR_CP_TRIGGER_PATH, 0); // Clear previous effect

        ms = effectToMs(effect, &status);

        if (!status.isOk())
            return status;
    }

#ifdef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
    ms *= DURATION_AMPLITUDE[strength];
#endif

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
    *_aidl_return = { Effect::CLICK, Effect::TICK, Effect::TEXTURE_TICK };

    if (mHasTimedOutEffect) {
      for (const auto& effect : CP_TRIGGER_EFFECTS) {
          _aidl_return->push_back(effect.first);
      }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::setAmplitude(float amplitude) {
    uint32_t intensity;

    if (amplitude <= 0.0f || amplitude > 1.0f) {
        return ndk::ScopedAStatus(AStatus_fromExceptionCode(EX_ILLEGAL_ARGUMENT));
    }

#ifdef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
    mDurationAmplitude = durationAmplitude(amplitude);
#endif

    LOG(DEBUG) << "Setting amplitude: " << amplitude;

    intensity = amplitude * INTENSITY_MAX;

#ifndef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
    LOG(DEBUG) << "Setting intensity: " << intensity;

    if (mHasTimedOutIntensity) {
        return writeNode(VIBRATOR_INTENSITY_PATH, intensity);
    }

    if (mIsForceFeedbackVibrator) {
        struct input_event event {
            .type = EV_FF,
            .code = FF_GAIN,
            .value = static_cast<__s32>(intensity),
        };
        if (write(mVibratorFd, &event, sizeof(event)) == -1)
            return ndk::ScopedAStatus::fromExceptionCode(STATUS_UNKNOWN_ERROR);
        return ndk::ScopedAStatus::ok();
    }
#endif

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
    if (mIsTimedOutVibrator) {
        return writeNode(VIBRATOR_TIMEOUT_PATH, timeoutMs);
    }
    if (mIsForceFeedbackVibrator) {
        struct input_event event {
            .type = EV_FF,
            .code = 0,
            .value = timeoutMs != 0,
        };
        if (write(mVibratorFd, &event, sizeof(event)) == -1)
            return ndk::ScopedAStatus::fromExceptionCode(STATUS_UNKNOWN_ERROR);
        return ndk::ScopedAStatus::ok();
    }
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::uploadFFEffect(std::vector<int16_t> effectData, int timeoutMs) {
    int ret;

    // Remove previously uploaded effect in case it exists
    ret = ioctl(mVibratorFd, EVIOCRMFF, 0);
    if (ret == -1) {
        LOG(WARNING) << "Failed to remove effect";
    }

    struct ff_effect effect = {
        .type = FF_PERIODIC,
        .id = -1,
        .replay = {
            .length = static_cast<uint16_t>(timeoutMs),
        },
        .u.periodic = {
            .waveform = FF_CUSTOM,
            .custom_len = static_cast<uint32_t>(effectData.size()),
            .custom_data = effectData.data(),
        },
    };

    ret = ioctl(mVibratorFd, EVIOCSFF, &effect);
    if (ret == -1) {
        LOG(ERROR) << "Effect upload failed: " << errno;
        return ndk::ScopedAStatus::fromStatus(STATUS_UNKNOWN_ERROR);
    }
    return ndk::ScopedAStatus::ok();
}

float Vibrator::strengthToAmplitude(EffectStrength strength, ndk::ScopedAStatus* status) {
    *status = ndk::ScopedAStatus::ok();

    switch (strength) {
        case EffectStrength::LIGHT:
            return AMPLITUDE_LIGHT;
        case EffectStrength::MEDIUM:
            return AMPLITUDE_MEDIUM;
        case EffectStrength::STRONG:
            return AMPLITUDE_STRONG;
    }

    *status = ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return 0;
}

uint32_t Vibrator::effectToMs(Effect effect, ndk::ScopedAStatus* status) {
    *status = ndk::ScopedAStatus::ok();
    switch (effect) {
        case Effect::CLICK:
            return getIntProperty("click" + kVibratorPropDuration, 10);
        case Effect::TICK:
            return getIntProperty("tick" + kVibratorPropDuration, 5);
        case Effect::TEXTURE_TICK:
            return getIntProperty("texture_tick" + kVibratorPropDuration, 5);
        default:
            break;
    }
    *status = ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    return 0;
}

#ifdef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
float Vibrator::durationAmplitude(float amplitude) {
    if (amplitude == 1) {
        return DURATION_AMPLITUDE_STRONG;
    } else if (amplitude >= 0.5) {
        return DURATION_AMPLITUDE_MEDIUM;
    }

    return DURATION_AMPLITUDE_LIGHT;
}
#endif

} // namespace vibrator
} // namespace hardware
} // namespace android
} // namespace aidl
