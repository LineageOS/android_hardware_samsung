/*
 * Copyright (C) 2019 The LineageOS Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>
#include <android-base/strings.h>

#include <fstream>

#include "SunlightEnhancement.h"

using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace samsung {

static constexpr const char* kHBMPath = "/sys/class/lcd/panel/panel/auto_brightness";
static constexpr const char* kSREPath = "/sys/class/mdnie/mdnie/outdoor";

// Methods from ::vendor::lineage::livedisplay::V2_0::ISunlightEnhancement follow.
bool SunlightEnhancement::isSupported() {
    std::fstream sre(kSREPath, sre.in | sre.out);
    std::fstream hbm(kHBMPath, hbm.in | hbm.out);

    if (hbm.good()) {
        mHasHBM = true;
    }

    return sre.good();
}

// Methods from ::vendor::lineage::livedisplay::V2_0::IAdaptiveBacklight follow.
Return<bool> SunlightEnhancement::isEnabled() {
    std::string tmp;
    int32_t statusSRE = 0;
    int32_t statusHBM = 0;
    if (ReadFileToString(kSREPath, &tmp)) {
        statusSRE = std::stoi(Trim(tmp));
    }

    if (mHasHBM && ReadFileToString(kHBMPath, &tmp)) {
        statusHBM = std::stoi(Trim(tmp));
    }

    return ((statusSRE == 1 && statusHBM == 6) || statusSRE == 1);
}

Return<bool> SunlightEnhancement::setEnabled(bool enabled) {
    if (mHasHBM) {
        WriteStringToFile(enabled ? "6" : "0", kHBMPath, true);
    }

    return WriteStringToFile(enabled ? "1" : "0", kSREPath, true);
}

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
