/*
 * SPDX-FileCopyrightText: 2020 The Android Open Source Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
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
