/*
 * SPDX-FileCopyrightText: 2019-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/file.h>
#include <android-base/strings.h>
#include <livedisplay/samsung/ReadingEnhancementExynos.h>

#include <fstream>

using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace aidl {
namespace vendor {
namespace lineage {
namespace livedisplay {
namespace samsung {

static constexpr const char* kREPath = "/sys/class/mdnie/mdnie/accessibility";

bool ReadingEnhancementExynos::isSupported() {
    std::fstream file(kREPath, file.in | file.out);
    return file.good();
}

// Methods from ::aidl::vendor::lineage::livedisplay::BnReadingEnhancement follow.
ndk::ScopedAStatus ReadingEnhancementExynos::getEnabled(bool* _aidl_return) {
    std::string contents;

    if (!ReadFileToString(kREPath, &contents)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    contents = Trim(contents);

    *_aidl_return =
            !contents.compare("Current accessibility : DSI0 : GRAYSCALE") || !contents.compare("4");
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ReadingEnhancementExynos::setEnabled(bool enabled) {
    if (!WriteStringToFile(enabled ? "4" : "0", kREPath, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace samsung
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
}  // namespace aidl
