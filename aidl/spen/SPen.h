/*
 * SPDX-FileCopyrightText: 2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/samsung/hardware/spen/BnSPen.h>

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace spen {

class SPen : public BnSPen {
  public:
    SPen();

    ndk::ScopedAStatus setCharging(bool charging, bool* _aidl_return) override;
    ndk::ScopedAStatus isCharging(bool* _aidl_return) override;
    ndk::ScopedAStatus getMACAddress(std::string* _aidl_return) override;
};

}  // namespace spen
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
