/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/health/BnFastCharge.h>

namespace aidl {
namespace vendor {
namespace lineage {
namespace health {

class FastChargeConfig {
  public:
    FastChargeConfig() : supportedModes(supportedModesInternal()) {}

    std::optional<FastChargeMode> modeToValue(const std::string& value) const {
        if (value == valueNone) return FastChargeMode::NONE;
        if (value == valueFastCharge) return FastChargeMode::FAST_CHARGE;
        if (value == valueSuperFastCharge) return FastChargeMode::SUPER_FAST_CHARGE;
        return {};
    }

    std::optional<std::string> valueToMode(FastChargeMode mode) const {
        switch (mode) {
            case FastChargeMode::NONE:
                return valueNone;
            case FastChargeMode::FAST_CHARGE:
                return valueFastCharge;
            case FastChargeMode::SUPER_FAST_CHARGE:
                return valueSuperFastCharge;
            default:
                return {};
        }
    }

    std::optional<std::string> node{HEALTH_FAST_CHARGE_NODE};
    std::optional<std::string> valueNone{HEALTH_FAST_CHARGE_VALUE_NONE};
    std::optional<std::string> valueFastCharge{HEALTH_FAST_CHARGE_VALUE_FAST_CHARGE};
    std::optional<std::string> valueSuperFastCharge{HEALTH_FAST_CHARGE_VALUE_SUPER_FAST_CHARGE};
    int64_t supportedModes;

  private:
    int64_t supportedModesInternal() const {
        int64_t ret = 0;

        if (node) {
            if (valueNone) ret |= static_cast<int>(FastChargeMode::NONE);
            if (valueFastCharge) ret |= static_cast<int>(FastChargeMode::FAST_CHARGE);
            if (valueSuperFastCharge) ret |= static_cast<int>(FastChargeMode::SUPER_FAST_CHARGE);
        }

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
