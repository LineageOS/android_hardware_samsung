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

#include <fstream>

#include "samsung_gestures.h"

namespace vendor {
namespace lineage {
namespace touch {
namespace V1_0 {
namespace samsung {

static constexpr const char* kCmdPath = TSP_CMD_NODE;

static constexpr const char* kEpenPath = TOUCHSCREEN_GESTURE_NODE;

bool TouchscreenGesture::isSupported() {
    std::ifstream file(kCmdPath);
    return file.good();
}

// Methods from ::vendor::lineage::touch::V1_0::ITouchscreenGesture follow.
Return<void> TouchscreenGesture::getSupportedGestures(getSupportedGestures_cb resultCb) {
    std::vector<Gesture> gestures;

    for (const auto& entry : kGestureInfoMap) {
        gestures.push_back({entry.first, entry.second.name, entry.second.keycode});
    }
    resultCb(gestures);

    return Void();
}

Return<bool> TouchscreenGesture::setGestureEnabled(
    const ::vendor::lineage::touch::V1_0::Gesture& gesture, bool enabled) {
    const auto entry = kGestureInfoMap.find(gesture.id);

    std::fstream file(entry->second.path);

    if (strcmp(entry->second.path, kEpenPath) == 0) {
        int gestureMode;
        int mask = 1 << gesture.id;

        file >> gestureMode;

        if (enabled)
            gestureMode |= mask;
        else
            gestureMode &= ~mask;

        file << gestureMode;
    } else {
        file << entry->second.command << (enabled ? "1" : "0");
    }


    return !file.fail();
}

// Methods from ::android::hidl::base::V1_0::IBase follow.

}  // namespace samsung
}  // namespace V1_0
}  // namespace touch
}  // namespace lineage
}  // namespace vendor
