/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <samsung_ril/VendorRil.h>

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace aidl::android::hardware::radio::samsung {

class RadioConfig;
class RadioModem;
class RadioNetwork;
class RadioSim;

class RadioSlot {
  public:
    using VendorRil = ::android::hardware::radio::samsung::VendorRil;
    using ResponseCallback = VendorRil::ResponseCallback;

    RadioSlot(RIL_SOCKET_ID socket, std::shared_ptr<RadioConfig> config);

    void publish();

    void request(int id, const void* data, size_t length, ResponseCallback callback);
    void request(int id, ResponseCallback callback);
    void requestInts(int id, std::vector<int> ints, ResponseCallback callback);
    void requestString(int id, const std::string& str, ResponseCallback callback);
    void requestStrings(int id, const std::vector<std::string>& strings, ResponseCallback callback,
                        bool allowEmpty = true);

    // libsec-ril holds back unsolicited reports until it has seen FW_READY.
    void setVendorConfiguration(std::vector<std::pair<std::string, std::string>> config);

    RIL_SOCKET_ID socket() const { return mSocket; }
    const std::string& instance() const { return mInstance; }

  private:
    void onUnsolicited(int id, const void* data, size_t length);
    void sendVendorConfiguration();

    RIL_SOCKET_ID mSocket;
    std::string mInstance;

    std::mutex mVendorConfigMutex;
    std::vector<std::pair<std::string, std::string>> mVendorConfig;
    bool mVendorConfigPending = false;

    std::shared_ptr<RadioConfig> mConfig;
    std::shared_ptr<RadioModem> mModem;
    std::shared_ptr<RadioNetwork> mNetwork;
    std::shared_ptr<RadioSim> mSim;
};

}  // namespace aidl::android::hardware::radio::samsung
