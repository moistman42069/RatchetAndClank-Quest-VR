// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.io.File;
import java.net.URISyntaxException;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Where everything lives on the PC, as Tools says it for the headset. What came in the release's folder (the launcher,
 * OpenRAC's tools with their Python, the game for both discs, the VR mod, the bundled mods) is read from it and never
 * written: the player's set-up, saves, choices and added mods are in %LOCALAPPDATA%\RatchetQuestVR, OpenRAC's own
 * launcher's folder (%LOCALAPPDATA%\openrac) left alone. The logs go to Downloads\RatchetQuestVR, as the headset's do
 * to its Download folder.
 *
 * RATCHETQUESTVR_HOME, RATCHETQUESTVR_INSTALL and RATCHETQUESTVR_LOGS move them, for the tests.
 */
final class PcPaths {
    static final String EXE = "RatchetClankVR.exe";

    final File install;
    final File home;
    final File logs;

    PcPaths() {
        install = installFolder();
        home = homeFolder();
        logs = logsFolder(home);
    }

    PcPaths(File install, File home, File logs) {
        this.install = install;
        this.home = home;
        this.logs = logs;
    }

    private static File installFolder() {
        String given = System.getenv("RATCHETQUESTVR_INSTALL");
        if (given != null && !given.isEmpty()) {
            return new File(given).getAbsoluteFile();
        }
        // The release's launcher (jpackage's) says where its .exe is.
        String app = System.getProperty("jpackage.app-path");
        if (app != null && !app.isEmpty()) {
            return new File(app).getAbsoluteFile().getParentFile();
        }
        // Else the jar's folder's parent: <install>/app/launcher.jar.
        try {
            File jar = new File(PcPaths.class.getProtectionDomain().getCodeSource().getLocation().toURI());
            return jar.getParentFile().getParentFile();
        } catch (URISyntaxException | RuntimeException e) {
            return new File(".").getAbsoluteFile();
        }
    }

    private static File homeFolder() {
        String given = System.getenv("RATCHETQUESTVR_HOME");
        if (given != null && !given.isEmpty()) {
            return new File(given).getAbsoluteFile();
        }
        String local = System.getenv("LOCALAPPDATA");
        if (local == null || local.isEmpty()) {
            local = System.getProperty("user.home") + File.separator + "AppData" + File.separator + "Local";
        }
        return new File(local, "RatchetQuestVR");
    }

    private static File logsFolder(File home) {
        String given = System.getenv("RATCHETQUESTVR_LOGS");
        if (given != null && !given.isEmpty()) {
            return new File(given).getAbsoluteFile();
        }
        File downloads = new File(System.getProperty("user.home"), "Downloads");
        return downloads.isDirectory() ? new File(downloads, "RatchetQuestVR") : new File(home, "logs");
    }

    // ---- What came with the release ----

    /** OpenRAC's copy (tools/bundle_standalone.py): its tools, editor, Python and the game for both discs. */
    File root() { return new File(install, "openrac"); }
    File python() { return new File(root(), "python/python.exe"); }
    File game(String version) { return new File(root(), "port/build/release/openrac-rac1-" + version + ".exe"); }
    /** The VR mod as its build packaged it: quest_vr.dll, the OpenXR loader beside it, its manifest. */
    File vrMod() { return new File(install, "vr"); }
    /** The bundled mods, a folder each as their build packaged them. */
    File bundledMods() { return new File(install, "mods"); }
    File texts() { return new File(install, "texts"); }

    // ---- The player's ----

    File data() { return new File(home, "data"); }
    File levels(String version) { return new File(data(), "port/" + version); }
    File keptDisc() { return new File(data(), "iso_data/rac1/disc.iso"); }
    File plugins() { return new File(data(), "plugins"); }
    File settingsFile() { return new File(data(), "quest-settings.ini"); }
    File statusFile() { return new File(data(), "quest-vr-status.txt"); }
    File runPending() { return new File(data(), "quest-run-pending"); }
    File gameOutput() { return new File(data(), "quest-game-output.txt"); }
    File lastReport() { return new File(data(), "quest-last-report.txt"); }
    /** The memory cards, as the game keeps them in its data home: memcard/<serial>, the mods' files beside. */
    File cards(Discs.Disc disc) { return new File(home, "memcard/" + disc.serial); }
    /** Each mod's own files (the SDK's data_dir, mod-data/<id>), beside the cards as the fork's options.cpp keeps them. */
    File modFiles() { return new File(home, "memcard/mod-data"); }
    File addedMods() { return new File(home, "added-mods"); }
    File updates() { return new File(home, "updates"); }
    File prefs(String name) { return new File(home, name + ".properties"); }

    /**
     * What every program, the game among them, runs with: this PC's own, less what would turn the tools' Python or
     * the mod's test headset elsewhere.
     */
    Map<String, String> environment() {
        Map<String, String> env = new LinkedHashMap<>(System.getenv());
        for (String key : new String[] {"PYTHONHOME", "PYTHONPATH", "PYTHONSTARTUP", "PYTHONUSERBASE", "QUEST_SIM",
                "QUEST_SIM_SCRIPT"}) {
            env.keySet().removeIf(name -> name.equalsIgnoreCase(key));
        }
        env.put("PYTHONDONTWRITEBYTECODE", "1");
        env.put("PYTHONUNBUFFERED", "1");
        env.put("PYTHONUTF8", "1");
        return env;
    }

    /** The tools' Python, isolated from any other on this PC (-E -s), unbuffered, writing no bytecode in the release. */
    String[] pythonCommand() {
        return new String[] {python().getPath(), "-E", "-s", "-B", "-u", "-X", "utf8"};
    }

    static void deleteRecursively(File file) {
        if (file == null) {
            return;
        }
        if (java.nio.file.Files.isSymbolicLink(file.toPath())) {
            file.delete();
            return;
        }
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteRecursively(child);
            }
        }
        file.delete();
    }
}
