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

#include <log/log.h>
#include <utils/Trace.h>

#include "PowerSessionManager.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

void PowerSessionManager::setHintManager(std::shared_ptr<HintManager> const &hint_manager) {
    // Only initialize hintmanager instance if hint is supported.
    if (hint_manager->IsHintSupported(kDisableBoostHintName)) {
        mHintManager = hint_manager;
    }
}

void PowerSessionManager::updateHintMode(const std::string &mode, bool enabled) {
    ALOGD("PowerSessionManager::updateHintMode: mode: %s, enabled: %d", mode.c_str(), enabled);
    if (enabled && mode.compare(0, 8, "REFRESH_") == 0) {
        if (mode.compare("REFRESH_120FPS") == 0) {
            mDisplayRefreshRate = 120;
        } else if (mode.compare("REFRESH_90FPS") == 0) {
            mDisplayRefreshRate = 90;
        } else if (mode.compare("REFRESH_60FPS") == 0) {
            mDisplayRefreshRate = 60;
        }
    }
}

int PowerSessionManager::getDisplayRefreshRate() {
    return mDisplayRefreshRate;
}

void PowerSessionManager::addPowerSession(PowerHintSession *session) {
    std::lock_guard<std::mutex> guard(mLock);
    mSessions.insert(session);
}

void PowerSessionManager::removePowerSession(PowerHintSession *session) {
    std::lock_guard<std::mutex> guard(mLock);
    mSessions.erase(session);
}

bool PowerSessionManager::isAnySessionActive() {
    std::lock_guard<std::mutex> guard(mLock);
    for (PowerHintSession *s : mSessions) {
        // session active and not stale is actually active.
        if (s->isActive() && !s->isStale()) {
            return true;
        }
    }
    return false;
}

void PowerSessionManager::handleMessage(const Message &) {
    if (isAnySessionActive()) {
        disableSystemTopAppBoost();
    } else {
        enableSystemTopAppBoost();
    }
}

void PowerSessionManager::enableSystemTopAppBoost() {
    if (mHintManager) {
        ALOGD("PowerSessionManager::enableSystemTopAppBoost!!");
        if (ATRACE_ENABLED()) {
            ATRACE_INT(kDisableBoostHintName.c_str(), 0);
        }
        mHintManager->EndHint(kDisableBoostHintName);
    }
}

void PowerSessionManager::disableSystemTopAppBoost() {
    if (mHintManager) {
        ALOGD("PowerSessionManager::disableSystemTopAppBoost!!");
        if (ATRACE_ENABLED()) {
            ATRACE_INT(kDisableBoostHintName.c_str(), 1);
        }
        mHintManager->DoHint(kDisableBoostHintName);
    }
}

// =========== PowerHintMonitor implementation start from here ===========
void PowerHintMonitor::start() {
    if (!isRunning()) {
        run("PowerHintMonitor", ::android::PRIORITY_HIGHEST);
    }
}

bool PowerHintMonitor::threadLoop() {
    while (true) {
        mLooper->pollOnce(-1);
    }
    return true;
}

Looper *PowerHintMonitor::getLooper() {
    return mLooper;
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
