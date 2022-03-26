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

import android.content.Context
import android.media.audiofx.AudioEffect

import org.lineageos.dap.DolbyFragment.Companion.PREF_DOLBY_MODES

import java.util.UUID

object DolbyCore {
    private const val EFFECT_PARAM_PROFILE = 0
    private const val EFFECT_PARAM_EFF_ENAB = 19

    private val EFFECT_TYPE_DAP = UUID.fromString("46d279d9-9be7-453d-9d7c-ef937f675587")

    const val PROFILE_AUTO = 0
    const val PROFILE_MOVIE = 1
    const val PROFILE_MUSIC = 2
    const val PROFILE_VOICE = 3
    const val PROFILE_GAME = 4
    const val PROFILE_OFF = 5
    const val PROFILE_GAME_1 = 6
    const val PROFILE_GAME_2 = 7
    const val PROFILE_SPACIAL_AUDIO = 8

    private val audioEffect = runCatching {
        AudioEffect(EFFECT_TYPE_DAP, AudioEffect.EFFECT_TYPE_NULL, 0, 0)
    }.getOrNull()

    fun getProfile(): Int {
        val out = intArrayOf(PROFILE_AUTO)
        audioEffect?.getParameter(EFFECT_PARAM_PROFILE, out)
        return out.first().coerceIn(PROFILE_AUTO, PROFILE_SPACIAL_AUDIO)
    }

    fun getProfileName(context: Context): String {
        val profile = getProfile()
        val resourceName = PREF_DOLBY_MODES.filter { it.value == profile }.keys.first()

        return context.resources.getString(context.resources.getIdentifier(
                resourceName, "string", context.packageName
        ))
    }

    fun setProfile(profile: Int) {
        audioEffect?.setParameter(EFFECT_PARAM_EFF_ENAB, 1)
        audioEffect?.setParameter(EFFECT_PARAM_PROFILE, profile)
    }

    fun setEnabled(enabled: Boolean) {
        audioEffect?.enabled = enabled
    }

    fun isEnabled() = audioEffect?.enabled ?: false
}
