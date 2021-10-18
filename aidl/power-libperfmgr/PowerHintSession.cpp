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
#include <sys/syscall.h>
#include <time.h>
#include <utils/Trace.h>
#include <atomic>

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

constexpr char kPowerHalAdpfPidPOver[] = "vendor.powerhal.adpf.pid_p.over";
constexpr char kPowerHalAdpfPidPUnder[] = "vendor.powerhal.adpf.pid_p.under";
constexpr char kPowerHalAdpfPidI[] = "vendor.powerhal.adpf.pid_i";
constexpr char kPowerHalAdpfPidDOver[] = "vendor.powerhal.adpf.pid_d.over";
constexpr char kPowerHalAdpfPidDUnder[] = "vendor.powerhal.adpf.pid_d.under";
constexpr char kPowerHalAdpfPidIInit[] = "vendor.powerhal.adpf.pid_i.init";
constexpr char kPowerHalAdpfPidIHighLimit[] = "vendor.powerhal.adpf.pid_i.high_limit";
constexpr char kPowerHalAdpfPidILowLimit[] = "vendor.powerhal.adpf.pid_i.low_limit";
constexpr char kPowerHalAdpfUclampEnable[] = "vendor.powerhal.adpf.uclamp";
constexpr char kPowerHalAdpfUclampMinGranularity[] = "vendor.powerhal.adpf.uclamp_min.granularity";
constexpr char kPowerHalAdpfUclampMinHighLimit[] = "vendor.powerhal.adpf.uclamp_min.high_limit";
constexpr char kPowerHalAdpfUclampMinLowLimit[] = "vendor.powerhal.adpf.uclamp_min.low_limit";
constexpr char kPowerHalAdpfStaleTimeFactor[] = "vendor.powerhal.adpf.stale_timeout_factor";
constexpr char kPowerHalAdpfPSamplingWindow[] = "vendor.powerhal.adpf.p.window";
constexpr char kPowerHalAdpfISamplingWindow[] = "vendor.powerhal.adpf.i.window";
constexpr char kPowerHalAdpfDSamplingWindow[] = "vendor.powerhal.adpf.d.window";

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

static double sPidPOver = getDoubleProperty(kPowerHalAdpfPidPOver, 2.0);
static double sPidPUnder = getDoubleProperty(kPowerHalAdpfPidPUnder, 1.0);
static double sPidI = getDoubleProperty(kPowerHalAdpfPidI, 0.001);
static double sPidDOver = getDoubleProperty(kPowerHalAdpfPidDOver, 500.0);
static double sPidDUnder = getDoubleProperty(kPowerHalAdpfPidDUnder, 0.0);
static const int64_t sPidIInit =
        (sPidI == 0) ? 0
                     : static_cast<int64_t>(::android::base::GetIntProperty<int64_t>(
                                                    kPowerHalAdpfPidIInit, 200) /
                                            sPidI);
static const int64_t sPidIHighLimit =
        (sPidI == 0) ? 0
                     : static_cast<int64_t>(::android::base::GetIntProperty<int64_t>(
                                                    kPowerHalAdpfPidIHighLimit, 512) /
                                            sPidI);
static const int64_t sPidILowLimit =
        (sPidI == 0) ? 0
                     : static_cast<int64_t>(::android::base::GetIntProperty<int64_t>(
                                                    kPowerHalAdpfPidILowLimit, -30) /
                                            sPidI);
static const int32_t sUclampMinHighLimit =
        ::android::base::GetUintProperty<uint32_t>(kPowerHalAdpfUclampMinHighLimit, 384);
static const int32_t sUclampMinLowLimit =
        ::android::base::GetUintProperty<uint32_t>(kPowerHalAdpfUclampMinLowLimit, 2);
static const uint32_t sUclampMinGranularity =
        ::android::base::GetUintProperty<uint32_t>(kPowerHalAdpfUclampMinGranularity, 5);
static const int64_t sStaleTimeFactor =
        ::android::base::GetUintProperty<uint32_t>(kPowerHalAdpfStaleTimeFactor, 20);
static const int64_t sPSamplingWindow =
        ::android::base::GetUintProperty<uint32_t>(kPowerHalAdpfPSamplingWindow, 1);
static const int64_t sISamplingWindow =
        ::android::base::GetUintProperty<uint32_t>(kPowerHalAdpfISamplingWindow, 0);
static const int64_t sDSamplingWindow =
        ::android::base::GetUintProperty<uint32_t>(kPowerHalAdpfDSamplingWindow, 1);

}  // namespace

PowerHintSession::PowerHintSession(int32_t tgid, int32_t uid, const std::vector<int32_t> &threadIds,
                                   int64_t durationNanos, const nanoseconds adpfRate)
    : kAdpfRate(adpfRate) {
    mDescriptor = new AppHintDesc(tgid, uid, threadIds);
    mDescriptor->duration = std::chrono::nanoseconds(durationNanos);
    mStaleHandler = sp<StaleHandler>(new StaleHandler(this));
    mPowerManagerHandler = PowerSessionManager::getInstance();

    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-target", idstr.c_str());
        ATRACE_INT(sz.c_str(), (int64_t)mDescriptor->duration.count());
        sz = StringPrintf("adpf.%s-active", idstr.c_str());
        ATRACE_INT(sz.c_str(), mDescriptor->is_active.load());
        sz = StringPrintf("adpf.%s-stale", idstr.c_str());
        ATRACE_INT(sz.c_str(), isStale());
    }
    PowerSessionManager::getInstance()->addPowerSession(this);
    // init boost
    setUclamp(sUclampMinHighLimit);
    ALOGV("PowerHintSession created: %s", mDescriptor->toString().c_str());
}

PowerHintSession::~PowerHintSession() {
    close();
    ALOGV("PowerHintSession deleted: %s", mDescriptor->toString().c_str());
    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-target", idstr.c_str());
        ATRACE_INT(sz.c_str(), 0);
        sz = StringPrintf("adpf.%s-actl_last", idstr.c_str());
        ATRACE_INT(sz.c_str(), 0);
        sz = sz = StringPrintf("adpf.%s-active", idstr.c_str());
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
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-min", idstr.c_str());
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
    mDescriptor->current_min = min;
    return 0;
}

