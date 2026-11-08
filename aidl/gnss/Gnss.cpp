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

#define LOG_TAG "GnssAidl"

#include "Gnss.h"
#include <inttypes.h>
#include <log/log.h>
#include <utils/Timers.h>
#include "AGnss.h"
#include "AGnssRil.h"
#include "DeviceFileReader.h"
#include "FixLocationParser.h"
#include "GnssAntennaInfo.h"
#include "GnssAssistanceInterface.h"
#include "GnssBatching.h"
#include "GnssConfiguration.h"
#include "GnssDebug.h"
#include "GnssGeofence.h"
#include "GnssNavigationMessageInterface.h"
#include "GnssPsds.h"
#include "GnssVisibilityControl.h"
#include "MeasurementCorrectionsInterface.h"
#include "Utils.h"

namespace aidl::android::hardware::gnss {
using ::android::hardware::gnss::common::Utils;

using ndk::ScopedAStatus;
using GnssSvInfo = IGnssCallback::GnssSvInfo;

constexpr int TTFF_MILLIS = 2200;

std::shared_ptr<IGnssCallback> Gnss::sGnssCallback = nullptr;

Gnss::Gnss() : mMinIntervalMs(1000), mFirstFixReceived(false) {}

ScopedAStatus Gnss::setCallback(const std::shared_ptr<IGnssCallback>& callback) {
    ALOGD("setCallback");
    if (callback == nullptr) {
        ALOGE("%s: Null callback ignored", __func__);
        return ScopedAStatus::fromExceptionCode(STATUS_INVALID_OPERATION);
    }
    {
        std::unique_lock<std::mutex> lock(mMutex);
        sGnssCallback = callback;
    }

    int capabilities =
            (int)(IGnssCallback::CAPABILITY_MEASUREMENTS | IGnssCallback::CAPABILITY_SCHEDULING |
                  IGnssCallback::CAPABILITY_SATELLITE_BLOCKLIST |
                  IGnssCallback::CAPABILITY_SATELLITE_PVT |
                  IGnssCallback::CAPABILITY_CORRELATION_VECTOR |
                  IGnssCallback::CAPABILITY_ANTENNA_INFO |
                  IGnssCallback::CAPABILITY_ACCUMULATED_DELTA_RANGE);
    auto status = callback->gnssSetCapabilitiesCb(capabilities);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback.gnssSetCapabilitiesCb", __func__);
    }

    IGnssCallback::GnssSystemInfo systemInfo = {
            .yearOfHw = 2022,
            .name = "Google, Cuttlefish, AIDL v3",
    };
    status = callback->gnssSetSystemInfoCb(systemInfo);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback.gnssSetSystemInfoCb", __func__);
    }
    GnssSignalType signalType1 = {
            .constellation = GnssConstellationType::GPS,
            .carrierFrequencyHz = 1.57542e+09,
            .codeType = GnssSignalType::CODE_TYPE_C,
    };
    GnssSignalType signalType2 = {
            .constellation = GnssConstellationType::GLONASS,
            .carrierFrequencyHz = 1.5980625e+09,
            .codeType = GnssSignalType::CODE_TYPE_C,
    };
    status = callback->gnssSetSignalTypeCapabilitiesCb(
            std::vector<GnssSignalType>({signalType1, signalType2}));
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback.gnssSetSignalTypeCapabilitiesCb", __func__);
    }

    // In case when the setCallback() and close() calls are not balanced
    mIsInitialized = false;
    mIsActive = false;
    mThreadBlocker.notify();
    if (mThread.joinable()) {
        mThread.join();
    }

    mIsInitialized = true;
    mThreadBlocker.reset();
    mThread = std::thread([this]() {
        while (mIsInitialized) {
            if (mIsActive) {
                if (mReportedLocationCount == 0) {
                    if (!mGnssMeasurementEnabled || mMinIntervalMs <= mGnssMeasurementIntervalMs) {
                        this->reportSvStatus();
                    }
                    if (!mFirstFixReceived) {
                        // Simulate the code start TTFF
                        std::this_thread::sleep_for(std::chrono::milliseconds(TTFF_MILLIS));
                        mFirstFixReceived = true;
                    }
                }
                if (!mGnssMeasurementEnabled || mMinIntervalMs <= mGnssMeasurementIntervalMs) {
                    this->reportSvStatus();
                }
                this->reportNmea();

                auto currentLocation = getLocationFromHW();
                mGnssPowerIndication->notePowerConsumption();
                if (currentLocation != nullptr) {
                    this->reportLocation(*currentLocation);
                } else {
                    const auto location = Utils::getMockLocation();
                    this->reportLocation(location);
                }
                mReportedLocationCount += 1;
                mThreadBlocker.wait_for(std::chrono::milliseconds(mMinIntervalMs));
            } else {
                // Wait indefinitely until start() or close() is called
                mThreadBlocker.wait();
            }
        }
    });

    return ScopedAStatus::ok();
}

