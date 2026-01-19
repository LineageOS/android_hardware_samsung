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

    std::string content;
    if (!android::base::ReadFileToString(*fastChargeConfig.node, &content, true)) {
        LOG(ERROR) << "Failed to read current fast charging value";
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    content = android::base::Trim(content);

    auto mode = fastChargeConfig.modeToValue(content);
    if (!mode) {
        LOG(ERROR) << "Failed to parse current fast charging value: " << content;
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    *_aidl_return = *mode;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FastCharge::setFastChargeMode(FastChargeMode in_mode,
                                                 FastChargeMode* _aidl_return) {
    auto value = fastChargeConfig.valueToMode(in_mode);
    if (!value) {
        LOG(ERROR) << "Unsupported fast charge mode: " << static_cast<int>(in_mode);
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    if (!android::base::WriteStringToFile(*value, *fastChargeConfig.node)) {
        LOG(ERROR) << "Failed to write to fast charge node: " << strerror(errno);
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
