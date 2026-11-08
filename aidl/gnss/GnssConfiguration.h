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

#include <aidl/android/hardware/gnss/BnGnssConfiguration.h>

namespace aidl::android::hardware::gnss {

struct GnssConfiguration : public BnGnssConfiguration {
    ndk::ScopedAStatus setSuplVersion(int) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setSuplMode(int) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setLppProfile(int) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setGlonassPositioningProtocol(int) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus setEmergencySuplPdn(bool) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setEsExtensionSec(int) override { return ndk::ScopedAStatus::ok(); }

    ndk::ScopedAStatus setBlocklist(const std::vector<BlocklistedSource>&) override {
        return ndk::ScopedAStatus::ok();
    }
};

}  // namespace aidl::android::hardware::gnss
