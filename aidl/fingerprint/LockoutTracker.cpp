/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Fingerprint.h"
#include "LockoutTracker.h"

#include <util/Util.h>

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

void LockoutTracker::reset(bool clearAttemptCounter) {
    if (clearAttemptCounter)
        mFailedCount = 0;
    mLockoutTimedStart = 0;
    mCurrentMode = LockoutMode::NONE;
}

void LockoutTracker::addFailedAttempt() {
    mFailedCount++;

    if (mFailedCount >= LOCKOUT_PERMANENT_THRESHOLD)
        mCurrentMode = LockoutMode::PERMANENT;
    else if (mFailedCount >= LOCKOUT_TIMED_THRESHOLD) {
        mCurrentMode = LockoutMode::TIMED;
        mLockoutTimedStart = Util::getSystemNanoTime();
    }
}

LockoutMode LockoutTracker::getMode() {
    if (mCurrentMode == LockoutMode::TIMED) {
        if (Util::hasElapsed(mLockoutTimedStart, LOCKOUT_TIMED_DURATION)) {
            mCurrentMode = LockoutMode::NONE;
            mLockoutTimedStart = 0;
        }
    }

    return mCurrentMode;
}

int64_t LockoutTracker::getLockoutTimeLeft() {
    int64_t res = 0;

    if (mLockoutTimedStart > 0) {
        auto now = Util::getSystemNanoTime();
        auto elapsed = (now - mLockoutTimedStart) / 1000000LL;
        res = LOCKOUT_TIMED_DURATION - elapsed;
    }

    return res;
}

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
