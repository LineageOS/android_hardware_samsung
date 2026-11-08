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

#include <aidl/android/hardware/gnss/BnGnssAntennaInfo.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace aidl::android::hardware::gnss {

struct GnssAntennaInfo : public BnGnssAntennaInfo {
  public:
    GnssAntennaInfo();
    ~GnssAntennaInfo();
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IGnssAntennaInfoCallback>& callback) override;
    ndk::ScopedAStatus close() override;

  private:
    void start();
    void stop();
    void reportAntennaInfo(
            const std::vector<IGnssAntennaInfoCallback::GnssAntennaInfo>& antennaInfo) const;

    // Guarded by mMutex
    static std::shared_ptr<IGnssAntennaInfoCallback> sCallback;

    std::atomic<bool> mIsActive;
    std::atomic<long> mMinIntervalMs;
    std::thread mThread;

    // Synchronization lock for sCallback
    mutable std::mutex mMutex;
};

}  // namespace aidl::android::hardware::gnss
