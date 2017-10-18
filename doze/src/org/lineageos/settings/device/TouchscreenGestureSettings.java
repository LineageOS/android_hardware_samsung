/*
 * Copyright (C) 2015 The CyanogenMod Project
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
import android.os.Bundle;
import android.support.v14.preference.PreferenceFragment;
import android.support.v14.preference.SwitchPreference;
import android.support.v7.preference.Preference;
import android.provider.Settings;
import android.view.MenuItem;

import org.lineageos.internal.util.ScreenType;

public class TouchscreenGestureSettings extends PreferenceFragment {

    private static final String KEY_HAND_WAVE = "gesture_hand_wave";
    private static final String KEY_PROXIMITY_WAKE = "proximity_wake_enable";

    private SwitchPreference mHandwavePreference;
    private SwitchPreference mProximityWakePreference;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.gesture_panel);
        final ActionBar actionBar = getActivity().getActionBar();
        actionBar.setDisplayHomeAsUpEnabled(true);

        mHandwavePreference =
            (SwitchPreference) findPreference(KEY_HAND_WAVE);
        mHandwavePreference.setOnPreferenceChangeListener(mProximityListener);
        mProximityWakePreference =
            (SwitchPreference) findPreference(KEY_PROXIMITY_WAKE);
        mProximityWakePreference.setOnPreferenceChangeListener(mProximityListener);
    }

    @Override
    public void onResume() {
        super.onResume();

        // If running on a phone, remove padding around the listview
        if (!ScreenType.isTablet(getContext())) {
            getListView().setPadding(0, 0, 0, 0);
        }
    }

    private Preference.OnPreferenceChangeListener mProximityListener =
        new Preference.OnPreferenceChangeListener() {
        @Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            if ((boolean) newValue) {
                if (preference.getKey().equals(KEY_HAND_WAVE)) {
                    mProximityWakePreference.setChecked(false);
                } else if (preference.getKey().equals(KEY_PROXIMITY_WAKE)) {
                    mHandwavePreference.setChecked(false);
                }
            }
            return true;
        }
    };

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            getActivity().onBackPressed();
            return true;
        }
        return false;
    }
}
