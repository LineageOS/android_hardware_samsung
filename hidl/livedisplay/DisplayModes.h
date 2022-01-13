/*
 * Copyright (C) 2019-2022 The LineageOS Project
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

#pragma once

#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <vendor/lineage/livedisplay/2.0/IDisplayModes.h>

namespace vendor {
namespace lineage {
namespace livedisplay {
namespace V2_0 {
namespace samsung {

using ::android::hardware::hidl_array;
using ::android::hardware::hidl_memory;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::sp;

class DisplayModes : public IDisplayModes {
  public:
    DisplayModes();
    bool isSupported();

    // Methods from ::vendor::lineage::livedisplay::V2_0::IDisplayModes follow.
    Return<void> getDisplayModes(getDisplayModes_cb resultCb) override;
    Return<void> getCurrentDisplayMode(getCurrentDisplayMode_cb resultCb) override;
    Return<void> getDefaultDisplayMode(getDefaultDisplayMode_cb resultCb) override;
    Return<bool> setDisplayMode(int32_t modeID, bool makeDefault) override;

    // Methods from ::android::hidl::base::V1_0::IBase follow.
  private:
    static const std::map<int32_t, std::string> kModeMap;
    int32_t mDefaultModeId;
};

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor
