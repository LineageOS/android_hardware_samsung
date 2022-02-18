/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions.settings;

import android.os.Bundle;
import com.android.settingslib.collapsingtoolbar.CollapsingToolbarBaseActivity;

public class SettingsActivity extends CollapsingToolbarBaseActivity {

    private static final String TAG_SPENACTIONS = "spenactions";

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

         getFragmentManager().beginTransaction().replace(
                 com.android.settingslib.collapsingtoolbar.R.id.content_frame,
                 new SPenSettingsFragment(), TAG_SPENACTIONS).commit();
    }

}
