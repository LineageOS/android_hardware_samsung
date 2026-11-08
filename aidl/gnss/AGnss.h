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

#include <aidl/android/hardware/gnss/BnAGnss.h>

namespace aidl::android::hardware::gnss {

using AGnssType = IAGnssCallback::AGnssType;

struct AGnss : public BnAGnss {
  public:
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IAGnssCallback>& callback) override;
    ndk::ScopedAStatus dataConnClosed() override { return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus dataConnFailed() override { return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus setServer(AGnssType type, const std::string& hostname, int port) override;
    ndk::ScopedAStatus dataConnOpen(int64_t networkHandle, const std::string& apn,
                                    ApnIpType apnIpType) override;

  private:
    // Synchronization lock for sCallback
    mutable std::mutex mMutex;
    // Guarded by mMutex
    static std::shared_ptr<IAGnssCallback> sCallback;
};

}  // namespace aidl::android::hardware::gnss
