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

#pragma once

#include <aidl/android/hardware/gnss/BnAGnss.h>
#include <aidl/android/hardware/gnss/BnAGnssRil.h>
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <aidl/android/hardware/gnss/BnGnssAntennaInfo.h>
#include <aidl/android/hardware/gnss/BnGnssBatching.h>
#include <aidl/android/hardware/gnss/BnGnssConfiguration.h>
#include <aidl/android/hardware/gnss/BnGnssDebug.h>
#include <aidl/android/hardware/gnss/BnGnssMeasurementInterface.h>
#include <aidl/android/hardware/gnss/BnGnssPowerIndication.h>
#include <aidl/android/hardware/gnss/BnGnssPsds.h>
#include <aidl/android/hardware/gnss/gnss_assistance/BnGnssAssistanceInterface.h>
#include <aidl/android/hardware/gnss/measurement_corrections/BnMeasurementCorrectionsInterface.h>
#include <aidl/android/hardware/gnss/visibility_control/BnGnssVisibilityControl.h>
#include <atomic>
#include <mutex>
#include <thread>
#include "GnssConfiguration.h"
#include "GnssMeasurementInterface.h"
#include "GnssPowerIndication.h"
#include "Utils.h"

namespace aidl::android::hardware::gnss {

class Gnss : public BnGnss {
  public:
    Gnss();
    ~Gnss() { stop(); };
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssCallback>& callback) override;
    ndk::ScopedAStatus start() override;
    ndk::ScopedAStatus stop() override;
    ndk::ScopedAStatus close() override;

    ndk::ScopedAStatus injectTime(int64_t timeMs, int64_t timeReferenceMs,
                                  int uncertaintyMs) override;
    ndk::ScopedAStatus injectLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus injectBestLocation(const GnssLocation& location) override;
    ndk::ScopedAStatus deleteAidingData(GnssAidingData aidingDataFlags) override;
    ndk::ScopedAStatus setPositionMode(const PositionModeOptions& options) override;
    ndk::ScopedAStatus startSvStatus() override;
    ndk::ScopedAStatus stopSvStatus() override;
    ndk::ScopedAStatus startNmea() override;
    ndk::ScopedAStatus stopNmea() override;

    ndk::ScopedAStatus getExtensionPsds(std::shared_ptr<IGnssPsds>* iGnssPsds) override;
    ndk::ScopedAStatus getExtensionGnssConfiguration(
            std::shared_ptr<IGnssConfiguration>* iGnssConfiguration) override;
    ndk::ScopedAStatus getExtensionGnssPowerIndication(
            std::shared_ptr<IGnssPowerIndication>* iGnssPowerIndication) override;
    ndk::ScopedAStatus getExtensionGnssMeasurement(
            std::shared_ptr<IGnssMeasurementInterface>* iGnssMeasurement) override;
    ndk::ScopedAStatus getExtensionGnssBatching(
            std::shared_ptr<IGnssBatching>* iGnssBatching) override;
    ndk::ScopedAStatus getExtensionGnssGeofence(
            std::shared_ptr<IGnssGeofence>* iGnssGeofence) override;
    ndk::ScopedAStatus getExtensionGnssNavigationMessage(
            std::shared_ptr<IGnssNavigationMessageInterface>* iGnssNavigationMessage) override;
    ndk::ScopedAStatus getExtensionAGnss(std::shared_ptr<IAGnss>* iAGnss) override;
    ndk::ScopedAStatus getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* iAGnssRil) override;
    ndk::ScopedAStatus getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* iGnssDebug) override;
    ndk::ScopedAStatus getExtensionGnssVisibilityControl(
            std::shared_ptr<android::hardware::gnss::visibility_control::IGnssVisibilityControl>*
                    iGnssVisibilityControl) override;
    ndk::ScopedAStatus getExtensionGnssAntennaInfo(
            std::shared_ptr<IGnssAntennaInfo>* iGnssAntennaInfo) override;
    ndk::ScopedAStatus getExtensionMeasurementCorrections(
            std::shared_ptr<android::hardware::gnss::measurement_corrections::
                                    IMeasurementCorrectionsInterface>* iMeasurementCorrections)
            override;
    ndk::ScopedAStatus getExtensionGnssAssistanceInterface(
            std::shared_ptr<android::hardware::gnss::gnss_assistance::IGnssAssistanceInterface>*
                    iGnssAssistanceInterface) override;

    void reportSvStatus() const;
    void setGnssMeasurementEnabled(const bool enabled);
    void setGnssMeasurementInterval(const long intervalMs);
    std::shared_ptr<GnssLocation> getLastLocation() const;
    std::shared_ptr<GnssConfiguration> mGnssConfiguration;
    std::shared_ptr<GnssPowerIndication> mGnssPowerIndication;
    std::shared_ptr<GnssMeasurementInterface> mGnssMeasurementInterface;

  private:
    void reportLocation(const GnssLocation&);
    void reportSvStatus(const std::vector<IGnssCallback::GnssSvInfo>& svInfoList) const;
    void reportGnssStatusValue(const IGnssCallback::GnssStatusValue gnssStatusValue) const;
    void reportNmea() const;
    std::vector<IGnssCallback::GnssSvInfo> filterBlocklistedSatellites(
            std::vector<IGnssCallback::GnssSvInfo> gnssSvInfoList) const;
    std::unique_ptr<GnssLocation> getLocationFromHW();

    static std::shared_ptr<IGnssCallback> sGnssCallback;

    std::atomic<long> mMinIntervalMs;
    std::atomic<long> mGnssMeasurementIntervalMs;
    std::atomic<bool> mIsActive;
    std::atomic<bool> mIsInitialized;
    std::atomic<bool> mIsSvStatusActive;
    std::atomic<bool> mIsNmeaActive;
    std::atomic<bool> mFirstFixReceived;
    std::atomic<bool> mGnssMeasurementEnabled;
    std::atomic<int> mReportedLocationCount;
    std::shared_ptr<GnssLocation> mLastLocation;
    std::thread mThread;
    ::android::hardware::gnss::common::ThreadBlocker mThreadBlocker;

    mutable std::mutex mMutex;
};

}  // namespace aidl::android::hardware::gnss
