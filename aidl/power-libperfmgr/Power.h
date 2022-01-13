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

#include <atomic>
#include <memory>
#include <thread>

#include <aidl/android/hardware/power/BnPower.h>
#include <perfmgr/HintManager.h>

#include "InteractionHandler.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using ::aidl::android::hardware::power::Boost;
using ::aidl::android::hardware::power::Mode;
using ::android::perfmgr::HintManager;

class Power : public ::aidl::android::hardware::power::BnPower {
  public:
    Power(std::shared_ptr<HintManager> hm);
    ndk::ScopedAStatus setMode(Mode type, bool enabled) override;
    ndk::ScopedAStatus isModeSupported(Mode type, bool *_aidl_return) override;
    ndk::ScopedAStatus setBoost(Boost type, int32_t durationMs) override;
    ndk::ScopedAStatus isBoostSupported(Boost type, bool *_aidl_return) override;
    binder_status_t dump(int fd, const char **args, uint32_t numArgs) override;

  private:
    std::shared_ptr<HintManager> mHintManager;
    std::unique_ptr<InteractionHandler> mInteractionHandler;
    std::atomic<bool> mVRModeOn;
    std::atomic<bool> mSustainedPerfModeOn;
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
