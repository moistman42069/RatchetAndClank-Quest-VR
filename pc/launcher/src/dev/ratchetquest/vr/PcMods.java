// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * OpenRAC's own mods the PC release carries beside the VR mod, as BundledMods is for the headset: the same mods and
 * the same rules, each built for Windows (tools/build_pc.py --package puts each packaged folder in mods/<id>). Mods
 * the player adds from a file are kept in added-mods. writeForGame puts the ones on in the game's plugins folder and
 * list, as OpenRAC's own launcher does (docs/port/MODS.md).
 */
final class PcMods {
    /** Those on unless the player turns them off: a convenience that changes no rule of the game. */
    private static final List<String> ON_BY_DEFAULT = Arrays.asList("ammo-max");
    /** The mod interface this game offers (sdk/include/openrac/openrac_mod.h): a mod for a newer minor is refused. */
    static final int API_MAJOR = 1, API_MINOR = 11;
    /** The build a native mod needs on this PC, its manifest's "plugin" key. */
    static final String PLATFORM = "windows-x86_64";
    private static final String MANIFEST = "openrac-mod.json";
    private static final long MOST_BYTES = 1L << 30;
    private static final int MOST_FILES = 20000;

    /** One setting of a manifest: bool, choice, int, float or string. */
    static final class Setting {
        final String key, name, type, help, fallback;
        /** bool and choice: what is shown and written; int and float: every step from min to max. */
        final String[] names, values;
        final int maxLength;

        Setting(String key, String name, String type, String help, String fallback, String[] names, String[] values,
                int maxLength) {
            this.key = key;
            this.name = name;
            this.type = type;
            this.help = help;
            this.fallback = fallback;
            this.names = names;
            this.values = values;
            this.maxLength = maxLength;
        }

        boolean stepped() { return type.equals("int") || type.equals("float"); }
    }

    /** A manifest's preset: some settings' values, the others back to their defaults. */
    static final class Preset {
        final String name;
        final Map<String, String> values;

        Preset(String name, Map<String, String> values) {
            this.name = name;
            this.values = values;
        }
    }

    static final class Mod {
        final String id, name, description;
        final List<Setting> settings;
        final List<Preset> presets;
        /** Its folder as it came: in the release's mods, or added-mods. */
        File folder;
        /** Added by the player from a file, not carried in the release. */
        boolean added;

        Mod(String id, String name, String description, List<Setting> settings, List<Preset> presets) {
            this.id = id;
            this.name = name;
            this.description = description;
            this.settings = settings;
            this.presets = presets;
        }
    }

    private final PcPaths paths;
    private final Prefs prefs;

    PcMods(PcPaths paths) {
        this.paths = paths;
        prefs = new Prefs(paths.prefs("mods"));
    }

    /** The release's mods in their folders' order, then those added; one whose manifest cannot be read is left out. */
    List<Mod> list() {
        List<Mod> mods = new ArrayList<>();
        for (boolean added : new boolean[] {false, true}) {
            File[] folders = (added ? paths.addedMods() : paths.bundledMods()).listFiles();
            if (folders == null) {
                continue;
            }
            Arrays.sort(folders);
            for (File folder : folders) {
                File manifest = new File(folder, MANIFEST);
                if (folder.getName().startsWith(".") || !manifest.isFile()) {
                    continue;
                }
                try {
                    Mod mod = parse(read(manifest));
                    mod.folder = folder;
                    mod.added = added;
                    mods.add(mod);
                } catch (IOException | IllegalArgumentException e) {
                    // Not offered: the game would refuse it too.
                }
            }
        }
        return mods;
    }

