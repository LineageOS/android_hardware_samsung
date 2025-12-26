/*
 * Copyright (C) 2025 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.util.Slog;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
public class Utils {
    private static final String TAG = "SB_Utils";

    public static byte[] readFile(File file) throws IOException {
        byte[] bArr = null;
        if (!file.exists()) {
            Slog.i(TAG, "Invalid file info, " + file);
            return null;
        }
        try {
            FileInputStream fileInputStream = new FileInputStream(file);
            bArr = new byte[(int) file.length()];
            fileInputStream.read(bArr);
            fileInputStream.close();
            return bArr;
        } catch (IOException e) {
            Slog.w(TAG, "failed to read file", e);
            return bArr;
        }
    }

    public static void writeFile(File file, byte[] bArr) throws IOException {
        if (bArr == null) {
            return;
        }
        try {
            FileOutputStream fileOutputStream = new FileOutputStream(file);
            fileOutputStream.write(bArr);
            fileOutputStream.close();
        } catch (IOException e) {
            Slog.w(TAG, "failed to write file", e);
        }
    }
}
