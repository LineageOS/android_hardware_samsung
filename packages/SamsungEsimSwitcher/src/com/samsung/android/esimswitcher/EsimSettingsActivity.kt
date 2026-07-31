/*
 * SPDX-FileCopyrightText: 2026 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package com.samsung.android.esimswitcher

import android.os.Bundle
import com.android.settingslib.collapsingtoolbar.CollapsingToolbarBaseActivity
import com.android.settingslib.collapsingtoolbar.R

class EsimSettingsActivity : CollapsingToolbarBaseActivity() {

    companion object {
        private const val TAG = "EsimSettingsActivity"
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportFragmentManager
            .beginTransaction()
            .replace(R.id.content_frame, EsimSettingsFragment(), TAG)
            .commit()
    }
}
