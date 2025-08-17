/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/livedisplay/BnDisplayColorCalibration.h>

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace samsung {

class DisplayColorCalibration : public BnDisplayColorCalibration {
  public:
    bool isSupported();

    // Methods from ::aidl::vendor::lineage::livedisplay::BnDisplayColorCalibration follow.
    ndk::ScopedAStatus getMaxValue(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getMinValue(int32_t* _aidl_return) override;
    ndk::ScopedAStatus getCalibration(std::vector<int32_t>* _aidl_return) override;
    ndk::ScopedAStatus setCalibration(const std::vector<int32_t>& rgb) override;
};

}  // namespace samsung
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
