/*
 * Copyright (C) 2021 The Android Open Source Project
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

#define LOG_TAG "AGnssRilAidl"

#include "AGnssRil.h"
#include <aidl/android/hardware/gnss/BnGnss.h>
#include <inttypes.h>
#include <log/log.h>

namespace aidl::android::hardware::gnss {

std::shared_ptr<IAGnssRilCallback> AGnssRil::sCallback = nullptr;

ndk::ScopedAStatus AGnssRil::setCallback(const std::shared_ptr<IAGnssRilCallback>& callback) {
    ALOGD("AGnssRil::setCallback");
    std::unique_lock<std::mutex> lock(mMutex);
    sCallback = callback;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AGnssRil::setRefLocation(const AGnssRefLocation& agnssReflocation) {
    const AGnssRefLocationCellID& cellInfo = agnssReflocation.cellID;
    ALOGD("AGnssRil::setRefLocation: type: %s, mcc: %d, mnc: %d, lac: %d, cid: %" PRId64
          ", tac: %d, pcid: "
          "%d, arfcn: %d",
          toString(agnssReflocation.type).c_str(), cellInfo.mcc, cellInfo.mnc, cellInfo.lac,
          cellInfo.cid, cellInfo.tac, cellInfo.pcid, cellInfo.arfcn);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AGnssRil::setSetId(SetIdType type, const std::string& setid) {
    ALOGD("AGnssRil::setSetId: type:%s, setid: %s", toString(type).c_str(), setid.c_str());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AGnssRil::updateNetworkState(const NetworkAttributes& attributes) {
    ALOGD("AGnssRil::updateNetworkState: networkHandle:%" PRId64
          ", isConnected: %d, capabilities: %d, "
          "apn: %s",
          attributes.networkHandle, attributes.isConnected, attributes.capabilities,
          attributes.apn.c_str());
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus AGnssRil::injectNiSuplMessageData(const std::vector<uint8_t>& msgData,
                                                     int slotIndex) {
    ALOGD("AGnssRil::injectNiSuplMessageData: msgData:%d bytes slotIndex:%d",
          static_cast<int>(msgData.size()), slotIndex);
    if (msgData.size() > 0) {
        return ndk::ScopedAStatus::ok();
    } else {
        return ndk::ScopedAStatus::fromServiceSpecificError(IGnss::ERROR_INVALID_ARGUMENT);
    }
}

}  // namespace aidl::android::hardware::gnss
