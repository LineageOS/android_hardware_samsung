/*
 * Copyright (C) 2015-2016 The CyanogenMod Project
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

import android.util.Log;

import org.lineageos.internal.util.FileUtils;

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.IOException;

/**
 * Touchscreen Hovering
 */
public class TouchscreenHovering {

    private static final String TAG = "TouchscreenHovering";

    private static final String COMMAND_PATH = "/sys/class/sec/tsp/cmd";
    private static final String COMMAND_LIST_PATH = "/sys/class/sec/tsp/cmd_list";
    private static final String COMMAND_RESULT_PATH = "/sys/class/sec/tsp/cmd_result";
    private static final String HOVER_MODE = "hover_enable";
    private static final String HOVER_MODE_ENABLE = "hover_enable,1";
    private static final String HOVER_MODE_DISABLE = "hover_enable,0";
    private static final String STATUS_OK = ":OK";

    /**
     * Whether device supports touchscreen hovering.
     *
     * @return boolean Supported devices must return always true
     */
    public static boolean isSupported() {
        if (!FileUtils.isFileWritable(COMMAND_PATH) ||
                !FileUtils.isFileReadable(COMMAND_LIST_PATH) ||
                !FileUtils.isFileReadable(COMMAND_RESULT_PATH)) {
            return false;
        }

        BufferedReader reader = null;
        try {
            String currentLine;
            reader = new BufferedReader(new FileReader(COMMAND_LIST_PATH));
            while ((currentLine = reader.readLine()) != null) {
                if (HOVER_MODE.equals(currentLine)) {
                    return true;
                }
            }
        } catch (IOException e) {
            Log.e(TAG, "Could not read from file " + COMMAND_LIST_PATH, e);
        } finally {
            if (reader != null) {
                try {
                    reader.close();
                } catch (IOException e) {
                    // Ignore exception, no recovery possible
                }
            }
        }
        return false;
    }

    /** This method returns the current activation status of touchscreen hovering
     *
     * @return boolean Must be false if touchscreen hovering is not supported or not activated,
     * or the operation failed while reading the status; true in any other case.
     */
    public static boolean isEnabled() {
        return (HOVER_MODE_ENABLE + STATUS_OK).equals(
                FileUtils.readOneLine(COMMAND_RESULT_PATH));
    }

    /**
     * This method allows to setup touchscreen hovering status.
     *
     * @param status The new touchscreen hovering status
     * @return boolean Must be false if touchscreen hovering is not supported or the operation
     * failed; true in any other case.
     */
    public static boolean setEnabled(boolean status) {
        return FileUtils.writeLine(COMMAND_PATH,
                status ? HOVER_MODE_ENABLE : HOVER_MODE_DISABLE);
    }
}
