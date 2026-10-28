/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

RadioResponseInfo responseInfo(int32_t serial, RadioError error) {
    return {
            .type = RadioResponseType::SOLICITED,
            .serial = serial,
            .error = error,
    };
}

RadioResponseInfo responseInfo(int32_t serial, RIL_Errno error, bool valid) {
    if (error == RIL_E_SUCCESS && !valid) {
        LOG(ERROR) << '[' << serial << "] invalid response";
        return responseInfo(serial, RadioError::INVALID_RESPONSE);
    }
    return responseInfo(serial, static_cast<RadioError>(error));
}

RadioResponseInfo notSupported(int32_t serial) {
    return responseInfo(serial, RadioError::REQUEST_NOT_SUPPORTED);
}

std::string toString(const char* str) {
    return str != nullptr ? str : "";
}

std::optional<int32_t> responseInt(const void* data, size_t length) {
    if (data == nullptr || length < sizeof(int)) {
        return std::nullopt;
    }
    return *static_cast<const int*>(data);
}

std::vector<int32_t> responseInts(const void* data, size_t length) {
    if (data == nullptr) {
        return {};
    }
    const auto* ints = static_cast<const int*>(data);
    return {ints, ints + length / sizeof(int)};
}

}  // namespace aidl::android::hardware::radio::samsung
