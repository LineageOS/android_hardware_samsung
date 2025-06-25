
/*
 * Copyright (C) 2022-2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <compositionengine/UdfpsExtension.h>

uint32_t getUdfpsDimZOrder(uint32_t z) {
    z |= UDFPS_DIM_LAYER_ZORDER;
    return z;
}

uint32_t getUdfpsZOrder(uint32_t z, bool touched) {
    if (touched) {
        z |= UDFPS_PRESSED_LAYER_ZORDER;
    }
    return z;
}

#if OVERWRITE_USAGEBITS
uint64_t getUdfpsUsageBits(uint64_t usageBits, bool touched) {
    if (touched) {
        usageBits |= 0x400000000LL;
    }
#else
uint64_t getUdfpsUsageBits(uint64_t usageBits, bool /* touched */) {
    return usageBits;
}
#endif
