/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace hardware {
namespace samsung {
namespace health {

class BatteryInfo {
  public:
    BatteryInfo();
    void RestoreBattType();

  private:
    const char* kBattTypeSysfs = "/sys/class/power_supply/battery/batt_type";
    const char* kSecAuthPresence = "/sys/class/power_supply/sec_auth/presence";
    const char* kSecAuthBattAuth = "/sys/class/power_supply/sec_auth/batt_auth";
    const char* kSecAuthQrCode = "/sys/class/power_supply/sec_auth/qr_code";
    const char* kHwParamBattQrEfs = "/efs/FactoryApp/HwParamBattQR";
};

}  // namespace health
}  // namespace samsung
}  // namespace hardware
