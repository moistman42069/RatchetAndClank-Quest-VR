// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.Shadows;
import org.robolectric.android.controller.ActivityController;
import org.robolectric.annotation.GraphicsMode;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** The launcher as the player meets it: every screen drawn and saved, every button's result checked. */
@RunWith(RobolectricTestRunner.class)
@org.robolectric.annotation.Config(sdk = 34, qualifiers = "w1180dp-h760dp-land-mdpi")
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class LauncherTest {
    private static final File OUT = new File(System.getProperty("user.dir"), "build/launcher-shots");

    private Application app;

    @Before
    public void setUp() {
        app = ApplicationProvider.getApplicationContext();
        OUT.mkdirs();
        Setup.STATE.running = false;
        Setup.STATE.starting = false;
        Setup.STATE.failure = null;
        Setup.STATE.done = false;
        Updater.network = false;
        Updater.STATE.tag = null;
        Updater.STATE.ready = false;
    }

    @Test
    public void updatesTakeOnlyThisProjectsReleases() throws Exception {
        assertEquals("RatchetQuestVR-0.2.0.apk", UpdatePolicy.asset("v0.2.0"));
        assertNull(UpdatePolicy.asset("0.2.0"));
        assertNull(UpdatePolicy.asset("v0.2"));
        assertNull(UpdatePolicy.asset("v0.2.0-test1"));
        assertTrue(UpdatePolicy.newer("v0.1.1", "0.1.0"));
        assertTrue(UpdatePolicy.newer("v1.0.0", "0.9.9"));
        assertTrue(UpdatePolicy.newer("v0.10.0", "0.9.0"));
        assertTrue(!UpdatePolicy.newer("v0.1.0", "0.1.0"));
        assertTrue(!UpdatePolicy.newer("v0.0.9", "0.1.0"));
        assertTrue(!UpdatePolicy.newer("latest", "0.1.0"));
        assertTrue(UpdatePolicy.allowedUrl("https://api.github.com/repos/moistman42069/RatchetAndClank-Quest-VR/releases/latest"));
        assertTrue(UpdatePolicy.allowedUrl("https://release-assets.githubusercontent.com/x"));
        assertTrue(!UpdatePolicy.allowedUrl("http://github.com/x"));
        assertTrue(!UpdatePolicy.allowedUrl("https://github.com.example.org/x"));
        assertTrue(!UpdatePolicy.allowedUrl("https://user@github.com/x"));
        assertTrue(UpdatePolicy.digest("sha256:" + "ab".repeat(32)));
        assertTrue(!UpdatePolicy.digest("sha1:" + "ab".repeat(20)));
        // This version can be found by the next one, and the build names its APK as the updater looks for it.
        assertEquals("RatchetQuestVR-" + BuildConfig.VERSION_NAME + ".apk", UpdatePolicy.asset("v" + BuildConfig.VERSION_NAME));
        String build = new String(Files.readAllBytes(new File("../../tools/build_quest.py").toPath()), StandardCharsets.UTF_8);
        assertTrue(build.contains("RatchetQuestVR-{version}.apk"));
    }

    @Test
    public void thePlayTabOffersUpdates() throws Exception {
        installed("pal");
        LauncherActivity activity = open();
        assertTrue(allText(activity).contains("Updates"));
        button(activity, "Check now");
        button(activity, "Automatic checks: On");
        Updater.STATE.tag = "v9.9.9";
        button(activity, "Play").performClick();
        button(activity, "Download and install 9.9.9");
        shot(activity, "13-update-available");
        button(activity, "Automatic checks: On").performClick();
        button(activity, "Automatic checks: Off");
        Updater.setAutomatic(app, true);
    }

    /** What's new: on a version's first start, biggest first (0.1.14: PC VR), once; Updates shows it again. */
    @Test
    public void whatsNewShowsOnceAVersionBiggestFirst() throws Exception {
        installed("pal");
        app.getSharedPreferences(LauncherActivity.WHATS_NEW_PREFS, Context.MODE_PRIVATE).edit().clear().commit();
        open();
        android.app.Dialog dialog = org.robolectric.shadows.ShadowDialog.getLatestDialog();
        assertTrue("the first start of a version shows it", dialog != null && dialog.isShowing());
        View box = dialog.getWindow().getDecorView();
        List<View> views = new ArrayList<>();
        collect(box, views);
        StringBuilder words = new StringBuilder();
        Button ok = null;
        for (View view : views) {
            if (view instanceof Button && ((Button) view).getText().toString().equals("Got it")) {
                ok = (Button) view;
            } else if (view instanceof TextView) {
                words.append(((TextView) view).getText()).append('\n');
            }
        }
        String text = words.toString();
        assertTrue(text.contains("What's new in " + BuildConfig.VERSION_NAME));
        int pc = text.indexOf("Now on PC VR too"), walls = text.indexOf("Wall run and jump");
        assertTrue("PC VR comes first", pc >= 0 && walls > pc);
        box.measure(View.MeasureSpec.makeMeasureSpec(820, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(700, View.MeasureSpec.AT_MOST));
        box.layout(0, 0, box.getMeasuredWidth(), box.getMeasuredHeight());
        Bitmap bitmap = Bitmap.createBitmap(box.getMeasuredWidth(), box.getMeasuredHeight(), Bitmap.Config.ARGB_8888);
        box.draw(new Canvas(bitmap));
        try (FileOutputStream out = new FileOutputStream(new File(OUT, "14-whats-new.png"))) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        }
        assertTrue("it has a Got it", ok != null);
        ok.performClick();
        assertFalse(dialog.isShowing());
        LauncherActivity again = open();
        assertTrue("once a version", org.robolectric.shadows.ShadowDialog.getLatestDialog() == dialog);
        button(again, "What's new").performClick();
        android.app.Dialog shown = org.robolectric.shadows.ShadowDialog.getLatestDialog();
        assertTrue("the Updates card shows it again", shown != dialog && shown.isShowing());
    }

    private LauncherActivity open() {
        ActivityController<LauncherActivity> controller = Robolectric.buildActivity(LauncherActivity.class);
        return controller.setup().get();
    }

    private void shot(LauncherActivity activity, String name) throws Exception {
        View root = activity.getWindow().getDecorView();
        root.measure(View.MeasureSpec.makeMeasureSpec(1180, View.MeasureSpec.EXACTLY),
            View.MeasureSpec.makeMeasureSpec(760, View.MeasureSpec.EXACTLY));
        root.layout(0, 0, 1180, 760);
        Bitmap bitmap = Bitmap.createBitmap(1180, 760, Bitmap.Config.ARGB_8888);
        root.draw(new Canvas(bitmap));
        try (FileOutputStream out = new FileOutputStream(new File(OUT, name + ".png"))) {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        }
    }

    private static void collect(View view, List<View> out) {
        out.add(view);
        if (view instanceof ViewGroup) {
            ViewGroup group = (ViewGroup) view;
            for (int i = 0; i < group.getChildCount(); i++) {
                collect(group.getChildAt(i), out);
            }
        }
    }

    private static String allText(LauncherActivity activity) {
        List<View> views = new ArrayList<>();
        collect(activity.getWindow().getDecorView(), views);
        StringBuilder out = new StringBuilder();
        for (View view : views) {
            if (view instanceof TextView) {
                out.append(((TextView) view).getText()).append('\n');
            }
        }
        return out.toString();
    }

    private static Button button(LauncherActivity activity, String label) {
        List<View> views = new ArrayList<>();
        collect(activity.getWindow().getDecorView(), views);
        for (View view : views) {
            if (view instanceof Button && ((Button) view).getText().toString().equals(label)) {
                return (Button) view;
            }
        }
        throw new AssertionError("no button " + label);
    }

    /**
     * An update moves Android's library folder, so the links in the last tools dangle, and File.exists() is false for
     * those: left behind, they kept openrac.old from going, and the next update "cannot move openrac aside".
     */
    @org.junit.Test
    public void anUpdateClearsToolsWhoseLinksDangle() throws Exception {
        File filesDir = app.getFilesDir();
        java.nio.file.Path gone = new File(filesDir, "a-library-folder-of-the-last-install/libopenrac-python.so").toPath();
        File old = new File(filesDir, "openrac.old");
        File bin = new File(old, "python/bin");
        bin.mkdirs();
        try {
            Files.createSymbolicLink(new File(bin, "python3").toPath(), gone);
        } catch (java.io.IOException | UnsupportedOperationException e) {
            org.junit.Assume.assumeNoException("this PC makes no links", e);
        }
        Tools.deleteRecursively(old);
        assertTrue("a folder with a dangling link in it is cleared", !old.exists());
    }

    private void installed(String version) throws Exception {
        Tools tools = new Tools(app);
        File levels = tools.levels(version);
        levels.mkdirs();
        Files.write(new File(levels, Setup.checkStamp()).toPath(), "{}".getBytes(StandardCharsets.UTF_8));
        app.getSharedPreferences(Setup.PREFS, Context.MODE_PRIVATE).edit().putString(Setup.KEY_VERSION, version).commit();
    }

    /** A set-up an older game made: its levels stamped in the format before this game's. */
    private void installedByAnOlderGame(String version) throws Exception {
        Tools tools = new Tools(app);
        File levels = tools.levels(version);
        levels.mkdirs();
        String old = "verified-" + (BuildConfig.LEVEL_FORMAT - 1) + ".json";
        Files.write(new File(levels, old).toPath(), "{}".getBytes(StandardCharsets.UTF_8));
        app.getSharedPreferences(Setup.PREFS, Context.MODE_PRIVATE).edit().putString(Setup.KEY_VERSION, version).commit();
    }

    private static String source(String path) throws Exception {
        return new String(Files.readAllBytes(new File("../../third_party/OpenRAC/" + path).toPath()), StandardCharsets.UTF_8);
    }

    /** A list the port's settings.cpp or settings.h parses a key against: its words, or its numbers. */
    private static List<String> listed(String text, String name) {
        java.util.regex.Matcher m = java.util.regex.Pattern.compile(name + "\\[\\] = \\{([^}]*)\\}").matcher(text);
        assertTrue("OpenRAC lists " + name, m.find());
        List<String> out = new ArrayList<>();
        for (String item : m.group(1).split(",")) {
            out.add(item.trim().replace("\"", ""));
        }
        return out;
    }

    private static void readByThePort(String ini, java.util.Map<String, List<String>> allowed) {
        for (String line : ini.split("\n")) {
            String[] pair = line.split(" = ", 2);
            assertTrue("a key the port reads: " + line, allowed.containsKey(pair[0]));
            assertTrue("a value the port reads: " + line, allowed.get(pair[0]).contains(pair[1]));
        }
    }

    @Test
    public void everyGameSettingIsOneThePortReads() throws Exception {
        String cpp = source("port/game/common/settings.cpp");
        String h = source("port/game/common/settings.h");
        List<String> bools = Arrays.asList("1", "true", "on", "yes", "0", "false", "off", "no");
        java.util.Map<String, List<String>> allowed = new java.util.HashMap<>();
        allowed.put("vsync", listed(cpp, "kVSync"));
        allowed.put("frame_rate", Arrays.asList("original", "display", "unlimited"));
        allowed.put("aspect", listed(cpp, "kAspect"));
        allowed.put("hud", listed(cpp, "kHud"));
        allowed.put("menu_bar", bools);
        allowed.put("show_fps", bools);
        allowed.put("render_scale", Arrays.asList("100"));
        allowed.put("msaa", listed(h, "kSampleCounts"));
        allowed.put("texture_filter", listed(cpp, "kTextureFilter"));
        List<String> anisotropy = new ArrayList<>(listed(h, "kAnisotropies"));
        anisotropy.add("auto");
        allowed.put("anisotropy", anisotropy);
        allowed.put("original_lod", bools);
        allowed.put("max_draw_distance", bools);
        assertTrue("the port parses max_draw_distance", cpp.contains("key == \"max_draw_distance\""));
        allowed.put("draw_distance", listed(h, "kDrawDistances"));
        assertTrue("the port parses draw_distance", cpp.contains("key == \"draw_distance\""));
        allowed.put("matching_shadow", bools);
        assertTrue("the port parses matching_shadow", cpp.contains("key == \"matching_shadow\""));
        allowed.put("fades_in_view", bools);
        assertTrue("the port parses fades_in_view", cpp.contains("key == \"fades_in_view\""));
        allowed.put("smooth_plant_edges", bools);
        assertTrue("the port parses smooth_plant_edges", cpp.contains("key == \"smooth_plant_edges\""));
        allowed.put("skip_any_movie", bools);
        assertTrue("the port parses skip_any_movie", cpp.contains("key == \"skip_any_movie\""));
        List<String> volumes = new ArrayList<>();
        for (int volume = 0; volume <= 100; volume++) {
            volumes.add(Integer.toString(volume));
        }
        allowed.put("volume", volumes);
        assertTrue("the port reads volume from 0 to 100", cpp.contains("parse_int(value, 0, 100, s.volume)"));

        Config config = new Config(app);
        readByThePort(config.gameSettings(), allowed);
        for (String preset : Config.PRESET.values) {
            config.applyPreset(preset);
            readByThePort(config.gameSettings(), allowed);
        }
        for (Config.Choice choice : Config.GRAPHICS) {
            for (String value : choice.values) {
                if (choice != Config.PRESET) {
                    config.set(choice, value);
                    readByThePort(config.gameSettings(), allowed);
                }
            }
        }
        for (Config.Choice choice : Config.GAME) {
            for (String value : choice.values) {
                config.set(choice, value);
                readByThePort(config.gameSettings(), allowed);
            }
        }
        // A value an older version stored and no longer offered is the default.
        app.getSharedPreferences("options", Context.MODE_PRIVATE).edit().putString("anisotropy", "off").commit();
        assertEquals("4", config.get(Config.ANISOTROPY));
    }

    @Test
    public void farSceneryIsDrawnByDefaultAndAfterTheUpdate() {
        android.content.SharedPreferences options = app.getSharedPreferences("options", Context.MODE_PRIVATE);
        // A new install, and Balanced chosen.
        Config config = new Config(app);
        assertTrue(config.gameSettings().contains("original_lod = false"));
        config.applyPreset("balanced");
        assertTrue(config.gameSettings().contains("original_lod = false"));

        // An update from 0.1.4 with On stored, by Balanced: once, whatever preset that was.
        options.edit().clear().putString("preset", "balanced").putString("original_lod", "true").commit();
        config = new Config(app);
        assertEquals("false", config.get(Config.ORIGINAL_LOD));
        config.set(Config.ORIGINAL_LOD, "true");
        assertEquals("the player's own choice afterwards stays", "true", new Config(app).get(Config.ORIGINAL_LOD));

        // Performance still asks for the limit, stored or chosen.
        options.edit().clear().putString("preset", "performance").putString("original_lod", "true").commit();
        assertEquals("true", new Config(app).get(Config.ORIGINAL_LOD));
        options.edit().clear().commit();
        config = new Config(app);
        config.applyPreset("performance");
        assertEquals("true", config.get(Config.ORIGINAL_LOD));
    }

    @Test
    public void maxDrawDistanceIsOnByDefaultAndOffOnPerformance() {
        android.content.SharedPreferences options = app.getSharedPreferences("options", Context.MODE_PRIVATE);
        // A new install, and an update from a version without it: on.
        options.edit().clear().commit();
        Config config = new Config(app);
        assertTrue(config.gameSettings().contains("max_draw_distance = true\n"));
        options.edit().clear().putString("preset", "balanced").putString("original_lod", "false").commit();
        assertEquals("true", new Config(app).get(Config.MAX_DRAW));
        // Performance turns it off, Balanced and Quality on; the player's own choice stays.
        config.applyPreset("performance");
        assertTrue(config.gameSettings().contains("max_draw_distance = false\n"));
        config.applyPreset("balanced");
        assertEquals("true", config.get(Config.MAX_DRAW));
        config.applyPreset("quality");
        assertEquals("true", config.get(Config.MAX_DRAW));
        config.set(Config.MAX_DRAW, "false");
        assertEquals("false", new Config(app).get(Config.MAX_DRAW));
        // The Draw distance is the launcher's too (0.1.19): never one the game's own menu left in the file. Max draw
        // distance on is Unlimited whatever it says (settings.h draws_any_distance).
        assertTrue(new Config(app).gameSettings().contains("draw_distance = 400\n"));
    }

    /** Foveated rendering (0.1.19, the owner's ask): Medium by default, High on Performance, Low on Quality, to the mod. */
    @Test
    public void foveatedRenderingIsMediumByDefaultAndFollowsThePresets() {
        android.content.SharedPreferences options = app.getSharedPreferences("options", Context.MODE_PRIVATE);
        options.edit().clear().commit();
        Config config = new Config(app);
        assertEquals("2", config.get(Config.FOVEATION));
        assertEquals(2, config.modSettings(false).get("foveation"));
        assertTrue("on the Graphics tab", Arrays.asList(Config.GRAPHICS).contains(Config.FOVEATION));
        config.applyPreset("performance");
        assertEquals(3, config.modSettings(false).get("foveation"));
        config.applyPreset("quality");
        assertEquals(1, config.modSettings(false).get("foveation"));
        config.applyPreset("balanced");
        assertEquals(2, config.modSettings(false).get("foveation"));
        config.set(Config.FOVEATION, "0");
        assertEquals("the player's own choice stays", 0, new Config(app).modSettings(false).get("foveation"));
    }

    /** Third person controls and the D-pad (0.1.19, the owner's ask): Gamepad and by the head to start, to the mod. */
    @Test
    public void thirdPersonIsAGamepadWithTheDpadByTheHead() {
        app.getSharedPreferences("options", Context.MODE_PRIVATE).edit().clear().commit();
        Config config = new Config(app);
        assertEquals("gamepad", config.modSettings(false).get("third_person_controls"));
        assertEquals("head", config.modSettings(false).get("dpad"));
        assertTrue("on the VR tab", Arrays.asList(Config.VR).contains(Config.THIRD_PERSON_CONTROLS));
        assertTrue("on the VR tab", Arrays.asList(Config.VR).contains(Config.DPAD));
        config.set(Config.THIRD_PERSON_CONTROLS, "vr");
        config.set(Config.DPAD, "thumb_rest");
        assertEquals("vr", new Config(app).modSettings(false).get("third_person_controls"));
        assertEquals("thumb_rest", new Config(app).modSettings(false).get("dpad"));
    }

    /**
     * Third person buttons (0.1.21, a tester: the wrench on X, on the left controller): Like first person to start (0.1.24,
     * a tester: "should be set to 'like first person' by default"; an Xbox controller's to 0.1.23); a control picked for an
     * action swaps with the action that had it, so none presses two and none is lost; a layout chosen is kept.
     */
    @Test
    public void thirdPersonButtonsSwapAndNeverShareAControl() {
        app.getSharedPreferences("options", Context.MODE_PRIVATE).edit().clear().commit();
        Config config = new Config(app);
        assertTrue(config.padLayoutIs(Config.PAD_LIKE_FIRST_PERSON));
        assertEquals("right_grip", config.modSettings(false).get("pad_square"));
        assertEquals("right_trigger", config.modSettings(false).get("pad_circle"));
        assertEquals("x", config.modSettings(false).get("pad_select"));
        assertEquals(Config.PAD_SELECT, config.assignPadButton(Config.PAD_SQUARE, "x"));
        assertEquals("x", new Config(app).modSettings(false).get("pad_square"));
        assertEquals("right_grip", new Config(app).modSettings(false).get("pad_select"));
        config.applyPadLayout(Config.PAD_XBOX);
        assertTrue(new Config(app).padLayoutIs(Config.PAD_XBOX));
        assertEquals("b", new Config(app).modSettings(false).get("pad_circle"));
        assertEquals("right_grip", new Config(app).modSettings(false).get("pad_r1"));
        // Two on one control (a file written by hand): the first keeps it, the other has none.
        config.set(Config.PAD_CIRCLE, "a");
        assertEquals("a", config.modSettings(false).get("pad_cross"));
        assertEquals("none", config.modSettings(false).get("pad_circle"));
        for (String[] layout : Config.PAD_PRESETS) {
            assertEquals(Config.PAD_BUTTONS.length, layout.length);
            // Each control on one button at most; Not used may be on any number.
            java.util.Set<String> seen = new java.util.HashSet<>();
            for (String control : layout) {
                assertTrue("a preset gives " + control + " one button", Config.PAD_NONE.equals(control) || seen.add(control));
            }
        }
    }

    @Test
    public void matchingShadowIsOnByDefaultWhateverThePreset() {
        app.getSharedPreferences("options", Context.MODE_PRIVATE).edit().clear().commit();
        Config config = new Config(app);
        assertTrue(config.gameSettings().contains("matching_shadow = true\n"));
        for (String preset : Config.PRESET.values) {
            config.applyPreset(preset);
            assertEquals("true", config.get(Config.MATCHING_SHADOW));
        }
        config.set(Config.MATCHING_SHADOW, "false");
        assertTrue(new Config(app).gameSettings().contains("matching_shadow = false\n"));
    }

    @Test
    public void fadesFillYourViewByDefaultWhateverThePreset() {
        app.getSharedPreferences("options", Context.MODE_PRIVATE).edit().clear().commit();
        Config config = new Config(app);
        assertTrue(config.gameSettings().contains("fades_in_view = true\n"));
        for (String preset : Config.PRESET.values) {
            config.applyPreset(preset);
            assertEquals("true", config.get(Config.FADES_IN_VIEW));
        }
        config.set(Config.FADES_IN_VIEW, "false");
        assertTrue(new Config(app).gameSettings().contains("fades_in_view = false\n"));
    }

    @Test
    public void plantEdgesAreSmoothByDefaultWhateverThePreset() {
        app.getSharedPreferences("options", Context.MODE_PRIVATE).edit().clear().commit();
        Config config = new Config(app);
        assertTrue(config.gameSettings().contains("smooth_plant_edges = true\n"));
        for (String preset : Config.PRESET.values) {
            config.applyPreset(preset);
            assertEquals("true", config.get(Config.SMOOTH_PLANT_EDGES));
        }
        config.set(Config.SMOOTH_PLANT_EDGES, "false");
        assertTrue(new Config(app).gameSettings().contains("smooth_plant_edges = false\n"));
    }

    @Test
    public void everyMovieSkipsAndTheVolumeIsFullByDefault() {
        app.getSharedPreferences("options", Context.MODE_PRIVATE).edit().clear().commit();
        Config config = new Config(app);
        assertTrue(config.gameSettings().contains("skip_any_movie = true\n"));
        assertTrue(config.gameSettings().contains("volume = 100\n"));
        config.set(Config.SKIP_ANY_MOVIE, "false");
        config.set(Config.VOLUME, "40");
        String ini = new Config(app).gameSettings();
        assertTrue(ini.contains("skip_any_movie = false\n"));
        assertTrue(ini.contains("volume = 40\n"));
    }

    /** A manifest as OpenRAC's SDK packages one (tools/package_mod.py): every setting type the launcher offers. */
    private static final String MANIFEST = "{\"format\": 2, \"id\": \"test-mod\", \"name\": \"Test mod\","
        + " \"description\": \"For the test.\", \"plugin\": {\"android-arm64\": \"lib/android-arm64/test_mod.so\"},"
        + " \"settings\": ["
        + "  {\"key\": \"trace\", \"name\": \"Trace\", \"type\": \"bool\", \"default\": false},"
        + "  {\"key\": \"mode\", \"name\": \"Mode\", \"type\": \"choice\", \"default\": \"b\","
        + "   \"choices\": [{\"value\": \"a\", \"name\": \"A\"}, {\"value\": \"b\", \"name\": \"B\"}]},"
        + "  {\"key\": \"count\", \"name\": \"Count\", \"type\": \"int\", \"default\": 2, \"min\": 0, \"max\": 4},"
        + "  {\"key\": \"scale\", \"name\": \"Scale\", \"type\": \"float\", \"default\": 1.0, \"min\": 0.5,"
        + "   \"max\": 2.0, \"step\": 0.25},"
        + "  {\"key\": \"seed\", \"name\": \"Seed\", \"type\": \"string\", \"default\": \"\", \"max_length\": 8}],"
        + " \"presets\": [{\"id\": \"hard\", \"name\": \"Hard\", \"values\": {\"count\": 4, \"scale\": 2.0}}]}";

    @Test
    public void aBundledModsSettingsAreItsManifests() throws Exception {
        BundledMods.Mod mod = BundledMods.parse(MANIFEST);
        assertEquals("test-mod", mod.id);
        assertEquals("test_mod.so", mod.library);
        assertEquals(5, mod.settings.size());
        BundledMods.Setting count = mod.settings.get(2), scale = mod.settings.get(3);
        assertEquals(Arrays.asList("0", "1", "2", "3", "4"), Arrays.asList(count.values));
        assertEquals(Arrays.asList("0.5", "0.75", "1", "1.25", "1.5", "1.75", "2"), Arrays.asList(scale.values));
        assertEquals("1", scale.fallback);

        app.getSharedPreferences("mods", Context.MODE_PRIVATE).edit().clear().commit();
        BundledMods mods = new BundledMods(app);
        JSONObject defaults = mods.settings(mod);
        assertEquals(false, defaults.get("trace"));
        assertEquals("b", defaults.get("mode"));
        assertEquals(2, defaults.get("count"));
        assertEquals(1.0, defaults.getDouble("scale"), 0.0);
        assertEquals("", defaults.get("seed"));
        assertTrue("a mod that changes the game's rules is off until the player turns it on", !mods.on(mod));

        // As OpenRAC's launcher (settings.md): a preset sets what the presets cover (each setting one names, and every
        // keybind); the others keep the player's values. The button lit is the one whose values the settings have.
        assertEquals(BundledMods.DEFAULT, mods.lit(mod));
        mods.set(mod, mod.settings.get(1), "a");
        mods.apply(mod, mod.presets.get(0));
        JSONObject hard = mods.settings(mod);
        assertEquals(4, hard.get("count"));
        assertEquals(2.0, hard.getDouble("scale"), 0.0);
        assertEquals("a", hard.get("mode"));
        assertEquals(BundledMods.presetValue(0), mods.lit(mod));
        mods.set(mod, count, "3");
        assertEquals("", mods.lit(mod));
        mods.apply(mod, null);
        assertEquals(2, mods.settings(mod).get("count"));
        assertEquals(1.0, mods.settings(mod).getDouble("scale"), 0.0);
        assertEquals("a", mods.settings(mod).get("mode"));
        assertEquals(BundledMods.DEFAULT, mods.lit(mod));

        // A stored value the manifest no longer offers, or too long, is not written.
        mods.set(mod, count, "9");
        mods.set(mod, mod.settings.get(4), "abcdefghij");
        assertEquals(2, mods.settings(mod).get("count"));
        assertEquals("abcdefgh", mods.settings(mod).get("seed"));
    }

    @Test
    public void aModsKeybindsPresetsAndOtherGamesSettingsAreAsOpenRacsLauncherHasThem() throws Exception {
        // RAC-Board's kind of manifest: keybinds, a setting only Going Commando shows, and a preset that is the default.
        BundledMods.Mod mod = BundledMods.parse("{\"format\": 2, \"id\": \"board\", \"name\": \"Board\", \"settings\": ["
            + " {\"key\": \"toggle\", \"name\": \"Toggle\", \"type\": \"keybind\", \"default\": \"KeyB\"},"
            + " {\"key\": \"gc\", \"name\": \"Only in GC\", \"type\": \"bool\", \"default\": true, \"games\": [\"rac2\"]},"
            + " {\"key\": \"speed\", \"name\": \"Speed\", \"type\": \"int\", \"default\": 1, \"min\": 0, \"max\": 2},"
            + " {\"key\": \"trace\", \"name\": \"Trace\", \"type\": \"bool\", \"default\": false}],"
            + " \"presets\": [{\"id\": \"default\", \"name\": \"Normal\", \"values\": {\"speed\": 1}},"
            + "  {\"id\": \"fast\", \"name\": \"Fast\", \"values\": {\"speed\": 2, \"toggle\": \"KeyF\"}}]}");
        assertEquals("the Going Commando one is left to the mod's default", 3, mod.settings.size());
        assertEquals("keybind", mod.settings.get(0).type);
        assertTrue(BundledMods.ownDefault(mod));

        app.getSharedPreferences("mods", Context.MODE_PRIVATE).edit().clear().commit();
        BundledMods mods = new BundledMods(app);
        assertEquals(BundledMods.presetValue(0), mods.lit(mod));
        mods.set(mod, mod.settings.get(2), "true");  // Trace: no preset names it
        mods.apply(mod, mod.presets.get(1));
        JSONObject fast = mods.settings(mod);
        assertEquals("KeyF", fast.get("toggle"));
        assertEquals(2, fast.get("speed"));
        assertEquals(true, fast.get("trace"));
        assertEquals(BundledMods.presetValue(1), mods.lit(mod));
        mods.apply(mod, mod.presets.get(0));
        assertEquals("a keybind the preset leaves out goes back to its default", "KeyB", mods.settings(mod).get("toggle"));
        assertEquals(true, mods.settings(mod).get("trace"));
        assertEquals(BundledMods.presetValue(0), mods.lit(mod));

        StringBuilder many = new StringBuilder();
        for (int i = 0; i < 20; i++) {
            many.append("KeyA,");
        }
        mods.set(mod, mod.settings.get(0), many.toString());
        assertEquals(64, mods.settings(mod).getString("toggle").length());
    }

    /** OpenRAC's own index fixture and the minisign test key it is signed with (launcher/core/src/index.rs tests). */
    private static final String INDEX_FIXTURES = "../../third_party/OpenRAC/launcher/core/tests/fixtures";
    private static final String INDEX_TEST_KEY = "dW50cnVzdGVkIGNvbW1lbnQ6IG1pbmlzaWduIHB1YmxpYyBrZXkgMzk2RkM2MzFGRDkyM0NGMgpSV1R"
        + "5UEpMOU1jWnZPUU8zNE1OTytWWmdjN3hwMjRXOFBKRUpSSkNvVGlwMlIxU1pYV0FIVGpsSAo=";

    private static String hex(byte[] bytes) {
        StringBuilder out = new StringBuilder();
        for (byte b : bytes) {
            out.append(String.format("%02x", b & 0xff));
        }
        return out.toString();
    }

    private static byte[] unhex(String text) {
        byte[] out = new byte[text.length() / 2];
        for (int i = 0; i < out.length; i++) {
            out[i] = (byte) Integer.parseInt(text.substring(2 * i, 2 * i + 2), 16);
        }
        return out;
    }

    @Test
    public void openRacsModListIsCheckedAndReadAsItsLauncherDoes() throws Exception {
        assertEquals("786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b9"
            + "03a685b1448b755d56f701afe9be2ce", hex(ModIndex.Blake2b.digest512(new byte[0])));
        assertEquals("ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc95"
            + "18d38aa8dbf1925ab92386edd4009923", hex(ModIndex.Blake2b.digest512("abc".getBytes(StandardCharsets.US_ASCII))));
        // RFC 8032 7.1, test 1.
        byte[] key = unhex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
        byte[] signed = unhex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46"
            + "bd25bf5f0595bbe24655141438e7a100b");
        assertTrue(ModIndex.Ed25519.verify(key, new byte[0], signed));
        signed[5] ^= 1;
        assertTrue(!ModIndex.Ed25519.verify(key, new byte[0], signed));

        byte[] body = Files.readAllBytes(new File(INDEX_FIXTURES, "index.json").toPath());
        String signature = new String(Files.readAllBytes(new File(INDEX_FIXTURES, "index.json.minisig").toPath()),
            StandardCharsets.UTF_8);
        assertTrue("the fixture checks with its key", ModIndex.Minisign.verify(body, signature, INDEX_TEST_KEY));
        assertTrue("not with OpenRAC's own", !ModIndex.Minisign.verify(body, signature, ModIndex.PUBLIC_KEY));
        byte[] changed = body.clone();
        changed[20] ^= 1;
        assertTrue("a changed list is refused", !ModIndex.Minisign.verify(changed, signature, INDEX_TEST_KEY));
        assertTrue("a changed trusted comment is refused",
            !ModIndex.Minisign.verify(body, signature.replace("file:index.json", "file:other.json"), INDEX_TEST_KEY));
        assertTrue(ModIndex.Minisign.publicKey(ModIndex.PUBLIC_KEY) != null);

        ModIndex index = ModIndex.read(body, signature, INDEX_TEST_KEY, 0, "android-arm64", 1, 14, BundledMods::jsonMap, false, "");
        assertTrue("the fixture's expiry has passed", index.stale);
        ModIndex.Offer ammo = index.get("ammo-max");
        assertTrue("ammo-max offers 1.0.1, not the 1.0.0 taken back",
            ammo != null && "1.0.1".equals(ammo.version) && ammo.url.endsWith("/ammo-max-1.0.1.zip"));
        assertTrue(index.revoked("ammo-max", "1.0.0") != null);
        boolean refused = false;
        try {
            ModIndex.read(body, signature, INDEX_TEST_KEY, index.serial + 1, "android-arm64", 1, 14, BundledMods::jsonMap, false, "");
        } catch (IOException e) {
            refused = true;
        }
        assertTrue("a list older than one seen is refused", refused);
        int left = 0;
        for (ModIndex.Offer offer : ModIndex.read(body, signature, INDEX_TEST_KEY, 0, "android-arm64", 1, 9, BundledMods::jsonMap,
                                                 false, "").offers) {
            left += offer.why != null ? 1 : 0;
        }
        assertTrue("an older mod interface leaves some out, saying why", left > 0);
    }

    @Test
    public void theMultiplayerNameIsAsOpenRacsLauncherKeepsIt() throws Exception {
        // OpenRAC's random_name_from: the same seed, the same name, always a valid one.
        assertEquals("BraveLombax00", PlayerName.fromSeed(0));
        for (long seed : new long[] {1, 42, 123456789L, -1, Long.MIN_VALUE, Long.MAX_VALUE}) {
            assertTrue(PlayerName.fromSeed(seed), PlayerName.valid(PlayerName.fromSeed(seed)));
        }
        assertFalse(PlayerName.valid("no spaces"));
        assertFalse(PlayerName.valid("ab"));
        assertFalse(PlayerName.valid("x2345678901234567"));
        assertTrue(PlayerName.valid("Brave_Lombax_42"));

        app.getSharedPreferences("mods", Context.MODE_PRIVATE).edit().clear().commit();
        BundledMods mods = new BundledMods(app);
        String made = mods.playerName();
        assertTrue(PlayerName.valid(made));
        assertEquals("a made-up name is kept", made, mods.playerName());
        assertTrue(PlayerName.validKey(mods.playerKey()));
        assertEquals("the key never changes", mods.playerKey(), mods.playerKey());
        assertTrue(mods.setPlayerName("no spaces") != null);
        assertEquals(made, mods.playerName());
        assertNull(mods.setPlayerName("Brave_Lombax42"));

        // A string setting whose default is ${player_name} shows the name, and the game gets the placeholder until
        // the player sets a value of their own (docs/port/MODS.md).
        BundledMods.Mod mod = BundledMods.parse("{\"format\": 2, \"id\": \"online\", \"name\": \"Online\", \"settings\": ["
            + " {\"key\": \"name\", \"name\": \"Name\", \"type\": \"string\", \"default\": \"${player_name}\", \"max_length\": 16}]}");
        BundledMods.Setting name = mod.settings.get(0);
        assertEquals("Brave_Lombax42", mods.shown(mod, name));
        assertEquals("${player_name}", mods.settings(mod).get("name"));
        mods.set(mod, name, "Brave_Lombax42");
        assertEquals("typed back as shown, it stays the placeholder", "${player_name}", mods.settings(mod).get("name"));
        assertNull(mods.setPlayerName("Other_One"));
        assertEquals("Other_One", mods.shown(mod, name));
        mods.set(mod, name, "Mine");
        assertEquals("Mine", mods.settings(mod).get("name"));
        assertEquals("Mine", mods.shown(mod, name));
    }

    private static byte[] zip(String... namesAndTexts) throws Exception {
        java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
        try (java.util.zip.ZipOutputStream out = new java.util.zip.ZipOutputStream(bytes)) {
            for (int i = 0; i < namesAndTexts.length; i += 2) {
                out.putNextEntry(new java.util.zip.ZipEntry(namesAndTexts[i]));
                out.write(namesAndTexts[i + 1].getBytes(StandardCharsets.UTF_8));
                out.closeEntry();
            }
        }
        return bytes.toByteArray();
    }

    @Test
    public void aModFileIsKeptAndGoesToTheGame() throws Exception {
        String manifest = "{\"format\": 2, \"id\": \"swap\", \"name\": \"Swap\", \"type\": \"script\", \"api\": \"1.1\","
            + " \"scripts\": [\"main.luau\"], \"games\": {\"rac1\": [\"pal\", \"ntsc\"]}}";
        app.getSharedPreferences("mods", Context.MODE_PRIVATE).edit().clear().commit();
        BundledMods mods = new BundledMods(app);
        assertEquals("Swap", mods.add(new java.io.ByteArrayInputStream(
            zip("swap/openrac-mod.json", manifest, "swap/main.luau", "-- a script"))));
        BundledMods.Mod added = null;
        for (BundledMods.Mod mod : mods.list()) {
            if (mod.id.equals("swap")) {
                added = mod;
            } else {
                mods.setOn(mod, false);
            }
        }
        assertTrue(added != null && added.added);
        assertTrue("a mod the player added is on until they turn it off", mods.on(added));
        File plugins = new File(app.getFilesDir(), "test-plugins");
        java.util.List<JSONObject> entries = mods.writeForGame(plugins, new File(app.getFilesDir(), "no-libraries"));
        assertEquals(1, entries.size());
        assertEquals("swap", entries.get(0).get("id"));
        assertTrue(new File(plugins, "swap/main.luau").isFile());
        assertEquals("", BundledMods.lastStart(plugins, added));
        Files.write(new File(plugins, "status.json").toPath(), ("{\"mods\": [{\"id\": \"swap\", \"state\": \"failed\","
            + " \"reason\": \"it stopped\"}]}").getBytes(StandardCharsets.UTF_8));
        assertEquals("it stopped", BundledMods.lastStart(plugins, added));
        mods.remove(added, plugins);
        assertTrue(!new File(plugins, "swap").exists());
        for (BundledMods.Mod mod : mods.list()) {
            assertTrue(!mod.id.equals("swap"));
        }
    }

    @Test
    public void aBuiltInModTakesTheListsVersionAndGoesBack() throws Exception {
        // 0.1.19, the owner: "we shouldnt have mismatches". The list's version of a built-in mod takes its place, keeps
        // its on or off, goes to the game, and the built-in comes back when it is removed; RAC-Board's changes stay.
        assertEquals(1, ModIndex.compareVersions("0.12.10", "0.12.9"));
        assertEquals(1, ModIndex.compareVersions("0.12.3", "0.12.2"));
        assertEquals(-1, ModIndex.compareVersions("0.8.0", "0.12.2"));
        assertEquals(0, ModIndex.compareVersions("1.0", "1.0.0"));
        assertTrue(ModIndex.keepsBuiltIn("rac-board"));
        assertFalse(ModIndex.keepsBuiltIn("multiplayer"));
        byte[] body = Files.readAllBytes(new File(INDEX_FIXTURES, "index.json").toPath());
        String signature = new String(Files.readAllBytes(new File(INDEX_FIXTURES, "index.json.minisig").toPath()),
            StandardCharsets.UTF_8);
        ModIndex index = ModIndex.read(body, signature, INDEX_TEST_KEY, 0, "android-arm64", 1, 14, BundledMods::jsonMap, false, "");
        assertNull("the same version: nothing to take", index.differing("ammo-max", "1.0.1", true));
        ModIndex.Offer newer = index.differing("ammo-max", "1.0.0", true);
        assertTrue(newer != null && "1.0.1".equals(newer.version));
        assertEquals("Update to 1.0.1", ModIndex.takeLabel("1.0.0", newer));
        assertEquals("Use 1.0.1, as OpenRAC's mod list has it", ModIndex.takeLabel("1.2.0", newer));
        assertNull("not offered for a mod the list lacks", index.differing("no-such-mod", "1.0.0", true));

        // Robolectric lists none of the APK's assets/mods (getAssets().list gives nothing here), so the built-ins are
        // made from manifests; PcSelfTest takes a built-in's place, writes it for the game and brings it back with the
        // release's real files (PcMods, the same rule).
        app.getSharedPreferences("mods", Context.MODE_PRIVATE).edit().clear().commit();
        BundledMods mods = new BundledMods(app);
        java.util.List<BundledMods.Mod> list = new java.util.ArrayList<>();
        for (String id : new String[] {"ammo-max", "button-swap", "rac-board"}) {
            list.add(BundledMods.parse("{\"format\": 2, \"id\": \"" + id + "\", \"name\": \"" + id + "\", \"type\": \"script\","
                + " \"version\": \"1.0.1\", \"api\": \"1.1\", \"scripts\": [\"main.luau\"], \"games\": {\"rac1\": [\"pal\"]}}"));
        }
        BundledMods.Mod builtIn = list.get(1);
        boolean wasOn = mods.on(builtIn);
        BundledMods.Mod taken = BundledMods.parse("{\"format\": 2, \"id\": \"button-swap\", \"name\": \"Button Swap\","
            + " \"type\": \"script\", \"version\": \"9.9.9\", \"api\": \"1.1\", \"scripts\": [\"main.luau\"],"
            + " \"games\": {\"rac1\": [\"pal\", \"ntsc\"]}}");
        taken.added = true;
        BundledMods.place(list, taken);
        assertEquals("one Button Swap, in its place", 3, list.size());
        assertSame(taken, list.get(1));
        assertEquals("1.0.1", taken.builtIn);
        assertEquals("it keeps the built-in's on or off", wasOn, mods.on(taken));
        mods.setOn(builtIn, true);
        assertTrue("the built-in's on is the list's one's", mods.on(taken));
        BundledMods.Mod board = BundledMods.parse("{\"format\": 2, \"id\": \"rac-board\", \"name\": \"RAC-Board\","
            + " \"type\": \"script\", \"version\": \"0.2.1\", \"api\": \"1.1\", \"scripts\": [\"main.luau\"]}");
        board.added = true;
        BundledMods.place(list, board);
        assertTrue("RAC-Board keeps this release's changes", list.get(2).builtIn == null && !list.get(2).added);
        BundledMods.Mod other = BundledMods.parse("{\"format\": 2, \"id\": \"swap\", \"name\": \"Swap\", \"type\": \"script\","
            + " \"api\": \"1.1\", \"scripts\": [\"main.luau\"]}");
        other.added = true;
        BundledMods.place(list, other);
        assertTrue("any other at the end", list.size() == 4 && list.get(3) == other && other.builtIn == null);
        assertTrue("an added mod with no built-in is on from its adding", mods.on(other));
    }

    @Test
    public void aModFileTheGameWouldRefuseIsNotKept() throws Exception {
        assertNull(BundledMods.refusal(new JSONObject("{\"id\": \"a\", \"api\": \"1." + BundledMods.API_MINOR + "\"}")));
        assertTrue(BundledMods.refusal(new JSONObject("{\"id\": \"a\", \"api\": \"1."
            + (BundledMods.API_MINOR + 1) + "\"}")).contains("mod interface"));
        assertTrue(BundledMods.refusal(new JSONObject("{\"id\": \"a\", \"api\": \"2.0\"}")).contains("mod interface"));
        assertTrue(BundledMods.refusal(new JSONObject("{\"id\": \"a\", \"api\": \"1.1\", \"plugin\": "
            + "{\"windows-x86_64\": \"a.dll\"}}")).contains("android-arm64"));
        assertTrue(BundledMods.refusal(new JSONObject("{\"id\": \"a\", \"api\": \"1.1\", \"games\": "
            + "{\"rac2\": [\"ntsc\"]}}")).contains("another game"));
        assertTrue(BundledMods.refusal(new JSONObject("{\"id\": \"../a\", \"api\": \"1.1\"}")).contains("id"));
        assertTrue(BundledMods.refusal(new JSONObject("{\"id\": \"quest-vr\", \"api\": \"1.1\"}")).contains("id"));

        BundledMods mods = new BundledMods(app);
        String[][] files = {
            {"readme.txt", "no manifest"},
            {"../outside.txt", "a path out of its folder"},
            {"openrac-mod.json", "{\"id\": \"a\", \"api\": \"1.1\", \"plugin\": {\"android-arm64\": \"lib/a.so\"}}"},
        };
        for (String[] file : files) {
            boolean refused = false;
            try {
                mods.add(new java.io.ByteArrayInputStream(zip(file)));
            } catch (java.io.IOException e) {
                refused = true;
            }
            assertTrue(file[0], refused);
        }
        assertTrue(!new File(new File(app.getFilesDir(), "added-mods"), "../outside.txt").exists());
        assertTrue(!new File(new File(app.getFilesDir(), "added-mods"), "a").exists());
    }

    @Test
    public void theLauncherKnowsTheGamesModInterface() throws Exception {
        String header = new String(Files.readAllBytes(
            new File("../../third_party/OpenRAC/sdk/include/openrac/openrac_mod.h").toPath()), StandardCharsets.UTF_8);
        assertTrue(header.contains("#define OPENRAC_MOD_API_MINOR " + BundledMods.API_MINOR + "\n"));
        assertTrue(header.contains("#define OPENRAC_MOD_API_MAJOR " + BundledMods.API_MAJOR + "\n"));
    }

    @Test
    public void theBuildBundlesTheModsTheLauncherKnows() throws Exception {
        String build = new String(Files.readAllBytes(new File("../../tools/build_quest.py").toPath()), StandardCharsets.UTF_8);
        assertTrue(build.contains("\"ammo-max\": \"ammo_max\""));
        assertTrue(build.contains("assets/mods/{mod_id}/openrac-mod.json"));
    }

    @Test
    public void theLevelFormatIsOpenRACs() throws Exception {
        String port = new String(Files.readAllBytes(new File("../../third_party/OpenRAC/editor/port.py").toPath()),
            StandardCharsets.UTF_8);
        assertTrue(port.contains("\nFORMAT = " + BuildConfig.LEVEL_FORMAT + "\n"));
        String level = new String(Files.readAllBytes(new File("../../third_party/OpenRAC/port/scene/level.h").toPath()),
            StandardCharsets.UTF_8);
        assertTrue(level.contains("kManifestFormat = " + BuildConfig.LEVEL_FORMAT + ";"));
    }

    @Test
    public void levelsOfAnOlderFormatAreMadeAgainFromTheKeptDisc() throws Exception {
        installedByAnOlderGame("pal");
        File kept = new Tools(app).keptDisc();
        kept.getParentFile().mkdirs();
        // The disc's size, reserved and never written.
        try (java.io.RandomAccessFile disc = new java.io.RandomAccessFile(kept, "rw")) {
            disc.setLength(Discs.PAL.size);
        }
        try {
            assertNull(Setup.installedVersion(app));
            assertEquals("pal", Setup.outdatedVersion(app));
            LauncherActivity activity = open();
            String words = allText(activity);
            assertTrue(words.contains("Update the levels"));
            assertTrue(words.contains("SCES-50916"));
            assertTrue(!words.contains("Ready to play"));
            shot(activity, "12-levels-outdated");
            button(activity, "UPDATE THE LEVELS").performClick();
            Intent service = Shadows.shadowOf(app).getNextStartedService();
            assertEquals(SetupService.class.getName(), service.getComponent().getClassName());
            assertTrue(service.getBooleanExtra(SetupService.EXTRA_LEVELS_ONLY, false));
            assertNull(service.getData());
            assertTrue(Setup.STATE.starting);
        } finally {
            Setup.STATE.starting = false;
            kept.delete();
        }
    }

    @Test
    public void levelsOfAnOlderFormatWithoutTheDiscAskForIt() throws Exception {
        installedByAnOlderGame("pal");
        assertNull(Setup.installedVersion(app));
        assertNull(Setup.outdatedVersion(app));
        LauncherActivity activity = open();
        String words = allText(activity);
        assertTrue(words.contains("First, your game disc"));
        assertTrue(!words.contains("Ready to play"));
    }

    @Test
    public void firstScreenSaysExactlyWhichDiscIsNeeded() throws Exception {
        LauncherActivity activity = open();
        String words = allText(activity);
        assertTrue(words.contains("SCES-50916"));
        assertTrue(words.contains("v2.00"));
        assertTrue(words.contains("4,214,784,000"));
        assertTrue(words.contains("SCUS-97199"));
        assertTrue(words.contains("v1.00"));
        assertTrue(words.contains("4,214,095,872"));
        assertTrue(words.contains("Greatest Hits"));
        shot(activity, "01-play-no-disc");
    }

    @Test
    public void choosingADiscOpensThePicker() {
        LauncherActivity activity = open();
        button(activity, "CHOOSE YOUR .ISO FILE").performClick();
        Intent next = Shadows.shadowOf(activity).getNextStartedActivity();
        assertEquals(Intent.ACTION_OPEN_DOCUMENT, next.getAction());
    }

    @Test
    public void everyTabDraws() throws Exception {
        LauncherActivity activity = open();
        String[] tabs = {"Play", "Graphics", "VR", "Game", "Controls", "Logs", "Credits"};
        for (int i = 0; i < tabs.length; i++) {
            button(activity, tabs[i]).performClick();
            shot(activity, String.format("0%d-%s", i + 2, tabs[i].toLowerCase()));
        }
        button(activity, "VR").performClick();
        assertTrue("the VR tab says what is only in the game", allText(activity).contains("Looking for Moves"));
        button(activity, "Controls").performClick();
        String controls = allText(activity);
        for (String words : new String[] {"Menus, and where every option is", "Hold your hand on the bottom row", "More up/down",
                                          "Wall run and jump", "Swingshot reach", "Wrench comes back", "Touch controllers"}) {
            assertTrue("the Controls tab shows " + words, controls.contains(words));
        }
        button(activity, "Game").performClick();
        String game = allText(activity);
        for (String words : new String[] {"Skip any movie", "Game volume", "Cheats", "OpenRAC's mods"}) {
            assertTrue("the Game tab shows " + words, game.contains(words));
        }
        button(activity, "Credits").performClick();
        String credits = allText(activity);
        for (String name : new String[] {"OpenRAC", "npiriou", "Lynder063", "Rerac-vr", "ReRAC", "Insomniac", "Sony",
                                         "rac1-mods"}) {
            assertTrue("credits name " + name, credits.contains(name));
        }
    }

    @Test
    public void smoothTurningIsTheDefault() {
        Config config = new Config(app);
        assertEquals("smooth", config.get(Config.TURN));
        assertEquals("first", config.get(Config.VIEW));
        assertEquals("true", config.get(Config.HAND_WEAPON));
    }

    @Test
    public void readyScreenOffersPlayAndTheTest() throws Exception {
        installed("pal");
        LauncherActivity activity = open();
        String words = allText(activity);
        assertTrue(words.contains("Ready to play"));
        assertTrue(words.contains("SCES-50916"));
        button(activity, "PLAY IN VR");
        button(activity, "Play in 2D");
        shot(activity, "08-ready");
    }

    @Test
    public void aFailureIsShownInPlainWords() throws Exception {
        Setup.STATE.failure = Discs.wrongSize(3_700_000_000L);
        LauncherActivity activity = open();
        String words = allText(activity);
        assertTrue(words.contains("3,700,000,000"));
        assertTrue(words.contains("not a supported disc"));
        shot(activity, "09-wrong-disc");
        Setup.STATE.failure = null;
    }

    @Test
    public void vrFailureFromLastRunIsShown() throws Exception {
        installed("pal");
        Tools tools = new Tools(app);
        tools.statusFile().getParentFile().mkdirs();
        Files.write(tools.statusFile().toPath(), "xrCreateSession: XR_ERROR_FORM_FACTOR_UNAVAILABLE\n".getBytes(StandardCharsets.UTF_8));
        LauncherActivity activity = open();
        assertTrue(allText(activity).contains("VR could not start"));
        assertTrue(allText(activity).contains("XR_ERROR_FORM_FACTOR_UNAVAILABLE"));
        shot(activity, "10-vr-failed");
        tools.statusFile().delete();
    }

    @Test
    public void progressShowsWhileSettingUp() throws Exception {
        Setup.STATE.starting = true;
        Setup.STATE.percent = 42;
        Setup.STATE.step = "Preparing the levels";
        Setup.STATE.detail = "level 10: 4412 meshes placed";
        LauncherActivity activity = open();
        assertTrue(allText(activity).contains("Setting up the game"));
        shot(activity, "11-progress");
        Setup.STATE.starting = false;
    }

    @Test
    public void discsAreKnownBySize() {
        assertEquals(Discs.PAL, Discs.bySize(4214784000L));
        assertEquals(Discs.NTSC, Discs.bySize(4214095872L));
        assertNull(Discs.bySize(4214095873L));
        assertEquals("openrac-rac1-pal", Discs.PAL.library);
        assertEquals("openrac-rac1-ntsc", Discs.NTSC.library);
    }

    @Test
    public void presetsSetEverything() {
        Config config = new Config(app);
        config.applyPreset("performance");
        assertEquals("0.8", config.get(Config.RESOLUTION));
        assertEquals("1", config.get(Config.MSAA));
        config.applyPreset("quality");
        assertEquals("1.2", config.get(Config.RESOLUTION));
        assertEquals("4", config.get(Config.MSAA));
    }

    @Test
    public void everyOptionsDefaultIsOneOfItsValues() {
        for (Config.Choice[] group : Arrays.asList(Config.GRAPHICS, Config.VR)) {
            for (Config.Choice choice : group) {
                assertTrue(choice.key, Arrays.asList(choice.values).contains(choice.fallback));
                assertEquals(choice.key, choice.names.length, choice.values.length);
            }
        }
    }

    @Test
    public void pluginListMatchesTheModsSettings() throws Exception {
        File manifestFile = new File("src/main/assets/../../../../build/stage/assets/openrac-mod.json");
        JSONObject manifest = new JSONObject(new String(Files.readAllBytes(manifestFile.toPath()), StandardCharsets.UTF_8));
        List<String> keys = new ArrayList<>();
        for (int i = 0; i < manifest.getJSONArray("settings").length(); i++) {
            keys.add(manifest.getJSONArray("settings").getJSONObject(i).getString("key"));
        }
        for (Config.Choice choice : Config.VR) {
            assertTrue("the mod has " + choice.key, keys.contains(choice.key));
        }
        assertTrue(keys.contains("refresh_hz"));
        assertTrue(keys.contains("resolution"));
        for (String key : new String[] {Config.FOVEATION.key, Config.SPACE_WARP.key, Config.CPU_GPU_BOOST.key}) {
            assertTrue("the mod has " + key, keys.contains(key));
        }
        // "Hide Ratchet's body" is gone (0.1.13): Off broke first person (a tester's report). Neither the mod nor the
        // launcher may have it again.
        assertFalse("the mod has no hide_body", keys.contains("hide_body"));
        for (Config.Choice choice : Config.VR) {
            assertNotEquals("hide_body", choice.key);
        }
        assertEquals("smooth", manifest.getJSONArray("settings").getJSONObject(keys.indexOf("turn")).getString("default"));
        assertTrue(manifest.getJSONObject("plugin").has("android-arm64"));
    }
}