    /** Why a manifest cannot be added, in words for the player; null when it can. */
    static String refusal(Map<String, Object> manifest) {
        String id = Json.string(manifest, "id", "");
        if (!id.matches("[A-Za-z0-9][A-Za-z0-9._-]*") || id.equals("quest-vr")) {
            return "This file's manifest has no id the game can use.";
        }
        String name = Json.string(manifest, "name", id);
        String api = Json.string(manifest, "api", "");
        String[] parts = api.split("\\.");
        int major, minor;
        try {
            major = Integer.parseInt(parts[0]);
            minor = parts.length > 1 ? Integer.parseInt(parts[1]) : 0;
        } catch (NumberFormatException e) {
            return name + " does not say which mod interface it is for.";
        }
        if (major != API_MAJOR || minor > API_MINOR) {
            return name + " needs OpenRAC's mod interface " + api + "; this game has " + API_MAJOR + "." + API_MINOR + ".";
        }
        Map<String, Object> games = Json.object(manifest.get("games"));
        if (games != null && !games.containsKey("rac1")) {
            return name + " is for another game.";
        }
        Map<String, Object> plugin = Json.object(manifest.get("plugin"));
        if (plugin != null ? Json.string(plugin, PLATFORM, "").isEmpty()
                : Json.string(manifest, "type", "").equals("native")) {
            return name + " has no build for Windows (" + PLATFORM + ").";
        }
        return null;
    }

    /**
     * Adds a mod from its .zip: its manifest at the top or in one folder, checked before anything replaces an added
     * mod of the same id. Its name; an IOException's message says in the player's words why not.
     */
    String add(InputStream zip) throws IOException {
        File root = paths.addedMods();
        File incoming = new File(root, ".incoming");
        PcPaths.deleteRecursively(incoming);
        incoming.mkdirs();
        try {
            unpack(zip, incoming);
            File top = incoming;
            if (!new File(top, MANIFEST).isFile()) {
                File[] inside = top.listFiles();
                if (inside != null && inside.length == 1 && inside[0].isDirectory()) {
                    top = inside[0];
                }
            }
            File manifestFile = new File(top, MANIFEST);
            if (!manifestFile.isFile()) {
                throw new IOException("That file is not a mod: it has no " + MANIFEST + ".");
            }
            String text = read(manifestFile);
            Mod mod;
            Map<String, Object> manifest;
            try {
                manifest = Json.object(text);
                String refused = refusal(manifest);
                if (refused != null) {
                    throw new IOException(refused);
                }
                mod = parse(text);
            } catch (IllegalArgumentException e) {
                throw new IOException("That mod's manifest cannot be read: " + e.getMessage());
            }
            for (Mod other : list()) {
                if (other.id.equals(mod.id) && !other.added) {
                    throw new IOException(mod.name + " is already built in.");
                }
            }
            Map<String, Object> plugin = Json.object(manifest.get("plugin"));
            if (plugin != null) {
                File library = new File(top, Json.string(plugin, PLATFORM, ""));
                if (!inside(library, top) || !library.isFile()) {
                    throw new IOException(mod.name + "'s file has no build for Windows (" + PLATFORM + ").");
                }
            }
            File target = new File(root, mod.id);
            PcPaths.deleteRecursively(target);
            if (!top.renameTo(target)) {
                throw new IOException("Could not keep " + mod.name + " in the launcher's files.");
            }
            return mod.name;
        } finally {
            PcPaths.deleteRecursively(incoming);
        }
    }

    private static void unpack(InputStream zip, File to) throws IOException {
        long bytes = 0;
        int files = 0;
        byte[] buffer = new byte[65536];
        try (ZipInputStream in = new ZipInputStream(zip)) {
            for (ZipEntry entry = in.getNextEntry(); entry != null; entry = in.getNextEntry()) {
                File file = new File(to, entry.getName());
                if (!inside(file, to)) {
                    throw new IOException("That file is not a mod: a path in it leaves its folder.");
                }
                if (++files > MOST_FILES) {
                    throw new IOException("That file is too big for a mod.");
                }
                if (entry.isDirectory()) {
                    file.mkdirs();
                    continue;
                }
                file.getParentFile().mkdirs();
                try (OutputStream out = Files.newOutputStream(file.toPath())) {
                    for (int n = in.read(buffer); n > 0; n = in.read(buffer)) {
                        bytes += n;
                        if (bytes > MOST_BYTES) {
                            throw new IOException("That file is too big for a mod.");
                        }
                        out.write(buffer, 0, n);
                    }
                }
            }
        }
    }

