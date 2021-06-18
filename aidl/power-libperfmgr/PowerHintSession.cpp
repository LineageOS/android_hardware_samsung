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
#include <android-base/parsedouble.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <log/log.h>
#include <time.h>
#include <utils/Trace.h>

#include <sys/syscall.h>

#include "PowerHintSession.h"
#include "PowerSessionManager.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using ::android::base::StringPrintf;
using std::chrono::duration_cast;
using std::chrono::nanoseconds;
using std::literals::chrono_literals::operator""s;

constexpr char kPowerHalAdpfPidOffset[] = "vendor.powerhal.adpf.pid.offset";
constexpr char kPowerHalAdpfPidP[] = "vendor.powerhal.adpf.pid.p";
constexpr char kPowerHalAdpfPidI[] = "vendor.powerhal.adpf.pid.i";
constexpr char kPowerHalAdpfPidIClamp[] = "vendor.powerhal.adpf.pid.i_clamp";
constexpr char kPowerHalAdpfPidD[] = "vendor.powerhal.adpf.pid.d";
constexpr char kPowerHalAdpfPidInitialIntegral[] = "vendor.powerhal.adpf.pid.i_init";
constexpr char kPowerHalAdpfUclampEnable[] = "vendor.powerhal.adpf.uclamp";
constexpr char kPowerHalAdpfUclampCapRatio[] = "vendor.powerhal.adpf.uclamp.cap_ratio";
constexpr char kPowerHalAdpfUclampGranularity[] = "vendor.powerhal.adpf.uclamp.granularity";
constexpr char kPowerHalAdpfStaleTimeFactor[] = "vendor.powerhal.adpf.stale_timeout_factor";
constexpr char kPowerHalAdpfSamplingWindow[] = "vendor.powerhal.adpf.sampling_window";

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

static int sched_setattr(int pid, struct sched_attr *attr, unsigned int flags) {
    static const bool kPowerHalAdpfUclamp =
            ::android::base::GetBoolProperty(kPowerHalAdpfUclampEnable, true);
    if (!kPowerHalAdpfUclamp) {
        ALOGV("PowerHintSession:%s: skip", __func__);
        return 0;
    }
    return syscall(__NR_sched_setattr, pid, attr, flags);
}

static inline void TRACE_ADPF_PID(uintptr_t session_id, int32_t uid, int32_t tgid, uint64_t count,
                                  int64_t err, int64_t integral, int64_t previous, int64_t p,
                                  int64_t i, int64_t d, int32_t output) {
    if (ATRACE_ENABLED()) {
        const std::string idstr = StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR, tgid, uid,
                                               session_id & 0xffff);
        std::string sz = StringPrintf("%s-pid.count", idstr.c_str());
        ATRACE_INT(sz.c_str(), count);
        sz = StringPrintf("%s-pid.err", idstr.c_str());
        ATRACE_INT(sz.c_str(), err);
        sz = StringPrintf("%s-pid.accu", idstr.c_str());
        ATRACE_INT(sz.c_str(), integral);
        sz = StringPrintf("%s-pid.prev", idstr.c_str());
        ATRACE_INT(sz.c_str(), previous);
        sz = StringPrintf("%s-pid.pOut", idstr.c_str());
        ATRACE_INT(sz.c_str(), p);
        sz = StringPrintf("%s-pid.iOut", idstr.c_str());
        ATRACE_INT(sz.c_str(), i);
        sz = StringPrintf("%s-pid.dOut", idstr.c_str());
        ATRACE_INT(sz.c_str(), d);
        sz = StringPrintf("%s-pid.output", idstr.c_str());
        ATRACE_INT(sz.c_str(), output);
    }
}

static inline int64_t ns_to_100us(int64_t ns) {
    return ns / 100000;
}

static double getDoubleProperty(const char *prop, double value) {
    std::string result = ::android::base::GetProperty(prop, std::to_string(value).c_str());
    if (!::android::base::ParseDouble(result.c_str(), &value)) {
        ALOGE("PowerHintSession : failed to parse double in %s", prop);
    }
    return value;
}

