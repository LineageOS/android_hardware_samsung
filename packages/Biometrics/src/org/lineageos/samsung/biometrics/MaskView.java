/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

public final class MaskView implements DisplayBrightnessMonitor.OnBrightnessListener {
    private static final String TAG = "SB_MaskView";
    private static final int DEFAULT_BRIGHTNESS = 127;
    private static final float TARGET_BRIGHTNESS = 319.0f; // Hardcoded!!!
    private static final float BRIGHTNESS_CONTROL_FACTOR = 1.0f;

    private final Context mContext;
    private final WindowManager mWm;
    private final Handler mMain = new Handler(Looper.getMainLooper());
    private final DisplayBrightnessMonitor mDisplayBrightnessMonitor;
    private final DisplaySolution mDisplaySolution;

    private View mRoot;
    private boolean mAdded;

    public MaskView(Context context) {
        mContext = context;
        mWm = context.getSystemService(WindowManager.class);
        mDisplayBrightnessMonitor = DisplayBrightnessMonitor.getInstance();
        mDisplaySolution = new DisplaySolution(context);
        mDisplayBrightnessMonitor.registerListener(this);
    }

    public void stop() {
        mDisplayBrightnessMonitor.unregisterListener(this);
    }

    @Override
    public void onBrightnessChanged(int brightness) {
        updateBackgroundColor(brightness);
    }

    public void show() {
        mMain.post(() -> {
            if (mAdded) return;
            ensureView();
            updateBackgroundColor(-1);
            mWm.addView(mRoot, buildLayoutParams());
            mAdded = true;
        });
    }

    public void hide() {
        mMain.post(() -> {
            if (!mAdded) return;
            try {
                mWm.removeViewImmediate(mRoot);
            } finally {
                mAdded = false;
            }
        });
    }

    public boolean isShowing() {
        return mAdded;
    }

    private void ensureView() {
        if (mRoot != null) return;

        mRoot = new FrameLayout(mContext);
        mRoot.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
    }

    private WindowManager.LayoutParams buildLayoutParams() {
        Point size = new Point();
        Display d = mContext.getDisplay();
        if (d != null) {
            d.getRealSize(size);
        } else {
            size.x = WindowManager.LayoutParams.MATCH_PARENT;
            size.y = WindowManager.LayoutParams.MATCH_PARENT;
        }

        final int flags = WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED
                | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;

        int type = WindowManager.LayoutParams.TYPE_DISPLAY_OVERLAY;

        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                size.x,
                size.y,
                type,
                flags,
                PixelFormat.TRANSLUCENT
        );

        lp.privateFlags =
                WindowManager.LayoutParams.PRIVATE_FLAG_TRUSTED_OVERLAY
                        | WindowManager.LayoutParams.PRIVATE_FLAG_IS_ROUNDED_CORNERS_OVERLAY;
        // lp.gravity = Gravity.TOP | Gravity.START;
        lp.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        lp.setFitInsetsTypes(0);
        lp.setTitle("Dim Layer for UDFPS");

        return lp;
    }

    public void updateBackgroundColor(int displayBrightness) {
        if (mRoot == null) return;

        if (displayBrightness < 0) {
            displayBrightness = mDisplayBrightnessMonitor.getCurrentBrightness();
        }
        if (displayBrightness < 0) {
            displayBrightness = DEFAULT_BRIGHTNESS;
        }

        float alpha = mDisplaySolution.getAlphaMaskLevel(displayBrightness, TARGET_BRIGHTNESS,
                BRIGHTNESS_CONTROL_FACTOR);
        int maskAlpha = (int) (255f * alpha);
        int argb = (maskAlpha << 24);

        Log.i(TAG, "updateBackgroundColor: " + Integer.toHexString(argb) + ", " + TARGET_BRIGHTNESS
                + ", " + maskAlpha);
        mRoot.setBackgroundColor(argb);
    }
}
