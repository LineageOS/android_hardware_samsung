/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include <android/hardware/bluetooth/1.0/IBluetoothHci.h>
#include <android/hardware/bluetooth/1.0/IBluetoothHciCallbacks.h>
#include <bluetooth_address.h>
#include <bluetooth_hci.h>
#include <cutils/properties.h>
#include <fuzzer/FuzzedDataProvider.h>
#include <log/log.h>

#include "bt_vendor.h"

using namespace std;
using ::android::sp;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::bluetooth::V1_0::IBluetoothHci;
using ::android::hardware::bluetooth::V1_0::IBluetoothHciCallbacks;
using ::android::hardware::bluetooth::V1_0::Status;
using ::android::hardware::bluetooth::V1_0::implementation::BluetoothAddress;
using ::android::hardware::bluetooth::V1_0::implementation::BluetoothHci;
using ::android::hardware::bluetooth::V1_0::implementation::FACTORY_BDADDR_PROPERTY;
using ::android::hardware::bluetooth::V1_0::implementation::PERSIST_BDADDR_PROPERTY;
using ::android::hardware::bluetooth::V1_0::implementation::PROPERTY_BT_BDADDR_PATH;

constexpr size_t kMaxPacketSize = 100;
constexpr size_t kMinFdcount = 2;

template <typename T>
const hidl_vec<T> toHidlVec(const std::vector<T>& vec) {
    hidl_vec<T> hVec;
    hVec.setToExternal(const_cast<T*>(vec.data()), vec.size());
    return hVec;
}

class BluetoothHciCallbacks : public IBluetoothHciCallbacks {
  public:
    virtual ~BluetoothHciCallbacks() = default;

    Return<void> initializationComplete(Status status) override {
        if (status == Status::SUCCESS) {
            isInitialized = true;
        } else {
            isInitialized = false;
        }
        return Return<void>();
    };

    Return<void> hciEventReceived(
            const ::android::hardware::hidl_vec<uint8_t>& /*event*/) override {
        return Return<void>();
    };

    Return<void> aclDataReceived(const ::android::hardware::hidl_vec<uint8_t>& /*data*/) override {
        return Return<void>();
    };

    Return<void> scoDataReceived(const ::android::hardware::hidl_vec<uint8_t>& /*data*/) override {
        return Return<void>();
    };
    bool isInitialized;
};

class BluetoothFuzzer {
  public:
    ~BluetoothFuzzer() {
        if (mFdp) {
            delete mFdp;
        }
        mBtHci->close();
        mBtHci.clear();
        for (size_t i = 0; i < mFdCount; ++i) {
            if (mFdList[i]) {
                close(mFdList[i]);
            }
        }
    }
    bool init(const uint8_t* data, size_t size);
    void process();

  private:
    size_t mFdCount = 1;
    int32_t mFdList[CH_MAX] = {0};
    sp<BluetoothHci> mBtHci = nullptr;
    FuzzedDataProvider* mFdp = nullptr;
};

bool BluetoothFuzzer::init(const uint8_t* data, size_t size) {
    mBtHci = sp<BluetoothHci>::make();
    if (!mBtHci) {
        return false;
    }
    mFdp = new FuzzedDataProvider(data, size);
    return true;
}

void BluetoothFuzzer::process() {
    sp<BluetoothHciCallbacks> bluetoothCallback = sp<BluetoothHciCallbacks>::make();

    uint8_t btAddress[BluetoothAddress::kBytes];
    mFdp->ConsumeData(btAddress, sizeof(uint8_t) * BluetoothAddress::kBytes);

    char btAddrString[BluetoothAddress::kStringLength + 1];
    BluetoothAddress::bytes_to_string(btAddress, btAddrString);

    /* property_set() is called so that BluetoothAddress::get_local_address()
     * could return true and the LOG_ALWAYS_FATAL() that aborts the run, if
     * BluetoothAddress::get_local_address() returns false, could be avoided.
     *
     * BluetoothAddress::get_local_address() first searches if
     * PROPERTY_BT_BDADDR_PATH is set, if it fails to get PROPERTY_BT_BDADDR_PATH,
     * it searches for FACTORY_BDADDR_PROPERTY. If it fails to get
     * FACTORY_BDADDR_PROPERTY, it then searches for PERSIST_BDADDR_PROPERTY. If
     * PERSIST_BDADDR_PROPERTY is also not set, it results in an abort.
     */
    property_set(PERSIST_BDADDR_PROPERTY, btAddrString);

    if (mFdp->ConsumeBool()) {
        property_set(FACTORY_BDADDR_PROPERTY, btAddrString);
    }

    if (mFdp->ConsumeBool()) {
        char property[PROPERTY_VALUE_MAX] = {0};
        property_get("ro.vendor.bt.bdaddr_path", property, NULL);
        // get the value of ro.vendor.bt.bdaddr_path and set it to
        // PROPERTY_BT_BDADDR_PATH
        property_set(PROPERTY_BT_BDADDR_PATH, property);
    }

    bool shouldSetH4Protocol = mFdp->ConsumeBool();
    BtVendor* btVendor = BtVendor::getInstance();

    if (!shouldSetH4Protocol) {
        mFdCount = mFdp->ConsumeIntegralInRange<size_t>(kMinFdcount, CH_MAX - 1);
    }

    for (size_t i = 0; i < mFdCount; ++i) {
        mFdList[i] = open("/dev/null", O_RDWR | O_CREAT);
    }

    btVendor->populateFdList(mFdList, mFdCount);
    mBtHci->initialize(bluetoothCallback);

    if (!bluetoothCallback->isInitialized) {
        return;
    }

    std::vector<uint8_t> hciPacket, aclPacket;

    size_t hciPacketSize = mFdp->ConsumeIntegralInRange<size_t>(0, kMaxPacketSize);
    hciPacket = mFdp->ConsumeBytes<uint8_t>(hciPacketSize);
    mBtHci->sendHciCommand(toHidlVec(hciPacket));

    size_t aclPacketSize = mFdp->ConsumeIntegralInRange<size_t>(0, kMaxPacketSize);
    aclPacket = mFdp->ConsumeBytes<uint8_t>(aclPacketSize);
    mBtHci->sendAclData(toHidlVec(aclPacket));

    if (shouldSetH4Protocol) {
        std::vector<uint8_t> scoPacket;
        size_t scoPacketSize = mFdp->ConsumeIntegralInRange<size_t>(0, kMaxPacketSize);
        scoPacket = mFdp->ConsumeBytes<uint8_t>(scoPacketSize);
        mBtHci->sendScoData(toHidlVec(scoPacket));
    }

    btVendor->callRemainingCbacks();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    BluetoothFuzzer bluetoothFuzzer;
    if (bluetoothFuzzer.init(data, size)) {
        bluetoothFuzzer.process();
    }
    return 0;
}
