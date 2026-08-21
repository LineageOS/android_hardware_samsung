/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.soundbooster.stage;

import android.app.Service;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.AudioPlaybackConfiguration;
import android.media.audiofx.AudioEffect;
import android.os.Build;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemClock;
import android.telephony.PhoneStateListener;
import android.telephony.TelephonyManager;
import android.util.Log;

import java.util.List;
import java.util.UUID;
import java.util.concurrent.Executor;

public class SoundBoosterStageService extends Service {
    private static final String TAG = "SoundBoosterStage";

    private static final UUID EFFECT_TYPE_SOUNDBOOSTER =
            UUID.fromString("ee8aeac0-5d4b-11e5-a837-0800200c9a66");
    private static final UUID EFFECT_IMPL_SOUNDBOOSTER =
            UUID.fromString("50de45f0-5d4c-11e5-a837-0800200c9a66");

    // Priority 100, session 0 (output mix)
    private static final int SB_PRIORITY = 100;
    private static final int SB_SESSION = 0;

    private AudioManager mAudioManager;
    private TelephonyManager mTelephonyManager;
    private AudioEffect mSbEffect;
    private Handler mHandler;
    private PhoneStateListener mPhoneStateListener;
    private AudioManager.AudioPlaybackCallback mPlaybackCallback;

    private boolean mLastShouldEnable = false;
    private long mLastUpdateMs = 0;
    private String mBypassReason = "";
    private final Runnable mUpdateRunnable = this::updateStageDebounced;

    @Override
    public void onCreate() {
        super.onCreate();
        Log.i(TAG, "onCreate");

        mHandler = new Handler(Looper.getMainLooper());
        mAudioManager = getSystemService(AudioManager.class);
        mTelephonyManager = getSystemService(TelephonyManager.class);

        registerListeners();
        mHandler.postDelayed(mUpdateRunnable, 2000);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        mHandler.post(mUpdateRunnable);
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        unregisterListeners();
        setSbEnabled(false);
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void registerListeners() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            try {
                Executor exec = getMainExecutor();
                mAudioManager.addOnModeChangedListener(
                        exec, mode -> mHandler.post(mUpdateRunnable));
            } catch (Exception e) {
                Log.w(TAG, "addOnModeChangedListener failed", e);
            }
        }

