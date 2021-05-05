/*
 * Copyright 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <aidl/android/hardware/power/BnPowerHintSession.h>
#include <aidl/android/hardware/power/WorkDuration.h>
#include <utils/Looper.h>
#include <utils/Thread.h>

#include <mutex>
#include <unordered_map>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using aidl::android::hardware::power::BnPowerHintSession;
using aidl::android::hardware::power::WorkDuration;
using ::android::Message;
using ::android::MessageHandler;
using ::android::sp;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::steady_clock;
using std::chrono::time_point;

struct AppHintDesc {
    AppHintDesc(int32_t tgid, int32_t uid, std::vector<int> threadIds, int uclamp_min)
        : tgid(tgid),
          uid(uid),
          threadIds(std::move(threadIds)),
          duration(0LL),
          current_min(uclamp_min),
          is_active(true),
          update_count(0),
          integral_error(0),
          previous_error(0) {}
    std::string toString() const;
    const int32_t tgid;
    const int32_t uid;
    const std::vector<int> threadIds;
    nanoseconds duration;
    int current_min;
    // status
    std::atomic<bool> is_active;
    // pid
    uint64_t update_count;
    int64_t integral_error;
    int64_t previous_error;
};

class PowerHintSession : public BnPowerHintSession {
  public:
    PowerHintSession(int32_t tgid, int32_t uid, const std::vector<int32_t> &threadIds,
                     int64_t durationNanos);
    ~PowerHintSession();
    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus pause() override;
    ndk::ScopedAStatus resume() override;
    ndk::ScopedAStatus updateTargetWorkDuration(int64_t targetDurationNanos) override;
    ndk::ScopedAStatus reportActualWorkDuration(
            const std::vector<WorkDuration> &actualDurations) override;
    bool isActive();
    bool isStale();

  private:
    class StaleHandler : public MessageHandler {
      public:
        StaleHandler(PowerHintSession *session, int64_t timeout_ms)
            : kStaleTimeout(timeout_ms),
              mSession(session),
              mIsMonitoringStale(false),
              mLastUpdatedTime(steady_clock::now()) {}
        void handleMessage(const Message &message) override;
        void updateStaleTimer();
        time_point<steady_clock> getStaleTime();

      private:
        const milliseconds kStaleTimeout;
        PowerHintSession *mSession;
        std::atomic<bool> mIsMonitoringStale;
        std::atomic<time_point<steady_clock>> mLastUpdatedTime;
        std::mutex mStaleLock;
    };

  private:
    void setStale();
    void updateUniveralBoostMode();
    int setUclamp(int32_t max, int32_t min);
    AppHintDesc *mDescriptor = nullptr;
    sp<StaleHandler> mStaleHandler;
    sp<MessageHandler> mPowerManagerHandler;
    std::mutex mLock;
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
