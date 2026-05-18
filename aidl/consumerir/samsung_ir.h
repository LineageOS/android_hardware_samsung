/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/ir/ConsumerIrFreqRange.h>
#include <vector>
/*
 * Board specific nodes
 *
 * If your device exposes these controls in another place, you can either
 * symlink to the locations given here, or override this header in your
 * device tree.
 */
#define IR_PATH "/sys/class/sec/sec_ir/ir_send"
/*
 * Board specific configs
 *
 * If your device needs a different configuration, you
 * can override this header in your device tree
 */
// Some devices need MS_IR_SIGNAL to avoid ms to pulses conversionn
// #define MS_IR_SIGNAL

namespace aidl::android::hardware::ir {

inline const std::vector<ConsumerIrFreqRange> kCarrierFreqRanges = {
        {30000, 30000}, {33000, 33000}, {36000, 36000},
        {38000, 38000}, {40000, 40000}, {56000, 56000},
};

}  // namespace aidl::android::hardware::ir