static double sPidOffset = getDoubleProperty(kPowerHalAdpfPidOffset, 0.0);
static double sPidP = getDoubleProperty(kPowerHalAdpfPidP, 2.0);
static double sPidI = getDoubleProperty(kPowerHalAdpfPidI, 0.001);
static double sPidD = getDoubleProperty(kPowerHalAdpfPidD, 100.0);
static const int64_t sPidIInit =
        (sPidI == 0) ? 0
                     : static_cast<int64_t>(::android::base::GetIntProperty<int64_t>(
                                                    kPowerHalAdpfPidInitialIntegral, 100) /
                                            sPidI);
static const int64_t sPidIClamp =
        (sPidI == 0) ? 0
                     : std::abs(static_cast<int64_t>(::android::base::GetIntProperty<int64_t>(
                                                             kPowerHalAdpfPidIClamp, 512) /
                                                     sPidI));
static const int sUclampCap =
        static_cast<int>(getDoubleProperty(kPowerHalAdpfUclampCapRatio, 0.5) * 1024);
static const uint32_t sUclampGranularity =
        ::android::base::GetUintProperty<uint32_t>(kPowerHalAdpfUclampGranularity, 5);
static const int64_t sStaleTimeFactor =
        ::android::base::GetIntProperty<int64_t>(kPowerHalAdpfStaleTimeFactor, 20);
static const size_t sSamplingWindow =
        ::android::base::GetUintProperty<size_t>(kPowerHalAdpfSamplingWindow, 0);

}  // namespace

PowerHintSession::PowerHintSession(int32_t tgid, int32_t uid, const std::vector<int32_t> &threadIds,
                                   int64_t durationNanos, const nanoseconds adpfRate)
    : kAdpfRate(adpfRate) {
    mDescriptor = new AppHintDesc(tgid, uid, threadIds, sUclampCap);
    mDescriptor->duration = std::chrono::nanoseconds(durationNanos);
    mStaleHandler = sp<StaleHandler>(new StaleHandler(this));
    mPowerManagerHandler = PowerSessionManager::getInstance();

    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("%s-target", idstr.c_str());
        ATRACE_INT(sz.c_str(), (int64_t)mDescriptor->duration.count());
        sz = StringPrintf("%s-active", idstr.c_str());
        ATRACE_INT(sz.c_str(), mDescriptor->is_active.load());
    }
    PowerSessionManager::getInstance()->addPowerSession(this);
    ALOGV("PowerHintSession created: %s", mDescriptor->toString().c_str());
}

PowerHintSession::~PowerHintSession() {
    close();
    ALOGV("PowerHintSession deleted: %s", mDescriptor->toString().c_str());
    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("%s-target", idstr.c_str());
        ATRACE_INT(sz.c_str(), 0);
        sz = StringPrintf("%s-actl_last", idstr.c_str());
        ATRACE_INT(sz.c_str(), 0);
        sz = sz = StringPrintf("%s-active", idstr.c_str());
        ATRACE_INT(sz.c_str(), 0);
    }
    delete mDescriptor;
}

std::string PowerHintSession::getIdString() const {
    std::string idstr = StringPrintf("%" PRId32 "-%" PRId32 "-%" PRIxPTR, mDescriptor->tgid,
                                     mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
    return idstr;
}

void PowerHintSession::updateUniveralBoostMode() {
    PowerHintMonitor::getInstance()->getLooper()->sendMessage(mPowerManagerHandler, NULL);
}

int PowerHintSession::setUclamp(int32_t min, int32_t max) {
    std::lock_guard<std::mutex> guard(mLock);
    min = std::max(0, min);
    min = std::min(min, max);
    max = std::max(0, max);
    max = std::max(min, max);
    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-min", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), min);
    }
    for (const auto tid : mDescriptor->threadIds) {
        sched_attr attr = {};
        attr.size = sizeof(attr);

        attr.sched_flags = (SCHED_FLAG_KEEP_ALL | SCHED_FLAG_UTIL_CLAMP);
        attr.sched_util_min = min;
        attr.sched_util_max = max;

        int ret = sched_setattr(tid, &attr, 0);
        if (ret) {
            ALOGW("sched_setattr failed for thread %d, err=%d", tid, errno);
        }
        ALOGV("PowerHintSession tid: %d, uclamp(%d, %d)", tid, min, max);
    }
    return 0;
}

ndk::ScopedAStatus PowerHintSession::pause() {
    if (!mDescriptor->is_active.load())
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    // Reset to default uclamp value.
    setUclamp(0, 1024);
    mDescriptor->is_active.store(false);
    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-active", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), mDescriptor->is_active.load());
    }
    updateUniveralBoostMode();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSession::resume() {
    if (mDescriptor->is_active.load())
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    mDescriptor->is_active.store(true);
    mDescriptor->integral_error = std::max(sPidIInit, mDescriptor->integral_error);
    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-active", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), mDescriptor->is_active.load());
    }
    updateUniveralBoostMode();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSession::close() {
    PowerHintMonitor::getInstance()->getLooper()->removeMessages(mStaleHandler);
    // Reset to (0, 1024) uclamp value -- instead of threads' original setting.
    setUclamp(0, 1024);
    PowerSessionManager::getInstance()->removePowerSession(this);
    updateUniveralBoostMode();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSession::updateTargetWorkDuration(int64_t targetDurationNanos) {
    if (targetDurationNanos <= 0) {
        ALOGE("Error: targetDurationNanos(%" PRId64 ") should bigger than 0", targetDurationNanos);
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    ALOGV("update target duration: %" PRId64 " ns", targetDurationNanos);
    double ratio =
            targetDurationNanos == 0 ? 1.0 : mDescriptor->duration.count() / targetDurationNanos;
    mDescriptor->integral_error =
            std::max(sPidIInit, static_cast<int64_t>(mDescriptor->integral_error * ratio));

    mDescriptor->duration = std::chrono::nanoseconds(targetDurationNanos);
    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-target", mDescriptor->tgid,
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
    if (!mDescriptor->is_active.load()) {
        ALOGE("Error: shouldn't report duration during pause state.");
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    if (PowerHintMonitor::getInstance()->isRunning() && isStale()) {
        if (ATRACE_ENABLED()) {
            std::string sz = StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-stale",
                                          mDescriptor->tgid, mDescriptor->uid,
                                          reinterpret_cast<uintptr_t>(this) & 0xffff);
            ATRACE_INT(sz.c_str(), 0);
        }
        mDescriptor->integral_error = std::max(sPidIInit, mDescriptor->integral_error);
    }
    int64_t targetDurationNanos = (int64_t)mDescriptor->duration.count();
    size_t length = actualDurations.size();
    size_t start = sSamplingWindow == 0 || sSamplingWindow > length ? 0 : length - sSamplingWindow;
    int64_t dt = ns_to_100us(targetDurationNanos);
    int64_t err_sum = 0;
    int64_t derivative_sum = 0;
    for (size_t i = start; i < length; i++) {
        int64_t actualDurationNanos = actualDurations[i].durationNanos;
        if (std::abs(actualDurationNanos) > targetDurationNanos * 20) {
            ALOGW("The actual duration is way far from the target (%" PRId64 " >> %" PRId64 ")",
                  actualDurationNanos, targetDurationNanos);
        }
        // PID control algorithm
        int64_t error = ns_to_100us(actualDurationNanos - targetDurationNanos) +
                        static_cast<int64_t>(sPidOffset);
        derivative_sum += error - mDescriptor->previous_error;
        err_sum += error;
        mDescriptor->previous_error = error;
        mDescriptor->integral_error = mDescriptor->integral_error + error * dt;
        mDescriptor->integral_error = std::min(sPidIClamp, mDescriptor->integral_error);
        mDescriptor->integral_error = std::max(-sPidIClamp, mDescriptor->integral_error);
    }
    if (ATRACE_ENABLED()) {
        std::string sz = StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-actl_last",
                                      mDescriptor->tgid, mDescriptor->uid,
                                      reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), actualDurations[length - 1].durationNanos);
        sz = StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-target", mDescriptor->tgid,
                          mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), (int64_t)mDescriptor->duration.count());
        sz = StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-sample_size",
                          mDescriptor->tgid, mDescriptor->uid,
                          reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), length);
    }
    int64_t pOut = static_cast<int64_t>(sPidP * err_sum / (length - start));
    int64_t iOut = static_cast<int64_t>(sPidI * mDescriptor->integral_error);
    int64_t dOut = static_cast<int64_t>(sPidD * derivative_sum / dt / (length - start));

    int64_t output = pOut + iOut + dOut;
    TRACE_ADPF_PID(reinterpret_cast<uintptr_t>(this) & 0xffff, mDescriptor->uid, mDescriptor->tgid,
                   mDescriptor->update_count, err_sum / (length - start),
                   mDescriptor->integral_error, derivative_sum / dt / (length - start), pOut, iOut,
                   dOut, static_cast<int>(output));
    mDescriptor->update_count++;

    mStaleHandler->updateStaleTimer();

    /* apply to all the threads in the group */
    if (output != 0) {
        int next_min = std::min(sUclampCap, mDescriptor->current_min + static_cast<int>(output));
        next_min = std::max(0, next_min);
        if (std::abs(mDescriptor->current_min - next_min) > sUclampGranularity) {
            setUclamp(next_min, 1024);
            mDescriptor->current_min = next_min;
        }
    }

    return ndk::ScopedAStatus::ok();
}

