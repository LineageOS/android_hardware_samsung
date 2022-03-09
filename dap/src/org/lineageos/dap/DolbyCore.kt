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

import android.media.audiofx.AudioEffect

import java.util.UUID

object DolbyCore {
    private const val EFFECT_PARAM_PROFILE = 0
    private const val EFFECT_PARAM_EFF_ENAB = 19

    const val PROFILE_AUTO = 0
    const val PROFILE_MOVIE = 1
    const val PROFILE_MUSIC = 2
    const val PROFILE_VOICE = 3
    const val PROFILE_GAME = 4
    const val PROFILE_OFF = 5
    const val PROFILE_GAME_1 = 6
    const val PROFILE_GAME_2 = 7
    const val PROFILE_SPACIAL_AUDIO = 8

    private val audioEffect = AudioEffect(
        UUID.fromString("46d279d9-9be7-453d-9d7c-ef937f675587"), AudioEffect.EFFECT_TYPE_NULL, 0, 0
    )

    fun setProfile(profile: Int) {
        audioEffect.setParameter(EFFECT_PARAM_EFF_ENAB, 1)
        audioEffect.setParameter(EFFECT_PARAM_PROFILE, profile)
    }

    fun setEnabled(enabled: Boolean) {
        audioEffect.enabled = enabled
    }

    fun isEnabled() = audioEffect.enabled
}
