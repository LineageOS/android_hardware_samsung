/*
 * SPDX-FileCopyrightText: 2022 The Android Open Source Project
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
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

#include "ModuleLegacy.h"
#include "core-impl/AudioPolicyConfigXmlConverter.h"
#include "core-impl/ChildInterface.h"
#include "core-impl/Config.h"
#include "core-impl/Module.h"

using aidl::android::hardware::audio::core::ChildInterface;
using aidl::android::hardware::audio::core::Config;
using aidl::android::hardware::audio::core::Module;
using aidl::android::hardware::audio::core::ModuleLegacy;
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
        // The primary module is backed by the legacy HAL, the others by the default
        // implementations, which replace the legacy r_submix, usb and bluetooth HALs
        std::shared_ptr<Module> module =
                *moduleType == Module::Type::DEFAULT
                        ? ModuleLegacy::create(std::move(config))
                        : Module::createInstance(*moduleType, std::move(config));
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

extern "C" binder_status_t createIBluetoothAudioProviderFactory();

int main() {
    // Random values are used in the implementation.
    std::srand(std::time(nullptr));

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

    // Like the HIDL audio service, host the Bluetooth audio provider, used by the bluetooth module
    if (status = createIBluetoothAudioProviderFactory(); status != STATUS_OK) {
        LOG(ERROR) << "failed to register the Bluetooth audio provider factory";
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
