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

#include "SunlightEnhancementExynos.h"

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace samsung {

// Methods from ::vendor::lineage::livedisplay::V2_0::ISunlightEnhancement follow.
bool SunlightEnhancementExynos::isSupported() {
    std::ofstream file("/sys/class/mdnie/mdnie/lux");
    return file.good();
}

// Methods from ::vendor::lineage::livedisplay::V2_0::IAdaptiveBacklight follow.
Return<bool> SunlightEnhancementExynos::isEnabled() {
    std::ifstream file("/sys/class/mdnie/mdnie/lux");
    int status = -1;

    if (file.is_open()) {
        file >> status;
    }

    return file.good() && status > 0;
}

Return<bool> SunlightEnhancementExynos::setEnabled(bool enabled) {
    std::ofstream file("/sys/class/mdnie/mdnie/lux");
    if (file.is_open()) {
        /* see drivers/video/fbdev/exynos/decon_7880/panels/mdnie_lite_table*, get_hbm_index */
        file << (enabled ? "40000" : "0");
    }

    return true;
}

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
