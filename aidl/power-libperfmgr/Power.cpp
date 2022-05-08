/*
 * Copyright (C) 2020 The Android Open Source Project
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

#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)
#define LOG_TAG "powerhal-libperfmgr"

#include "Power.h"

#include <mutex>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <cutils/properties.h>

#include <utils/Log.h>
#include <utils/Trace.h>

#include "adpf/PowerHintSession.h"

namespace aidl {
namespace google {
namespace hardware {
namespace power {
namespace impl {
namespace pixel {

using ::aidl::google::hardware::power::impl::pixel::adpf::PowerHintSession;

constexpr char kPowerHalStateProp[] = "vendor.powerhal.state";
constexpr char kPowerHalAudioProp[] = "vendor.powerhal.audio";
constexpr char kPowerHalRenderingProp[] = "vendor.powerhal.rendering";
constexpr char kPowerHalAdpfRateProp[] = "vendor.powerhal.adpf.rate";
constexpr int64_t kPowerHalAdpfRateDefault = -1;

Power::Power(std::shared_ptr<HintManager> hm)
    : mHintManager(hm),
      mInteractionHandler(nullptr),
      mVRModeOn(false),
      mSustainedPerfModeOn(false),
      mAdpfRate(::android::base::GetIntProperty(kPowerHalAdpfRateProp, kPowerHalAdpfRateDefault)) {
    mInteractionHandler = std::make_unique<InteractionHandler>(mHintManager);
    mInteractionHandler->Init();

    std::string state = ::android::base::GetProperty(kPowerHalStateProp, "");
    if (state == "SUSTAINED_PERFORMANCE") {
        LOG(INFO) << "Initialize with SUSTAINED_PERFORMANCE on";
        mHintManager->DoHint("SUSTAINED_PERFORMANCE");
        mSustainedPerfModeOn = true;
    } else if (state == "VR") {
        LOG(INFO) << "Initialize with VR on";
        mHintManager->DoHint(state);
        mVRModeOn = true;
    } else if (state == "VR_SUSTAINED_PERFORMANCE") {
        LOG(INFO) << "Initialize with SUSTAINED_PERFORMANCE and VR on";
        mHintManager->DoHint("VR_SUSTAINED_PERFORMANCE");
        mSustainedPerfModeOn = true;
        mVRModeOn = true;
    } else {
        LOG(INFO) << "Initialize PowerHAL";
    }

    state = ::android::base::GetProperty(kPowerHalAudioProp, "");
    if (state == "AUDIO_STREAMING_LOW_LATENCY") {
        LOG(INFO) << "Initialize with AUDIO_LOW_LATENCY on";
        mHintManager->DoHint(state);
    }

    state = ::android::base::GetProperty(kPowerHalRenderingProp, "");
    if (state == "EXPENSIVE_RENDERING") {
        LOG(INFO) << "Initialize with EXPENSIVE_RENDERING on";
        mHintManager->DoHint("EXPENSIVE_RENDERING");
    }

    // Now start to take powerhint
    LOG(INFO) << "PowerHAL ready to take hints, Adpf update rate: " << mAdpfRate;
}

ndk::ScopedAStatus Power::setMode(Mode type, bool enabled) {
    LOG(DEBUG) << "Power setMode: " << toString(type) << " to: " << enabled;
    ATRACE_INT(toString(type).c_str(), enabled);
    switch (type) {
        case Mode::LOW_POWER:
            if (enabled) {
                mHintManager->DoHint(toString(type));
            } else {
                mHintManager->EndHint(toString(type));
            }
            break;
        case Mode::SUSTAINED_PERFORMANCE:
            if (enabled && !mSustainedPerfModeOn) {
                if (!mVRModeOn) {  // Sustained mode only.
                    mHintManager->DoHint("SUSTAINED_PERFORMANCE");
                } else {  // Sustained + VR mode.
                    mHintManager->EndHint("VR");
                    mHintManager->DoHint("VR_SUSTAINED_PERFORMANCE");
                }
                mSustainedPerfModeOn = true;
            } else if (!enabled && mSustainedPerfModeOn) {
                mHintManager->EndHint("VR_SUSTAINED_PERFORMANCE");
                mHintManager->EndHint("SUSTAINED_PERFORMANCE");
                if (mVRModeOn) {  // Switch back to VR Mode.
                    mHintManager->DoHint("VR");
                }
                mSustainedPerfModeOn = false;
            }
            break;
        case Mode::VR:
            if (enabled && !mVRModeOn) {
                if (!mSustainedPerfModeOn) {  // VR mode only.
                    mHintManager->DoHint("VR");
                } else {  // Sustained + VR mode.
                    mHintManager->EndHint("SUSTAINED_PERFORMANCE");
                    mHintManager->DoHint("VR_SUSTAINED_PERFORMANCE");
                }
                mVRModeOn = true;
            } else if (!enabled && mVRModeOn) {
                mHintManager->EndHint("VR_SUSTAINED_PERFORMANCE");
                mHintManager->EndHint("VR");
                if (mSustainedPerfModeOn) {  // Switch back to sustained Mode.
                    mHintManager->DoHint("SUSTAINED_PERFORMANCE");
                }
                mVRModeOn = false;
            }
            break;
        case Mode::LAUNCH:
            if (mVRModeOn || mSustainedPerfModeOn) {
                break;
            }
            [[fallthrough]];
        case Mode::DOUBLE_TAP_TO_WAKE:
            [[fallthrough]];
        case Mode::FIXED_PERFORMANCE:
            [[fallthrough]];
        case Mode::EXPENSIVE_RENDERING:
            [[fallthrough]];
        case Mode::INTERACTIVE:
            [[fallthrough]];
        case Mode::DEVICE_IDLE:
            [[fallthrough]];
        case Mode::DISPLAY_INACTIVE:
            [[fallthrough]];
        case Mode::AUDIO_STREAMING_LOW_LATENCY:
            [[fallthrough]];
        case Mode::CAMERA_STREAMING_SECURE:
            [[fallthrough]];
        case Mode::CAMERA_STREAMING_LOW:
            [[fallthrough]];
        case Mode::CAMERA_STREAMING_MID:
            [[fallthrough]];
        case Mode::CAMERA_STREAMING_HIGH:
            [[fallthrough]];
        default:
            if (enabled) {
                mHintManager->DoHint(toString(type));
            } else {
                mHintManager->EndHint(toString(type));
            }
            break;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::isModeSupported(Mode type, bool *_aidl_return) {
    bool supported = mHintManager->IsHintSupported(toString(type));
    switch (type) {
        case Mode::LOW_POWER: // LOW_POWER handled insides PowerHAL specifically
            supported = true;
            break;
        case Mode::DOUBLE_TAP_TO_WAKE:
            supported = true;
            break;
        case Mode::INTERACTIVE:
            supported = true;
            break;
        default:
            break;
    }

    LOG(INFO) << "Power mode " << toString(type) << " isModeSupported: " << supported;
    *_aidl_return = supported;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::setBoost(Boost type, int32_t durationMs) {
    LOG(DEBUG) << "Power setBoost: " << toString(type) << " duration: " << durationMs;
    ATRACE_INT(toString(type).c_str(), durationMs);
    switch (type) {
        case Boost::INTERACTION:
            if (mVRModeOn || mSustainedPerfModeOn) {
                break;
            }
            mInteractionHandler->Acquire(durationMs);
            break;
        case Boost::DISPLAY_UPDATE_IMMINENT:
            [[fallthrough]];
        case Boost::ML_ACC:
            [[fallthrough]];
        case Boost::AUDIO_LAUNCH:
            [[fallthrough]];
        case Boost::CAMERA_LAUNCH:
            [[fallthrough]];
        case Boost::CAMERA_SHOT:
            [[fallthrough]];
        default:
            if (mVRModeOn || mSustainedPerfModeOn) {
                break;
            }
            if (durationMs > 0) {
                mHintManager->DoHint(toString(type), std::chrono::milliseconds(durationMs));
            } else if (durationMs == 0) {
                mHintManager->DoHint(toString(type));
            } else {
                mHintManager->EndHint(toString(type));
            }
            break;
    }

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::isBoostSupported(Boost type, bool *_aidl_return) {
    bool supported = mHintManager->IsHintSupported(toString(type));
    LOG(INFO) << "Power boost " << toString(type) << " isBoostSupported: " << supported;
    *_aidl_return = supported;
    return ndk::ScopedAStatus::ok();
}

constexpr const char *boolToString(bool b) {
    return b ? "true" : "false";
}

binder_status_t Power::dump(int fd, const char **, uint32_t) {
    std::string buf(::android::base::StringPrintf(
            "HintManager Running: %s\n"
            "VRMode: %s\n"
            "SustainedPerformanceMode: %s\n",
            boolToString(mHintManager->IsRunning()), boolToString(mVRModeOn),
            boolToString(mSustainedPerfModeOn)));
    // Dump nodes through libperfmgr
    mHintManager->DumpToFd(fd);
    if (!::android::base::WriteStringToFd(buf, fd)) {
        PLOG(ERROR) << "Failed to dump state to fd";
    }
    // TODO(jimmyshiu@): dump weak_ptr of PowerHintSession
    fsync(fd);
    return STATUS_OK;
}

ndk::ScopedAStatus Power::createHintSession(int32_t tgid, int32_t uid,
                                            const std::vector<int32_t> &threadIds,
                                            int64_t durationNanos,
                                            std::shared_ptr<IPowerHintSession> *_aidl_return) {
    if (mAdpfRate == -1) {
        *_aidl_return = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }
    if (threadIds.size() == 0) {
        LOG(ERROR) << "Error: threadIds.size() shouldn't be " << threadIds.size();
        *_aidl_return = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::shared_ptr<IPowerHintSession> session =
            ndk::SharedRefBase::make<PowerHintSession>(tgid, uid, threadIds, durationNanos);
    *_aidl_return = session;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::getHintSessionPreferredRate(int64_t *outNanoseconds) {
    *outNanoseconds = mAdpfRate;
    if (mAdpfRate == -1) {
        return ndk::ScopedAStatus::fromExceptionCode(EX_UNSUPPORTED_OPERATION);
    }

    return ndk::ScopedAStatus::ok();
}

}  // namespace pixel
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace google
}  // namespace aidl