ndk::ScopedAStatus PowerHintSession::pause() {
    if (!mDescriptor->is_active.load())
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    // Reset to default uclamp value.
    setUclamp(0);
    mDescriptor->is_active.store(false);
    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-active", idstr.c_str());
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
    // resume boost
    setUclamp(sUclampMinHighLimit);
    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-active", idstr.c_str());
        ATRACE_INT(sz.c_str(), mDescriptor->is_active.load());
    }
    updateUniveralBoostMode();
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSession::close() {
    bool sessionClosedExpectedToBe = false;
    if (!mSessionClosed.compare_exchange_strong(sessionClosedExpectedToBe, true)) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    PowerHintMonitor::getInstance()->getLooper()->removeMessages(mStaleHandler);
    setUclamp(0);
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
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-target", idstr.c_str());
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
        mDescriptor->integral_error = std::max(sPidIInit, mDescriptor->integral_error);
        if (ATRACE_ENABLED()) {
            const std::string idstr = getIdString();
            std::string sz = StringPrintf("adpf.%s-wakeup", idstr.c_str());
            ATRACE_INT(sz.c_str(), mDescriptor->integral_error);
            ATRACE_INT(sz.c_str(), 0);
        }
    }
    int64_t targetDurationNanos = (int64_t)mDescriptor->duration.count();
    int64_t length = actualDurations.size();
    int64_t p_start =
            sPSamplingWindow == 0 || sPSamplingWindow > length ? 0 : length - sPSamplingWindow;
    int64_t i_start =
            sISamplingWindow == 0 || sISamplingWindow > length ? 0 : length - sISamplingWindow;
    int64_t d_start =
            sDSamplingWindow == 0 || sDSamplingWindow > length ? 0 : length - sDSamplingWindow;
    int64_t dt = ns_to_100us(targetDurationNanos);
    int64_t err_sum = 0;
    int64_t derivative_sum = 0;
    for (int64_t i = std::min({p_start, i_start, d_start}); i < length; i++) {
        int64_t actualDurationNanos = actualDurations[i].durationNanos;
        if (std::abs(actualDurationNanos) > targetDurationNanos * 20) {
            ALOGW("The actual duration is way far from the target (%" PRId64 " >> %" PRId64 ")",
                  actualDurationNanos, targetDurationNanos);
        }
        // PID control algorithm
        int64_t error = ns_to_100us(actualDurationNanos - targetDurationNanos);
        if (i >= d_start) {
            derivative_sum += error - mDescriptor->previous_error;
        }
        if (i >= p_start) {
            err_sum += error;
        }
        if (i >= i_start) {
            mDescriptor->integral_error = mDescriptor->integral_error + error * dt;
            mDescriptor->integral_error = std::min(sPidIHighLimit, mDescriptor->integral_error);
            mDescriptor->integral_error = std::max(sPidILowLimit, mDescriptor->integral_error);
        }
        mDescriptor->previous_error = error;
    }
    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-err", idstr.c_str());
        ATRACE_INT(sz.c_str(), err_sum / (length - p_start));
        sz = StringPrintf("adpf.%s-integral", idstr.c_str());
        ATRACE_INT(sz.c_str(), mDescriptor->integral_error);
        sz = StringPrintf("adpf.%s-derivative", idstr.c_str());
        ATRACE_INT(sz.c_str(), derivative_sum / dt / (length - d_start));
    }
    int64_t pOut = static_cast<int64_t>((err_sum > 0 ? sPidPOver : sPidPUnder) * err_sum /
                                        (length - p_start));
    int64_t iOut = static_cast<int64_t>(sPidI * mDescriptor->integral_error);
    int64_t dOut = static_cast<int64_t>((derivative_sum > 0 ? sPidDOver : sPidDUnder) *
                                        derivative_sum / dt / (length - d_start));

    int64_t output = pOut + iOut + dOut;

    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-actl_last", idstr.c_str());
        ATRACE_INT(sz.c_str(), actualDurations[length - 1].durationNanos);
        sz = StringPrintf("adpf.%s-target", idstr.c_str());
        ATRACE_INT(sz.c_str(), (int64_t)mDescriptor->duration.count());
        sz = StringPrintf("adpf.%s-sample_size", idstr.c_str());
        ATRACE_INT(sz.c_str(), length);
        sz = StringPrintf("adpf.%s-pid.count", idstr.c_str());
        ATRACE_INT(sz.c_str(), mDescriptor->update_count);
        sz = StringPrintf("adpf.%s-pid.pOut", idstr.c_str());
        ATRACE_INT(sz.c_str(), pOut);
        sz = StringPrintf("adpf.%s-pid.iOut", idstr.c_str());
        ATRACE_INT(sz.c_str(), iOut);
        sz = StringPrintf("adpf.%s-pid.dOut", idstr.c_str());
        ATRACE_INT(sz.c_str(), dOut);
        sz = StringPrintf("adpf.%s-pid.output", idstr.c_str());
        ATRACE_INT(sz.c_str(), output);
        sz = StringPrintf("adpf.%s-stale", idstr.c_str());
        ATRACE_INT(sz.c_str(), isStale());
        sz = StringPrintf("adpf.%s-pid.overtime", idstr.c_str());
        ATRACE_INT(sz.c_str(), err_sum > 0);
    }
    mDescriptor->update_count++;

    mStaleHandler->updateStaleTimer();

    /* apply to all the threads in the group */
    if (output != 0) {
        int next_min = std::min(sUclampMinHighLimit, static_cast<int>(output));
        next_min = std::max(sUclampMinLowLimit, next_min);
        if (std::abs(mDescriptor->current_min - next_min) > sUclampMinGranularity) {
            setUclamp(next_min);
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

const std::vector<int> &PowerHintSession::getTidList() const {
    return mDescriptor->threadIds;
}

void PowerHintSession::setStale() {
    if (ATRACE_ENABLED()) {
        const std::string idstr = getIdString();
        std::string sz = StringPrintf("adpf.%s-stale", idstr.c_str());
        ATRACE_INT(sz.c_str(), 1);
    }
    // Reset to default uclamp value.
    setUclamp(0);
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
        if (ATRACE_ENABLED()) {
            const std::string idstr = mSession->getIdString();
            std::string sz = StringPrintf("adpf.%s-stale", idstr.c_str());
            ATRACE_INT(sz.c_str(), 0);
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
