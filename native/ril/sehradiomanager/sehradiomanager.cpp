/*
 * Copyright (C) 2024-2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SehRadioManager"

#include "aidl/SehRadioNetworkIndication.h"
#include "aidl/SehRadioNetworkResponse.h"
#include "hidl/SehRadioIndication.h"
#include "hidl/SehRadioResponse.h"

#include <thread>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <aidl/vendor/samsung/hardware/radio/network/ISehRadioNetwork.h>
#include <vendor/samsung/hardware/radio/2.2/ISehRadio.h>

using namespace std::chrono_literals;

using android::sp;
using android::String16;
using android::wp;
using android::base::GetIntProperty;
using android::base::ReadFileToString;
using android::base::Split;
using android::hardware::hidl_death_recipient;
using android::hardware::hidl_vec;
using android::hidl::base::V1_0::IBase;

using aidl::vendor::samsung::hardware::radio::network::ISehRadioNetwork;
using aidl::vendor::samsung::hardware::radio::network::implementation::SehRadioNetworkIndication;
using aidl::vendor::samsung::hardware::radio::network::implementation::SehRadioNetworkResponse;
using vendor::samsung::hardware::radio::V2_2::ISehRadio;
using vendor::samsung::hardware::radio::V2_2::SehVendorConfiguration;
using vendor::samsung::hardware::radio::V2_2::implementation::SehRadioIndication;
using vendor::samsung::hardware::radio::V2_2::implementation::SehRadioResponse;

using AidlVendorConfig = aidl::vendor::samsung::hardware::radio::network::SehVendorConfiguration;

template <typename C>
std::vector<C> LoadConfiguration(std::string data) {
    std::vector<C> config;

    for (std::string line : Split(data, "\n")) {
        if (line == "\0") break;

        std::vector<std::string> parts = Split(line, "=");
        if (parts.size() == 2) {
            config.push_back(C(parts[0], parts[1]));
            LOG(INFO) << line;
        } else {
            LOG(ERROR) << "Invalid data: " << line;
        }
    }

    return config;
}

bool died = false;
void onRILDDeath(void* cookie) {
    if (died) return;
    died = true;

    LOG(ERROR) << "SehRadio died, sleeping for 10s before restarting";
    std::this_thread::sleep_for(10000ms);
    exit(1);
}

struct RilHidlDeathRecipient : public hidl_death_recipient {
    void serviceDied(uint64_t cookie, const wp<IBase>& who) override { onRILDDeath((void*)cookie); }
};

static const auto gHalDeathRecipient = AIBinder_DeathRecipient_new(onRILDDeath);
static const auto gHidlHalDeathRecipient = sp<RilHidlDeathRecipient>::make();

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    ABinderProcess_startThreadPool();

    std::string content;
    if (!ReadFileToString("/vendor/etc/sehradiomanager.conf", &content)) {
        LOG(WARNING) << "Could not read config, setting defaults";
        content = "FW_READY=1";
    }

    int slotCount = GetIntProperty("ro.vendor.multisim.simslotcount", 1);
    for (int slot = 1; slot <= slotCount; slot++) {
        auto slotName = "slot" + std::to_string(slot);
        auto aidlSvcName = std::string(ISehRadioNetwork::descriptor) + "/" + slotName;
        if (AServiceManager_isDeclared(aidlSvcName.c_str())) {
            auto config = LoadConfiguration<AidlVendorConfig>(content);
            auto samsungIndication = ndk::SharedRefBase::make<SehRadioNetworkIndication>();
            auto samsungResponse = ndk::SharedRefBase::make<SehRadioNetworkResponse>();
            auto svc = ISehRadioNetwork::fromBinder(
                    ndk::SpAIBinder(AServiceManager_waitForService(aidlSvcName.c_str())));
            svc->setResponseFunctions(samsungResponse, samsungIndication);
            svc->setVendorSpecificConfiguration(0x4242, config);
            AIBinder_linkToDeath(svc->asBinder().get(), gHalDeathRecipient, 0);
            AIBinder_incStrong(svc->asBinder().get());
        } else {
            auto config = LoadConfiguration<SehVendorConfiguration>(content);
            auto samsungIndication = sp<SehRadioIndication>::make();
            auto samsungResponse = sp<SehRadioResponse>::make();
            auto svc = ISehRadio::getService(slotName);
            svc->setResponseFunction(samsungResponse, samsungIndication);
            svc->setVendorSpecificConfiguration(0x3232, hidl_vec(config));
            svc->linkToDeath(gHidlHalDeathRecipient, 0);
            svc->incStrong(gHidlHalDeathRecipient.get());
        }
        LOG(INFO) << "Done (slot" << slot << ")";
    }

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
