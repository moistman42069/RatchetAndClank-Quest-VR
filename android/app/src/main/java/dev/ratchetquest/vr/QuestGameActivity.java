// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.os.Bundle;
import android.view.WindowManager;

import dev.openrac.game.GameActivity;

/** The game, immersive: OpenRAC's activity with the headset's manifest entries (AndroidManifest.xml). */
public final class QuestGameActivity extends GameActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
}
