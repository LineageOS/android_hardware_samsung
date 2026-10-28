/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungVoice"

#include "voice/CallStatePublisher.h"

#include <android-base/logging.h>
#include <android-base/properties.h>

#include <chrono>
#include <string>

#include "voice/AudioCallSlot.h"

namespace aidl::android::hardware::radio::samsung {

namespace {

constexpr char kCallStateProperty[] = "vendor.calls.state";
constexpr size_t kMaxCalls = 64;

bool readCallStates(const void* data, size_t length, unsigned* mask) {
    if (length % sizeof(SecCall*) != 0 || length > kMaxCalls * sizeof(SecCall*) ||
        (length != 0 && data == nullptr)) {
        return false;
    }
    const auto* calls = static_cast<SecCall* const*>(data);
    unsigned states = 0;
    for (size_t i = 0; i < length / sizeof(SecCall*); i++) {
        const auto* call = calls[i];
        if (call == nullptr || call->state < RIL_CALL_ACTIVE || call->state > RIL_CALL_WAITING) {
            return false;
        }
        if (call->isVoice != 0) {
            states |= 1u << call->state;
        }
    }
    *mask = states;
    return true;
}

}  // namespace

CallStatePublisher& CallStatePublisher::get() {
    static CallStatePublisher instance;
    return instance;
}

CallStatePublisher::CallStatePublisher()
    : mGeneration(std::chrono::steady_clock::now().time_since_epoch().count()) {
    std::lock_guard lock(mMutex);
    publishLocked();
}

bool CallStatePublisher::valid(RIL_SOCKET_ID socket) const {
    return socket >= 0 && static_cast<size_t>(socket) < kSlots;
}

CallStatePublisher::Query CallStatePublisher::beginQuery(RIL_SOCKET_ID socket) {
    std::lock_guard lock(mMutex);
    return {valid(socket) ? mSlotGeneration[socket] : 0, ++mSerial};
}

void CallStatePublisher::completeQuery(RIL_SOCKET_ID socket, const Query& query, RIL_Errno error,
                                       const void* data, size_t length) {
    std::optional<int> audioSlot;
    {
        std::lock_guard lock(mMutex);
        if (!valid(socket) || query.generation != mSlotGeneration[socket] ||
            query.serial <= mLastSerial[socket]) {
            return;
        }
        mLastSerial[socket] = query.serial;
        if (error == RIL_E_RADIO_NOT_AVAILABLE) {
            resetLocked(socket);
            return;
        }
        unsigned mask;
        if (error != RIL_E_SUCCESS || !readCallStates(data, length, &mask)) {
            return;
        }
        mSlots[socket] = mask;
        // Publish unchanged masks too, so a new voice session can start.
        publishLocked();
        audioSlot = updateAudioSlotLocked();
    }
    if (audioSlot.has_value() && !AudioCallSlot::get().set(*audioSlot)) {
        std::lock_guard lock(mMutex);
        if (mAudioSlot == *audioSlot) {
            mAudioSlot = -1;
        }
    }
}

void CallStatePublisher::invalidate(RIL_SOCKET_ID socket) {
    std::lock_guard lock(mMutex);
    if (valid(socket)) {
        ++mSlotGeneration[socket];
    }
}

void CallStatePublisher::reset(RIL_SOCKET_ID socket) {
    std::lock_guard lock(mMutex);
    if (valid(socket)) {
        resetLocked(socket);
    }
}

void CallStatePublisher::resetAll() {
    std::lock_guard lock(mMutex);
    for (auto& generation : mSlotGeneration) {
        ++generation;
    }
    mSlots = {};
    mAudioSlot = -1;
    publishLocked();
}

void CallStatePublisher::resetLocked(RIL_SOCKET_ID socket) {
    ++mSlotGeneration[socket];
    mSlots[socket] = 0;
    if (mAudioSlot == socket) {
        mAudioSlot = -1;
    }
    publishLocked();
}

std::optional<int> CallStatePublisher::updateAudioSlotLocked() {
    constexpr unsigned kForeground =
            1u << RIL_CALL_ACTIVE | 1u << RIL_CALL_DIALING | 1u << RIL_CALL_ALERTING;
    int slot = -1;
    // A held call keeps its slot until another slot has a foreground call.
    if (mAudioSlot >= 0 && (mSlots[mAudioSlot] & kForeground)) {
        slot = mAudioSlot;
    } else {
        for (size_t i = 0; i < kSlots && slot < 0; i++) {
            if (mSlots[i] & kForeground) {
                slot = i;
            }
        }
        if (slot < 0 && mAudioSlot >= 0 && (mSlots[mAudioSlot] & 1u << RIL_CALL_HOLDING)) {
            slot = mAudioSlot;
        }
    }
    if (slot == mAudioSlot) {
        return std::nullopt;
    }
    mAudioSlot = slot;
    return slot >= 0 ? std::optional<int>(slot) : std::nullopt;
}

void CallStatePublisher::publishLocked() {
    ++mSequence;
    const auto value = "1:" + std::to_string(mGeneration) + ":" + std::to_string(mSequence) + ":" +
                       std::to_string(mSlots[0]) + ":" + std::to_string(mSlots[1]);
    if (!::android::base::SetProperty(kCallStateProperty, value)) {
        LOG(ERROR) << "Failed to publish " << kCallStateProperty;
    }
}

}  // namespace aidl::android::hardware::radio::samsung
