// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Setting the game up from the player's disc, in the order OpenRAC's launcher does on Android
 * (docs/port/ANDROID.md): the image copied into the app, tools/extractor.py, editor/extract.py port
 * for the levels, then the game's own --check-levels. Runs on SetupService's thread; the launcher
 * reads State.
 */
final class Setup {
    private static final String TAG = "RatchetQuest";
    private static final long NEEDED_BEYOND_IMAGE = 2_000_000_000L;
    static final String PREFS = "setup";
    static final String KEY_VERSION = "version";

    /** How far it is; written here, read by the launcher's page. */
    static final class State {
        volatile boolean starting;  // asked for, and the service not there yet
        volatile boolean running;
        volatile int percent;
        volatile String step = "";
        volatile String detail = "";
        volatile String failure;
        volatile boolean done;
    }

    static final State STATE = new State();

    private final Context context;
    private final Tools tools;
    private final StringBuilder log = new StringBuilder();

    Setup(Context context) {
        this.context = context.getApplicationContext();
        this.tools = new Tools(context);
    }

    /** What the game's level check leaves beside levels it passed in the format this game reads. */
    static String checkStamp() {
        return "verified-" + BuildConfig.LEVEL_FORMAT + ".json";
    }

    /** The disc version a finished set-up was made from, or null. */
    static String installedVersion(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String version = prefs.getString(KEY_VERSION, null);
        if (version == null) {
            return null;
        }
        // Another format's stamp is levels an older or newer game made: this one cannot read them.
        return new File(new Tools(context).levels(version), checkStamp()).isFile() ? version : null;
    }

    /**
     * The disc version of a set-up whose levels this game cannot read (an update changed their
     * format) while the disc it was made from is still here, or null: the levels are made again
     * from that disc, as OpenRAC's launcher does, without choosing it again.
     */
    static String outdatedVersion(Context context) {
        String version = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString(KEY_VERSION, null);
        if (version == null || installedVersion(context) != null) {
            return null;
        }
        File kept = new Tools(context).keptDisc();
        Discs.Disc disc = kept.isFile() ? Discs.bySize(kept.length()) : null;
        return disc != null && disc.version.equals(version) ? version : null;
    }

    /** Sets the game up from `image`, or with none makes the levels again from the disc kept. */
    void run(Uri image) {
        STATE.failure = null;
        STATE.done = false;
        STATE.running = true;
        try {
            step("Preparing tools", 1, "");
            tools.prepare();
            Discs.Disc disc;
            if (image != null) {
                disc = extract(copyImage(image));
            } else {
                disc = Discs.bySize(tools.keptDisc().length());
                if (!tools.keptDisc().isFile() || disc == null) {
                    throw new Failure("The disc copy on the headset is gone: choose your .iso again.");
                }
                log.append("levels made again from the disc kept, for level format ")
                    .append(BuildConfig.LEVEL_FORMAT).append('\n');
            }
            levels(disc);
            check(disc);
            context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
                .putString(KEY_VERSION, disc.version).apply();
            step("Ready", 100, disc.region + " disc set up");
            STATE.done = true;
        } catch (Failure failure) {
            Log.e(TAG, "set-up failed: " + failure.getMessage());
            STATE.failure = failure.getMessage();
        } catch (IOException | RuntimeException e) {
            Log.e(TAG, "set-up failed", e);
            STATE.failure = "Set-up stopped: " + e.getMessage();
        } finally {
            RunLog.saveSetup(context, log.toString());
            STATE.running = false;
        }
    }

    private static final class Failure extends Exception {
        Failure(String message) { super(message); }
    }

    private void step(String step, int percent, String detail) {
        STATE.step = step;
        STATE.percent = percent;
        STATE.detail = detail;
        log.append("== ").append(step).append(' ').append(detail).append('\n');
    }

    private long sizeOf(Uri image) {
        try (Cursor cursor = context.getContentResolver().query(image, new String[] {OpenableColumns.SIZE},
                null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) {
                return cursor.getLong(0);
            }
        } catch (RuntimeException e) {
            Log.w(TAG, "the picker gave no size: " + e);
        }
        return -1;
    }

