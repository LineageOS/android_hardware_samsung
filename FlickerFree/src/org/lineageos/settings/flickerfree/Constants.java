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

package org.lineageos.settings.flickerfree;

import java.util.HashMap;
import java.util.Map;

class Constants {

    static final String ACTION_INITIALIZE_LINEAGE_HARDWARE =
            lineageos.content.Intent.ACTION_INITIALIZE_LINEAGE_HARDWARE;

    // Holds <preference_key> -> <proc_node> mapping
    static final Map<String, String> sBooleanNodePreferenceMap = new HashMap<>();
    static final Map<String, String> sStringNodePreferenceMap = new HashMap<>();

    // Holds <preference_key> -> <default_values> mapping
    static final Map<String, Object> sNodeDefaultMap = new HashMap<>();

    static final String[] sFlickerFreePrefKeys = {
        FlickerFreeConstants.FLICKER_FREE_KEY,
    };

    static {
        sBooleanNodePreferenceMap.put(
                FlickerFreeConstants.FLICKER_FREE_KEY,
                FlickerFreeConstants.FLICKER_FREE_NODE);

        sNodeDefaultMap.put(FlickerFreeConstants.FLICKER_FREE_KEY, false);
    }

}
