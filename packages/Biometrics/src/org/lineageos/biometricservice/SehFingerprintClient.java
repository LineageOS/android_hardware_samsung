/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.graphics.Rect;
import android.os.Handler;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import vendor.samsung.hardware.biometrics.fingerprint.ISehFingerprint;
import vendor.samsung.hardware.biometrics.fingerprint.ISehFingerprintCallback;

/**
 * Reconnecting client wrapper around the Samsung Biometrics Fingerprint vendor AIDL service
 * ({@code vendor.samsung.hardware.biometrics.fingerprint.ISehFingerprint/default}).
 */
public class SehFingerprintClient {
    private static final String TAG = "SB_SehFpClient";

    private static final String SERVICE_NAME =
            "vendor.samsung.hardware.biometrics.fingerprint.ISehFingerprint/default";

    private static final long RETRY_MS = 5000;
    private static final long DEATH_RETRY_MS = 2000;

    public interface Listener {
        void onConnected();
        void onFingerDown(int x, int y);
        void onFingerUp();
        void onOpticalCaptureStarted();
        void onOpticalCaptureFinished();
        default void onHbmChanged(boolean enabled) {}
    }

    private final Handler mHandler;
    private final Runnable mConnectRunnable = this::tryConnect;

    private Listener mListener;
    private volatile ISehFingerprint mHal;
    private IBinder mBinder;
    private boolean mWanted;

    private final IBinder.DeathRecipient mDeathRecipient;

    private final ISehFingerprintCallback mCallback = new ISehFingerprintCallback.Stub() {
        @Override
        public void onFingerDown(int x, int y) {
            mHandler.post(() -> {
                if (mListener != null) {
                    mListener.onFingerDown(x, y);
                }
            });
        }

        @Override
        public void onFingerUp() {
            mHandler.post(() -> {
                if (mListener != null) {
                    mListener.onFingerUp();
                }
            });
        }

        @Override
        public void onOpticalCaptureStarted() {
            mHandler.post(() -> {
                if (mListener != null) {
                    mListener.onOpticalCaptureStarted();
                }
            });
        }

        @Override
        public void onOpticalCaptureFinished() {
            mHandler.post(() -> {
                if (mListener != null) {
                    mListener.onOpticalCaptureFinished();
                }
            });
        }

        @Override
        public void onHbmChanged(boolean enabled) {
            mHandler.post(() -> {
                if (mListener != null) {
                    mListener.onHbmChanged(enabled);
                }
            });
        }

        @Override
        public int getInterfaceVersion() {
            return ISehFingerprintCallback.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return ISehFingerprintCallback.HASH;
        }
    };

    public SehFingerprintClient(Handler handler) {
        mHandler = handler;
        mDeathRecipient = () -> mHandler.post(this::onHalDied);
    }

    // ---- Connection lifecycle ----

    public void setListener(Listener listener) { mListener = listener; }

    public void connect() {
        mWanted = true;
        mHandler.removeCallbacks(mConnectRunnable);
        tryConnect();
    }

    public void disconnect() {
        mWanted = false;
        mHandler.removeCallbacks(mConnectRunnable);
        teardownLocked();
    }

    public boolean isConnected() { return mHal != null; }

    // ---- FoD & display configuration ----

    public boolean setFodEnable(int mode) {
        ISehFingerprint hal = mHal;
        if (hal == null)
            return false;
        try {
            hal.setFodEnable(mode);
            return true;
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed setFodEnable(" + mode + ")", e);
            return false;
        }
    }

    public boolean setFodRect(Rect rect) {
        if (rect == null)
            return false;
        ISehFingerprint hal = mHal;
        if (hal == null)
            return false;
        try {
            hal.setFodRect(rect.left, rect.top, rect.right, rect.bottom);
            return true;
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed setFodRect(" + rect + ")", e);
            return false;
        }
    }

    public boolean updateDisplayInfo(int displayWidth, int displayHeight, float xdpi) {
        ISehFingerprint hal = mHal;
        if (hal == null)
            return false;
        try {
            hal.updateDisplayInfo(displayWidth, displayHeight, xdpi);
            return true;
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed updateDisplayInfo(" + displayWidth + "x" + displayHeight + ")", e);
            return false;
        }
    }

    public FingerprintSensorInfo getSensorInfo() {
        ISehFingerprint hal = mHal;
        if (hal == null)
            return null;
        try {
            int[] info = hal.getSensorInfo();
            if (info != null && info.length >= 4 && info[2] > 0) {
                String lightColor = null;
                try {
                    lightColor = hal.getSensorLightColor();
                } catch (RemoteException | RuntimeException e) {
                    Log.w(TAG, "Failed to get sensor light color", e);
                }
                return new FingerprintSensorInfo(info[0], info[1], info[2], info[3], lightColor);
            }
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed getSensorInfo", e);
        }
        return null;
    }

    public Rect getFodRect() {
        ISehFingerprint hal = mHal;
        if (hal == null)
            return null;
        try {
            int[] r = hal.getFodRect();
            if (r != null && r.length == 4) {
                return new Rect(r[0], r[1], r[2], r[3]);
            }
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed getFodRect", e);
        }
        return null;
    }

