// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.math.BigDecimal;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

/**
 * The PC launcher's checks, run on the release's folder (RatchetClankVR.exe --self-test; tools/build_pc.py --package
 * runs them on what it built): what the headset's LauncherTest checks of the same code, against the release's own
 * files. Nothing here reaches the network or the player's folders: each check works in a folder of its own.
 */
final class PcSelfTest {
    private static int failed = 0;
    private static int passed = 0;

    private PcSelfTest() {}

    private static void check(boolean ok, String what) {
        if (ok) {
            passed++;
        } else {
            failed++;
            System.out.println("FAILED: " + what);
        }
    }

    static int run(PcPaths release) {
        PcUpdater.network = false;
        File scratch = null;
        try {
            scratch = Files.createTempDirectory("ratchetvr-selftest").toFile();
            PcPaths paths = new PcPaths(release.install, new File(scratch, "home"), new File(scratch, "logs"));
            json();
            options(paths);
            writeForGame(paths);
            mods(paths);
            setup(paths);
            updates(paths, scratch);
            release(paths);
            shortcut(paths, scratch);
        } catch (Exception e) {
            failed++;
            System.out.println("FAILED: " + e);
            e.printStackTrace(System.out);
        } finally {
            PcPaths.deleteRecursively(scratch);
        }
        System.out.println(failed == 0 ? "all " + passed + " checks passed" : failed + " of " + (passed + failed)
            + " checks failed");
        return failed == 0 ? 0 : 1;
    }

    // Create shortcut (0.1.18): a .lnk made where asked, to the release's exe (only the release has one).
    private static void shortcut(PcPaths paths, File scratch) throws IOException {
        File exe = new File(paths.install, PcPaths.EXE);
        if (!exe.isFile()) {
            System.out.println("(no " + PcPaths.EXE + " here: the shortcut is checked in the release)");
            return;
        }
        File link = new File(scratch, "test shortcut & play.lnk");
        File made = PcLauncher.writeShortcut(exe, PcLauncher.PLAY_VR, link, PcLauncher.SHORTCUT_VR);
        check(made.isFile() && made.getCanonicalFile().equals(link.getCanonicalFile()) && made.length() > 0,
            "a shortcut is made where asked");
    }

    private static void json() {
        Map<String, Object> object = Json.object("{\"a\": [1, 2.50, -3e2, true, null], \"b\": \"\\u00e9\\n\\\"x\\\"\","
            + " \"c\": {}}");
        List<Object> a = Json.array(object.get("a"));
        check(a.size() == 5 && a.get(0).equals(new BigDecimal("1")) && Json.plain(a.get(1)).equals("2.5")
            && Json.plain(a.get(2)).equals("-300") && Boolean.TRUE.equals(a.get(3)) && a.get(4) == null, "JSON arrays");
        check("é\n\"x\"".equals(object.get("b")), "JSON text escapes");
        // Numbers are written plainly (2.50 as 2.5, -3e2 as -300), so the text is what must come back the same.
        String written = Json.write(object);
        check(Json.write(Json.object(written)).equals(written) && written.contains("2.5") && written.contains("-300")
            && Json.object(written).get("b").equals(object.get("b")), "JSON written reads back the same");
        check(Json.write(140.0).equals("140") && Json.write(0.8).equals("0.8"), "JSON doubles as org.json writes them");
        boolean refused = false;
        try {
            Json.parse("{\"a\": 1,}");
        } catch (IllegalArgumentException e) {
            refused = true;
        }
        check(refused, "JSON refuses a trailing comma");
    }

