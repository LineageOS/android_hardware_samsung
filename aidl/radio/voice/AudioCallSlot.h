/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/hardware/audio/7.0/IPrimaryDevice.h>

#include <mutex>

namespace aidl::android::hardware::radio::samsung {

// Selects the SIM for CS call audio. Set before answering GET_CURRENT_CALLS, which
// Telephony waits for before switching audio to IN_CALL.
class AudioCallSlot {
  public:
    static AudioCallSlot& get();

    bool set(int slot);

  private:
    AudioCallSlot() = default;
    ::android::sp<::android::hardware::audio::V7_0::IDevice> device();

    std::mutex mMutex;
    ::android::sp<::android::hardware::audio::V7_0::IDevice> mDevice;
};

}  // namespace aidl::android::hardware::radio::samsung
