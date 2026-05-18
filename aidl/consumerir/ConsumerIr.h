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
    ndk::ScopedAStatus getCarrierFreqs(
            std::vector<::aidl::android::hardware::ir::ConsumerIrFreqRange>* _aidl_return) override;
    ndk::ScopedAStatus transmit(int32_t carrierFreqHz,
                                const std::vector<int32_t>& pattern) override;

  private:
    bool isInRange(int32_t carrierFreqHz);
};

}  // namespace ir
}  // namespace hardware
}  // namespace android
}  // namespace aidl
