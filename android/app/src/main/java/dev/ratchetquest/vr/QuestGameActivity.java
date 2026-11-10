// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.view.WindowManager;

import dev.openrac.game.GameActivity;

/** The game, immersive: OpenRAC's activity with the headset's manifest entries (AndroidManifest.xml). */
public final class QuestGameActivity extends GameActivity {
    private static final String TAG = "RatchetQuestVR";

    /**
     * The game's hold on the sound (0.1.18, the owner's: "Hey Meta" left the game silent until it was started again).
     * Asked for whenever the game has the focus, so the system gives the sound back once its voice or menu is done
     * with it, and lowers the game under them rather than stopping it. The port opens its sound device again as well
     * (platform/audio.cpp).
     */
    private AudioFocusRequest audioFocus;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            askForSound();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        askForSound();
    }

    private void askForSound() {
        AudioManager audio = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (audio == null) {
            return;
        }
        if (audioFocus == null) {
            AudioAttributes game = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_GAME)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build();
            audioFocus = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(game)
                .setWillPauseWhenDucked(false)
                .setOnAudioFocusChangeListener(change -> Log.i(TAG, "sound focus: " + change))
                .build();
        }
        int result = audio.requestAudioFocus(audioFocus);
        Log.i(TAG, "sound focus asked: " + (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED ? "granted" : "not now (" + result + ")"));
    }
}
