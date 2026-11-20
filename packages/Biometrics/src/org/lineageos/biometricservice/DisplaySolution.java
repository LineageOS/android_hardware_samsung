/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.os.Environment;
import android.util.Log;
import android.util.Xml;
import android.view.Display;
import android.view.DisplayAddress;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Computes optical UDFPS dim-layer alpha values and backlight-to-nits mappings from the device's
 * panel luminance lookup table (LUT) and AOD brightness buckets.
 *
 * <p>The LUT is read from the display's {@code screenBrightnessMap} in its display device config,
 * falling back to {@code config_screenBrightnessBacklightValue} and {@code
 * config_screenBrightnessNitsValue}.
 */
public class DisplaySolution {
    private static final String TAG = "SB_DisplaySolution";

    public static final float MAX_ALPHA = 0.93f;
    private static final float DEFAULT_GAMMA = 2.2f;
    private static final float FALLBACK_GAMMA = 1.85f;
    private static final float DEFAULT_UDFPS_BRIGHTNESS = 319.0f;
    private static final float DEFAULT_OPTICAL_NITS = 525.0f;
    private static final float DEFAULT_MAX_BACKLIGHT = 255.0f;

    private final float[] mBacklightLut;
    private final float[] mNitsLut;
    private final float mGamma;
    private final float mMaxBacklight;
    private final int[] mAodBrightnessValues;

    public DisplaySolution(Context context) {
        String[] gammaStrings =
                context.getResources().getStringArray(R.array.config_displaySolutionGammaValue);
        int[] aodValues = context.getResources().getIntArray(R.array.config_aodBrightnessValues);

        float[][] lut = loadDisplayConfigLut(context);
        if (lut == null) {
            lut = loadResourceLut(context);
        }

        int lutLen = lut != null ? lut[0].length : 0;

        if (lutLen > 1) {
            mBacklightLut = lut[0];
            mNitsLut = lut[1];
            float lastBl = mBacklightLut[lutLen - 1];
            mMaxBacklight = lastBl > 0.0f ? lastBl : DEFAULT_MAX_BACKLIGHT;
        } else {
            mBacklightLut = new float[0];
            mNitsLut = new float[0];
            mMaxBacklight = DEFAULT_MAX_BACKLIGHT;
        }

        if (gammaStrings != null && gammaStrings.length > 0) {
            float parsed = parseFloatOr(gammaStrings[0], DEFAULT_GAMMA);
            mGamma = parsed > 0.0f ? parsed : DEFAULT_GAMMA;
        } else {
            mGamma = DEFAULT_GAMMA;
        }

        mAodBrightnessValues = (aodValues != null && aodValues.length >= 2) ? aodValues : null;

        Log.i(TAG,
                "Initialized DisplaySolution: lutLen=" + mBacklightLut.length
                        + " maxBacklight=" + mMaxBacklight + " gamma=" + mGamma + " aodBuckets="
                        + (mAodBrightnessValues != null ? mAodBrightnessValues.length : 0));
    }

    public float getMaxBacklight() { return mMaxBacklight; }

    // ---- Alpha calculation ----

    /**
     * Computes compensating dim-layer alpha in {@code [0.0f, MAX_ALPHA]} for the given current
     * backlight level and optical HBM target backlight level:
     * {@code alpha = 1 - ((currentNits * brCtrl) / targetNits) ^ (1 / gamma)}.
     * Falls back to a piecewise luminance approximation if the panel LUT is unavailable.
     */
    public float getAlphaMaskLevel(
            float currentBrightness, float fingerPrintBrightness, float brCtrl) {
        float alpha = calculateAlphaBlendingValue(currentBrightness, fingerPrintBrightness, brCtrl);
        return Math.min(MAX_ALPHA, Math.max(0.0f, alpha));
    }

    public float calculateAlphaBlendingValue(
            float currentBrightness, float fingerPrintBrightness, float brCtrl) {
        float currentNits = getNitsForBacklight(currentBrightness);
        float targetNits = getNitsForBacklight(fingerPrintBrightness);

        if (currentNits > 0.0f && targetNits > 0.0f) {
            float ratio = (currentNits * brCtrl) / targetNits;
            if (ratio >= 1.0f) {
                return 0.0f;
            }
            float alpha = (float) (1.0d - Math.pow(ratio, 1.0d / mGamma));
            Log.d(TAG,
                    "LUT alpha: bl=" + currentBrightness + " (" + currentNits + " cd) "
                            + "fpBl=" + fingerPrintBrightness + " (" + targetNits + " cd) "
                            + "gamma=" + mGamma + " -> alpha=" + alpha);
            return alpha;
        }

        // Fallback piecewise luminance estimate when no panel LUT is configured.
        float b = Math.max(0.0f, currentBrightness);
        float linear = 1.644f * b;
        float corr;
        if (b <= 128.0f) {
            corr = 0.23f * b - 2.0f;
        } else if (b <= 255.0f) {
            corr = (255.0f - b) * 0.215f;
        } else {
            corr = 0.0f;
        }
        float estNit = Math.max(0.0f, linear - corr);
        float hbmNit = targetNits > 0.0f ? targetNits : DEFAULT_OPTICAL_NITS;
        if (estNit >= hbmNit) {
            return 0.0f;
        }
        float alpha = (float) (1.0d - Math.pow(estNit / hbmNit, 1.0d / FALLBACK_GAMMA));
        Log.d(TAG,
                "Fallback alpha: bl=" + b + " estNit=" + estNit + " hbmNit=" + hbmNit
                        + " -> alpha=" + alpha);
        return alpha;
    }

