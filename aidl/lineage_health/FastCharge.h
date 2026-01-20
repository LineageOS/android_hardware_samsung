/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/vendor/lineage/health/BnFastCharge.h>
#include <android-base/file.h>
#include <android-base/strings.h>
#include <fstream>

namespace aidl {
namespace vendor {
namespace lineage {
namespace health {

static constexpr const char* kAFCNodePath = "/sys/class/sec/switch/afc_disable";
static constexpr const char* kSFCNodePath = "/sys/class/power_supply/battery/pd_disable";
static constexpr const char* kEnabledValue = "0";

class FastChargeConfig {
  public:
    FastChargeConfig()
        : AFCNode(kAFCNodePath),
          SFCNode(kSFCNodePath),
          supportedModes(supportedModesInternal(*AFCNode, *SFCNode)) {}

    std::optional<std::string> AFCNode, SFCNode;
    int64_t supportedModes;

    bool readEnabledStateFromNode(const std::string& path, bool* enabled) const {
        std::string content;
        if (!android::base::ReadFileToString(path, &content, true)) {
            return false;
        }
        content = android::base::Trim(content);
        *enabled = (content == kEnabledValue);
        return true;
    }

  private:
    int64_t supportedModesInternal(const std::string& AFCNode, const std::string& SFCNode) const {
        int64_t ret = static_cast<int>(FastChargeMode::NONE);

        if (std::ifstream(AFCNode)) {
            ret |= static_cast<int64_t>(FastChargeMode::FAST_CHARGE);
        }

        if (std::ifstream(SFCNode)) {
            ret |= static_cast<int64_t>(FastChargeMode::SUPER_FAST_CHARGE);
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
