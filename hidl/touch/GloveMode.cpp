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

#include "GloveMode.h"

namespace vendor {
namespace lineage {
namespace touch {
namespace V1_0 {
namespace samsung {

bool GloveMode::isSupported() {

    std::ofstream touchkeyGloveMode("/sys/class/sec/sec_touchkey/glove_mode");
    if (touchkeyGloveMode.good() == true ) {
        GloveMode::touchkeySupportGloveMode = true;
    }

    std::ifstream file("/sys/class/sec/tsp/cmd_list");
    if (file.is_open()) {
        std::string line;
        while (getline(file, line)) {
            if (!line.compare("glove_mode")) return true;
        }
        file.close();
    }

    return false;
}

// Methods from ::vendor::lineage::touch::V1_0::IGloveMode follow.
Return<bool> GloveMode::isEnabled() {
    std::ifstream file("/sys/class/sec/tsp/cmd_result");
    if (file.is_open()) {
        std::string line;
        getline(file, line);
        if (!line.compare("glove_mode,1:OK")) return true;
        file.close();
    }
    return false;
}

Return<bool> GloveMode::setEnabled(bool enabled) {

    if (GloveMode::touchkeySupportGloveMode == true) {
        std::ofstream touchkeyGloveMode("/sys/class/sec/sec_touchkey/glove_mode");
        touchkeyGloveMode << (enabled ? "1" : "0");
    }

    std::ofstream file("/sys/class/sec/tsp/cmd");
    file << "glove_mode," << (enabled ? "1" : "0");

    return true;
}

}  // namespace samsung
}  // namespace V1_0
}  // namespace touch
}  // namespace lineage
}  // namespace vendor
