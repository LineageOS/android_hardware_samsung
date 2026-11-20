/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.content.Context;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;

/**
 * Optical UDFPS overlay manager.
 *
 * <p>Mounts a full-screen dim window ({@code "Dim Layer for UDFPS"}) that drives panel HBM
 * atomically via SurfaceFlinger/SDM, paired with an optional {@code "FP LightSource"} window
 * hosting {@link LightSourceView} above the dim layer. Also locks the display refresh rate while
 * HBM is active and holds the lock until hardware HBM exits ({@code actual_mask_brightness == 0})
 * to prevent VRR flicker.
 */
public final class MaskView implements DisplayBrightnessMonitor.OnBrightnessListener {
    private static final String TAG = "SB_MaskView";

    // SurfaceFlinger (libudfps_extension.samsung) tags "Dim Layer for UDFPS" with
    // dim_layer_zorder (0x20000000u), which SDM maps to CONNECTOR_SET_FINGERPRINT_MASK=1.
    // The illumination circle uses its own title ("FP LightSource") above the dim layer so it
    // does not collide with SystemUI's "UdfpsControllerOverlay".
    private static final String DIM_WINDOW_TITLE = "Dim Layer for UDFPS";
    private static final String CIRCLE_WINDOW_TITLE = "FP LightSource";
    private static final String RR_LOCK_WINDOW_TITLE = "FP RefreshRateLock";

    private static final int DEFAULT_BRIGHTNESS = 127;
    private static final float BRIGHTNESS_CONTROL_FACTOR = 1.0f;
    private static final long RR_LOCK_RELEASE_TIMEOUT_MS = 350L;

    private final Context mContext;
    private final WindowManager mWm;
    private final DisplayManager mDisplayManager;
    private final Handler mMain = new Handler(Looper.getMainLooper());
    private final DisplayBrightnessMonitor mDisplayBrightnessMonitor;
    private final DisplaySolution mDisplaySolution;

    private DisplayStateListener mDisplayStateListener;
    private FrameLayout mRoot;
    private FrameLayout mCircleRoot;
    private View mDimView;
    private LightSourceView mLightSourceView;
    private View mRrLockView;
    private FingerprintSensorInfo mSensorInfo;
    private volatile boolean mAdded;
    private boolean mCircleAdded;
    private boolean mUseHwLightSource;
    private boolean mRrLockAdded;
    private float mLockedRefreshRate;
    private float mUdfpsBrightness;
    private volatile int mCurrentAlpha;

    private final Runnable mReleaseRrLockTimeout = this::removeRefreshRateLockWindow;

    public MaskView(Context context) {
        mContext = context;
        mWm = context.getSystemService(WindowManager.class);
        mDisplayManager = context.getSystemService(DisplayManager.class);
        mDisplaySolution = new DisplaySolution(context);
        mDisplayBrightnessMonitor = DisplayBrightnessMonitor.getInstance();
        mDisplayBrightnessMonitor.init(context);
        mDisplayBrightnessMonitor.setMaxBacklight(mDisplaySolution.getMaxBacklight());
    }

    // ---- Configuration & lifecycle ----

    public void setDisplayStateListener(DisplayStateListener listener) {
        mDisplayStateListener = listener;
    }

    public void setUdfpsBrightness(float brightness) { mUdfpsBrightness = brightness; }

    public DisplaySolution getDisplaySolution() { return mDisplaySolution; }

    public int getCurrentAlpha() { return mCurrentAlpha; }

    private void runOnMain(Runnable action) {
        if (Looper.myLooper() == Looper.getMainLooper()) {
            action.run();
        } else {
            mMain.post(action);
        }
    }

    public void setUseHwLightSource(boolean useHwLightSource) {
        runOnMain(() -> {
            mUseHwLightSource = useHwLightSource;
            if (useHwLightSource && mCircleAdded && mCircleRoot != null) {
                if (mLightSourceView != null) {
                    mLightSourceView.hide();
                }
                try {
                    mWm.removeViewImmediate(mCircleRoot);
                } catch (RuntimeException e) {
                    Log.w(TAG, "removing circle window on HW light source switch failed", e);
                }
                mCircleAdded = false;
            }
        });
    }

    public boolean isUsingHwLightSource() { return mUseHwLightSource; }

    public void setSensorInfo(FingerprintSensorInfo info) {
        runOnMain(() -> {
            mSensorInfo = info;
            if (mLightSourceView != null)
                mLightSourceView.setSensorInfo(info);
        });
    }