    public boolean setFodIconVisible(boolean visible) {
        ISehFingerprint hal = mHal;
        if (hal == null)
            return false;
        try {
            hal.setFodIconVisible(visible);
            return true;
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed setFodIconVisible(" + visible + ")", e);
            return false;
        }
    }

    // ---- Optical HBM & light source ----

    public boolean setMaskBrightness(int brightness) {
        ISehFingerprint hal = mHal;
        if (hal == null) {
            hal = getService();
        }
        if (hal == null)
            return false;
        try {
            hal.setMaskBrightness(brightness);
            return true;
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed setMaskBrightness(" + brightness + ")", e);
            return false;
        }
    }

    public boolean isHbmEnabled() {
        ISehFingerprint hal = mHal;
        if (hal == null) {
            hal = getService();
        }
        if (hal == null)
            return false;
        try {
            return hal.isHbmEnabled();
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed isHbmEnabled", e);
            return false;
        }
    }

    public boolean notifyTouchEvent(boolean down, int currentAlpha, boolean screenOn) {
        ISehFingerprint hal = mHal;
        if (hal == null) {
            hal = getService();
        }
        if (hal == null)
            return false;
        try {
            hal.notifyTouchEvent(down, currentAlpha, screenOn);
            return true;
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed notifyTouchEvent(" + down + ")", e);
            return false;
        }
    }

    public boolean isHwLightSourceSupported() {
        ISehFingerprint hal = mHal;
        if (hal == null) {
            hal = getService();
        }
        if (hal == null)
            return false;
        try {
            return hal.isHwLightSourceSupported();
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed isHwLightSourceSupported", e);
            return false;
        }
    }

    public boolean setHwLightSource(boolean enable) {
        ISehFingerprint hal = mHal;
        if (hal == null) {
            hal = getService();
        }
        if (hal == null)
            return false;
        try {
            hal.setHwLightSource(enable);
            return true;
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed setHwLightSource(" + enable + ")", e);
            return false;
        }
    }

    // ---- Static one-shot queries ----

    public static ISehFingerprint getService() {
        try {
            IBinder binder = ServiceManager.getService(SERVICE_NAME);
            if (binder != null) {
                return ISehFingerprint.Stub.asInterface(binder);
            }
        } catch (Exception e) {
            Log.w(TAG, "Failed to get " + SERVICE_NAME, e);
        }
        return null;
    }

    public static int getTargetNits() {
        ISehFingerprint hal = getService();
        if (hal == null)
            return -1;
        try {
            return hal.getSensorTargetNits();
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed getTargetNits static", e);
            return -1;
        }
    }

    public int getSensorTargetNits() {
        ISehFingerprint hal = mHal;
        if (hal == null) {
            hal = getService();
        }
        if (hal == null)
            return -1;
        try {
            return hal.getSensorTargetNits();
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed getSensorTargetNits", e);
            return -1;
        }
    }

    public static String getLightColor() {
        ISehFingerprint hal = getService();
        if (hal == null)
            return null;
        try {
            return hal.getSensorLightColor();
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed getLightColor static", e);
            return null;
        }
    }

    public String getSensorLightColor() {
        ISehFingerprint hal = mHal;
        if (hal == null) {
            hal = getService();
        }
        if (hal == null)
            return null;
        try {
            return hal.getSensorLightColor();
        } catch (RemoteException | RuntimeException e) {
            Log.e(TAG, "Failed getSensorLightColor", e);
            return null;
        }
    }

    // ---- Binder management ----

    private void tryConnect() {
        if (!mWanted || mHal != null)
            return;

        IBinder binder = ServiceManager.checkService(SERVICE_NAME);
        if (binder == null) {
            Log.i(TAG, "Waiting for " + SERVICE_NAME);
            mHandler.postDelayed(mConnectRunnable, RETRY_MS);
            return;
        }

        try {
            binder.linkToDeath(mDeathRecipient, 0);
        } catch (RemoteException e) {
            Log.w(TAG, "Service died before linkToDeath, retrying soon", e);
            mHandler.postDelayed(mConnectRunnable, DEATH_RETRY_MS);
            return;
        }

        mBinder = binder;
        mHal = ISehFingerprint.Stub.asInterface(binder);
        try {
            mHal.registerCallback(mCallback);
        } catch (RemoteException | RuntimeException e) {
            Log.w(TAG, "Failed to register callback with ISehFingerprint", e);
        }

        Log.i(TAG, "Connected to " + SERVICE_NAME);

        if (mListener != null) {
            mListener.onConnected();
        }
    }

    private void onHalDied() {
        Log.w(TAG, SERVICE_NAME + " died");
        teardownLocked();
        if (mWanted) {
            mHandler.postDelayed(mConnectRunnable, DEATH_RETRY_MS);
        }
    }

    private void teardownLocked() {
        if (mBinder != null) {
            try {
                mBinder.unlinkToDeath(mDeathRecipient, 0);
            } catch (Exception ignored) {
            }
            mBinder = null;
        }
        mHal = null;
    }
}
