//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

package vendor.samsung.hardware.health;

import vendor.samsung.hardware.health.SehHealthInfo;

@VintfStability
interface ISehHealthInfoCallback {
    oneway void healthInfoChanged(in SehHealthInfo info);
}
