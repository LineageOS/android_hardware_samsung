/*
 * Copyright (C) 2024 The Android Open Source Project
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

#define LOG_TAG "GnssAssistanceInterfaceAidl"

#include "GnssAssistanceInterface.h"
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <log/log.h>

namespace aidl::android::hardware::gnss::gnss_assistance {

std::shared_ptr<IGnssAssistanceCallback> GnssAssistanceInterface::sCallback = nullptr;

ndk::ScopedAStatus GnssAssistanceInterface::setCallback(
        const std::shared_ptr<IGnssAssistanceCallback>& callback) {
    ALOGD("setCallback");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssAssistanceInterface::injectGnssAssistance(
        const GnssAssistance& gnssAssistance) {
    ALOGD("injectGnssAssistance. %s", gnssAssistance.toString().c_str());
    if (!gnssAssistance.gpsAssistance) {
        ALOGE("Empty GpsAssistance");
        return ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_INVALID_ARGUMENT);
    }
    if (gnssAssistance.gpsAssistance->satelliteEphemeris.empty()) {
        ALOGE("Empty SatelliteEphemeris");
        return ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_INVALID_ARGUMENT);
    }
    if (gnssAssistance.ionexAssistance) {
        const IonexAssistance& ionexAssistance = *gnssAssistance.ionexAssistance;

        const size_t tecMapSize = ionexAssistance.tecMapSnapshot.tecMap.size();
        const size_t expectedSize =
                static_cast<size_t>(ionexAssistance.header.axesInfo.latitudeAxis.numPoints *
                                    ionexAssistance.header.axesInfo.longitudeAxis.numPoints);
        if (tecMapSize == 0 || tecMapSize != expectedSize) {
            ALOGE("Invalid IONEX tecMap size. Expected: %zu, Actual: %zu", expectedSize,
                  tecMapSize);
            return ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_INVALID_ARGUMENT);
        }
    }
    return ndk::ScopedAStatus::ok();
}
}  // namespace aidl::android::hardware::gnss::gnss_assistance
