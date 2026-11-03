/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <HalProxy.h>

#include <mutex>
#include <set>
#include <string>

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

/*
 * Keeps a companion sensor active on the sensor hub whenever a proximity sensor is active.
 *
 * Samsung hubs run the proximity driver from island memory, where it calls into the
 * call_gesture algorithm. That code is only mapped while call_gesture is active, so the hub
 * crashes if proximity runs without it.
 */
class ProximityCompanion {
  public:
    using HalProxy = ::android::hardware::sensors::V2_1::implementation::HalProxy;

    ProximityCompanion(HalProxy& proxy, const std::string& companionType);

    bool isCompanion(int32_t handle) const { return handle == mCompanionHandle; }

    // A client asked for the companion sensor itself.
    void setRequested(bool requested);
    // Ignores handles that are not proximity sensors.
    void setProximityActive(int32_t handle, bool active);
    // Forgets all state after HalProxy disabled every sensor behind our back.
    void reset();

  private:
    void updateLocked();

    HalProxy& mProxy;
    int32_t mCompanionHandle = -1;
    std::set<int32_t> mProximityHandles;

    std::mutex mMutex;
    std::set<int32_t> mActiveProximity;
    bool mRequested = false;
    bool mEnabled = false;
};

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
