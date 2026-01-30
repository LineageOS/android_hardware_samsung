/*
 * Copyright (C) 2024 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "SehRadioNetworkIndication"

// #define DEBUG

#include "SehRadioNetworkIndication.h"

#include <android-base/logging.h>

namespace aidl {
namespace vendor {
namespace samsung {
namespace hardware {
namespace radio {
namespace network {
namespace implementation {

void l(std::string line) {
#ifdef DEBUG
    LOG(INFO) << line;
#endif
}

ndk::ScopedAStatus SehRadioNetworkIndication::acbInfo(int32_t type,
                                                      const std::vector<int32_t>& acbInfo) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::callDetailsChanged(
        int32_t type, const std::vector<SehCallDetails>& callDetails) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::csFallback(int32_t type, int32_t state) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::currentNetworkScanIsrequested(int32_t type,
                                                                            int8_t mode) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::eriInfoReceived(int32_t type,
                                                              const SehEriInfo& eriInfo) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::execute(int32_t type, const std::string& cmd) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::extendedRegistrationState(
        int32_t type, const SehExtendedRegStateResult& state) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::imsPreferenceChanged(
        int32_t type, const std::vector<int32_t>& imsPref) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::needTurnOnRadioIndication(int32_t type) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::nrBearerAllocationChanged(int32_t type,
                                                                        int32_t status) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::nrIconTypeChanged(int32_t type, int32_t nrIconType) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::nrNetworkTypeAdded(int32_t type,
                                                                 int32_t nrNetworkType) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::roamingNetworkScanIsRequested(
        int32_t type, const std::vector<uint8_t>& scanData) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::signalLevelInfoChanged(
        int32_t type, const SehSignalBar& signalBarInfo) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus SehRadioNetworkIndication::vendorConfigurationChanged(
        int32_t type, const std::vector<SehVendorConfiguration>& configurations) {
    l(__func__);
    return ndk::ScopedAStatus::ok();
}

}  // namespace implementation
}  // namespace network
}  // namespace radio
}  // namespace hardware
}  // namespace samsung
}  // namespace vendor
}  // namespace aidl
