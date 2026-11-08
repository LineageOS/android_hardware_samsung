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

#include <aidl/android/hardware/gnss/BnGnssNavigationMessageInterface.h>
#include <atomic>
#include <future>
#include <thread>
#include "Utils.h"

namespace aidl::android::hardware::gnss {

struct GnssNavigationMessageInterface : public BnGnssNavigationMessageInterface {
  public:
    GnssNavigationMessageInterface();
    ~GnssNavigationMessageInterface();
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IGnssNavigationMessageCallback>& callback) override;
    ndk::ScopedAStatus close() override;

  private:
    void start();
    void stop();
    void reportMessage(const IGnssNavigationMessageCallback::GnssNavigationMessage& message);
    void waitForStoppingThreads();

    std::atomic<long> mMinIntervalMillis;
    std::atomic<bool> mIsActive;
    std::vector<std::thread> mThreads;
    std::vector<std::future<void>> mFutures;
    ::android::hardware::gnss::common::ThreadBlocker mThreadBlocker;

    // Guarded by mMutex
    static std::shared_ptr<IGnssNavigationMessageCallback> sCallback;
    // Synchronization lock for sCallback
    mutable std::mutex mMutex;
};

}  // namespace aidl::android::hardware::gnss
