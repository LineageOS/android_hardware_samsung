/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define LOG_TAG "GnssPowerIndicationAidl"

#include "GnssPowerIndication.h"
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <log/log.h>
#include <utils/SystemClock.h>

namespace aidl::android::hardware::gnss {

std::shared_ptr<IGnssPowerIndicationCallback> GnssPowerIndication::sCallback = nullptr;

ndk::ScopedAStatus GnssPowerIndication::setCallback(
        const std::shared_ptr<IGnssPowerIndicationCallback>& callback) {
    ALOGD("setCallback");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;
    sCallback->setCapabilitiesCb(IGnssPowerIndicationCallback::CAPABILITY_TOTAL |
                                 IGnssPowerIndicationCallback::CAPABILITY_SINGLEBAND_TRACKING |
                                 IGnssPowerIndicationCallback::CAPABILITY_MULTIBAND_TRACKING |
                                 IGnssPowerIndicationCallback::CAPABILITY_SINGLEBAND_ACQUISITION |
                                 IGnssPowerIndicationCallback::CAPABILITY_MULTIBAND_ACQUISITION |
                                 IGnssPowerIndicationCallback::CAPABILITY_OTHER_MODES);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssPowerIndication::requestGnssPowerStats() {
    ALOGD("requestGnssPowerStats");
    std::unique_lock<std::mutex> lock(mMutex);

    ElapsedRealtime elapsedRealtime = {
            .flags = ElapsedRealtime::HAS_TIMESTAMP_NS | ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS,
            .timestampNs = ::android::elapsedRealtimeNano(),
            .timeUncertaintyNs = 1000,
    };
    GnssPowerStats gnssPowerStats = {
            .elapsedRealtime = elapsedRealtime,
            .totalEnergyMilliJoule = 1.500e+3 + numLocationReported * 22.0,
            .singlebandTrackingModeEnergyMilliJoule = 0.0,
            .multibandTrackingModeEnergyMilliJoule = 1.28e+2 + numLocationReported * 4.0,
            .singlebandAcquisitionModeEnergyMilliJoule = 0.0,
            .multibandAcquisitionModeEnergyMilliJoule = 3.65e+2 + numLocationReported * 15.0,
            .otherModesEnergyMilliJoule = {1.232e+2, 3.234e+3},
    };
    sCallback->gnssPowerStatsCb(gnssPowerStats);
    return ndk::ScopedAStatus::ok();
}

void GnssPowerIndication::notePowerConsumption() {
    numLocationReported++;
}

}  // namespace aidl::android::hardware::gnss
