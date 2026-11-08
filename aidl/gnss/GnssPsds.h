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

#include <aidl/android/hardware/gnss/BnGnssPsds.h>

namespace aidl::android::hardware::gnss {

struct GnssPsds : public BnGnssPsds {
  public:
    ndk::ScopedAStatus setCallback(const std::shared_ptr<IGnssPsdsCallback>& callback) override;
    ndk::ScopedAStatus injectPsdsData(PsdsType psdsType,
                                      const std::vector<uint8_t>& psdsData) override;

  private:
    // Guarded by mMutex
    static std::shared_ptr<IGnssPsdsCallback> sCallback;

    // Synchronization lock for sCallback
    mutable std::mutex mMutex;
};

}  // namespace aidl::android::hardware::gnss
