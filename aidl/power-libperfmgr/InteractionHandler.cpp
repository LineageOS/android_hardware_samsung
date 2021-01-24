/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <array>
#include <memory>

#include <fcntl.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

#include <android-base/properties.h>
#include <utils/Log.h>
#include <utils/Trace.h>

#include "InteractionHandler.h"

#define MAX_LENGTH 64

#define MSINSEC 1000L
#define NSINMS 1000000L

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

namespace {

static const bool kDisplayIdleSupport =
        ::android::base::GetBoolProperty("vendor.powerhal.disp.idle_support", true);
static const std::array<const char *, 2> kDispIdlePath = {"/sys/class/drm/card0/device/idle_state",
                                                          "/sys/class/graphics/fb0/idle_state"};
static const uint32_t kWaitMs =
        ::android::base::GetUintProperty("vendor.powerhal.disp.idle_wait", /*default*/ 100U);
static const uint32_t kMinDurationMs =
        ::android::base::GetUintProperty("vendor.powerhal.interaction.min", /*default*/ 1400U);
static const uint32_t kMaxDurationMs =
        ::android::base::GetUintProperty("vendor.powerhal.interaction.max", /*default*/ 5650U);
static const uint32_t kDurationOffsetMs =
        ::android::base::GetUintProperty("vendor.powerhal.interaction.offset", /*default*/ 650U);

static size_t CalcTimespecDiffMs(struct timespec start, struct timespec end) {
    size_t diff_in_ms = 0;
    diff_in_ms += (end.tv_sec - start.tv_sec) * MSINSEC;
    diff_in_ms += (end.tv_nsec - start.tv_nsec) / NSINMS;
    return diff_in_ms;
}

static int FbIdleOpen(void) {
    int fd;
    for (const auto &path : kDispIdlePath) {
        fd = open(path, O_RDONLY);
        if (fd >= 0)
            return fd;
    }
    ALOGE("Unable to open fb idle state path (%d)", errno);
    return -1;
}

}  // namespace

InteractionHandler::InteractionHandler(std::shared_ptr<HintManager> const &hint_manager)
    : mState(INTERACTION_STATE_UNINITIALIZED),
      mDurationMs(0),
      mHintManager(hint_manager) {}

InteractionHandler::~InteractionHandler() {
    Exit();
}

bool InteractionHandler::Init() {
    std::lock_guard<std::mutex> lk(mLock);

    if (mState != INTERACTION_STATE_UNINITIALIZED)
        return true;

    int fd = FbIdleOpen();
    if (fd < 0)
        return false;
    mIdleFd = fd;

    mEventFd = eventfd(0, EFD_NONBLOCK);
    if (mEventFd < 0) {
        ALOGE("Unable to create event fd (%d)", errno);
        close(mIdleFd);
        return false;
    }

    mState = INTERACTION_STATE_IDLE;
    mThread = std::unique_ptr<std::thread>(new std::thread(&InteractionHandler::Routine, this));

    return true;
}

void InteractionHandler::Exit() {
    std::unique_lock<std::mutex> lk(mLock);
    if (mState == INTERACTION_STATE_UNINITIALIZED)
        return;

    AbortWaitLocked();
    mState = INTERACTION_STATE_UNINITIALIZED;
    lk.unlock();

    mCond.notify_all();
    mThread->join();

    close(mEventFd);
    close(mIdleFd);
}

void InteractionHandler::PerfLock() {
    ALOGV("%s: acquiring perf lock", __func__);
    if (!mHintManager->DoHint("INTERACTION")) {
        ALOGE("%s: do hint INTERACTION failed", __func__);
    }
    ATRACE_INT("interaction_lock", 1);
}

void InteractionHandler::PerfRel() {
    ALOGV("%s: releasing perf lock", __func__);
    if (!mHintManager->EndHint("INTERACTION")) {
        ALOGE("%s: end hint INTERACTION failed", __func__);
    }
    ATRACE_INT("interaction_lock", 0);
}

