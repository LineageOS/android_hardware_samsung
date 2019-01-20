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

#include <fstream>

#include "SunlightEnhancement.h"

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace samsung {

// Methods from ::vendor::lineage::livedisplay::V2_0::ISunlightEnhancement follow.
bool SunlightEnhancement::isSupported() {
    std::ofstream fileSRE("/sys/class/mdnie/mdnie/outdoor");
    std::ofstream fileHBM("/sys/class/lcd/panel/panel/auto_brightness");
    return fileSRE.good() || fileHBM.good();
}

// Methods from ::vendor::lineage::livedisplay::V2_0::IAdaptiveBacklight follow.
Return<bool> SunlightEnhancement::isEnabled() {
    std::ifstream fileSRE("/sys/class/mdnie/mdnie/outdoor");
    std::ifstream fileHBM("/sys/class/lcd/panel/panel/auto_brightness");
    int statusSRE = -1;
    int statusHBM = -1;

    if (fileSRE.is_open()) {
        fileSRE >> statusSRE;
    }
    if (fileHBM.is_open()) {
        fileHBM >> statusHBM;
    }

    return (fileSRE.good() || fileHBM.good()) && ((statusSRE == 1 && statusHBM == 6) || statusSRE == 1);
}

Return<bool> SunlightEnhancement::setEnabled(bool enabled) {
    std::ofstream fileSRE("/sys/class/mdnie/mdnie/outdoor");
    std::ofstream fileHBM("/sys/class/lcd/panel/panel/auto_brightness");

    if (fileSRE.is_open()) {
        fileSRE << (enabled ? "1" : "0");
    }
    if (fileHBM.is_open()) {
        fileHBM << (enabled ? "6" : "0");
    }

    return true;
}

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
