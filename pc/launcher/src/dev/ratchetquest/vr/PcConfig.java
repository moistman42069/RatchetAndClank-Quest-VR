// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * The player's choices on the PC, and writing them where the game reads them, as Config does on the headset: the same
 * options (Options), the VR mod's Windows build in the game's plugins folder with the OpenXR loader beside it.
 */
final class PcConfig extends Options {
    private final PcPaths paths;
    private final Prefs prefs;

    PcConfig(PcPaths paths) {
        this.paths = paths;
        prefs = new Prefs(paths.prefs("options"));
    }

    @Override
    String get(Choice choice) {
        // A value no longer offered (an older version's) is the default, not passed to the game.
        String value = prefs.getString(choice.key, choice.fallback);
        return Arrays.asList(choice.values).contains(value) ? value : choice.fallback;
    }

    @Override
    void set(Choice choice, String value) {
        prefs.putString(choice.key, value);
    }

    /** The mod in the game's plugins folder, and the list that turns it on with its settings, the bundled mods after. */
    void writeForGame(boolean bigScreen) throws IOException {
        File folder = new File(paths.plugins(), MOD_ID);
        File libraries = new File(folder, "lib/" + PcMods.PLATFORM);
        libraries.mkdirs();
        for (String name : new String[] {"quest_vr.dll", "openxr_loader.dll"}) {
            File from = new File(paths.vrMod(), "lib/" + PcMods.PLATFORM + "/" + name);
            if (!from.isFile()) {
                throw new IOException("this release has no " + from);
            }
            Files.copy(from.toPath(), new File(libraries, name).toPath(), StandardCopyOption.REPLACE_EXISTING);
        }
        Files.copy(new File(paths.vrMod(), "openrac-mod.json").toPath(), new File(folder, "openrac-mod.json").toPath(),
            StandardCopyOption.REPLACE_EXISTING);

        Map<String, Object> mod = new LinkedHashMap<>();
        mod.put("id", MOD_ID);
        mod.put("enabled", true);
        mod.put("settings", modSettings(bigScreen));
        List<Object> mods = new ArrayList<>();
        mods.add(mod);
        mods.addAll(new PcMods(paths).writeForGame());
        Map<String, Object> list = new LinkedHashMap<>();
        list.put("format", 1);
        list.put("mods", mods);
        Files.write(new File(paths.plugins(), "plugins.json").toPath(), Json.write(list).getBytes(StandardCharsets.UTF_8));

        Files.write(paths.settingsFile().toPath(), gameSettings().getBytes(StandardCharsets.UTF_8));
    }
}
