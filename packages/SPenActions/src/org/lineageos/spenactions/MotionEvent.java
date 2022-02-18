/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

public class MotionEvent {

    private final Action mAction;
    private final short mDx;
    private final short mDy;

    private static short extractShortValue(byte[] data, int index) {
        return (short) ((data[index] & 0xFF) | ((data[index + 1] & 0xFF) << 8));
    }

    private MotionEvent(Action action, short dx, short dy) {
        mAction = action;
        mDx = dx;
        mDy = dy;
    }

    public enum Action {
        MOVE;
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

    public static MotionEvent fromTypeData(int type, byte[] data) {
        if (type != 0x0F) {
            return null;
        }

        short dx = extractShortValue(data, 1);
        short dy = extractShortValue(data, 3);

        return new MotionEvent(Action.MOVE, dx, dy);
    }
}
