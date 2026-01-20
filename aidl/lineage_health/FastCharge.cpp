/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FastCharge.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>
#include <map>

#define LOG_TAG "vendor.lineage.health-service.samsung"

namespace aidl {
namespace vendor {
namespace lineage {
namespace health {

ndk::ScopedAStatus FastCharge::getSupportedFastChargeModes(int64_t* _aidl_return) {
    *_aidl_return = fastChargeConfig.supportedModes;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FastCharge::getFastChargeMode(FastChargeMode* _aidl_return) {
    if (fastChargeConfig.supportedModes == 0) {
        LOG(ERROR) << "Fast charge not supported";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    bool SFCEnabled = false, AFCEnabled = false;

    if (fastChargeConfig.supportedModes & static_cast<int64_t>(FastChargeMode::SUPER_FAST_CHARGE)) {
        if (!fastChargeConfig.readEnabledStateFromNode(*fastChargeConfig.SFCNode, &SFCEnabled)) {
            LOG(ERROR) << "Failed to read current fast charging value";
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        }
    }

    if (fastChargeConfig.supportedModes & static_cast<int64_t>(FastChargeMode::FAST_CHARGE)) {
        if (!fastChargeConfig.readEnabledStateFromNode(*fastChargeConfig.AFCNode, &AFCEnabled)) {
            LOG(ERROR) << "Failed to read current fast charging value";
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        }
    }

    if (SFCEnabled && AFCEnabled)
        *_aidl_return = FastChargeMode::SUPER_FAST_CHARGE;
    else if (AFCEnabled)
        *_aidl_return = FastChargeMode::FAST_CHARGE;
    else
        *_aidl_return = FastChargeMode::NONE;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FastCharge::setFastChargeMode(FastChargeMode in_mode,
                                                 FastChargeMode* _aidl_return) {
    bool success = true;
    if (in_mode >= FastChargeMode::FAST_CHARGE) {
        success &= android::base::WriteStringToFile("0", *fastChargeConfig.AFCNode);

        if (in_mode >= FastChargeMode::SUPER_FAST_CHARGE) {
            success &= android::base::WriteStringToFile("0", *fastChargeConfig.SFCNode);
        }

        else {
            success &= android::base::WriteStringToFile("1", *fastChargeConfig.SFCNode);
        }
    } else {
        success &= android::base::WriteStringToFile("1", *fastChargeConfig.AFCNode);
        success &= android::base::WriteStringToFile("1", *fastChargeConfig.SFCNode);
    }

    if (!success) {
        LOG(ERROR) << "Failed to write to fast charge nodes: " << strerror(errno);
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return getFastChargeMode(_aidl_return);
}

binder_status_t FastCharge::dump(int fd, const char** args, uint32_t numArgs) {
    int64_t supportedFastChargeModes;
    getSupportedFastChargeModes(&supportedFastChargeModes);

    FastChargeMode fastChargeMode;
    getFastChargeMode(&fastChargeMode);

    dprintf(fd, "Fast charge supported modes: %ld\n", supportedFastChargeModes);
    dprintf(fd, "Fast charge mode: %d\n", fastChargeMode);

    return STATUS_OK;
}

}  // namespace health
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
