/*
 * Copyright (C) 2013-2016 The CyanogenMod Project
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

package org.lineageos.hardware;

import org.lineageos.internal.util.FileUtils;

public class VibratorHW {

    private static final String DEFAULT_PATH = "/sys/class/timed_output/vibrator/pwm_default";
    private static final String LEVEL_PATH = "/sys/class/timed_output/vibrator/pwm_value";
    private static final String MAX_PATH = "/sys/class/timed_output/vibrator/pwm_max";
    private static final String MIN_PATH = "/sys/class/timed_output/vibrator/pwm_min";
    private static final String THRESHOLD_PATH = "/sys/class/timed_output/vibrator/pwm_threshold";

    public static boolean isSupported() {
        return FileUtils.isFileWritable(LEVEL_PATH) &&
                FileUtils.isFileReadable(LEVEL_PATH) &&
                FileUtils.isFileReadable(DEFAULT_PATH) &&
                FileUtils.isFileReadable(MAX_PATH) &&
                FileUtils.isFileReadable(MIN_PATH) &&
                FileUtils.isFileReadable(THRESHOLD_PATH);
    }

    public static int getMaxIntensity()  {
        try {
            return Integer.parseInt(FileUtils.readOneLine(MAX_PATH));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    public static int getMinIntensity()  {
        try {
            return Integer.parseInt(FileUtils.readOneLine(MIN_PATH));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    public static int getWarningThreshold()  {
        try {
            return Integer.parseInt(FileUtils.readOneLine(THRESHOLD_PATH));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    public static int getCurIntensity()  {
        try {
            return Integer.parseInt(FileUtils.readOneLine(LEVEL_PATH));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    public static int getDefaultIntensity()  {
        try {
            return Integer.parseInt(FileUtils.readOneLine(DEFAULT_PATH));
        } catch (NumberFormatException e) {
            return -1;
        }
    }

    public static boolean setIntensity(int intensity)  {
        return FileUtils.writeLine(LEVEL_PATH, String.valueOf(intensity));
    }
}