    // ---- Luminance & AOD lookup ----

    /**
     * Piecewise-linear lookup of panel nits for a given backlight value in the LUT.
     * Returns {@code -1.0f} if the LUT is unconfigured.
     */
    public float getNitsForBacklight(float backlight) {
        int len = mBacklightLut.length;
        if (len < 2) {
            return -1.0f;
        }

        if (backlight <= mBacklightLut[0]) {
            return mNitsLut[0];
        }
        if (backlight >= mBacklightLut[len - 1]) {
            return mNitsLut[len - 1];
        }

        // Binary search for the bracketing interval [lo, hi]
        int lo = 0;
        int hi = len - 1;
        while (lo + 1 < hi) {
            int mid = (lo + hi) >>> 1;
            if (mBacklightLut[mid] <= backlight) {
                lo = mid;
            } else {
                hi = mid;
            }
        }

        float bl0 = mBacklightLut[lo];
        float bl1 = mBacklightLut[hi];
        float n0 = mNitsLut[lo];
        float n1 = mNitsLut[hi];
        if (bl1 <= bl0) {
            return n0;
        }
        float t = (backlight - bl0) / (bl1 - bl0);
        return n0 + t * (n1 - n0);
    }

    /**
     * Finds the lowest backlight value in the LUT whose luminance is at least {@code requiredNits}.
     */
    public float getFingerPrintBacklightValue(int requiredNits) {
        for (int i = 0; i < mNitsLut.length; i++) {
            if (mNitsLut[i] >= 0.0f && requiredNits <= Math.round(mNitsLut[i])) {
                Log.d(TAG,
                        "getFingerPrintBacklightValue(" + requiredNits + " cd) -> "
                                + mBacklightLut[i]);
                return mBacklightLut[i];
            }
        }
        return -1.0f;
    }

    /**
     * Quantizes raw brightness into discrete AOD luminance buckets (2, 10, 30, 60 nits) using
     * {@code config_aodBrightnessValues} and returns the corresponding LUT backlight value.
     */
    public float getAodBacklightValue(float rawBrightness) {
        int nitBucket = 2;
        if (mAodBrightnessValues != null) {
            for (int idx = mAodBrightnessValues.length - 1; idx >= 0; idx--) {
                if (idx == 0) {
                    nitBucket = 2;
                    break;
                }
                int threshold = mAodBrightnessValues[idx];
                if (threshold > 0 && rawBrightness >= threshold) {
                    if (idx == 1) {
                        nitBucket = 10;
                    } else if (idx == 2) {
                        nitBucket = 30;
                    } else {
                        nitBucket = 60;
                    }
                    break;
                }
            }
        } else {
            nitBucket = rawBrightness > 90.0f ? 60 : 2;
        }

        float bl = getFingerPrintBacklightValue(nitBucket);
        return bl >= 0.0f ? bl : rawBrightness;
    }

    /**
     * Resolves the target optical UDFPS backlight level from the vendor HAL's target nits mapped
     * through the panel LUT, falling back to {@link #DEFAULT_OPTICAL_NITS} / {@link
     * #DEFAULT_UDFPS_BRIGHTNESS}.
     */
    public float getUdfpsBrightness() {
        int targetNits = SehFingerprintClient.getTargetNits();
        if (targetNits > 0) {
            float bl = getFingerPrintBacklightValue(targetNits);
            if (bl > 0.0f) {
                Log.i(TAG,
                        "Resolved UDFPS brightness from HAL (" + targetNits + " nits) -> backlight "
                                + bl);
                return bl;
            }
        }

        float defaultNitsBl = getFingerPrintBacklightValue(Math.round(DEFAULT_OPTICAL_NITS));
        if (defaultNitsBl > 0.0f) {
            Log.i(TAG,
                    "Resolved UDFPS brightness from default 525 nits -> backlight "
                            + defaultNitsBl);
            return defaultNitsBl;
        }

        Log.i(TAG, "Using fallback UDFPS brightness: " + DEFAULT_UDFPS_BRIGHTNESS);
        return DEFAULT_UDFPS_BRIGHTNESS;
    }

