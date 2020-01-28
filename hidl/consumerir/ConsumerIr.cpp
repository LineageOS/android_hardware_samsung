/*
 * Copyright (C) 2020 The LineageOS Project
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

#include "ConsumerIr.h"

#include <samsung_ir.h>

#include <android-base/file.h>
#include <android-base/strings.h>

using android::base::Join;
using android::base::WriteStringToFile;

namespace android {
namespace hardware {
namespace ir {
namespace V1_0 {
namespace implementation {

// Methods from ::android::hardware::ir::V1_0::IConsumerIr follow.
Return<bool> ConsumerIr::transmit(int32_t carrierFreq, const hidl_vec<int32_t>& pattern) {
    float factor;
    std::vector<int32_t> buffer{carrierFreq};

#ifndef MS_IR_SIGNAL
    // Calculate factor of conversion from microseconds to pulses
    factor = 1000000 / carrierFreq;
#else
    factor = 1;
#endif

    for (const int32_t& number : pattern) {
        buffer.emplace_back(number / factor);
    }

    return WriteStringToFile(Join(buffer, ','), IR_PATH, true);
}

Return<void> ConsumerIr::getCarrierFreqs(getCarrierFreqs_cb _hidl_cb) {
    _hidl_cb(true, consumerirFreqs);
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace ir
}  // namespace hardware
}  // namespace android
