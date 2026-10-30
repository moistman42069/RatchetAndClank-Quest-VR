// SPDX-License-Identifier: GPL-3.0-or-later
// After the Halo CE Decomp Remix launcher's Updater (moistman42069).
package dev.ratchetquest.vr;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Arrays;
import java.util.Locale;

/**
 * Updates the app from this project's GitHub releases (UpdatePolicy): the latest release's APK,
 * checked against GitHub's SHA-256 of it, its size, the package, a newer version and the signing
 * key of the app installed, then handed to Android's installer, which asks the player. The same
 * package and key: the set-up, the levels, the saves and the settings stay.
 */
final class Updater {
    private static final String TAG = "RatchetQuest";
    static final String PREFS = "updates";
    private static final long CHECK_INTERVAL = 6L * 60 * 60 * 1000;

    /** Off in the launcher's tests, which never reach the network. */
    static volatile boolean network = true;

    /** Where it is; written here, read by the launcher's page. */
    static final class State {
        volatile boolean busy;
        volatile int percent = -1;  // a download under way, else -1
        volatile String tag;        // a newer release, once a check found one
        volatile String url;
        volatile String digest;
        volatile long size;
        volatile boolean ready;     // its APK downloaded and checked, for the installer
    }

    static final State STATE = new State();

    private Updater() {}

    private static SharedPreferences prefs(Context context) {
        return context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
    }

    static boolean automatic(Context context) {
        return prefs(context).getBoolean("automatic", true);
    }

    static void setAutomatic(Context context, boolean on) {
        prefs(context).edit().putBoolean("automatic", on).apply();
    }

    static String status(Context context) {
        return prefs(context).getString("status", "Not checked yet.");
    }

    private static void status(Context context, String text) {
        Log.i(TAG, "update: " + text);
        prefs(context).edit().putString("status", text).apply();
    }

    /** As the launcher opens: a check, when they are automatic and the last is 6 hours old. */
    static void maybeCheck(Activity activity, Runnable done) {
        long now = System.currentTimeMillis();
        long last = prefs(activity).getLong("last_check", 0);
        if (network && automatic(activity) && (last > now || now - last >= CHECK_INTERVAL)) {
            check(activity, done);
        }
    }

    static void check(Activity activity, Runnable done) {
        if (!network || STATE.busy) {
            return;
        }
        Context app = activity.getApplicationContext();
        STATE.busy = true;
        status(app, "Checking for updates...");
        new Thread(() -> {
            try {
                prefs(app).edit().putLong("last_check", System.currentTimeMillis()).apply();
                JSONObject release = new JSONObject(new String(fetch(UpdatePolicy.LATEST, 1 << 20), StandardCharsets.UTF_8));
                String tag = release.optString("tag_name");
                String name = UpdatePolicy.asset(tag);
                if (release.optBoolean("draft") || release.optBoolean("prerelease") || name == null) {
                    throw new IOException("the latest release (" + tag + ") is not one this app installs");
                }
                JSONObject found = null;
                JSONArray assets = release.getJSONArray("assets");
                for (int i = 0; i < assets.length(); i++) {
                    if (name.equals(assets.getJSONObject(i).optString("name"))) {
                        if (found != null) {
                            throw new IOException("the release has two " + name);
                        }
                        found = assets.getJSONObject(i);
                    }
                }
                if (found == null) {
                    throw new IOException("the release " + tag + " has no " + name);
                }
                String url = found.getString("browser_download_url");
                String digest = found.optString("digest");
                long size = found.optLong("size");
                if (!UpdatePolicy.digest(digest) || !UpdatePolicy.allowedUrl(url) || size <= 0 || size > UpdatePolicy.MAX_APK) {
                    throw new IOException("the release's APK has no checksum to check it by");
                }
                if (!UpdatePolicy.newer(tag, BuildConfig.VERSION_NAME)) {
                    STATE.tag = null;
                    status(app, "Up to date: " + BuildConfig.VERSION_NAME + " is the latest release.");
                } else {
                    if (!tag.equals(STATE.tag)) {
                        STATE.ready = false;
                    }
                    STATE.url = url;
                    STATE.digest = digest;
                    STATE.size = size;
                    STATE.tag = tag;
                    status(app, "Version " + tag.substring(1) + " is available (" + size / 1_000_000 + " MB). "
                        + "Your set-up, saves and settings are kept.");
                }
            } catch (NotFound e) {
                status(app, "No release is published yet. You have " + BuildConfig.VERSION_NAME + ".");
            } catch (Exception e) {
                // Offline is not an error worth a check every launch: another in 15 minutes.
                prefs(app).edit().putLong("last_check", System.currentTimeMillis() - CHECK_INTERVAL + 15L * 60 * 1000).apply();
                status(app, "Could not check for updates (" + e.getMessage() + "). The game still plays.");
            } finally {
                STATE.busy = false;
                activity.runOnUiThread(done);
            }
        }, "update-check").start();
    }

