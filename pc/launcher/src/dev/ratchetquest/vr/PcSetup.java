// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.FileStore;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Setting the game up from the player's disc, in the steps Setup takes on the headset and OpenRAC's own launcher on a
 * PC: tools/extractor.py, editor/extract.py port for the levels, then the game's own --check-levels. The image is read
 * where it is (the extractor keeps it in the data folder, a hard link on the same drive, else a copy). Runs on a thread
 * of its own; the launcher's page reads State.
 */
final class PcSetup {
    private static final long NEEDED_BEYOND_IMAGE = 2_000_000_000L;
    static final String KEY_VERSION = "version";

    /** How far it is; written here, read by the launcher's page. */
    static final class State {
        volatile boolean starting;  // asked for, and the thread not there yet
        volatile boolean running;
        volatile int percent;
        volatile String step = "";
        volatile String detail = "";
        volatile String failure;
        volatile boolean done;
    }

    static final State STATE = new State();
    /** The program running now, stopped if the launcher closes. */
    private static volatile Process current;

    private final PcPaths paths;
    private final StringBuilder log = new StringBuilder();

    PcSetup(PcPaths paths) {
        this.paths = paths;
    }

    static Prefs prefs(PcPaths paths) {
        return new Prefs(paths.prefs("setup"));
    }

    /** What the game's level check leaves beside levels it passed in the format this game reads. */
    static String checkStamp() {
        return "verified-" + PcBuild.LEVEL_FORMAT + ".json";
    }

    /** The disc version a finished set-up was made from, or null. */
    static String installedVersion(PcPaths paths) {
        String version = prefs(paths).getString(KEY_VERSION, null);
        if (version == null || Discs.byVersion(version) == null) {
            return null;
        }
        // Another format's stamp is levels an older or newer game made: this one cannot read them.
        return new File(paths.levels(version), checkStamp()).isFile() ? version : null;
    }

    /**
     * The disc version of a set-up whose levels this game cannot read (an update changed their format) while the disc
     * it was made from is still here, or null: the levels are made again from that disc without choosing it again.
     */
    static String outdatedVersion(PcPaths paths) {
        String version = prefs(paths).getString(KEY_VERSION, null);
        if (version == null || installedVersion(paths) != null) {
            return null;
        }
        File kept = paths.keptDisc();
        Discs.Disc disc = kept.isFile() ? Discs.bySize(kept.length()) : null;
        return disc != null && disc.version.equals(version) ? version : null;
    }

    /** Stops what the set-up is running: the launcher is closing. */
    static void stop() {
        Process process = current;
        if (process != null) {
            process.descendants().forEach(ProcessHandle::destroyForcibly);
            process.destroyForcibly();
        }
    }

    /** Sets the game up from `image`, or with none makes the levels again from the disc kept. */
    void run(File image) {
        STATE.failure = null;
        STATE.done = false;
        STATE.running = true;
        STATE.starting = false;
        try {
            step("Preparing tools", 1, "");
            log.append("Ratchet & Clank VR ").append(PcBuild.VERSION).append(" (PC), ")
                .append(System.getProperty("os.name")).append(' ').append(System.getProperty("os.version")).append('\n');
            for (File needed : new File[] {paths.python(), paths.game("pal")}) {
                if (!needed.isFile()) {
                    throw new Failure("This copy of the launcher is missing " + needed
                        + ". Unzip the release again, whole, and start it from there.");
                }
            }
            Discs.Disc disc;
            if (image != null) {
                disc = extract(read(image));
            } else {
                disc = Discs.bySize(paths.keptDisc().length());
                if (!paths.keptDisc().isFile() || disc == null) {
                    throw new Failure("The disc copy the set-up kept is gone: choose your .iso again.");
                }
                log.append("levels made again from the disc kept, for level format ").append(PcBuild.LEVEL_FORMAT)
                    .append('\n');
            }
            if (!paths.game(disc.version).isFile()) {
                throw new Failure("This release has no game for the " + disc.region + " disc ("
                    + paths.game(disc.version).getName() + ").");
            }
            levels(disc);
            check(disc);
            prefs(paths).putString(KEY_VERSION, disc.version);
            step("Ready", 100, disc.region + " disc set up");
            STATE.done = true;
        } catch (Failure failure) {
            log.append("set-up failed: ").append(failure.getMessage()).append('\n');
            STATE.failure = failure.getMessage();
        } catch (IOException | RuntimeException e) {
            log.append("set-up failed: ").append(e).append('\n');
            STATE.failure = "Set-up stopped: " + e.getMessage();
        } finally {
            PcRunLog.saveSetup(paths, log.toString());
            STATE.running = false;
        }
    }

    static final class Failure extends Exception {
        Failure(String message) { super(message); }
    }

    private void step(String step, int percent, String detail) {
        STATE.step = step;
        STATE.percent = percent;
        STATE.detail = detail;
        log.append("== ").append(step).append(' ').append(detail).append('\n');
    }

