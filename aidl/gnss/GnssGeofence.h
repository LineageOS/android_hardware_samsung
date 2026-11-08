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

#pragma once

#include <aidl/android/hardware/gnss/BnGnssGeofence.h>

namespace aidl::android::hardware::gnss {

struct GnssGeofence : public BnGnssGeofence {
  public:
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssGeofenceCallback>& callback) override;
    ndk::ScopedAStatus addGeofence(int geofenceId, double latitudeDegrees, double longitudeDegrees,
                                   double radiusMeters, int lastTransition, int monitorTransitions,
                                   int notificationResponsivenessMs, int unknownTimerMs) override;
    ndk::ScopedAStatus pauseGeofence(int geofenceId) override;
    ndk::ScopedAStatus resumeGeofence(int geofenceId, int monitorTransitions) override;
    ndk::ScopedAStatus removeGeofence(int geofenceId) override;

  private:
    // Guarded by mMutex
    static std::shared_ptr<IGnssGeofenceCallback> sCallback;

    // Synchronization lock for sCallback
    mutable std::mutex mMutex;
};

}  // namespace aidl::android::hardware::gnss
