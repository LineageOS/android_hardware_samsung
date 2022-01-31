/*
 * Copyright (C) 2021 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/vibrator/BnVibrator.h>

#define INTENSITY_MIN 1000
#define INTENSITY_MAX 10000
#define INTENSITY_DEFAULT INTENSITY_MAX

#define VIBRATOR_TIMEOUT_PATH "/sys/class/timed_output/vibrator/enable"
#define VIBRATOR_INTENSITY_PATH "/sys/class/timed_output/vibrator/intensity"
#define VIBRATOR_CP_TRIGGER_PATH "/sys/class/timed_output/vibrator/cp_trigger_index"

using ::aidl::android::hardware::vibrator::IVibratorCallback;
using ::aidl::android::hardware::vibrator::Braking;
using ::aidl::android::hardware::vibrator::Effect;
using ::aidl::android::hardware::vibrator::EffectStrength;
using ::aidl::android::hardware::vibrator::CompositeEffect;
using ::aidl::android::hardware::vibrator::CompositePrimitive;
using ::aidl::android::hardware::vibrator::PrimitivePwle;

namespace aidl {
namespace android {
namespace hardware {
namespace vibrator {

class Vibrator : public BnVibrator {
public:
    Vibrator();
    ndk::ScopedAStatus getCapabilities(int32_t* _aidl_return) override;
    ndk::ScopedAStatus off() override;
    ndk::ScopedAStatus on(int32_t timeoutMs, const std::shared_ptr<IVibratorCallback>& callback) override;
    ndk::ScopedAStatus perform(Effect effect, EffectStrength strength, const std::shared_ptr<IVibratorCallback>& callback, int32_t* _aidl_return) override;
    ndk::ScopedAStatus getSupportedEffects(std::vector<Effect>* _aidl_return) override;
    ndk::ScopedAStatus setAmplitude(float amplitude) override;
    ndk::ScopedAStatus setExternalControl(bool enabled) override;
    ndk::ScopedAStatus getCompositionDelayMax(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getCompositionSizeMax(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getSupportedPrimitives(std::vector<CompositePrimitive>* _aidl_return) override;
    ndk::ScopedAStatus getPrimitiveDuration(CompositePrimitive primitive, int32_t* _aidl_return) override;
    ndk::ScopedAStatus compose(const std::vector<CompositeEffect>& composite, const std::shared_ptr<IVibratorCallback>& callback) override;
    ndk::ScopedAStatus getSupportedAlwaysOnEffects(std::vector<Effect>* _aidl_return) override;
    ndk::ScopedAStatus alwaysOnEnable(int32_t id, Effect effect, EffectStrength strength) override;
    ndk::ScopedAStatus alwaysOnDisable(int32_t id) override;
    ndk::ScopedAStatus getResonantFrequency(float* _aidl_return) override;
    ndk::ScopedAStatus getQFactor(float* _aidl_return) override;
    ndk::ScopedAStatus getFrequencyResolution(float* _aidl_return) override;
    ndk::ScopedAStatus getFrequencyMinimum(float* _aidl_return) override;
    ndk::ScopedAStatus getBandwidthAmplitudeMap(std::vector<float>* _aidl_return) override;
    ndk::ScopedAStatus getPwlePrimitiveDurationMax(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getPwleCompositionSizeMax(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getSupportedBraking(std::vector<Braking>* _aidl_return) override;
    ndk::ScopedAStatus composePwle(const std::vector<PrimitivePwle>& composite, const std::shared_ptr<IVibratorCallback>& callback) override;

private:
    ndk::ScopedAStatus activate(uint32_t ms);
    static uint32_t effectToMs(Effect effect, ndk::ScopedAStatus* status);
    static uint8_t strengthToAmplitude(EffectStrength strength, ndk::ScopedAStatus* status);

    bool mEnabled{false};
    bool mExternalControl{false};
    std::mutex mMutex;

    bool mIsTimedOutVibrator;
    bool mHasTimedOutIntensity;
    bool mHasTimedOutEffect;
};

} // namespace vibrator
} // namespace hardware
} // namespace android
} // namespace aidl
