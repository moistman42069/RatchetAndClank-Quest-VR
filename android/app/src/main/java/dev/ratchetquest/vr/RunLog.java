// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.RandomAccessFile;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Logs in the headset's Download folder, Download/RatchetQuestVR, where a PC sees them over USB. What
 * a run left (the game's own log, crash reports, why VR stopped, this app's logcat) is put there the
 * next time the launcher opens, so a problem can be reported with no tools.
 */
final class RunLog {
    private static final String TAG = "RatchetQuest";
    static final String FOLDER = Environment.DIRECTORY_DOWNLOADS + "/RatchetQuestVR";

    private RunLog() {}

    private static String stamp() {
        return new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(new Date());
    }

    static void write(Context context, String name, String text) {
        ContentResolver resolver = context.getContentResolver();
        ContentValues values = new ContentValues();
        values.put(MediaStore.MediaColumns.DISPLAY_NAME, name);
        values.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
        values.put(MediaStore.MediaColumns.RELATIVE_PATH, FOLDER);
        Uri uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
        if (uri == null) {
            Log.w(TAG, "the Download folder refused " + name);
            return;
        }
        try (OutputStream out = resolver.openOutputStream(uri)) {
            if (out != null) {
                out.write(text.getBytes(StandardCharsets.UTF_8));
            }
        } catch (IOException e) {
            Log.w(TAG, "cannot write " + name + ": " + e);
        }
    }

    static void saveSetup(Context context, String text) {
        write(context, "setup_" + stamp() + ".txt", text);
    }

    private static void section(StringBuilder out, String title, String body) {
        out.append("\n======== ").append(title).append(" ========\n").append(body).append('\n');
    }

    private static String read(File file) {
        try {
            return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "(cannot be read: " + e.getMessage() + ")";
        }
    }

    /** The end of a file, at most `most` bytes of it, from a line's start. */
    private static String tail(File file, long most) {
        try (RandomAccessFile in = new RandomAccessFile(file, "r")) {
            long length = in.length();
            long from = Math.max(0, length - most);
            byte[] bytes = new byte[(int) (length - from)];
            in.seek(from);
            in.readFully(bytes);
            int start = 0;
            if (from > 0) {
                while (start < bytes.length && bytes[start] != '\n') {
                    start++;
                }
                start = Math.min(start + 1, bytes.length);
            }
            String text = new String(bytes, start, bytes.length - start, StandardCharsets.UTF_8);
            return from > 0 ? "(its first " + (from + start) + " bytes left out)\n" + text : text;
        } catch (IOException e) {
            return "(cannot be read: " + e.getMessage() + ")";
        }
    }

    /**
     * Multiplayer's verbose log (its Verbose log setting): from 0.1.23 the mod writes it to its own folder (the Files
     * app's Mod files, multiplayer), as OpenRAC's has it, not into the game's log. The run's report takes it, so it is
     * still sent with the log, and empties it: Multiplayer adds to it each run, and a long party writes megabytes.
     */
    private static void multiplayerTrace(StringBuilder out, Context context) {
        File trace = new File(context.getFilesDir(), "share/openrac/memcard/mod-data/multiplayer/mp-trace.log");
        if (!trace.isFile() || trace.length() == 0) {
            return;
        }
        section(out, "multiplayer mp-trace.log", tail(trace, 8L << 20));
        if (!trace.delete()) {
            Log.w(TAG, "cannot empty " + trace);
        }
    }

    private static String logcat() {
        StringBuilder out = new StringBuilder();
        try {
            Process process = new ProcessBuilder("logcat", "-d", "-v", "threadtime", "-t", "6000")
                .redirectErrorStream(true).start();
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8))) {
                for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                    out.append(line).append('\n');
                }
            }
        } catch (IOException e) {
            out.append("(no logcat: ").append(e.getMessage()).append(")\n");
        }
        return out.toString();
    }

    /** What the last run of the game left, once. True when there was something. */
    static boolean saveLastRun(Context context) {
        Tools tools = new Tools(context);
        File marker = new File(tools.data(), "quest-run-pending");
        if (!marker.exists()) {
            return false;
        }
        marker.delete();
        StringBuilder out = new StringBuilder();
        out.append("Ratchet & Clank Quest VR ").append(BuildConfig.VERSION_NAME).append('\n');
        out.append(android.os.Build.MANUFACTURER).append(' ').append(android.os.Build.MODEL)
            .append(", Android ").append(android.os.Build.VERSION.RELEASE).append('\n');
        File status = tools.statusFile();
        if (status.isFile()) {
            section(out, "why VR stopped", read(status));
        }
        File[] files = tools.data().listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.getName().endsWith(".log")) {
                    section(out, file.getName(), read(file));
                }
            }
        }
        File plugins = new File(tools.plugins(), "status.json");
        if (plugins.isFile()) {
            section(out, "plugins/status.json", read(plugins));
        }
        File[] crashes = new File(tools.data(), "crashes").listFiles();
        if (crashes != null) {
            for (File crash : crashes) {
                if (crash.isFile()) {
                    section(out, "crash " + crash.getName(), read(crash));
                }
            }
        }
        multiplayerTrace(out, context);
        section(out, "logcat", logcat());
        String report = out.toString();
        write(context, "run_" + stamp() + ".txt", report);
        // The newest report stays in the app too: the launcher's Logs page shows it in the headset.
        try {
            Files.write(lastReport(tools).toPath(), report.getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            Log.w(TAG, "cannot keep the last report: " + e);
        }
        return true;
    }

    private static File lastReport(Tools tools) {
        return new File(tools.data(), "quest-last-report.txt");
    }

    /** The report's lines that say most, for the headset's own Logs page. A mod the game refused ("mod quest-vr not
     *  loaded: ...") is among them: without it the headset stays black and nothing else says why. */
    static String lastRunTail(Context context) {
        File report = lastReport(new Tools(context));
        if (!report.isFile()) {
            return "No run yet.";
        }
        StringBuilder out = new StringBuilder();
        int kept = 0;
        for (String line : read(report).split("\n")) {
            if (line.contains("[openxr]") || line.contains("[vr-") || line.contains("======")
                    || line.contains("not loaded") || line.toLowerCase(Locale.US).contains("error")) {
                out.append(line).append('\n');
                if (++kept >= 400) {
                    break;
                }
            }
        }
        return out.length() > 0 ? out.toString() : read(report);
    }
}
