/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/messaging/BnRadioMessaging.h>

#include <functional>

#include "common.h"

namespace aidl::android::hardware::radio::samsung {

class RadioMessaging : public messaging::BnRadioMessaging,
                       public RadioService<messaging::IRadioMessagingResponse,
                                           messaging::IRadioMessagingIndication> {
  public:
    explicit RadioMessaging(RadioSlot& slot);

    ::ndk::ScopedAStatus setResponseFunctions(
            const std::shared_ptr<messaging::IRadioMessagingResponse>& response,
            const std::shared_ptr<messaging::IRadioMessagingIndication>& indication) override;
    ::ndk::ScopedAStatus responseAcknowledgement() override;

    ::ndk::ScopedAStatus acknowledgeIncomingGsmSmsWithPdu(int32_t serial, bool success,
                                                          const std::string& ackPdu) override;
    ::ndk::ScopedAStatus acknowledgeLastIncomingCdmaSms(
            int32_t serial, const messaging::CdmaSmsAck& smsAck) override;
    ::ndk::ScopedAStatus acknowledgeLastIncomingGsmSms(
            int32_t serial, bool success, messaging::SmsAcknowledgeFailCause cause) override;
    ::ndk::ScopedAStatus deleteSmsOnRuim(int32_t serial, int32_t index) override;
    ::ndk::ScopedAStatus deleteSmsOnSim(int32_t serial, int32_t index) override;
    ::ndk::ScopedAStatus getCdmaBroadcastConfig(int32_t serial) override;
    ::ndk::ScopedAStatus getGsmBroadcastConfig(int32_t serial) override;
    ::ndk::ScopedAStatus getSmscAddress(int32_t serial) override;
    ::ndk::ScopedAStatus reportSmsMemoryStatus(int32_t serial, bool available) override;
    ::ndk::ScopedAStatus sendCdmaSms(int32_t serial, const messaging::CdmaSmsMessage& sms) override;
    ::ndk::ScopedAStatus sendCdmaSmsExpectMore(int32_t serial,
                                               const messaging::CdmaSmsMessage& sms) override;
    ::ndk::ScopedAStatus sendImsSms(int32_t serial,
                                    const messaging::ImsSmsMessage& message) override;
    ::ndk::ScopedAStatus sendSms(int32_t serial, const messaging::GsmSmsMessage& message) override;
    ::ndk::ScopedAStatus sendSmsExpectMore(int32_t serial,
                                           const messaging::GsmSmsMessage& message) override;
    ::ndk::ScopedAStatus setCdmaBroadcastActivation(int32_t serial, bool activate) override;
    ::ndk::ScopedAStatus setCdmaBroadcastConfig(
            int32_t serial,
            const std::vector<messaging::CdmaBroadcastSmsConfigInfo>& configInfo) override;
    ::ndk::ScopedAStatus setGsmBroadcastActivation(int32_t serial, bool activate) override;
    ::ndk::ScopedAStatus setGsmBroadcastConfig(
            int32_t serial,
            const std::vector<messaging::GsmBroadcastSmsConfigInfo>& configInfo) override;
    ::ndk::ScopedAStatus setSmscAddress(int32_t serial, const std::string& smsc) override;
    ::ndk::ScopedAStatus writeSmsToRuim(int32_t serial,
                                        const messaging::CdmaSmsWriteArgs& cdmaSms) override;
    ::ndk::ScopedAStatus writeSmsToSim(int32_t serial,
                                       const messaging::SmsWriteArgs& smsWriteArgs) override;

    bool onUnsolicited(int id, const void* data, size_t length, RadioIndicationType type);

  private:
    using SendSmsCallback = std::function<void(RIL_Errno error, bool valid,
                                               const messaging::SendSmsResult& result)>;

    void sendGsmSms(int id, const messaging::GsmSmsMessage& message, SendSmsCallback callback);

    RadioSlot& mSlot;
};

}  // namespace aidl::android::hardware::radio::samsung
