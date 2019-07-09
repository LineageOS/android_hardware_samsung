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

#ifndef ANDROID_HARDWARE_LIGHT_V2_0_LIGHT_H
#define ANDROID_HARDWARE_LIGHT_V2_0_LIGHT_H

#include <android/hardware/light/2.0/ILight.h>
#include <fstream>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include "samsung_lights.h"

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

struct Light : public ILight {
    Light();
    
    Return<::android::hardware::light::V2_0::Status> setLight(::android::hardware::light::V2_0::Type type, const ::android::hardware::light::V2_0::LightState& state) override;
    Return<void> getSupportedTypes(getSupportedTypes_cb _hidl_cb) override;
    
private:
    uint32_t rgbToBrightness(const ::android::hardware::light::V2_0::LightState& state);
    uint32_t calibrateColor(uint32_t color, uint32_t brightness);

    std::ofstream mBacklight;
    std::ofstream mButtonlight;
    std::ofstream mIndicator;
#ifdef LED_BLN_NODE
    std::ofstream mBacklightNotification;
#endif

    std::mutex mLock;
};

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_LIGHT_V2_0_LIGHT_H
