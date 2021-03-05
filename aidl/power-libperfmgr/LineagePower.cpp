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

#include "LineagePower.h"
#include "power-common.h"

namespace aidl {
namespace vendor {
namespace lineage {
namespace power {
namespace impl {

ndk::ScopedAStatus Power::getFeature(Feature feature, int* _aidl_return) {
    switch (feature) {
        case Feature::SUPPORTED_PROFILES:
            *_aidl_return = get_number_of_profiles();
            break;
        default:
            *_aidl_return = -1;
            break;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::setBoost(Boost type, int durationMs) {
    switch (type) {
        case Boost::SET_PROFILE:
            set_power_profile(durationMs);
            break;
        default:
            break;
    }
    return ndk::ScopedAStatus::ok();
}

}  // namespace impl
}  // namespace power
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl