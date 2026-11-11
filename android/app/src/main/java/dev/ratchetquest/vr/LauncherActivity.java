// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.app.Activity;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputFilter;
import android.text.InputType;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.function.Consumer;

/** The launcher: a 2D panel on the Quest. Choose the disc once, change settings, play. */
public final class LauncherActivity extends Activity {
    private static final int REQUEST_DISC = 0x5243;
    private static final int REQUEST_MOD = 0x5244;
    /** What became of the last mod file picked, shown on the Game tab. */
    private String modNote = "";
    /**
     * OpenRAC's mod list once read (ModIndex: by Browse, or at the start with Automatic checks on, for the mods' Update
     * buttons), what Browse is doing, and the mod being added from it.
     */
    private volatile ModIndex modIndex;
    private String browseNote = "";
    private boolean browsing = false, reading = false;
    private String adding = "";
    /** The Game tab, with the mods (showTab). */
    private static final int GAME_TAB = 3;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private LinearLayout page;
    private final List<Button> tabs = new ArrayList<>();
    private int tab = 0;
    private Config config;
    private BundledMods mods;
    private Tools tools;
    private ProgressBar progress;
    private TextView progressText, progressDetail;
    private boolean watching;

    private final Runnable watcher = new Runnable() {
        @Override
        public void run() {
            if (!watching) {
                return;
            }
            if (progress != null) {
                progress.setProgress(Setup.STATE.percent);
                progressText.setText(Setup.STATE.step + "  " + Setup.STATE.percent + "%");
                progressDetail.setText(Setup.STATE.detail);
            }
            if (!Setup.STATE.running && !Setup.STATE.starting) {
                watching = false;
                showTab(0);
                return;
            }
            handler.postDelayed(this, 300);
        }
    };

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        config = new Config(this);
        mods = new BundledMods(this);
        tools = new Tools(this);
        buildFrame();
        // What the last run left goes to Download, on a thread of its own: logcat takes a moment.
        new Thread(() -> RunLog.saveLastRun(this), "last-run-log").start();
        showTab(0);
        showWhatsNewOnce();
        Updater.maybeCheck(this, this::updatesChanged);
        // OpenRAC's mod list, for each mod's Update button (the Game tab), with Automatic checks on, as the APK's own.
        if (Updater.network && Updater.automatic(this)) {
            readModIndex();
        }
    }

    // ---- What's new ----

    static final String WHATS_NEW_PREFS = "whats_new";
    static final String KEY_WHATS_NEW_SHOWN = "shown";

    /** The first start of a version (a new install or an update): what it brought, biggest first, once. */
    private void showWhatsNewOnce() {
        SharedPreferences seen = getSharedPreferences(WHATS_NEW_PREFS, MODE_PRIVATE);
        if (BuildConfig.VERSION_NAME.equals(seen.getString(KEY_WHATS_NEW_SHOWN, ""))) {
            return;
        }
        seen.edit().putString(KEY_WHATS_NEW_SHOWN, BuildConfig.VERSION_NAME).apply();
        showWhatsNew();
    }

    /** whats_new.txt in a box over the launcher, until Got it. The Play tab's Updates card shows it again. */
    Dialog showWhatsNew() {
        final Dialog dialog = new Dialog(this);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setBackground(plate(Artwork.NAVY, Artwork.ORANGE, 14));
        box.setPadding(dp(24), dp(20), dp(24), dp(18));
        box.addView(text("What's new in " + BuildConfig.VERSION_NAME, 22, Artwork.ORANGE, true));
        TextView words = text(readAsset("whats_new.txt").trim(), 15, Artwork.INK, false);
        words.setPadding(0, dp(10), 0, dp(14));
        ScrollView scroll = new ScrollView(this);
        scroll.addView(words);
        box.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(460)));
        box.addView(button("Got it", true, view -> dialog.dismiss()),
            new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        dialog.setContentView(box);
        Window window = dialog.getWindow();
        if (window != null) {
            window.setBackgroundDrawable(new ColorDrawable(0));
            window.setLayout(dp(820), ViewGroup.LayoutParams.WRAP_CONTENT);
        }
        dialog.show();
        return dialog;
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (tab == 0) {
            showTab(0);
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        watching = false;
    }

    // ---- The frame ----

    private int dp(float value) {
        return Math.round(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics()));
    }

    private TextView text(String words, float size, int colour, boolean bold) {
        TextView view = new TextView(this);
        view.setText(words);
        view.setTextSize(size);
        view.setTextColor(colour);
        view.setLineSpacing(dp(2), 1f);
        if (bold) {
            view.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        }
        return view;
    }

    private GradientDrawable plate(int fill, int edge, float radius) {
        GradientDrawable plate = new GradientDrawable();
        plate.setColor(fill);
        plate.setStroke(dp(1), edge);
        plate.setCornerRadius(dp(radius));
        return plate;
    }

    private Button button(String label, boolean primary, View.OnClickListener action) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        button.setTextSize(primary ? 20 : 16);
        button.setTextColor(primary ? Artwork.NAVY : Artwork.INK);
        button.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        button.setBackground(plate(primary ? Artwork.ORANGE : 0xff1b3157, primary ? 0xffffd9a0 : 0xff46638c, 10));
        button.setPadding(dp(18), dp(10), dp(18), dp(10));
        button.setOnClickListener(action);
        return button;
    }

    private LinearLayout card() {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setBackground(plate(Artwork.PANEL, 0x5546638c, 12));
        card.setPadding(dp(20), dp(16), dp(20), dp(16));
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.bottomMargin = dp(12);
        card.setLayoutParams(params);
        return card;
    }

    private void buildFrame() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackground(new Artwork.Space());
        root.setPadding(dp(24), dp(18), dp(24), dp(14));

        LinearLayout header = new LinearLayout(this);
        header.setGravity(Gravity.CENTER_VERTICAL);
        ImageView bolt = new ImageView(this);
        bolt.setImageDrawable(new Artwork.Bolt());
        header.addView(bolt, new LinearLayout.LayoutParams(dp(46), dp(46)));
        LinearLayout titles = new LinearLayout(this);
        titles.setOrientation(LinearLayout.VERTICAL);
        titles.setPadding(dp(12), 0, 0, 0);
        titles.addView(text("RATCHET & CLANK", 26, Artwork.ORANGE, true));
        titles.addView(text("Quest VR  ·  unofficial fan port  ·  v" + BuildConfig.VERSION_NAME, 13, Artwork.MUTED, false));
        header.addView(titles);
        root.addView(header);

        LinearLayout bar = new LinearLayout(this);
        bar.setPadding(0, dp(12), 0, dp(10));
        String[] names = {"Play", "Graphics", "VR", "Game", "Controls", "Logs", "Credits"};
        for (int i = 0; i < names.length; i++) {
            final int index = i;
            Button tabButton = button(names[i], false, view -> showTab(index));
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1);
            params.rightMargin = dp(6);
            bar.addView(tabButton, params);
            tabs.add(tabButton);
        }
        root.addView(bar);

        ScrollView scroll = new ScrollView(this);
        page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        scroll.addView(page);
        root.addView(scroll, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1));
        setContentView(root);
    }

    private void showTab(int index) {
        tab = index;
        for (int i = 0; i < tabs.size(); i++) {
            tabs.get(i).setBackground(plate(i == index ? Artwork.ORANGE_DARK : 0xff1b3157,
                i == index ? Artwork.ORANGE : 0xff46638c, 10));
        }
        page.removeAllViews();
        switch (index) {
            case 0: showPlay(); break;
            case 1: showChoices(Config.GRAPHICS); break;
            case 2: showInGameOptions(); showChoices(Config.VR); break;
            case GAME_TAB: showGame(); break;
            case 4: showControls(); break;
            case 5: showLogs(); break;
            default: showCredits(); break;
        }
    }

    // ---- Play ----

    private void showPlay() {
        showPlayCards();
        // Not while the game is being set up: an install would stop it.
        if (!Setup.STATE.running && !Setup.STATE.starting) {
            showUpdates();
        }
    }

    // ---- Updates ----

    private TextView updateText;

    private final Runnable updateWatcher = new Runnable() {
        @Override
        public void run() {
            if (updateText != null) {
                updateText.setText(updateLine());
            }
            if (Updater.STATE.busy) {
                handler.postDelayed(this, 300);
            }
        }
    };

    private String updateLine() {
        String line = Updater.STATE.percent >= 0 ? "Downloading: " + Updater.STATE.percent + "%" : Updater.status(this);
        return line + "\nInstalled: " + BuildConfig.VERSION_NAME
            + ". The launcher asks GitHub for this project's releases; nothing of yours is sent.";
    }

    private void showUpdates() {
        LinearLayout card = card();
        card.addView(text("Updates", 16, Artwork.INK, true));
        updateText = text(updateLine(), 14, Artwork.MUTED, false);
        card.addView(updateText);
        LinearLayout row = new LinearLayout(this);
        row.setPadding(0, dp(8), 0, 0);
        String tag = Updater.STATE.tag;
        if (tag != null) {
            String label = (Updater.STATE.ready ? "Install " : "Download and install ") + tag.substring(1);
            addToRow(row, button(label, false, view -> {
                Updater.download(this, this::updatesChanged);
                handler.post(updateWatcher);
            }), 2);
        }
        addToRow(row, button("Check now", false, view -> {
            Updater.check(this, this::updatesChanged);
            handler.post(updateWatcher);
        }), 1);
        addToRow(row, button("Automatic checks: " + (Updater.automatic(this) ? "On" : "Off"), false, view -> {
            Updater.setAutomatic(this, !Updater.automatic(this));
            showTab(0);
        }), 1);
        addToRow(row, button("What's new", false, view -> showWhatsNew()), 1);
        card.addView(row);
        page.addView(card);
    }

    private void addToRow(LinearLayout row, Button button, float weight) {
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, weight);
        params.rightMargin = dp(6);
        row.addView(button, params);
    }

    /** A check or a download ended: the Play tab drawn again, unless something else is on it. */
    private void updatesChanged() {
        if (tab == 0 && !isFinishing() && !Setup.STATE.running && !Setup.STATE.starting) {
            showTab(0);
        }
    }

    private void showPlayCards() {
        String version = Setup.installedVersion(this);
        File status = tools.statusFile();
        if (status.isFile()) {
            LinearLayout card = card();
            card.addView(text("VR could not start last time", 18, Artwork.BAD, true));
            card.addView(text(read(status).trim(), 15, Artwork.INK, false));
            card.addView(text("The full report is in Download/RatchetQuestVR.", 13, Artwork.MUTED, false));
            page.addView(card);
        }
        if (Setup.STATE.running || Setup.STATE.starting) {
            showProgress();
            return;
        }
        if (Setup.STATE.failure != null) {
            LinearLayout card = card();
            card.addView(text("That did not work", 18, Artwork.BAD, true));
            card.addView(text(Setup.STATE.failure, 15, Artwork.INK, false));
            page.addView(card);
        }
        if (version == null) {
            String outdated = Setup.outdatedVersion(this);
            if (outdated != null) {
                showLevelsOutdated(Discs.byVersion(outdated));
            } else {
                showDiscNeeded();
            }
            return;
        }

        Discs.Disc disc = Discs.byVersion(version);
        LinearLayout card = card();
        card.addView(text("Ready to play", 22, Artwork.GOOD, true));
        card.addView(text(Discs.TITLE + "\n" + disc.line(), 15, Artwork.INK, false));
        View gap = new View(this);
        card.addView(gap, new LinearLayout.LayoutParams(1, dp(14)));
        card.addView(button("PLAY IN VR", true, view -> play(disc, true)));
        page.addView(card);

        LinearLayout test = card();
        test.addView(text("2D mode", 16, Artwork.INK, true));
        test.addView(text("The game on a big flat screen in the headset, played with your Touch controllers as a "
            + "PlayStation pad: no gamepad needed. Also tells a problem with the game from a problem with VR.",
            14, Artwork.MUTED, false));
        test.addView(button("Play in 2D", false, view -> play(disc, false)));
        page.addView(test);

        LinearLayout other = card();
        other.addView(text("Another disc", 16, Artwork.INK, true));
        other.addView(text("Sets the game up again from another supported disc. Saves are kept.", 14, Artwork.MUTED, false));
        other.addView(button("Choose another disc image", false, view -> chooseDisc()));
        page.addView(other);
    }

    private void showDiscNeeded() {
        LinearLayout card = card();
        card.addView(text("First, your game disc", 22, Artwork.ORANGE, true));
        card.addView(text(Discs.REQUIREMENT, 15, Artwork.INK, false));
        View gap = new View(this);
        card.addView(gap, new LinearLayout.LayoutParams(1, dp(14)));
        card.addView(button("CHOOSE YOUR .ISO FILE", true, view -> chooseDisc()));
        page.addView(card);
    }

    private void showLevelsOutdated(Discs.Disc disc) {
        LinearLayout card = card();
        card.addView(text("Update the levels", 22, Artwork.ORANGE, true));
        card.addView(text("This version of the game reads its levels in a newer form. They are made again from "
            + "the disc already on the headset: no need to choose it again, and saves are kept.\n\n"
            + Discs.TITLE + "\n" + disc.line(), 15, Artwork.INK, false));
        View gap = new View(this);
        card.addView(gap, new LinearLayout.LayoutParams(1, dp(14)));
        card.addView(button("UPDATE THE LEVELS", true, view -> updateLevels()));
        page.addView(card);

        LinearLayout other = card();
        other.addView(text("Another disc", 16, Artwork.INK, true));
        other.addView(text("Sets the game up from another supported disc instead.", 14, Artwork.MUTED, false));
        other.addView(button("Choose another disc image", false, view -> chooseDisc()));
        page.addView(other);
    }

    private void updateLevels() {
        Setup.STATE.starting = true;
        Setup.STATE.failure = null;
        Setup.STATE.percent = 0;
        Setup.STATE.step = "Starting";
        Setup.STATE.detail = "";
        try {
            SetupService.startLevels(this);
        } catch (RuntimeException e) {
            Setup.STATE.starting = false;
            Setup.STATE.failure = "The update could not start: " + e.getMessage();
        }
        showTab(0);
    }

    private void showProgress() {
        LinearLayout card = card();
        card.addView(text("Setting up the game", 22, Artwork.ORANGE, true));
        progressText = text(Setup.STATE.step, 16, Artwork.INK, true);
        card.addView(progressText);
        progress = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progress.setMax(100);
        progress.setProgress(Setup.STATE.percent);
        card.addView(progress);
        progressDetail = text(Setup.STATE.detail, 13, Artwork.MUTED, false);
        card.addView(progressDetail);
        card.addView(text("Keep the app open. It only happens once.", 13, Artwork.MUTED, false));
        page.addView(card);
        watching = true;
        handler.post(watcher);
    }

    private void chooseDisc() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        pick.setType("*/*");
        pick.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {"application/x-iso9660-image", "application/octet-stream", "*/*"});
        try {
            startActivityForResult(pick, REQUEST_DISC);
        } catch (ActivityNotFoundException e) {
            Setup.STATE.failure = "This headset has no file picker to choose the disc with.";
            showTab(0);
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == REQUEST_MOD && result == RESULT_OK && data != null && data.getData() != null) {
            addMod(data.getData());
            return;
        }
        if (request != REQUEST_DISC || result != RESULT_OK || data == null || data.getData() == null) {
            return;
        }
        Uri image = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(image, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException e) {
            // A one-time grant is enough while the app stays open.
        }
        Setup.STATE.starting = true;
        Setup.STATE.failure = null;
        Setup.STATE.percent = 0;
        Setup.STATE.step = "Starting";
        Setup.STATE.detail = "";
        try {
            SetupService.start(this, image);
        } catch (RuntimeException e) {
            Setup.STATE.starting = false;
            Setup.STATE.failure = "The set-up could not start: " + e.getMessage();
        }
        showTab(0);
    }

    private void play(Discs.Disc disc, boolean vr) {
        // After an update the tools are unpacked again: seconds, away from the page's thread.
        new Thread(() -> {
            try {
                tools.prepare();
                config.writeForGame(!vr);
                tools.statusFile().delete();
                new File(tools.data(), "quest-run-pending").createNewFile();
                runOnUiThread(() -> startGame(disc, vr));
            } catch (IOException e) {
                runOnUiThread(() -> {
                    Setup.STATE.failure = "The game could not be started: " + e.getMessage();
                    showTab(0);
                });
            }
        }, "game-start").start();
    }

    private void startGame(Discs.Disc disc, boolean vr) {
        List<String> arguments = new ArrayList<>(java.util.Arrays.asList(
            "--data", tools.data().getPath(),
            "--window",
            "--levels", tools.levels(disc.version).getPath(),
            "--settings", tools.settingsFile().getPath(),
            "--plugins", new File(tools.plugins(), "plugins.json").getPath()));
        List<String> environment = new ArrayList<>();
        for (Map.Entry<String, String> entry : tools.environment().entrySet()) {
            environment.add(entry.getKey() + "=" + entry.getValue());
        }
        environment.add("QUEST_VR_STATUS=" + tools.statusFile().getPath());
        // The Multiplayer name and this installation's key, as OpenRAC's launcher gives them (online mods read them).
        environment.add(PlayerName.ENV + "=" + mods.playerName());
        environment.add(PlayerName.KEY_ENV + "=" + mods.playerKey());
        // The 2D mode is the same headset session, the game on a big screen in it, so that the Touch
        // controllers are read as ever and no gamepad is needed.
        Intent game = new Intent(this, QuestGameActivity.class)
            .putExtra(QuestGameActivity.EXTRA_LIBRARY, disc.library)
            .putExtra(QuestGameActivity.EXTRA_ARGUMENTS, arguments.toArray(new String[0]))
            .putExtra(QuestGameActivity.EXTRA_ENVIRONMENT, environment.toArray(new String[0]));
        startActivity(game);
        // As the Halo CE Quest port: the panel goes as the headset turns to the game.
        finish();
    }

    // ---- Options ----

    private void showChoices(Config.Choice[] choices) {
        for (Config.Choice choice : choices) {
            LinearLayout card = card();
            card.addView(text(choice.label, 16, Artwork.INK, true));
            if (!choice.help.isEmpty()) {
                card.addView(text(choice.help, 13, Artwork.MUTED, false));
            }
            LinearLayout row = new LinearLayout(this);
            row.setPadding(0, dp(8), 0, 0);
            String current = config.get(choice);
            for (int i = 0; i < choice.names.length; i++) {
                final String value = choice.values[i];
                Button option = button(choice.names[i], false, view -> {
                    if (choice == Config.PRESET) {
                        config.applyPreset(value);
                    } else {
                        config.set(choice, value);
                    }
                    showTab(tab);
                });
                if (value.equals(current)) {
                    option.setBackground(plate(Artwork.ORANGE_DARK, Artwork.ORANGE, 10));
                }
                LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1);
                params.rightMargin = dp(6);
                row.addView(option, params);
            }
            card.addView(row);
            page.addView(card);
        }
    }

    // ---- Game: the port's options for play, the cheats, and OpenRAC's own mods (BundledMods) ----

    private void showGame() {
        showChoices(Config.GAME);
        LinearLayout cheats = card();
        cheats.addView(text("Cheats", 16, Artwork.INK, true));
        cheats.addView(text("In the game: pause, Options, VR Options, then Cheats, further down its list. Invincible, infinite ammo, infinite bolts, every "
            + "weapon, every gadget, every planet, and the game's own big heads and trippy contrails, without beating "
            + "the game first. All off until you turn them on, and on until you turn them off; a save keeps what they "
            + "gave you.", 13, Artwork.MUTED, false));
        page.addView(cheats);
        LinearLayout about = card();
        about.addView(text("OpenRAC's mods", 18, Artwork.ORANGE, true));
        about.addView(text("Mods made for OpenRAC, the PC port this game runs on, built for the headset. Each counts "
            + "from the game's next start. A save made with the Item randomizer belongs to its seed.", 13, Artwork.MUTED,
            false));
        about.addView(text("Add a mod file takes another OpenRAC mod's .zip: a script mod, a data pack, or a native mod "
            + "with an android-arm64 build. Mods are programs: add only ones you trust. Made for a flat screen, some may "
            + "not suit VR.", 13, Artwork.MUTED, false));
        about.addView(text(ModVr.PAD, 13, Artwork.MUTED, false));
        about.addView(button(browsing ? "Hide OpenRAC's mod list" : "Browse OpenRAC's mods", false, view -> browseMods()));
        about.addView(button("Add a mod file", false, view -> chooseMod()));
        // The files each mod keeps (a seed, a spoiler log), in the Files app, as OpenRAC's launcher shows them.
        about.addView(button("Mod files", false, view -> {
            File modFiles = new File(getFilesDir(), FilesProvider.MOD_FILES);
            modFiles.mkdirs();
            if (!FilesProvider.show(this, modFiles)) {
                modNote = "This headset has no Files app to show the mods' files with.";
                showTab(tab);
            }
        }));
        about.addView(button("openrac.dev/mods", false, view -> {
            try {
                startActivity(new Intent(Intent.ACTION_VIEW, Uri.parse(ModIndex.SITE)));
            } catch (ActivityNotFoundException e) {
                modNote = "This headset has no browser to open " + ModIndex.SITE + " with.";
                showTab(tab);
            }
        }));
        if (!modNote.isEmpty()) {
            about.addView(text(modNote, 13, Artwork.ORANGE, false));
        }
        page.addView(about);
        if (mods.usesPlayerName()) {
            showPlayerName();
        }
        if (browsing) {
            showModIndex();
        }
        for (BundledMods.Mod mod : mods.list()) {
            showMod(mod);
        }
    }

    /**
     * The Multiplayer name (PlayerName), as OpenRAC's launcher shows it: only while a mod that is on uses it. A name
     * that is not one is not kept; the last one stays.
     */
    private void showPlayerName() {
        LinearLayout card = card();
        card.addView(text("Multiplayer name", 16, Artwork.INK, true));
        card.addView(text("What the other players see in Multiplayer. " + PlayerName.MIN + " to " + PlayerName.MAX
            + " letters, digits or underscores (_), no spaces. Made up at random until you give yours.", 13,
            Artwork.MUTED, false));
        EditText name = new EditText(this);
        name.setSingleLine(true);
        name.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        name.setFilters(new InputFilter[] {new InputFilter.LengthFilter(PlayerName.MAX)});
        name.setText(mods.playerName());
        name.setTextColor(Artwork.INK);
        name.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence text, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence text, int start, int before, int count) {}
            @Override public void afterTextChanged(Editable text) { mods.setPlayerName(text.toString()); }
        });
        card.addView(name);
        page.addView(card);
    }

    // ---- Browse: OpenRAC's mod list (ModIndex), each mod added as Add a mod file adds one ----

    private void browseMods() {
        // Each opening reads the list again, as OpenRAC's launcher does at Check for updates; the list from before
        // stays on show until the new one is in.
        browsing = !browsing;
        if (browsing) {
            readModIndex();
        }
        showTab(tab);
    }

    /** OpenRAC's mod list read again on a thread of its own; the Game tab drawn again when it is in, if it shows. */
    private void readModIndex() {
        if (reading) {
            return;
        }
        reading = true;
        browseNote = modIndex == null ? "Reading OpenRAC's mod list..." : "Checking OpenRAC's mod list again...";
        new Thread(() -> {
            String note = "";
            ModIndex read = null;
            try {
                read = ModIndex.fetch(new File(getFilesDir(), "mod-index"), "RatchetQuestVR-launcher/" + BuildConfig.VERSION_NAME,
                    "android-arm64", BundledMods.API_MAJOR, BundledMods.API_MINOR, BundledMods::jsonMap);
            } catch (IOException | RuntimeException e) {
                note = e.getMessage();
            }
            final ModIndex got = read;
            final String said = note;
            runOnUiThread(() -> {
                reading = false;
                if (got != null) {
                    modIndex = got;
                }
                browseNote = said;
                if (tab == GAME_TAB) {
                    showTab(tab);
                }
            });
        }, "mod-index").start();
    }

    private void showModIndex() {
        LinearLayout head = card();
        head.addView(text("OpenRAC's mods", 16, Artwork.INK, true));
        ModIndex index = modIndex;
        if (index == null) {
            head.addView(text(browseNote, 13, Artwork.MUTED, false));
            page.addView(head);
            return;
        }
        head.addView(text("From OpenRAC's own mod list, checked against OpenRAC's signature. Each is the newest version made "
            + "for this game and the headset; Add puts it with your mods, on, from the game's next start. Mods are "
            + "programs: add only ones you trust." + (index.offline ? " (Offline: the list from before. " + index.problem + ")" : "")
            + (index.stale ? " This list is out of date." : ""), 13, Artwork.MUTED, false));
        if (!browseNote.isEmpty()) {
            head.addView(text(browseNote, 13, Artwork.ORANGE, false));
        }
        page.addView(head);
        List<BundledMods.Mod> have = mods.list();
        for (boolean addable : new boolean[] {true, false}) {
            for (ModIndex.Offer offer : index.offers) {
                if ((offer.why == null) != addable) {
                    continue;
                }
                LinearLayout card = card();
                card.addView(text(offer.name, 15, addable ? Artwork.INK : Artwork.MUTED, true));
                String by = (offer.author.isEmpty() ? "" : "by " + offer.author + "  ") + (offer.version.isEmpty() ? "" : offer.version + "  ")
                    + offer.kind + (offer.aiAssisted ? "  made with AI help" : "") + (offer.impact.contains("touches-saves") ? "  changes saves" : "")
                    + (offer.impact.contains("online") ? "  goes online" : "");
                card.addView(text(by.trim(), 12, Artwork.MUTED, false));
                if (!offer.description.isEmpty()) {
                    card.addView(text(offer.description, 13, Artwork.MUTED, false));
                }
                String vr = ModVr.controls(offer.id);
                if (vr != null) {
                    card.addView(text(vr, 13, Artwork.ORANGE, false));
                }
                BundledMods.Mod installed = null;
                for (BundledMods.Mod mod : have) {
                    if (mod.id.equals(offer.id)) {
                        installed = mod;
                    }
                }
                if (!addable) {
                    card.addView(text(offer.why, 13, Artwork.MUTED, false));
                } else if (installed != null && !installed.added && index.differing(offer.id, installed.version, true) == null) {
                    card.addView(text(ModIndex.keepsBuiltIn(offer.id) ? "Built in, with this release's own changes: it is "
                        + "with your mods below." : "Built in: it is with your mods below.", 13, Artwork.MUTED, false));
                } else if (adding.equals(offer.id)) {
                    card.addView(text("Adding " + offer.name + "...", 13, Artwork.ORANGE, false));
                } else {
                    // A built-in one at another version than the list's: the list's takes its place (ModIndex.differing).
                    String label = installed == null ? "Add " + offer.name
                        : installed.version.equals(offer.version) ? "Added (" + offer.version + "): add it again"
                        : ModIndex.takeLabel(installed.version, offer);
                    card.addView(button(label, installed == null, view -> addFromIndex(offer)));
                    String revoked = installed != null && !installed.version.isEmpty() ? index.revoked(offer.id, installed.version) : null;
                    if (revoked != null) {
                        card.addView(text("OpenRAC took your version back: " + revoked, 13, Artwork.ORANGE, false));
                    }
                }
                page.addView(card);
            }
        }
    }

    private void addFromIndex(ModIndex.Offer offer) {
        if (!adding.isEmpty()) {
            return;
        }
        adding = offer.id;
        browseNote = "";
        showTab(tab);
        new Thread(() -> {
            String note;
            try {
                byte[] bytes = ModIndex.download(offer, "RatchetQuestVR-launcher/" + BuildConfig.VERSION_NAME);
                note = "Added " + mods.add(new java.io.ByteArrayInputStream(bytes)) + " " + offer.version
                    + ". It counts from the game's next start.";
            } catch (IOException | RuntimeException e) {
                note = "Not added. " + e.getMessage();
            }
            final String said = note;
            runOnUiThread(() -> {
                adding = "";
                browseNote = said;
                if (!browsing) {
                    modNote = said;  // an Update on a mod's card, Browse shut: said where the mods are
                }
                showTab(tab);
            });
        }, "add-mod").start();
    }

    private void chooseMod() {
        Intent pick = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        pick.addCategory(Intent.CATEGORY_OPENABLE);
        pick.setType("*/*");
        try {
            startActivityForResult(pick, REQUEST_MOD);
        } catch (ActivityNotFoundException e) {
            modNote = "This headset has no file picker to choose the mod with.";
            showTab(tab);
        }
    }

    private void addMod(Uri file) {
        modNote = "Adding the mod...";
        showTab(tab);
        new Thread(() -> {
            String note;
            try (java.io.InputStream in = getContentResolver().openInputStream(file)) {
                note = "Added " + mods.add(in) + ". It counts from the game's next start.";
            } catch (java.io.IOException | RuntimeException e) {
                note = "Not added. " + e.getMessage();
            }
            final String said = note;
            runOnUiThread(() -> {
                modNote = said;
                showTab(tab);
            });
        }, "add-mod").start();
    }

    private void showMod(BundledMods.Mod mod) {
        LinearLayout card = card();
        card.addView(text(mod.name + (mod.version.isEmpty() ? "" : "  " + mod.version), 16, Artwork.INK, true));
        card.addView(text(mod.description, 13, Artwork.MUTED, false));
        String vr = ModVr.controls(mod.id);
        if (vr != null) {
            card.addView(text(vr, 13, Artwork.ORANGE, false));
        }
        boolean on = mods.on(mod);
        if (on) {
            String problem = BundledMods.lastStart(tools.plugins(), mod);
            if (!problem.isEmpty()) {
                card.addView(text("At the game's last start it did not run: " + problem, 13, Artwork.ORANGE, false));
            }
        }
        // OpenRAC's mod list at another version than this one (built in or added): take the list's, so a party's games
        // match (the owner: "we shouldnt have mismatches"). Its archive is checked as Browse's are.
        ModIndex index = modIndex;
        ModIndex.Offer listed = index == null ? null : index.differing(mod.id, mod.version, !mod.added);
        if (adding.equals(mod.id)) {
            card.addView(text("Getting " + mod.name + " from OpenRAC's mod list...", 13, Artwork.ORANGE, false));
        } else if (listed != null) {
            boolean party = listed.impact.contains("gameplay") || listed.impact.contains("online");
            card.addView(text("OpenRAC's mod list has " + listed.version + "." + (party ? " In Multiplayer every game "
                + "needs the same version of it." : ""), 13, Artwork.ORANGE, false));
            card.addView(button(ModIndex.takeLabel(mod.version, listed), true, view -> addFromIndex(listed)));
        }
        if (mod.added) {
            card.addView(button(mod.builtIn != null ? "Back to the built-in " + mod.builtIn : "Remove this mod", false, view -> {
                mods.remove(mod, tools.plugins());
                showTab(tab);
            }));
        }
        card.addView(row(new String[] {"On", "Off"}, new String[] {"true", "false"}, on ? "true" : "false", value -> {
            mods.setOn(mod, Boolean.parseBoolean(value));
            showTab(tab);
        }));
        if (on) {
            if (!mod.presets.isEmpty()) {
                // As OpenRAC's launcher: a Default button of its own (unless a preset is the default), the one whose
                // values the settings have lit.
                card.addView(text("Presets", 14, Artwork.INK, true));
                List<String> names = new ArrayList<>(), values = new ArrayList<>();
                if (!BundledMods.ownDefault(mod)) {
                    names.add("Default");
                    values.add(BundledMods.DEFAULT);
                }
                for (int i = 0; i < mod.presets.size(); i++) {
                    names.add(mod.presets.get(i).name);
                    values.add(BundledMods.presetValue(i));
                }
                card.addView(row(names.toArray(new String[0]), values.toArray(new String[0]), mods.lit(mod), value -> {
                    mods.apply(mod, value.equals(BundledMods.DEFAULT) ? null
                        : mod.presets.get(values.indexOf(value) - (BundledMods.ownDefault(mod) ? 0 : 1)));
                    showTab(tab);
                }));
            }
            for (BundledMods.Setting s : mod.settings) {
                card.addView(text(s.name, 14, Artwork.INK, true));
                if (!s.help.isEmpty()) {
                    card.addView(text(s.help, 12, Artwork.MUTED, false));
                }
                String current = mods.shown(mod, s);
                if (s.stepped()) {
                    card.addView(stepper(s, current, value -> {
                        mods.set(mod, s, value);
                        showTab(tab);
                    }));
                } else if (s.type.equals("string") || s.type.equals("keybind")) {
                    EditText typed = field(s, current, value -> mods.set(mod, s, value));
                    if (s.type.equals("keybind")) {
                        card.addView(text("Keyboard keys by name, comma-separated (KeyB, Space); the controllers do not use "
                            + "them.", 12, Artwork.MUTED, false));
                        typed.setHint("none");
                    }
                    card.addView(typed);
                } else {
                    card.addView(row(s.names, s.values, current, value -> {
                        mods.set(mod, s, value);
                        showTab(tab);
                    }));
                }
            }
        }
        page.addView(card);
    }

    /** A button for each value, the current one lit. */
    private LinearLayout row(String[] names, String[] values, String current, Consumer<String> chosen) {
        LinearLayout row = new LinearLayout(this);
        row.setPadding(0, dp(6), 0, dp(4));
        for (int i = 0; i < names.length; i++) {
            final String value = values[i];
            Button option = button(names[i], false, view -> chosen.accept(value));
            if (value.equals(current)) {
                option.setBackground(plate(Artwork.ORANGE_DARK, Artwork.ORANGE, 10));
            }
            LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1);
            params.rightMargin = dp(6);
            row.addView(option, params);
        }
        return row;
    }

    /** A number's value between a step down and a step up, its manifest's range. */
    private LinearLayout stepper(BundledMods.Setting s, String current, Consumer<String> chosen) {
        LinearLayout row = new LinearLayout(this);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, dp(6), 0, dp(4));
        int at = Math.max(0, Arrays.asList(s.values).indexOf(current));
        Button less = button("−", false, view -> chosen.accept(s.values[Math.max(0, at - 1)]));
        Button more = button("+", false, view -> chosen.accept(s.values[Math.min(s.values.length - 1, at + 1)]));
        TextView value = text(current + (current.equals(s.fallback) ? "  (default)" : ""), 16, Artwork.INK, true);
        value.setGravity(Gravity.CENTER);
        row.addView(less, new LinearLayout.LayoutParams(dp(72), ViewGroup.LayoutParams.WRAP_CONTENT));
        row.addView(value, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1));
        row.addView(more, new LinearLayout.LayoutParams(dp(72), ViewGroup.LayoutParams.WRAP_CONTENT));
        return row;
    }

    /** A line of text the player types (a seed), kept as it changes. */
    private EditText field(BundledMods.Setting s, String current, Consumer<String> typed) {
        EditText field = new EditText(this);
        field.setSingleLine(true);
        field.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        field.setFilters(new InputFilter[] {new InputFilter.LengthFilter(s.maxLength)});
        field.setText(current);
        field.setTextColor(Artwork.INK);
        field.setHint("blank: a random one");
        field.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence text, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence text, int start, int before, int count) {}
            @Override public void afterTextChanged(Editable text) { typed.accept(text.toString()); }
        });
        return field;
    }

    /** The VR tab's first card: what is not on it (a player could not find the Moves page here, 0.1.15). */
    private void showInGameOptions() {
        LinearLayout card = card();
        card.addView(text("Looking for Moves, VR Hands, VR Body or Cheats?", 16, Artwork.INK, true));
        card.addView(text("They are in the game, not here. In a level, press Y, then Options, then VR Options. The "
            + "Controls tab says how the menus work and where every option is.", 13, Artwork.MUTED, false));
        page.addView(card);
    }

    private void showControls() {
        LinearLayout menus = card();
        menus.addView(text("Menus, and where every option is", 18, Artwork.ORANGE, true));
        menus.addView(text(readAsset("menus.txt"), 15, Artwork.INK, false));
        page.addView(menus);
        LinearLayout card = card();
        card.addView(text("Touch controllers", 18, Artwork.ORANGE, true));
        card.addView(text(readAsset("controls.txt"), 15, Artwork.INK, false));
        page.addView(card);
    }

    private void showLogs() {
        LinearLayout card = card();
        card.addView(text("Logs", 18, Artwork.ORANGE, true));
        card.addView(text("Every run and set-up is saved in the headset's Download/RatchetQuestVR folder. "
            + "Send the newest file with any report. Below is the summary of the last run.", 15, Artwork.INK, false));
        page.addView(card);
        LinearLayout last = card();
        last.addView(text("Last run", 16, Artwork.INK, true));
        last.addView(text(RunLog.lastRunTail(this), 11, Artwork.MUTED, false));
        page.addView(last);
    }

    private void showCredits() {
        LinearLayout card = card();
        card.addView(text("Credits", 18, Artwork.ORANGE, true));
        card.addView(text(readAsset("credits.txt"), 14, Artwork.INK, false));
        page.addView(card);
        LinearLayout licences = card();
        licences.addView(text("Open-source licences", 18, Artwork.ORANGE, true));
        licences.addView(text(readAsset("third_party_notices.txt"), 11, Artwork.MUTED, false));
        page.addView(licences);
    }

    private String readAsset(String name) {
        StringBuilder out = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(getAssets().open(name), StandardCharsets.UTF_8))) {
            for (String line = reader.readLine(); line != null; line = reader.readLine()) {
                out.append(line).append('\n');
            }
        } catch (IOException e) {
            out.append("(").append(name).append(" is missing)");
        }
        return out.toString();
    }

    private static String read(File file) {
        try {
            return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8);
        } catch (IOException e) {
            return "";
        }
    }
}
