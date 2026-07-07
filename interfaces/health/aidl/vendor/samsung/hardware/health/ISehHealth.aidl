//
// SPDX-FileCopyrightText: 2026 The LineageOS Project
// SPDX-License-Identifier: Apache-2.0
//

package vendor.samsung.hardware.health;

import vendor.samsung.hardware.health.ISehHealthInfoCallback;

@VintfStability
interface ISehHealth {
    void registerCallback(in ISehHealthInfoCallback callback);
    void unregisterCallback(in ISehHealthInfoCallback callback);
    void update();
    void sehWriteEnableToParam(in int arg0, in boolean arg1);
}
