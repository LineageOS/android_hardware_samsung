/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fstream>

#include "TouchscreenGesture.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

const std::map<int32_t, TouchscreenGesture::GestureInfo> TouchscreenGesture::kGestureInfoMap = {
    // clang-format off
    {0, {0x2f1, "Swipe up stylus"}},
    {1, {0x2f2, "Swipe down stylus"}},
    {2, {0x2f3, "Swipe left stylus"}},
    {3, {0x2f4, "Swipe right stylus"}},
    {4, {0x2f5, "Long press stylus"}},
    // clang-format on
};

bool TouchscreenGesture::isSupported() {
    std::ifstream file(TOUCHSCREEN_GESTURE_NODE);
    return file.good();
}

ndk::ScopedAStatus TouchscreenGesture::getSupportedGestures(std::vector<Gesture>* _aidl_return) {
    std::vector<Gesture> gestures;

    for (const auto& entry : kGestureInfoMap) {
        gestures.push_back({entry.first, entry.second.name, entry.second.keycode});
    }

    *_aidl_return = gestures;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus TouchscreenGesture::setGestureEnabled(const Gesture& gesture, bool enabled) {
    std::fstream file(TOUCHSCREEN_GESTURE_NODE);
    int gestureMode;
    int mask = 1 << gesture.id;

    file >> gestureMode;

    if (enabled)
        gestureMode |= mask;
    else
        gestureMode &= ~mask;

    file << gestureMode;

    return ndk::ScopedAStatus::ok();
}

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
