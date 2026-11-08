/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <aidl/android/hardware/gnss/BnGnssPowerIndication.h>

namespace aidl::android::hardware::gnss {

struct GnssPowerIndication : public BnGnssPowerIndication {
  public:
    ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IGnssPowerIndicationCallback>& callback) override;
    ndk::ScopedAStatus requestGnssPowerStats() override;

    void notePowerConsumption();

  private:
    // Guarded by mMutex
    static std::shared_ptr<IGnssPowerIndicationCallback> sCallback;

    // Synchronization lock for sCallback
    mutable std::mutex mMutex;

    int numLocationReported;
};

}  // namespace aidl::android::hardware::gnss
