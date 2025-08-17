/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/file.h>
#include <android-base/strings.h>
#include <livedisplay/samsung/SunlightEnhancement.h>

#include <fstream>

using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace samsung {

// Note: Exynos method is also available on Qualcomm
static constexpr const char* kSREPath = "/sys/class/mdnie/mdnie/outdoor";

bool SunlightEnhancement::isSupported() {
    std::fstream file(kSREPath, file.in | file.out);
    return file.good();
}

// Methods from ::aidl::vendor::lineage::livedisplay::BnSunlightEnhancement follow.
ndk::ScopedAStatus SunlightEnhancement::getEnabled(bool* _aidl_return) {
    std::string tmp;
    int32_t contents = 0;

    if (!ReadFileToString(kSREPath, &tmp)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    contents = std::stoi(Trim(tmp));

    *_aidl_return = contents == 1;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SunlightEnhancement::setEnabled(bool enabled) {
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
