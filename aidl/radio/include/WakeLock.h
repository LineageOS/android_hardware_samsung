/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>

namespace aidl::android::hardware::radio::samsung {

class WakeLock {
  public:
    static WakeLock& get();

    void acquire();
    void release();

  private:
    WakeLock();
    void timeoutLoop();
    void releaseAllLocked();

    std::mutex mMutex;
    std::condition_variable mCondition;
    unsigned mCount = 0;
    std::optional<std::chrono::steady_clock::time_point> mDeadline;
};

}  // namespace aidl::android::hardware::radio::samsung
