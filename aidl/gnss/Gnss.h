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

#include <aidl/android/hardware/gnss/BnGnss.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include "GnssConfiguration.h"

namespace aidl::android::hardware::gnss {

class Gnss : public BnGnss {
  public:
    Gnss();
    ~Gnss() override;
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

  private:
    std::shared_ptr<IGnssCallback> getCallback() const;
    void onLocation(const std::vector<uint8_t>& payload);
    void onSvStatus(const std::vector<uint8_t>& payload);
    void onNmea(const std::vector<uint8_t>& payload);
    void sendPositionMode();

    std::shared_ptr<IGnssCallback> mCallback;
    std::shared_ptr<GnssConfiguration> mGnssConfiguration;
    PositionModeOptions mPositionMode;
    bool mPositionModeSet = false;
    bool mEverConnected = false;
    std::atomic<bool> mIsActive{false};
    std::atomic<bool> mIsSvStatusActive{false};
    std::atomic<bool> mIsNmeaActive{false};
    uint32_t mLastNmeaTime = 0;
    uint32_t mLastInjectionSeconds = 0;
    mutable std::mutex mMutex;
};

}  // namespace aidl::android::hardware::gnss
