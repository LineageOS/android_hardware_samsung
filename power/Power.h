/*
 * Copyright (C) 2018 The LineageOS Project
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

#ifndef ANDROID_HARDWARE_POWER_V1_0_POWER_H
#define ANDROID_HARDWARE_POWER_V1_0_POWER_H

#include <android/hardware/power/1.0/IPower.h>
#include <vendor/lineage/power/1.0/ILineagePower.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <hardware/power.h>

extern "C" {
void samsung_power_init(void);
void samsung_power_hint(power_hint_t hint, void *data);
void samsung_power_set_interactive(int on);
void samsung_set_feature(feature_t feature, int state);
int __attribute__ ((weak)) get_number_of_profiles();
}

namespace android {
namespace hardware {
namespace power {
namespace V1_0 {
namespace implementation {

using ::android::hardware::power::V1_0::Feature;
using ::android::hardware::power::V1_0::PowerHint;
using ::android::hardware::power::V1_0::IPower;
using ::vendor::lineage::power::V1_0::ILineagePower;
using ::vendor::lineage::power::V1_0::LineageFeature;
using ::android::hardware::Return;
using ::android::hardware::Void;;

struct Power : public IPower, public ILineagePower {
    // Methods from ::android::hardware::power::V1_0::IPower follow.
    Power();
    status_t registerAsSystemService();

    Return<void> setInteractive(bool interactive) override;
    Return<void> powerHint(PowerHint hint, int32_t data) override;
    Return<void> setFeature(Feature feature, bool activate) override;
    Return<void> getPlatformLowPowerStats(getPlatformLowPowerStats_cb _hidl_cb) override;

    // Methods from ::vendor::lineage::power::V1_0::ILineagePower follow.
    Return<int32_t> getFeature(LineageFeature feature) override;
};

}  // namespace implementation
}  // namespace V1_0
}  // namespace power
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_POWER_V1_0_POWER_H