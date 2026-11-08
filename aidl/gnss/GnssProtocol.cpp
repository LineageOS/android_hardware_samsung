/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "GnssProtocol.h"

#include <cstdio>
#include <map>

namespace aidl::android::hardware::gnss::protocol {

std::string Reader::string(size_t maximum) {
    const auto size = std::min(maximum, remaining());
    const auto begin = mData.begin() + mPosition;
    const auto end = std::find(begin, begin + size, 0);
    if (end == begin + size) {
        fail();
        return {};
    }
    std::string value(begin, end);
    mPosition += value.size() + 1;
    return value;
}

void Reader::skip(size_t size) {
    if (size > remaining())
        fail();
    else
        mPosition += size;
}

std::vector<uint8_t> makeStartRequest() {
    return std::vector<uint8_t>(12);
}

namespace {

using Config = std::map<std::string, std::vector<std::string>>;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n\v\f");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n\v\f") - first + 1);
}

const Config& config() {
    static const auto values = [] {
        Config result;
        FILE* input = std::fopen("/vendor/etc/gnss/gps.cfg", "r");
        if (!input) return result;
        char buffer[1024];
        while (std::fgets(buffer, sizeof(buffer), input)) {
            const auto line = trim(buffer);
            if (line.empty() || line.front() == '#') continue;
            const auto equal = line.find('=');
            if (equal == std::string::npos) continue;
            const auto key = trim(line.substr(0, equal));
            if (key.empty()) continue;
            auto value = trim(line.substr(equal + 1));
            if (value == "TRUE")
                value = "1";
            else if (value == "FALSE")
                value = "0";
            result[key].push_back(std::move(value));
        }
        std::fclose(input);
        return result;
    }();
    return values;
}

const std::string* numericValue(const char* key) {
    const auto& values = config();
    const auto entry = values.find(key);
    if (entry == values.end()) return nullptr;
    if (entry->second.size() != 1) return nullptr;
    return &entry->second.front();
}

}  // namespace

int readConfigInt(const char* key, int fallback) {
    const auto* text = numericValue(key);
    int value;
    return text && std::sscanf(text->c_str(), "%i", &value) == 1 ? value : fallback;
}

}  // namespace aidl::android::hardware::gnss::protocol
