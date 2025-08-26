/*
 * Copyright 2018 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "android.software.media.c2-service.samsung"

#include <android-base/logging.h>
#include <minijail.h>

#include <util/C2InterfaceHelper.h>
#include <C2Component.h>
#include <C2Config.h>

// HIDL
#ifdef LEGACY_COMPONENT_STORE
#include <codec2/hidl/1.1/ComponentStore.h>
#else
#include <codec2/hidl/1.2/ComponentStore.h>
#endif
#include <binder/ProcessState.h>
#include <hidl/HidlTransportSupport.h>

// AIDL
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <codec2/aidl/ComponentStore.h>
#include <codec2/aidl/ParamTypes.h>

// This is the absolute on-device path of the prebuild_etc module
// "android.software.media.c2-default-seccomp_policy" in Android.bp.
static constexpr char kBaseSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/"
        "android.software.media.c2-default-seccomp_policy";

// Additional seccomp permissions can be added in this file.
// This file does not exist by default.
static constexpr char kExtSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/"
        "android.software.media.c2-extended-seccomp_policy";

// We want multiple threads to be running so that a blocking operation
// on one codec does not block the other codecs.
// For HIDL: Extra threads may be needed to handle a stacked IPC sequence that
// contains alternating binder and hwbinder calls. (See b/35283480.)
static constexpr int kThreadCount = 8;

#ifdef LEGACY_COMPONENT_STORE
extern "C" void RegisterSECCodecServices();
#else
extern "C" void RegisterSECCodecAidlServices();
extern "C" void RegisterSECCodecHidlServices();
#endif

void RegisterLegacyComponentStore() {
    using namespace ::android;

    // Enable vndbinder to allow vendor-to-vendor binder calls.
    ProcessState::initWithDriver("/dev/vndbinder");

    ProcessState::self()->startThreadPool();
    hardware::configureRpcThreadpool(kThreadCount, true /* callerWillJoin */);

    RegisterSECCodecServices();

    hardware::joinRpcThreadpool();
}

int main(int /* argc */, char** /* argv */) {
    const bool aidlEnabled = ::aidl::android::hardware::media::c2::utils::IsSelected();
#ifdef LEGACY_COMPONENT_STORE
    LOG(DEBUG) << "android.software.media.c2@1.0-service.samsung starting...";
#else
    LOG(DEBUG) << "android.software.media.c2" << (aidlEnabled ? "-V1" : "@1.2")
               << "-service.samsung starting...";
#endif

    // Set up minijail to limit system calls.
    signal(SIGPIPE, SIG_IGN);
    android::SetUpMinijail(kBaseSeccompPolicyPath, kExtSeccompPolicyPath);

#ifdef LEGACY_COMPONENT_STORE
    RegisterLegacyComponentStore();
#else
    if (aidlEnabled) {
        RegisterSECCodecAidlServices();
    } else {
        RegisterSECCodecHidlServices();
    }
#endif
    return 0;
}