    // ---- LUT loading ----

    private static float[][] loadResourceLut(Context context) {
        String[] blStrings = context.getResources().getStringArray(
                R.array.config_screenBrightnessBacklightValue);
        String[] nitsStrings =
                context.getResources().getStringArray(R.array.config_screenBrightnessNitsValue);
        int len = (blStrings != null && nitsStrings != null)
                ? Math.min(blStrings.length, nitsStrings.length)
                : 0;
        if (len < 2) {
            return null;
        }

        float[][] lut = new float[2][len];
        for (int i = 0; i < len; i++) {
            lut[0][i] = parseFloatOr(blStrings[i], i);
            lut[1][i] = parseFloatOr(nitsStrings[i], -1.0f);
        }
        return lut;
    }

    /**
     * Reads {@code screenBrightnessMap} from the default display's device config, looked up the
     * same way as DisplayDeviceConfig, and scales its normalized values to the panel's
     * {@code max_brightness}.
     */
    private static float[][] loadDisplayConfigLut(Context context) {
        DisplayManager dm = context.getSystemService(DisplayManager.class);
        Display display = dm != null ? dm.getDisplay(Display.DEFAULT_DISPLAY) : null;
        if (display == null || !(display.getAddress() instanceof DisplayAddress.Physical)) {
            return null;
        }

        int maxBacklight =
                DisplayBrightnessMonitor.readIntFromFile(
                        DisplayBrightnessMonitor.getMaxBrightnessFilePath());
        if (maxBacklight <= 0) {
            return null;
        }

        DisplayAddress.Physical address = (DisplayAddress.Physical) display.getAddress();
        String[] suffixes = {
                String.format(Locale.ROOT, "id_%d", address.getPhysicalDisplayId()),
                String.format(Locale.ROOT, "port_%d", address.getPort()),
        };
        File[] dirs = {Environment.getProductDirectory(), Environment.getVendorDirectory()};

        for (File dir : dirs) {
            for (String suffix : suffixes) {
                File file = new File(dir, "etc/displayconfig/display_" + suffix + ".xml");
                if (!file.exists()) {
                    continue;
                }
                float[][] lut = parseScreenBrightnessMap(file, maxBacklight);
                if (lut != null) {
                    Log.i(TAG, "Loaded " + lut[0].length + " LUT points from " + file);
                    return lut;
                }
            }
        }
        return null;
    }

    private static float[][] parseScreenBrightnessMap(File file, int maxBacklight) {
        List<float[]> points = new ArrayList<>();
        try (InputStream in = new FileInputStream(file)) {
            XmlPullParser parser = Xml.newPullParser();
            parser.setInput(in, null);

            boolean inMap = false;
            float value = Float.NaN;
            float nits = Float.NaN;
            String tag = null;
            for (int type = parser.getEventType(); type != XmlPullParser.END_DOCUMENT;
                    type = parser.next()) {
                if (type == XmlPullParser.START_TAG) {
                    tag = parser.getName();
                    if ("screenBrightnessMap".equals(tag)) {
                        inMap = true;
                    } else if (inMap && "point".equals(tag)) {
                        value = Float.NaN;
                        nits = Float.NaN;
                    }
                } else if (type == XmlPullParser.TEXT && inMap && tag != null) {
                    if ("value".equals(tag)) {
                        value = parseFloatOr(parser.getText(), Float.NaN);
                    } else if ("nits".equals(tag)) {
                        nits = parseFloatOr(parser.getText(), Float.NaN);
                    }
                } else if (type == XmlPullParser.END_TAG) {
                    String name = parser.getName();
                    if ("screenBrightnessMap".equals(name)) {
                        break;
                    } else if (inMap && "point".equals(name)
                            && !Float.isNaN(value) && !Float.isNaN(nits)) {
                        points.add(new float[] {value * maxBacklight, nits});
                    }
                    tag = null;
                }
            }
        } catch (IOException | XmlPullParserException e) {
            Log.w(TAG, "Failed to parse " + file, e);
            return null;
        }

        if (points.size() < 2) {
            return null;
        }

        float[][] lut = new float[2][points.size()];
        for (int i = 0; i < points.size(); i++) {
            lut[0][i] = points.get(i)[0];
            lut[1][i] = points.get(i)[1];
        }
        return lut;
    }

    private static float parseFloatOr(String s, float fallback) {
        if (s == null)
            return fallback;
        try {
            return Float.parseFloat(s.trim());
        } catch (NumberFormatException e) {
            return fallback;
        }
    }
}
