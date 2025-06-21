/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace aidl {
namespace android {
namespace hardware {
namespace biometrics {
namespace fingerprint {

enum class SemInputDevice : int {
    CURRENT_TSP = 0,
    DEFAULT_TSP = 1,
    EXTRA_TSP = 2,
    SPEN = 11,
    KEY = 21,
    KEYBOARD = 31,
    TAAS = 41,
    UNSPECIFIED = 100,
};

enum class SemInputCommand : int {
    NONE = 0,
    GAME = 1,
    SCAN_RATE = 2,
    REFRESH_RATE = 3,
    GLOVE = 4,
    CLEAR_COVER = 5,
    ORIENTATION = 6,
    PROX_LP_SCAN = 7,
    GRIP_DATA = 8,
    SIP = 9,
    NOTE_APP = 10,
    TEMPERATURE = 11,
    SPAY = 12,
    STYLUS = 13,
    BRUSH = 14,
    AOD_RECT = 15,
    AOD = 16,
    FOD = 17,
    FOD_ICON_VISIBLE = 18,
    FOD_RECT = 19,
    FOD_LP = 20,
    SINGLETAP = 21,
    EAR_DETECT = 22,
    EXTERNAL_NOISE = 23,
    TOUCHABLE_AREA = 24,
    FP_INT_CONTROL = 25,
    SYNC_CHANGED = 26,
    POCKET_MODE = 27,
    LOW_SENSITIVITY = 28,
    CHARGER = 29,
    AOT = 30,
    FOLD_STATE = 31,
    WIRELESS_CHARGER = 32,
    TWO_FINGER_DOUBLETAP = 33,
    SPEN_COVER_TYPE = 34,
    SPEN_SAVING_MODE = 35,
    SPEN_POWER = 36,
    SPEN_BLE_CHARGING = 37,
    SPEN_SCREEN_OFF_MEMO = 38,
    SPEN_PDCT_LOWSENSITIVITY = 39,
    SPEN_LOWCURRENT = 40,
    ALWAYS_LOW_POWER_MODE = 41,
    BEZEL = 42,
    BODY_DETECTION = 43,
    AWD = 44,
    NFC_FIELD = 45,
    SPEN_SET_WIRELESS_CHARGER_TX_ID = 46,
    AOD_NOTI_RECT = 47,
    FAST_RESPONSE = 48,
};

enum class SemInputProperty : int {
    NONE = 0,
    FEATURE = 1,
    CMD_LIST = 2,
    SCRUB_POS = 3,
    FOD_INFO = 4,
    FOD_POS = 5,
    AOD_ACTIVE_AREA = 6,
    AOD_ENABLE = 7,
    EPEN_POS = 8,
    PROX_OFF = 9,
    HW_PARAM = 10,
    LP_DUMP = 11,
    BLE_CHARGING = 12,
    EPEN_SAVING = 13,
    EPEN_MEMO = 14,
    HAND_EDGE = 15,
    EPEN_WCHARGING = 16,
    ENABLED = 17,
    CMD = 18,
};

} // namespace fingerprint
} // namespace biometrics
} // namespace hardware
} // namespace android
} // namespace aidl
