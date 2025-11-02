/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.feature;

import android.os.SystemProperties;
import android.util.Log;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlPullParserFactory;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Hashtable;

public final class SemFloatingFeature implements IFloatingFeature {

    private static final String TAG = "SemFloatingFeature";
    private static final String FEATURE_XML = "/system/etc/floating_feature.xml";

    private static final boolean DEFAULT_BOOLEAN_VALUE = false;
    private static final int DEFAULT_INT_VALUE = -1;
    private static final String DEFAULT_STRING_VALUE = "";

    private static final boolean LOG_ENABLED = false;
    private static final SemFloatingFeature sInstance;

    private final Hashtable<String, String> mFeatureList = new Hashtable<>();

    static {
        sInstance = new SemFloatingFeature();
    }

    private SemFloatingFeature() {
        try {
            loadFloatingFeature();
        } catch (Exception e) {
            loge(e);
        }
    }

    public static SemFloatingFeature getInstance() {
        return sInstance;
    }

    private static void logw(Object msg) {
        if (LOG_ENABLED) Log.w(TAG, String.valueOf(msg));
    }

    private static void loge(Object msg) {
        if (LOG_ENABLED) Log.e(TAG, String.valueOf(msg));
    }


    @Override
    public boolean getBoolean(String tag) {
        if (tag == null) {
            loge("getBoolean(): tag is null.");
            return DEFAULT_BOOLEAN_VALUE;
        }
        String val = mFeatureList.get(tag);
        return val != null && Boolean.parseBoolean(val);
    }

    @Override
    public String getString(String tag) {
        if (tag == null) {
            loge("getString(): tag is null.");
            return DEFAULT_STRING_VALUE;
        }
        String val = mFeatureList.get(tag);
        return val == null ? DEFAULT_STRING_VALUE : val;
    }

    @Override
    public int getInt(String tag) {
        if (tag == null) {
            loge("getInt(): tag is null.");
            return DEFAULT_INT_VALUE;
        }
        String val = mFeatureList.get(tag);
        if (val == null) return DEFAULT_INT_VALUE;

        try {
            return Integer.parseInt(val);
        } catch (NumberFormatException e) {
            loge("Value for " + tag + " cannot be parsed as int: " + val);
            return DEFAULT_INT_VALUE;
        }
    }

    public int getInteger(String tag) {
        return getInt(tag);
    }

    private void loadFloatingFeature() throws XmlPullParserException, IOException {
        File file = new File(FEATURE_XML);
        if (!file.exists()) {
            loge("Feature file not found: " + FEATURE_XML);
            return;
        }

        FileInputStream fis = null;
        try {
            fis = new FileInputStream(file);
            XmlPullParserFactory factory = XmlPullParserFactory.newInstance();
            factory.setNamespaceAware(true);

            XmlPullParser parser = factory.newPullParser();
            parser.setInput(fis, "UTF-8");

            int eventType = parser.getEventType();
            String name = null;
            String value = null;

            while (eventType != XmlPullParser.END_DOCUMENT) {
                switch (eventType) {
                    case XmlPullParser.START_TAG:
                        if ("feature".equals(parser.getName())) {
                            name = parser.getAttributeValue(null, "name");
                            value = parser.nextText();
                            if (name != null && value != null) {
                                mFeatureList.put(name.trim(), value.trim());
                            }
                        }
                        break;
                }
                eventType = parser.next();
            }

            logw("Loaded " + mFeatureList.size() + " floating features.");
        } catch (Exception e) {
            loge("Error parsing " + FEATURE_XML + ": " + e);
        } finally {
            if (fis != null) {
                try {
                    fis.close();
                } catch (IOException ignored) {
                }
            }
        }
    }
}
