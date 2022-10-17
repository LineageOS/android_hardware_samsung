/*
 * Copyright (C) 2022 The LineageOS Project
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

#pragma once

#include "samsung_touch.h"
#include "TouchscreenGesture.h"

namespace vendor {
namespace lineage {
namespace touch {
namespace V1_0 {
namespace samsung {

const std::map<int32_t, TouchscreenGesture::GestureInfo> TouchscreenGesture::kGestureInfoMap = {
    // clang-format off
    {0, {0x2f1, "Swipe up stylus", TOUCHSCREEN_GESTURE_NODE, ""}},
    {1, {0x2f2, "Swipe down stylus", TOUCHSCREEN_GESTURE_NODE, ""}},
    {2, {0x2f3, "Swipe left stylus", TOUCHSCREEN_GESTURE_NODE, ""}},
    {3, {0x2f4, "Swipe right stylus", TOUCHSCREEN_GESTURE_NODE, ""}},
    {4, {0x2f5, "Long press stylus", TOUCHSCREEN_GESTURE_NODE, ""}},
    {5, {0x1c7, "Single Tap", TSP_CMD_NODE, "singletap_enable,"}},
    // clang-format on
};

}  // namespace samsung
}  // namespace V1_0
}  // namespace touch
}  // namespace lineage
}  // namespace vendor
