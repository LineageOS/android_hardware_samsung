/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ProximityCompanion.h"

#include <log/log.h>

#include <string_view>

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V2_1::SensorType;

ProximityCompanion::ProximityCompanion(HalProxy& proxy, const std::string& companionType)
    : mProxy(proxy) {
    for (const auto& [handle, sensor] : proxy.getSensors()) {
        std::string_view type(sensor.typeAsString.c_str());
        if (type == companionType) {
            mCompanionHandle = handle;
        } else if (sensor.type == SensorType::PROXIMITY ||
                   type.find("proximity") != std::string_view::npos) {
            mProximityHandles.insert(handle);
        }
    }

    if (mCompanionHandle < 0) {
        ALOGE("Proximity companion sensor %s not found", companionType.c_str());
        mProximityHandles.clear();
    }
}

void ProximityCompanion::setRequested(bool requested) {
    std::lock_guard<std::mutex> lock(mMutex);
    mRequested = requested;
    updateLocked();
}

void ProximityCompanion::setProximityActive(int32_t handle, bool active) {
    if (!mProximityHandles.contains(handle)) return;

    std::lock_guard<std::mutex> lock(mMutex);
    if (active) {
        mActiveProximity.insert(handle);
    } else {
        mActiveProximity.erase(handle);
    }
    updateLocked();
}

void ProximityCompanion::reset() {
    std::lock_guard<std::mutex> lock(mMutex);
    mActiveProximity.clear();
    mRequested = false;
    mEnabled = false;
}

void ProximityCompanion::updateLocked() {
    bool enable = mRequested || !mActiveProximity.empty();
    if (enable == mEnabled) return;

    // A client that asked for the companion itself has already configured its rate.
    if (enable && !mRequested) {
        mProxy.batch(mCompanionHandle, 0, 0);
    }
    Result result = mProxy.activate(mCompanionHandle, enable);
    if (result != Result::OK) {
        ALOGE("Failed to %s proximity companion: %d", enable ? "enable" : "disable",
              static_cast<int>(result));
        return;
    }
    mEnabled = enable;
}

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
