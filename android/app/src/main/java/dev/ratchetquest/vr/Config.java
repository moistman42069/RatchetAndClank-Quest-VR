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

/**
 * The player's choices, and writing them where the game reads them: the mod's plugins.json and the
 * port's settings file. Each option here is real: it reaches the VR mod's settings (openrac-mod.json)
 * or the port's settings file (docs/port/SETTINGS.md).
 */
final class Config {
    static final String MOD_ID = "quest-vr";

    /** An option, its label, what the player picks among, the values written. */
    static final class Choice {
        final String key, label, help;
        final String[] names, values;
        final String fallback;

        Choice(String key, String label, String help, String[] names, String[] values, String fallback) {
            this.key = key;
            this.label = label;
            this.help = help;
            this.names = names;
            this.values = values;
            this.fallback = fallback;
        }
    }

    private static final String[] ON_OFF = {"On", "Off"};
    private static final String[] TRUE_FALSE = {"true", "false"};

    static final Choice PRESET = new Choice("preset", "Graphics preset",
        "Sets everything below at once. Start on Balanced, then adjust from there.",
        new String[] {"Performance", "Balanced", "Quality"}, new String[] {"performance", "balanced", "quality"},
        "balanced");
    static final Choice REFRESH = new Choice("refresh_hz", "Refresh rate",
        "Higher is smoother and costs more. If a rate is not held the game drops back to 72 by itself.",
        new String[] {"72 Hz", "80 Hz", "90 Hz", "120 Hz"}, new String[] {"72", "80", "90", "120"}, "72");
    static final Choice RESOLUTION = new Choice("resolution", "Eye resolution",
        "Of the headset's recommended size. The biggest effect on performance.",
        new String[] {"70%", "80%", "90%", "100%", "110%", "120%", "130%"},
        new String[] {"0.7", "0.8", "0.9", "1.0", "1.1", "1.2", "1.3"}, "1.0");
    static final Choice MSAA = new Choice("msaa", "Anti-aliasing",
        "Smooths jagged edges. Cheap on the Quest's GPU, and well worth it in VR.",
        new String[] {"Off", "2×", "4×"}, new String[] {"1", "2", "4"}, "2");
    static final Choice FILTER = new Choice("texture_filter", "Texture filter",
        "Edge-aware keeps the game's low-resolution textures sharper up close.",
        new String[] {"Normal", "Edge-aware"}, new String[] {"normal", "edge-aware"}, "normal");
    static final Choice ANISOTROPY = new Choice("anisotropy", "Anisotropic filtering",
        "Keeps floors sharp at a low angle.", new String[] {"Off", "2×", "4×", "8×"},
        new String[] {"1", "2", "4", "8"}, "4");
    static final Choice ORIGINAL_LOD = new Choice("original_lod", "Original draw distances",
        "On leaves out far scenery, as the PlayStation 2 did, for a little more frame rate. Off draws all of it.",
        ON_OFF, TRUE_FALSE, "false");
    static final Choice MAX_DRAW = new Choice("max_draw_distance", "Max draw distance",
        "On draws everything as far as you can see: enemies, crates, plants, effects, so nothing pops in. "
            + "Off draws each of those only as near as the game did. Costs some frame rate. Original draw distances "
            + "On overrides it.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice[] GRAPHICS = {PRESET, REFRESH, RESOLUTION, MSAA, FILTER, ANISOTROPY, ORIGINAL_LOD, MAX_DRAW};

    static final Choice VIEW = new Choice("view", "View",
        "First person is through Ratchet's eyes. Third person keeps the game's camera, with your head looking around. VR Options in the game switches it at any time.",
        new String[] {"First person", "Third person"}, new String[] {"first", "third"}, "first");
    static final Choice CUTSCENES = new Choice("cutscenes", "Cutscenes",
        "Through Ratchet's eyes: you see the scene from his head, your own head free, his hands as the scene moves them; " + 
            "a scene without him is seen from its camera. In VR: from the game's camera, in 3D around you. On a screen: the game's " + 
            "own framing on a big screen ahead, kinder if camera moves bother you. Movies are always on the screen.",
        new String[] {"Through his eyes", "In VR", "On a screen"}, new String[] {"eyes", "vr", "screen"}, "eyes");
    static final Choice TURN = new Choice("turn", "Turning", "Smooth is the default. If it makes you queasy, Snap is kinder to most stomachs.",
        new String[] {"Smooth", "Snap"}, new String[] {"smooth", "snap"}, "smooth");
    static final Choice SNAP = new Choice("snap_degrees", "Snap angle", "",
        new String[] {"30°", "45°", "60°", "90°"}, new String[] {"30", "45", "60", "90"}, "45");
    static final Choice SMOOTH = new Choice("smooth_degrees", "Smooth turn speed", "",
        new String[] {"Slow", "Medium", "Fast"}, new String[] {"90", "140", "220"}, "140");
    static final Choice WORLD = new Choice("world_scale", "World scale", "How big you are against the world.",
        new String[] {"80%", "90%", "100%", "110%", "120%"}, new String[] {"0.8", "0.9", "1", "1.1", "1.2"}, "1");
    static final Choice HUD = new Choice("hud_scale", "HUD size", "", new String[] {"Small", "Medium", "Large"},
        new String[] {"0.8", "1", "1.2"}, "1");
    static final Choice HAND_AIM = new Choice("hand_aim", "Aim with your right hand",
        "On: Ratchet faces where your hand points. Off: where you look.", ON_OFF, TRUE_FALSE, "true");
    static final Choice HIDE_BODY = new Choice("hide_body", "Hide Ratchet's body",
        "In first person: Ratchet, Clank and the pack on his back. The weapon in your hand stays.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice HAND_WEAPON = new Choice("hand_weapon", "Weapon in your right hand",
        "In first person. Off leaves it where the game draws it.", ON_OFF, TRUE_FALSE, "true");
    static final Choice THROW_BY_HAND = new Choice("throw_by_hand", "Throw bombs, mines and Decoys by hand",
        "Hold the trigger, make a throwing motion, let go. Let go slowly to keep it. Off: the trigger throws as the game does.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SWING_WRENCH = new Choice("swing_wrench", "Swing the wrench for real",
        "Hold the grip and hit things with the wrench itself; slam it down in the air for a Hyper Strike; let go mid-swing "
            + "to throw it, slowly to put it away. With the Walloper, punch to punch. Off: the buttons do it as in the game.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice TWO_HANDED = new Choice("two_handed", "Two-handed weapons",
        "The big guns Ratchet holds with both arms are heavy in one hand: they lag and droop. Squeeze the left grip "
            + "on the barrel to hold them steady. Off: every weapon follows the hand exactly.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SHOW_HANDS = new Choice("show_hands", "Ratchet's hands",
        "See Ratchet's forearms and hands at your controllers; they close with the grips and hold what you hold. "
            + "Needs Ratchet's body hidden. Off: only what you hold is seen.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice GRAB_LEDGES = new Choice("grab_ledges", "Grab ledges with your hands",
        "Squeeze the grips near a ledge to hold it. Let go with both to drop; shove down with both, or press A, to climb; "
            + "pull along the ledge to move. Off: Ratchet catches ledges by himself, as in the game.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice ROOM_SCALE = new Choice("room_scale", "Walk around your room",
        "Your steps move Ratchet, and walls stop you as they stop him. Off: only the stick moves him.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SWIM_STROKES = new Choice("swim_strokes", "Swim with your arms",
        "In water, stroke with your palms to swim where you look; push the water up with both hands to dive. "
            + "Off: as in the game.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SOLID_HANDS = new Choice("solid_hands", "Solid walls for what you hold",
        "Your wrench and guns stop at walls, and cannot fire through them. Off: they pass through.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice GLYPHS = new Choice("glyphs", "Controller buttons in hints",
        "The game's hints and menus show your controller's buttons (A, the trigger, the grips, the sticks) in place of "
            + "the PlayStation's. Off: the PlayStation's.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice PUZZLE_OVER_TOOL = new Choice("puzzle_over_tool", "The Trespasser's rings over the tool",
        "On: the puzzle hangs over the tool you plugged in. Off: it stays in front of your face.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SHOP_ON_FOOT = new Choice("shop_on_foot", "Shop on foot",
        "On: the vendor's screens are on the machine and you can walk while you shop. Off: the vendor is a flat screen.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice MENU_IN_WORLD = new Choice("menu_in_world", "Menus in the world",
        "On: the pause menu stands in the level around you, each part at its own depth. Off: one flat panel on black.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice HAPTICS = new Choice("haptics", "Vibration", "", ON_OFF, TRUE_FALSE, "true");
    static final Choice[] VR = {VIEW, CUTSCENES, TURN, SNAP, SMOOTH, WORLD, HUD, HAND_AIM, HAND_WEAPON, THROW_BY_HAND,
        SWING_WRENCH, TWO_HANDED, SHOW_HANDS, GRAB_LEDGES, PUZZLE_OVER_TOOL, SHOP_ON_FOOT, MENU_IN_WORLD, ROOM_SCALE,
        SWIM_STROKES, SOLID_HANDS, HIDE_BODY, GLYPHS, HAPTICS};

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

    String get(Choice choice) {
        // A value no longer offered (an older version's) is the default, not passed to the game.
        String value = prefs.getString(choice.key, choice.fallback);
        return java.util.Arrays.asList(choice.values).contains(value) ? value : choice.fallback;
    }

    void set(Choice choice, String value) {
        prefs.edit().putString(choice.key, value).apply();
    }

    void applyPreset(String preset) {
        set(PRESET, preset);
        switch (preset) {
            case "performance":
                set(RESOLUTION, "0.8");
                set(MSAA, "1");
                set(ANISOTROPY, "1");
                set(FILTER, "normal");
                set(ORIGINAL_LOD, "true");
                set(MAX_DRAW, "false");
                set(REFRESH, "72");
                break;
            case "quality":
                set(RESOLUTION, "1.2");
                set(MSAA, "4");
                set(ANISOTROPY, "8");
                set(FILTER, "edge-aware");
                set(ORIGINAL_LOD, "false");
                set(MAX_DRAW, "true");
                set(REFRESH, "72");
                break;
            default:
                set(RESOLUTION, "1.0");
                set(MSAA, "2");
                set(ANISOTROPY, "4");
                set(FILTER, "normal");
                set(ORIGINAL_LOD, "false");
                set(MAX_DRAW, "true");
                set(REFRESH, "72");
                break;
        }
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
            settings.put("view", bigScreen ? "screen" : get(VIEW));
            settings.put("cutscenes", get(CUTSCENES));
            settings.put("turn", get(TURN));
            settings.put("snap_degrees", Double.parseDouble(get(SNAP)));
            settings.put("smooth_degrees", Double.parseDouble(get(SMOOTH)));
            settings.put("world_scale", Double.parseDouble(get(WORLD)));
            settings.put("hud_scale", Double.parseDouble(get(HUD)));
            settings.put("haptics", Boolean.parseBoolean(get(HAPTICS)));
            settings.put("hand_aim", Boolean.parseBoolean(get(HAND_AIM)));
            settings.put("hide_body", Boolean.parseBoolean(get(HIDE_BODY)));
            settings.put("hand_weapon", Boolean.parseBoolean(get(HAND_WEAPON)));
            settings.put("throw_by_hand", Boolean.parseBoolean(get(THROW_BY_HAND)));
            settings.put("swing_wrench", Boolean.parseBoolean(get(SWING_WRENCH)));
            settings.put("two_handed", Boolean.parseBoolean(get(TWO_HANDED)));
            settings.put("show_hands", Boolean.parseBoolean(get(SHOW_HANDS)));
            settings.put("grab_ledges", Boolean.parseBoolean(get(GRAB_LEDGES)));
            settings.put("puzzle_over_tool", Boolean.parseBoolean(get(PUZZLE_OVER_TOOL)));
            settings.put("shop_on_foot", Boolean.parseBoolean(get(SHOP_ON_FOOT)));
            settings.put("menu_in_world", Boolean.parseBoolean(get(MENU_IN_WORLD)));
            settings.put("room_scale", Boolean.parseBoolean(get(ROOM_SCALE)));
            settings.put("swim_strokes", Boolean.parseBoolean(get(SWIM_STROKES)));
            settings.put("solid_hands", Boolean.parseBoolean(get(SOLID_HANDS)));
            settings.put("glyphs", Boolean.parseBoolean(get(GLYPHS)));
            settings.put("refresh_hz", Integer.parseInt(get(REFRESH)));
            settings.put("resolution", Double.parseDouble(get(RESOLUTION)));
            JSONObject mod = new JSONObject();
            mod.put("id", MOD_ID);
            mod.put("enabled", true);
            mod.put("settings", settings);
            JSONObject list = new JSONObject();
            list.put("format", 1);
            list.put("mods", new JSONArray().put(mod));
            Files.write(new File(tools.plugins(), "plugins.json").toPath(),
                list.toString(2).getBytes(StandardCharsets.UTF_8));
        } catch (JSONException e) {
            throw new IOException(e);
        }

        Files.write(tools.settingsFile().toPath(), gameSettings().getBytes(StandardCharsets.UTF_8));
    }

    /** The game's settings file (port/game/common/settings.cpp reads it; LauncherTest checks each value). */
    String gameSettings() {
        // The headset paces the frames: the window's swap never waits, and the port's own frame
        // cap stays out of it.
        return "vsync = off\n"
            + "frame_rate = display\n"
            + "aspect = 4:3\n"
            + "hud = centre\n"
            + "menu_bar = off\n"
            + "show_fps = off\n"
            + "render_scale = 100\n"
            + "msaa = " + get(MSAA) + "\n"
            + "texture_filter = " + get(FILTER) + "\n"
            + "anisotropy = " + get(ANISOTROPY) + "\n"
            + "original_lod = " + get(ORIGINAL_LOD) + "\n"
            + "max_draw_distance = " + get(MAX_DRAW) + "\n";
    }
}