std::unique_ptr<GnssLocation> Gnss::getLocationFromHW() {
    if (!::android::hardware::gnss::common::ReplayUtils::hasFixedLocationDeviceFile()) {
        return nullptr;
    }
    std::string inputStr =
            ::android::hardware::gnss::common::DeviceFileReader::Instance().getLocationData();
    return ::android::hardware::gnss::common::FixLocationParser::getLocationFromInputStr(inputStr);
}

ScopedAStatus Gnss::start() {
    ALOGD("start");
    if (mIsActive) {
        ALOGW("Gnss has started. Restarting...");
        stop();
    }
    mIsActive = true;
    // notify measurement engine to update measurement interval
    mGnssMeasurementInterface->setLocationEnabled(true);
    this->reportGnssStatusValue(IGnssCallback::GnssStatusValue::SESSION_BEGIN);
    mThreadBlocker.notify();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stop() {
    ALOGD("stop");
    mIsActive = false;
    mGnssMeasurementInterface->setLocationEnabled(false);
    this->reportGnssStatusValue(IGnssCallback::GnssStatusValue::SESSION_END);

    int reportedLocationCount = mReportedLocationCount;
    ALOGD("reportedLocationCount: %d", reportedLocationCount);
    mReportedLocationCount = 0;
    mThreadBlocker.notify();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::close() {
    ALOGD("close");
    {
        std::unique_lock<std::mutex> lock(mMutex);
        sGnssCallback = nullptr;
    }
    mIsInitialized = false;
    mThreadBlocker.notify();
    if (mThread.joinable()) {
        mThread.join();
    }
    return ScopedAStatus::ok();
}

void Gnss::reportLocation(const GnssLocation& location) {
    std::shared_ptr<IGnssCallback> callback;
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (sGnssCallback == nullptr) {
            ALOGE("%s: GnssCallback is null.", __func__);
            return;
        }
        callback = sGnssCallback;
    }
    mLastLocation = std::make_shared<GnssLocation>(location);
    auto status = callback->gnssLocationCb(location);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke gnssLocationCb", __func__);
    }
    return;
}

void Gnss::reportSvStatus() const {
    if (mIsSvStatusActive) {
        auto svStatus = filterBlocklistedSatellites(Utils::getMockSvInfoList());
        reportSvStatus(svStatus);
    }
}

void Gnss::reportSvStatus(const std::vector<GnssSvInfo>& svInfoList) const {
    std::shared_ptr<IGnssCallback> callback;
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (sGnssCallback == nullptr) {
            ALOGE("%s: GnssCallback is null.", __func__);
            return;
        }
        callback = sGnssCallback;
    }
    auto status = callback->gnssSvStatusCb(svInfoList);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke callback", __func__);
    }
}

std::vector<GnssSvInfo> Gnss::filterBlocklistedSatellites(
        std::vector<GnssSvInfo> gnssSvInfoList) const {
    for (uint32_t i = 0; i < gnssSvInfoList.size(); i++) {
        if (mGnssConfiguration->isBlocklisted(gnssSvInfoList[i])) {
            gnssSvInfoList[i].svFlag &= ~(uint32_t)IGnssCallback::GnssSvFlags::USED_IN_FIX;
        }
    }
    return gnssSvInfoList;
}

