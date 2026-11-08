/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

namespace aidl::android::hardware::gnss::protocol {

class Writer {
  public:
    template <typename T>
    Writer& put(T value) {
        static_assert(std::is_arithmetic_v<T> && sizeof(T) <= sizeof(uint64_t));
        uint64_t bits = 0;
        if constexpr (std::is_floating_point_v<T>) {
            if constexpr (sizeof(T) == 4) {
                uint32_t word;
                std::memcpy(&word, &value, sizeof(value));
                bits = word;
            } else {
                std::memcpy(&bits, &value, sizeof(value));
            }
        } else {
            bits = static_cast<uint64_t>(value);
        }
        for (size_t i = 0; i < sizeof(T); ++i) mData.push_back(bits >> (8 * i));
        return *this;
    }
    Writer& u8(uint8_t value) { return put(value); }
    Writer& u16(uint16_t value) { return put(value); }
    Writer& u32(uint32_t value) { return put(value); }
    Writer& f32(float value) { return put(value); }
    Writer& f64(double value) { return put(value); }
    const std::vector<uint8_t>& data() const { return mData; }

  private:
    std::vector<uint8_t> mData;
};

class Reader {
  public:
    explicit Reader(const std::vector<uint8_t>& data) : mData(data) {}
    template <typename T>
    T get() {
        static_assert(std::is_arithmetic_v<T> && sizeof(T) <= sizeof(uint64_t));
        if (sizeof(T) > remaining()) {
            fail();
            return {};
        }
        uint64_t bits = 0;
        for (size_t i = 0; i < sizeof(T); ++i) bits |= uint64_t(mData[mPosition++]) << (8 * i);
        if constexpr (std::is_floating_point_v<T>) {
            T value;
            if constexpr (sizeof(T) == 4) {
                uint32_t word = bits;
                std::memcpy(&value, &word, sizeof(value));
            } else {
                std::memcpy(&value, &bits, sizeof(value));
            }
            return value;
        } else {
            return static_cast<T>(bits);
        }
    }
    uint8_t u8() { return get<uint8_t>(); }
    uint16_t u16() { return get<uint16_t>(); }
    uint32_t u32() { return get<uint32_t>(); }
    float f32() { return get<float>(); }
    double f64() { return get<double>(); }
    std::string string(size_t maximum);
    void skip(size_t size);
    bool ok() const { return mOk; }
    size_t remaining() const { return mData.size() - mPosition; }

  private:
    void fail() {
        mOk = false;
        mPosition = mData.size();
    }
    const std::vector<uint8_t>& mData;
    size_t mPosition = 0;
    bool mOk = true;
};

std::vector<uint8_t> makeStartRequest();
int readConfigInt(const char* key, int fallback);

}  // namespace aidl::android::hardware::gnss::protocol

namespace aidl::android::hardware::gnss {

using protocol::readConfigInt;
using protocol::Reader;
using protocol::Writer;

}  // namespace aidl::android::hardware::gnss
