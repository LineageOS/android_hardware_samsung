/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

import java.util.Set;
import java.util.UUID;

public final class SPenIdentity {

    // List of known S Pen GATT services
    private static final Set<UUID> SERVICE_UUIDS = Set.of(
            UUID.fromString("0000fd6c-0000-1000-8000-00805f9b34fb"), // EXT1/CANVAS/STICKY
            UUID.fromString("dc6bb0a8-202e-487c-8f1b-53b37cc837c6"), // davinci
            UUID.fromString("edfec62e-9910-0bac-5241-d8bda6932a2f"), // crown/built-in
            UUID.fromString("edfec62e-9910-0bac-5241-d8bda6932a30")  // add-on
    );

    private SPenIdentity() {
    }

    public static boolean isSPenService(UUID uuid) {
        return SERVICE_UUIDS.contains(uuid);
    }

    public static Set<UUID> getServiceUUIDs() {
        return SERVICE_UUIDS;
    }
}
