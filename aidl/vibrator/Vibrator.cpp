/*
 * Copyright (C) 2021-2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Vibrator.h"
extern "C" {
#include "owt.h"
}

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>

#include <fcntl.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>

#include <linux/input.h>

using android::base::ReadFileToString;

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

const std::string kVibratorPropPrefix = "ro.vendor.vibrator_hal.";
const std::string kVibratorPropDuration = "_duration";

static std::map<Effect, int> CP_TRIGGER_EFFECTS{{Effect::CLICK, 10},
                                                {Effect::DOUBLE_CLICK, 14},
                                                {Effect::HEAVY_CLICK, 23},
                                                {Effect::TEXTURE_TICK, 50},
                                                {Effect::TICK, 50}};

static std::map<Effect, std::pair<short, int>> FF_EFFECT_IDS{{Effect::CLICK, {1, 50}},
                                                             {Effect::DOUBLE_CLICK, {5, 250}},
                                                             {Effect::TICK, {41, 25}},
                                                             {Effect::HEAVY_CLICK, {14, 100}},
                                                             {Effect::TEXTURE_TICK, {41, 25}}};

std::map<CompositePrimitive, std::pair<short, int>> FF_PRIMITIVE_IDS{
        {CompositePrimitive::NOOP, {0, 0}},           {CompositePrimitive::CLICK, {1, 20}},
        {CompositePrimitive::THUD, {124, 300}},       {CompositePrimitive::SPIN, {123, 130}},
        {CompositePrimitive::QUICK_RISE, {121, 150}}, {CompositePrimitive::SLOW_RISE, {122, 500}},
        {CompositePrimitive::QUICK_FALL, {120, 100}}, {CompositePrimitive::LIGHT_TICK, {41, 20}},
        {CompositePrimitive::LOW_TICK, {119, 20}},
};

#ifdef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
static std::map<EffectStrength, float> DURATION_AMPLITUDE = {
        {EffectStrength::LIGHT, DURATION_AMPLITUDE_LIGHT},
        {EffectStrength::MEDIUM, DURATION_AMPLITUDE_MEDIUM},
        {EffectStrength::STRONG, DURATION_AMPLITUDE_STRONG}};
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
        for (const auto& file : std::filesystem::directory_iterator("/dev/input")) {
            auto fd = open(file.path().c_str(), O_RDWR);
            if (fd != -1) {
                char name[32];
                ioctl(fd, EVIOCGNAME(sizeof(name)), name);
                if (strcmp("sec_vibrator_inputff", name) == 0) {
                    mVibratorFd = fd;
                    mIsForceFeedbackVibrator = true;
                    if (nodeExists(VIBRATOR_FUNCTIONS_PATH)) {
                        std::string contents;
                        if (ReadFileToString(VIBRATOR_FUNCTIONS_PATH, &contents) &&
                            contents.find("COMMON_INPUTFF_INTERFACE") != std::string::npos) {
                            mUsesCommonFFInterface = true;
                        } else {
                            writeNode("/sys/class/sec_vib_inputff/control/use_sep_index", 1);
                        }
                        if (ReadFileToString(VIBRATOR_FUNCTIONS_PATH, &contents) &&
                            contents.find("PRIMITIVE_EFFECT_COMPOSE") != std::string::npos) {
                            mSupportsPrimitives = true;
                        }
                    }
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
                    IVibrator::CAP_ALWAYS_ON_CONTROL*/
            ;

#ifdef VIBRATOR_SUPPORTS_DURATION_AMPLITUDE_CONTROL
    *_aidl_return |= IVibrator::CAP_AMPLITUDE_CONTROL | IVibrator::CAP_EXTERNAL_AMPLITUDE_CONTROL;
#else
    if (mHasTimedOutIntensity)
        *_aidl_return |=
                IVibrator::CAP_AMPLITUDE_CONTROL | IVibrator::CAP_EXTERNAL_AMPLITUDE_CONTROL;
