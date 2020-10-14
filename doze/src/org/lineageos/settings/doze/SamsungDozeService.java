/*
 * Copyright (c) 2015 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.lineageos.settings.doze;

import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.IBinder;
import android.os.PowerManager;
import android.os.SystemClock;
import android.os.UserHandle;
import android.util.Log;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

public class SamsungDozeService extends Service {
    private static final String TAG = "SamsungDozeService";
    private static final boolean DEBUG = false;

    private static final String DOZE_INTENT = "com.android.systemui.doze.pulse";

    private static final int POCKET_DELTA_NS = 1000 * 1000 * 1000;

    private Context mContext;
    private ExecutorService mExecutorService;
    private SamsungProximitySensor mSensor;
    private PowerManager mPowerManager;

    private boolean mHandwaveGestureEnabled = false;
    private boolean mPocketGestureEnabled = false;

    class SamsungProximitySensor implements SensorEventListener {
        private SensorManager mSensorManager;
        private Sensor mSensor;

        private boolean mSawNear = false;
        private long mInPocketTime = 0;

        public SamsungProximitySensor(Context context) {
            mSensorManager = context.getSystemService(SensorManager.class);
            mSensor = mSensorManager.getDefaultSensor(Sensor.TYPE_PROXIMITY);
            mExecutorService = Executors.newSingleThreadExecutor();
        }

        private Future<?> submit(Runnable runnable) {
            return mExecutorService.submit(runnable);
        }

        @Override
        public void onSensorChanged(SensorEvent event) {
            boolean isNear = event.values[0] < mSensor.getMaximumRange();
            if (mSawNear && !isNear) {
                if (shouldPulse(event.timestamp)) {
                    wakeOrLaunchDozePulse();
                }
            } else {
                mInPocketTime = event.timestamp;
            }
            mSawNear = isNear;
        }

        @Override
        public void onAccuracyChanged(Sensor sensor, int accuracy) {
            /* Empty */
        }

        private boolean shouldPulse(long timestamp) {
            long delta = timestamp - mInPocketTime;

            if (Utils.isHandwaveGestureEnabled(mContext) &&
                    Utils.isPocketGestureEnabled(mContext)) {
                return true;
            } else if (Utils.isHandwaveGestureEnabled(mContext) &&
                    !Utils.isPocketGestureEnabled(mContext)) {
                return delta < POCKET_DELTA_NS;
            } else if (!Utils.isHandwaveGestureEnabled(mContext) &&
                    Utils.isPocketGestureEnabled(mContext)) {
                return delta >= POCKET_DELTA_NS;
            }
            return false;
        }

        public void enable() {
            submit(() -> {
                mSensorManager.registerListener(this, mSensor,
                        SensorManager.SENSOR_DELAY_NORMAL);
            });
        }

        public void disable() {
            submit(() -> {
                mSensorManager.unregisterListener(this, mSensor);
            });
        }
    }

    @Override
    public void onCreate() {
        if (DEBUG) Log.d(TAG, "SamsungDozeService Started");
        mContext = this;
        mPowerManager = getSystemService(PowerManager.class);
        mSensor = new SamsungProximitySensor(mContext);
        if (!isInteractive()) {
            mSensor.enable();
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (DEBUG) Log.d(TAG, "Starting service");
        IntentFilter screenStateFilter = new IntentFilter(Intent.ACTION_SCREEN_ON);
        screenStateFilter.addAction(Intent.ACTION_SCREEN_OFF);
        mContext.registerReceiver(mScreenStateReceiver, screenStateFilter);
        return START_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    private void wakeOrLaunchDozePulse() {
        if (Utils.isWakeOnGestureEnabled(mContext)) {
            if (DEBUG) Log.d(TAG, "Wake up display");
            PowerManager powerManager = mContext.getSystemService(PowerManager.class);
            powerManager.wakeUp(SystemClock.uptimeMillis(), PowerManager.WAKE_REASON_GESTURE, TAG);
        } else {
            if (DEBUG) Log.d(TAG, "Launch doze pulse");
            mContext.sendBroadcastAsUser(
                    new Intent(DOZE_INTENT), new UserHandle(UserHandle.USER_CURRENT));
        }
    }

    private boolean isInteractive() {
        return mPowerManager.isInteractive();
    }

    private void onDisplayOn() {
        if (DEBUG) Log.d(TAG, "Display on");
        mSensor.disable();
    }

    private void onDisplayOff() {
        if (DEBUG) Log.d(TAG, "Display off");
        mSensor.enable();
    }

    private BroadcastReceiver mScreenStateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (intent.getAction().equals(Intent.ACTION_SCREEN_OFF)) {
                onDisplayOff();
            } else if (intent.getAction().equals(Intent.ACTION_SCREEN_ON)) {
                onDisplayOn();
            }
        }
    };
}
