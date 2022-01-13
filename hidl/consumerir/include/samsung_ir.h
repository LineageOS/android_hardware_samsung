/*
 * Copyright (C) 2016 The CyanogenMod Project
 *               2020-2022 The LineageOS Project
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

#include <android/hardware/ir/1.0/IConsumerIr.h>

using android::hardware::ir::V1_0::ConsumerIrFreqRange;

/*
 * Board specific nodes
 *
 * If your kernel exposes these controls in another place, you can either
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
//#define MS_IR_SIGNAL

static const std::vector<ConsumerIrFreqRange> consumerirFreqs = {
    {.min = 30000, .max = 30000},
    {.min = 33000, .max = 33000},
    {.min = 36000, .max = 36000},
    {.min = 38000, .max = 38000},
    {.min = 40000, .max = 40000},
    {.min = 56000, .max = 56000},
};