    /** Downloads the newer release found, checks it, and hands it to Android's installer. */
    static void download(Activity activity, Runnable done) {
        final String tag = STATE.tag, url = STATE.url, digest = STATE.digest;
        final long size = STATE.size;
        if (!network || STATE.busy || tag == null) {
            return;
        }
        if (STATE.ready && UpdateProvider.apk(activity).isFile()) {
            install(activity);
            return;
        }
        Context app = activity.getApplicationContext();
        STATE.busy = true;
        STATE.percent = 0;
        status(app, "Downloading " + tag.substring(1) + "...");
        new Thread(() -> {
            File folder = new File(app.getCacheDir(), UpdateProvider.DIRECTORY);
            File part = new File(folder, "download.part");
            File apk = UpdateProvider.apk(app);
            boolean installNow = false;
            try {
                if (!folder.isDirectory() && !folder.mkdirs()) {
                    throw new IOException("no room for the download");
                }
                apk.delete();
                MessageDigest sha = MessageDigest.getInstance("SHA-256");
                long count = 0;
                HttpURLConnection connection = open(url);
                try (InputStream in = connection.getInputStream(); FileOutputStream out = new FileOutputStream(part)) {
                    byte[] buffer = new byte[1 << 16];
                    for (int n = in.read(buffer); n != -1; n = in.read(buffer)) {
                        count += n;
                        if (count > size) {
                            throw new IOException("the download is larger than the release says");
                        }
                        out.write(buffer, 0, n);
                        sha.update(buffer, 0, n);
                        STATE.percent = (int) (100 * count / size);
                    }
                    out.getFD().sync();
                } finally {
                    connection.disconnect();
                }
                if (count != size || !digest.equalsIgnoreCase("sha256:" + hex(sha.digest()))) {
                    throw new IOException("the download does not match the release's checksum");
                }
                checkPackage(app, part);
                if (!part.renameTo(apk)) {
                    throw new IOException("cannot keep the download");
                }
                STATE.ready = true;
                status(app, "Version " + tag.substring(1) + " is downloaded and checked. "
                    + "Android asks you to install it; your set-up, saves and settings are kept.");
                installNow = true;
            } catch (Exception e) {
                status(app, "Update not installed: " + e.getMessage() + ". This version is unchanged.");
            } finally {
                part.delete();
                STATE.percent = -1;
                STATE.busy = false;
                final boolean install = installNow;
                activity.runOnUiThread(() -> {
                    if (install && !activity.isFinishing()) {
                        install(activity);
                    }
                    done.run();
                });
            }
        }, "update-download").start();
    }

