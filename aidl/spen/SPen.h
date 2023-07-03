/*
 * SPDX-FileCopyrightText: 2022-2023 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/samsung/hardware/spen/BnSPen.h>

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace spen {

#define SYSFS_CHARGING_NODE "/sys/class/sec/sec_epen/epen_ble_charging_mode"
#define SPEN_STATE_CHARGE "CHARGE"
#define SPEN_STATE_NG "NG"
#define SPEN_ADDR_PATH_VENDOR "/mnt/vendor/efs/spen/blespen_addr"
#define SPEN_ADDR_PATH "/efs/spen/blespen_addr"
#define SPEN_ADDR_DEFAULT "00:00:00:00:00:00"

class SPen : public BnSPen {
  public:
    SPen();

    ndk::ScopedAStatus setCharging(bool in_charging, bool* _aidl_return) override;
    ndk::ScopedAStatus isCharging(bool* _aidl_return) override;
    ndk::ScopedAStatus getMACAddress(std::string* _aidl_return) override;
    ndk::ScopedAStatus setMACAddress(const std::string& in_mac) override;
};

}  // namespace spen
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
