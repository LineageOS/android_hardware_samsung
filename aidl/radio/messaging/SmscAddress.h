/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include <string_view>

namespace aidl::android::hardware::radio::samsung {

// GET returns a dialable address; SEND takes a BCD SCA.
bool normalizeSmscAddress(std::string_view input, std::string* address);
bool normalizeSmscPdu(std::string_view input, std::string* pdu);

}  // namespace aidl::android::hardware::radio::samsung
