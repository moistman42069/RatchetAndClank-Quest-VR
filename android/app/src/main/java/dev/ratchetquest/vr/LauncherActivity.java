// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
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
import java.util.List;
import java.util.Map;

/** The launcher: a 2D panel on the Quest. Choose the disc once, change settings, play. */
public final class LauncherActivity extends Activity {
    private static final int REQUEST_DISC = 0x5243;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private LinearLayout page;
    private final List<Button> tabs = new ArrayList<>();
    private int tab = 0;
    private Config config;
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
        tools = new Tools(this);
        buildFrame();
        // What the last run left goes to Download, on a thread of its own: logcat takes a moment.
        new Thread(() -> RunLog.saveLastRun(this), "last-run-log").start();
        showTab(0);
        Updater.maybeCheck(this, this::updatesChanged);
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
        String[] names = {"Play", "Graphics", "VR", "Controls", "Logs", "Credits"};
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
            case 2: showChoices(Config.VR); break;
            case 3: showControls(); break;
            case 4: showLogs(); break;
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
        test.addView(text("Test without VR", 16, Artwork.INK, true));
        test.addView(text("Runs the game in this flat window with no VR at all, to tell a problem with the game "
            + "from a problem with VR. It is only a test: a gamepad is needed to play it.", 14, Artwork.MUTED, false));
        test.addView(button("Run the 2D test", false, view -> play(disc, false)));
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
                config.writeForGame();
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
            "--settings", tools.settingsFile().getPath()));
        if (vr) {
            arguments.add("--plugins");
            arguments.add(new File(tools.plugins(), "plugins.json").getPath());
        } else {
            arguments.add("--no-mods");
        }
        List<String> environment = new ArrayList<>();
        for (Map.Entry<String, String> entry : tools.environment().entrySet()) {
            environment.add(entry.getKey() + "=" + entry.getValue());
        }
        environment.add("QUEST_VR_STATUS=" + tools.statusFile().getPath());
        Intent game = new Intent(this, vr ? QuestGameActivity.class : FlatGameActivity.class)
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

    private void showControls() {
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
