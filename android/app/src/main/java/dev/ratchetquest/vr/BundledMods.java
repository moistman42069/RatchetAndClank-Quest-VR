// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * OpenRAC's own mods the APK carries beside the VR mod (github.com/OpenRAC/openrac-mods; tools/build_quest.py
 * BUNDLED_MODS): each one's packaged manifest in assets/mods/<id>, a script mod's files beside it, a native mod's
 * library in the APK's native libraries. Mods the player adds from a file are kept in the app's added-mods folder. The
 * launcher's Game tab turns each on and sets what its manifest offers; writeForGame puts the ones on in the game's
 * plugins folder and list, as OpenRAC's own launcher does (docs/port/MODS.md).
 */
final class BundledMods {
    /** Those on unless the player turns them off: a convenience that changes no rule of the game. */
    // Xbox button icons from 0.1.19 (the owner: Xbox style buttons the normal; the VR mod points its icons at the buttons
    // that do each thing in first person, vr/src/glyphs.h). A player who turned one off keeps it off.
    private static final List<String> ON_BY_DEFAULT = Arrays.asList("ammo-max", "xbox-icons");
    /** The mod interface this game offers (sdk/include/openrac/openrac_mod.h): a mod for a newer minor is refused. */
    static final int API_MAJOR = 1, API_MINOR = 14;
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

    /** A manifest's preset: some settings' values, the others it covers back to their defaults (settings.md). */
    static final class Preset {
        final String id, name;
        final Map<String, String> values;

        Preset(String id, String name, Map<String, String> values) {
            this.id = id;
            this.name = name;
            this.values = values;
        }
    }

    static final class Mod {
        final String id, name, description, library, version;
        final List<Setting> settings;
        final List<Preset> presets;
        /** Added by the player from a file, not carried in the APK. */
        boolean added;
        /** An added mod in a built-in's place (the list's version, ModIndex.differing): that built-in's version; else null. */
        String builtIn;

        Mod(String id, String name, String description, String library, String version, List<Setting> settings,
            List<Preset> presets) {
            this.id = id;
            this.name = name;
            this.description = description;
            this.library = library;
            this.version = version;
            this.settings = settings;
            this.presets = presets;
        }
    }

    private final Context context;
    private final SharedPreferences prefs;

    BundledMods(Context context) {
        this.context = context.getApplicationContext();
        prefs = context.getSharedPreferences("mods", Context.MODE_PRIVATE);
    }

    /** The mods in assets/mods, in their folders' order; one whose manifest cannot be read is left out. */
    List<Mod> list() {
        List<Mod> mods = new ArrayList<>();
        String[] ids;
        try {
            ids = context.getAssets().list("mods");
        } catch (IOException e) {
            return mods;
        }
        if (ids == null) {
            return mods;
        }
        Arrays.sort(ids);
        for (String id : ids) {
            try (InputStream in = context.getAssets().open("mods/" + id + "/" + MANIFEST)) {
                mods.add(parse(read(in)));
            } catch (IOException | JSONException | IllegalArgumentException e) {
                // Not offered: the game would refuse it too.
            }
        }
        File[] added = addedRoot().listFiles();
        if (added != null) {
            Arrays.sort(added);
            for (File folder : added) {
                File manifest = new File(folder, MANIFEST);
                if (folder.getName().startsWith(".") || !manifest.isFile()) {
                    continue;
                }
                try (InputStream in = Files.newInputStream(manifest.toPath())) {
                    Mod mod = parse(read(in));
                    mod.added = true;
                    place(mods, mod);
                } catch (IOException | JSONException | IllegalArgumentException e) {
                    // Not offered: the game would refuse it too.
                }
            }
        }
        return mods;
    }

    /**
     * An added mod into the list: one of a built-in's id takes its place, where it is in the list (Mod.builtIn: the
     * built-in comes back when it is removed); one RAC-Board's changes keep (ModIndex.keepsBuiltIn) is not added, and
     * not used here; any other goes at the end.
     */
    static void place(List<Mod> mods, Mod mod) {
        int at = builtInAt(mods, mod.id);
        if (at < 0) {
            mods.add(mod);
        } else if (!ModIndex.keepsBuiltIn(mod.id)) {
            mod.builtIn = mods.get(at).version;
            mods.set(at, mod);
        }
    }

