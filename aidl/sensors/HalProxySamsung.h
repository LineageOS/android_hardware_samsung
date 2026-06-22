/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <HalProxyAidl.h>

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

class HalProxySamsung : public HalProxyAidl {
  public:
    ~HalProxySamsung();

    ndk::ScopedAStatus getSensorsList(
            std::vector<::aidl::android::hardware::sensors::SensorInfo>* _aidl_return) override;

    ndk::ScopedAStatus activate(int32_t in_sensorHandle, bool in_enabled) override;

    void postEventsToMessageQueue(
            const std::vector<::android::hardware::sensors::V2_1::Event>& events,
            size_t numWakeupEvents,
            ::android::hardware::sensors::V2_0::implementation::ScopedWakelock wakelock) override;

  private:
    bool isTrackedStkProximityHandle(int32_t sensorHandle) const;
    int32_t getProximityReactivateIntervalMs() const;
    int32_t getProximityRawNearThreshold() const;
    void startProximityReactivationWorkerLocked(int32_t sensorHandle);
    void stopProximityReactivationWorker();
    void proximityReactivationWorker(int32_t sensorHandle);

    std::atomic<int32_t> mStkProximityHandle{-1};
    std::atomic<bool> mStkProximityNear{false};

    std::mutex mHalOperationMutex;
    std::mutex mProximityWorkerMutex;
    std::condition_variable mProximityWorkerCond;
    std::thread mProximityWorker;
    bool mProximityWorkerStop = false;
    bool mStkProximityEnabled = false;
};

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
