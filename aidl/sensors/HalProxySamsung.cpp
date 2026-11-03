/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "HalProxySamsung.h"

#include <ConvertUtils.h>
#include <sensors.sysprop.h>

// #define VERBOSE

using namespace ::android::sensors::samsung;

namespace aidl {
namespace android {
namespace hardware {
namespace sensors {
namespace implementation {

using ::android::hardware::sensors::V1_0::Result;
using ::android::hardware::sensors::V2_0::implementation::ScopedWakelock;
using ::android::hardware::sensors::V2_1::SensorType;

namespace {

// Mirrors HalProxyAidl, whose activate() is private.
ndk::ScopedAStatus resultToAStatus(Result result) {
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

}  // namespace

HalProxySamsung::HalProxySamsung() {
    using Workaround = SensorsHalProperties::device_orientation_workaround_values;
    auto workaround = SensorsHalProperties::device_orientation_workaround();
    if (workaround == Workaround::HIDE) {
        mHideDeviceOrientation = true;
    } else if (workaround == Workaround::SWAP_UPRIGHT_WHEN_FOLDED) {
        for (const auto& [handle, sensor] : getSensors()) {
            if (sensor.typeAsString == "com.samsung.sensor.folding_state") {
                mFoldingStateHandle = handle;
            }
        }
    }

    auto companion = SensorsHalProperties::proximity_companion();
    if (companion.has_value()) {
        mProximityCompanion.emplace(*this, *companion);
    }
}

ndk::ScopedAStatus HalProxySamsung::activate(int32_t in_sensorHandle, bool in_enabled) {
    if (!mProximityCompanion) {
        return resultToAStatus(HalProxy::activate(in_sensorHandle, in_enabled));
    }

    if (mProximityCompanion->isCompanion(in_sensorHandle)) {
        mProximityCompanion->setRequested(in_enabled);
        return ndk::ScopedAStatus::ok();
    }

    // The companion must run before the proximity driver starts and until it has stopped.
    if (in_enabled) {
        mProximityCompanion->setProximityActive(in_sensorHandle, true);
    }
    Result result = HalProxy::activate(in_sensorHandle, in_enabled);
    if (!in_enabled || result != Result::OK) {
        mProximityCompanion->setProximityActive(in_sensorHandle, false);
    }
    return resultToAStatus(result);
}

void HalProxySamsung::postEventsToMessageQueue(const std::vector<Event>& events,
                                               size_t numWakeupEvents, ScopedWakelock wakelock) {
    if (mFoldingStateHandle < 0) {
        HalProxy::postEventsToMessageQueue(events, numWakeupEvents, std::move(wakelock));
        return;
    }

    for (const Event& event : events) {
        if (event.sensorHandle == mFoldingStateHandle) mFolded = event.u.scalar < 1.0f;
    }

    if (!mFolded) {
        HalProxy::postEventsToMessageQueue(events, numWakeupEvents, std::move(wakelock));
        return;
    }

    std::vector<Event> remapped(events);
    for (Event& event : remapped) {
        if (event.sensorType == SensorType::DEVICE_ORIENTATION) {
            // The cover screen faces the other way, so upright and upside down are swapped.
            event.u.scalar = (6 - static_cast<int>(event.u.scalar)) % 4;
        }
    }
    HalProxy::postEventsToMessageQueue(remapped, numWakeupEvents, std::move(wakelock));
}

ndk::ScopedAStatus HalProxySamsung::getSensorsList(
        std::vector<::aidl::android::hardware::sensors::SensorInfo>* _aidl_return) {
    // SensorService calls this right after initialize(), which disabled every sensor
    // without going through activate().
    if (mProximityCompanion) {
        mProximityCompanion->reset();
    }

    for (const auto& sensor : HalProxy::getSensors()) {
        SensorInfo dst = sensor.second;

        // Lets the framework derive screen orientation from the accelerometer instead.
        if (mHideDeviceOrientation && dst.type == SensorType::DEVICE_ORIENTATION) {
            continue;
        }

        if (dst.requiredPermission == "com.samsung.permission.SSENSOR") {
            dst.requiredPermission = "";
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
