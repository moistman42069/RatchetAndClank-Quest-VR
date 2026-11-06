// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.Map;

/**
 * The player's choices, and writing them where the game reads them: the mod's plugins.json and the
 * port's settings file. The options themselves, their words and what each writes, are Options, which
 * the PC's launcher is built from too; here is where the headset keeps them.
 */
final class Config extends Options {
    private static final String KEY_FAR_SCENERY = "far_scenery_moved";

    private final Context context;
    private final SharedPreferences prefs;
    private final Tools tools;

    Config(Context context) {
        this.context = context.getApplicationContext();
        prefs = context.getSharedPreferences("options", Context.MODE_PRIVATE);
        tools = new Tools(context);
        drawFarSceneryOnce();
    }

    @Override
    String get(Choice choice) {
        // A value no longer offered (an older version's) is the default, not passed to the game.
        String value = prefs.getString(choice.key, choice.fallback);
        return java.util.Arrays.asList(choice.values).contains(value) ? value : choice.fallback;
    }

    @Override
    void set(Choice choice, String value) {
        prefs.edit().putString(choice.key, value).apply();
    }

    /**
     * Before 0.1.5 On was the default and Balanced set it, which left far scenery out. Once, on the first start after
     * that update, it goes back to Off, unless the player chose the Performance preset, which still asks for it.
     */
    private void drawFarSceneryOnce() {
        if (prefs.getBoolean(KEY_FAR_SCENERY, false)) {
            return;
        }
        SharedPreferences.Editor edit = prefs.edit().putBoolean(KEY_FAR_SCENERY, true);
        if (!"performance".equals(get(PRESET))) {
            edit.putString(ORIGINAL_LOD.key, "false");
        }
        edit.apply();
    }

    /** The mod in the game's plugins folder, and the list that turns it on with its settings. */
    void writeForGame(boolean bigScreen) throws IOException {
        File folder = new File(tools.plugins(), MOD_ID);
        File library = new File(folder, "lib/android-arm64/quest_vr.so");
        library.getParentFile().mkdirs();
        // The port makes it read-only, as Android asks of code an app loads from its own files.
        library.delete();
        Files.copy(new File(tools.nativeLibraries(), "libquest_vr.so").toPath(), library.toPath(),
            StandardCopyOption.REPLACE_EXISTING);
        try (InputStream manifest = context.getAssets().open("openrac-mod.json");
                FileOutputStream out = new FileOutputStream(new File(folder, "openrac-mod.json"))) {
            byte[] buffer = new byte[8192];
            for (int n = manifest.read(buffer); n > 0; n = manifest.read(buffer)) {
                out.write(buffer, 0, n);
            }
        }
        try {
            JSONObject settings = new JSONObject();
            for (Map.Entry<String, Object> setting : modSettings(bigScreen).entrySet()) {
                settings.put(setting.getKey(), setting.getValue());
            }
            JSONObject mod = new JSONObject();
            mod.put("id", MOD_ID);
            mod.put("enabled", true);
            mod.put("settings", settings);
            JSONArray mods = new JSONArray().put(mod);
            for (JSONObject bundled : new BundledMods(context).writeForGame(tools.plugins(), tools.nativeLibraries())) {
                mods.put(bundled);
            }
            JSONObject list = new JSONObject();
            list.put("format", 1);
            list.put("mods", mods);
            Files.write(new File(tools.plugins(), "plugins.json").toPath(),
                list.toString(2).getBytes(StandardCharsets.UTF_8));
        } catch (JSONException e) {
            throw new IOException(e);
        }

        Files.write(tools.settingsFile().toPath(), gameSettings().getBytes(StandardCharsets.UTF_8));
    }
}
