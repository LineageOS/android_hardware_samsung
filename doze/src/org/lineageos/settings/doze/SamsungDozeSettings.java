/*
 * Copyright (C) 2015 The CyanogenMod Project
 *               2017-2019 The LineageOS Project
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

package org.lineageos.settings.doze;

import android.os.Bundle;
import android.os.Handler;
import android.widget.Switch;

import androidx.preference.Preference;
import androidx.preference.Preference.OnPreferenceChangeListener;
import androidx.preference.PreferenceFragment;
import androidx.preference.SwitchPreference;

import com.android.settingslib.widget.MainSwitchPreference;
import com.android.settingslib.widget.OnMainSwitchChangeListener;

import org.lineageos.internal.util.ScreenType;

public class SamsungDozeSettings extends PreferenceFragment
        implements OnPreferenceChangeListener, OnMainSwitchChangeListener {

    private MainSwitchPreference mSwitchBar;

    private SwitchPreference mAlwaysOnDisplayPreference;
    private SwitchPreference mHandwavePreference;
    private SwitchPreference mPickUpPreference;
    private SwitchPreference mPocketPreference;
    private SwitchPreference mWakeOnGesturePreference;

    private Handler mHandler = new Handler();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.gesture_panel);

        boolean dozeEnabled = Utils.isDozeEnabled(getActivity());

        mSwitchBar = (MainSwitchPreference) findPreference(Utils.DOZE_ENABLE);
        mSwitchBar.addOnSwitchChangeListener(this);
        mSwitchBar.setChecked(dozeEnabled);

        mAlwaysOnDisplayPreference = findPreference(Utils.ALWAYS_ON_DISPLAY);
        mAlwaysOnDisplayPreference.setEnabled(dozeEnabled);
        mAlwaysOnDisplayPreference.setChecked(Utils.isAlwaysOnEnabled(getActivity()));
        mAlwaysOnDisplayPreference.setOnPreferenceChangeListener(this);

        mHandwavePreference = findPreference(Utils.GESTURE_HAND_WAVE_KEY);
        mHandwavePreference.setEnabled(dozeEnabled);
        mHandwavePreference.setOnPreferenceChangeListener(this);

        mPickUpPreference = findPreference(Utils.GESTURE_PICK_UP_KEY);
        mPickUpPreference.setEnabled(dozeEnabled);
        mPickUpPreference.setOnPreferenceChangeListener(this);

        mPocketPreference = findPreference(Utils.GESTURE_POCKET_KEY);
        mPocketPreference.setEnabled(dozeEnabled);
        mPocketPreference.setOnPreferenceChangeListener(this);

        mWakeOnGesturePreference = (SwitchPreference) findPreference(Utils.WAKE_ON_GESTURE_KEY);
        mWakeOnGesturePreference.setEnabled(dozeEnabled);
        mWakeOnGesturePreference.setOnPreferenceChangeListener(this);

        // Hide AOD if not supported and set all its dependents otherwise
        if (!Utils.alwaysOnDisplayAvailable(getActivity())) {
            getPreferenceScreen().removePreference(mAlwaysOnDisplayPreference);
        } else {
            mHandwavePreference.setDependency(Utils.ALWAYS_ON_DISPLAY);
            mPickUpPreference.setDependency(Utils.ALWAYS_ON_DISPLAY);
            mPocketPreference.setDependency(Utils.ALWAYS_ON_DISPLAY);
            mWakeOnGesturePreference.setDependency(Utils.ALWAYS_ON_DISPLAY);
        }
    }

    @Override
    public void onResume() {
        super.onResume();

        // If running on a phone, remove padding around the listview
        if (!ScreenType.isTablet(getContext())) {
            getListView().setPadding(0, 0, 0, 0);
        }
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (Utils.ALWAYS_ON_DISPLAY.equals(preference.getKey())) {
            Utils.enableAlwaysOn(getActivity(), (Boolean) newValue);
        }

        mHandler.post(() -> Utils.checkDozeService(getActivity()));

        return true;
    }

    @Override
    public void onSwitchChanged(Switch switchView, boolean isChecked) {
        Utils.enableDoze(getActivity(), isChecked);
        Utils.checkDozeService(getActivity());

        mSwitchBar.setChecked(isChecked);

        if (!isChecked) {
            Utils.enableAlwaysOn(getActivity(), false);
            mAlwaysOnDisplayPreference.setChecked(false);
        }
        mAlwaysOnDisplayPreference.setEnabled(isChecked);

        mHandwavePreference.setEnabled(isChecked);
        mPickUpPreference.setEnabled(isChecked);
        mPocketPreference.setEnabled(isChecked);
        mWakeOnGesturePreference.setEnabled(isChecked);
    }
}