#endif

    if (mIsForceFeedbackVibrator) {
        *_aidl_return |= IVibrator::CAP_AMPLITUDE_CONTROL;
        if (mSupportsPrimitives) {
            *_aidl_return |= IVibrator::CAP_COMPOSE_EFFECTS;
        }
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::off() {
    return activate(0);
}

ndk::ScopedAStatus Vibrator::on(int32_t timeoutMs,
                                const std::shared_ptr<IVibratorCallback>& callback) {
    ndk::ScopedAStatus status;

    if (mHasTimedOutEffect) {
        writeNode(VIBRATOR_CP_TRIGGER_PATH, 0);  // Clear all effects
    }

    if (mIsForceFeedbackVibrator) {
        uploadFFEffect({0}, timeoutMs);
    }

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

ndk::ScopedAStatus Vibrator::perform(Effect effect, EffectStrength strength,
                                     const std::shared_ptr<IVibratorCallback>& callback,
                                     int32_t* _aidl_return) {
    ndk::ScopedAStatus status;
    float amplitude = strengthToAmplitude(strength, &status);
    uint32_t ms = 1000;

    if (!status.isOk()) {
        return status;
    }

    if (mIsTimedOutVibrator) {
        activate(0);
    }

    setAmplitude(amplitude);

    if (mHasTimedOutEffect && CP_TRIGGER_EFFECTS.find(effect) != CP_TRIGGER_EFFECTS.end()) {
        writeNode(VIBRATOR_CP_TRIGGER_PATH, CP_TRIGGER_EFFECTS[effect]);
    } else if (mIsForceFeedbackVibrator) {
        if (FF_EFFECT_IDS.find(effect) == FF_EFFECT_IDS.end())
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        if (mUsesCommonFFInterface) {
            uploadFFEffect({FF_EFFECT_IDS.at(effect).first}, 0);
        } else {
            uploadFFEffect({0, FF_EFFECT_IDS.at(effect).first}, 0);
        }
        ms = FF_EFFECT_IDS.at(effect).second;
    } else {
        if (mHasTimedOutEffect) {
            writeNode(VIBRATOR_CP_TRIGGER_PATH, 0);  // Clear previous effect
        }

        ms = effectToMs(effect, &status);

        if (!status.isOk()) {
            return status;
        }
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
    *_aidl_return = {Effect::CLICK, Effect::TICK, Effect::TEXTURE_TICK};

    if (mHasTimedOutEffect) {
        for (const auto& effect : CP_TRIGGER_EFFECTS) {
            _aidl_return->push_back(effect.first);
        }
    }

    if (mIsForceFeedbackVibrator) {
        for (const auto& effect : FF_EFFECT_IDS) {
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
        struct input_event event{
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

ndk::ScopedAStatus Vibrator::getCompositionDelayMax(int32_t* _aidl_return) {
    if (!mSupportsPrimitives)
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);

    *_aidl_return = 1000;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getCompositionSizeMax(int32_t* _aidl_return) {
    if (!mSupportsPrimitives)
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);

    *_aidl_return = 10;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedPrimitives(std::vector<CompositePrimitive>* _aidl_return) {
    if (!mSupportsPrimitives)
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);

    for (auto primitive : FF_PRIMITIVE_IDS) _aidl_return->push_back(primitive.first);

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getPrimitiveDuration(CompositePrimitive primitive,
                                                  int32_t* _aidl_return) {
    if (!mSupportsPrimitives)
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);

    *_aidl_return = FF_PRIMITIVE_IDS.at(primitive).second;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::compose(const std::vector<CompositeEffect>& composite,
                                     const std::shared_ptr<IVibratorCallback>& callback) {
    if (!mSupportsPrimitives)
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    if (composite.empty() || composite.size() > 10)
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);

    struct common_inputff_effects input_compose = {};
    int16_t data[WT_TYPE12_PWLE_SINGLE_PACKED_MAX / 2];
    std::string effect_str;
    int len, ms = 0;

    for (auto segment : composite) {
        if (segment.delayMs < 0 || segment.delayMs > 1000)
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        if (FF_PRIMITIVE_IDS.find(segment.primitive) == FF_PRIMITIVE_IDS.end() ||
            segment.scale < 0 || segment.scale > 1)
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);

        if (segment.primitive == CompositePrimitive::NOOP) {
            // No haptic effect. Used to generate extended delays between primitives.
            if (segment.delayMs > 0) {
                if (mUsesCommonFFInterface)
                    input_compose.effects[input_compose.num_of_effects++] = {0, 0, 0,
                                                                             segment.delayMs, 0};
                else
                    effect_str.append(std::format("{} ", segment.delayMs));
            }
            ms += segment.delayMs;
            continue;
        }

        // Consider 15% the lowest "feelable" amplitude
        int scale = round(segment.scale * 85 + 15);

        uint32_t id = FF_PRIMITIVE_IDS.at(segment.primitive).first;

        int duration = 0;
        getPrimitiveDuration(segment.primitive, &duration);
        ms += duration + segment.delayMs;

        if (mUsesCommonFFInterface) {
            if (segment.delayMs > 0) {
                input_compose.effects[input_compose.num_of_effects++] = {0, 0, 0, segment.delayMs,
                                                                         0};
            }
            input_compose.effects[input_compose.num_of_effects++] = {FF_PERIODIC, (int)id, scale,
                                                                     duration, 0};
        } else {  // Cirrus OWT
            effect_str.append(
                    segment.delayMs == 0
                            ? std::format("{}.{} ", cirrusEffectId(id), scale)
                            : std::format("{} {}.{} ", segment.delayMs, cirrusEffectId(id), scale));
        }
    }

    if (mUsesCommonFFInterface) {
        setAmplitude(1);
        std::vector<int16_t> commonEffectData(
                (int16_t*)&input_compose,
                (int16_t*)&input_compose + sizeof(input_compose) / sizeof(int16_t));
        uploadFFEffect(commonEffectData, 0);
    } else {
        len = get_owt_data(effect_str.data(), (uint8_t*)data);
        if (!len) return ndk::ScopedAStatus::fromExceptionCode(EX_SERVICE_SPECIFIC);
        std::vector<int16_t> effectData(data, data + len);
        uploadFFEffect(effectData, 0);
    }
    activate(1);
    if (callback != nullptr) {
        std::thread([=] {
            usleep(ms * 1000);
            callback->onComplete();
        }).detach();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Vibrator::getSupportedAlwaysOnEffects(std::vector<Effect>* /*_aidl_return*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::alwaysOnEnable(int32_t /*id*/, Effect /*effect*/,
                                            EffectStrength /*strength*/) {
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

ndk::ScopedAStatus Vibrator::composePwle(const std::vector<PrimitivePwle>& /*composite*/,
                                         const std::shared_ptr<IVibratorCallback>& /*callback*/) {
    return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
}

ndk::ScopedAStatus Vibrator::activate(uint32_t timeoutMs) {
    std::lock_guard<std::mutex> lock{mMutex};
    if (mIsTimedOutVibrator) {
        return writeNode(VIBRATOR_TIMEOUT_PATH, timeoutMs);
    }
    if (mIsForceFeedbackVibrator) {
        struct input_event event{
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
            .id = -1,
            .replay.length = static_cast<uint16_t>(timeoutMs),
    };

    if (mUsesCommonFFInterface && effectData[0] == 0 &&
        effectData.size() != sizeof(common_inputff_effects) / 2) {
        effect.type = FF_CONSTANT;
    } else {
        effect.type = FF_PERIODIC;
        effect.u.periodic = {
                .waveform = FF_CUSTOM,
                .custom_len = static_cast<uint32_t>(effectData.size()),
                .custom_data = effectData.data(),
        };
    }

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

uint32_t Vibrator::cirrusEffectId(uint32_t id) {
    if (id >= FF_PRIMITIVE_IDS.at(CompositePrimitive::LOW_TICK).first &&
        id <= FF_PRIMITIVE_IDS.at(CompositePrimitive::THUD).first)
        id += 16;
    else if (id == FF_PRIMITIVE_IDS.at(CompositePrimitive::LIGHT_TICK).first)
        id += 9;
    return id;
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

}  // namespace vibrator
}  // namespace hardware
}  // namespace android
}  // namespace aidl
