/*
 * SPDX-FileCopyrightText: 2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.samsung.biometrics;

import android.app.Activity;
import android.graphics.Insets;
import android.os.Bundle;
import android.view.View;
import android.view.WindowInsets;
import android.widget.Switch;

public class DebugActivity extends Activity {
    private MaskView mMaskView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_debug);

        mMaskView = new MaskView(this);
        final Switch maskViewToggle = findViewById(R.id.mask_view_toggle);
        maskViewToggle.setOnCheckedChangeListener((buttonView, isChecked) -> {
            if (isChecked) {
                mMaskView.show();
            } else {
                mMaskView.hide();
             }
        });

        final View root = findViewById(R.id.main);
        if (root != null) {
            root.setOnApplyWindowInsetsListener((v, insets) -> {
                Insets sys = insets.getInsets(WindowInsets.Type.systemBars());
                v.setPadding(sys.left, sys.top, sys.right, sys.bottom);
                return insets;
            });
            root.requestApplyInsets();
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        if (mMaskView != null) mMaskView.hide();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (mMaskView != null) mMaskView.stop();
    }
}
