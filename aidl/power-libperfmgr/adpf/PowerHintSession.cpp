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

#define LOG_TAG "powerhal-libperfmgr"
#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <log/log.h>
#include <utils/Trace.h>

#include <sys/syscall.h>

#include "PowerHintSession.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {
namespace adpf {

namespace {
/* there is no glibc or bionic wrapper */
struct sched_attr {
    __u32 size;
    __u32 sched_policy;
    __u64 sched_flags;
    __s32 sched_nice;
    __u32 sched_priority;
    __u64 sched_runtime;
    __u64 sched_deadline;
    __u64 sched_period;
    __u32 sched_util_min;
    __u32 sched_util_max;
};

int sched_setattr(int pid, struct sched_attr *attr, unsigned int flags) {
    return syscall(__NR_sched_setattr, pid, attr, flags);
}
}  // namespace

using ::android::base::StringPrintf;

PowerHintSession::PowerHintSession(int32_t tgid, int32_t uid, const std::vector<int32_t> &threadIds,
                                   int64_t durationNanos) {
    mDescriptor = new AppHintDesc(tgid, uid, threadIds);
    // TODO(jimmyshiu@): use better control algorithms
    mDescriptor->duration = std::chrono::nanoseconds(durationNanos);

    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-target", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), (int64_t)mDescriptor->duration.count());
        sz = StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-active", mDescriptor->tgid,
                          mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), mDescriptor->is_active);
    }
    ALOGV("PowerHintSession created: %s", mDescriptor->toString().c_str());
}

PowerHintSession::~PowerHintSession() {
    close();
    ALOGV("PowerHintSession deleted: %s", mDescriptor->toString().c_str());
    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-target", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), 0);
        sz = StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-min", mDescriptor->tgid,
                          mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), 0);
        sz = StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-max", mDescriptor->tgid,
                          mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), 0);
        sz = StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-actl_last", mDescriptor->tgid,
                          mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), 0);
    }
    delete mDescriptor;
}

int PowerHintSession::setUclamp(int32_t tid, int32_t min, int32_t max) {
    sched_attr attr = sched_attr();
    attr.size = sizeof(attr);

    attr.sched_flags = (SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP);
    attr.sched_util_min = min;
    attr.sched_util_max = max;

    int ret = sched_setattr(tid, &attr, 0);
    if (ret) {
        ALOGW("sched_setattr failed for thread %d, err=%d", tid, ret);
        return ret;
    }
    ALOGV("PowerHintSession tid: %d, uclamp(%d, %d)", tid, min, max);
    return 0;
}

ndk::ScopedAStatus PowerHintSession::pause() {
    if (!mDescriptor->is_active)
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    for (const auto tid : mDescriptor->threadIds) {
        setUclamp(tid, 0, 1024);
    }
    mDescriptor->is_active = false;
    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-active", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), mDescriptor->is_active);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSession::resume() {
    if (mDescriptor->is_active)
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    for (const auto tid : mDescriptor->threadIds) {
        setUclamp(tid, mDescriptor->current_min, mDescriptor->current_max);
    }
    mDescriptor->is_active = true;
    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-active", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), mDescriptor->is_active);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSession::close() {
    for (const auto tid : mDescriptor->threadIds) {
        setUclamp(tid, 0, 1024);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSession::updateTargetWorkDuration(int64_t targetDurationNanos) {
    if (targetDurationNanos <= 0) {
        ALOGE("Error: targetDurationNanos(%" PRId64 ") should bigger than 0", targetDurationNanos);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    ALOGV("update target duration: %" PRId64 " ~ %" PRId64 " ns", targetDurationNanos,
          (int64_t)(targetDurationNanos * (1.0 - mDescriptor->tolerance)));
    /* TODO(jimmyshiu@): if we have some good history then we should probably adjust
     * the utilization limits based on the difference between the old and
     * new desired durations. For now, we just do nothing and it will resettle
     * after a few frames.
     */
    mDescriptor->duration = std::chrono::nanoseconds(targetDurationNanos);

    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-target", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), (int64_t)mDescriptor->duration.count());
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSession::reportActualWorkDuration(
        const std::vector<WorkDuration> &actualDurations) {
    if (mDescriptor->duration.count() == 0LL) {
        ALOGE("Expect to call updateTargetWorkDuration() first.");
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    if (actualDurations.size() == 0) {
        ALOGE("Error: duration.size() shouldn't be %zu.", actualDurations.size());
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (!mDescriptor->is_active) {
        ALOGE("Error: shouldn't report duration during pause state.");
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    int64_t actualDurationNanos = actualDurations.back().durationNanos;

    constexpr int step_size = 4;
    float target_duration =
            static_cast<float>(mDescriptor->duration.count() * mDescriptor->dur_scale);
    float z = static_cast<float>((actualDurationNanos - target_duration) / target_duration);
    if (z > 0) {
        mDescriptor->current_min = std::min(1024, mDescriptor->current_min + step_size);
        mDescriptor->current_max = std::max(mDescriptor->current_min, mDescriptor->current_max);
    } else if (z < -mDescriptor->tolerance) {
        mDescriptor->current_max = std::max(0, mDescriptor->current_max - step_size);
        mDescriptor->current_min = std::min(mDescriptor->current_min, mDescriptor->current_max);
    } else {
        return ndk::ScopedAStatus::ok();
    }

    if (ATRACE_ENABLED()) {
        std::string sz = StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-min", mDescriptor->tgid,
                                      mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), mDescriptor->current_min);
        sz = StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-max", mDescriptor->tgid,
                          mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), mDescriptor->current_max);
        sz = StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR "-actl_last", mDescriptor->tgid,
                          mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), actualDurationNanos);
    }

    /* apply to all the threads in the group */
    for (const auto tid : mDescriptor->threadIds) {
        setUclamp(tid, mDescriptor->current_min, mDescriptor->current_max);
    }
    return ndk::ScopedAStatus::ok();
}

std::string AppHintDesc::toString() const {
    std::string out =
            StringPrintf("session %" PRIxPTR "\n", reinterpret_cast<uintptr_t>(this) & 0xffff);
    const int64_t durationNanos = duration.count();
    out.append(StringPrintf("  duration: %" PRId64 " ~ %" PRId64 " ns\n", durationNanos,
                            (int64_t)(durationNanos * (1.0 - tolerance))));

    out.append("  threadIds: [");
    bool first = true;
    for (int tid : threadIds) {
        if (!first) {
            out.append(", ");
        }
        out.append(std::to_string(tid));
        first = false;
    }
    out.append("]\n");
    return out;
}

}  // namespace adpf
}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