    /** The download is this app, newer, and signed with the key it is: else Android would refuse it. */
    @SuppressWarnings("deprecation")
    private static void checkPackage(Context context, File apk) throws Exception {
        PackageManager pm = context.getPackageManager();
        PackageInfo current = pm.getPackageInfo(context.getPackageName(),
            PackageManager.GET_SIGNING_CERTIFICATES | PackageManager.GET_SIGNATURES);
        PackageInfo next = pm.getPackageArchiveInfo(apk.getPath(),
            PackageManager.GET_SIGNING_CERTIFICATES | PackageManager.GET_SIGNATURES);
        if (next == null || !context.getPackageName().equals(next.packageName)) {
            throw new IOException("the download is not this app");
        }
        if (next.getLongVersionCode() <= current.getLongVersionCode()) {
            throw new IOException("the download is not newer than this version");
        }
        Signature[] before = signers(current);
        Signature[] after = signers(next);
        if (after == null) {
            Log.w(TAG, "update: the download's signer could not be read here; Android's installer checks it");
        } else if (before == null || before.length != 1 || after.length != 1 || !Arrays.equals(before[0].toByteArray(), after[0].toByteArray())) {
            throw new IOException("the download is signed with another key than this app");
        }
    }

    @SuppressWarnings("deprecation")
    private static Signature[] signers(PackageInfo info) {
        if (info.signingInfo != null) {
            return info.signingInfo.getApkContentsSigners();
        }
        return info.signatures;
    }

    /** Android's installer for the APK downloaded, once this app may ask for installs. */
    static void install(Activity activity) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && !activity.getPackageManager().canRequestPackageInstalls()) {
            status(activity, "Allow Ratchet & Clank Quest VR to install updates in the screen Android opens, then press "
                + "Install again.");
            try {
                activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,
                    Uri.parse("package:" + activity.getPackageName())));
            } catch (RuntimeException e) {
                status(activity, "Allow this app to install unknown apps in the headset's settings, then press Install again.");
            }
            return;
        }
        try {
            activity.startActivity(new Intent(Intent.ACTION_VIEW)
                .setDataAndType(UpdateProvider.URI, "application/vnd.android.package-archive")
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION));
        } catch (RuntimeException e) {
            status(activity, "Android could not open its installer (" + e.getMessage() + "). This version is unchanged.");
        }
    }

    private static final class NotFound extends IOException {
        NotFound() { super("not found"); }
    }

    /** A GitHub connection, each redirect checked against UpdatePolicy before it is followed. */
    private static HttpURLConnection open(String address) throws IOException {
        for (int i = 0; i < 6; i++) {
            if (!UpdatePolicy.allowedUrl(address)) {
                throw new IOException("an address outside GitHub");
            }
            HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
            connection.setConnectTimeout(20000);
            connection.setReadTimeout(20000);
            connection.setInstanceFollowRedirects(false);
            connection.setRequestProperty("User-Agent", "RatchetQuestVR-updater/" + BuildConfig.VERSION_NAME);
            connection.setRequestProperty("Accept", "application/vnd.github+json");
            int code = connection.getResponseCode();
            if (code >= 300 && code < 400) {
                String next = connection.getHeaderField("Location");
                connection.disconnect();
                if (next == null) {
                    throw new IOException("a redirect to nowhere");
                }
                address = new URL(new URL(address), next).toString();
                continue;
            }
            if (code == 404) {
                connection.disconnect();
                throw new NotFound();
            }
            if (code != 200) {
                connection.disconnect();
                throw new IOException("GitHub answered " + code);
            }
            return connection;
        }
        throw new IOException("too many redirects");
    }

    private static byte[] fetch(String address, int limit) throws IOException {
        HttpURLConnection connection = open(address);
        try (InputStream in = connection.getInputStream(); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[16384];
            for (int n = in.read(buffer); n != -1; n = in.read(buffer)) {
                if (out.size() + n > limit) {
                    throw new IOException("GitHub's answer is too large");
                }
                out.write(buffer, 0, n);
            }
            return out.toByteArray();
        } finally {
            connection.disconnect();
        }
    }

    static String hex(byte[] bytes) {
        StringBuilder out = new StringBuilder();
        for (byte b : bytes) {
            out.append(String.format(Locale.ROOT, "%02x", b & 0xff));
        }
        return out.toString();
    }
}