    private static void options(PcPaths paths) throws IOException {
        PcConfig config = new PcConfig(paths);
        for (Options.Choice[] group : new Options.Choice[][] {Options.GRAPHICS, Options.VR, Options.GAME}) {
            for (Options.Choice choice : group) {
                check(config.get(choice).equals(choice.fallback), choice.key + " starts at its default");
                check(java.util.Arrays.asList(choice.values).contains(choice.fallback), choice.key + "'s default is offered");
            }
        }
        check(config.gameSettings().contains("frame_rate = unlimited\n"), "the headset paces the frames");
        check(config.get(Options.KEEP_REFRESH).equals("true"), "Keep the refresh rate is on");
        config.set(Options.TURN, "snap");
        check(new PcConfig(paths).get(Options.TURN).equals("snap"), "a choice is kept");
        config.set(Options.TURN, "sideways");
        check(new PcConfig(paths).get(Options.TURN).equals("smooth"), "a value no longer offered is the default");
        config.applyPreset("quality");
        check(config.get(Options.RESOLUTION).equals("1.2") && config.get(Options.MSAA).equals("4"), "the Quality preset");
        config.applyPreset("balanced");

        // Every setting written for the VR mod is one its manifest declares, in its type, and the other way round.
        Map<String, Object> manifest = Json.object(PcMods.read(new File(paths.vrMod(), "openrac-mod.json")));
        Map<String, String> types = new java.util.LinkedHashMap<>();
        for (Object one : Json.array(manifest.get("settings"))) {
            Map<String, Object> s = Json.object(one);
            types.put(Json.string(s, "key", ""), Json.string(s, "type", ""));
        }
        Map<String, Object> written = config.modSettings(false);
        check(written.keySet().equals(types.keySet()), "the settings written are the manifest's: " + written.keySet()
            + " against " + types.keySet());
        for (Map.Entry<String, Object> setting : written.entrySet()) {
            String type = types.get(setting.getKey());
            Object value = setting.getValue();
            boolean fits = type == null ? false : type.equals("bool") ? value instanceof Boolean
                : type.equals("int") ? value instanceof Integer : type.equals("float") ? value instanceof Double
                : value instanceof String;
            check(fits, setting.getKey() + " is written as its manifest's " + type);
        }
        check("screen".equals(config.modSettings(true).get("view")), "2D mode is the big screen");
    }

    private static void writeForGame(PcPaths paths) throws IOException {
        new PcConfig(paths).writeForGame(false);
        File folder = new File(paths.plugins(), Options.MOD_ID);
        check(new File(folder, "lib/windows-x86_64/quest_vr.dll").isFile(), "the mod in the plugins folder");
        check(new File(folder, "lib/windows-x86_64/openxr_loader.dll").isFile(), "the OpenXR loader beside it");
        check(new File(folder, "openrac-mod.json").isFile(), "its manifest");
        Map<String, Object> list = Json.object(PcMods.read(new File(paths.plugins(), "plugins.json")));
        List<Object> mods = Json.array(list.get("mods"));
        Map<String, Object> first = Json.object(mods.get(0));
        check(Json.string(first, "id", "").equals("quest-vr") && Boolean.TRUE.equals(first.get("enabled")),
            "the VR mod is first and on");
        check(new BigDecimal("1").equals(list.get("format")), "the list's format");
        boolean ammo = false;
        for (Object one : mods) {
            ammo |= Json.string(Json.object(one), "id", "").equals("ammo-max");
        }
        check(ammo, "Ammo max is on by default");
        String ini = PcMods.read(paths.settingsFile());
        check(ini.contains("vsync = off\n") && ini.contains("volume = 100\n"), "the game's settings file");
    }

