/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/RadioError.h>
#include <aidl/android/hardware/radio/RadioIndicationType.h>
#include <aidl/android/hardware/radio/RadioResponseInfo.h>
#include <android-base/logging.h>
#include <android/binder_auto_utils.h>
#include <samsung_ril/structs.h>

#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace aidl::android::hardware::radio::samsung {

class RadioSlot;

RadioResponseInfo responseInfo(int32_t serial, RadioError error);
RadioResponseInfo responseInfo(int32_t serial, RIL_Errno error, bool valid = true);
RadioResponseInfo notSupported(int32_t serial);

constexpr RadioIndicationType kUnsolicited = RadioIndicationType::UNSOLICITED;

std::string toString(const char* str);

template <size_t N>
std::string fixedString(const char (&str)[N]) {
    return std::string(str, strnlen(str, N));
}

inline char* cString(const std::string& str) {
    return str.empty() ? nullptr : const_cast<char*>(str.c_str());
}

template <typename T>
const T* responseAs(const void* data, size_t length) {
    if (data == nullptr) {
        return nullptr;
    }
    if (length != sizeof(T)) {
        LOG(ERROR) << "Payload is " << length << " bytes, expected " << sizeof(T);
        return nullptr;
    }
    return static_cast<const T*>(data);
}

std::optional<int32_t> responseInt(const void* data, size_t length);
std::vector<int32_t> responseInts(const void* data, size_t length);

inline ::ndk::ScopedAStatus ok() {
    return ::ndk::ScopedAStatus::ok();
}

using ResponseCallback = std::function<void(RIL_Errno error, const void* data, size_t length)>;

template <typename Response, typename Indication>
class RadioService {
  public:
    virtual ~RadioService() = default;

  protected:
    template <typename Method>
    ResponseCallback statusResponse(int32_t serial, Method method) {
        return [this, serial, method](RIL_Errno error, const void*, size_t) {
            if (const auto response = respond()) {
                (response.get()->*method)(responseInfo(serial, error));
            }
        };
    }

    template <typename Value, typename Method>
    ResponseCallback intResponse(int32_t serial, Method method) {
        return [this, serial, method](RIL_Errno error, const void* data, size_t length) {
            const auto value = responseInt(data, length);
            if (const auto response = respond()) {
                (response.get()->*method)(responseInfo(serial, error, value.has_value()),
                                          static_cast<Value>(value.value_or(0)));
            }
        };
    }

    std::shared_ptr<Response> respond() {
        std::lock_guard lock(mMutex);
        return mResponse;
    }

    std::shared_ptr<Indication> indicate() {
        std::lock_guard lock(mMutex);
        return mIndication;
    }

    void setCallbacks(const std::shared_ptr<Response>& response,
                      const std::shared_ptr<Indication>& indication) {
        std::lock_guard lock(mMutex);
        mResponse = response;
        mIndication = indication;
    }

  private:
    std::mutex mMutex;
    std::shared_ptr<Response> mResponse;
    std::shared_ptr<Indication> mIndication;
};

}  // namespace aidl::android::hardware::radio::samsung

#define LOG_CALL LOG(VERBOSE) << '[' << serial << "] " << __func__
#define LOG_NOT_SUPPORTED LOG(DEBUG) << '[' << serial << "] " << __func__ << " (not supported)"
