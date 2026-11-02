// SPDX-License-Identifier: GPL-3.0-or-later
// The unpacking and environment follow OpenRAC's OpenracSetup.kt (GPL-3.0-or-later).
package dev.ratchetquest.vr;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.system.Os;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * OpenRAC's tools and where everything lives. Android runs programs only from the app's native
 * library folder, so Python and the games' windowless programs are libraries there, linked from
 * where the tools expect them; the folder moves on an update, so the links are made again each time.
 */
final class Tools {
    private static final String TAG = "RatchetQuest";
    private static final String BUNDLE = "openrac.zip";

    private final Context context;

    Tools(Context context) {
        this.context = context.getApplicationContext();
    }

    File root() { return new File(context.getFilesDir(), "openrac"); }
    File discs() { return new File(context.getFilesDir(), "discs"); }
    File data() { return new File(context.getFilesDir(), "games/active/rac1/data"); }
    File levels(String version) { return new File(data(), "port/" + version); }
    File keptDisc() { return new File(data(), "iso_data/rac1/disc.iso"); }
    File plugins() { return new File(data(), "plugins"); }
    File settingsFile() { return new File(data(), "quest-settings.ini"); }
    File statusFile() { return new File(data(), "quest-vr-status.txt"); }
    File nativeLibraries() { return new File(context.getApplicationInfo().nativeLibraryDir); }
    File python() { return new File(root(), "python/bin/python3"); }

    /** What every program, the game among them, runs with. */
    Map<String, String> environment() {
        Map<String, String> env = new LinkedHashMap<>();
        env.put("HOME", context.getFilesDir().getPath());
        env.put("TMPDIR", context.getCacheDir().getPath());
        env.put("XDG_DATA_HOME", new File(context.getFilesDir(), "share").getPath());
        env.put("XDG_CONFIG_HOME", new File(context.getFilesDir(), "config").getPath());
        env.put("OPENRAC_NATIVE_LIBRARY_DIR", nativeLibraries().getPath());
        env.put("PYTHONHOME", new File(root(), "python").getPath());
        env.put("PYTHONDONTWRITEBYTECODE", "1");
        env.put("PYTHONUNBUFFERED", "1");
        env.put("LD_LIBRARY_PATH", nativeLibraries().getPath());
        return env;
    }

    /**
     * Makes the tools ready: unpacked once per install or update, linked every time. One at a time: Play and the set-up
     * can both ask, and each moves the same folders.
     */
    void prepare() throws IOException {
        synchronized (Tools.class) {
            prepareNow();
        }
    }

    private void prepareNow() throws IOException {
        String stamp = stamp();
        File marker = new File(root(), ".unpacked");
        boolean unpacked = marker.isFile()
            && new String(Files.readAllBytes(marker.toPath()), StandardCharsets.UTF_8).equals(stamp);
        if (!unpacked) {
            unpack(stamp);
        }
        link(new File(nativeLibraries(), "libopenrac-python.so"), python());
        File[] libraries = nativeLibraries().listFiles();
        if (libraries != null) {
            for (File library : libraries) {
                String name = library.getName();
                if (name.startsWith("libopenrac-") && name.endsWith("-cli.so")) {
                    String game = name.substring("libopenrac-".length(), name.length() - "-cli.so".length());
                    link(library, windowless(game));
                }
            }
        }
    }

    File windowless(String game) { return new File(root(), "port/build/release/openrac-" + game); }

    private String stamp() throws IOException {
        try {
            PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            return info.getLongVersionCode() + " " + info.lastUpdateTime;
        } catch (PackageManager.NameNotFoundException e) {
            throw new IOException(e);
        }
    }

    private void unpack(String stamp) throws IOException {
        long started = System.nanoTime();
        File fresh = new File(context.getFilesDir(), "openrac.new");
        File old = new File(context.getFilesDir(), "openrac.old");
        deleteRecursively(fresh);
        deleteRecursively(old);
        if (!fresh.mkdirs()) {
            throw new IOException("cannot make " + fresh);
        }
        String inside = fresh.getCanonicalPath() + File.separator;
        byte[] buffer = new byte[1 << 16];
        try (InputStream asset = context.getAssets().open(BUNDLE);
                ZipInputStream zip = new ZipInputStream(new java.io.BufferedInputStream(asset))) {
            for (ZipEntry entry = zip.getNextEntry(); entry != null; entry = zip.getNextEntry()) {
                File dest = new File(fresh, entry.getName());
                if (!dest.getCanonicalPath().startsWith(inside)) {
                    throw new IOException("the tools name a file outside them: " + entry.getName());
                }
                if (entry.isDirectory()) {
                    dest.mkdirs();
                    continue;
                }
                dest.getParentFile().mkdirs();
                try (FileOutputStream out = new FileOutputStream(dest)) {
                    for (int n = zip.read(buffer); n > 0; n = zip.read(buffer)) {
                        out.write(buffer, 0, n);
                    }
                }
            }
        }
        Files.write(new File(fresh, ".unpacked").toPath(), stamp.getBytes(StandardCharsets.UTF_8));
        File root = root();
        // The tools hold nothing of the player's and were just unpacked anew: what will not move goes, so that what an
        // earlier update left behind can never stop the game.
        if (root.exists() && !root.renameTo(old)) {
            Log.w(TAG, "cannot move " + root + " aside: cleared in its place");
            deleteRecursively(root);
        }
        if (root.exists()) {
            throw new IOException("cannot clear " + root);
        }
        if (!fresh.renameTo(root)) {
            throw new IOException("cannot move " + fresh + " to " + root);
        }
        deleteRecursively(old);
        Log.i(TAG, "tools unpacked in " + (System.nanoTime() - started) / 1000000 + " ms");
    }

    private static void link(File target, File link) throws IOException {
        link.getParentFile().mkdirs();
        link.delete();
        if (!target.isFile()) {
            throw new IOException("this APK has no " + target.getName());
        }
        try {
            Os.symlink(target.getPath(), link.getPath());
        } catch (android.system.ErrnoException e) {
            throw new IOException("cannot link " + link + ": " + e.getMessage());
        }
    }

    static void deleteRecursively(File file) {
        if (file == null) {
            return;
        }
        // A link is deleted itself, not followed, and first: an update moves the library folder the tools' links
        // point into, and exists() is false for a link whose target went. Left, they keep the folders from going.
        if (Files.isSymbolicLink(file.toPath())) {
            file.delete();
            return;
        }
        if (!file.exists()) {
            return;
        }
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursively(child);
                }
            }
        }
        file.delete();
    }
}