    static boolean inside(File file, File folder) throws IOException {
        return file.getCanonicalPath().startsWith(folder.getCanonicalPath() + File.separator);
    }

    private static void copyAll(File from, File to) throws IOException {
        File[] inside = from.listFiles();
        if (inside != null) {
            to.mkdirs();
            for (File each : inside) {
                copyAll(each, new File(to, each.getName()));
            }
        } else {
            Files.copy(from.toPath(), to.toPath(), StandardCopyOption.REPLACE_EXISTING);
        }
    }

    /** Takes out a mod the player added, here and from the game's folder. */
    void remove(Mod mod) {
        if (mod.added) {
            PcPaths.deleteRecursively(new File(paths.addedMods(), mod.id));
            PcPaths.deleteRecursively(new File(paths.plugins(), mod.id));
        }
    }

    /** What the game said of a mod it did not run at its last start (plugins/status.json), or "" when it ran. */
    static String lastStart(File plugins, Mod mod) {
        File status = new File(plugins, "status.json");
        try {
            List<Object> mods = Json.array(Json.object(read(status)).get("mods"));
            for (int i = 0; mods != null && i < mods.size(); i++) {
                Map<String, Object> one = Json.object(mods.get(i));
                if (one != null && Json.string(one, "id", "").equals(mod.id)
                        && !Json.string(one, "state", "").equals("loaded")) {
                    String reason = Json.string(one, "reason", "");
                    return reason.isEmpty() ? Json.string(one, "state", "") : reason;
                }
            }
        } catch (IOException | IllegalArgumentException e) {
            // No start yet.
        }
        return "";
    }

    static Mod parse(String text) {
        Map<String, Object> manifest = Json.object(text);
        String id = Json.string(manifest, "id", "");
        if (id.isEmpty()) {
            throw new IllegalArgumentException("a manifest without an id");
        }
        List<Setting> settings = new ArrayList<>();
        List<Object> list = Json.array(manifest.get("settings"));
        for (int i = 0; list != null && i < list.size(); i++) {
            settings.add(setting(Json.object(list.get(i))));
        }
        List<Preset> presets = new ArrayList<>();
        List<Object> given = Json.array(manifest.get("presets"));
        for (int i = 0; given != null && i < given.size(); i++) {
            Map<String, Object> preset = Json.object(given.get(i));
            Map<String, Object> values = preset == null ? null : Json.object(preset.get("values"));
            if (values == null) {
                throw new IllegalArgumentException("a preset without values");
            }
            Map<String, String> map = new LinkedHashMap<>();
            for (Map.Entry<String, Object> entry : values.entrySet()) {
                map.put(entry.getKey(), Json.plain(entry.getValue()));
            }
            presets.add(new Preset(Json.string(preset, "name", Json.string(preset, "id", "")), map));
        }
        return new Mod(id, Json.string(manifest, "name", id), Json.string(manifest, "description", ""), settings,
            presets);
    }

