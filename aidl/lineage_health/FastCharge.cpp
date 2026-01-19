/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "FastCharge.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

#define LOG_TAG "vendor.lineage.health-service.samsung"

namespace aidl {
namespace vendor {
namespace lineage {
namespace health {

FastCharge::FastCharge() : mSupportedModes(0) {
    if (!access(AFC_DISABLE_NODE, O_RDWR))
        mSupportedModes |= static_cast<int32_t>(FastChargeMode::NONE) |
                           static_cast<int32_t>(FastChargeMode::FAST_CHARGE);

    if (!access(PD_DISABLE_NODE, O_RDWR))
        mSupportedModes |= static_cast<int32_t>(FastChargeMode::SUPER_FAST_CHARGE);
}

ndk::ScopedAStatus FastCharge::getSupportedFastChargeModes(int64_t* _aidl_return) {
    *_aidl_return = mSupportedModes;

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus FastCharge::getFastChargeMode(FastChargeMode* _aidl_return) {
    std::string afcDisableContent;

    if (!android::base::ReadFileToString(AFC_DISABLE_NODE, &afcDisableContent, true)) {
        LOG(ERROR) << "Failed to open " << AFC_DISABLE_NODE << ", " << strerror(errno);
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    afcDisableContent = android::base::Trim(afcDisableContent);

    if (afcDisableContent == "1") {
        *_aidl_return = FastChargeMode::NONE;
        return ndk::ScopedAStatus::ok();
    } else if (afcDisableContent == "0")
        *_aidl_return = FastChargeMode::FAST_CHARGE;
    else
        LOG(ERROR) << "Invalid afc_disable value read: " << afcDisableContent;

    std::string pdDisableContent;
    if (android::base::ReadFileToString(PD_DISABLE_NODE, &pdDisableContent, true)) {
        pdDisableContent = android::base::Trim(pdDisableContent);
        if (pdDisableContent == "0") *_aidl_return = FastChargeMode::SUPER_FAST_CHARGE;
    }

    return ndk::ScopedAStatus::ok();
}

#define WRITE_WITH_CHECK(file, value, is_error)                                               \
    if (!android::base::WriteStringToFile(value, file, true)) {                               \
        LOG(is_error ? ERROR : VERBOSE)                                                       \
                << "Failed to write " << value << " to " << file << ": " << strerror(errno);  \
        if (is_error) return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION); \
    }

ndk::ScopedAStatus FastCharge::setFastChargeMode(FastChargeMode in_mode,
                                                 FastChargeMode* _aidl_return) {
    if (!(static_cast<int32_t>(in_mode) & mSupportedModes)) {
        LOG(ERROR) << "Mode " << toString(in_mode) << " not supported!";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    switch (in_mode) {
        case FastChargeMode::NONE:
            WRITE_WITH_CHECK(AFC_DISABLE_NODE, "1", true)
            WRITE_WITH_CHECK(PD_DISABLE_NODE, "1", false)
            break;
        case FastChargeMode::FAST_CHARGE:
            WRITE_WITH_CHECK(AFC_DISABLE_NODE, "0", true)
            WRITE_WITH_CHECK(PD_DISABLE_NODE, "1", false)
            break;
        case FastChargeMode::SUPER_FAST_CHARGE:
            WRITE_WITH_CHECK(AFC_DISABLE_NODE, "0", true)
            WRITE_WITH_CHECK(PD_DISABLE_NODE, "0", true)
            break;
    }

    return getFastChargeMode(_aidl_return);
}

}  // namespace health
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
