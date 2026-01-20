/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fstream>
#include <aidl/vendor/lineage/health/BnFastCharge.h>

namespace aidl {
namespace vendor {
namespace lineage {
namespace health {

class FastChargeConfig {
  public:
    FastChargeConfig() : supportedModes(supportedModesInternal()) {}

    std::optional<std::string> AFCNode{"/sys/class/sec/switch/afc_disable"};
    std::optional<std::string> SFCNode{"/sys/class/power_supply/battery/pd_disable"};
    int64_t supportedModes;

  private:
    int64_t supportedModesInternal() const {
        int64_t ret = static_cast<int>(FastChargeMode::NONE);
        std::ifstream file;
        
        file.open("/sys/class/sec/switch/afc_disable");
        if (file.is_open()) {
            ret |= static_cast<int>(FastChargeMode::FAST_CHARGE);
            file.close();
        }

        file.open("/sys/class/power_supply/battery/pd_disable");
        if (file.is_open()) {
            ret |= static_cast<int>(FastChargeMode::SUPER_FAST_CHARGE);
            file.close();
        }

        ret |= static_cast<int>(FastChargeMode::SUPER_FAST_CHARGE);
        return ret;
    }
};

class FastCharge : public BnFastCharge {
  public:
    ndk::ScopedAStatus getSupportedFastChargeModes(int64_t* _aidl_return) override;
    ndk::ScopedAStatus getFastChargeMode(FastChargeMode* _aidl_return) override;
    ndk::ScopedAStatus setFastChargeMode(FastChargeMode in_mode,
                                         FastChargeMode* _aidl_return) override;

    binder_status_t dump(int fd, const char** args, uint32_t numArgs) override;

  private:
    const FastChargeConfig fastChargeConfig;
};

}  // namespace health
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
