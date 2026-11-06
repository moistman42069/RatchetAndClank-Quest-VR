// SPDX-License-Identifier: GPL-3.0-or-later
// After the Halo CE Decomp Remix launcher's Updater (moistman42069), as the headset's Updater.
package dev.ratchetquest.vr;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Updates the PC release from this project's GitHub releases (UpdatePolicy), as Updater does the APK: the latest
 * release's Windows zip, checked against GitHub's SHA-256 of it and its size, for a newer version. Installing unpacks
 * it beside the launcher's files, then a small script waits for the launcher to close, puts the new files in place
 * and opens it again. The set-up, the levels, the saves and the settings are in the player's own folder: they stay.
 */
final class PcUpdater {
    private static final long CHECK_INTERVAL = 6L * 60 * 60 * 1000;
    private static final int MOST_FILES = 100000;
    /** The release's folders, each replaced whole; its files at the top are copied over. */
    static final String[] FOLDERS = {"app", "runtime", "openrac", "vr", "mods", "texts"};

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
        volatile boolean ready;     // its zip downloaded and checked, to install
    }

    static final State STATE = new State();

    private PcUpdater() {}

    private static Prefs prefs(PcPaths paths) {
        return new Prefs(paths.prefs("updates"));
    }

    static boolean automatic(PcPaths paths) {
        return prefs(paths).getBoolean("automatic", true);
    }

    static void setAutomatic(PcPaths paths, boolean on) {
        prefs(paths).putBoolean("automatic", on);
    }

    static String status(PcPaths paths) {
        return prefs(paths).getString("status", "Not checked yet.");
    }

    private static void status(PcPaths paths, String text) {
        prefs(paths).putString("status", text);
    }

    private static File zip(PcPaths paths) {
        return new File(paths.updates(), "release.zip");
    }

    /** As the launcher opens: a check, when they are automatic and the last is 6 hours old. */
    static void maybeCheck(PcPaths paths, Runnable done) {
        long now = System.currentTimeMillis();
        long last = prefs(paths).getLong("last_check", 0);
        if (network && automatic(paths) && (last > now || now - last >= CHECK_INTERVAL)) {
            check(paths, done);
        }
    }

    static void check(PcPaths paths, Runnable done) {
        if (!network || STATE.busy) {
            return;
        }
        STATE.busy = true;
        status(paths, "Checking for updates...");
        new Thread(() -> {
            try {
                prefs(paths).putLong("last_check", System.currentTimeMillis());
                Map<String, Object> release = Json.object(new String(fetch(UpdatePolicy.LATEST, 1 << 20),
                    StandardCharsets.UTF_8));
                String tag = Json.string(release, "tag_name", "");
                String name = UpdatePolicy.pcAsset(tag);
                if (Boolean.TRUE.equals(release.get("draft")) || Boolean.TRUE.equals(release.get("prerelease"))
                        || name == null) {
                    throw new IOException("the latest release (" + tag + ") is not one this launcher installs");
                }
                Map<String, Object> found = null;
                List<Object> assets = Json.array(release.get("assets"));
                for (int i = 0; assets != null && i < assets.size(); i++) {
                    Map<String, Object> asset = Json.object(assets.get(i));
                    if (asset != null && name.equals(Json.string(asset, "name", ""))) {
                        if (found != null) {
                            throw new IOException("the release has two " + name);
                        }
                        found = asset;
                    }
                }
                if (!UpdatePolicy.newer(tag, PcBuild.VERSION)) {
                    STATE.tag = null;
                    status(paths, "Up to date: " + PcBuild.VERSION + " is the latest release.");
                    return;
                }
                if (found == null) {
                    throw new IOException("the release " + tag + " has no " + name + " for the PC yet");
                }
                String url = Json.string(found, "browser_download_url", "");
                String digest = Json.string(found, "digest", "");
                long size;
                try {
                    size = Long.parseLong(Json.string(found, "size", "0"));
                } catch (NumberFormatException e) {
                    size = 0;
                }
                if (!UpdatePolicy.digest(digest) || !UpdatePolicy.allowedUrl(url) || size <= 0
                        || size > UpdatePolicy.MAX_PC_ZIP) {
                    throw new IOException("the release's zip has no checksum to check it by");
                }
                // A zip of this release downloaded and checked before (the launcher closed since) is ready still.
                STATE.ready = tag.equals(prefs(paths).getString("ready", "")) && zip(paths).length() == size;
                STATE.url = url;
                STATE.digest = digest;
                STATE.size = size;
                STATE.tag = tag;
                status(paths, "Version " + tag.substring(1) + " is available (" + size / 1_000_000 + " MB). "
                    + "Your set-up, saves and settings are kept.");
            } catch (NotFound e) {
                status(paths, "No release is published yet. You have " + PcBuild.VERSION + ".");
            } catch (Exception e) {
                // Offline is not an error worth a check every launch: another in 15 minutes.
                prefs(paths).putLong("last_check", System.currentTimeMillis() - CHECK_INTERVAL + 15L * 60 * 1000);
                status(paths, "Could not check for updates (" + e.getMessage() + "). The game still plays.");
            } finally {
                STATE.busy = false;
                done.run();
            }
        }, "update-check").start();
    }

    /** Downloads the newer release found and checks it; ready to install after. */
    static void download(PcPaths paths, Runnable done) {
        final String tag = STATE.tag, url = STATE.url, digest = STATE.digest;
        final long size = STATE.size;
        if (!network || STATE.busy || tag == null) {
            return;
        }
        STATE.busy = true;
        STATE.percent = 0;
        status(paths, "Downloading " + tag.substring(1) + "...");
        new Thread(() -> {
            File folder = paths.updates();
            File part = new File(folder, "download.part");
            File zip = zip(paths);
            try {
                if (!folder.isDirectory() && !folder.mkdirs()) {
                    throw new IOException("no room for the download");
                }
                zip.delete();
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
                if (!part.renameTo(zip)) {
                    throw new IOException("cannot keep the download");
                }
                prefs(paths).putString("ready", tag);
                STATE.ready = true;
                status(paths, "Version " + tag.substring(1) + " is downloaded and checked. Install closes the launcher, "
                    + "puts it in place and opens it again; your set-up, saves and settings are kept.");
            } catch (Exception e) {
                status(paths, "Update not installed: " + e.getMessage() + ". This version is unchanged.");
            } finally {
                part.delete();
                STATE.percent = -1;
                STATE.busy = false;
                done.run();
            }
        }, "update-download").start();
    }

    /**
     * Unpacks the zip downloaded and hands it to the script that puts it in place once this launcher has closed;
     * null when it is on its way (the launcher then closes), else why not, in the player's words.
     */
    static String install(PcPaths paths, boolean start) {
        File zip = zip(paths);
        if (!STATE.ready || !zip.isFile()) {
            return "Download the update first.";
        }
        File fresh = new File(paths.updates(), "new");
        try {
            // Checked again: it lay in the player's folder since it was downloaded.
            if (STATE.digest != null) {
                MessageDigest sha = MessageDigest.getInstance("SHA-256");
                try (InputStream in = Files.newInputStream(zip.toPath())) {
                    byte[] buffer = new byte[1 << 16];
                    for (int n = in.read(buffer); n != -1; n = in.read(buffer)) {
                        sha.update(buffer, 0, n);
                    }
                }
                if (!STATE.digest.equalsIgnoreCase("sha256:" + hex(sha.digest()))) {
                    zip.delete();
                    STATE.ready = false;
                    return "The download changed since it was checked: download it again.";
                }
            }
            PcPaths.deleteRecursively(fresh);
            fresh.mkdirs();
            unpack(zip, fresh);
            File top = fresh;
            if (!new File(top, PcPaths.EXE).isFile()) {
                File[] inside = top.listFiles();
                if (inside != null && inside.length == 1 && inside[0].isDirectory()) {
                    top = inside[0];
                }
            }
            if (!new File(top, PcPaths.EXE).isFile() || !new File(top, "app").isDirectory()) {
                return "The download is not this launcher's release. This version is unchanged.";
            }
            String writable = writable(paths.install);
            if (writable != null) {
                return writable;
            }
            if (paths.updates().getPath().contains("%")) {
                return "The launcher's data folder (" + paths.home + ") has a % in its name. Unzip the release over "
                    + paths.install + " yourself (the release page has it).";
            }
            File script = writeScript(paths, top, ProcessHandle.current().pid(), start);
            runScript(script);
            status(paths, "Installing " + STATE.tag + "...");
            return null;
        } catch (IOException | java.security.NoSuchAlgorithmException e) {
            return "Update not installed: " + e.getMessage() + ". This version is unchanged.";
        }
    }

    /** Null when the launcher's own folder can take the new files, else what to do instead. */
    private static String writable(File install) {
        String manual = "Unzip the release over " + install + " yourself (the release page has it).";
        if (install.getPath().contains("%")) {
            return "The launcher's folder has a % in its name. " + manual;
        }
        File probe = new File(install, ".update-probe");
        try {
            Files.write(probe.toPath(), new byte[] {1});
            Files.delete(probe.toPath());
            return null;
        } catch (IOException e) {
            return "The launcher's folder cannot be written. " + manual;
        }
    }

    /**
     * Windows' own cmd.exe on the script, named by its folder's path (.\apply.cmd): where NoDefaultCurrentDirectoryInExePath
     * is set, cmd does not look in the current folder for a bare name, and the script would not run.
     */
    static Process runScript(File script) throws IOException {
        String root = System.getenv("SystemRoot");
        File cmd = root == null ? null : new File(root, "System32\\cmd.exe");
        return new ProcessBuilder(cmd != null && cmd.isFile() ? cmd.getPath() : "cmd.exe", "/d", "/c",
            ".\\" + script.getName()).directory(script.getParentFile()).redirectErrorStream(true)
            .redirectOutput(ProcessBuilder.Redirect.DISCARD).start();
    }

    /**
     * The script, in the updates folder: it waits for this launcher's process to end, mirrors the release's folders
     * over the old ones and copies its files at the top, then opens the launcher (when `start`) and clears what it
     * unpacked. Paths are written into it, not passed: cmd's quoting of arguments is fragile.
     */
    static File writeScript(PcPaths paths, File fresh, long pid, boolean start) throws IOException {
        File script = new File(paths.updates(), "apply.cmd");
        File log = new File(paths.updates(), "apply.log");
        StringBuilder s = new StringBuilder();
        s.append("@echo off\r\n");
        s.append("chcp 65001 >nul\r\n");
        s.append("title Updating Ratchet ^& Clank VR\r\n");
        s.append("set \"NEW=").append(fresh.getAbsolutePath()).append("\"\r\n");
        s.append("set \"DEST=").append(paths.install.getAbsolutePath()).append("\"\r\n");
        s.append("set \"LOG=").append(log.getAbsolutePath()).append("\"\r\n");
        s.append("set \"PID=").append(pid).append("\"\r\n");
        // Windows' own tools by their full path: another ping or findstr earlier on the PATH (Cygwin's) does not do.
        s.append("set \"SYS=%SystemRoot%\\System32\"\r\n");
        s.append("echo Updating Ratchet ^& Clank VR: the launcher opens again when it is done.\r\n");
        s.append("echo update into \"%DEST%\"> \"%LOG%\"\r\n");
        s.append(":wait\r\n");
        s.append("\"%SYS%\\tasklist.exe\" /FI \"PID eq %PID%\" /NH 2>nul | \"%SYS%\\findstr.exe\" /C:\" %PID% \" >nul"
            + " && (\"%SYS%\\PING.EXE\" -n 2 127.0.0.1 >nul & goto wait)\r\n");
        for (String folder : FOLDERS) {
            s.append("if exist \"%NEW%\\").append(folder).append("\" (\r\n");
            s.append("  \"%SYS%\\Robocopy.exe\" \"%NEW%\\").append(folder).append("\" \"%DEST%\\").append(folder)
                .append("\" /MIR /R:10 /W:1 /NP /NFL /NDL >> \"%LOG%\"\r\n");
            s.append("  if errorlevel 8 goto failed\r\n");
            s.append(")\r\n");
        }
        s.append("\"%SYS%\\Robocopy.exe\" \"%NEW%\" \"%DEST%\" /R:10 /W:1 /NP /NFL /NDL >> \"%LOG%\"\r\n");
        s.append("if errorlevel 8 goto failed\r\n");
        s.append("echo done >> \"%LOG%\"\r\n");
        if (start) {
            s.append("start \"\" \"%DEST%\\").append(PcPaths.EXE).append("\"\r\n");
        }
        s.append("rmdir /s /q \"%NEW%\"\r\n");
        s.append("exit /b 0\r\n");
        s.append(":failed\r\n");
        s.append("echo the update stopped part way: unzip the release over \"%DEST%\" yourself>> \"%LOG%\"\r\n");
        if (start) {
            s.append("start \"\" \"%DEST%\\").append(PcPaths.EXE).append("\"\r\n");
        }
        s.append("exit /b 1\r\n");
        paths.updates().mkdirs();
        Files.write(script.toPath(), s.toString().getBytes(StandardCharsets.UTF_8));
        return script;
    }

    /** What the last install's script wrote, for the launcher that started after it; "" when none. */
    static String lastInstall(PcPaths paths) {
        File log = new File(paths.updates(), "apply.log");
        if (!log.isFile()) {
            return "";
        }
        String text = PcRunLog.read(log);
        log.delete();
        zip(paths).delete();
        return text.contains("\ndone") || text.startsWith("done") ? "" : "The last update did not finish: "
            + "unzip the release over " + paths.install + " yourself (the release page has it).";
    }

    private static void unpack(File zip, File to) throws IOException {
        int files = 0;
        byte[] buffer = new byte[1 << 16];
        try (ZipInputStream in = new ZipInputStream(Files.newInputStream(zip.toPath()))) {
            for (ZipEntry entry = in.getNextEntry(); entry != null; entry = in.getNextEntry()) {
                File file = new File(to, entry.getName());
                if (!PcMods.inside(file, to)) {
                    throw new IOException("a path in the download leaves its folder");
                }
                if (++files > MOST_FILES) {
                    throw new IOException("the download has too many files");
                }
                if (entry.isDirectory()) {
                    file.mkdirs();
                    continue;
                }
                file.getParentFile().mkdirs();
                try (OutputStream out = Files.newOutputStream(file.toPath())) {
                    for (int n = in.read(buffer); n > 0; n = in.read(buffer)) {
                        out.write(buffer, 0, n);
                    }
                }
            }
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
            connection.setRequestProperty("User-Agent", "RatchetQuestVR-pc-updater/" + PcBuild.VERSION);
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
