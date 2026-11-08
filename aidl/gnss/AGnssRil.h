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

#include <aidl/android/hardware/gnss/BnAGnssRil.h>

namespace aidl::android::hardware::gnss {

struct AGnssRil : public BnAGnssRil {
  public:
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IAGnssRilCallback>& callback) override;
    ndk::ScopedAStatus setRefLocation(const AGnssRefLocation& agnssReflocation) override;
    ndk::ScopedAStatus setSetId(SetIdType type, const std::string& setid) override;
    ndk::ScopedAStatus updateNetworkState(const NetworkAttributes& attributes) override;
    ndk::ScopedAStatus injectNiSuplMessageData(const std::vector<uint8_t>& msgData,
                                               int slotIndex) override;

  private:
    // Synchronization lock for sCallback
    mutable std::mutex mMutex;
    // Guarded by mMutex
    static std::shared_ptr<IAGnssRilCallback> sCallback;
};

}  // namespace aidl::android::hardware::gnss