std::string AppHintDesc::toString() const {
    std::string out =
            StringPrintf("session %" PRIxPTR "\n", reinterpret_cast<uintptr_t>(this) & 0xffff);
    const int64_t durationNanos = duration.count();
    out.append(StringPrintf("  duration: %" PRId64 " ns\n", durationNanos));
    out.append(StringPrintf("  uclamp.min: %d \n", current_min));
    out.append(StringPrintf("  uid: %d, tgid: %d\n", uid, tgid));

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

bool PowerHintSession::isActive() {
    return mDescriptor->is_active.load();
}

bool PowerHintSession::isStale() {
    auto now = std::chrono::steady_clock::now();
    return now >= mStaleHandler->getStaleTime();
}

void PowerHintSession::setStale() {
    if (ATRACE_ENABLED()) {
        std::string sz =
                StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRIxPTR "-stale", mDescriptor->tgid,
                             mDescriptor->uid, reinterpret_cast<uintptr_t>(this) & 0xffff);
        ATRACE_INT(sz.c_str(), 1);
    }
    // Reset to default uclamp value.
    setUclamp(0, 1024);
    // Deliver a task to check if all sessions are inactive.
    updateUniveralBoostMode();
}

void PowerHintSession::StaleHandler::updateStaleTimer() {
    std::lock_guard<std::mutex> guard(mStaleLock);
    if (PowerHintMonitor::getInstance()->isRunning()) {
        auto when = getStaleTime();
        auto now = std::chrono::steady_clock::now();
        mLastUpdatedTime.store(now);
        if (now > when) {
            mSession->updateUniveralBoostMode();
        }
        if (!mIsMonitoringStale.load()) {
            auto next = getStaleTime();
            PowerHintMonitor::getInstance()->getLooper()->sendMessageDelayed(
                    duration_cast<nanoseconds>(next - now).count(), this, NULL);
            mIsMonitoringStale.store(true);
        }
    }
}

time_point<steady_clock> PowerHintSession::StaleHandler::getStaleTime() {
    return mLastUpdatedTime.load() +
           std::chrono::duration_cast<milliseconds>(mSession->kAdpfRate) * sStaleTimeFactor;
}

void PowerHintSession::StaleHandler::handleMessage(const Message &) {
    std::lock_guard<std::mutex> guard(mStaleLock);
    auto now = std::chrono::steady_clock::now();
    auto when = getStaleTime();
    // Check if the session is stale based on the last_updated_time.
    if (now > when) {
        mSession->setStale();
        mIsMonitoringStale.store(false);
        return;
    }
    // Schedule for the next checking time.
    PowerHintMonitor::getInstance()->getLooper()->sendMessageDelayed(
            duration_cast<nanoseconds>(when - now).count(), this, NULL);
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
