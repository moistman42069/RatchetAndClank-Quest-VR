// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.net.Uri;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

/** Keeps the app alive while the game is set up: the headset may go to sleep on the player's desk. */
public final class SetupService extends Service {
    private static final String CHANNEL = "setup";
    private static final int ID = 1;

    static final String EXTRA_LEVELS_ONLY = "levels_only";

    static void start(Context context, Uri image) {
        Intent intent = new Intent(context, SetupService.class).setData(image)
            .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        context.startForegroundService(intent);
    }

    /** The levels made again from the disc kept on the headset (Setup.outdatedVersion). */
    static void startLevels(Context context) {
        context.startForegroundService(new Intent(context, SetupService.class).putExtra(EXTRA_LEVELS_ONLY, true));
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(
            new NotificationChannel(CHANNEL, "Setting up the game", NotificationManager.IMPORTANCE_LOW));
        Notification notification = new Notification.Builder(this, CHANNEL)
            .setSmallIcon(android.R.drawable.stat_sys_download)
            .setContentTitle("Ratchet & Clank Quest VR")
            .setContentText("Setting up the game from your disc")
            .setOngoing(true)
            .build();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(ID, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_DATA_SYNC);
        } else {
            startForeground(ID, notification);
        }
        boolean levelsOnly = intent != null && intent.getBooleanExtra(EXTRA_LEVELS_ONLY, false);
        if (Setup.STATE.running) {
            // The set-up under way keeps the service; it stops it when done.
            Setup.STATE.starting = false;
            return START_NOT_STICKY;
        }
        if (intent == null || (intent.getData() == null && !levelsOnly)) {
            Setup.STATE.starting = false;
            stopForeground(STOP_FOREGROUND_REMOVE);
            stopSelf(startId);
            return START_NOT_STICKY;
        }
        final Uri image = levelsOnly ? null : intent.getData();
        Setup.STATE.running = true;
        Setup.STATE.starting = false;
        new Thread(() -> {
            PowerManager power = getSystemService(PowerManager.class);
            PowerManager.WakeLock wake = power.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "ratchetquest:setup");
            wake.acquire(60L * 60L * 1000L);
            try {
                new Setup(this).run(image);
            } finally {
                wake.release();
                stopForeground(STOP_FOREGROUND_REMOVE);
                stopSelf();
            }
        }, "game-setup").start();
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