    private static int builtInAt(List<Mod> mods, String id) {
        for (int i = 0; i < mods.size(); i++) {
            if (!mods.get(i).added && mods.get(i).id.equals(id)) {
                return i;
            }
        }
        return -1;
    }

    private File addedRoot() {
        return new File(context.getFilesDir(), "added-mods");
    }

    /** Why a manifest cannot be added, in words for the player; null when it can. */
    static String refusal(JSONObject manifest) {
        String id = manifest.optString("id");
        if (!id.matches("[A-Za-z0-9][A-Za-z0-9._-]*") || id.equals("quest-vr")) {
            return "This file's manifest has no id the game can use.";
        }
        String name = manifest.optString("name", id);
        String[] api = manifest.optString("api").split("\\.");
        int major = -1, minor = -1;
        try {
            major = Integer.parseInt(api[0]);
            minor = api.length > 1 ? Integer.parseInt(api[1]) : 0;
        } catch (NumberFormatException e) {
            return name + " does not say which mod interface it is for.";
        }
        if (major != API_MAJOR || minor > API_MINOR) {
            return name + " needs OpenRAC's mod interface " + manifest.optString("api") + "; this game has " + API_MAJOR
                + "." + API_MINOR + ".";
        }
        JSONObject games = manifest.optJSONObject("games");
        if (games != null && !games.has("rac1")) {
            return name + " is for another game.";
        }
        JSONObject plugin = manifest.optJSONObject("plugin");
        if (plugin != null ? plugin.optString("android-arm64").isEmpty() : manifest.optString("type").equals("native")) {
            return name + " has no build for the headset (android-arm64).";
        }
        return null;
    }

