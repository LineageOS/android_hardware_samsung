/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <samsung_ril/ril.h>

#include <cstddef>
#include <cstdint>

enum SecRequest {
    SEC_REQUEST_ENABLE_MODEM = 146,
    SEC_REQUEST_GET_MODEM_STATUS = 147,
    SEC_REQUEST_DEVICE_IMEI = 152,
    SEC_REQUEST_GET_SLOT_STATUS = 200,
    SEC_REQUEST_SET_LOGICAL_TO_PHYSICAL_SLOT_MAPPING = 201,
    SEC_REQUEST_SET_SIGNAL_STRENGTH_REPORTING_CRITERIA = 202,
    SEC_REQUEST_SET_LINK_CAPACITY_REPORTING_CRITERIA = 203,
    SEC_REQUEST_SET_PREFERRED_DATA_MODEM = 204,
    SEC_REQUEST_EMERGENCY_DIAL = 205,
    SEC_REQUEST_GET_PHONE_CAPABILITY = 206,
    SEC_REQUEST_ENABLE_UICC_APPLICATIONS = 208,
    SEC_REQUEST_GET_UICC_APPLICATIONS_ENABLEMENT = 209,
    SEC_REQUEST_SET_SYSTEM_SELECTION_CHANNELS = 210,
    SEC_REQUEST_GET_BARRING_INFO = 211,
    SEC_REQUEST_ENABLE_NR_DUAL_CONNECTIVITY = 213,
    SEC_REQUEST_IS_NR_DUAL_CONNECTIVITY_ENABLED = 214,
    SEC_REQUEST_ALLOCATE_PDU_SESSION_ID = 215,
    SEC_REQUEST_RELEASE_PDU_SESSION_ID = 216,
    SEC_REQUEST_START_HANDOVER = 217,
    SEC_REQUEST_CANCEL_HANDOVER = 218,
    SEC_REQUEST_GET_SYSTEM_SELECTION_CHANNELS = 219,
    SEC_REQUEST_GET_HAL_DEVICE_CAPABILITIES = 220,
    SEC_REQUEST_SET_DATA_THROTTLING = 221,
    SEC_REQUEST_SET_ALLOWED_NETWORK_TYPES_BITMAP = 222,
    SEC_REQUEST_GET_ALLOWED_NETWORK_TYPES_BITMAP = 223,
    SEC_REQUEST_GET_SLICING_CONFIG = 224,
    SEC_REQUEST_ENABLE_VONR = 225,
    SEC_REQUEST_IS_VONR_ENABLED = 226,
    SEC_REQUEST_SET_USAGE_SETTING = 227,
    SEC_REQUEST_GET_USAGE_SETTING = 228,
    SEC_REQUEST_SET_EMERGENCY_MODE = 229,
    SEC_REQUEST_TRIGGER_EMERGENCY_NETWORK_SCAN = 230,
    SEC_REQUEST_CANCEL_EMERGENCY_NETWORK_SCAN = 231,
    SEC_REQUEST_EXIT_EMERGENCY_MODE = 232,
    SEC_REQUEST_SET_NULL_CIPHER_AND_INTEGRITY_ENABLED = 239,
    SEC_REQUEST_SET_N1_MODE_ENABLED = 241,
    SEC_REQUEST_IS_N1_MODE_ENABLED = 242,
    SEC_REQUEST_IS_NULL_CIPHER_AND_INTEGRITY_ENABLED = 245,
    SEC_REQUEST_OEM_SET_VENDOR_SPECIFIC_CONFIGURATION = 10051,
};

// Unsolicited ids arrive as SSRNNNN: SS selects the service, NNNN is the AOSP RIL_UNSOL_* id.
enum SecUnsolService {
    SEC_UNSOL_CONFIG = 20,
    SEC_UNSOL_DATA = 21,
    SEC_UNSOL_MESSAGING = 22,
    SEC_UNSOL_MODEM = 23,
    SEC_UNSOL_NETWORK = 24,
    SEC_UNSOL_SIM = 25,
    SEC_UNSOL_VOICE = 26,
};

constexpr int kSecUnsolServiceDivisor = 100000;

enum SecUnsol {
    SEC_UNSOL_UNTHROTTLE_APN = 1052,
    SEC_UNSOL_ICC_SLOT_STATUS = 1100,
    SEC_UNSOL_EMERGENCY_NUMBER_LIST = 1102,
    SEC_UNSOL_PHYSICAL_CHANNEL_CONFIG = 1101,
    SEC_UNSOL_UICC_APPLICATIONS_ENABLEMENT_CHANGED = 1103,
    SEC_UNSOL_REGISTRATION_FAILED = 1104,
    SEC_UNSOL_BARRING_INFO_CHANGED = 1105,
    SEC_UNSOL_EMERGENCY_NETWORK_SCAN_RESULT = 1106,
};

struct SecCardStatus {
    RIL_CardStatus_v6 base;
    int physicalSlotId;
    // Padding in libsec-ril, never written by it.
    int portId;
    char* atr;
    char* iccid;
    char* eid;
};

static_assert(offsetof(SecCardStatus, physicalSlotId) == 0x198);
static_assert(offsetof(SecCardStatus, eid) == 0x1b0);
static_assert(sizeof(SecCardStatus) == 0x1b8);

constexpr int kSecMaxSlots = 4;
constexpr int kSecMaxPorts = 4;

struct SecSimPortInfo {
    char* iccid;
    int logicalSlot;
    int portState;
};

struct SecSimSlotStatus {
    int cardState;
    char* atr;
    char* eid;
    int numPorts;
    int mepMode;
    SecSimPortInfo ports[kSecMaxPorts];
    char reserved[0x10];
};

struct SecSimSlotStatusList {
    int numSlots;
    SecSimSlotStatus slots[kSecMaxSlots];
};

static_assert(sizeof(SecSimSlotStatus) == 0x70);
static_assert(offsetof(SecSimSlotStatusList, slots) == 0x8);
static_assert(sizeof(SecSimSlotStatusList) == 0x1c8);

struct SecSlotMapping {
    int physicalSlot;
    int port;
};

struct SecPhoneCapability {
    uint8_t maxActiveData;
    uint8_t maxActiveInternetData;
    uint8_t isInternetLingeringSupported;
    int numLogicalModems;
    uint8_t* logicalModemIds;
};

static_assert(sizeof(SecPhoneCapability) == 0x10);

struct SecImei {
    int type;
    char imei[0x22];
    char svn[0x22];
};

static_assert(sizeof(SecImei) == 0x48);

constexpr int kSecMaxVendorConfigurations = 14;

struct SecVendorConfiguration {
    int count;
    struct {
        char* name;
        char* value;
    } entries[kSecMaxVendorConfigurations];
};

static_assert(sizeof(SecVendorConfiguration) == 0xe8);
