/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <aidl/android/hardware/radio/AccessNetwork.h>
#include <aidl/android/hardware/radio/RadioTechnology.h>
#include <aidl/android/hardware/radio/network/BarringInfo.h>
#include <aidl/android/hardware/radio/network/CellIdentity.h>
#include <aidl/android/hardware/radio/network/CellInfo.h>
#include <aidl/android/hardware/radio/network/EmergencyRegResult.h>
#include <aidl/android/hardware/radio/network/LinkCapacityEstimate.h>
#include <aidl/android/hardware/radio/network/PhysicalChannelConfig.h>
#include <aidl/android/hardware/radio/network/RadioAccessSpecifier.h>
#include <aidl/android/hardware/radio/network/RegStateResult.h>
#include <aidl/android/hardware/radio/network/SignalStrength.h>
#include <samsung_ril/network.h>

#include <optional>
#include <string>
#include <vector>

namespace aidl::android::hardware::radio::samsung {

int toRilRadioAccessNetwork(AccessNetwork accessNetwork);
AccessNetwork toAccessNetwork(int radioAccessNetwork);
int toRilRadioTechnology(AccessNetwork accessNetwork);
RadioTechnology toRadioTechnology(int rat);

int toOperatorStatus(const std::string& status);

std::optional<int> toNetworkType(int32_t networkTypeBitmap);
std::optional<int32_t> toNetworkTypeBitmap(int networkType);

network::CellIdentity toCellIdentity(const SecCellIdentity& identity);
network::RegStateResult toVoiceRegState(const SecVoiceRegState& state);
network::RegStateResult toDataRegState(const SecDataRegState& state);
network::SignalStrength toSignalStrength(const SecSignalStrength& signal);
std::optional<std::vector<network::CellInfo>> toCellInfoList(const void* data, size_t length);
std::vector<network::CellInfo> toCellInfoList(const SecCellInfo* cells, size_t count);
std::optional<std::vector<network::PhysicalChannelConfig>> toPhysicalChannelConfigs(
        const void* data, size_t length);
std::vector<network::BarringInfo> toBarringInfos(const SecBarringInfo& info);
network::LinkCapacityEstimate toLinkCapacityEstimate(const SecLinkCapacityEstimate& lce);
network::EmergencyRegResult toEmergencyRegResult(const SecEmergencyRegResult& result);

bool toSecRadioAccessSpecifier(const network::RadioAccessSpecifier& specifier,
                               SecRadioAccessSpecifier& out);
network::RadioAccessSpecifier toRadioAccessSpecifier(const SecRadioAccessSpecifier& specifier);

}  // namespace aidl::android::hardware::radio::samsung