void Gnss::reportGnssStatusValue(const IGnssCallback::GnssStatusValue gnssStatusValue) const {
    std::shared_ptr<IGnssCallback> callback;
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (sGnssCallback == nullptr) {
            ALOGE("%s: GnssCallback is null.", __func__);
            return;
        }
        callback = sGnssCallback;
    }
    auto status = callback->gnssStatusCb(gnssStatusValue);
    if (!status.isOk()) {
        ALOGE("%s: Unable to invoke gnssStatusCb", __func__);
    }
}

void Gnss::reportNmea() const {
    if (mIsNmeaActive) {
        std::shared_ptr<IGnssCallback> callback;
        {
            std::unique_lock<std::mutex> lock(mMutex);
            if (sGnssCallback == nullptr) {
                ALOGE("%s: GnssCallback is null.", __func__);
                return;
            }
            callback = sGnssCallback;
        }
        nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
        auto status = callback->gnssNmeaCb(now, "$TEST,0,1,2,3,4,5");
        if (!status.isOk()) {
            ALOGE("%s: Unable to invoke callback", __func__);
        }
    }
}

ScopedAStatus Gnss::startSvStatus() {
    ALOGD("startSvStatus");
    mIsSvStatusActive = true;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stopSvStatus() {
    ALOGD("stopSvStatus");
    mIsSvStatusActive = false;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::startNmea() {
    ALOGD("startNmea");
    mIsNmeaActive = true;
    return ScopedAStatus::ok();
}
ScopedAStatus Gnss::stopNmea() {
    ALOGD("stopNmea");
    mIsNmeaActive = false;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionAGnss(std::shared_ptr<IAGnss>* iAGnss) {
    ALOGD("Gnss::getExtensionAGnss");
    *iAGnss = SharedRefBase::make<AGnss>();
    return ndk::ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectTime(int64_t timeMs, int64_t timeReferenceMs, int uncertaintyMs) {
    ALOGD("injectTime. timeMs:%" PRId64 ", timeReferenceMs:%" PRId64 ", uncertaintyMs:%d", timeMs,
          timeReferenceMs, uncertaintyMs);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionAGnssRil(std::shared_ptr<IAGnssRil>* iAGnssRil) {
    ALOGD("Gnss::getExtensionAGnssRil");
    *iAGnssRil = SharedRefBase::make<AGnssRil>();
    return ndk::ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectLocation(const GnssLocation& location) {
    ALOGD("injectLocation. lat:%lf, lng:%lf, acc:%f", location.latitudeDegrees,
          location.longitudeDegrees, location.horizontalAccuracyMeters);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectBestLocation(const GnssLocation& location) {
    ALOGD("injectBestLocation. lat:%lf, lng:%lf, acc:%f", location.latitudeDegrees,
          location.longitudeDegrees, location.horizontalAccuracyMeters);
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::deleteAidingData(GnssAidingData aidingDataFlags) {
    ALOGD("deleteAidingData. flags:%d", (int)aidingDataFlags);
    mFirstFixReceived = false;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::setPositionMode(const PositionModeOptions& options) {
    ALOGD("setPositionMode. minIntervalMs:%d, lowPowerMode:%d", options.minIntervalMs,
          (int)options.lowPowerMode);
    if (std::max(1000, options.minIntervalMs) != mMinIntervalMs) {
        mMinIntervalMs = std::max(1000, options.minIntervalMs);
        mGnssMeasurementInterface->setLocationInterval(mMinIntervalMs);
        if (mIsActive) {
            stop();
            start();
        }
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionPsds(std::shared_ptr<IGnssPsds>* iGnssPsds) {
    ALOGD("getExtensionPsds");
    *iGnssPsds = SharedRefBase::make<GnssPsds>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssConfiguration(
        std::shared_ptr<IGnssConfiguration>* iGnssConfiguration) {
    ALOGD("getExtensionGnssConfiguration");
    if (mGnssConfiguration == nullptr) {
        mGnssConfiguration = SharedRefBase::make<GnssConfiguration>();
    }
    *iGnssConfiguration = mGnssConfiguration;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssPowerIndication(
        std::shared_ptr<IGnssPowerIndication>* iGnssPowerIndication) {
    ALOGD("getExtensionGnssPowerIndication");
    if (mGnssPowerIndication == nullptr) {
        mGnssPowerIndication = SharedRefBase::make<GnssPowerIndication>();
    }

    *iGnssPowerIndication = mGnssPowerIndication;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssMeasurement(
        std::shared_ptr<IGnssMeasurementInterface>* iGnssMeasurement) {
    ALOGD("getExtensionGnssMeasurement");
    if (mGnssMeasurementInterface == nullptr) {
        mGnssMeasurementInterface = SharedRefBase::make<GnssMeasurementInterface>();
        mGnssMeasurementInterface->setGnssInterface(static_cast<std::shared_ptr<Gnss>>(this));
    }
    *iGnssMeasurement = mGnssMeasurementInterface;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssBatching(std::shared_ptr<IGnssBatching>* iGnssBatching) {
    ALOGD("getExtensionGnssBatching");

    *iGnssBatching = SharedRefBase::make<GnssBatching>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssGeofence(std::shared_ptr<IGnssGeofence>* iGnssGeofence) {
    ALOGD("getExtensionGnssGeofence");

    *iGnssGeofence = SharedRefBase::make<GnssGeofence>();
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::getExtensionGnssNavigationMessage(
        std::shared_ptr<IGnssNavigationMessageInterface>* iGnssNavigationMessage) {
    ALOGD("getExtensionGnssNavigationMessage");

    *iGnssNavigationMessage = SharedRefBase::make<GnssNavigationMessageInterface>();
    return ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionGnssDebug(std::shared_ptr<IGnssDebug>* iGnssDebug) {
    ALOGD("Gnss::getExtensionGnssDebug");
    *iGnssDebug = SharedRefBase::make<GnssDebug>();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionGnssVisibilityControl(
        std::shared_ptr<visibility_control::IGnssVisibilityControl>* iGnssVisibilityControl) {
    ALOGD("Gnss::getExtensionGnssVisibilityControl");

    *iGnssVisibilityControl = SharedRefBase::make<visibility_control::GnssVisibilityControl>();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionGnssAntennaInfo(
        std::shared_ptr<IGnssAntennaInfo>* iGnssAntennaInfo) {
    ALOGD("Gnss::getExtensionGnssAntennaInfo");

    *iGnssAntennaInfo = SharedRefBase::make<GnssAntennaInfo>();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionMeasurementCorrections(
        std::shared_ptr<measurement_corrections::IMeasurementCorrectionsInterface>*
                iMeasurementCorrections) {
    ALOGD("Gnss::getExtensionMeasurementCorrections");

    *iMeasurementCorrections =
            SharedRefBase::make<measurement_corrections::MeasurementCorrectionsInterface>();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Gnss::getExtensionGnssAssistanceInterface(
        std::shared_ptr<gnss_assistance::IGnssAssistanceInterface>* iGnssAssistanceInterface) {
    ALOGD("Gnss::getExtensionGnssAssistanceInterface");

    *iGnssAssistanceInterface = SharedRefBase::make<gnss_assistance::GnssAssistanceInterface>();
    return ndk::ScopedAStatus::ok();
}

void Gnss::setGnssMeasurementEnabled(const bool enabled) {
    mGnssMeasurementEnabled = enabled;
}

void Gnss::setGnssMeasurementInterval(const long intervalMs) {
    mGnssMeasurementIntervalMs = intervalMs;
}

std::shared_ptr<GnssLocation> Gnss::getLastLocation() const {
    return mLastLocation;
}

}  // namespace aidl::android::hardware::gnss
