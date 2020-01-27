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

#ifndef VENDOR_LINEAGE_LIVEDISPLAY_V2_0_SUNLIGHTENHANCEMENT_H
#define VENDOR_LINEAGE_LIVEDISPLAY_V2_0_SUNLIGHTENHANCEMENT_H

#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <vendor/lineage/livedisplay/2.0/ISunlightEnhancement.h>

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

class SunlightEnhancement : public ISunlightEnhancement {
  public:
    bool isSupported();

    // Methods from ::vendor::lineage::livedisplay::V2_0::ISunlightEnhancement follow.
    Return<bool> isEnabled() override;
    Return<bool> setEnabled(bool enabled) override;

    // Methods from ::android::hidl::base::V1_0::IBase follow.
  private:
    bool mHasHBM = false;
};

}  // namespace samsung
}  // namespace V2_0
}  // namespace livedisplay
}  // namespace lineage
}  // namespace vendor

#endif  // VENDOR_LINEAGE_LIVEDISPLAY_V2_0_SUNLIGHTENHANCEMENT_H
