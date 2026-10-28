/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "RadioSamsungSlot"

#include "RadioSlot.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <samsung_ril/structs.h>

#include <algorithm>

#include "config/RadioConfig.h"
#include "modem/RadioModem.h"
#include "network/RadioNetwork.h"
#include "sim/RadioSim.h"

namespace aidl::android::hardware::radio::samsung {

namespace {

template <typename T>
void publishService(const std::shared_ptr<T>& service, const std::string& instance) {
    const auto name = std::string(T::descriptor) + "/" + instance;
    if (!AServiceManager_isDeclared(name.c_str())) {
        LOG(INFO) << name << " is not declared, skipping";
        return;
    }
    const auto status = AServiceManager_addService(service->asBinder().get(), name.c_str());
    CHECK_EQ(status, STATUS_OK) << "Failed to register " << name;
}

}  // namespace

RadioSlot::RadioSlot(RIL_SOCKET_ID socket, std::shared_ptr<RadioConfig> config)
    : mSocket(socket),
      mInstance("slot" + std::to_string(socket + 1)),
      mConfig(std::move(config)),
      mModem(::ndk::SharedRefBase::make<RadioModem>(*this)),
      mNetwork(::ndk::SharedRefBase::make<RadioNetwork>(*this)),
      mSim(::ndk::SharedRefBase::make<RadioSim>(*this)) {
    VendorRil::get().setUnsolicitedCallback(
            mSocket,
            [this](int id, const void* data, size_t length) { onUnsolicited(id, data, length); });
}

void RadioSlot::publish() {
    publishService(mModem, mInstance);
    publishService(mNetwork, mInstance);
    publishService(mSim, mInstance);
}

void RadioSlot::request(int id, const void* data, size_t length, ResponseCallback callback) {
    VendorRil::get().request(mSocket, id, data, length, std::move(callback));
}

void RadioSlot::request(int id, ResponseCallback callback) {
    request(id, nullptr, 0, std::move(callback));
}

void RadioSlot::requestInts(int id, std::vector<int> ints, ResponseCallback callback) {
    request(id, ints.data(), ints.size() * sizeof(int), std::move(callback));
}

void RadioSlot::requestString(int id, const std::string& str, ResponseCallback callback) {
    request(id, str.empty() ? nullptr : str.c_str(), sizeof(char*), std::move(callback));
}

void RadioSlot::requestStrings(int id, const std::vector<std::string>& strings,
                               ResponseCallback callback, bool allowEmpty) {
    std::vector<const char*> pointers;
    for (const auto& str : strings) {
        pointers.push_back(str.empty() && !allowEmpty ? nullptr : str.c_str());
    }
    request(id, pointers.data(), pointers.size() * sizeof(char*), std::move(callback));
}

void RadioSlot::setVendorConfiguration(std::vector<std::pair<std::string, std::string>> config) {
    {
        std::lock_guard lock(mVendorConfigMutex);
        mVendorConfig = std::move(config);
        mVendorConfigPending = true;
    }
    sendVendorConfiguration();
}

void RadioSlot::sendVendorConfiguration() {
    if (VendorRil::get().radioState(mSocket) == RADIO_STATE_UNAVAILABLE) {
        return;
    }
    std::vector<std::pair<std::string, std::string>> config;
    {
        std::lock_guard lock(mVendorConfigMutex);
        if (!mVendorConfigPending) {
            return;
        }
        mVendorConfigPending = false;
        config = mVendorConfig;
    }
    SecVendorConfiguration configuration = {
            .count = static_cast<int>(std::min<size_t>(config.size(), kSecMaxVendorConfigurations)),
    };
    for (int i = 0; i < configuration.count; i++) {
        configuration.entries[i] = {const_cast<char*>(config[i].first.c_str()),
                                    const_cast<char*>(config[i].second.c_str())};
    }
    request(SEC_REQUEST_OEM_SET_VENDOR_SPECIFIC_CONFIGURATION, &configuration,
            sizeof(configuration), [this](auto e, auto, auto) {
                if (e == RIL_E_SUCCESS) {
                    LOG(INFO) << mInstance << ": vendor configuration sent";
                    return;
                }
                LOG(ERROR) << mInstance << ": vendor configuration failed: " << e;
                std::lock_guard lock(mVendorConfigMutex);
                mVendorConfigPending = e == RIL_E_RADIO_NOT_AVAILABLE;
            });
}

void RadioSlot::onUnsolicited(int id, const void* data, size_t length) {
    LOG(VERBOSE) << mInstance << ": unsolicited " << id << " (" << length << " bytes)";
    const int service = id / kSecUnsolServiceDivisor;
    const int unsol = id % kSecUnsolServiceDivisor;

    switch (service) {
        case SEC_UNSOL_CONFIG:
            mConfig->onUnsolicited(unsol, data, length, kUnsolicited);
            break;
        case SEC_UNSOL_MODEM:
            mModem->onUnsolicited(unsol, data, length, kUnsolicited);
            if (unsol == RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED) {
                sendVendorConfiguration();
            }
            break;
        case SEC_UNSOL_NETWORK:
            mNetwork->onUnsolicited(unsol, data, length, kUnsolicited);
            break;
        case SEC_UNSOL_SIM:
            mSim->onUnsolicited(unsol, data, length, kUnsolicited);
            break;
        default:
            LOG(DEBUG) << mInstance << ": unhandled unsolicited " << id << " (" << length
                       << " bytes)";
    }
}

}  // namespace aidl::android::hardware::radio::samsung
