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

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent

import androidx.preference.PreferenceManager

import org.lineageos.dap.DolbyFragment.Companion.PREF_DOLBY_ENABLE
import org.lineageos.dap.DolbyFragment.Companion.PREF_DOLBY_MODES

class BootCompletedReceiver : BroadcastReceiver() {

    override fun onReceive(context: Context, intent: Intent) {
        val sharedPrefs = PreferenceManager.getDefaultSharedPreferences(context)
        for ((key, value) in PREF_DOLBY_MODES) {
            if (sharedPrefs.getBoolean(key, false)) {
                DolbyCore.setProfile(value)
                break
            }
        }
        DolbyCore.setEnabled(sharedPrefs.getBoolean(PREF_DOLBY_ENABLE, false))
    }
}
