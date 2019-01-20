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

#include "ReadingEnhancement.h"

using android::base::ReadFileToString;
using android::base::Trim;
using android::base::WriteStringToFile;

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace samsung {

static constexpr const char *kREPath = "/sys/class/mdnie/mdnie/accessibility";

// Methods from ::vendor::lineage::livedisplay::V2_0::ISunlightEnhancement follow.
bool ReadingEnhancement::isSupported() {
    std::fstream re(kREPath, re.in | re.out);
    return re.good();
}

// Methods from ::vendor::lineage::livedisplay::V2_0::IReadingEnhancement follow.
Return<bool> ReadingEnhancement::isEnabled() {
    std::string contents;

    if (ReadFileToString(kREPath, &contents)) {
        contents = Trim(contents);
    }

    return !contents.compare("Current accessibility : DSI0 : GRAYSCALE ") || !contents.compare("4");
}

Return<bool> ReadingEnhancement::setEnabled(bool enabled) {
    return WriteStringToFile(enabled ? "4" : "0", kREPath, true);
}


// Methods from ::android::hidl::base::V1_0::IBase follow.

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
