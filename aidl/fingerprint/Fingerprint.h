/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>

#include "LegacyHAL.h"
#include "LockoutTracker.h"
#include "Session.h"

using ::aidl::android::hardware::biometrics::fingerprint::ISession;
using ::aidl::android::hardware::biometrics::fingerprint::ISessionCallback;
using ::aidl::android::hardware::biometrics::fingerprint::SensorProps;

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

class Fingerprint : public BnFingerprint {
public:
    Fingerprint();
    ndk::ScopedAStatus getSensorProps(std::vector<SensorProps>* _aidl_return) override;
    ndk::ScopedAStatus createSession(int32_t sensorId, int32_t userId,
                                     const std::shared_ptr<ISessionCallback>& cb,
                                     std::shared_ptr<ISession>* out) override;

private:
    static void notify(
        const fingerprint_msg_t* msg); /* Static callback for legacy HAL implementation */
    void handleEvent(int eventCode);

    LegacyHAL mHal;
    LockoutTracker mLockoutTracker;
    FingerprintSensorType mSensorType;
    int mMaxEnrollmentsPerUser;
    bool mSupportsGestures;
    int uinputFd;

    std::shared_ptr<Session> mSession;
};

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