    /** The chosen image in the app's own storage: the game reads the disc for as long as it runs. */
    private File copyImage(Uri image) throws IOException, Failure {
        long size = sizeOf(image);
        if (size >= 0 && Discs.bySize(size) == null) {
            throw new Failure(Discs.wrongSize(size));
        }
        tools.discs().mkdirs();
        long free = tools.discs().getUsableSpace();
        long needed = (size > 0 ? size : Discs.PAL.size) + NEEDED_BEYOND_IMAGE;
        if (free < needed) {
            throw new Failure("Not enough free space: this needs about "
                + needed / 1_000_000_000L + " GB and the headset has "
                + free / 1_000_000_000L + " GB free.");
        }
        File dest = new File(tools.discs(), "disc.iso");
        File partial = new File(tools.discs(), "disc.iso.part");
        step("Copying your disc", 2, "This takes a few minutes");
        MessageDigest sha1;
        try {
            sha1 = MessageDigest.getInstance("SHA-1");
        } catch (java.security.NoSuchAlgorithmException e) {
            throw new IOException(e);
        }
        long copied = 0;
        try (InputStream in = context.getContentResolver().openInputStream(image);
                FileOutputStream out = new FileOutputStream(partial)) {
            if (in == null) {
                throw new Failure("The chosen file could not be opened.");
            }
            byte[] buffer = new byte[1 << 20];
            long total = size > 0 ? size : Discs.PAL.size;
            for (int n = in.read(buffer); n > 0; n = in.read(buffer)) {
                out.write(buffer, 0, n);
                sha1.update(buffer, 0, n);
                copied += n;
                STATE.percent = 2 + (int) (33 * Math.min(copied, total) / total);
            }
        }
        if (Discs.bySize(copied) == null) {
            partial.delete();
            throw new Failure(Discs.wrongSize(copied));
        }
        StringBuilder hex = new StringBuilder();
        for (byte b : sha1.digest()) {
            hex.append(String.format("%02x", b));
        }
        Discs.Disc disc = Discs.bySize(copied);
        log.append("image ").append(copied).append(" bytes, SHA-1 ").append(hex).append('\n');
        if (!hex.toString().equals(disc.sha1)) {
            // The boot executable decides, below; a different image of the right disc may still run.
            log.append("the SHA-1 differs from the known ").append(disc.sha1).append('\n');
        }
        dest.delete();
        if (!partial.renameTo(dest)) {
            throw new IOException("cannot move the copied disc into place");
        }
        return dest;
    }

    private Discs.Disc extract(File image) throws IOException, Failure {
        step("Checking your disc", 36, "");
        tools.data().mkdirs();
        int code = run(Arrays.asList(tools.python().getPath(), "tools/extractor.py", image.getPath(),
            "--game", "rac1", "--extract", "--validate", "--proj-path", tools.data().getPath(), "--boot-only"),
            36, 38, null);
        if (code != 0) {
            image.delete();
            throw new Failure("This disc image was refused:\n\n" + lastLines(4) + "\n\n" + Discs.REQUIREMENT);
        }
        // The extractor keeps the image beside what it wrote (a link where it can, else a copy):
        // the chosen copy is then not needed twice.
        if (tools.keptDisc().isFile() && tools.keptDisc().length() == image.length()
                && !tools.keptDisc().getCanonicalPath().equals(image.getCanonicalPath())) {
            image.delete();
        }
        Discs.Disc disc = Discs.bySize(tools.keptDisc().length());
        if (disc == null) {
            throw new Failure(Discs.wrongSize(tools.keptDisc().length()));
        }
        return disc;
    }

    private void levels(Discs.Disc disc) throws IOException, Failure {
        step("Preparing the levels", 38, "Up to half an hour, once only. The headset can stay on charge.");
        File out = tools.levels(disc.version);
        // The tool says how far it is: "openrac-progress: read 3/21 7".
        Pattern level = Pattern.compile("openrac-progress: \\w+ (\\d+)/(\\d+)");
        int code = run(Arrays.asList(tools.python().getPath(), "editor/extract.py", "port",
            tools.keptDisc().getPath(), out.getPath(), "--replace", "--anywhere", "--jobs", "4"),
            38, 94, level);
        if (code != 0) {
            throw new Failure("The levels could not be prepared:\n\n" + lastLines(6));
        }
    }

    private void check(Discs.Disc disc) throws IOException, Failure {
        step("Checking the levels", 95, "");
        int code = run(Arrays.asList(tools.windowless("rac1-" + disc.version).getPath(),
            "--data", tools.data().getPath(), "--levels", tools.levels(disc.version).getPath(), "--check-levels"),
            95, 99, null);
        if (code != 0) {
            throw new Failure("The levels did not pass their check:\n\n" + lastLines(6));
        }
    }

    private final List<String> recent = new ArrayList<>();

    private String lastLines(int count) {
        int from = Math.max(0, recent.size() - count);
        return String.join("\n", recent.subList(from, recent.size()));
    }

    /** Runs a program from the tools' folder, its output going to the log and, by level, the bar. */
    private int run(List<String> command, int fromPercent, int toPercent, Pattern progress)
            throws IOException {
        log.append("$ ").append(String.join(" ", command)).append('\n');
        ProcessBuilder builder = new ProcessBuilder(command);
        builder.directory(tools.root());
        builder.redirectErrorStream(true);
        Map<String, String> env = builder.environment();
        env.putAll(tools.environment());
        Process process = builder.start();
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
        }
    }
}
