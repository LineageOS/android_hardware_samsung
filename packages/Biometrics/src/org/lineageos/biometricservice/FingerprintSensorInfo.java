/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.graphics.Point;
import android.util.Log;
import android.view.Surface;

/**
 * Optical fingerprint sensor geometry in display coordinates, accounting for screen rotation.
 * Coordinates are sourced from the vendor {@code ISehFingerprint} HAL.
 */
public class FingerprintSensorInfo {
    private static final String TAG = "SB_SensorInfo";

    /** Sensor center coordinates in natural (portrait) display orientation. */
    public final int sensorX;
    public final int sensorY;
    public final int sensorRadius;
    public final int sensorImageSize;
    public final String lightColor;

    public FingerprintSensorInfo(int x, int y, int radius, int imageSize) {
        this(x, y, radius, imageSize, null);
    }

    public FingerprintSensorInfo(int x, int y, int radius, int imageSize, String lightColor) {
        sensorX = x;
        sensorY = y;
        sensorRadius = radius;
        sensorImageSize = imageSize > 0 ? imageSize : radius * 2;
        this.lightColor = (lightColor != null && !lightColor.trim().isEmpty())
                ? lightColor.trim() : "FFFFFF";
        Log.i(TAG, "sensor centre=(" + x + "," + y + ") radius=" + radius
                + " imageSize=" + sensorImageSize + " lightColor=" + this.lightColor);
    }

    /**
     * Builds fallback sensor geometry from typical physical metrics if the HAL returns empty data.
     */
    public static FingerprintSensorInfo createDefault(int displayWidth, int displayHeight, float xdpi) {
        return createDefault(displayWidth, displayHeight, xdpi, null);
    }

    public static FingerprintSensorInfo createDefault(int displayWidth, int displayHeight, float xdpi,
                                                      String lightColor) {
        double mmToPx = (xdpi > 0.0f) ? (xdpi / 25.4) : (420.0 / 25.4);
        int sensorX = displayWidth / 2;
        int sensorY = (int) (displayHeight - (14.31 * mmToPx) - ((9.10 / 2.0) * mmToPx));
        int imageSize = (int) (12.00 * mmToPx);
        int radius = imageSize / 2;
        Log.w(TAG, "Using default sensor geometry: centre=(" + sensorX + "," + sensorY + ") radius=" + radius);
        return new FingerprintSensorInfo(sensorX, sensorY, radius, imageSize, lightColor);
    }

    /**
     * Returns the top-left corner of the sensor illumination bounding box for the given display
     * size and surface rotation.
     */
    public Point getSensorImagePoint(Point displaySize, int rotation) {
        int half = sensorImageSize / 2;
        int x;
        int y;

        switch (rotation) {
            case Surface.ROTATION_90:
                x = sensorY - half;
                y = displaySize.y - sensorX - half;
                break;
            case Surface.ROTATION_180:
                x = displaySize.x - sensorX - half;
                y = displaySize.y - sensorY - half;
                break;
            case Surface.ROTATION_270:
                x = displaySize.x - sensorY - half;
                y = sensorX - half;
                break;
            case Surface.ROTATION_0:
            default:
                x = sensorX - half;
                y = sensorY - half;
                break;
        }
        return new Point(x, y);
    }
}
