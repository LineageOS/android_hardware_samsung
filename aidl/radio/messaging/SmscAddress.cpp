/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "messaging/SmscAddress.h"

namespace aidl::android::hardware::radio::samsung {
namespace {

constexpr size_t kMaxDigits = 20;
constexpr size_t kMaxInput = 128;

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }
    return text;
}

int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int byte(std::string_view text, size_t offset) {
    if (offset + 2 > text.size()) return -1;
    int high = nibble(text[offset]);
    int low = nibble(text[offset + 1]);
    return high < 0 || low < 0 ? -1 : (high << 4) | low;
}

bool numericToa(int toa) {
    return toa >= 0x80 && toa <= 0xff && (toa & 0x70) != 0x50 && (toa & 0x70) != 0x70;
}

bool decodePdu(std::string_view text, std::string* digits, int* toa) {
    int length = byte(text, 0);
    int type = byte(text, 2);
    if (length < 2 || length > 11 || text.size() != size_t(length + 1) * 2 || !numericToa(type))
        return false;

    std::string number;
    for (size_t i = 4; i < text.size(); i += 2) {
        int value = byte(text, i);
        if (value < 0 || (value & 15) > 9) return false;
        number += char('0' + (value & 15));
        if ((value >> 4) <= 9) {
            number += char('0' + (value >> 4));
        } else if ((value >> 4) != 15 || i + 2 != text.size()) {
            return false;
        }
    }
    *digits = number;
    *toa = type;
    return true;
}

bool parseAddress(std::string_view text, std::string* digits, int* toa) {
    if (text.empty() || text.size() > kMaxInput) return false;
    text = trim(text);
    if (text.substr(0, 6) == "+CSCA:") text = trim(text.substr(6));

    std::string_view address;
    std::string_view suffix;
    if (!text.empty() && text.front() == '"') {
        size_t end = text.find('"', 1);
        if (end == std::string_view::npos) return false;
        address = text.substr(1, end - 1);
        suffix = trim(text.substr(end + 1));
    } else {
        size_t comma = text.find(',');
        address = trim(text.substr(0, comma));
        if (comma != std::string_view::npos) suffix = text.substr(comma);
    }

    int type = -1;
    if (!suffix.empty()) {
        if (suffix.front() != ',') return false;
        suffix = trim(suffix.substr(1));
        if (suffix.empty() || suffix.size() > 3) return false;
        type = 0;
        for (char c : suffix) {
            if (c < '0' || c > '9') return false;
            type = type * 10 + c - '0';
        }
        if (!numericToa(type)) return false;
    }

    if (decodePdu(address, digits, toa)) return true;
    bool international = !address.empty() && address.front() == '+';
    if (international) address.remove_prefix(1);
    if (address.empty() || address.size() > kMaxDigits) return false;
    for (char c : address) {
        if (c < '0' || c > '9') return false;
    }
    *digits = std::string(address);
    // Samsung omits the TON of international numbers.
    *toa = international || type == -1 ? 0x91 : type;
    return true;
}

int normalizeToa(int toa) {
    return (toa & 0x70) == 0 ? toa | 0x10 : toa;
}

void appendByte(std::string* output, int value) {
    constexpr char hex[] = "0123456789ABCDEF";
    *output += hex[value >> 4];
    *output += hex[value & 15];
}

}  // namespace

bool normalizeSmscAddress(std::string_view input, std::string* address) {
    std::string digits;
    int toa;
    if (!parseAddress(input, &digits, &toa)) return false;
    *address = (normalizeToa(toa) & 0x70) == 0x10 ? "+" + digits : digits;
    return true;
}

bool normalizeSmscPdu(std::string_view input, std::string* pdu) {
    // An empty SCA selects the SIM's default SMSC.
    if (input.empty() || input == "00") {
        *pdu = std::string(input);
        return true;
    }
    std::string digits;
    int toa;
    if (decodePdu(input, &digits, &toa)) {
        *pdu = std::string(input);
        return true;
    }
    // Only text is repaired; an invalid PDU is rejected.
    auto text = trim(input);
    if (text.empty() ||
        (text.front() != '+' && text.front() != '"' && text.find(',') == std::string_view::npos))
        return false;
    if (!parseAddress(text, &digits, &toa)) return false;
    pdu->clear();
    appendByte(pdu, 1 + (digits.size() + 1) / 2);
    appendByte(pdu, normalizeToa(toa));
    for (size_t i = 0; i < digits.size(); i += 2) {
        int high = i + 1 < digits.size() ? digits[i + 1] - '0' : 15;
        appendByte(pdu, (high << 4) | (digits[i] - '0'));
    }
    return true;
}

}  // namespace aidl::android::hardware::radio::samsung
