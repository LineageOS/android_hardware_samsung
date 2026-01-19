/*
 * Copyright (C) 2022-2023 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/health/BnChargingControl.h>
#include <aidl/vendor/lineage/health/ChargingControlSupportedMode.h>
#include <aidl/vendor/lineage/health/ChargingLimitInfo.h>

#define CHARGING_ENABLED_NODE "/sys/class/power_supply/battery/charging_enabled"

namespace aidl {
namespace vendor {
namespace lineage {
namespace health {

class ChargingControl : public BnChargingControl {
  public:
    ndk::ScopedAStatus getChargingEnabled(bool* _aidl_return) override;
    ndk::ScopedAStatus setChargingEnabled(bool enabled) override;
    ndk::ScopedAStatus setChargingDeadline(int64_t deadline) override;
    ndk::ScopedAStatus getSupportedMode(int* _aidl_return) override;
    ndk::ScopedAStatus getChargingDeadline(int64_t* _aidl_return) override;
    ndk::ScopedAStatus getChargingLimit(ChargingLimitInfo* _aidl_return) override;
    ndk::ScopedAStatus setChargingLimit(const ChargingLimitInfo& limit) override;
};

}  // namespace health
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
