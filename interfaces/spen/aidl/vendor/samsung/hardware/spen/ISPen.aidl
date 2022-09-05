/*
 * SPDX-FileCopyrightText: 2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package vendor.samsung.hardware.spen;

@VintfStability
interface ISPen {
    boolean isCharging();
    boolean setCharging(boolean charge);
    String getMACAddress();
    void setMACAddress(String mac);
}