void InteractionHandler::Acquire(int32_t duration) {
    ATRACE_CALL();

    std::lock_guard<std::mutex> lk(mLock);

    int inputDuration = duration + kDurationOffsetMs;
    int finalDuration;
    if (inputDuration > kMaxDurationMs)
        finalDuration = kMaxDurationMs;
    else if (inputDuration > kMinDurationMs)
        finalDuration = inputDuration;
    else
        finalDuration = kMinDurationMs;

    // Fallback to do boost directly
    // 1) override property is set OR
    // 2) InteractionHandler not initialized
    if (!kDisplayIdleSupport || mState == INTERACTION_STATE_UNINITIALIZED) {
        mHintManager->DoHint("INTERACTION", std::chrono::milliseconds(finalDuration));
        return;
    }

    struct timespec cur_timespec;
    clock_gettime(CLOCK_MONOTONIC, &cur_timespec);
    if (mState != INTERACTION_STATE_IDLE && finalDuration <= mDurationMs) {
        size_t elapsed_time = CalcTimespecDiffMs(mLastTimespec, cur_timespec);
        // don't hint if previous hint's duration covers this hint's duration
        if (elapsed_time <= (mDurationMs - finalDuration)) {
            ALOGV("%s: Previous duration (%d) cover this (%d) elapsed: %lld", __func__,
                  static_cast<int>(mDurationMs), static_cast<int>(finalDuration),
                  static_cast<long long>(elapsed_time));
            return;
        }
    }
    mLastTimespec = cur_timespec;
    mDurationMs = finalDuration;

    ALOGV("%s: input: %d final duration: %d", __func__, duration, finalDuration);

    if (mState == INTERACTION_STATE_WAITING)
        AbortWaitLocked();
    else if (mState == INTERACTION_STATE_IDLE)
        PerfLock();

    mState = INTERACTION_STATE_INTERACTION;
    mCond.notify_one();
}

void InteractionHandler::Release() {
    std::lock_guard<std::mutex> lk(mLock);
    if (mState == INTERACTION_STATE_WAITING) {
        ATRACE_CALL();
        PerfRel();
        mState = INTERACTION_STATE_IDLE;
    } else {
        // clear any wait aborts pending in event fd
        uint64_t val;
        ssize_t ret = read(mEventFd, &val, sizeof(val));

        ALOGW_IF(ret < 0, "%s: failed to clear eventfd (%zd, %d)", __func__, ret, errno);
    }
}

// should be called while locked
void InteractionHandler::AbortWaitLocked() {
    uint64_t val = 1;
    ssize_t ret = write(mEventFd, &val, sizeof(val));
    if (ret != sizeof(val))
        ALOGW("Unable to write to event fd (%zd)", ret);
}

void InteractionHandler::WaitForIdle(int32_t wait_ms, int32_t timeout_ms) {
    char data[MAX_LENGTH];
    ssize_t ret;
    struct pollfd pfd[2];

    ATRACE_CALL();

    ALOGV("%s: wait:%d timeout:%d", __func__, wait_ms, timeout_ms);

    pfd[0].fd = mEventFd;
    pfd[0].events = POLLIN;
    pfd[1].fd = mIdleFd;
    pfd[1].events = POLLPRI | POLLERR;

    ret = poll(pfd, 1, wait_ms);
    if (ret > 0) {
        ALOGV("%s: wait aborted", __func__);
        return;
    } else if (ret < 0) {
        ALOGE("%s: error in poll while waiting", __func__);
        return;
    }

    ret = pread(mIdleFd, data, sizeof(data), 0);
    if (!ret) {
        ALOGE("%s: Unexpected EOF!", __func__);
        return;
    }

    if (!strncmp(data, "idle", 4)) {
        ALOGV("%s: already idle", __func__);
        return;
    }

    ret = poll(pfd, 2, timeout_ms);
    if (ret < 0)
        ALOGE("%s: Error on waiting for idle (%zd)", __func__, ret);
    else if (ret == 0)
        ALOGV("%s: timed out waiting for idle", __func__);
    else if (pfd[0].revents)
        ALOGV("%s: wait for idle aborted", __func__);
    else if (pfd[1].revents)
        ALOGV("%s: idle detected", __func__);
}

void InteractionHandler::Routine() {
    pthread_setname_np(pthread_self(), "DispIdle");
    std::unique_lock<std::mutex> lk(mLock, std::defer_lock);

    while (true) {
        lk.lock();
        mCond.wait(lk, [&] { return mState != INTERACTION_STATE_IDLE; });
        if (mState == INTERACTION_STATE_UNINITIALIZED)
            return;
        mState = INTERACTION_STATE_WAITING;
        lk.unlock();

        WaitForIdle(kWaitMs, mDurationMs);
        Release();
    }
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
