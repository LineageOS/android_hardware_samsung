/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.PorterDuff;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.WindowManager;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Full-screen opaque black {@link SurfaceView} overlay shown briefly after optical fingerprint
 * authentication succeeds from Screen-Off or AOD to mask the panel brightness transition.
 *
 * <p>Uses an initial {@code 70 ms} timeout that extends in {@code 35 ms} steps (up to
 * {@code 350 ms}) while the display is still leaving AOD/Doze, and dismisses early once
 * interactive brightness settles in {@code STATE_ON}.
 */
public final class BlackMaskView implements DisplayBrightnessMonitor.OnBrightnessListener {

    private static final String TAG = "SB_BlackMaskView";
    private static final long INITIAL_TIMEOUT_MS = 70L;
    private static final long AOD_EXTEND_MS = 35L;
    private static final long MAX_TOTAL_TIMEOUT_MS = 350L;

    private final Context mContext;
    private final WindowManager mWm;
    private final Handler mMain = new Handler(Looper.getMainLooper());
    private final DisplayBrightnessMonitor mBrightnessMonitor;
    private final AtomicBoolean mShowing = new AtomicBoolean(false);

    private DisplayStateListener mDisplayStateListener;
    private BlackSurfaceView mSurfaceView;
    private int mInitialBrightness = -1;
    private long mElapsedTimeoutMs = 0L;

    private final Runnable mTimeoutRunnable = this::onTimeoutTick;

    public BlackMaskView(Context context) {
        mContext = context;
        mWm = context.getSystemService(WindowManager.class);
        mBrightnessMonitor = DisplayBrightnessMonitor.getInstance();
    }

    public void setDisplayStateListener(DisplayStateListener listener) {
        mDisplayStateListener = listener;
    }

    // ---- Visibility ----

    public void show() {
        if (!mShowing.compareAndSet(false, true)) {
            Log.d(TAG, "show: already showing");
            return;
        }

        mInitialBrightness = mBrightnessMonitor.getCurrentBrightness();
        mElapsedTimeoutMs = INITIAL_TIMEOUT_MS;

        mMain.post(() -> {
            if (!mShowing.get()) return;
            ensureView();
            try {
                mSurfaceView.setBlackMode(true);
                mWm.addView(mSurfaceView, buildLayoutParams());
                mBrightnessMonitor.registerListener(this);
                mMain.removeCallbacks(mTimeoutRunnable);
                mMain.postDelayed(mTimeoutRunnable, INITIAL_TIMEOUT_MS);
                Log.i(TAG, "show: black mask visible (initialBl=" + mInitialBrightness + ")");
            } catch (Exception e) {
                Log.e(TAG, "Failed to add black mask view to WindowManager", e);
                mShowing.set(false);
            }
        });
    }

    public void hide() {
        if (!mShowing.compareAndSet(true, false)) return;
        mMain.post(() -> {
            mMain.removeCallbacks(mTimeoutRunnable);
            mBrightnessMonitor.unregisterListener(this);
            if (mSurfaceView != null) {
                try {
                    mSurfaceView.setBlackMode(false);
                    mWm.removeViewImmediate(mSurfaceView);
                } catch (Exception e) {
                    Log.w(TAG, "hide: removeView failed: " + e.getMessage());
                } finally {
                    mSurfaceView = null;
                }
            }
            Log.i(TAG, "hide: black mask removed");
        });
    }

    public boolean isShowing() {
        return mShowing.get();
    }

    // ---- Brightness & timeout ----

    @Override
    public void onBrightnessChanged(int brightness) {
        if (!mShowing.get()) return;
        boolean displayOn = mDisplayStateListener == null
                || (mDisplayStateListener.isScreenOn() && mDisplayStateListener.isInteractive());
        if (displayOn && mInitialBrightness >= 0 && brightness != mInitialBrightness) {
            Log.d(TAG, "onBrightnessChanged: " + mInitialBrightness + " -> " + brightness
                    + " (STATE_ON) — hiding black mask early");
            hide();
        }
    }

    private void onTimeoutTick() {
        if (!mShowing.get()) return;
        boolean stillInAod = mDisplayStateListener != null
                && (!mDisplayStateListener.isScreenOn() || !mDisplayStateListener.isInteractive());
        if (stillInAod && mElapsedTimeoutMs < MAX_TOTAL_TIMEOUT_MS) {
            mElapsedTimeoutMs += AOD_EXTEND_MS;
            Log.d(TAG, "Display still transitioning from AOD; extending black mask by "
                    + AOD_EXTEND_MS + "ms (total=" + mElapsedTimeoutMs + "ms)");
            mMain.postDelayed(mTimeoutRunnable, AOD_EXTEND_MS);
            return;
        }
        hide();
    }

    // ---- Surface overlay ----

    private void ensureView() {
        if (mSurfaceView != null) return;
        mSurfaceView = new BlackSurfaceView(mContext);
    }

    private WindowManager.LayoutParams buildLayoutParams() {
        final int flags = WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED
                | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED;

        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_DISPLAY_OVERLAY,
                flags,
                PixelFormat.TRANSLUCENT);

        lp.flags &= ~WindowManager.LayoutParams.FLAG_LAYOUT_INSET_DECOR;
        lp.privateFlags =
                WindowManager.LayoutParams.PRIVATE_FLAG_TRUSTED_OVERLAY
                        | WindowManager.LayoutParams.PRIVATE_FLAG_IS_ROUNDED_CORNERS_OVERLAY;
        lp.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        lp.setFitInsetsTypes(0);
        lp.setTitle("FP BlackMaskView");
        return lp;
    }

    private static final class BlackSurfaceView extends SurfaceView
            implements SurfaceHolder.Callback {
        private final SurfaceHolder mHolder;
        private final Paint mPaint;
        private final SurfaceControl.Transaction mTx;
        private boolean mBlackMode;

        BlackSurfaceView(Context context) {
            super(context);
            mHolder = getHolder();
            mHolder.addCallback(this);
            mHolder.setFormat(PixelFormat.TRANSPARENT);
            mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
            mPaint.setColor(Color.BLACK);
            mTx = new SurfaceControl.Transaction();
            setZOrderMediaOverlay(true);
            setLayoutDirection(LAYOUT_DIRECTION_LTR);
        }

        void setBlackMode(boolean black) {
            mBlackMode = black;
            setZOrderMediaOverlay(black);
            drawMask();
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            applyTrustedOverlay();
            drawMask();
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            applyTrustedOverlay();
            drawMask();
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {}

        private void applyTrustedOverlay() {
            SurfaceControl sc = getSurfaceControl();
            if (sc != null && sc.isValid()) {
                mTx.setTrustedOverlay(sc, true).apply();
            }
        }

        private void drawMask() {
            if (mHolder == null || mHolder.getSurface() == null || !mHolder.getSurface().isValid()) {
                return;
            }
            Canvas canvas = null;
            try {
                canvas = mHolder.lockCanvas(null);
                if (canvas == null) return;
                canvas.drawColor(0, PorterDuff.Mode.CLEAR);
                if (mBlackMode) {
                    canvas.drawPaint(mPaint);
                }
            } finally {
                if (canvas != null) {
                    mHolder.unlockCanvasAndPost(canvas);
                }
            }
        }
    }
}
