/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions;

public class ButtonAction {

    public enum Action {
        UP, DOWN;
    }

    private final Action mAction;

    private ButtonAction(Action action) {
        this.mAction = action;
    }

    public Action getAction() {
        return mAction;
    }

    public static ButtonAction fromType(int type) {
        switch (type) {
            case 0: return new ButtonAction(Action.UP);
            case 3: return new ButtonAction(Action.DOWN);
            default: return null;
        }
    }

}
