/*
 * Copyright (C) 2021 The LineageOS Project
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

#include <aidl/vendor/lineage/power/BnPower.h>
#include <perfmgr/HintManager.h>

using ::aidl::vendor::lineage::power::Boost;
using ::aidl::vendor::lineage::power::Feature;
using ::android::perfmgr::HintManager;

namespace aidl {
namespace vendor {
namespace lineage {
namespace power {
namespace impl {

enum PowerProfile {
  POWER_SAVE = 0,
  BALANCED,
  HIGH_PERFORMANCE,
  BIAS_POWER_SAVE,
  BIAS_PERFORMANCE,
  MAX
};

class LineagePower : public BnPower {
  public:
    LineagePower(std::shared_ptr<HintManager> hm);
    ndk::ScopedAStatus getFeature(Feature feature, int* _aidl_return) override;
    ndk::ScopedAStatus setBoost(Boost type, int durationMs) override;

    private:
      std::shared_ptr<HintManager> mHintManager;
      std::atomic<PowerProfile> mCurrentPerfProfile;

      void setProfile(PowerProfile profile);
};

}  // namespace impl
}  // namespace power
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl