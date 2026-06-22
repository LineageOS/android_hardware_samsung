/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "HalProxySamsung.h"

#include <ConvertUtils.h>

#include <android-base/properties.h>
#include <log/log.h>

#include <chrono>
#include <string>
#include <thread>
#include <utility>

// #define VERBOSE

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

namespace {

using ::android::hardware::sensors::V1_0::Result;

constexpr char kStkProximityReactivateMsProperty[] =
        "persist.vendor.sensors.stk_prox_reactivate_ms";
constexpr char kStkProximityRawNearThresholdProperty[] =
        "persist.vendor.sensors.stk_prox_raw_near_threshold";
constexpr int32_t kDefaultStkProximityRawNearThreshold = 220;
constexpr int32_t kMinimumReactivateIntervalMs = 100;
constexpr int32_t kReactivateOffOnDelayMs = 20;

ndk::ScopedAStatus resultToScopedAStatus(Result result) {
    switch (result) {
        case Result::OK:
            return ndk::ScopedAStatus::ok();
        case Result::PERMISSION_DENIED:
            return ndk::ScopedAStatus::fromExceptionCode(EX_SECURITY);
        case Result::NO_MEMORY:
            return ndk::ScopedAStatus::fromServiceSpecificError(ISensors::ERROR_NO_MEMORY);
        case Result::BAD_VALUE:
            return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        case Result::INVALID_OPERATION:
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        default:
            return ndk::ScopedAStatus::fromExceptionCode(EX_TRANSACTION_FAILED);
    }
}

Result unwrapHalResult(const ::android::hardware::Return<Result>& result) {
    if (!result.isOk()) {
        return Result::INVALID_OPERATION;
    }

    return static_cast<Result>(result);
}

}  // anonymous namespace

HalProxySamsung::~HalProxySamsung() {
    stopProximityReactivationWorker();
}

bool HalProxySamsung::isTrackedStkProximityHandle(int32_t sensorHandle) const {
    return mStkProximityHandle.load() == sensorHandle;
}

int32_t HalProxySamsung::getProximityReactivateIntervalMs() const {
    const int32_t intervalMs =
            ::android::base::GetIntProperty(kStkProximityReactivateMsProperty, 0);

    if (intervalMs > 0 && intervalMs < kMinimumReactivateIntervalMs) {
        return kMinimumReactivateIntervalMs;
    }

    return intervalMs;
}

int32_t HalProxySamsung::getProximityRawNearThreshold() const {
    return ::android::base::GetIntProperty(kStkProximityRawNearThresholdProperty,
                                           kDefaultStkProximityRawNearThreshold);
}

void HalProxySamsung::startProximityReactivationWorkerLocked(int32_t sensorHandle) {
    if (mProximityWorker.joinable()) {
        return;
    }

    mProximityWorkerStop = false;
    mProximityWorker = std::thread(&HalProxySamsung::proximityReactivationWorker, this,
                                   sensorHandle);
}

void HalProxySamsung::stopProximityReactivationWorker() {
    std::thread worker;

    {
        std::lock_guard<std::mutex> lock(mProximityWorkerMutex);
        mStkProximityEnabled = false;
        mProximityWorkerStop = true;
        mProximityWorkerCond.notify_all();

        if (mProximityWorker.joinable()) {
            worker = std::move(mProximityWorker);
        }
    }

    if (worker.joinable()) {
        worker.join();
    }

    {
        std::lock_guard<std::mutex> lock(mProximityWorkerMutex);
        mProximityWorkerStop = false;
    }
}

void HalProxySamsung::proximityReactivationWorker(int32_t sensorHandle) {
    while (true) {
        const int32_t intervalMs = getProximityReactivateIntervalMs();

        {
            std::unique_lock<std::mutex> lock(mProximityWorkerMutex);

            if (mProximityWorkerStop || !mStkProximityEnabled || intervalMs <= 0) {
                break;
            }

            if (mProximityWorkerCond.wait_for(lock, std::chrono::milliseconds(intervalMs),
                                              [this]() {
                                                  return mProximityWorkerStop ||
                                                         !mStkProximityEnabled;
                                              })) {
                continue;
            }

            if (mProximityWorkerStop || !mStkProximityEnabled) {
                break;
            }
        }

        if (!mStkProximityNear.load()) {
            continue;
        }

        std::lock_guard<std::mutex> halLock(mHalOperationMutex);

        const auto offReturn = HalProxy::activate(sensorHandle, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(kReactivateOffOnDelayMs));
        const auto onReturn = HalProxy::activate(sensorHandle, true);

        const Result offResult = unwrapHalResult(offReturn);
        const Result onResult = unwrapHalResult(onReturn);

        if (offResult != Result::OK || onResult != Result::OK) {
            ALOGW("STK proximity reactivation returned off=%d on=%d",
                  static_cast<int>(offResult), static_cast<int>(onResult));
        }
    }
}

