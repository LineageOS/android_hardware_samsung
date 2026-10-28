/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <samsung_ril/structs.h>

// RIL_Call with Samsung's audio quality and forwarded number appended.
struct SecCall {
    int state;
    int index;
    int toa;
    char isMpty;
    char isMT;
    char als;
    char isVoice;
    char isVoicePrivacy;
    char* number;
    int numberPresentation;
    char* name;
    int namePresentation;
    RIL_UUS_Info* uusInfo;
    int audioQuality;
    char* forwardedNumber;
};

static_assert(offsetof(SecCall, isVoicePrivacy) == 0x10);
static_assert(offsetof(SecCall, number) == 0x18);
static_assert(offsetof(SecCall, uusInfo) == 0x38);
static_assert(offsetof(SecCall, audioQuality) == 0x40);
static_assert(sizeof(SecCall) == 0x50);

struct SecEmergencyDial {
    RIL_Dial dialInfo;
    int categories;
    int routing;
    char* urn;
    int hasKnownUserIntentEmergency;
    int isTesting;
};

static_assert(offsetof(SecEmergencyDial, categories) == 0x18);
static_assert(offsetof(SecEmergencyDial, urn) == 0x20);
static_assert(sizeof(SecEmergencyDial) == 0x30);

struct SecEmergencyNumber {
    char number[0x54];
    int sources;
    int categories;
    char mcc[4];
    char mnc[4];
    char urn[0x54];
};

static_assert(offsetof(SecEmergencyNumber, sources) == 0x54);
static_assert(offsetof(SecEmergencyNumber, mcc) == 0x5c);
static_assert(offsetof(SecEmergencyNumber, urn) == 0x64);
static_assert(sizeof(SecEmergencyNumber) == 0xb8);
