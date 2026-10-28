/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/data/BnRadioData.h>

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

class RadioData : public data::BnRadioData,
                  public RadioService<data::IRadioDataResponse, data::IRadioDataIndication> {
  public:
    explicit RadioData(RadioSlot& slot);

    ::ndk::ScopedAStatus setResponseFunctions(
            const std::shared_ptr<data::IRadioDataResponse>& response,
            const std::shared_ptr<data::IRadioDataIndication>& indication) override;
    ::ndk::ScopedAStatus responseAcknowledgement() override;

    ::ndk::ScopedAStatus allocatePduSessionId(int32_t serial) override;
    ::ndk::ScopedAStatus cancelHandover(int32_t serial, int32_t callId) override;
    ::ndk::ScopedAStatus deactivateDataCall(int32_t serial, int32_t cid,
                                            data::DataRequestReason reason) override;
    ::ndk::ScopedAStatus getDataCallList(int32_t serial) override;
    ::ndk::ScopedAStatus getSlicingConfig(int32_t serial) override;
    ::ndk::ScopedAStatus releasePduSessionId(int32_t serial, int32_t id) override;
    ::ndk::ScopedAStatus setDataAllowed(int32_t serial, bool allow) override;
    ::ndk::ScopedAStatus setDataProfile(
            int32_t serial, const std::vector<data::DataProfileInfo>& profiles) override;
    ::ndk::ScopedAStatus setDataThrottling(int32_t serial,
                                           data::DataThrottlingAction dataThrottlingAction,
                                           int64_t completionDurationMillis) override;
    ::ndk::ScopedAStatus setInitialAttachApn(
            int32_t serial, const std::optional<data::DataProfileInfo>& dataProfileInfo) override;
    ::ndk::ScopedAStatus setupDataCall(int32_t serial, AccessNetwork accessNetwork,
                                       const data::DataProfileInfo& dataProfileInfo,
                                       bool roamingAllowed, data::DataRequestReason reason,
                                       const std::vector<data::LinkAddress>& addresses,
                                       const std::vector<std::string>& dnses, int32_t pduSessionId,
                                       const std::optional<data::SliceInfo>& sliceInfo,
                                       bool matchAllRuleAllowed) override;
    ::ndk::ScopedAStatus startHandover(int32_t serial, int32_t callId) override;
    ::ndk::ScopedAStatus startKeepalive(int32_t serial,
                                        const data::KeepaliveRequest& keepalive) override;
    ::ndk::ScopedAStatus stopKeepalive(int32_t serial, int32_t sessionHandle) override;
    ::ndk::ScopedAStatus setUserDataEnabled(int32_t serial, bool enabled) override;
    ::ndk::ScopedAStatus setUserDataRoamingEnabled(int32_t serial, bool enabled) override;
    ::ndk::ScopedAStatus notifyImsDataNetwork(int32_t serial,
                                              const data::ImsDataNetworkInfo& info) override;

    bool onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type);

  private:
    RadioSlot& mSlot;
};

}  // namespace aidl::android::hardware::radio::samsung
