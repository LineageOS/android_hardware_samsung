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

#include "PowerHintSession.h"

#include <android-base/properties.h>
#include <perfmgr/HintManager.h>
#include <utils/Looper.h>

#include <mutex>
#include <unordered_set>

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using ::android::Looper;
using ::android::Message;
using ::android::MessageHandler;
using ::android::Thread;
using ::android::perfmgr::HintManager;

constexpr char kPowerHalAdpfDisableTopAppBoost[] = "vendor.powerhal.adpf.disable.hint";

class PowerSessionManager : public MessageHandler {
  public:
    // current hint info
    void updateHintMode(const std::string &mode, bool enabled);
    int getDisplayRefreshRate();
    // monitoring session status
    void addPowerSession(PowerHintSession *session);
    void removePowerSession(PowerHintSession *session);
    bool isAnySessionActive();
    void handleMessage(const Message &message) override;
    void setHintManager(std::shared_ptr<HintManager> const &hint_manager);

    // Singleton
    static sp<PowerSessionManager> getInstance() {
        static sp<PowerSessionManager> instance = new PowerSessionManager();
        return instance;
    }

  private:
    void disableSystemTopAppBoost();
    void enableSystemTopAppBoost();
    const std::string kDisableBoostHintName;
    std::shared_ptr<HintManager> mHintManager;
    std::unordered_set<PowerHintSession *> mSessions;
    std::mutex mLock;
    int mDisplayRefreshRate;
    // Singleton
    PowerSessionManager()
        : kDisableBoostHintName(::android::base::GetProperty(kPowerHalAdpfDisableTopAppBoost,
                                                             "ADPF_DISABLE_TA_BOOST")),
          mHintManager(nullptr),
          mDisplayRefreshRate(60) {}
    PowerSessionManager(PowerSessionManager const &) = delete;
    void operator=(PowerSessionManager const &) = delete;
};

class PowerHintMonitor : public Thread {
  public:
    void start();
    bool threadLoop() override;
    sp<Looper> getLooper();
    // Singleton
    static sp<PowerHintMonitor> getInstance() {
        static sp<PowerHintMonitor> instance = new PowerHintMonitor();
        return instance;
    }
    PowerHintMonitor(PowerHintMonitor const &) = delete;
    void operator=(PowerHintMonitor const &) = delete;

  private:
    sp<Looper> mLooper;
    // Singleton
    PowerHintMonitor() : Thread(false), mLooper(new Looper(true)) {}
};

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
