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
#include "Power.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace power {
namespace impl {

using aidl::google::hardware::power::impl::pixel::Power;
using aidl::google::hardware::power::impl::pixel::PowerProfile;

class LineagePower : public BnPower {
  public:
    LineagePower(std::shared_ptr<Power> power, int32_t serviceNumPerfProfiles);
    ndk::ScopedAStatus getFeature(Feature feature, int* _aidl_return) override;
    ndk::ScopedAStatus setBoost(Boost type, int durationMs) override;

  private:
    std::shared_ptr<Power> mPower;
    int32_t mNumPerfProfiles;
};

}  // namespace impl
}  // namespace power
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl