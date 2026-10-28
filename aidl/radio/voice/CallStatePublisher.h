/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <samsung_ril/voice.h>

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>

namespace aidl::android::hardware::radio::samsung {

// Publishes each SIM's CS call states for the audio HAL as
// "1:<generation>:<sequence>:<slot1 mask>:<slot2 mask>".
class CallStatePublisher {
  public:
    struct Query {
        uint64_t generation;
        uint64_t serial;
    };

    static CallStatePublisher& get();

    Query beginQuery(RIL_SOCKET_ID socket);
    void completeQuery(RIL_SOCKET_ID socket, const Query& query, RIL_Errno error, const void* data,
                       size_t length);

    void invalidate(RIL_SOCKET_ID socket);
    void reset(RIL_SOCKET_ID socket);
    void resetAll();

  private:
    static constexpr size_t kSlots = 2;

    CallStatePublisher();
    bool valid(RIL_SOCKET_ID socket) const;
    void resetLocked(RIL_SOCKET_ID socket);
    void publishLocked();
    std::optional<int> updateAudioSlotLocked();

    std::mutex mMutex;
    uint64_t mGeneration;
    uint64_t mSequence = 0;
    uint64_t mSerial = 0;
    std::array<unsigned, kSlots> mSlots{};
    std::array<uint64_t, kSlots> mSlotGeneration{};
    std::array<uint64_t, kSlots> mLastSerial{};
    int mAudioSlot = -1;
};

}  // namespace aidl::android::hardware::radio::samsung
