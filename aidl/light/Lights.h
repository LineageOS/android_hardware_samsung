/*
 * Copyright (C) 2021 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/light/BnLights.h>
#include <unordered_map>
#include "samsung_lights.h"

using ::aidl::android::hardware::light::HwLightState;
using ::aidl::android::hardware::light::HwLight;

namespace aidl {
namespace android {
namespace hardware {
namespace light {

class Lights : public BnLights {
public:
    Lights();

    ndk::ScopedAStatus setLightState(int32_t id, const HwLightState& state) override;
    ndk::ScopedAStatus getLights(std::vector<HwLight> *_aidl_return) override;

private:
    void handleBacklight(const HwLightState& state);
#ifdef BUTTON_BRIGHTNESS_NODE
    void handleButtons(const HwLightState& state);
#endif /* BUTTON_BRIGHTNESS_NODE */
#ifdef LED_BLINK_NODE
    void handleBattery(const HwLightState& state);
    void handleNotifications(const HwLightState& state);
    void handleAttention(const HwLightState& state);
    void setNotificationLED();
    uint32_t calibrateColor(uint32_t color, int32_t brightness);

    HwLightState mAttentionState;
    HwLightState mBatteryState;
    HwLightState mNotificationState;
#endif /* LED_BLINK_NODE */

    uint32_t rgbToBrightness(const HwLightState& state);

    std::mutex mLock;
    std::unordered_map<LightType, std::function<void(const HwLightState&)>> mLights;
};

} // namespace light
} // namespace hardware
} // namespace android
} // namespace aidl
