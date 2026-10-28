/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WakeLock.h"

#include <hardware_legacy/power.h>

#include <thread>

namespace aidl::android::hardware::radio::samsung {

namespace {

constexpr char kWakeLockName[] = "radio-interface";
constexpr auto kTimeout = std::chrono::milliseconds(200);

}  // namespace

WakeLock& WakeLock::get() {
    static WakeLock instance;
    return instance;
}

WakeLock::WakeLock() {
    std::thread(&WakeLock::timeoutLoop, this).detach();
}

void WakeLock::acquire() {
    std::lock_guard lock(mMutex);
    if (mCount++ == 0) {
        acquire_wake_lock(PARTIAL_WAKE_LOCK, kWakeLockName);
    }
    mDeadline = std::chrono::steady_clock::now() + kTimeout;
    mCondition.notify_one();
}

void WakeLock::release() {
    std::lock_guard lock(mMutex);
    if (mCount == 0) {
        return;
    }
    if (--mCount == 0) {
        releaseAllLocked();
    }
}

void WakeLock::releaseAllLocked() {
    mCount = 0;
    mDeadline.reset();
    release_wake_lock(kWakeLockName);
}

void WakeLock::timeoutLoop() {
    std::unique_lock lock(mMutex);
    while (true) {
        if (!mDeadline.has_value()) {
            mCondition.wait(lock);
            continue;
        }
        if (mCondition.wait_until(lock, *mDeadline) == std::cv_status::timeout &&
            mDeadline.has_value() && std::chrono::steady_clock::now() >= *mDeadline) {
            releaseAllLocked();
        }
    }
}

}  // namespace aidl::android::hardware::radio::samsung
