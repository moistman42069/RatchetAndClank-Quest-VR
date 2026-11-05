// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
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

    /** What's new (0.1.13): on a version's first start, biggest first (Swingshot anywhere), once; Updates shows it again. */
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
        int anywhere = text.indexOf("Swingshot anywhere"), akimbo = text.indexOf("Akimbo is on for everyone");
        assertTrue("Swingshot anywhere comes first", anywhere >= 0 && akimbo > anywhere);
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

        // A preset sets what it names and puts the others back.
        mods.set(mod, mod.settings.get(1), "a");
        mods.apply(mod, mod.presets.get(0));
        JSONObject hard = mods.settings(mod);
        assertEquals(4, hard.get("count"));
        assertEquals(2.0, hard.getDouble("scale"), 0.0);
        assertEquals("b", hard.get("mode"));

        // A stored value the manifest no longer offers, or too long, is not written.
        mods.set(mod, count, "9");
        mods.set(mod, mod.settings.get(4), "abcdefghij");
        assertEquals(2, mods.settings(mod).get("count"));
        assertEquals("abcdefgh", mods.settings(mod).get("seed"));
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