    private static Setting setting(Map<String, Object> s) {
        if (s == null || !(s.get("key") instanceof String) || !(s.get("type") instanceof String)) {
            throw new IllegalArgumentException("a setting without a key or a type");
        }
        String key = (String) s.get("key");
        String type = (String) s.get("type");
        String name = Json.string(s, "name", key);
        String help = Json.string(s, "description", "");
        String fallback = Json.plain(s.get("default"));
        switch (type) {
            case "bool":
                return new Setting(key, name, type, help, fallback, new String[] {"On", "Off"},
                    new String[] {"true", "false"}, 0);
            case "choice": {
                List<Object> choices = Json.array(s.get("choices"));
                if (choices == null) {
                    throw new IllegalArgumentException("a choice without choices");
                }
                String[] names = new String[choices.size()], values = new String[choices.size()];
                for (int i = 0; i < choices.size(); i++) {
                    Map<String, Object> choice = Json.object(choices.get(i));
                    values[i] = Json.string(choice, "value", null);
                    if (values[i] == null) {
                        throw new IllegalArgumentException("a choice without a value");
                    }
                    names[i] = Json.string(choice, "name", values[i]);
                }
                return new Setting(key, name, type, help, fallback, names, values, 0);
            }
            case "int":
            case "float": {
                BigDecimal min = new BigDecimal(Json.plain(s.get("min")));
                BigDecimal max = new BigDecimal(Json.plain(s.get("max")));
                Object stepGiven = s.get("step");
                BigDecimal step = new BigDecimal(type.equals("int") && stepGiven == null ? "1" : Json.plain(stepGiven));
                if (step.signum() <= 0) {
                    throw new IllegalArgumentException("a step that is not above 0");
                }
                List<String> steps = new ArrayList<>();
                for (BigDecimal v = min; v.compareTo(max) <= 0 && steps.size() < 1000; v = v.add(step)) {
                    steps.add(v.stripTrailingZeros().toPlainString());
                }
                String[] values = steps.toArray(new String[0]);
                // The default as one of the steps' spellings ("1" for 1.0).
                String spelt = new BigDecimal(fallback).stripTrailingZeros().toPlainString();
                return new Setting(key, name, type, help, spelt, values, values, 0);
            }
            case "string": {
                Object most = s.get("max_length");
                int maxLength = most instanceof BigDecimal ? ((BigDecimal) most).intValue() : 64;
                return new Setting(key, name, type, help, fallback, new String[0], new String[0], maxLength);
            }
            default:
                throw new IllegalArgumentException("a setting of type " + type);
        }
    }

    boolean on(Mod mod) {
        return prefs.getBoolean(mod.id + ".on", mod.added || ON_BY_DEFAULT.contains(mod.id));
    }

    void setOn(Mod mod, boolean on) {
        prefs.putBoolean(mod.id + ".on", on);
    }

    /** A setting's value: the stored one if the manifest still offers it, else its default. */
    String get(Mod mod, Setting s) {
        String value = prefs.getString(mod.id + "." + s.key, s.fallback);
        if (s.type.equals("string")) {
            return value.length() > s.maxLength ? value.substring(0, s.maxLength) : value;
        }
        if (s.stepped()) {
            try {
                value = new BigDecimal(value).stripTrailingZeros().toPlainString();
            } catch (NumberFormatException e) {
                return s.fallback;
            }
        }
        return Arrays.asList(s.values).contains(value) ? value : s.fallback;
    }

    void set(Mod mod, Setting s, String value) {
        prefs.putString(mod.id + "." + s.key, value);
    }

    /** A preset's values, the settings it does not name back to their defaults. */
    void apply(Mod mod, Preset preset) {
        for (Setting s : mod.settings) {
            String value = preset.values.containsKey(s.key) ? preset.values.get(s.key) : s.fallback;
            if (s.stepped()) {
                value = new BigDecimal(value).stripTrailingZeros().toPlainString();
            }
            prefs.putString(mod.id + "." + s.key, value);
        }
    }

    /** The settings as the game reads them: each in its manifest's type. */
    Map<String, Object> settings(Mod mod) {
        Map<String, Object> out = new LinkedHashMap<>();
        for (Setting s : mod.settings) {
            String value = get(mod, s);
            switch (s.type) {
                case "bool": out.put(s.key, Boolean.parseBoolean(value)); break;
                case "int": out.put(s.key, Integer.parseInt(value)); break;
                case "float": out.put(s.key, Double.parseDouble(value)); break;
                default: out.put(s.key, value); break;
            }
        }
        return out;
    }

    /**
     * The ones on, put in the game's plugins folder as they came (manifest, library, scripts), and their entries for
     * its list; those off are left out of the list.
     */
    List<Map<String, Object>> writeForGame() throws IOException {
        List<Map<String, Object>> entries = new ArrayList<>();
        for (Mod mod : list()) {
            if (!on(mod)) {
                continue;
            }
            File folder = new File(paths.plugins(), mod.id);
            PcPaths.deleteRecursively(folder);
            copyAll(mod.folder, folder);
            Map<String, Object> entry = new LinkedHashMap<>();
            entry.put("id", mod.id);
            entry.put("enabled", true);
            entry.put("settings", settings(mod));
            entries.add(entry);
        }
        return entries;
    }

    static String read(File file) throws IOException {
        return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
    }
}
