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

const std::pair<int32_t, TouchscreenGesture::GestureInfo> TouchscreenGesture::kSingleTapEntry = {
    // clang-format off
    {5, {0x1c7, "Single Tap"}},
    // clang-format on
};

int32_t count = 0;

bool TouchscreenGesture::isSupported() {
    return mHasEpenGestureNode || mHasTspCmdNode;
}

ndk::ScopedAStatus TouchscreenGesture::getSupportedGestures(std::vector<Gesture>* _aidl_return) {
    std::vector<Gesture> gestures;

    if (mHasEpenGestureNode) {
        for (const auto& entry : kGestureInfoMap) {
            gestures.push_back({count, entry.second.name, entry.second.keycode});
            count++;
        }
    }
    if (mHasTspCmdNode) {
        gestures.push_back({count, kSingleTapEntry.second.name, kSingleTapEntry.second.keycode});
    }

    *_aidl_return = gestures;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus TouchscreenGesture::setGestureEnabled(const Gesture& gesture, bool enabled) {
    if (mHasTspCmdNode && (gesture.id == count)) {
        std::fstream file(TSP_CMD_NODE);
        file << "singletap_enable," << (enabled ? "1" : "0");
    } else {
        std::fstream file(EPEN_GESTURE_NODE);
        int gestureMode;
        int mask = 1 << gesture.id;

        file >> gestureMode;

        if (enabled)
            gestureMode |= mask;
        else
            gestureMode &= ~mask;

        file << gestureMode;
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
