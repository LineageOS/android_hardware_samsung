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

#define LOG_TAG "GnssBatchingAidl"

#include "GnssBatching.h"
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <inttypes.h>
#include <log/log.h>
#include <utils/SystemClock.h>
#include "Utils.h"

namespace aidl::android::hardware::gnss {

using namespace ::android::hardware::gnss;

constexpr int BATCH_SIZE = 10;

std::shared_ptr<IGnssBatchingCallback> GnssBatching::sCallback = nullptr;

GnssBatching::GnssBatching()
    : mMinIntervalMs(1000),
      mWakeUpOnFifoFull(false),
      mBatchedLocations(std::vector<GnssLocation>()) {}
GnssBatching::~GnssBatching() {
    cleanup();
}

ndk::ScopedAStatus GnssBatching::init(const std::shared_ptr<IGnssBatchingCallback>& callback) {
    ALOGD("init");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssBatching::getBatchSize(int* size) {
    ALOGD("getBatchingSize");
    *size = BATCH_SIZE;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssBatching::start(const Options& options) {
    ALOGD("start: periodNanos=%" PRId64 ", minDistanceMeters=%f, flags=%d", options.periodNanos,
          options.minDistanceMeters, options.flags);
    if (mIsActive) {
        ALOGW("Gnss has started. Restarting...");
        stop();
    }

    // mMinIntervalMs is not smaller than 1 sec
    long periodNanos = (options.periodNanos < 1e9) ? 1e9 : options.periodNanos;
    mMinIntervalMs = periodNanos / 1e6;
    mWakeUpOnFifoFull = (options.flags & IGnssBatching::WAKEUP_ON_FIFO_FULL) ? true : false;
    mMinDistanceMeters = options.minDistanceMeters;

    mIsActive = true;
    mThread = std::thread([this]() {
        while (mIsActive == true) {
            const auto location = common::Utils::getMockLocation();
            this->batchLocation(location);
            std::this_thread::sleep_for(std::chrono::milliseconds(mMinIntervalMs));
        }
    });

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssBatching::flush() {
    ALOGD("flush");
    std::vector<GnssLocation> copy = std::vector<GnssLocation>(mBatchedLocations);
    ndk::ScopedAStatus status;
    if (sCallback != nullptr) {
        sCallback->gnssLocationBatchCb(copy);
        status = ndk::ScopedAStatus::ok();
    } else {
        ALOGE("GnssBatchingCallback is null. flush() failed.");
        status = ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_GENERIC);
    }
    mBatchedLocations.clear();
    return status;
}

ndk::ScopedAStatus GnssBatching::stop() {
    ALOGD("stop");
    // Do not call flush() at stop()
    mIsActive = false;
    if (mThread.joinable()) {
        mThread.join();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus GnssBatching::cleanup() {
    ALOGD("cleanup");
    std::unique_lock<std::mutex> lock(mMutex);
    if (mIsActive) {
        stop();
    }
    flush();

    sCallback = nullptr;
    return ndk::ScopedAStatus::ok();
}

void GnssBatching::batchLocation(const GnssLocation& location) {
    if (mBatchedLocations.size() > BATCH_SIZE) {
        mBatchedLocations.erase(mBatchedLocations.begin());
    }
    mBatchedLocations.push_back(location);
    if (mWakeUpOnFifoFull && mBatchedLocations.size() == BATCH_SIZE) {
        flush();
    }
}

}  // namespace aidl::android::hardware::gnss