    public void start() { mDisplayBrightnessMonitor.registerListener(this); }

    public void stop() { mDisplayBrightnessMonitor.unregisterListener(this); }

    @Override
    public void onBrightnessChanged(int brightness) {
        updateBackgroundColor(brightness);
    }

    // ---- Overlay visibility ----

    public void showCircle() {
        runOnMain(() -> {
            if (mUseHwLightSource)
                return;
            ensureView();
            if (mLightSourceView != null) {
                mLightSourceView.show();
            }
        });
    }

    public void hideCircle() {
        runOnMain(() -> {
            if (mUseHwLightSource)
                return;
            if (mLightSourceView != null) {
                mLightSourceView.hide();
            }
        });
    }

    public void show() {
        runOnMain(() -> {
            if (mAdded)
                return;
            removeRefreshRateLockWindow();
            ensureView();
            if (mDimView != null) {
                mDimView.setVisibility(View.VISIBLE);
            }
            updateBackgroundColor(-1);
            mLockedRefreshRate = pickLockedRefreshRate();
            try {
                // Add dim layer first, then SW light-source circle above it.
                mWm.addView(mRoot, buildLayoutParams(DIM_WINDOW_TITLE, mLockedRefreshRate));
                if (!mUseHwLightSource && mCircleRoot != null) {
                    mWm.addView(mCircleRoot,
                            buildLayoutParams(CIRCLE_WINDOW_TITLE, mLockedRefreshRate));
                    mCircleAdded = true;
                }
                mAdded = true;
                Log.d(TAG,
                        "MaskView added to WindowManager (rr=" + mLockedRefreshRate
                                + "Hz, hwLightSource=" + mUseHwLightSource + ")");
            } catch (Exception e) {
                Log.e(TAG, "Failed to add mask view to WindowManager", e);
            }
        });
    }

    /**
     * Removes the dim and circle windows so HBM begins exiting, while optionally keeping a tiny
     * transparent refresh-rate hold window until {@link #onHbmStateChanged(boolean)} reports HBM
     * has finished turning off.
     */
    public void hide() { hide(false); }

    public void hide(boolean deferRefreshRateRelease) {
        runOnMain(() -> {
            if (!mAdded)
                return;
            if (mLightSourceView != null) {
                mLightSourceView.hide();
            }
            if (deferRefreshRateRelease && mLockedRefreshRate > 0.0f) {
                addRefreshRateLockWindow(mLockedRefreshRate);
            }
            if (mCircleAdded && mCircleRoot != null) {
                try {
                    mWm.removeViewImmediate(mCircleRoot);
                } catch (RuntimeException e) {
                    Log.w(TAG, "removing circle window failed", e);
                } finally {
                    mCircleAdded = false;
                }
            }
            try {
                mWm.removeViewImmediate(mRoot);
            } catch (RuntimeException e) {
                Log.w(TAG, "removing dim window failed", e);
            } finally {
                mAdded = false;
            }
            Log.d(TAG,
                    "MaskView removed from WindowManager (deferRr=" + deferRefreshRateRelease
                            + ")");
        });
    }

    /** Called when hardware HBM ({@code actual_mask_brightness}) changes. */
    public void onHbmStateChanged(boolean enabled) {
        if (!enabled) {
            runOnMain(this::removeRefreshRateLockWindow);
        }
    }

    public boolean isShowing() { return mAdded; }

    // ---- Window & refresh-rate helpers ----

    private void ensureView() {
        if (mRoot == null) {
            mRoot = (FrameLayout) LayoutInflater.from(mContext).inflate(
                    R.layout.fingerprint_maskview, null);
            mDimView = mRoot.findViewById(R.id.fingerprint_maskview);
            mDimView.setVisibility(View.VISIBLE);
            mRoot.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
            mRoot.setForceDarkAllowed(false);
        }

        if (!mUseHwLightSource && mCircleRoot == null) {
            mCircleRoot = (FrameLayout) LayoutInflater.from(mContext).inflate(
                    R.layout.fingerprint_lightsource, null);
            mLightSourceView = mCircleRoot.findViewById(R.id.fingerprint_lightsource_surfaceview);
            mCircleRoot.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
            mCircleRoot.setForceDarkAllowed(false);

            if (mSensorInfo != null && mLightSourceView != null) {
                mLightSourceView.setSensorInfo(mSensorInfo);
            }
        }
    }

