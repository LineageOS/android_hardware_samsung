/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.biometricservice;

import android.util.Slog;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Bounded file read and write helpers for sysfs nodes.
 */
public class Utils {
    private static final String TAG = "SB_Utils";
    private static final int MAX_READ_BYTES = 4096;

    public static byte[] readFile(File file) throws IOException {
        if (file == null || !file.exists()) {
            Slog.i(TAG, "Invalid file info, " + file);
            return null;
        }
        try (FileInputStream fis = new FileInputStream(file);
                ByteArrayOutputStream baos = new ByteArrayOutputStream()) {
            byte[] buf = new byte[256];
            int total = 0;
            int read;
            while ((read = fis.read(buf)) > 0) {
                baos.write(buf, 0, read);
                total += read;
                if (total >= MAX_READ_BYTES)
                    break;
            }
            return baos.toByteArray();
        } catch (IOException e) {
            Slog.w(TAG, "failed to read file: " + file, e);
            return null;
        }
    }

    public static void writeFile(File file, byte[] data) {
        if (file == null || data == null) {
            return;
        }
        try (FileOutputStream fos = new FileOutputStream(file)) {
            fos.write(data);
        } catch (IOException e) {
            Slog.w(TAG, "failed to write file: " + file, e);
        }
    }
}