    private static void mods(PcPaths paths) throws IOException {
        PcMods mods = new PcMods(paths);
        List<PcMods.Mod> list = mods.list();
        Set<String> ids = new HashSet<>();
        for (PcMods.Mod mod : list) {
            ids.add(mod.id);
            check(!mod.added, mod.id + " came with the release");
        }
        for (String id : new String[] {"difficulty", "item-randomizer", "ammo-max", "xbox-icons", "button-swap", "multiplayer",
                                       "rac-board"}) {
            check(ids.contains(id), "the release carries " + id + " (" + ids + ")");
        }
        for (PcMods.Mod mod : list) {
            if (mod.id.equals("multiplayer") || mod.id.equals("rac-board")) {
                check(!mods.on(mod), mod.id + " is off until the player turns it on");
            }
            if (mod.id.equals("rac-board")) {
                check(new File(mod.folder, "assets/board.glb").isFile() && new File(mod.folder, "LICENSE").isFile(),
                    "RAC-Board's models and licence came with it");
            }
        }
        for (PcMods.Mod mod : list) {
            File manifest = new File(mod.folder, "openrac-mod.json");
            Map<String, Object> plugin = Json.object(Json.object(PcMods.read(manifest)).get("plugin"));
            if (plugin != null) {
                File library = new File(mod.folder, Json.string(plugin, PcMods.PLATFORM, "?"));
                check(library.isFile(), mod.id + "'s Windows build is there");
            }
        }

        check(PcMods.refusal(Json.object("{\"id\": \"x\", \"api\": \"1.11\", \"plugin\": {\"android-arm64\": \"a.so\"}}"))
            .contains("no build for Windows"), "a headset-only mod is refused");
        check(String.valueOf(PcMods.refusal(Json.object("{\"id\": \"x\", \"api\": \"1.15\"}"))).contains("1.14"),
            "a mod for a newer interface is refused");
        check(PcMods.refusal(Json.object("{\"id\": \"quest-vr\", \"api\": \"1.0\"}")) != null, "quest-vr's id is refused");
        check(PcMods.refusal(Json.object("{\"id\": \"script\", \"api\": \"1.3\", \"games\": {\"rac1\": [\"pal\"]}}")) == null,
            "a script mod is taken");

        ByteArrayOutputStream zip = new ByteArrayOutputStream();
        try (ZipOutputStream out = new ZipOutputStream(zip)) {
            out.putNextEntry(new ZipEntry("my-mod/openrac-mod.json"));
            out.write(("{\"id\": \"my-mod\", \"name\": \"My mod\", \"api\": \"1.11\", \"plugin\": {\"windows-x86_64\": "
                + "\"lib/windows-x86_64/my.dll\"}, \"settings\": [{\"key\": \"n\", \"type\": \"float\", \"min\": 0, "
                + "\"max\": 1, \"step\": 0.25, \"default\": 0.5}]}").getBytes(StandardCharsets.UTF_8));
            out.putNextEntry(new ZipEntry("my-mod/lib/windows-x86_64/my.dll"));
            out.write(new byte[] {'M', 'Z'});
        }
        check("My mod".equals(mods.add(new ByteArrayInputStream(zip.toByteArray()))), "a mod is added from its file");
        PcMods.Mod added = null;
        for (PcMods.Mod mod : mods.list()) {
            if (mod.id.equals("my-mod")) {
                added = mod;
            }
        }
        check(added != null && added.added && mods.on(added), "an added mod is listed and on");
        if (added != null) {
            check(mods.get(added, added.settings.get(0)).equals("0.5")
                && added.settings.get(0).values.length == 5, "its float setting's steps");
            new PcConfig(paths).writeForGame(false);
            check(new File(paths.plugins(), "my-mod/lib/windows-x86_64/my.dll").isFile(), "it goes in the plugins folder");
            mods.remove(added);
            check(!new File(paths.addedMods(), "my-mod").exists() && !new File(paths.plugins(), "my-mod").exists(),
                "a removed mod is gone");
        }

        ByteArrayOutputStream escape = new ByteArrayOutputStream();
        try (ZipOutputStream out = new ZipOutputStream(escape)) {
            out.putNextEntry(new ZipEntry("../outside.txt"));
            out.write(1);
        }
        boolean refused = false;
        try {
            mods.add(new ByteArrayInputStream(escape.toByteArray()));
        } catch (IOException e) {
            refused = e.getMessage().contains("leaves its folder");
        }
        check(refused && !new File(paths.home, "outside.txt").exists(), "a zip that leaves its folder is refused");
    }

    private static void setup(PcPaths paths) throws IOException {
        check(PcSetup.installedVersion(paths) == null, "nothing set up yet");
        PcSetup.prefs(paths).putString(PcSetup.KEY_VERSION, "pal");
        check(PcSetup.installedVersion(paths) == null, "no levels checked, not set up");
        File levels = paths.levels("pal");
        levels.mkdirs();
        Files.write(new File(levels, "verified-" + (PcBuild.LEVEL_FORMAT - 1) + ".json").toPath(), new byte[] {'{', '}'});
        check(PcSetup.installedVersion(paths) == null, "levels of another format are not set up");
        Files.write(new File(levels, PcSetup.checkStamp()).toPath(), new byte[] {'{', '}'});
        check("pal".equals(PcSetup.installedVersion(paths)), "set up once the levels passed their check");
        check(Discs.bySize(Discs.PAL.size) == Discs.PAL && Discs.bySize(123) == null, "discs by size");
        check(paths.cards(Discs.PAL).getPath().endsWith("memcard" + File.separator + "SCES-50916"),
            "the cards in the launcher's own folder");
        check(!paths.cards(Discs.PAL).getPath().toLowerCase(java.util.Locale.ROOT).contains("\\openrac\\"),
            "never OpenRAC's own folder");
    }

