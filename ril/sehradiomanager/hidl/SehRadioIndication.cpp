/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SehRadioIndication"

// #define DEBUG

#include "SehRadioIndication.h"

#include <android-base/logging.h>

namespace vendor {
namespace samsung {
namespace hardware {
namespace radio {
namespace V2_2 {
namespace implementation {

using ::android::hardware::Void;

void l(std::string line) {
#ifdef DEBUG
    LOG(INFO) << line;
#endif
}

Return<void> SehRadioIndication::acbInfoChanged(int32_t type, const hidl_vec<int32_t>& acbInfo) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::csFallback(int32_t type, int32_t state) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::imsPreferenceChanged(int32_t type,
                                                      const hidl_vec<int32_t>& imsPref) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::voiceRadioBearerHandoverStatusChanged(int32_t type,
                                                                       int32_t state) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::timerStatusChangedInd(int32_t type,
                                                       const hidl_vec<int32_t>& eventNoti) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::modemCapabilityIndication(int32_t type,
                                                           const hidl_vec<int8_t>& data) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::needTurnOnRadioIndication(int32_t type) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::simPhonebookReadyIndication(int32_t type) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::phonebookInitCompleteIndication(int32_t type) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::deviceReadyNoti(int32_t type) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::stkSmsSendResultIndication(int32_t type, int32_t result) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::stkCallControlResultIndication(int32_t type,
                                                                const hidl_string& cmd) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::simSwapStateChangedIndication(int32_t type, int32_t state) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::simCountMismatchedIndication(int32_t type, int32_t state) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::simOnOffStateChangedNotify(int32_t type, int32_t mode) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::releaseCompleteMessageIndication(
        int32_t type, const V2_0::SehSsReleaseComplete& result) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::sapNotify(int32_t type, const hidl_vec<int8_t>& data) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::nrBearerAllocationChanged(int32_t type, int32_t status) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::nrNetworkTypeAdded(int32_t type, int32_t status) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::rrcStateChanged(int32_t type, const V2_0::SehRrcStateInfo& state) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::configModemCapabilityChangeNoti(
        int32_t type, const V2_0::SehConfigModemCapability& configModemCapa) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::needApnProfileIndication(const hidl_string& select) {
    l(__func__);
    return Void();
}

Return<int32_t> SehRadioIndication::needSettingValueIndication(const hidl_string& key,
                                                               const hidl_string& table) {
    l(__func__);
    return -1;
}

Return<void> SehRadioIndication::execute(int32_t type, const hidl_string& cmd) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::signalLevelInfoChanged(int32_t type,
                                                        const V2_0::SehSignalBar& signalBarInfo) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::extendedRegistrationState(
        int32_t type, const V2_0::SehExtendedRegStateResult& state) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::needPacketUsage(const hidl_string& iface,
                                                 needPacketUsage_cb hidl_cb) {
    V2_0::SehPacketUsage usage;
    hidl_cb(0, usage);
    return Void();
}

Return<void> SehRadioIndication::nrIconTypeChanged(uint32_t type, uint32_t nrIconType) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::callDetailsChanged(
        uint32_t type, const hidl_vec<V2_0::SehCallDetails>& callDetails) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::vendorConfigurationChanged(
        uint32_t type, const hidl_vec<SehVendorConfiguration>& configurations) {
    l(__func__);
    return Void();
}

Return<void> SehRadioIndication::eriInfoReceived(uint32_t type, const SehEriInfo& eriInfo) {
    l(__func__);
    return Void();
}

}  // namespace implementation
}  // namespace V2_2
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
