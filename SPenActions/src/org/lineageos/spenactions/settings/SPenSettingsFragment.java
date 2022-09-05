/*
 * SPDX-FileCopyrightText: 2021-2022 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions.settings;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserHandle;
import android.view.View;
import android.widget.CompoundButton;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragment;
import androidx.preference.SwitchPreference;

import com.android.settingslib.widget.ActionButtonsPreference;

import org.lineageos.spenactions.BluetoothUtils;
import org.lineageos.spenactions.R;

public class SPenSettingsFragment extends PreferenceFragment implements
        Preference.OnPreferenceChangeListener, CompoundButton.OnCheckedChangeListener,
        View.OnClickListener {

    private SwitchPreference mEnableBluetoothPreference;
    private ActionButtonsPreference mActionButtonsPreference;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.spen_settings);

        mEnableBluetoothPreference = findPreference(SettingsUtils.SPEN_BLUETOOTH_ENABLE);
        mEnableBluetoothPreference.setOnPreferenceChangeListener(this);
        mActionButtonsPreference = findPreference(SettingsUtils.ACTION_BUTTONS);
        mActionButtonsPreference.setButton1Text(R.string.reset_spen);
        mActionButtonsPreference.setButton1OnClickListener(this);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (SettingsUtils.SPEN_BLUETOOTH_ENABLE.equals(preference.getKey())) {
            if (((BluetoothManager) getActivity().getSystemService(Context.BLUETOOTH_SERVICE))
                    .getAdapter().getState() == BluetoothAdapter.STATE_ON) {
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

    @Override
    public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
    }

    @Override
    public void onClick(View view) {
        BluetoothUtils.resetSPenMAC(getActivity());
    }
}