        try {
            mPhoneStateListener =
                    new PhoneStateListener() {
                        @Override
                        public void onCallStateChanged(int state, String phoneNumber) {
                            mHandler.post(mUpdateRunnable);
                        }
                    };
            mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_CALL_STATE);
        } catch (Exception e) {
            Log.w(TAG, "listen failed", e);
        }

        mPlaybackCallback =
                new AudioManager.AudioPlaybackCallback() {
                    @Override
                    public void onPlaybackConfigChanged(List<AudioPlaybackConfiguration> configs) {
                        mHandler.post(mUpdateRunnable);
                    }
                };
        try {
            mAudioManager.registerAudioPlaybackCallback(mPlaybackCallback, mHandler);
        } catch (Exception e) {
            Log.w(TAG, "registerAudioPlaybackCallback failed", e);
        }
    }

    private void unregisterListeners() {
        try {
            if (mPhoneStateListener != null) {
                mTelephonyManager.listen(mPhoneStateListener, PhoneStateListener.LISTEN_NONE);
            }
        } catch (Exception ignored) {
        }
        try {
            if (mPlaybackCallback != null) {
                mAudioManager.unregisterAudioPlaybackCallback(mPlaybackCallback);
            }
        } catch (Exception ignored) {
        }
    }

    private void updateStageDebounced() {
        long now = SystemClock.elapsedRealtime();
        if (now - mLastUpdateMs < 300) {
            mHandler.removeCallbacks(mUpdateRunnable);
            mHandler.postDelayed(this::updateStageDebounced, 300 - (now - mLastUpdateMs));
            return;
        }
        mLastUpdateMs = now;
        updateStage();
    }

    private void updateStage() {
        boolean shouldEnable = shouldEnableSb();
        if (shouldEnable != mLastShouldEnable) {
            String reason = shouldEnable ? "Speaker" : mBypassReason;
            Log.i(TAG, "shouldEnable=" + shouldEnable + " (" + reason + ")");
            mLastShouldEnable = shouldEnable;
        }

        setSbEnabled(shouldEnable);
    }

    private boolean shouldEnableSb() {
        int mode = mAudioManager.getMode();
        if (mode == AudioManager.MODE_IN_COMMUNICATION || mode == AudioManager.MODE_IN_CALL) {
            mBypassReason = "VoIP";
            return false;
        }

        try {
            if (mTelephonyManager.getCallState() == TelephonyManager.CALL_STATE_OFFHOOK) {
                mBypassReason = "call in progress";
                return false;
            }
        } catch (SecurityException ignored) {
        }

        try {
            List<AudioPlaybackConfiguration> configs =
                    mAudioManager.getActivePlaybackConfigurations();
            if (configs != null && !configs.isEmpty()) {
                for (AudioPlaybackConfiguration cfg : configs) {
                    try {
                        try {
                            java.lang.reflect.Method ps =
                                    AudioPlaybackConfiguration.class.getMethod("getPlayerState");
                            if ((int) ps.invoke(cfg) != 2) continue;
                        } catch (Exception ignored) {
                        }

                        try {
                            AudioAttributes attr = cfg.getAudioAttributes();
                            if (attr != null
                                    && attr.getUsage() != AudioAttributes.USAGE_MEDIA
                                    && attr.getUsage() != AudioAttributes.USAGE_GAME) {
                                continue;
                            }
                        } catch (Exception ignored) {
                        }

                        AudioDeviceInfo outDevice = cfg.getAudioDeviceInfo();
                        if (outDevice != null
                                && outDevice.getType() != AudioDeviceInfo.TYPE_BUILTIN_SPEAKER) {
                            mBypassReason = "active device " + deviceTypeName(outDevice.getType());
                            return false;
                        }
                    } catch (Exception ignored) {
                    }
                }
            } else {
                try {
                    if (mAudioManager.isWiredHeadsetOn()) {
                        mBypassReason = "Wired Headset";
                        return false;
                    }
                    if (mAudioManager.isBluetoothScoOn() || mAudioManager.isBluetoothA2dpOn()) {
                        mBypassReason = "Bluetooth";
                        return false;
                    }
                } catch (Exception ignored) {
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "getActivePlaybackConfigurations failed", e);
        }

        return true;
    }

    private static String deviceTypeName(int type) {
        switch (type) {
            case AudioDeviceInfo.TYPE_BUILTIN_EARPIECE:
                return "Earpiece";
            case AudioDeviceInfo.TYPE_WIRED_HEADSET:
                return "Wired Headset";
            case AudioDeviceInfo.TYPE_WIRED_HEADPHONES:
                return "Wired Headphones";
            case AudioDeviceInfo.TYPE_BLUETOOTH_SCO:
                return "Bluetooth SCO";
            case AudioDeviceInfo.TYPE_BLUETOOTH_A2DP:
                return "A2DP";
            case AudioDeviceInfo.TYPE_USB_DEVICE:
                return "USB Device";
            case AudioDeviceInfo.TYPE_USB_HEADSET:
                return "USB Headset";
            case AudioDeviceInfo.TYPE_HDMI:
                return "HDMI";
            case AudioDeviceInfo.TYPE_BLE_HEADSET:
                return "BLE Headset";
            case AudioDeviceInfo.TYPE_BLE_SPEAKER:
                return "BLE Speaker";
            default:
                return "Unknown (" + type + ")";
        }
    }

    private synchronized void setSbEnabled(boolean enable) {
        if (enable) {
            if (mSbEffect != null) {
                try {
                    if (!mSbEffect.getEnabled()) {
                        mSbEffect.setEnabled(true);
                    }
                } catch (Exception e) {
                    releaseSb();
                    createSb();
                }
                return;
            }
            createSb();
        } else {
            releaseSb();
        }
    }

    private void createSb() {
        if (mSbEffect != null) return;
        try {
            if (!isEffectTypeAvailable(EFFECT_TYPE_SOUNDBOOSTER)) {
                Log.w(TAG, "effect type not available");
                return;
            }
            // Hidden AudioEffect(UUID, UUID, int, int)
            java.lang.reflect.Constructor<AudioEffect> ctor =
                    AudioEffect.class.getDeclaredConstructor(
                            UUID.class, UUID.class, int.class, int.class);
            ctor.setAccessible(true);
            mSbEffect =
                    ctor.newInstance(
                            EFFECT_TYPE_SOUNDBOOSTER,
                            EFFECT_IMPL_SOUNDBOOSTER,
                            SB_PRIORITY,
                            SB_SESSION);
            mSbEffect.setEnabled(true);
            Log.i(TAG, "effect created");
        } catch (Exception e) {
            Log.e(TAG, "create failed", e);
            releaseSb();
        }
    }

    private static boolean isEffectTypeAvailable(UUID type) {
        try {
            return (boolean)
                    AudioEffect.class
                            .getMethod("isEffectTypeAvailable", UUID.class)
                            .invoke(null, type);
        } catch (Exception ignored) {
            try {
                for (AudioEffect.Descriptor d : AudioEffect.queryEffects()) {
                    if (d.type.equals(type)) return true;
                }
            } catch (Exception ignored2) {
            }
        }
        return false;
    }

    private void releaseSb() {
        if (mSbEffect == null) return;
        Log.i(TAG, "effect released");
        try {
            mSbEffect.setEnabled(false);
        } catch (Exception ignored) {
        }
        try {
            mSbEffect.release();
        } catch (Exception ignored) {
        }
        mSbEffect = null;
    }
}