    /**
     * Adds a mod from its .zip: its manifest at the top or in one folder, checked before anything replaces an added
     * mod of the same id. Its name; an IOException's message says in the player's words why not.
     */
    String add(InputStream zip) throws IOException {
        File root = addedRoot();
        File incoming = new File(root, ".incoming");
        deleteAll(incoming);
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
            String text = new String(Files.readAllBytes(manifestFile.toPath()), StandardCharsets.UTF_8);
            Mod mod;
            JSONObject manifest;
            try {
                manifest = new JSONObject(text);
                String refused = refusal(manifest);
                if (refused != null) {
                    throw new IOException(refused);
                }
                mod = parse(text);
            } catch (JSONException | IllegalArgumentException e) {
                throw new IOException("That mod's manifest cannot be read: " + e.getMessage());
            }
            for (Mod other : list()) {
                if (other.id.equals(mod.id) && !other.added && ModIndex.keepsBuiltIn(mod.id)) {
                    throw new IOException(mod.name + " is built in with this release's own changes: it stays as it is.");
                }
            }
            JSONObject plugin = manifest.optJSONObject("plugin");
            if (plugin != null) {
                File library = new File(top, plugin.optString("android-arm64"));
                if (!inside(library, top) || !library.isFile()) {
                    throw new IOException(mod.name + "'s file has no build for the headset (android-arm64).");
                }
            }
            File target = new File(root, mod.id);
            deleteAll(target);
            if (!top.renameTo(target)) {
                throw new IOException("Could not keep " + mod.name + " in the app's files.");
            }
            return mod.name;
        } finally {
            deleteAll(incoming);
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

    private static boolean inside(File file, File folder) throws IOException {
        return file.getCanonicalPath().startsWith(folder.getCanonicalPath() + File.separator);
    }

    private static void deleteAll(File file) {
        File[] inside = file.listFiles();
        if (inside != null) {
            for (File each : inside) {
                deleteAll(each);
            }
        }
        file.delete();
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
    void remove(Mod mod, File plugins) {
        if (mod.added) {
            deleteAll(new File(addedRoot(), mod.id));
            deleteAll(new File(plugins, mod.id));
        }
    }

    /** What the game said of a mod it did not run at its last start (plugins/status.json), or "" when it ran. */
    static String lastStart(File plugins, Mod mod) {
        File status = new File(plugins, "status.json");
        try (InputStream in = Files.newInputStream(status.toPath())) {
            JSONArray mods = new JSONObject(read(in)).optJSONArray("mods");
            for (int i = 0; mods != null && i < mods.length(); i++) {
                JSONObject one = mods.getJSONObject(i);
                if (one.optString("id").equals(mod.id) && !one.optString("state").equals("loaded")) {
                    String reason = one.optString("reason");
                    return reason.isEmpty() ? one.optString("state") : reason;
                }
            }
        } catch (IOException | JSONException e) {
            // No start yet.
        }
        return "";
    }

    static Mod parse(String text) throws JSONException {
        JSONObject manifest = new JSONObject(text);
        String id = manifest.getString("id");
        // A script mod or a data pack has no library.
        JSONObject plugins = manifest.optJSONObject("plugin");
        String plugin = plugins != null ? plugins.getString("android-arm64") : "";
        List<Setting> settings = new ArrayList<>();
        JSONArray list = manifest.optJSONArray("settings");
        for (int i = 0; list != null && i < list.length(); i++) {
            Setting s = setting(list.getJSONObject(i));
            if (s != null) {
                settings.add(s);
            }
        }
        List<Preset> presets = new ArrayList<>();
        JSONArray given = manifest.optJSONArray("presets");
        for (int i = 0; given != null && i < given.length(); i++) {
            JSONObject preset = given.getJSONObject(i);
            JSONObject values = preset.getJSONObject("values");
            Map<String, String> map = new LinkedHashMap<>();
            for (java.util.Iterator<String> keys = values.keys(); keys.hasNext();) {
                String key = keys.next();
                map.put(key, plain(values.get(key)));
            }
            presets.add(new Preset(preset.optString("id"), preset.optString("name", preset.optString("id")), map));
        }
        return new Mod(id, manifest.optString("name", id), manifest.optString("description"),
            plugin.substring(plugin.lastIndexOf('/') + 1), manifest.optString("version"), settings, presets);
    }

    /** A setting of the manifest; null for one only other games show (settings.md: the mod gets its default here). */
    private static Setting setting(JSONObject s) throws JSONException {
        JSONArray games = s.optJSONArray("games");
        if (games != null) {
            boolean here = false;
            for (int i = 0; i < games.length(); i++) {
                here |= "rac1".equals(games.optString(i));
            }
            if (!here) {
                return null;
            }
        }
        String key = s.getString("key");
        String type = s.getString("type");
        String name = s.optString("name", key);
        String help = s.optString("description");
        String fallback = plain(s.opt("default"));
        switch (type) {
            case "bool":
                return new Setting(key, name, type, help, fallback, new String[] {"On", "Off"},
                    new String[] {"true", "false"}, 0);
            case "choice": {
                JSONArray choices = s.getJSONArray("choices");
                String[] names = new String[choices.length()], values = new String[choices.length()];
                for (int i = 0; i < choices.length(); i++) {
                    values[i] = choices.getJSONObject(i).getString("value");
                    names[i] = choices.getJSONObject(i).optString("name", values[i]);
                }
                return new Setting(key, name, type, help, fallback, names, values, 0);
            }
            case "int":
            case "float": {
                BigDecimal min = new BigDecimal(plain(s.get("min")));
                BigDecimal max = new BigDecimal(plain(s.get("max")));
                BigDecimal step = new BigDecimal(type.equals("int") ? plain(s.opt("step") == null ? 1 : s.get("step"))
                                                                    : plain(s.get("step")));
                List<String> steps = new ArrayList<>();
                for (BigDecimal v = min; v.compareTo(max) <= 0 && steps.size() < 1000; v = v.add(step)) {
                    steps.add(v.stripTrailingZeros().toPlainString());
                }
                String[] values = steps.toArray(new String[0]);
                // The default as one of the steps' spellings ("1" for 1.0).
                String spelt = new BigDecimal(fallback).stripTrailingZeros().toPlainString();
                return new Setting(key, name, type, help, spelt, values, values, 0);
            }
            case "string":
                return new Setting(key, name, type, help, fallback, new String[0], new String[0], s.optInt("max_length", 64));
            case "keybind":
                // Keys and mouse buttons by name, comma-separated (settings.md: "KeyW,ArrowUp"), "" for none: a field.
                return new Setting(key, name, type, help, fallback, new String[0], new String[0], 64);
            default:
                throw new IllegalArgumentException("a setting of type " + type);
        }
    }

    private static String plain(Object value) {
        if (value instanceof Double || value instanceof Float) {
            return new BigDecimal(value.toString()).stripTrailingZeros().toPlainString();
        }
        return value == null || value == JSONObject.NULL ? "" : value.toString();
    }

    boolean on(Mod mod) {
        // An added mod is on from its adding; one in a built-in's place keeps the built-in's on or off.
        return prefs.getBoolean(mod.id + ".on", (mod.added && mod.builtIn == null) || ON_BY_DEFAULT.contains(mod.id));
    }

    void setOn(Mod mod, boolean on) {
        prefs.edit().putBoolean(mod.id + ".on", on).apply();
    }

    /** A setting's value: the stored one if the manifest still offers it, else its default. */
    String get(Mod mod, Setting s) {
        String value = prefs.getString(mod.id + "." + s.key, s.fallback);
        if (s.type.equals("string") || s.type.equals("keybind")) {
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
        // The Multiplayer name typed back as it is shown stays the placeholder, so it follows the name (OpenRAC's
        // set_setting).
        if (named(s) && value.equals(playerName())) {
            value = PlayerName.PLACEHOLDER;
        }
        prefs.edit().putString(mod.id + "." + s.key, value).apply();
    }

    // ---- The Multiplayer name and key (PlayerName), as OpenRAC's launcher keeps them ----

    private static final String NAME = "#player_name", KEY = "#player_key";

    /** A string setting whose default is the player's Multiplayer name. */
    static boolean named(Setting s) {
        return s.type.equals("string") && s.fallback.equals(PlayerName.PLACEHOLDER);
    }

    /** The player's Multiplayer name: a random one, kept, until they give their own. */
    String playerName() {
        String name = prefs.getString(NAME, "");
        if (!PlayerName.valid(name)) {
            name = PlayerName.random();
            prefs.edit().putString(NAME, name).apply();
        }
        return name;
    }

    /** Gives the player's own name; why not, in their words, or null when it is theirs now. */
    String setPlayerName(String name) {
        String why = PlayerName.refusal(name);
        if (why == null) {
            prefs.edit().putString(NAME, name).apply();
        }
        return why;
    }

    /** This installation's key, made once. */
    String playerKey() {
        String key = prefs.getString(KEY, "");
        if (!PlayerName.validKey(key)) {
            key = PlayerName.newKey();
            prefs.edit().putString(KEY, key).apply();
        }
        return key;
    }

    /** Whether a mod that is on uses the Multiplayer name (the bundled Multiplayer does): then the launcher shows it. */
    boolean usesPlayerName() {
        for (Mod mod : list()) {
            if (!on(mod)) {
                continue;
            }
            for (Setting s : mod.settings) {
                if (named(s)) {
                    return true;
                }
            }
        }
        return false;
    }

    /** A setting's value as the launcher shows it: the Multiplayer name in place of a placeholder still unset. */
    String shown(Mod mod, Setting s) {
        String value = get(mod, s);
        return named(s) && value.equals(PlayerName.PLACEHOLDER) ? playerName() : value;
    }

    /** The Default button's value, and each preset's (presetValue). */
    static final String DEFAULT = "#default";

    static String presetValue(int index) {
        return "#" + index;
    }

    /** A preset with the id `default` takes the place of the Default button (settings.md). */
    static boolean ownDefault(Mod mod) {
        for (Preset p : mod.presets) {
            if (p.id.equals("default")) {
                return true;
            }
        }
        return false;
    }

    /** What a preset covers (settings.md): every keybind, and each setting one of the mod's presets names. */
    static boolean covered(Mod mod, Setting s) {
        if (s.type.equals("keybind")) {
            return true;
        }
        for (Preset p : mod.presets) {
            if (p.values.containsKey(s.key)) {
                return true;
            }
        }
        return false;
    }

    /** The value `preset` (null: the defaults) gives `s`, as get spells it. */
    private static String presetGives(Preset preset, Setting s) {
        String value = preset != null && preset.values.containsKey(s.key) ? preset.values.get(s.key) : s.fallback;
        if (s.stepped()) {
            try {
                value = new BigDecimal(value).stripTrailingZeros().toPlainString();
            } catch (NumberFormatException e) {
                value = s.fallback;
            }
        }
        return value;
    }

    /**
     * A preset's values (null: the Default button's), the settings it covers but does not name back to their defaults;
     * a setting no preset names (a Trace switch, a seed) keeps the player's value, as OpenRAC's launcher does.
     */
    void apply(Mod mod, Preset preset) {
        SharedPreferences.Editor edit = prefs.edit();
        for (Setting s : mod.settings) {
            if (covered(mod, s)) {
                edit.putString(mod.id + "." + s.key, presetGives(preset, s));
            }
        }
        edit.apply();
    }

    /** The button whose values the settings have: DEFAULT, a presetValue, or "" for none. */
    String lit(Mod mod) {
        for (int i = 0; i < mod.presets.size(); i++) {
            if (has(mod, mod.presets.get(i))) {
                return presetValue(i);
            }
        }
        return !ownDefault(mod) && has(mod, null) ? DEFAULT : "";
    }

    private boolean has(Mod mod, Preset preset) {
        for (Setting s : mod.settings) {
            if (covered(mod, s) && !get(mod, s).equals(presetGives(preset, s))) {
                return false;
            }
        }
        return true;
    }

    /** JSON as maps, lists, strings, numbers and booleans (null for JSON's null): what ModIndex reads. */
    static Map<String, Object> jsonMap(String text) {
        try {
            Object value = plainJson(new JSONObject(text));
            @SuppressWarnings("unchecked")
            Map<String, Object> map = (Map<String, Object>) value;
            return map;
        } catch (JSONException e) {
            throw new IllegalArgumentException(e.getMessage());
        }
    }

    private static Object plainJson(Object value) throws JSONException {
        if (value instanceof JSONObject) {
            JSONObject object = (JSONObject) value;
            Map<String, Object> map = new LinkedHashMap<>();
            for (java.util.Iterator<String> keys = object.keys(); keys.hasNext();) {
                String key = keys.next();
                map.put(key, plainJson(object.get(key)));
            }
            return map;
        }
        if (value instanceof JSONArray) {
            JSONArray array = (JSONArray) value;
            List<Object> list = new ArrayList<>();
            for (int i = 0; i < array.length(); i++) {
                list.add(plainJson(array.get(i)));
            }
            return list;
        }
        return value == JSONObject.NULL ? null : value;
    }

    /** The settings as the game reads them: each in its manifest's type. */
    JSONObject settings(Mod mod) throws JSONException {
        JSONObject out = new JSONObject();
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
     * The ones on, put in the game's plugins folder (their manifest and library) and their entries for its list;
     * those off are left out of the list.
     */
    List<JSONObject> writeForGame(File plugins, File nativeLibraries) throws IOException, JSONException {
        List<JSONObject> entries = new ArrayList<>();
        for (Mod mod : list()) {
            if (!on(mod)) {
                continue;
            }
            File folder = new File(plugins, mod.id);
            if (mod.added) {
                // As it came; the port makes a library read-only, as Android asks of code an app loads from its files.
                deleteAll(folder);
                copyAll(new File(addedRoot(), mod.id), folder);
            } else {
                if (!mod.library.isEmpty()) {
                    File library = new File(folder, "lib/android-arm64/" + mod.library);
                    library.getParentFile().mkdirs();
                    library.delete();
                    Files.copy(new File(nativeLibraries, "lib" + mod.library).toPath(), library.toPath(),
                        StandardCopyOption.REPLACE_EXISTING);
                }
                folder.mkdirs();
                copyAssets("mods/" + mod.id, folder);
            }
            JSONObject entry = new JSONObject();
            entry.put("id", mod.id);
            entry.put("enabled", true);
            entry.put("settings", settings(mod));
            entries.add(entry);
        }
        return entries;
    }

    /**
     * A bundled mod's folder of the APK's assets as it is, byte for byte and with its folders: its manifest, and files
     * of its own (RAC-Board's models and sounds in assets/).
     */
    private void copyAssets(String path, File dest) throws IOException {
        String[] names = context.getAssets().list(path);
        if (names == null || names.length == 0) {
            try (InputStream in = context.getAssets().open(path)) {
                dest.getParentFile().mkdirs();
                Files.copy(in, dest.toPath(), StandardCopyOption.REPLACE_EXISTING);
            } catch (java.io.FileNotFoundException e) {
                dest.mkdirs();  // an empty folder
            }
            return;
        }
        dest.mkdirs();
        for (String name : names) {
            copyAssets(path + "/" + name, new File(dest, name));
        }
    }

    private static String read(InputStream in) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        byte[] buffer = new byte[8192];
        for (int n = in.read(buffer); n > 0; n = in.read(buffer)) {
            bytes.write(buffer, 0, n);
        }
        return new String(bytes.toByteArray(), StandardCharsets.UTF_8);
    }
}
