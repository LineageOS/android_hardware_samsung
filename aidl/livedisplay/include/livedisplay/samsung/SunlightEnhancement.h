/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/livedisplay/BnSunlightEnhancement.h>

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace samsung {

class SunlightEnhancement : public BnSunlightEnhancement {
  public:
    bool isSupported();

    // Methods from ::aidl::vendor::lineage::livedisplay::BnSunlightEnhancement follow.
    ndk::ScopedAStatus getEnabled(bool* _aidl_return) override;
    ndk::ScopedAStatus setEnabled(bool enabled) override;
};

}  // namespace samsung
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
