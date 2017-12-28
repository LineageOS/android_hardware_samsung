/*
 * Copyright (C) 2017 The LineageOS Project
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

package org.lineageos.settings.device;

import android.app.ActionBar;
import android.content.res.Resources;
import android.os.Bundle;
import android.support.v14.preference.PreferenceFragment;
import android.view.MenuItem;

import org.lineageos.settings.device.R;

import org.lineageos.internal.util.FileUtils;

public class AdvancedDisplayFragment extends PreferenceFragment {
    private mDNIeScenario mmDNIeScenario;
    private mDNIeNegative mmDNIeNegative;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.screen_preferences);
        Resources res = getResources();

        final ActionBar actionBar = getActivity().getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);

        /* mDNIe */
        mmDNIeScenario = (mDNIeScenario) findPreference(Constants.KEY_MDNIE_SCENARIO);
        mmDNIeScenario.setEnabled(
                FileUtils.isFileWritable(res.getString(R.string.mdnie_scenario_sysfs_file)));

        mmDNIeNegative = (mDNIeNegative) findPreference(Constants.KEY_MDNIE_NEGATIVE);
        mmDNIeNegative.setEnabled(
                FileUtils.isFileWritable(res.getString(R.string.mdnie_negative_sysfs_file)));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            getActivity().onBackPressed();
            return true;
        }
        return false;
    }
}
