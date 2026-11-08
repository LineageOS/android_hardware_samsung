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

#define LOG_TAG "GnssDebugAidl"

#include "GnssDebug.h"
#include <log/log.h>
#include <utils/SystemClock.h>
#include "Constants.h"
#include "Gnss.h"
#include "MockLocation.h"

namespace aidl::android::hardware::gnss {

using ::android::hardware::gnss::common::kMockTimestamp;

ndk::ScopedAStatus GnssDebug::getDebugData(DebugData* debugData) {
    ALOGD("GnssDebug::getDebugData");

    PositionDebug positionDebug = {.valid = true,
                                   .latitudeDegrees = 37.4219999,
                                   .longitudeDegrees = -122.0840575,
                                   .altitudeMeters = 1.60062531,
                                   .speedMetersPerSec = 0,
                                   .bearingDegrees = 0,
                                   .horizontalAccuracyMeters = 5,
                                   .verticalAccuracyMeters = 5,
                                   .speedAccuracyMetersPerSecond = 1,
                                   .bearingAccuracyDegrees = 90,
                                   .ageSeconds = 0.99};
    TimeDebug timeDebug = {.timeEstimateMs = static_cast<int64_t>(
                                   kMockTimestamp + ::android::elapsedRealtimeNano() / 1e6),
                           .timeUncertaintyNs = 1000,
                           .frequencyUncertaintyNsPerSec = 800};
    SatelliteData satelliteData1 = {
            .svid = 3,
            .constellation = GnssConstellationType::GPS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData2 = {
            .svid = 5,
            .constellation = GnssConstellationType::GPS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData3 = {
            .svid = 17,
            .constellation = GnssConstellationType::GPS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData4 = {
            .svid = 26,
            .constellation = GnssConstellationType::GPS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData5 = {
            .svid = 5,
            .constellation = GnssConstellationType::GLONASS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData6 = {
            .svid = 17,
            .constellation = GnssConstellationType::GLONASS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData7 = {
            .svid = 18,
            .constellation = GnssConstellationType::GLONASS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData8 = {
            .svid = 10,
            .constellation = GnssConstellationType::GLONASS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData9 = {
            .svid = 3,
            .constellation = GnssConstellationType::IRNSS,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData10 = {
            .svid = 2,
            .constellation = GnssConstellationType::GALILEO,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData11 = {
            .svid = 4,
            .constellation = GnssConstellationType::GALILEO,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData12 = {
            .svid = 10,
            .constellation = GnssConstellationType::GALILEO,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    SatelliteData satelliteData13 = {
            .svid = 29,
            .constellation = GnssConstellationType::GALILEO,
            .ephemerisType = SatelliteEphemerisType::EPHEMERIS,
            .ephemerisSource = SatellitePvt::SatelliteEphemerisSource::SERVER_LONG_TERM,
            .ephemerisHealth = SatelliteEphemerisHealth::GOOD,
            .ephemerisAgeSeconds = 12,
            .serverPredictionIsAvailable = true,
            .serverPredictionAgeSeconds = 30};
    std::vector<SatelliteData> satelliteDataArrayDebug = {
            satelliteData1,  satelliteData2,  satelliteData3, satelliteData4, satelliteData5,
            satelliteData6,  satelliteData7,  satelliteData8, satelliteData9, satelliteData10,
            satelliteData11, satelliteData12, satelliteData13};
    debugData->position = positionDebug;
    debugData->time = timeDebug;
    debugData->satelliteDataArray = satelliteDataArrayDebug;

    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
