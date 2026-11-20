/*
 * Copyright (C) 2022 The Android Open Source Project
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

#include "effectFactory-impl/EffectFactory.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <system/audio_config.h>

#ifdef __ANDROID_APEX__
#include <android/apexsupport.h>
#endif

/** Default name of effect configuration file. */
static const char* kDefaultConfigName = "audio_effects_config.xml";

static inline std::string config_file_path() {
    if constexpr (__ANDROID_VENDOR_API__ >= 202404) {
        AApexInfo *apexInfo;
        if (AApexInfo_create(&apexInfo) == AAPEXINFO_OK) {
            std::string apexName(AApexInfo_getName(apexInfo));
            AApexInfo_destroy(apexInfo);
            std::string candidatePath("/apex/");
            candidatePath.append(apexName).append("/etc/").append(kDefaultConfigName);
            LOG(DEBUG) << __func__ << " effect lib path " << candidatePath;
            if (access(candidatePath.c_str(), R_OK) == 0) {
                return candidatePath;
            }
        }
    } else {
        LOG(DEBUG) << __func__ << " libapexsupport is not supported";
    }
    LOG(DEBUG) << __func__ << ": Unable to resolve config file path in APEX";
    return android::audio_find_readable_configuration_file(kDefaultConfigName);
}

int main() {
    // This is a debug implementation, always enable debug logging.
    android::base::SetMinimumLogSeverity(::android::base::DEBUG);
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto configFile = config_file_path();
    if (configFile == "") {
        LOG(ERROR) << __func__ << ": config file " << kDefaultConfigName << " not found!";
        return EXIT_FAILURE;
    }
    LOG(DEBUG) << __func__ << ": start factory with configFile:" << configFile;
    auto effectFactory =
            ndk::SharedRefBase::make<aidl::android::hardware::audio::effect::Factory>(configFile);

    std::string serviceName = std::string() + effectFactory->descriptor + "/default";
    binder_status_t status =
            AServiceManager_addService(effectFactory->asBinder().get(), serviceName.c_str());
    CHECK_EQ(STATUS_OK, status);

    LOG(DEBUG) << __func__ << ": effectFactory: " << serviceName << " start";
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
