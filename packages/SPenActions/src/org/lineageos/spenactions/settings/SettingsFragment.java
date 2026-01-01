/*
 * SPDX-FileCopyrightText: 2021-2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.spenactions.settings;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothManager;
import android.content.Intent;
import android.os.Bundle;
import android.os.UserHandle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.preference.Preference;

import com.android.settingslib.widget.MainSwitchPreference;
import com.android.settingslib.widget.SettingsBasePreferenceFragment;

import org.lineageos.spenactions.BluetoothUtils;
import org.lineageos.spenactions.R;

public class SettingsFragment extends SettingsBasePreferenceFragment
        implements Preference.OnPreferenceChangeListener {

    private static final int MENU_RESET = Menu.FIRST;

    private MainSwitchPreference mSwitchBar;

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        super.onCreateOptionsMenu(menu, inflater);
        menu.add(0, MENU_RESET, 0, R.string.spen_reset)
                .setAlphabeticShortcut('r')
                .setEnabled(mSwitchBar.isEnabled())
                .setShowAsAction(MenuItem.SHOW_AS_ACTION_NEVER);
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setHasOptionsMenu(true);
        setPreferencesFromResource(R.xml.spen_settings, rootKey);

        mSwitchBar = findPreference(SettingsUtils.SPEN_BLUETOOTH_ENABLE);
        mSwitchBar.setOnPreferenceChangeListener(this);
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == MENU_RESET) {
            BluetoothUtils.resetSPenMAC(getActivity());
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (SettingsUtils.SPEN_BLUETOOTH_ENABLE.equals(preference.getKey())) {
            BluetoothAdapter btAdapter =
                    getActivity().getSystemService(BluetoothManager.class).getAdapter();
            if (btAdapter.getState() == BluetoothAdapter.STATE_ON) {
                // Simulate Bluetooth switch off/on to apply the new settings
                BluetoothUtils.simulateReconnection(getActivity());
            }
        }

        return true;
    }
}