    private static void updates(PcPaths paths, File scratch) throws Exception {
        check("RatchetQuestVR-1.2.3-windows.zip".equals(UpdatePolicy.pcAsset("v1.2.3")), "the release's zip's name");
        check(UpdatePolicy.pcAsset("1.2.3") == null && UpdatePolicy.pcAsset("v1.2") == null, "only vX.Y.Z tags");
        check(UpdatePolicy.newer("v" + PcBuild.VERSION.replaceFirst("\\d+$", "999"), PcBuild.VERSION)
            && !UpdatePolicy.newer("v" + PcBuild.VERSION, PcBuild.VERSION), "newer versions");
        check(!UpdatePolicy.allowedUrl("http://github.com/x") && !UpdatePolicy.allowedUrl("https://example.com/x"),
            "only GitHub over HTTPS");

        // The install script on a folder made for it: an old release, a new one, a process it waits for.
        File install = new File(scratch, "install folder & (old)");
        new File(install, "app").mkdirs();
        Files.write(new File(install, "app/old.jar").toPath(), new byte[] {1});
        Files.write(new File(install, "keep-mine.txt").toPath(), new byte[] {1});
        File fresh = new File(scratch, "new release");
        new File(fresh, "app").mkdirs();
        new File(fresh, "openrac/python").mkdirs();
        Files.write(new File(fresh, "app/new.jar").toPath(), new byte[] {2});
        Files.write(new File(fresh, "openrac/python/python.exe").toPath(), new byte[] {3});
        Files.write(new File(fresh, PcPaths.EXE).toPath(), new byte[] {4});
        PcPaths at = new PcPaths(install, paths.home, paths.logs);
        Process waited = new ProcessBuilder("ping", "-n", "3", "127.0.0.1").redirectErrorStream(true)
            .redirectOutput(ProcessBuilder.Redirect.DISCARD).start();
        long started = System.nanoTime();
        File script = PcUpdater.writeScript(at, fresh, waited.pid(), false);
        Process apply = PcUpdater.runScript(script);
        check(apply.waitFor(60, TimeUnit.SECONDS) && apply.exitValue() == 0, "the install script ran");
        check(!waited.isAlive() && System.nanoTime() - started > 1_000_000_000L, "it waited for the launcher to close");
        check(new File(install, "app/new.jar").isFile() && !new File(install, "app/old.jar").exists(),
            "the release's folders replaced whole");
        check(new File(install, "openrac/python/python.exe").isFile() && new File(install, PcPaths.EXE).isFile(),
            "its new folders and files in place");
        check(new File(install, "keep-mine.txt").isFile(), "a file of the player's beside them stays");
        check(!fresh.exists(), "what was unpacked is cleared");
        check(PcUpdater.lastInstall(at).isEmpty(), "the install says it finished");
    }

    private static void release(PcPaths paths) {
        for (Discs.Disc disc : Discs.ALL) {
            check(paths.game(disc.version).isFile(), "the release has the game for " + disc.region);
        }
        check(paths.python().isFile(), "the release has the tools' Python");
        check(new File(paths.root(), "tools/extractor.py").isFile() && new File(paths.root(), "editor/extract.py").isFile(),
            "the release has the set-up's tools");
        for (String name : new String[] {"whats_new.txt", "controls.txt", "menus.txt", "pc_controls.txt", "credits.txt",
                "third_party_notices.txt"}) {
            check(new File(paths.texts(), name).isFile(), "the release has " + name);
        }
        for (String name : new String[] {"vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll", "msvcp140_2.dll"}) {
            check(new File(paths.root(), "port/build/release/" + name).isFile(), "the game's runtime " + name + " beside it");
        }
    }
}