    /**
     * The chosen image read once through: its size and SHA-1 in the log, and room checked for what the set-up keeps
     * (the image itself too when the data folder is on another drive, where it cannot be linked).
     */
    private File read(File image) throws IOException, Failure {
        long size = image.length();
        if (!image.isFile()) {
            throw new Failure("The chosen file could not be opened.");
        }
        if (Discs.bySize(size) == null) {
            throw new Failure(Discs.wrongSize(size));
        }
        paths.data().mkdirs();
        FileStore home = Files.getFileStore(paths.data().toPath());
        boolean sameDrive = false;
        try {
            sameDrive = home.equals(Files.getFileStore(image.toPath()));
        } catch (IOException e) {
            // Taken as another drive: room for a copy.
        }
        long free = home.getUsableSpace();
        long needed = (sameDrive ? 0 : size) + NEEDED_BEYOND_IMAGE;
        if (free < needed) {
            throw new Failure("Not enough free space: this needs about " + (needed + 999_999_999L) / 1_000_000_000L
                + " GB on the drive of " + paths.home + " and it has " + free / 1_000_000_000L + " GB free.");
        }
        step("Reading your disc", 2, "This takes a minute or two");
        MessageDigest sha1;
        try {
            sha1 = MessageDigest.getInstance("SHA-1");
        } catch (java.security.NoSuchAlgorithmException e) {
            throw new IOException(e);
        }
        long done = 0;
        try (InputStream in = Files.newInputStream(image.toPath())) {
            byte[] buffer = new byte[1 << 20];
            for (int n = in.read(buffer); n > 0; n = in.read(buffer)) {
                sha1.update(buffer, 0, n);
                done += n;
                STATE.percent = 2 + (int) (33 * Math.min(done, size) / size);
            }
        }
        StringBuilder hex = new StringBuilder();
        for (byte b : sha1.digest()) {
            hex.append(String.format("%02x", b));
        }
        Discs.Disc disc = Discs.bySize(size);
        log.append("image ").append(image).append(", ").append(size).append(" bytes, SHA-1 ").append(hex).append('\n');
        if (!hex.toString().equals(disc.sha1)) {
            // The boot executable decides, below; a different image of the right disc may still run.
            log.append("the SHA-1 differs from the known ").append(disc.sha1).append('\n');
        }
        return image;
    }

    private Discs.Disc extract(File image) throws IOException, Failure {
        step("Checking your disc", 36, "");
        paths.data().mkdirs();
        List<String> command = new ArrayList<>(Arrays.asList(paths.pythonCommand()));
        command.addAll(Arrays.asList("tools/extractor.py", image.getPath(), "--game", "rac1", "--extract", "--validate",
            "--proj-path", paths.data().getPath(), "--boot-only"));
        int code = run(command, 36, 38, null);
        if (code != 0) {
            throw new Failure("This disc image was refused:\n\n" + lastLines(4) + "\n\n" + Discs.WHICH);
        }
        Discs.Disc disc = Discs.bySize(paths.keptDisc().length());
        if (disc == null) {
            throw new Failure(Discs.wrongSize(paths.keptDisc().length()));
        }
        return disc;
    }

    private void levels(Discs.Disc disc) throws IOException, Failure {
        step("Preparing the levels", 38, "A few minutes, once only.");
        File out = paths.levels(disc.version);
        // The tool says how far it is: "openrac-progress: read 3/21 7".
        Pattern level = Pattern.compile("openrac-progress: \\w+ (\\d+)/(\\d+)");
        List<String> command = new ArrayList<>(Arrays.asList(paths.pythonCommand()));
        command.addAll(Arrays.asList("editor/extract.py", "port", paths.keptDisc().getPath(), out.getPath(), "--replace",
            "--anywhere"));
        int code = run(command, 38, 94, level);
        if (code != 0) {
            throw new Failure("The levels could not be prepared:\n\n" + lastLines(6));
        }
    }

    private void check(Discs.Disc disc) throws IOException, Failure {
        step("Checking the levels", 95, "");
        // Its cards and settings named, so that the game never reaches for OpenRAC's own folder; no mods for a check.
        int code = run(Arrays.asList(paths.game(disc.version).getPath(), "--data", paths.data().getPath(),
            "--levels", paths.levels(disc.version).getPath(), "--cards", paths.cards(disc).getPath(),
            "--settings", paths.settingsFile().getPath(), "--no-mods", "--check-levels"), 95, 99, null);
        if (code != 0) {
            throw new Failure("The levels did not pass their check:\n\n" + lastLines(6));
        }
    }

    private final List<String> recent = new ArrayList<>();

    private String lastLines(int count) {
        int from = Math.max(0, recent.size() - count);
        return String.join("\n", recent.subList(from, recent.size()));
    }

    /** Runs a program in OpenRAC's copy, its output going to the log and, by level, the bar. */
    private int run(List<String> command, int fromPercent, int toPercent, Pattern progress) throws IOException {
        log.append("$ ").append(String.join(" ", command)).append('\n');
        ProcessBuilder builder = new ProcessBuilder(command);
        builder.directory(paths.root());
        builder.redirectErrorStream(true);
        builder.environment().clear();
        builder.environment().putAll(paths.environment());
        Process process = builder.start();
        current = process;
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(process.getInputStream(), StandardCharsets.UTF_8))) {
            for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                log.append(line).append('\n');
                recent.add(line);
                if (recent.size() > 200) {
                    recent.remove(0);
                }
                STATE.detail = line.length() > 90 ? line.substring(0, 90) : line;
                if (progress != null) {
                    Matcher matcher = progress.matcher(line);
                    if (matcher.find()) {
                        int done = Integer.parseInt(matcher.group(1));
                        int total = Math.max(1, Integer.parseInt(matcher.group(2)));
                        STATE.percent = fromPercent + (toPercent - fromPercent) * Math.min(done, total) / total;
                    }
                }
            }
        }
        try {
            int code = process.waitFor();
            log.append("exit ").append(code).append('\n');
            return code;
        } catch (InterruptedException e) {
            process.destroy();
            Thread.currentThread().interrupt();
            return -1;
        } finally {
            current = null;
        }
    }
}
