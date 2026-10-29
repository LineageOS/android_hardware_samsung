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

#include "WakeLock.h"
#include "config/RadioConfig.h"
#include "data/RadioData.h"
#include "messaging/RadioMessaging.h"
#include "modem/RadioModem.h"
#include "network/RadioNetwork.h"
#include "sap/RadioSap.h"
#include "sim/RadioSim.h"
#include "voice/CallStatePublisher.h"
#include "voice/RadioVoice.h"

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
      mData(::ndk::SharedRefBase::make<RadioData>(*this)),
      mMessaging(::ndk::SharedRefBase::make<RadioMessaging>(*this)),
      mModem(::ndk::SharedRefBase::make<RadioModem>(*this)),
      mNetwork(::ndk::SharedRefBase::make<RadioNetwork>(*this)),
      mSap(::ndk::SharedRefBase::make<RadioSap>(*this)),
      mSim(::ndk::SharedRefBase::make<RadioSim>(*this)),
      mVoice(::ndk::SharedRefBase::make<RadioVoice>(*this)) {
    VendorRil::get().setUnsolicitedCallback(
            mSocket,
            [this](int id, const void* data, size_t length) { onUnsolicited(id, data, length); });
}

void RadioSlot::publish() {
    publishService(mData, mInstance);
    publishService(mMessaging, mInstance);
    publishService(mModem, mInstance);
    publishService(mNetwork, mInstance);
    if (VendorRil::get().hasSap()) {
        publishService(mSap, mInstance);
    }
    publishService(mSim, mInstance);
    publishService(mVoice, mInstance);
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

    // Released on acknowledgement, or right away if nothing was sent.
    const bool wake = service != SEC_UNSOL_CONFIG && unsol != RIL_UNSOL_SIGNAL_STRENGTH &&
                      unsol != RIL_UNSOL_ON_USSD_REQUEST;
    if (wake) {
        WakeLock::get().acquire();
    }
    const auto type =
            wake ? RadioIndicationType::UNSOLICITED_ACK_EXP : RadioIndicationType::UNSOLICITED;

    bool sent = false;
    switch (service) {
        case SEC_UNSOL_CONFIG:
            sent = mConfig->onUnsolicited(unsol, data, length, type);
            break;
        case SEC_UNSOL_MESSAGING:
            sent = mMessaging->onUnsolicited(unsol, data, length, type);
            break;
        case SEC_UNSOL_DATA:
            sent = mData->onUnsolicited(unsol, data, length, type);
            break;
        case SEC_UNSOL_MODEM:
            if (unsol == RIL_UNSOL_MODEM_RESTART) {
                CallStatePublisher::get().resetAll();
            } else if (unsol == RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED) {
                CallStatePublisher::get().reset(mSocket);
            }
            sent = mModem->onUnsolicited(unsol, data, length, type);
            if (unsol == RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED) {
                sendVendorConfiguration();
            }
            break;
        case SEC_UNSOL_NETWORK:
            sent = mNetwork->onUnsolicited(unsol, data, length, type);
            break;
        case SEC_UNSOL_SIM:
            // STK call setup arrives as a SIM indication but belongs to voice.
            if (unsol == RIL_UNSOL_STK_CALL_SETUP) {
                sent = mVoice->onUnsolicited(unsol, data, length, type);
                break;
            }
            sent = mSim->onUnsolicited(unsol, data, length, type);
            break;
        case SEC_UNSOL_VOICE:
            sent = mVoice->onUnsolicited(unsol, data, length, type);
            break;
        default:
            LOG(DEBUG) << mInstance << ": unhandled unsolicited " << id << " (" << length
                       << " bytes)";
    }
    if (wake && !sent) {
        WakeLock::get().release();
    }
}

}  // namespace aidl::android::hardware::radio::samsung
