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

#define LOG_TAG "GnssGeofenceAidl"

#include "GnssGeofence.h"
#include <aidl/android/hardware/gnss/BnGnssGeofence.h>
#include <log/log.h>

namespace aidl::android::hardware::gnss {

std::shared_ptr<IGnssGeofenceCallback> GnssGeofence::sCallback = nullptr;

ndk::ScopedAStatus GnssGeofence::setCallback(
        const std::shared_ptr<IGnssGeofenceCallback>& callback) {
    ALOGD("setCallback");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssGeofence::addGeofence(int geofenceId, double latitudeDegrees,
                                             double longitudeDegrees, double radiusMeters,
                                             int lastTransition, int monitorTransitions,
                                             int notificationResponsivenessMs, int unknownTimerMs) {
    ALOGD("addGeofence. geofenceId=%d, lat=%lf, lng=%lf, rad=%lf, lastTransition=%d, "
          "monitorTransitions=%d, notificationResponsivenessMs=%d, unknownTimerMs=%d",
          geofenceId, latitudeDegrees, longitudeDegrees, radiusMeters, lastTransition,
          monitorTransitions, notificationResponsivenessMs, unknownTimerMs);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssGeofence::pauseGeofence(int geofenceId) {
    ALOGD("pauseGeofence. id=%d", geofenceId);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssGeofence::resumeGeofence(int geofenceId, int monitorTransitions) {
    ALOGD("resumeGeofence. id=%d, monitorTransitions=%d", geofenceId, monitorTransitions);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssGeofence::removeGeofence(int geofenceId) {
    ALOGD("removeGeofence. id=%d", geofenceId);
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::gnss
