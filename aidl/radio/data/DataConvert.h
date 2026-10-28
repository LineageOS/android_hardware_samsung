/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/data/SetupDataCallResult.h>
#include <aidl/android/hardware/radio/data/SlicingConfig.h>
#include <samsung_ril/data.h>

#include <optional>
#include <vector>

namespace aidl::android::hardware::radio::samsung {

data::SetupDataCallResult toSetupDataCallResult(const SecDataCallResponse& response);
std::optional<std::vector<data::SetupDataCallResult>> toDataCallList(const void* data,
                                                                     size_t length);
std::optional<data::SlicingConfig> toSlicingConfig(const void* data, size_t length);

}  // namespace aidl::android::hardware::radio::samsung
