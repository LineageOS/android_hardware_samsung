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

#include <cstdlib>
#include <ctime>
#include <utility>
#include <vector>

#define LOG_TAG "AHAL_Main"
#include <Log.h>
#include <android-base/properties.h>
#include <android/binder_ibinder_platform.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include "core-impl/AudioPolicyConfigXmlConverter.h"
#include "core-impl/ChildInterface.h"
#include "core-impl/Config.h"
#include "core-impl/Module.h"

using aidl::android::hardware::audio::core::ChildInterface;
using aidl::android::hardware::audio::core::Config;
using aidl::android::hardware::audio::core::Module;
using aidl::android::hardware::audio::core::internal::AudioPolicyConfigXmlConverter;

namespace {

ChildInterface<Module> createModule(const std::string& name,
                                    std::unique_ptr<Module::Configuration>&& config) {
    ChildInterface<Module> result;
    {
        auto moduleType = Module::typeFromString(name);
        if (!moduleType.has_value()) {
            LOG(ERROR) << __func__ << ": module type \"" << name << "\" is not supported";
            return result;
        }
        auto module = Module::createInstance(*moduleType, std::move(config));
        if (module == nullptr) return result;
        result = std::move(module);
    }
    const std::string moduleFqn = std::string().append(Module::descriptor).append("/").append(name);
    binder_status_t status = AServiceManager_addService(result.getBinder(), moduleFqn.c_str());
    if (status != STATUS_OK) {
        LOG(ERROR) << __func__ << ": failed to register service for \"" << moduleFqn << "\"";
        return ChildInterface<Module>();
    }
    return result;
};

}  // namespace

int main() {
    // Random values are used in the implementation.
    std::srand(std::time(nullptr));

    // This is a debug implementation, always enable debug logging.
    android::base::SetMinimumLogSeverity(::android::base::DEBUG);
    // For more logs, use VERBOSE, however this may hinder performance.
    // android::base::SetMinimumLogSeverity(::android::base::VERBOSE);
    ABinderProcess_setThreadPoolMaxThreadCount(16);
    ABinderProcess_startThreadPool();

    // Guaranteed log for b/210919187 and logd_integration_test
    LOG(INFO) << "Init for Audio AIDL HAL";

    AudioPolicyConfigXmlConverter audioPolicyConverter{
            ::android::audio_get_audio_policy_config_file()};

    // Make the default config service
    auto config = ndk::SharedRefBase::make<Config>(audioPolicyConverter);
    const std::string configFqn = std::string().append(Config::descriptor).append("/default");
    binder_status_t status =
            AServiceManager_addService(config->asBinder().get(), configFqn.c_str());
    if (status != STATUS_OK) {
        LOG(ERROR) << "failed to register service for \"" << configFqn << "\"";
    }

    // Make modules
    std::vector<ChildInterface<Module>> moduleInstances;
    auto configs(audioPolicyConverter.releaseModuleConfigs());
    for (std::pair<std::string, std::unique_ptr<Module::Configuration>>& configPair : *configs) {
        std::string name = configPair.first;
        if (auto instance = createModule(name, std::move(configPair.second)); instance) {
            moduleInstances.push_back(std::move(instance));
        }
    }

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
