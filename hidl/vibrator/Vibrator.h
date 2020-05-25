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

#ifndef ANDROID_HARDWARE_VIBRATOR_V1_3_VIBRATOR_H
#define ANDROID_HARDWARE_VIBRATOR_V1_3_VIBRATOR_H

#include <android/hardware/vibrator/1.3/IVibrator.h>
#include <hidl/Status.h>

#include <fstream>

#define INTENSITY_MIN 1000
#define INTENSITY_MAX 10000
#define INENSITY_DEFAULT INTENSITY_MAX

#define CLICK_TIMING_MS 20

#define VIBRATOR_TIMEOUT_PATH "/sys/class/timed_output/vibrator/enable"
#define VIBRATOR_INTENSITY_PATH "/sys/class/timed_output/vibrator/intensity"

namespace android {
namespace hardware {
namespace vibrator {
namespace V1_3 {
namespace implementation {

using android::hardware::vibrator::V1_0::EffectStrength;
using android::hardware::vibrator::V1_0::Status;

class Vibrator : public IVibrator {
  public:
    Vibrator();

    // Methods from ::android::hardware::vibrator::V1_0::IVibrator follow.
    Return<Status> on(uint32_t timeoutMs) override;
    Return<Status> off() override;
    Return<bool> supportsAmplitudeControl() override;
    Return<Status> setAmplitude(uint8_t amplitude) override;
    Return<void> perform(V1_0::Effect effect, EffectStrength strength,
                         perform_cb _hidl_cb) override;

    // Methods from ::android::hardware::vibrator::V1_1::IVibrator follow.
    Return<void> perform_1_1(V1_1::Effect_1_1 effect, EffectStrength strength,
                             perform_cb _hidl_cb) override;

    // Methods from ::android::hardware::vibrator::V1_2::IVibrator follow.
    Return<void> perform_1_2(V1_2::Effect effect, EffectStrength strength,
                             perform_cb _hidl_cb) override;

    // Methods from ::android::hardware::vibrator::V1_3::IVibrator follow.
    Return<bool> supportsExternalControl() override;
    Return<Status> setExternalControl(bool enabled) override;
    Return<void> perform_1_3(Effect effect, EffectStrength strength, perform_cb _hidl_cb) override;

  private:
    Return<void> perform(Effect effect, EffectStrength strength, perform_cb _hidl_cb);
    template <typename T>
    Return<void> perform(T effect, EffectStrength strength, perform_cb _hidl_cb);
    Status enable(bool enabled);
    Status activate(uint32_t ms);

    static uint32_t effectToMs(Effect effect, Status* status);
    static uint8_t strengthToAmplitude(EffectStrength strength, Status* status);

    bool mEnabled{false};
    uint8_t mAmplitude{UINT8_MAX};
    bool mExternalControl{false};
    std::mutex mMutex;
    timer_t mTimer{nullptr};

    bool mIsTimedOutVibriator;
    bool mhasTimedOutIntensity;
};

}  // namespace implementation
}  // namespace V1_3
}  // namespace vibrator
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_VIBRATOR_V1_3_VIBRATOR_H
