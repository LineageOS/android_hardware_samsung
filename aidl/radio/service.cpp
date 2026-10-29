/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsung"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "RadioSlot.h"
#include "config/RadioConfig.h"

using aidl::android::hardware::radio::samsung::RadioConfig;
using aidl::android::hardware::radio::samsung::RadioSlot;
using android::base::GetIntProperty;
using android::base::ReadFileToString;
using android::base::SetProperty;
using android::base::Split;
using android::hardware::radio::samsung::VendorRil;

namespace {

std::vector<std::pair<std::string, std::string>> loadVendorConfiguration() {
    std::string content;
    if (!ReadFileToString("/vendor/etc/sehradiomanager.conf", &content)) {
        content = "FW_READY=1";
    }
    std::vector<std::pair<std::string, std::string>> config;
    for (const auto& line : Split(content, "\n")) {
        const auto parts = Split(line, "=");
        if (parts.size() == 2) {
            config.emplace_back(parts[0], parts[1]);
        }
    }
    return config;
}

}  // namespace

int main() {
    android::base::InitLogging(nullptr, android::base::LogdLogger(android::base::RADIO));

    const int simCount = std::clamp(GetIntProperty("ro.vendor.multisim.simslotcount",
                                                   GetIntProperty("ro.multisim.simslotcount", 1)),
                                    1, static_cast<int>(RIL_SOCKET_NUM));

    auto config = ndk::SharedRefBase::make<RadioConfig>();

    std::vector<std::unique_ptr<RadioSlot>> slots;
    for (int socket = 0; socket < simCount; socket++) {
        const auto suffix = ".slot" + std::to_string(socket + 1);
        SetProperty("ril.halservice.registered" + suffix, "false");
        SetProperty("ril.phone.connected" + suffix, "false");
        slots.push_back(std::make_unique<RadioSlot>(static_cast<RIL_SOCKET_ID>(socket), config));
    }

    const std::string libPath = "/vendor/lib64/libsec-ril-impl.so";
    if (!VendorRil::get().load(libPath, {"-s", std::to_string(simCount), "-c", "0"})) {
        LOG(FATAL) << "Failed to load " << libPath;
    }
    SetProperty("gsm.version.ril-impl", VendorRil::get().version());

    const auto vendorConfig = loadVendorConfiguration();
    for (auto& slot : slots) {
        slot->setVendorConfiguration(vendorConfig);
    }

    const auto configName = std::string(RadioConfig::descriptor) + "/default";
    CHECK_EQ(AServiceManager_addService(config->asBinder().get(), configName.c_str()), STATUS_OK);

    for (auto& slot : slots) {
        slot->publish();
    }

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;
}
