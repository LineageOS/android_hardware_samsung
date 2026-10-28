/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <telephony/ril.h>

// These collide with constants of the same name in the AIDL radio headers.
#undef CDMA_ALPHA_INFO_BUFFER_LENGTH
#undef CDMA_NUMBER_INFO_BUFFER_LENGTH
#undef MAX_RILDS
#undef MAX_UUID_LENGTH
#undef NUM_SERVICE_CLASSES
#undef SS_INFO_MAX

// libsec-ril inserts onPhoneDied before getVersion.
struct SecRilFunctions {
    int version;
    RIL_RequestFunc onRequest;
    RIL_RadioStateRequest onStateRequest;
    RIL_Supports supports;
    RIL_Cancel onCancel;
    void (*onPhoneDied)(int slot);
    RIL_GetVersion getVersion;
};
