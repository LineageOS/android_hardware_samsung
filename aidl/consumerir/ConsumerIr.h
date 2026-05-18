/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/ir/BnConsumerIr.h>

using ::aidl::android::hardware::ir::ConsumerIrFreqRange;

namespace aidl {
namespace android {
namespace hardware {
namespace ir {

class ConsumerIr : public BnConsumerIr {
  public:
    // Methods from aidl::android::hardware::ir::ConsumerIr follow.
    ndk::ScopedAStatus getCarrierFreqs(std::vector<ConsumerIrFreqRange>* _aidl_return) override;
    ndk::ScopedAStatus transmit(int32_t in_carrierFreqHz,
                                const std::vector<int32_t>& in_pattern) override;
};

}  // namespace ir
}  // namespace hardware
}  // namespace android
}  // namespace aidl
