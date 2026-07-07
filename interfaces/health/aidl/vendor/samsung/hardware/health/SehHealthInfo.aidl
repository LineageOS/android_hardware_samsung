//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

package vendor.samsung.hardware.health;

import android.hardware.health.HealthInfo;

@VintfStability
parcelable SehHealthInfo {
    android.hardware.health.HealthInfo aospHealthInfo;
    int batteryCurrentNow;
    int batteryOnline;
    int batteryChargeType;
    boolean batteryPowerSharingOnline;
    boolean chargerPogoOnline;
    int batteryHighVoltageCharger;
    int batteryEvent;
    int batteryCurrentEvent;
    boolean chargerOtgOnline;
    int wirelessPowerSharingTxEvent;
}
