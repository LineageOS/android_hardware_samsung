/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/file.h>
#include <android-base/strings.h>

#include <fstream>

#include "SunlightEnhancement.h"

using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace samsung {

static constexpr const char* kHBMPath = "/sys/class/lcd/panel/panel/auto_brightness";
static constexpr const char* kSREPath = "/sys/class/mdnie/mdnie/outdoor";

bool SunlightEnhancement::isSupported() {
    std::fstream sreFile(kSREPath, sreFile.in | sreFile.out);
    std::fstream hbmFile(kHBMPath, hbmFile.in | hbmFile.out);

    if (hbmFile.good()) {
        mHasHBM = true;
    }

    return sreFile.good();
}

// Methods from ::aidl::vendor::lineage::livedisplay::BnSunlightEnhancement follow.
ndk::ScopedAStatus SunlightEnhancement::getEnabled(bool* _aidl_return) {
    std::string tmp;
    int32_t statusSRE = 0;
    int32_t statusHBM = 0;

    if (!ReadFileToString(kSREPath, &tmp)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    statusSRE = std::stoi(Trim(tmp));

    if (mHasHBM) {
        if (!ReadFileToString(kHBMPath, &tmp)) {
            return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
        }
        statusHBM = std::stoi(Trim(tmp));
    }

    *_aidl_return = ((statusSRE == 1 && statusHBM == 6) || statusSRE == 1);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SunlightEnhancement::setEnabled(bool enabled) {
    if (mHasHBM && !WriteStringToFile(enabled ? "6" : "0", kHBMPath, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    if (!WriteStringToFile(enabled ? "1" : "0", kSREPath, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace samsung
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
