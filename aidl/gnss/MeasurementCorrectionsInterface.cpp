/*
 * Copyright (C) 2021 The Android Open Source Project
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

#define LOG_TAG "MeasurementCorrectionsInterface"

#include "MeasurementCorrectionsInterface.h"
#include <inttypes.h>
#include <log/log.h>

namespace aidl::android::hardware::gnss::measurement_corrections {

std::shared_ptr<IMeasurementCorrectionsCallback> MeasurementCorrectionsInterface::sCallback =
        nullptr;

ndk::ScopedAStatus MeasurementCorrectionsInterface::setCorrections(
        const MeasurementCorrections& corrections) {
    ALOGD("setCorrections");
    ALOGD("corrections = lat: %f, lng: %f, alt: %f, hUnc: %f, vUnc: %f, toa: %llu, "
          "satCorrections.size: %d",
          corrections.latitudeDegrees, corrections.longitudeDegrees, corrections.altitudeMeters,
          corrections.horizontalPositionUncertaintyMeters,
          corrections.verticalPositionUncertaintyMeters,
          static_cast<unsigned long long>(corrections.toaGpsNanosecondsOfWeek),
          static_cast<int>(corrections.satCorrections.size()));
    for (auto singleSatCorrection : corrections.satCorrections) {
        ALOGD("singleSatCorrection = flags: %d, constellation: %d, svid: %d"
              ", cfHz: %" PRId64
              ", probLos: %f, combinedEpl: %f, combinedEplUnc: %f, combinedAttenuation: %f"
              ", excessPathInfos.size: %d",
              singleSatCorrection.singleSatCorrectionFlags, singleSatCorrection.constellation,
              singleSatCorrection.svid, singleSatCorrection.carrierFrequencyHz,
              singleSatCorrection.probSatIsLos, singleSatCorrection.combinedExcessPathLengthMeters,
              singleSatCorrection.combinedExcessPathLengthUncertaintyMeters,
              singleSatCorrection.combinedAttenuationDb,
              static_cast<int>(singleSatCorrection.excessPathInfos.size()));

        for (auto excessPathInfo : singleSatCorrection.excessPathInfos) {
            ALOGD("excessPathInfo = epl: %f, eplUnc: %f, attenuation: %f",
                  excessPathInfo.excessPathLengthMeters,
                  excessPathInfo.excessPathLengthUncertaintyMeters, excessPathInfo.attenuationDb);
            ALOGD("reflecting plane = lat: %f, lng: %f, alt: %f, azm: %f",
                  excessPathInfo.reflectingPlane.latitudeDegrees,
                  excessPathInfo.reflectingPlane.longitudeDegrees,
                  excessPathInfo.reflectingPlane.altitudeMeters,
                  excessPathInfo.reflectingPlane.reflectingPlaneAzimuthDegrees);
        }
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus MeasurementCorrectionsInterface::setCallback(
        const std::shared_ptr<IMeasurementCorrectionsCallback>& callback) {
    ALOGD("MeasurementCorrections::setCallback");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;
    auto ret = sCallback->setCapabilitiesCb(
            IMeasurementCorrectionsCallback::CAPABILITY_LOS_SATS |
            IMeasurementCorrectionsCallback::CAPABILITY_EXCESS_PATH_LENGTH |
            IMeasurementCorrectionsCallback::CAPABILITY_REFLECTING_PLANE);
    if (!ret.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }
    return ndk::ScopedAStatus::ok();
}
}  // namespace aidl::android::hardware::gnss::measurement_corrections
