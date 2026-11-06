// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.text.SimpleDateFormat;
import java.util.Arrays;
import java.util.Date;
import java.util.Locale;

/**
 * Logs in Downloads\RatchetQuestVR, as RunLog keeps them in the headset's Download folder: every set-up, and what each
 * run left (the game's output, where the VR mod writes, the game's own log, crash reports, why VR stopped), written
 * as the game closes, or at the launcher's next start if it was closed first.
 */
final class PcRunLog {
    private PcRunLog() {}

    private static String stamp() {
        return new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(new Date());
    }

    static File write(PcPaths paths, String name, String text) {
        File file = new File(paths.logs, name);
        try {
            paths.logs.mkdirs();
            Files.write(file.toPath(), text.getBytes(StandardCharsets.UTF_8));
            return file;
        } catch (IOException e) {
            System.err.println("cannot write " + file + ": " + e);
            return null;
        }
    }

    static void saveSetup(PcPaths paths, String text) {
        write(paths, "setup_" + stamp() + ".txt", text);
    }

    private static void section(StringBuilder out, String title, String body) {
        out.append("\n======== ").append(title).append(" ========\n").append(body).append('\n');
    }

    static String read(File file) {
        try {
            return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "(cannot be read: " + e.getMessage() + ")";
        }
    }

    /** What the last run of the game left, once: the report's file, or null when there was nothing. */
    static synchronized File saveLastRun(PcPaths paths, String ended) {
        File marker = paths.runPending();
        if (!marker.exists()) {
            return null;
        }
        marker.delete();
        StringBuilder out = new StringBuilder();
        out.append("Ratchet & Clank VR ").append(PcBuild.VERSION).append(" (PC)\n");
        out.append(System.getProperty("os.name")).append(' ').append(System.getProperty("os.version")).append(", ")
            .append(System.getProperty("os.arch")).append(", ").append(Runtime.getRuntime().availableProcessors())
            .append(" processors\n");
        out.append("the game ").append(ended).append('\n');
        File status = paths.statusFile();
        if (status.isFile()) {
            section(out, "why VR stopped", read(status));
        }
        File output = paths.gameOutput();
        if (output.isFile()) {
            section(out, "the game's output", read(output));
        }
        File[] files = paths.data().listFiles();
        if (files != null) {
            Arrays.sort(files);
            for (File file : files) {
                // This run's log; the game keeps the earlier runs' as <name>.1.log and so on.
                if (file.getName().endsWith(".log") && !file.getName().matches(".*\\.\\d+\\.log")) {
                    section(out, file.getName(), read(file));
                }
            }
        }
        File plugins = new File(paths.plugins(), "status.json");
        if (plugins.isFile()) {
            section(out, "plugins/status.json", read(plugins));
        }
        File[] crashes = new File(paths.data(), "crashes").listFiles();
        if (crashes != null) {
            Arrays.sort(crashes);
            for (File crash : crashes) {
                if (crash.isFile()) {
                    section(out, "crash " + crash.getName(), read(crash));
                }
            }
        }
        String report = out.toString();
        File saved = write(paths, "run_" + stamp() + ".txt", report);
        // The newest report stays with the launcher too: its Logs page shows it.
        try {
            Files.write(paths.lastReport().toPath(), report.getBytes(StandardCharsets.UTF_8));
        } catch (IOException e) {
            System.err.println("cannot keep the last report: " + e);
        }
        return saved;
    }

    /** The report's lines that say most, for the Logs page, as RunLog.lastRunTail. */
    static String lastRunTail(PcPaths paths) {
        File report = paths.lastReport();
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
