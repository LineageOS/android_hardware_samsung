/*
 * Copyright (C) 2022 The LineageOS Project
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

package org.lineageos.dap

import android.service.quicksettings.Tile
import android.service.quicksettings.TileService

import androidx.preference.PreferenceManager

import org.lineageos.dap.DolbyFragment.Companion.PREF_DOLBY_ENABLE

class DolbyTile : TileService() {
    private var isEnabled = false
        set(value) {
            field = value
            qsTile.state = if (value) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
            qsTile.subtitle = DolbyCore.getProfileName(this)
            qsTile.updateTile()
        }

    override fun onStartListening() {
        super.onStartListening()
        isEnabled = DolbyCore.isEnabled()
    }

    override fun onClick() {
        isEnabled = !isEnabled
        DolbyCore.setEnabled(isEnabled)
        PreferenceManager.getDefaultSharedPreferences(this)
                .edit()
                .putBoolean(PREF_DOLBY_ENABLE, isEnabled)
                .commit()
    }
}
