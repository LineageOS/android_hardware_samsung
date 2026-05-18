/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ConsumerIr.h"

#include <samsung_ir.h>

#include <android-base/file.h>
#include <android-base/strings.h>

using android::base::Join;
using android::base::WriteStringToFile;

namespace aidl::android::hardware::ir {

ndk::ScopedAStatus ConsumerIr::transmit(int32_t in_carrierFreqHz,
                                        const std::vector<int32_t>& in_pattern) {
    float factor;
    std::vector<int32_t> buffer{in_carrierFreqHz};

#ifndef MS_IR_SIGNAL
    factor = 1000000 / in_carrierFreqHz;
#else
    factor = 1;
#endif

    for (const int32_t& number : in_pattern) {
        buffer.emplace_back(static_cast<int32_t>(number / factor));
    }

    if (!WriteStringToFile(Join(buffer, ','), IR_PATH, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ConsumerIr::getCarrierFreqs(
        std::vector<::aidl::android::hardware::ir::ConsumerIrFreqRange>* out_ranges) {
    *out_ranges = consumerirFreqs;
    return ndk::ScopedAStatus::ok();
}

}  // namespace aidl::android::hardware::ir
