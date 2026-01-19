/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/health/BnFastCharge.h>

#define AFC_DISABLE_NODE "/sys/class/sec/switch/afc_disable"
#define PD_DISABLE_NODE "/sys/class/power_supply/battery/pd_disable"

namespace aidl {
namespace vendor {
namespace lineage {
namespace health {

class FastCharge : public BnFastCharge {
  public:
    FastCharge();

    ndk::ScopedAStatus getSupportedFastChargeModes(int64_t* _aidl_return) override;
    ndk::ScopedAStatus getFastChargeMode(FastChargeMode* _aidl_return) override;
    ndk::ScopedAStatus setFastChargeMode(FastChargeMode in_mode,
                                         FastChargeMode* _aidl_return) override;

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;

  private:
    int64_t mSupportedModes;
};

}  // namespace health
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
