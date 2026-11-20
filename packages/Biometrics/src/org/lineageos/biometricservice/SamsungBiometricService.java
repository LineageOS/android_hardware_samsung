/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.app.Service;
import android.content.Intent;
import android.graphics.Point;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.fingerprint.FingerprintSensorProperties;
import android.hardware.fingerprint.FingerprintSensorPropertiesInternal;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.Log;
import android.view.Display;

/**
 * Single entry point for both optical and ultrasonic under-display sensors.
 *
 * <p>The sensor type reported by the framework decides what runs:
 * <ul>
 *   <li>Ultrasonic: {@link FodController} only (TSP FoD enable + rect).</li>
 *   <li>Optical: {@link FodController} plus {@link SamsungOpticalUdfpsService}
 *       (dim layer, light circle, black mask, finger detection).</li>
 * </ul>
 */
public class SamsungBiometricService extends Service
        implements BioStateListener.OnBiometricStateChangedListener,
                   BioStateListener.OnSensorPropertiesListener {

    private static final String TAG = "SB_Service";

    private final Handler mMain = new Handler(Looper.getMainLooper());

    private SehFingerprintClient mClient;
    private BioStateListener mBioStateListener;
    private FodController mFod;
    private SamsungOpticalUdfpsService mOptical;

    private int mBioState;
    private boolean mSensorHandled;
    private boolean mDestroyed;

    // ---- Service lifecycle ----

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "SamsungBiometricService is alive");
        DisplayBrightnessMonitor.getInstance().init(this);
        mClient = new SehFingerprintClient(mMain);
        mClient.connect();
        mBioStateListener = new BioStateListener(this, this, this);
        mBioStateListener.register();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "SamsungBiometricService is killed");
        mDestroyed = true;
        mMain.removeCallbacksAndMessages(null);
        if (mBioStateListener != null) {
            mBioStateListener.unregister();
            mBioStateListener = null;
        }
        if (mOptical != null) {
            mOptical.destroy();
            mOptical = null;
        }
        if (mFod != null) {
            mFod.stop();
            mFod = null;
        }
        if (mClient != null) {
            mClient.disconnect();
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    // ---- BioStateListener callbacks (binder threads) ----

    @Override
    public void onBiometricStateChanged(int newState) {
        mMain.post(() -> {
            if (mDestroyed) return;
            mBioState = newState;
            dispatchBioState();
        });
    }

    @Override
    public void onAuthenticationAcquired(int acquiredInfo) {
        mMain.post(() -> {
            if (mDestroyed) return;
            if (mOptical != null) mOptical.onAuthenticationAcquired(acquiredInfo);
        });
    }

    @Override
    public void onAuthenticationFailed() {
        mMain.post(() -> {
            if (mDestroyed) return;
            if (mOptical != null) mOptical.onAuthenticationFailed();
        });
    }

    @Override
    public void onAuthenticationError(int errorCode, String errString) {
        mMain.post(() -> {
            if (mDestroyed) return;
            if (mOptical != null) mOptical.onAuthenticationError(errorCode, errString);
        });
    }

    @Override
    public void onAuthenticationSucceeded() {
        mMain.post(() -> {
            if (mDestroyed) return;
            if (mFod != null) mFod.onAuthenticationSucceeded();
            if (mOptical != null) mOptical.onAuthenticationSucceeded();
        });
    }

    @Override
    public void onSensorPropertiesAvailable(FingerprintSensorPropertiesInternal props) {
        mMain.post(() -> setupForSensor(props));
    }

    // ---- Sensor initialization (main thread) ----

    private void dispatchBioState() {
        if (mFod != null) mFod.onBiometricStateChanged(mBioState);
        if (mOptical != null) mOptical.onBiometricStateChanged(mBioState);
    }

    private void setupForSensor(FingerprintSensorPropertiesInternal props) {
        if (mDestroyed || mSensorHandled) return;
        mSensorHandled = true;

        int type = props.sensorType;
        Log.i(TAG, "Sensor id=" + props.sensorId + " type=" + type);

        mFod = new FodController(this, mClient, mMain);
        mClient.setListener(mFod);

        Point screen = new Point();
        DisplayManager dm = getSystemService(DisplayManager.class);
        Display display = dm != null ? dm.getDisplay(Display.DEFAULT_DISPLAY) : null;
        if (display != null) {
            display.getRealSize(screen);
        }
        float xdpi = getResources().getDisplayMetrics().xdpi;
        mClient.updateDisplayInfo(screen.x, screen.y, xdpi);

        if (type == FingerprintSensorProperties.TYPE_UDFPS_OPTICAL) {
            FingerprintSensorInfo info = mClient.getSensorInfo();
            if (info == null || info.sensorRadius <= 0) {
                Log.w(TAG, "Failed to get sensor info from HAL, using fallback");
                info = FingerprintSensorInfo.createDefault(screen.x, screen.y, xdpi);
            }
            mFod.configure();
            mOptical = new SamsungOpticalUdfpsService(this, mFod, info);
        } else if (type == FingerprintSensorProperties.TYPE_UDFPS_ULTRASONIC) {
            mFod.configure();
        } else {
            Log.i(TAG, "Not an under-display sensor, nothing to do");
            mFod = null;
            mClient.setListener(null);
            return;
        }

        mFod.start();
        // The state listener is registered right after this callback; replay anything we have.
        dispatchBioState();
    }
}
