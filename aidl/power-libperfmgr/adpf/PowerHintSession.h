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

#include <unordered_map>

#include <aidl/android/hardware/power/BnPowerHintSession.h>
#include <aidl/android/hardware/power/WorkDuration.h>
#include <utils/Mutex.h>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {
namespace adpf {

using aidl::android::hardware::power::WorkDuration;
using std::chrono::nanoseconds;

struct AppHintDesc {
    AppHintDesc(int32_t tgid, int32_t uid, std::vector<int> threadIds)
        : tgid(tgid),
          uid(uid),
          threadIds(std::move(threadIds)),
          duration(0LL),
          current_min(0),
          current_max(1024),
          is_active(true) {}
    std::string toString() const;
    const int32_t tgid;
    const int32_t uid;
    const std::vector<int> threadIds;
    nanoseconds duration;
    int current_min;
    int current_max;
    bool apply;
    float dur_scale;
    float tolerance;
    bool is_active;
};

class PowerHintSession : public ::aidl::android::hardware::power::BnPowerHintSession {
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

  private:
    static int setUclamp(int32_t tid, int32_t max, int32_t min);
    AppHintDesc *mDescriptor = nullptr;
};

}  // namespace adpf
}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
