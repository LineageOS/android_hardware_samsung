/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/touch/BnTouchscreenGesture.h>
#include <samsung_touch.h>
#include <fstream>
#include <map>

namespace aidl {
namespace vendor {
namespace lineage {
namespace touch {

class TouchscreenGesture : public BnTouchscreenGesture {
  public:
    bool isSupported();

    ndk::ScopedAStatus getSupportedGestures(std::vector<Gesture>* _aidl_return) override;
    ndk::ScopedAStatus setGestureEnabled(const Gesture& gesture, bool enabled) override;

  private:
    typedef struct {
        int32_t keycode;
        const char* name;
    } GestureInfo;
    static const std::map<int32_t, GestureInfo> kGestureInfoMap;  // id -> info
    static const std::pair<int32_t, GestureInfo> kSingleTapEntry;

    bool mHasEpenGestureNode = std::ifstream(EPEN_GESTURE_NODE).good();
    bool mIsTspCmdSupported = false;
};

}  // namespace touch
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
