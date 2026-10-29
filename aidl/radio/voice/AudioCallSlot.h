/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <android/hardware/audio/7.0/IPrimaryDevice.h>

#include <mutex>

namespace aidl::android::hardware::radio::samsung {

// Selects the SIM for CS call audio and its voice session. Set before answering
// GET_CURRENT_CALLS, which Telephony waits for before switching audio to IN_CALL.
class AudioCallSlot {
  public:
    static AudioCallSlot& get();

    // Moves the voice session to the slot, or ends it for -1.
    bool set(int slot);

  private:
    AudioCallSlot() = default;
    ::android::sp<::android::hardware::audio::V7_0::IDevice> device();
    bool setParameters(
            const ::android::hardware::hidl_vec<::android::hardware::audio::V7_0::ParameterValue>&
                    parameters);

    std::mutex mMutex;
    ::android::sp<::android::hardware::audio::V7_0::IDevice> mDevice;
    // -2 after a partial update, so that the next one clears both sessions.
    int mSlot = -1;
};

}  // namespace aidl::android::hardware::radio::samsung
