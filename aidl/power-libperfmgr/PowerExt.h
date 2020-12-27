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

#include <aidl/google/hardware/power/extension/pixel/BnPowerExt.h>
#include <perfmgr/HintManager.h>

#include "disp-power/DisplayLowPower.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using ::android::perfmgr::HintManager;

class PowerExt : public ::aidl::google::hardware::power::extension::pixel::BnPowerExt {
  public:
    PowerExt(std::shared_ptr<HintManager> hm, std::shared_ptr<DisplayLowPower> dlpw)
        : mHintManager(hm), mDisplayLowPower(dlpw) {}
    ndk::ScopedAStatus setMode(const std::string &mode, bool enabled) override;
    ndk::ScopedAStatus isModeSupported(const std::string &mode, bool *_aidl_return) override;
    ndk::ScopedAStatus setBoost(const std::string &boost, int32_t durationMs) override;
    ndk::ScopedAStatus isBoostSupported(const std::string &boost, bool *_aidl_return) override;

  private:
    std::shared_ptr<HintManager> mHintManager;
    std::shared_ptr<DisplayLowPower> mDisplayLowPower;
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
