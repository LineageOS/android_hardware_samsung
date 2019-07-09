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
#include <android-base/strings.h>

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

Light::Light() {
    mBacklight.open(PANEL_BRIGHTNESS_NODE);
    mButtonlight.open(BUTTON_BRIGHTNESS_NODE);
    mIndicator.open(LED_BLINK_NODE);
#ifdef LED_BLN_NODE
    mBacklightNotification.open(LED_BLN_NODE);
#endif
}

Return<Status> Light::setLight(Type type, const LightState& state) {
    std::lock_guard<std::mutex> lock(mLock);
    switch (type) {
        case Type::BACKLIGHT:
            if (mBacklight.good()) {
                std::string tmp;
                uint32_t max_brightness = 255;
                uint32_t brightness = rgbToBrightness(state);
                
                if (ReadFileToString(PANEL_MAX_BRIGHTNESS_NODE, &tmp)) {
                    max_brightness = std::stoi(Trim(tmp));
                }
                
                if (max_brightness != MAX_INPUT_BRIGHTNESS) {
                    brightness = brightness * max_brightness / MAX_INPUT_BRIGHTNESS;
                }
                
                // TODO: check for max brightness
                mBacklight << brightness << std::endl;
            }
            break;

        case Type::BUTTONS:
            if (mButtonlight.good()) {
                uint32_t brightness = (state.color & COLOR_MASK) ? 1 : 0;

#ifdef VAR_BUTTON_BRIGHTNESS
                brightness = rgbToBrightness(state);
#endif
                mButtonlight << brightness << std::endl;
            }
            break;

        case Type::BATTERY:
        case Type::NOTIFICATIONS:
        case Type::ATTENTION:
            if (mIndicator.good()) {
                uint32_t adjusted_brightness = 255;
                uint32_t color = state.color;
                uint32_t flashOnMs = state.flashOnMs;
                uint32_t flashOffMs = state.flashOffMs;
                Flash flashMode = state.flashMode;
                
                switch (type) {
                    case Type::BATTERY:
                        adjusted_brightness = LED_BRIGHTNESS_BATTERY;
                        break;
                    case Type::NOTIFICATIONS:
                        adjusted_brightness = LED_BRIGHTNESS_NOTIFICATION;
                        break;
                    case Type::ATTENTION:
                        if (flashMode == Flash::HARDWARE) {
                            if (flashOnMs > 0 && flashOffMs == 0)
                                flashMode = Flash::NONE;
                            color = 0x000000ff;
                        }
                        adjusted_brightness = LED_BRIGHTNESS_ATTENTION;
                        break;
                    default:
                        break;
                }

                if (flashMode == Flash::NONE) {
                    color = 0;
                    flashOnMs = 0;
                    flashOffMs = 0;
                }

                color = calibrateColor(color & COLOR_MASK, adjusted_brightness);

                mIndicator << std::hex <<  "0x" << std::setfill('0') << std::setw(8) << color << std::setw(0) << " " << flashOnMs << " " << flashOffMs << std::endl;

#ifdef LED_BLN_NODE
                if (mBacklightNotification.good) {
                    mBacklightNotification << (state.color & COLOR_MASK) ? 1 : 0;
                }
#endif
            }
            break;

        default:
            break;
    }

    return Status::LIGHT_NOT_SUPPORTED;
}

Return<void> Light::getSupportedTypes(getSupportedTypes_cb _hidl_cb) {
    std::vector<Type> supportedTypes {
        Type::BACKLIGHT,
        Type::BUTTONS,
        Type::BATTERY,
        Type::NOTIFICATIONS,
        Type::ATTENTION
    };

    _hidl_cb(supportedTypes);

    return Void();
}

uint32_t Light::rgbToBrightness(const LightState& state) {
    uint32_t color = state.color & COLOR_MASK;
    return ((77 * ((color >> 16) & 0xff)) + (150 * ((color >> 8) & 0xff)) +
            (29 * (color & 0xff))) >> 8;
}

uint32_t Light::calibrateColor(uint32_t color, uint32_t brightness)
{
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
