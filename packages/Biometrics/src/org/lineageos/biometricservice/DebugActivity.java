/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.app.Activity;
import android.graphics.Insets;
import android.graphics.Point;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.Display;
import android.view.View;
import android.view.WindowInsets;
import android.widget.Switch;
import android.widget.TextView;

/**
 * Diagnostic activity for manually testing optical UDFPS overlays and inspecting live dimming
 * and HBM telemetry. Hidden from launchers; launch via {@code am start} with {@code DUMP} access.
 */
public class DebugActivity extends Activity
        implements DisplayBrightnessMonitor.OnBrightnessListener {

    private static final String TAG = "SB_DebugActivity";
    private static final float BRIGHTNESS_CONTROL_FACTOR = 1.0f;

    private final Handler mMain = new Handler(Looper.getMainLooper());
    private MaskView mMaskView;
    private BlackMaskView mBlackMaskView;
    private DisplayBrightnessMonitor mBrightnessMonitor;
    private DisplaySolution mDisplaySolution;
    private SehFingerprintClient mClient;

    private TextView mStatBrightness;
    private TextView mStatNits;
    private TextView mStatAlpha;
    private TextView mStatArgb;
    private TextView mStatHbm;
    private TextView mStatLightSourceMode;
    private TextView mStatUdfpsBrightness;
    private TextView mStatSysfsPath;

    private float mUdfpsBrightness = 319.0f;
    private boolean mUseHwLightSource;

    // ---- Activity lifecycle ----

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_debug);

        mMaskView = new MaskView(this);
        mBlackMaskView = new BlackMaskView(this);
        mDisplaySolution = mMaskView.getDisplaySolution();
        mBrightnessMonitor = DisplayBrightnessMonitor.getInstance();

        mClient = new SehFingerprintClient(mMain);
        mClient.connect();

        mUseHwLightSource = mClient.isHwLightSourceSupported();
        mMaskView.setUseHwLightSource(mUseHwLightSource);

        mUdfpsBrightness = mDisplaySolution.getUdfpsBrightness();
        mMaskView.setUdfpsBrightness(mUdfpsBrightness);
        mMaskView.setSensorInfo(resolveSensorInfo());

        mStatBrightness = findViewById(R.id.stat_brightness);
        mStatNits = findViewById(R.id.stat_nits);
        mStatAlpha = findViewById(R.id.stat_alpha);
        mStatArgb = findViewById(R.id.stat_argb);
        mStatHbm = findViewById(R.id.stat_hbm);
        mStatLightSourceMode = findViewById(R.id.stat_light_source_mode);
        mStatUdfpsBrightness = findViewById(R.id.stat_udfps_brightness);
        mStatSysfsPath = findViewById(R.id.stat_sysfs_path);

        mStatLightSourceMode.setText(mUseHwLightSource
                ? "HW Self-Mask (fp_green_circle)"
                : "SW SurfaceView (FP LightSource)");
        float targetNits = mDisplaySolution.getNitsForBacklight(mUdfpsBrightness);
        mStatUdfpsBrightness.setText(String.format("%.1f (%.1f nits)", mUdfpsBrightness, targetNits));
        mStatSysfsPath.setText(mBrightnessMonitor.getBrightnessFilePath());

        final Switch maskViewToggle = findViewById(R.id.mask_view_toggle);
        final Switch circleToggle = findViewById(R.id.circle_view_toggle);
        final Switch blackMaskToggle = findViewById(R.id.black_mask_toggle);

        maskViewToggle.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (isChecked) {
                mMaskView.show();
                mMaskView.start();
                if (circleToggle.isChecked()) {
                    setCircleEnabled(true);
                }
            } else {
                circleToggle.setChecked(false);
                setCircleEnabled(false);
                mMaskView.hide();
                mMaskView.stop();
            }
            mMain.postDelayed(this::refreshHbmStat, 100L);
        });

        circleToggle.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (isChecked) {
                if (!maskViewToggle.isChecked()) {
                    maskViewToggle.setChecked(true);
                }
                setCircleEnabled(true);
            } else {
                setCircleEnabled(false);
            }
        });

        blackMaskToggle.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (isChecked) {
                mBlackMaskView.show();
                mMain.postDelayed(() -> blackMaskToggle.setChecked(false), 400L);
            } else {
                mBlackMaskView.hide();
            }
        });

        final View root = findViewById(R.id.main);
        if (root != null) {
            root.setOnApplyWindowInsetsListener((v, insets) -> {
                Insets sys = insets.getInsets(WindowInsets.Type.systemBars());
                v.setPadding(sys.left, sys.top, sys.right, sys.bottom);
                return insets;
            });
            root.requestApplyInsets();
        }

        updateStats(mBrightnessMonitor.getCurrentBrightness());
        refreshHbmStat();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mBrightnessMonitor.registerListener(this);
        updateStats(mBrightnessMonitor.getCurrentBrightness());
        refreshHbmStat();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mBrightnessMonitor.unregisterListener(this);
    }

    @Override
    protected void onStop() {
        super.onStop();
        setCircleEnabled(false);
        if (mMaskView != null) {
            mMaskView.hide();
            mMaskView.stop();
        }
        if (mBlackMaskView != null) {
            mBlackMaskView.hide();
        }
    }

    private void setCircleEnabled(boolean enabled) {
        if (mUseHwLightSource) {
            if (mClient != null) {
                mClient.setHwLightSource(enabled);
            }
        } else if (mMaskView != null) {
            if (enabled) {
                mMaskView.showCircle();
            } else {
                mMaskView.hideCircle();
            }
        }
    }

    @Override
    protected void onDestroy() {
        if (mClient != null) {
            mClient.disconnect();
        }
        super.onDestroy();
    }

    // ---- Brightness & stats ----

    @Override
    public void onBrightnessChanged(int brightness) {
        runOnUiThread(() -> {
            updateStats(brightness);
            refreshHbmStat();
        });
    }

    private FingerprintSensorInfo resolveSensorInfo() {
        FingerprintSensorInfo info = mClient != null ? mClient.getSensorInfo() : null;
        if (info != null && info.sensorRadius > 0) {
            return info;
        }
        Point screen = new Point();
        DisplayManager dm = getSystemService(DisplayManager.class);
        Display display = dm != null ? dm.getDisplay(Display.DEFAULT_DISPLAY) : null;
        if (display != null) {
            display.getRealSize(screen);
        }
        float xdpi = getResources().getDisplayMetrics().xdpi;
        return FingerprintSensorInfo.createDefault(screen.x, screen.y, xdpi);
    }

    private void refreshHbmStat() {
        if (mStatHbm == null) return;
        boolean hbm = mClient != null && mClient.isHbmEnabled();
        mStatHbm.setText(hbm ? "ON (actual_mask_brightness != 0)" : "OFF (0)");
    }

    private void updateStats(int brightness) {
        if (brightness < 0) {
            mStatBrightness.setText("unavailable");
            mStatNits.setText("—");
            mStatAlpha.setText("—");
            mStatArgb.setText("—");
            return;
        }

        float norm = mBrightnessMonitor.getCurrentBrightnessFloat();
        mStatBrightness.setText(String.format("%d  (norm %.4f)", brightness, norm));
        float curNits = mDisplaySolution.getNitsForBacklight(brightness);
        mStatNits.setText(curNits >= 0.0f ? String.format("%.1f cd/m²", curNits) : "fallback");

        try {
            float alpha = mDisplaySolution.getAlphaMaskLevel(brightness, mUdfpsBrightness,
                    BRIGHTNESS_CONTROL_FACTOR);
            int maskAlpha = Math.max(0, Math.min(255, (int) (255.0f * alpha)));
            mStatAlpha.setText(String.format("%.3f  (%d / 255)", alpha, maskAlpha));
            mStatArgb.setText(String.format("#%08X", maskAlpha << 24));
        } catch (RuntimeException e) {
            Log.e(TAG, "Failed to compute alpha for brightness " + brightness, e);
            mStatAlpha.setText("error");
            mStatArgb.setText("error");
        }
    }
}