void HalProxySamsung::postEventsToMessageQueue(
        const std::vector<::android::hardware::sensors::V2_1::Event>& events,
        size_t numWakeupEvents,
        ::android::hardware::sensors::V2_0::implementation::ScopedWakelock wakelock) {
    if (events.empty()) {
        HalProxy::postEventsToMessageQueue(events, numWakeupEvents, std::move(wakelock));
        return;
    }

    std::vector<::android::hardware::sensors::V2_1::Event> filteredEvents;
    filteredEvents.reserve(events.size());

    size_t filteredWakeupEvents = numWakeupEvents;
    const bool workaroundEnabled = getProximityReactivateIntervalMs() > 0;
    const int32_t rawNearThreshold = getProximityRawNearThreshold();

    for (const auto& event : events) {
        bool dropEvent = false;

        if (isTrackedStkProximityHandle(event.sensorHandle)) {
            const float distance = event.u.vec3.x;
            const float raw = event.u.vec3.z;

            const bool reportedNear = distance >= 0.0f && distance < 1.0f;
            const bool rawNear = raw >= rawNearThreshold;

            if (reportedNear) {
                mStkProximityNear.store(true);
            } else if (workaroundEnabled && rawNear) {
                /*
                 * STK33911 can report a far distance after reactivation even
                 * while the raw channel still looks covered. Do not forward
                 * that false far event to framework, otherwise the in-call
                 * display wakes while the phone is still at the ear.
                 */
                mStkProximityNear.store(true);
                dropEvent = true;
                if (filteredWakeupEvents > 0) {
                    filteredWakeupEvents--;
                }
            } else {
                mStkProximityNear.store(false);
            }
        }

        if (!dropEvent) {
            filteredEvents.push_back(event);
        }
    }

    if (filteredEvents.empty()) {
        return;
    }

    HalProxy::postEventsToMessageQueue(filteredEvents, filteredWakeupEvents, std::move(wakelock));
}

ndk::ScopedAStatus HalProxySamsung::activate(int32_t in_sensorHandle, bool in_enabled) {
    const bool isStkProximity = isTrackedStkProximityHandle(in_sensorHandle);

    if (isStkProximity && !in_enabled) {
        mStkProximityNear.store(false);
        stopProximityReactivationWorker();
    }

    Result result;
    {
        std::lock_guard<std::mutex> halLock(mHalOperationMutex);
        result = unwrapHalResult(HalProxy::activate(in_sensorHandle, in_enabled));
    }

    ndk::ScopedAStatus status = resultToScopedAStatus(result);

    if (isStkProximity && in_enabled && status.isOk()) {
        const int32_t intervalMs = getProximityReactivateIntervalMs();

        std::lock_guard<std::mutex> lock(mProximityWorkerMutex);
        mStkProximityEnabled = true;

        if (intervalMs > 0) {
            startProximityReactivationWorkerLocked(in_sensorHandle);
        }
    }

    return status;
}

ndk::ScopedAStatus HalProxySamsung::getSensorsList(
        std::vector<::aidl::android::hardware::sensors::SensorInfo>* _aidl_return) {
    for (const auto& sensor : HalProxy::getSensors()) {
        SensorInfo dst = sensor.second;

        if (dst.requiredPermission == "com.samsung.permission.SSENSOR") {
            dst.requiredPermission = "";
        }

        const std::string sensorName(dst.name.c_str());
        const std::string sensorTypeAsString(dst.typeAsString.c_str());
        if ((sensorTypeAsString == SENSOR_STRING_TYPE_PROXIMITY ||
             sensorTypeAsString == "com.samsung.sensor.physical_proximity") &&
            sensorName.find("STK33911") != std::string::npos &&
            sensorName.find("Wakeup") != std::string::npos) {
            const int32_t oldHandle = mStkProximityHandle.exchange(dst.sensorHandle);
            if (oldHandle != dst.sensorHandle) {
                ALOGI("Tracking STK wakeup proximity sensor: name=%s typeStr=%s handle=%d",
                      sensorName.c_str(), sensorTypeAsString.c_str(), dst.sensorHandle);
            }
        }

        if (dst.typeAsString == "com.samsung.sensor.physical_proximity" ||
            dst.typeAsString == "com.samsung.sensor.hover_proximity") {
            ALOGI("Fixing %s", dst.typeAsString.c_str());
            dst.type = ::android::hardware::sensors::V2_1::SensorType::PROXIMITY;
            dst.typeAsString = SENSOR_STRING_TYPE_PROXIMITY;
            dst.maxRange = 1;
        }

#ifdef VERBOSE
        ALOGI("SENSOR NAME:%s           ", dst.name.c_str());
        ALOGI("       VENDOR:%s         ", dst.name.c_str());
        ALOGI("       TYPE:%d           ", (uint32_t)dst.type);
        ALOGI("       TYPE_AS_STRING:%s ", dst.typeAsString.c_str());
#endif

        _aidl_return->push_back(convertSensorInfo(dst));
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace implementation
}  // namespace sensors
}  // namespace hardware
}  // namespace android
}  // namespace aidl