    private float pickLockedRefreshRate() {
        if (mDisplayManager == null)
            return 0.0f;
        Display display = mDisplayManager.getDisplay(Display.DEFAULT_DISPLAY);
        if (display == null)
            return 0.0f;
        float currentRate = display.getRefreshRate();
        return currentRate >= 90.0f ? 120.0f : 60.0f;
    }

    private void addRefreshRateLockWindow(float refreshRate) {
        if (mRrLockAdded || refreshRate <= 0.0f)
            return;
        if (mRrLockView == null) {
            mRrLockView = new View(mContext);
        }
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(0, 0,
                WindowManager.LayoutParams.TYPE_DISPLAY_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,
                PixelFormat.TRANSLUCENT);
        lp.privateFlags = WindowManager.LayoutParams.PRIVATE_FLAG_TRUSTED_OVERLAY;
        lp.preferredRefreshRate = refreshRate;
        lp.setTitle(RR_LOCK_WINDOW_TITLE);
        try {
            mWm.addView(mRrLockView, lp);
            mRrLockAdded = true;
            mMain.removeCallbacks(mReleaseRrLockTimeout);
            mMain.postDelayed(mReleaseRrLockTimeout, RR_LOCK_RELEASE_TIMEOUT_MS);
            Log.d(TAG, "Holding refresh rate " + refreshRate + "Hz until HBM exits");
        } catch (Exception e) {
            Log.w(TAG, "Failed to add refresh-rate lock window", e);
        }
    }

    private void removeRefreshRateLockWindow() {
        mMain.removeCallbacks(mReleaseRrLockTimeout);
        if (!mRrLockAdded || mRrLockView == null)
            return;
        try {
            mWm.removeViewImmediate(mRrLockView);
            Log.d(TAG, "Released deferred refresh-rate lock");
        } catch (RuntimeException e) {
            Log.w(TAG, "removing refresh-rate lock window failed", e);
        } finally {
            mRrLockAdded = false;
        }
    }

    private WindowManager.LayoutParams buildLayoutParams(String title, float preferredRefreshRate) {
        final int flags = WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED
                | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                | WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED;

        WindowManager.LayoutParams lp = new WindowManager.LayoutParams(
                WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT,
                WindowManager.LayoutParams.TYPE_DISPLAY_OVERLAY, flags, PixelFormat.TRANSLUCENT);

        lp.flags &= ~WindowManager.LayoutParams.FLAG_LAYOUT_INSET_DECOR;
        lp.privateFlags = WindowManager.LayoutParams.PRIVATE_FLAG_TRUSTED_OVERLAY
                | WindowManager.LayoutParams.PRIVATE_FLAG_IS_ROUNDED_CORNERS_OVERLAY;
        lp.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        lp.setFitInsetsTypes(0);
        if (preferredRefreshRate > 0.0f) {
            lp.preferredRefreshRate = preferredRefreshRate;
        }
        lp.setTitle(title);
        return lp;
    }

    // ---- Dimming calculation ----

    public void updateBackgroundColor(int displayBrightness) {
        if (mDimView == null)
            return;

        int rawBrightness = displayBrightness >= 0
                ? displayBrightness
                : mDisplayBrightnessMonitor.getCurrentBrightness();
        if (rawBrightness < 0) {
            rawBrightness = DEFAULT_BRIGHTNESS;
        }

        float effectiveBrightness;
        if (mDisplayStateListener != null
                && (!mDisplayStateListener.isScreenOn()
                        || !mDisplayStateListener.isInteractive())) {
            effectiveBrightness = mDisplaySolution.getAodBacklightValue(rawBrightness);
        } else {
            effectiveBrightness = rawBrightness;
        }

        float alpha = mDisplaySolution.getAlphaMaskLevel(
                effectiveBrightness, mUdfpsBrightness, BRIGHTNESS_CONTROL_FACTOR);
        int maskAlpha = Math.max(0, Math.min(255, (int) (255.0f * alpha)));
        mCurrentAlpha = maskAlpha;
        int argb = maskAlpha << 24;

        Log.d(TAG,
                "updateBackgroundColor: argb=#" + Integer.toHexString(argb)
                        + " rawBl=" + rawBrightness + " effBl=" + effectiveBrightness
                        + " fpBl=" + mUdfpsBrightness + " alpha=" + maskAlpha);
        mDimView.setBackgroundColor(argb);
    }
}
