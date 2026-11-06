// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.Properties;

/** A file of choices, what SharedPreferences is to the headset's launcher: each change written at once, whole. */
final class Prefs {
    private final File file;
    private final Properties values = new Properties();

    Prefs(File file) {
        this.file = file;
        if (file.isFile()) {
            try (InputStream in = Files.newInputStream(file.toPath())) {
                values.load(in);
            } catch (IOException | IllegalArgumentException e) {
                // A file that cannot be read is no choices: the defaults.
                values.clear();
            }
        }
    }

    synchronized String getString(String key, String fallback) {
        return values.getProperty(key, fallback);
    }

    synchronized boolean getBoolean(String key, boolean fallback) {
        String value = values.getProperty(key);
        return value == null ? fallback : Boolean.parseBoolean(value);
    }

    synchronized long getLong(String key, long fallback) {
        try {
            String value = values.getProperty(key);
            return value == null ? fallback : Long.parseLong(value);
        } catch (NumberFormatException e) {
            return fallback;
        }
    }

    synchronized Prefs putString(String key, String value) {
        values.setProperty(key, value);
        save();
        return this;
    }

    Prefs putBoolean(String key, boolean value) {
        return putString(key, Boolean.toString(value));
    }

    Prefs putLong(String key, long value) {
        return putString(key, Long.toString(value));
    }

    /** Written beside, then moved over the old file: never half a file. */
    private void save() {
        File folder = file.getAbsoluteFile().getParentFile();
        folder.mkdirs();
        File part = new File(folder, file.getName() + ".part");
        try {
            try (OutputStream out = Files.newOutputStream(part.toPath())) {
                values.store(out, "Ratchet & Clank VR (PC)");
            }
            try {
                Files.move(part.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING,
                    StandardCopyOption.ATOMIC_MOVE);
            } catch (IOException e) {
                Files.move(part.toPath(), file.toPath(), StandardCopyOption.REPLACE_EXISTING);
            }
        } catch (IOException e) {
            System.err.println("cannot keep " + file + ": " + e);
        }
    }
}
