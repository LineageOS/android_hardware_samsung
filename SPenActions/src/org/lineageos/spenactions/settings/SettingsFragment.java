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

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.SwitchPreference;

import org.lineageos.spenactions.R;

public class SettingsFragment extends PreferenceFragmentCompat
        implements Preference.OnPreferenceChangeListener {

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.spen_settings, rootKey);

        SwitchPreference enableBluetoothPreference =
                findPreference(SettingsUtils.SPEN_BLUETOOTH_ENABLE);
        enableBluetoothPreference.setOnPreferenceChangeListener(this);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (SettingsUtils.SPEN_BLUETOOTH_ENABLE.equals(preference.getKey())) {
            BluetoothAdapter btAdapter =
                    getActivity().getSystemService(BluetoothManager.class).getAdapter();
            if (btAdapter.getState() == BluetoothAdapter.STATE_ON) {
                // Simulate bluetooth switch on/off to apply the new settings
                Intent bluetoothState = new Intent(BluetoothAdapter.ACTION_STATE_CHANGED);
                bluetoothState.setPackage(getActivity().getPackageName());
                bluetoothState.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_OFF);
                getActivity().sendBroadcastAsUser(bluetoothState, UserHandle.CURRENT);
                bluetoothState.putExtra(BluetoothAdapter.EXTRA_STATE, BluetoothAdapter.STATE_ON);
                getActivity().sendBroadcastAsUser(bluetoothState, UserHandle.CURRENT);
            }
        }

        return true;
    }
}
