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

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <utils/Errors.h>

#include <iomanip>
#include "Light.h"

#define COLOR_MASK 0x00ffffff
#define MAX_INPUT_BRIGHTNESS 255

using android::base::ReadFileToString;
using android::base::Trim;

using android::hardware::light::V2_0::LightState;
using android::hardware::light::V2_0::Status;
using android::hardware::light::V2_0::Type;

namespace android {
namespace hardware {
namespace light {
namespace V2_0 {
namespace implementation {

/*
 * Write value to path and close file.
 */
template <typename T>
static void set(const std::string& path, const T& value) {
    std::ofstream file(path);
    file << value << std::endl;
}

template <typename T>
static T get(const std::string& path, const T& def) {
    std::ifstream file(path);
    T result;

    file >> result;
    return file.fail() ? def : result;
}

Light::Light() {
    mLights.emplace(Type::BACKLIGHT, std::bind(&Light::handleBacklight, this, std::placeholders::_1));
    mLights.emplace(Type::BUTTONS, std::bind(&Light::handleButtons, this, std::placeholders::_1));
    mLights.emplace(Type::BATTERY, std::bind(&Light::handleRgb, this, std::placeholders::_1, 0));
    mLights.emplace(Type::NOTIFICATIONS, std::bind(&Light::handleRgb, this, std::placeholders::_1, 1));
    mLights.emplace(Type::ATTENTION, std::bind(&Light::handleRgb, this, std::placeholders::_1, 2));
}

Return<Status> Light::setLight(Type type, const LightState& state) {
    auto it = mLights.find(type);

    if (it == mLights.end()) {
        return Status::LIGHT_NOT_SUPPORTED;
    }

    /*
     * Lock global mutex until light state is updated.
     */
    std::lock_guard<std::mutex> lock(mLock);

    it->second(state);

    return Status::SUCCESS;
}

void Light::handleBacklight(const LightState& state) {
    uint32_t max_brightness = get(PANEL_MAX_BRIGHTNESS_NODE, 255);
    uint32_t brightness = rgbToBrightness(state);

    if (max_brightness != MAX_INPUT_BRIGHTNESS) {
        brightness = brightness * max_brightness / MAX_INPUT_BRIGHTNESS;
    }

    set(PANEL_BRIGHTNESS_NODE, brightness);
}

void Light::handleButtons(const LightState& state) {
    uint32_t brightness = (state.color & COLOR_MASK) ? 1 : 0;

#ifdef VAR_BUTTON_BRIGHTNESS
    brightness = rgbToBrightness(state);
#endif

    set(BUTTON_BRIGHTNESS_NODE, brightness);
}

void Light::handleRgb(const LightState& state, size_t index) {
    int32_t adjusted_brightness = 255;
    mLightStates.at(index) = state;

    LightState stateToUse = mLightStates.front();
    for (const auto& lightState : mLightStates) {
        if (lightState.color & 0xffffff) {
            stateToUse = lightState;
            break;
        }
    }

    if (index == 0) {
        adjusted_brightness = LED_BRIGHTNESS_BATTERY;
    } else if (index == 1) {
        adjusted_brightness = LED_BRIGHTNESS_NOTIFICATION;
    } else if (index == 2) {
        adjusted_brightness = LED_BRIGHTNESS_ATTENTION;
        if (stateToUse.flashMode == Flash::HARDWARE) {
            if (stateToUse.flashOnMs > 0 && stateToUse.flashOffMs == 0)
                stateToUse.flashMode = Flash::NONE;
            stateToUse.color = 0x000000ff;
        }
        if (stateToUse.flashMode == Flash::NONE) {
            stateToUse.color = 0;
        }
    } else {
        set(LED_BLINK_NODE, "0x00000000 0 0");
        return;
    }

    if (stateToUse.flashMode == Flash::NONE) {
        stateToUse.flashOnMs = 0;
        stateToUse.flashOffMs = 0;
    }

    stateToUse.color = calibrateColor(stateToUse.color & COLOR_MASK, adjusted_brightness);
    std::stringstream ss;
    ss << std::hex <<  "0x" << std::setfill('0') << std::setw(8) << stateToUse.color << std::setw(0) << " " << stateToUse.flashOnMs << " " << stateToUse.flashOffMs;
    set(LED_BLINK_NODE, ss.str());

#ifdef LED_BLN_NODE
    if (type == Type::NOTIFICATIONS) {
        set(LED_BLN_NODE, (stateToUse.color & COLOR_MASK) ? 1 : 0);
    }
#endif
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> types;

    for (auto const& light : mLights) {
        types.push_back(light.first);
    }

    _hidl_cb(types);

    return Void();
}

uint32_t Light::rgbToBrightness(const LightState& state) {
    uint32_t color = state.color & COLOR_MASK;

    return ((77 * ((color >> 16) & 0xff)) + (150 * ((color >> 8) & 0xff)) +
            (29 * (color & 0xff))) >> 8;
}

uint32_t Light::calibrateColor(uint32_t color, int32_t brightness) {
    uint32_t red = ((color >> 16) & 0xFF) * LED_ADJUSTMENT_R;
    uint32_t green = ((color >> 8) & 0xFF) * LED_ADJUSTMENT_G;
    uint32_t blue = (color & 0xFF) * LED_ADJUSTMENT_B;

    return (((red * brightness) / 255) << 16) + (((green * brightness) / 255) << 8) + ((blue * brightness) / 255);
}

}  // namespace implementation
}  // namespace V2_0
}  // namespace light
}  // namespace hardware
}  // namespace android
