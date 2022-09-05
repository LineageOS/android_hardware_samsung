/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions.settings;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserHandle;
import android.view.View;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.SwitchPreferenceCompat;

import com.android.settingslib.widget.ActionButtonsPreference;

import org.lineageos.spenactions.BluetoothUtils;
import org.lineageos.spenactions.R;

public class SettingsFragment extends PreferenceFragmentCompat
        implements Preference.OnPreferenceChangeListener, View.OnClickListener {

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.spen_settings, rootKey);

        ActionButtonsPreference actionButtonsPreference =
                findPreference(SettingsUtils.ACTION_BUTTONS);
        actionButtonsPreference.setButton1Text(R.string.spen_reset);
        actionButtonsPreference.setButton1OnClickListener(this);

        SwitchPreferenceCompat enableBluetoothPreference =
                findPreference(SettingsUtils.SPEN_BLUETOOTH_ENABLE);
        enableBluetoothPreference.setOnPreferenceChangeListener(this);
    }

    @Override
    public void onClick(View view) {
        BluetoothUtils.resetSPenMAC(getActivity());
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (SettingsUtils.SPEN_BLUETOOTH_ENABLE.equals(preference.getKey())) {
            BluetoothAdapter btAdapter =
                    getActivity().getSystemService(BluetoothManager.class).getAdapter();
            if (btAdapter.getState() == BluetoothAdapter.STATE_ON) {
                // Simulate bluetooth switch off/on to apply the new settings
                BluetoothUtils.simulateReconnection(getActivity());
            }
        }

        return true;
    }
}
