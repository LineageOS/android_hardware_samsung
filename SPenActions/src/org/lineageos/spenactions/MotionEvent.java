/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

public class MotionEvent {

    public enum Action {
        MOVE;
    }

    private final Action mAction;
    private final short mDx;
    private final short mDy;

    private MotionEvent(Action action, short dx, short dy) {
        this.mAction = action;
        this.mDx = dx;
        this.mDy = dy;
    }

    public Action getAction() {
        return mAction;
    }

    public short getDX() {
        return mDx;
    }

    public short getDY() {
        return mDy;
    }

    private static short extractShortValue(byte[] bArr, int i) {
        return (short) ((bArr[i] & 255) | ((bArr[i + 1] & 255) << 8));
    }

    public static MotionEvent fromTypeData(int type, byte[] value) {
        switch (type) {
            case 15:
                return new MotionEvent(Action.MOVE,
                        extractShortValue(value, 1),
                        extractShortValue(value, 3));
            default:
                return null;
        }
    }
}
