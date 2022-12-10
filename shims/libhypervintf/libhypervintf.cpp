/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string>
#include <vector>

struct HyPerToken {
};

void hyper_getNewHyPerToken(void) {
}

void hyper_setTokenProcName(HyPerToken*, std::string) {
}

void hyper_checkResourceExist(int) {
}

void hyper_acquire(HyPerToken, std::vector<int>) {
}

void hyper_setProcName(std::string) {
}

void hyper_getSupportedFrequency(int, int) {
}

void hyper_release(HyPerToken) {
}
