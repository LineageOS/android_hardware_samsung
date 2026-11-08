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
#include <utils/SystemClock.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include "AGnss.h"
#include "AGnssRil.h"
#include "GnssAntennaInfo.h"
#include "GnssAssistanceInterface.h"
#include "GnssBackend.h"
#include "GnssBatching.h"
#include "GnssConfiguration.h"
#include "GnssDebug.h"
#include "GnssGeofence.h"
#include "GnssNavigationMessageInterface.h"
#include "GnssProtocol.h"
#include "GnssPsds.h"
#include "GnssVisibilityControl.h"
#include "MeasurementCorrectionsInterface.h"

namespace aidl::android::hardware::gnss {

using ndk::ScopedAStatus;

Gnss::Gnss() {
    auto& backend = GnssBackend::get();
    backend.setHandler(0x650a, [this](const auto& p) { onLocation(p); });
    backend.setHandler(0x710a, [this](const auto& p) { onSvStatus(p); });
    backend.setHandler(0x670a, [this](const auto& p) { onNmea(p); });
    backend.setHandler(0x6e0a, [this](const auto&) {
        GnssBackend::get().postCallback([this] {
            if (auto callback = getCallback()) callback->gnssRequestTimeCb();
        });
    });
    backend.setHandler(0x760a, [this](const auto& p) {
        if (p.empty()) return;
        GnssBackend::get().postCallback([this, forDbh = p[0]] {
            if (auto callback = getCallback())
                callback->gnssRequestLocationCb(forDbh == 0, forDbh != 0);
        });
    });
    backend.setHandler(0x01c9, [](const auto&) {
        std::vector<uint8_t> version(256);
        const std::string name = "SPOTNAV_4.11.1_25_E56_R14";
        std::copy(name.begin(), name.end(), version.begin());
        GnssBackend::get().send(0x02c9, version);
    });
    backend.setConnectedHandler([this] {
        std::lock_guard lock(mMutex);
        mLastNmeaTime = 0;
        if (!mEverConnected) {
            mEverConnected = true;
            return;
        }
        GnssBackend::get().send(0x6504, {static_cast<uint8_t>(mCallback != nullptr)});
        if (mPositionModeSet) sendPositionMode();
        if (mIsActive) GnssBackend::get().send(0x6604, protocol::makeStartRequest());
    });
    backend.start();
    backend.send(0x6504, {1});
}

Gnss::~Gnss() {
    close();
    auto& backend = GnssBackend::get();
    backend.stop();
    for (auto id : {0x650a, 0x710a, 0x670a, 0x6e0a, 0x760a, 0x01c9})
        backend.setHandler(id, nullptr);
    backend.setConnectedHandler(nullptr);
}

std::shared_ptr<IGnssCallback> Gnss::getCallback() const {
    std::lock_guard lock(mMutex);
    return mCallback;
}

ScopedAStatus Gnss::setCallback(const std::shared_ptr<IGnssCallback>& callback) {
    ALOGD("setCallback");
    if (!callback) return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    {
        std::lock_guard lock(mMutex);
        mCallback = callback;
    }
    callback->gnssSetCapabilitiesCb(IGnssCallback::CAPABILITY_SCHEDULING |
                                    IGnssCallback::CAPABILITY_ON_DEMAND_TIME);
    IGnssCallback::GnssSystemInfo info;
    info.yearOfHw = readConfigInt("GlueLayer_YearOfHW", 0);
    info.name = "S.LSI,SPOTNAV_4.11.1_25_E56_R14";
    struct Signal {
        int mask;
        GnssConstellationType constellation;
        double frequency;
        const char* code;
    };
    const Signal signals[] = {
            {1, GnssConstellationType::GPS, 1575420000, "C"},
            {0x80, GnssConstellationType::GPS, 1176450000, "X"},
            {0x10, GnssConstellationType::SBAS, 1575420000, "C"},
            {2, GnssConstellationType::GLONASS, 1602000000, "C"},
            {4, GnssConstellationType::GALILEO, 1575420000, "C"},
            {0x100, GnssConstellationType::GALILEO, 1176450000, "X"},
            {8, GnssConstellationType::BEIDOU, 1561098000, "I"},
            {0x20, GnssConstellationType::QZSS, 1575420000, "C"},
            {0x40, GnssConstellationType::IRNSS, 1176450000, "X"},
    };
    std::vector<GnssSignalType> types;
    const uint16_t constraint = readConfigInt("Chip_Configuration_GNSSConstConstraintDef", 0x3f);
    for (const auto& signal : signals) {
        if (constraint & signal.mask) {
            GnssSignalType type;
            type.constellation = signal.constellation;
            type.carrierFrequencyHz = signal.frequency;
            type.codeType = signal.code;
            types.push_back(type);
        }
    }
    callback->gnssSetSignalTypeCapabilitiesCb(types);
    callback->gnssSetSystemInfoCb(info);
    GnssBackend::get().send(0x6504, {1});
    return ScopedAStatus::ok();
}

void Gnss::onLocation(const std::vector<uint8_t>& payload) {
    Reader reader(payload);
    reader.u16();
    GnssLocation location;
    location.latitudeDegrees = reader.f64();
    location.longitudeDegrees = reader.f64();
    location.altitudeMeters = reader.f64();
    location.speedMetersPerSec = reader.f32();
    location.bearingDegrees = reader.f32();
    location.horizontalAccuracyMeters = reader.f32();
    uint64_t low = reader.u32();
    uint64_t high = reader.u32();
    location.timestampMillis = low | (high << 32);
    reader.u32();
    reader.u32();
    reader.u16();
    reader.u32();
    location.verticalAccuracyMeters = reader.f32();
    location.speedAccuracyMetersPerSecond = reader.f32();
    location.bearingAccuracyDegrees = reader.f32();
    if (!reader.ok()) return;
    location.gnssLocationFlags = 0x77 | (location.speedMetersPerSec != 0 ? 0x88 : 0);
    location.elapsedRealtime.flags = static_cast<int>(ElapsedRealtime::HAS_TIMESTAMP_NS) |
                                     static_cast<int>(ElapsedRealtime::HAS_TIME_UNCERTAINTY_NS);
    location.elapsedRealtime.timestampNs = ::android::elapsedRealtimeNano();
    location.elapsedRealtime.timeUncertaintyNs = 1000;
    {
        std::lock_guard lock(mMutex);
        if (!mIsActive) return;
        mLastLocation = std::make_shared<GnssLocation>(location);
    }
    GnssBackend::get().postCallback([this, location] {
        if (auto callback = getCallback()) {
            callback->gnssLocationCb(location);
        }
    });
}

void Gnss::onSvStatus(const std::vector<uint8_t>& payload) {
    Reader reader(payload);
    const uint8_t type = reader.u8();
    if (type != 10 && type != 11) return;
    reader.skip(3 + 8 * sizeof(float));
    const auto count = reader.u8();
    if (count > 64) return;
    std::vector<IGnssCallback::GnssSvInfo> satellites;
    const auto loss = readConfigInt("RfPathLossDb_Ap", 5);
    for (int i = 0; i < count; ++i) {
        IGnssCallback::GnssSvInfo satellite;
        satellite.svid = reader.u8();
        const auto constellation = reader.u8();
        satellite.constellation = constellation < 8
                                          ? static_cast<GnssConstellationType>(constellation)
                                          : GnssConstellationType::UNKNOWN;
        reader.u8();
        satellite.svFlag = reader.u8();
        reader.u8();
        const float cn0 = reader.f32();
        satellite.cN0Dbhz = std::min(63.0f, cn0 + loss);
        satellite.basebandCN0DbHz = std::min(63.0f, cn0);
        satellite.elevationDegrees = reader.f32();
        satellite.azimuthDegrees = reader.f32();
        const float frequency = reader.f32();
        reader.f64();
        satellite.carrierFrequencyHz = std::isnan(frequency)  ? 0
                                       : frequency >= 0x1p63f ? std::numeric_limits<int64_t>::max()
                                       : frequency < -0x1p63f ? std::numeric_limits<int64_t>::min()
                                                              : static_cast<int64_t>(frequency);
        if (!(satellite.svFlag & 8)) {
            switch (satellite.constellation) {
                case GnssConstellationType::GPS:
                case GnssConstellationType::SBAS:
                case GnssConstellationType::QZSS:
                case GnssConstellationType::GALILEO:
                    satellite.carrierFrequencyHz = 1575420000;
                    break;
                case GnssConstellationType::GLONASS:
                    satellite.carrierFrequencyHz = 1602000000;
                    break;
                case GnssConstellationType::BEIDOU:
                    satellite.carrierFrequencyHz = 1561098000;
                    break;
                case GnssConstellationType::IRNSS:
                    satellite.carrierFrequencyHz = 1176450000;
                    break;
                default:
                    break;
            }
            if (satellite.constellation != GnssConstellationType::UNKNOWN) satellite.svFlag |= 8;
        }
        if (cn0 > 0) satellites.push_back(satellite);
    }
    if (!reader.ok()) return;
    GnssBackend::get().postCallback([this, satellites = std::move(satellites)] {
        std::shared_ptr<IGnssCallback> callback;
        {
            std::lock_guard lock(mMutex);
            if (mIsSvStatusActive) callback = mCallback;
        }
        if (callback) callback->gnssSvStatusCb(satellites);
    });
}

void Gnss::onNmea(const std::vector<uint8_t>& payload) {
    Reader reader(payload);
    const uint32_t time = reader.u32();
    const uint64_t low = reader.u32();
    const uint64_t high = reader.u32();
    const size_t length = reader.u8();
    auto nmea = reader.string(256);
    if (!reader.ok()) return;
    if (nmea.size() > length) nmea.resize(length);
    {
        std::lock_guard lock(mMutex);
        if (!mIsNmeaActive) return;
        if (time + 3 < mLastNmeaTime) return;
        mLastNmeaTime = time;
    }
    GnssBackend::get().postCallback([this, timestamp = low | (high << 32), nmea = std::move(nmea)] {
        if (auto callback = getCallback()) callback->gnssNmeaCb(timestamp, nmea);
    });
}

ScopedAStatus Gnss::start() {
    ALOGD("start");
    {
        std::lock_guard lock(mMutex);
        mIsActive = true;
        GnssBackend::get().send(0x6604, protocol::makeStartRequest());
    }
    if (auto callback = getCallback()) {
        callback->gnssStatusCb(IGnssCallback::GnssStatusValue::ENGINE_ON);
        callback->gnssStatusCb(IGnssCallback::GnssStatusValue::SESSION_BEGIN);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::stop() {
    ALOGD("stop");
    {
        std::lock_guard lock(mMutex);
        if (!mIsActive) return ScopedAStatus::ok();
        mIsActive = false;
        GnssBackend::get().send(0x6704, {0});
    }
    if (auto callback = getCallback()) {
        callback->gnssStatusCb(IGnssCallback::GnssStatusValue::SESSION_END);
        callback->gnssStatusCb(IGnssCallback::GnssStatusValue::ENGINE_OFF);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::close() {
    ALOGD("close");
    stop();
    std::lock_guard lock(mMutex);
    mCallback = nullptr;
    mIsSvStatusActive = false;
    mIsNmeaActive = false;
    GnssBackend::get().send(0x6804, {0});
    return ScopedAStatus::ok();
}

void Gnss::reportSvStatus() const {}

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
    Writer request;
    request.u32(static_cast<uint64_t>(timeMs) >> 32)
            .u32(timeMs)
            .u32(static_cast<uint64_t>(timeReferenceMs) >> 32)
            .u32(timeReferenceMs)
            .u32(uncertaintyMs);
    GnssBackend::get().send(0x6e04, request.data());
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
    if (!(location.gnssLocationFlags & GnssLocation::HAS_LAT_LONG)) return ScopedAStatus::ok();
    std::lock_guard lock(mMutex);
    const int64_t seconds = ::android::elapsedRealtimeNano() / 1000000000;
    const uint32_t earliest = mLastInjectionSeconds +
                              static_cast<uint32_t>(readConfigInt("loc_injection_interval", 1));
    if (seconds < earliest) return ScopedAStatus::ok();
    Writer request;
    request.f64(location.latitudeDegrees)
            .f64(location.longitudeDegrees)
            .f32(location.gnssLocationFlags & GnssLocation::HAS_HORIZONTAL_ACCURACY
                         ? location.horizontalAccuracyMeters
                         : 150000);
    GnssBackend::get().send(0x7b04, request.data());
    mLastInjectionSeconds = seconds;
    return ScopedAStatus::ok();
}

ScopedAStatus Gnss::injectBestLocation(const GnssLocation& location) {
    ALOGD("injectBestLocation. lat:%lf, lng:%lf, acc:%f", location.latitudeDegrees,
          location.longitudeDegrees, location.horizontalAccuracyMeters);
    return injectLocation(location);
}

ScopedAStatus Gnss::deleteAidingData(GnssAidingData aidingDataFlags) {
    ALOGD("deleteAidingData. flags:%d", (int)aidingDataFlags);
    Writer request;
    std::lock_guard lock(mMutex);
    request.u16(static_cast<uint16_t>(aidingDataFlags));
    GnssBackend::get().send(0x6904, request.data());
    return ScopedAStatus::ok();
}

void Gnss::sendPositionMode() {
    Writer request;
    request.u32(static_cast<uint32_t>(mPositionMode.mode))
            .u32(static_cast<uint32_t>(mPositionMode.recurrence))
            .u32(mPositionMode.minIntervalMs)
            .u32(mPositionMode.preferredAccuracyMeters)
            .u32(mPositionMode.preferredTimeMs)
            .u8(mPositionMode.lowPowerMode);
    GnssBackend::get().send(0x6a04, request.data());
}

ScopedAStatus Gnss::setPositionMode(const PositionModeOptions& options) {
    ALOGD("setPositionMode. minIntervalMs:%d, lowPowerMode:%d", options.minIntervalMs,
          (int)options.lowPowerMode);
    std::lock_guard lock(mMutex);
    mPositionMode = options;
    mPositionModeSet = true;
    sendPositionMode();
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
        mGnssMeasurementInterface->setGnssInterface(ref<Gnss>());
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

void Gnss::reportSvStatus() const {}

void Gnss::setGnssMeasurementEnabled(const bool enabled) {
    mGnssMeasurementEnabled = enabled;
}

void Gnss::setGnssMeasurementInterval(const long intervalMs) {
    mGnssMeasurementIntervalMs = intervalMs;
}

std::shared_ptr<GnssLocation> Gnss::getLastLocation() const {
    std::lock_guard lock(mMutex);
    return mLastLocation;
}

}  // namespace aidl::android::hardware::gnss
