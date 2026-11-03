/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <HalProxyAidl.h>

#include "ProximityCompanion.h"

#include <atomic>
#include <optional>

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

class HalProxySamsung : public HalProxyAidl {
  public:
    HalProxySamsung();

  private:
    ndk::ScopedAStatus activate(int32_t in_sensorHandle, bool in_enabled) override;
    ndk::ScopedAStatus getSensorsList(
            std::vector<::aidl::android::hardware::sensors::SensorInfo>* _aidl_return) override;

    void postEventsToMessageQueue(
            const std::vector<Event>& events, size_t numWakeupEvents,
            ::android::hardware::sensors::V2_0::implementation::ScopedWakelock wakelock) override;

    bool mHideDeviceOrientation = false;
    int32_t mFoldingStateHandle = -1;
    std::atomic<bool> mFolded = false;
    std::optional<ProximityCompanion> mProximityCompanion;
};

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
