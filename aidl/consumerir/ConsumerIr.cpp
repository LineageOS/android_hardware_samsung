/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "android.hardware.ir-service.samsung"

#include "ConsumerIr.h"

#include <samsung_ir.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/strings.h>

using android::base::Join;
using android::base::WriteStringToFile;

namespace aidl::android::hardware::ir {

ndk::ScopedAStatus ConsumerIr::getCarrierFreqs(std::vector<ConsumerIrFreqRange>* _aidl_return) {
    *_aidl_return = consumerirFreqs;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus ConsumerIr::transmit(int32_t carrierFreqHz,
                                        const std::vector<int32_t>& pattern) {
    float factor;
    std::vector<int32_t> buffer{carrierFreqHz};

    if (!isInRange(carrierFreqHz)) {
        LOG(ERROR) << "Unsupported carrier " << carrierFreqHz;
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

#ifndef MS_IR_SIGNAL
    factor = 1000000.0f / static_cast<float>(carrierFreqHz);
#else
    factor = 1.0f;
#endif

    for (const int32_t& number : pattern) {
        buffer.emplace_back(static_cast<int32_t>(number / factor));
    }

    if (!WriteStringToFile(Join(buffer, ','), IR_PATH, true)) {
        LOG(ERROR) << "Failed to set buffer for carrier " << carrierFreqHz << ", error: " << errno;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }

    return ndk::ScopedAStatus::ok();
}

bool ConsumerIr::isInRange(int32_t carrierFreqHz) {
    for (const auto& range : consumerirFreqs) {
        if (carrierFreqHz >= range.minHz && carrierFreqHz <= range.maxHz) {
            return true;
        }
    }
    return false;
}

}  // namespace aidl::android::hardware::ir
