/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.graphics.PorterDuff;
import android.util.AttributeSet;
import android.util.Slog;
import android.view.Display;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/**
 * Draws the optical UDFPS illumination circle on a transparent {@link SurfaceView} positioned at
 * the hardware sensor coordinates.
 *
 * <p>Sits above the dim layer via {@code setZOrderOnTop(true)} and {@code PixelFormat.TRANSPARENT}.
 * Visibility toggles between {@code VISIBLE} and {@code INVISIBLE} on {@link #show()}/{@link
 * #hide()} so no stale surface remains active when not illuminating.
 */
public class LightSourceView extends SurfaceView implements SurfaceHolder.Callback {
    private static final String TAG = "SB_LightSourceView";

    private final SurfaceHolder mHolder;
    private final Paint mPaint;
    private final SurfaceControl.Transaction mSurfaceTransaction;

    private boolean mIsShown = false;
    private boolean mHasPendingDraw = false;

    private int mPosX;
    private int mPosY;
    private int mSize;
    private int mLastRotation = -1;

    private FingerprintSensorInfo mSensorInfo;

    public LightSourceView(Context context) {
        super(context);
        mSurfaceTransaction = new SurfaceControl.Transaction();
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mHolder = getHolder();
        init();
    }

    public LightSourceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mSurfaceTransaction = new SurfaceControl.Transaction();
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mHolder = getHolder();
        init();
    }

    public LightSourceView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        mSurfaceTransaction = new SurfaceControl.Transaction();
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        mHolder = getHolder();
        init();
    }

    private void init() {
        mHolder.addCallback(this);
        mHolder.setFormat(PixelFormat.TRANSPARENT);
        setZOrderOnTop(true);
        setVisibility(INVISIBLE);
    }

    // ---- Visibility & configuration ----

    public void setSensorInfo(FingerprintSensorInfo sensorInfo) {
        mSensorInfo = sensorInfo;
        setLightSourceInfo();
    }

    public void show() {
        if (mSensorInfo == null) {
            Slog.w(TAG, "show() called before setSensorInfo — ignoring");
            return;
        }
        int rotation = getDisplayRotation();
        if (mLastRotation != rotation) {
            setLightSourceInfo();
        }
        mIsShown = true;
        if (getVisibility() != VISIBLE) {
            setVisibility(VISIBLE);
        }
        draw();
    }

    public void hide() {
        mIsShown = false;
        mHasPendingDraw = false;
        if (mHolder != null && mHolder.getSurface() != null && mHolder.getSurface().isValid()) {
            draw();
        }
        if (getVisibility() != INVISIBLE) {
            setVisibility(INVISIBLE);
        }
    }

    public boolean isLightShown() { return mIsShown; }

    // ---- SurfaceHolder.Callback ----

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        applyTrustedOverlay();
        if (mHasPendingDraw || mIsShown) {
            draw();
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        applyTrustedOverlay();
        if (mHasPendingDraw || mIsShown) {
            draw();
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mHasPendingDraw = false;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        Slog.i(TAG, "onConfigurationChanged: " + newConfig);
        if (mSensorInfo != null) {
            setLightSourceInfo();
            if (mIsShown)
                draw();
        }
    }

    // ---- Rendering ----

    private void applyTrustedOverlay() {
        SurfaceControl sc = getSurfaceControl();
        if (sc != null && sc.isValid()) {
            mSurfaceTransaction.setTrustedOverlay(sc, true).apply();
        }
    }

    private void setLightSourceInfo() {
        if (mSensorInfo == null)
            return;
        mLastRotation = getDisplayRotation();
        Point displaySize = getDisplaySize();
        Point pos = mSensorInfo.getSensorImagePoint(displaySize, mLastRotation);
        mPosX = pos.x;
        mPosY = pos.y;
        mSize = mSensorInfo.sensorImageSize;
        mPaint.setColor(parseLightColor(mSensorInfo.lightColor));
        Slog.i(TAG,
                "setLightSourceInfo: rotation=" + mLastRotation + " pos=(" + mPosX + "," + mPosY
                        + ") size=" + mSize + " color=0x"
                        + Integer.toHexString(mPaint.getColor()));
    }

    private int parseLightColor(String colorStr) {
        if (colorStr != null && !colorStr.trim().isEmpty()) {
            try {
                String hex = colorStr.trim();
                if (hex.startsWith("#")) {
                    hex = hex.substring(1);
                } else if (hex.startsWith("0x") || hex.startsWith("0X")) {
                    hex = hex.substring(2);
                }
                if (hex.length() == 6) {
                    hex = "FF" + hex;
                }
                return (int) Long.parseLong(hex, 16);
            } catch (Exception e) {
                Slog.w(TAG, "Failed to parse light color: " + colorStr + ", falling back to WHITE",
                        e);
            }
        }
        return Color.WHITE;
    }

    private void draw() {
        if (mHolder == null) {
            Slog.w(TAG, "draw: mHolder is null");
            return;
        }

        if (mHolder.getSurface() == null || !mHolder.getSurface().isValid()) {
            if (mIsShown) {
                Slog.d(TAG, "draw: surface not ready, pending");
                mHasPendingDraw = true;
            }
            return;
        }

        Canvas canvas = null;
        try {
            mHasPendingDraw = false;
            canvas = mHolder.lockCanvas(null);

            if (canvas == null) {
                Slog.d(TAG, "draw: lockCanvas returned null — surface not ready, pending");
                if (mIsShown)
                    mHasPendingDraw = true;
                return;
            }

            canvas.drawColor(0, PorterDuff.Mode.CLEAR);

            if (mIsShown && mSize > 0) {
                float cx = mPosX + mSize / 2.0f;
                float cy = mPosY + mSize / 2.0f;
                float radius = mSize / 2.0f;
                canvas.drawCircle(cx, cy, radius, mPaint);
                Slog.d(TAG, "draw: circle at (" + cx + "," + cy + ") r=" + radius);
            }

        } finally {
            if (canvas != null) {
                mHolder.unlockCanvasAndPost(canvas);
            }
        }
    }

    private int getDisplayRotation() {
        Display display = getDisplay();
        return display != null ? display.getRotation() : 0;
    }

    private Point getDisplaySize() {
        Point size = new Point();
        Display display = getDisplay();
        if (display != null) {
            display.getRealSize(size);
        }
        return size;
    }
}
