/*
 * Copyright (C) 2019 The MoKee Open Source Project
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

package org.lineageos.settings.fastcharge;

import android.app.ActionBar;
import android.content.Context;
import android.os.Bundle;
import android.preference.PreferenceActivity;

public class FastChargeSettingsActivity extends PreferenceActivity {

    static boolean isSupported() {
        return FastChargeUtils.isSupported();
    }

    static void restoreState(Context context) {
        if (isSupported()) {
            Utils.enableComponent(context, FastChargeSettingsActivity.class);
        } else {
            Utils.disableComponent(context, FastChargeSettingsActivity.class);
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final ActionBar actionBar = getActionBar();
        if (actionBar != null) {
            actionBar.setDisplayHomeAsUpEnabled(true);
            actionBar.setHomeButtonEnabled(true);
        }
        getFragmentManager().beginTransaction().replace(android.R.id.content,
                new FastChargeSettingsFragment()).commit();
    }

}
