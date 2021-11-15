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

class DolbyTile : TileService() {
    private var isEnabled = false
    private var dolbyCore = DolbyCore()

    override fun onStartListening() {
        super.onStartListening()
        isEnabled = dolbyCore.isEnabled()
        updateTile()
    }

    override fun onClick() {
        isEnabled = !isEnabled
        dolbyCore.setEnabled(isEnabled)
        updateTile()
    }

    private fun updateTile() {
        val tile = qsTile
        tile.state = if (isEnabled) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
        tile.updateTile()
    }
}
