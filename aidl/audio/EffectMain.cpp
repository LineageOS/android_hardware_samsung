/*
 * SPDX-FileCopyrightText: 2022 The Android Open Source Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "effectFactory-impl/EffectFactory.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <system/audio_config.h>

/** Default name of effect configuration file. */
static const char* kDefaultConfigName = "audio_effects_config.xml";

static inline std::string config_file_path() {
    return android::audio_find_readable_configuration_file(kDefaultConfigName);
}

int main() {
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
