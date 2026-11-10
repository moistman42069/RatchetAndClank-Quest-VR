// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Every option the launcher offers, its words and its values, the presets, and what each choice writes for the game:
 * the VR mod's settings (openrac-mod.json) and the port's settings file (docs/port/SETTINGS.md). Plain Java, nothing of
 * Android's: the APK's launcher (Config) and the PC's (pc/launcher, PcConfig) are built from this one file, so the two
 * offer the same options and write the same settings. Where a choice is kept is each launcher's own (get and set).
 */
abstract class Options {
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
        "Higher is smoother and costs more. Keep the refresh rate (below) says what happens where a rate is not held.",
        new String[] {"72 Hz", "80 Hz", "90 Hz", "120 Hz"}, new String[] {"72", "80", "90", "120"}, "72");
    static final Choice KEEP_REFRESH = new Choice("keep_refresh", "Keep the refresh rate",
        "On: the headset stays at the rate you chose, so anywhere the game can keep up it is that smooth. Off: where "
            + "the game cannot hold a rate above 72 for twenty seconds, the headset drops back to 72 Hz by itself.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SPACE_WARP = new Choice("space_warp", "Smoother frames (AppSW)",
        "On: the game draws every other frame and the Quest makes the ones between from how everything moved (Meta's "
            + "Application SpaceWarp), so each picture has twice the time and 90 or 120 Hz holds. Fast moves can smear a "
            + "little at the edges of things. Off: the game draws every frame. On a PC your headset's own smoothing does this.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice CPU_GPU_BOOST = new Choice("cpu_gpu_boost", "CPU and GPU boost",
        "On: the Quest runs its processors at the highest speed it lets a game ask for, so fewer frames are missed. "
            + "It uses more battery and the headset gets warmer; when it is too warm the Quest slows them again by itself. "
            + "Off: the usual high speed. Apps such as Quest Games Optimizer can go higher still through developer mode, "
            + "which a game cannot do on its own.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice RESOLUTION = new Choice("resolution", "Eye resolution",
        "Of the headset's recommended size. The biggest effect on performance.",
        new String[] {"70%", "80%", "90%", "100%", "110%", "120%", "130%"},
        new String[] {"0.7", "0.8", "0.9", "1.0", "1.1", "1.2", "1.3"}, "1.0");
    static final Choice MSAA = new Choice("msaa", "Anti-aliasing",
        "Smooths jagged edges. Cheap for the graphics chip, and well worth it in VR.",
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
    static final Choice MATCHING_SHADOW = new Choice("matching_shadow", "Shadow matches what you see",
        "In first person Ratchet's shadow is cast by what you see of him: only your hands, or his body too with Player "
            + "body on. Off: by the whole body the game animates, wherever your hands are.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice FADES_IN_VIEW = new Choice("fades_in_view", "Fades fill your view",
        "The game's fades to black and white, the underwater tint and its screen flashes cover your whole view, as "
            + "they covered the television. Off: only the panel in front of you shows them.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SMOOTH_PLANT_EDGES = new Choice("smooth_plant_edges", "Smooth plant edges",
        "Removes the thin stray lines on leaves and flowers, and the hard straight edge of the dark patch round a "
            + "tree's foot, which fades out instead. Off: those lines and that edge stay.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice[] GRAPHICS = {PRESET, REFRESH, KEEP_REFRESH, SPACE_WARP, CPU_GPU_BOOST, RESOLUTION, MSAA, FILTER, ANISOTROPY, ORIGINAL_LOD,
        MAX_DRAW, MATCHING_SHADOW, FADES_IN_VIEW, SMOOTH_PLANT_EDGES};

    // The port's own options for play (docs/port/SETTINGS.md), beside OpenRAC's mods on the Game tab.
    static final Choice SKIP_ANY_MOVIE = new Choice("skip_any_movie", "Skip any movie",
        "Y skips every movie, also those the game makes you watch the first time. Off: only those the game lets you skip.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice VOLUME = new Choice("volume", "Game volume",
        "Everything the game plays, under the headset's own volume and the game's music and effects options.",
        new String[] {"100%", "80%", "60%", "40%", "20%", "Off"}, new String[] {"100", "80", "60", "40", "20", "0"}, "100");
    static final Choice[] GAME = {SKIP_ANY_MOVIE, VOLUME};

    static final Choice VIEW = new Choice("view", "View",
        "First person is through Ratchet's eyes. Third person keeps the game's camera, with your head looking around. VR Options in the game switches it at any time.",
        new String[] {"First person", "Third person"}, new String[] {"first", "third"}, "first");
    static final Choice CUTSCENES = new Choice("cutscenes", "Cutscenes",
        "Through Ratchet's eyes: you see the scene from his head, your own head free, his hands as the scene moves them; " +
            "a scene without him is seen from its camera. In VR: from the game's camera, in 3D around you. On a screen: the game's " +
            "own framing on a big screen ahead, kinder if camera moves bother you. Movies are always on the screen. " +
            "Through his eyes can now and then show a shot from an odd place, as his eyes are where the scene puts him even when " +
            "its camera looks elsewhere. X during a cutscene switches between Through his eyes and In VR (never to the screen), " +
            "and VR Options in the game has this row too; " +
            "it changes only how cutscenes are seen, never the View you play in.",
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
    // "Hide Ratchet's body" (hide_body, to 0.1.12) is gone, from the mod and its manifest too: Off drew his whole model
    // around the eyes in first person, the view inside or above his head and the weapon floating, with all of VR Body's
    // fitting off (a tester's report, 2026-11-05). His body in first person is VR Body's Player body and Body parts. A
    // value 0.1.12 saved for it stays in the preferences, never read nor passed to the game.
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
            + "Off: only what you hold is seen.",
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
    static final Choice WALL_FEEL = new Choice("wall_feel", "Vibrate when your hands meet the world",
        "The controller vibrates as your hand or what it holds touches a wall, more the harder you push.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SOLID_HANDS = new Choice("solid_hands", "World collision",
        "Your hands, wrench and guns stop at walls and cannot fire through them. Off: they pass through.",
        ON_OFF, TRUE_FALSE, "true");
    static final Choice SPARKLES_IN_WORLD = new Choice("sparkles_in_world", "Sparkles on the bolts",
        "On: the twinkle that shows a bolt or a gold bolt is on it, in the world. Off: as the game draws it, flat on your "
            + "view, where it seems to float in front of you.",
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
        SWIM_STROKES, SOLID_HANDS, WALL_FEEL, SPARKLES_IN_WORLD, GLYPHS, HAPTICS};

    /** The value chosen for `choice`: one it offers, else its default. Kept where each launcher keeps its choices. */
    abstract String get(Choice choice);

    abstract void set(Choice choice, String value);

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
     * The VR mod's settings in plugins.json, each in its manifest's type (Boolean, Double, Integer or String), in the
     * order they are written. `bigScreen` is the 2D mode: the game on a big screen in the headset.
     */
    Map<String, Object> modSettings(boolean bigScreen) {
        Map<String, Object> settings = new LinkedHashMap<>();
        settings.put("view", bigScreen ? "screen" : get(VIEW));
        settings.put("cutscenes", get(CUTSCENES));
        settings.put("turn", get(TURN));
        settings.put("snap_degrees", Double.parseDouble(get(SNAP)));
        settings.put("smooth_degrees", Double.parseDouble(get(SMOOTH)));
        settings.put("world_scale", Double.parseDouble(get(WORLD)));
        settings.put("hud_scale", Double.parseDouble(get(HUD)));
        settings.put("haptics", Boolean.parseBoolean(get(HAPTICS)));
        settings.put("hand_aim", Boolean.parseBoolean(get(HAND_AIM)));
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
        settings.put("wall_feel", Boolean.parseBoolean(get(WALL_FEEL)));
        settings.put("sparkles_in_world", Boolean.parseBoolean(get(SPARKLES_IN_WORLD)));
        settings.put("glyphs", Boolean.parseBoolean(get(GLYPHS)));
        settings.put("refresh_hz", Integer.parseInt(get(REFRESH)));
        settings.put("keep_refresh", Boolean.parseBoolean(get(KEEP_REFRESH)));
        settings.put("space_warp", Boolean.parseBoolean(get(SPACE_WARP)));
        settings.put("cpu_gpu_boost", Boolean.parseBoolean(get(CPU_GPU_BOOST)));
        settings.put("resolution", Double.parseDouble(get(RESOLUTION)));
        return settings;
    }

    /** The game's settings file (port/game/common/settings.cpp reads it; LauncherTest checks each value). */
    String gameSettings() {
        // The headset paces the frames (xrWaitFrame): the window's swap never waits, and the port's own frame
        // cap stays out of it. Not "display" (0.1.13 and before): the port holds that to the window's refresh
        // (frame_pacing::limit), which is not the headset's: on a PC with a 60 Hz monitor a 90 or 120 Hz headset
        // would get 60 frames. On the Quest the window's rate was not below the headset's (82 frames a second
        // seen at 120 Hz in the owner's 0.1.13 log), so nothing changes there.
        return "vsync = off\n"
            + "frame_rate = unlimited\n"
            + "aspect = 4:3\n"
            + "hud = centre\n"
            + "menu_bar = off\n"
            + "show_fps = off\n"
            + "render_scale = 100\n"
            + "msaa = " + get(MSAA) + "\n"
            + "texture_filter = " + get(FILTER) + "\n"
            + "anisotropy = " + get(ANISOTROPY) + "\n"
            + "original_lod = " + get(ORIGINAL_LOD) + "\n"
            + "max_draw_distance = " + get(MAX_DRAW) + "\n"
            + "matching_shadow = " + get(MATCHING_SHADOW) + "\n"
            + "fades_in_view = " + get(FADES_IN_VIEW) + "\n"
            + "smooth_plant_edges = " + get(SMOOTH_PLANT_EDGES) + "\n"
            + "skip_any_movie = " + get(SKIP_ANY_MOVIE) + "\n"
            + "volume = " + get(VOLUME) + "\n";
    }
}
