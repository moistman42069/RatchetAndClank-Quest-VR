// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Desktop;
import java.awt.Dimension;
import java.awt.FileDialog;
import java.awt.Frame;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.event.KeyEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.nio.channels.FileLock;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Consumer;

import javax.imageio.ImageIO;
import javax.swing.BorderFactory;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTextArea;
import javax.swing.KeyStroke;
import javax.swing.SwingUtilities;
import javax.swing.Timer;
import javax.swing.border.EmptyBorder;

import static dev.ratchetquest.vr.PcWidgets.Btn;
import static dev.ratchetquest.vr.PcWidgets.Card;
import static dev.ratchetquest.vr.PcWidgets.Page;
import static dev.ratchetquest.vr.PcWidgets.column;
import static dev.ratchetquest.vr.PcWidgets.gap;
import static dev.ratchetquest.vr.PcWidgets.row;
import static dev.ratchetquest.vr.PcWidgets.text;
import static dev.ratchetquest.vr.PcWidgets.weighted;

/**
 * The PC's launcher: the headset's (LauncherActivity) on a PC's screen. The same tabs, options, disc set-up, mods,
 * logs and updates; Play starts the game for PC VR, which opens in whichever headset software is running.
 */
public final class PcLauncher {
    static final String TITLE = "Ratchet & Clank VR";
    private static final String[] TABS = {"Play", "Graphics", "VR", "Game", "Controls", "Logs", "Credits"};

    private final PcPaths paths;
    private final PcConfig config;
    private final PcMods mods;
    private JFrame frame;
    private JPanel root;
    private Page page;
    private JScrollPane scroll;
    private final List<Btn> tabs = new ArrayList<>();
    private int tab = 0;
    /** What became of the last mod file picked, shown on the Game tab. */
    private String modNote = "";
    /** Why the last update did not finish, if it did not. */
    private String updateNote = "";
    private volatile Process game;
    private volatile String playFailure;

    private PcWidgets.Bar progress;
    private JTextArea progressText, progressDetail, updateText;
    private final Timer watcher = new Timer(300, event -> watch());
    private final Timer updateWatcher = new Timer(300, event -> watchUpdate());

    PcLauncher(PcPaths paths) {
        this.paths = paths;
        config = new PcConfig(paths);
        mods = new PcMods(paths);
    }

    // ---- Starting ----

    public static void main(String[] args) throws Exception {
        PcPaths paths = new PcPaths();
        if (args.length >= 2 && args[0].equals("--write-icon")) {
            PcArtwork.writeIcon(new File(args[1]));
            return;
        }
        if (args.length >= 1 && args[0].equals("--self-test")) {
            System.exit(PcSelfTest.run(paths));
        }
        if (args.length >= 2 && args[0].equals("--test-setup")) {
            System.exit(testSetup(paths, new File(args[1])));
        }
        if (args.length >= 3 && args[0].equals("--test-play")) {
            System.exit(testPlay(paths, args[1].equals("vr"), Integer.parseInt(args[2]),
                args.length >= 4 ? Integer.valueOf(args[3]) : null));
        }
        if (args.length >= 3 && args[0].equals("--render")) {
            int w = args.length >= 5 ? Integer.parseInt(args[3]) : 1180, h = args.length >= 5 ? Integer.parseInt(args[4]) : 780;
            PcUpdater.network = false;
            SwingUtilities.invokeAndWait(() -> {
                try {
                    new PcLauncher(paths).render(Integer.parseInt(args[1]), new File(args[2]), w, h);
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            });
            System.exit(0);
        }
        // One launcher at a time: two would set the game up, or start it, over each other.
        paths.home.mkdirs();
        RandomAccessFile lockFile = new RandomAccessFile(new File(paths.home, "launcher.lock"), "rw");
        FileLock lock = lockFile.getChannel().tryLock();
        if (lock == null) {
            JOptionPane.showMessageDialog(null, "The " + TITLE + " launcher is already open.", TITLE,
                JOptionPane.INFORMATION_MESSAGE);
            return;
        }
        SwingUtilities.invokeLater(() -> new PcLauncher(paths).open());
    }

    private void open() {
        frame = new JFrame(TITLE);
        frame.setIconImages(PcArtwork.icons());
        frame.setDefaultCloseOperation(JFrame.DO_NOTHING_ON_CLOSE);
        frame.addWindowListener(new WindowAdapter() {
            @Override
            public void windowClosing(WindowEvent event) {
                close();
            }
        });
        buildFrame();
        frame.setContentPane(root);
        frame.setMinimumSize(new Dimension(860, 560));
        frame.setSize(1180, 800);
        frame.setLocationRelativeTo(null);
        // What the last run left goes to the logs, on a thread of its own, unless a game of ours is still open.
        new Thread(() -> {
            PcRunLog.saveLastRun(paths, "was closed while the launcher was not open, or the launcher was closed first");
        }, "last-run-log").start();
        updateNote = PcUpdater.lastInstall(paths);
        showTab(0);
        frame.setVisible(true);
        showWhatsNewOnce();
        PcUpdater.maybeCheck(paths, this::updatesChanged);
    }

    private void close() {
        if (PcSetup.STATE.running || PcSetup.STATE.starting) {
            int answer = JOptionPane.showConfirmDialog(frame, "The game is still being set up. Stop and close? "
                + "It starts again from the beginning next time.", TITLE, JOptionPane.YES_NO_OPTION,
                JOptionPane.WARNING_MESSAGE);
            if (answer != JOptionPane.YES_OPTION) {
                return;
            }
            PcSetup.stop();
        }
        frame.dispose();
        System.exit(0);
    }

    // ---- What's new ----

    private void showWhatsNewOnce() {
        Prefs seen = new Prefs(paths.prefs("whats_new"));
        if (PcBuild.VERSION.equals(seen.getString("shown", ""))) {
            return;
        }
        seen.putString("shown", PcBuild.VERSION);
        showWhatsNew();
    }

    /** whats_new.txt in a box over the launcher, until Got it. The Play tab's Updates card shows it again. */
    private void showWhatsNew() {
        JDialog dialog = new JDialog(frame, "What's new", true);
        dialog.setUndecorated(true);
        dialog.setBackground(new Color(0, 0, 0, 0));
        Card box = new Card(PcArtwork.NAVY, PcArtwork.ORANGE, 14, 24, 20);
        box.add(text("What's new in " + PcBuild.VERSION, 22, PcArtwork.ORANGE, true));
        JTextArea words = text(readText("whats_new.txt").trim(), 15, PcArtwork.INK, false);
        Page inside = new Page(0);
        inside.setBorder(new EmptyBorder(10, 0, 14, 8));
        inside.add(words);
        JPanel holder = new JPanel(new BorderLayout());
        holder.setOpaque(false);
        holder.add(scrollPane(inside));
        box.add(holder);
        box.add(new Btn("Got it", true, dialog::dispose));
        dialog.getRootPane().registerKeyboardAction(event -> dialog.dispose(), KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0),
            JComponent.WHEN_IN_FOCUSED_WINDOW);
        dialog.setContentPane(box);
        int height = Math.min(640, frame.getHeight() - 40);
        holder.setPreferredSize(new Dimension(760, height - 150));
        dialog.setSize(820, height);
        dialog.setLocationRelativeTo(frame);
        dialog.setVisible(true);
    }

    // ---- The frame ----

    private static JScrollPane scrollPane(Page inside) {
        JScrollPane pane = new JScrollPane(inside, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED,
            JScrollPane.HORIZONTAL_SCROLLBAR_NEVER);
        pane.setOpaque(false);
        pane.getViewport().setOpaque(false);
        pane.setBorder(BorderFactory.createEmptyBorder());
        PcWidgets.style(pane.getVerticalScrollBar());
        return pane;
    }

    private void buildFrame() {
        root = new JPanel(new BorderLayout()) {
            @Override
            protected void paintComponent(Graphics g) {
                Graphics2D g2 = (Graphics2D) g.create();
                PcArtwork.space(g2, getWidth(), getHeight());
                g2.dispose();
            }
        };
        root.setBorder(new EmptyBorder(18, 24, 14, 24));

        JPanel top = column(0);
        JPanel header = new JPanel(new BorderLayout(12, 0));
        header.setOpaque(false);
        JComponent bolt = new JComponent() {
            @Override
            protected void paintComponent(Graphics g) {
                Graphics2D g2 = (Graphics2D) g.create();
                PcArtwork.bolt(g2, 0, (getHeight() - 46) / 2f, 46);
                g2.dispose();
            }
        };
        bolt.setPreferredSize(new Dimension(46, 46));
        header.add(bolt, BorderLayout.WEST);
        JPanel titles = column(0);
        titles.add(text("RATCHET & CLANK", 26, PcArtwork.ORANGE, true));
        titles.add(text("PC VR  ·  unofficial fan port  ·  v" + PcBuild.VERSION, 13, PcArtwork.MUTED, false));
        header.add(titles, BorderLayout.CENTER);
        JPanel headerRow = column(0);
        headerRow.add(new HeaderBox(header));
        top.add(headerRow);

        JPanel bar = row(6);
        bar.setBorder(new EmptyBorder(12, 0, 10, 0));
        for (int i = 0; i < TABS.length; i++) {
            final int index = i;
            Btn button = new Btn(TABS[i], false, () -> showTab(index));
            bar.add(weighted(button, 1f));
            tabs.add(button);
        }
        top.add(bar);
        root.add(top, BorderLayout.NORTH);

        page = new Page(12);
        page.setBorder(new EmptyBorder(0, 0, 0, 10));
        scroll = scrollPane(page);
        root.add(scroll, BorderLayout.CENTER);
    }

    /** The header in a column: its height the bolt's or the titles', whichever is taller. */
    private static final class HeaderBox extends JPanel {
        HeaderBox(JPanel header) {
            super(new BorderLayout());
            setOpaque(false);
            add(header);
        }

        @Override
        public Dimension getPreferredSize() {
            return new Dimension(800, 52);
        }
    }

    private void showTab(int index) {
        tab = index;
        for (int i = 0; i < tabs.size(); i++) {
            Btn button = tabs.get(i);
            button.fill = i == index ? PcArtwork.ORANGE_DARK : PcArtwork.BUTTON;
            button.edge = i == index ? PcArtwork.ORANGE : PcArtwork.BUTTON_EDGE;
            button.repaint();
        }
        watcher.stop();
        progress = null;
        updateText = null;
        page.removeAll();
        switch (index) {
            case 0: showPlay(); break;
            case 1: showChoices(Options.GRAPHICS); break;
            case 2: showInGameOptions(); showChoices(Options.VR); break;
            case 3: showGame(); break;
            case 4: showControls(); break;
            case 5: showLogs(); break;
            default: showCredits(); break;
        }
        page.revalidate();
        page.repaint();
    }

    /** The same tab drawn again where it was scrolled to: a choice made, a mod turned on. */
    private void redraw() {
        int at = scroll.getVerticalScrollBar().getValue();
        showTab(tab);
        SwingUtilities.invokeLater(() -> scroll.getVerticalScrollBar().setValue(at));
    }

    private Card card() {
        return new Card();
    }

    // ---- Play ----

    private void showPlay() {
        showPlayCards();
        // Not while the game is being set up: an install would stop it.
        if (!PcSetup.STATE.running && !PcSetup.STATE.starting) {
            showUpdates();
        }
    }

    private void showPlayCards() {
        File status = paths.statusFile();
        if (status.isFile() && game == null) {
            Card card = card();
            card.add(text("VR could not start last time", 18, PcArtwork.BAD, true));
            card.add(text(PcRunLog.read(status).trim(), 15, PcArtwork.INK, false));
            card.add(text("The full report is in " + paths.logs + ".", 13, PcArtwork.MUTED, false));
            page.add(card);
        }
        if (game != null) {
            Card card = card();
            card.add(text("Playing", 22, PcArtwork.GOOD, true));
            card.add(text("The game is open in its own window and in your headset. Close it (or press Stop the game) "
                + "to come back here; its log is saved as it closes.", 15, PcArtwork.INK, false));
            card.add(gap(10));
            card.add(new Btn("Stop the game", false, () -> {
                Process running = game;
                if (running != null) {
                    running.descendants().forEach(ProcessHandle::destroyForcibly);
                    running.destroyForcibly();
                }
            }));
            page.add(card);
            return;
        }
        if (PcSetup.STATE.running || PcSetup.STATE.starting) {
            showProgress();
            return;
        }
        String failure = playFailure != null ? playFailure : PcSetup.STATE.failure;
        if (failure != null) {
            Card card = card();
            card.add(text("That did not work", 18, PcArtwork.BAD, true));
            card.add(text(failure, 15, PcArtwork.INK, false));
            page.add(card);
        }
        String version = PcSetup.installedVersion(paths);
        if (version == null) {
            String outdated = PcSetup.outdatedVersion(paths);
            if (outdated != null) {
                showLevelsOutdated(Discs.byVersion(outdated));
            } else {
                showDiscNeeded();
            }
            return;
        }

        Discs.Disc disc = Discs.byVersion(version);
        Card card = card();
        card.add(text("Ready to play", 22, PcArtwork.GOOD, true));
        card.add(text(Discs.TITLE + "\n" + disc.line(), 15, PcArtwork.INK, false));
        card.add(gap(14));
        card.add(new Btn("PLAY IN VR", true, () -> play(disc, true)));
        card.add(gap(6));
        card.add(text("Start your headset's PC VR software first: SteamVR, Meta Horizon Link (Link or Air Link), "
            + "Virtual Desktop, Pico Connect, Windows Mixed Reality or another OpenXR one. The game opens in whichever "
            + "is running, with the controllers it has.", 13, PcArtwork.MUTED, false));
        page.add(card);

        Card test = card();
        test.add(text("2D mode", 16, PcArtwork.INK, true));
        test.add(text("The game on a big flat screen in the headset, played with your VR controllers as a "
            + "PlayStation pad: no gamepad needed. Also tells a problem with the game from a problem with VR.",
            14, PcArtwork.MUTED, false));
        test.add(new Btn("Play in 2D", false, () -> play(disc, false)));
        page.add(test);

        Card other = card();
        other.add(text("Another disc", 16, PcArtwork.INK, true));
        other.add(text("Sets the game up again from another supported disc. Saves are kept.", 14, PcArtwork.MUTED, false));
        other.add(new Btn("Choose another disc image", false, this::chooseDisc));
        page.add(other);
    }

    private void showDiscNeeded() {
        Card card = card();
        card.add(text("First, your game disc", 22, PcArtwork.ORANGE, true));
        card.add(text(Discs.WHICH + "\n\nChoose the .iso wherever it is on this PC. Setting up keeps about 2 GB in "
            + paths.home + " (6 GB when the .iso is on another drive, which is then copied there), and the disc is "
            + "checked before anything is kept.", 15, PcArtwork.INK, false));
        card.add(gap(14));
        card.add(new Btn("CHOOSE YOUR .ISO FILE", true, this::chooseDisc));
        page.add(card);
    }

    private void showLevelsOutdated(Discs.Disc disc) {
        Card card = card();
        card.add(text("Update the levels", 22, PcArtwork.ORANGE, true));
        card.add(text("This version of the game reads its levels in a newer form. They are made again from "
            + "the disc already set up on this PC: no need to choose it again, and saves are kept.\n\n"
            + Discs.TITLE + "\n" + disc.line(), 15, PcArtwork.INK, false));
        card.add(gap(14));
        card.add(new Btn("UPDATE THE LEVELS", true, () -> startSetup(null)));
        page.add(card);

        Card other = card();
        other.add(text("Another disc", 16, PcArtwork.INK, true));
        other.add(text("Sets the game up from another supported disc instead.", 14, PcArtwork.MUTED, false));
        other.add(new Btn("Choose another disc image", false, this::chooseDisc));
        page.add(other);
    }

    private void showProgress() {
        Card card = card();
        card.add(text("Setting up the game", 22, PcArtwork.ORANGE, true));
        progressText = text(PcSetup.STATE.step, 16, PcArtwork.INK, true);
        card.add(progressText);
        progress = new PcWidgets.Bar(PcSetup.STATE.percent);
        card.add(progress);
        progressDetail = text(PcSetup.STATE.detail, 13, PcArtwork.MUTED, false);
        card.add(progressDetail);
        card.add(text("Keep the launcher open. It only happens once.", 13, PcArtwork.MUTED, false));
        page.add(card);
        watcher.start();
    }

    private void watch() {
        if (progress != null) {
            progress.set(PcSetup.STATE.percent);
            progressText.setText(PcSetup.STATE.step + "  " + PcSetup.STATE.percent + "%");
            progressDetail.setText(PcSetup.STATE.detail);
        }
        if (!PcSetup.STATE.running && !PcSetup.STATE.starting) {
            watcher.stop();
            if (tab == 0) {
                showTab(0);
            }
        }
    }

    private void chooseDisc() {
        FileDialog pick = new FileDialog(frame, "Choose your Ratchet & Clank disc image (.iso)", FileDialog.LOAD);
        pick.setFile("*.iso");
        pick.setFilenameFilter((folder, name) -> name.toLowerCase(java.util.Locale.ROOT).endsWith(".iso"));
        pick.setVisible(true);
        if (pick.getFile() == null) {
            return;
        }
        startSetup(new File(pick.getDirectory(), pick.getFile()));
    }

    private void startSetup(File image) {
        if (PcSetup.STATE.running || PcSetup.STATE.starting || game != null) {
            return;
        }
        playFailure = null;
        PcSetup.STATE.starting = true;
        PcSetup.STATE.failure = null;
        PcSetup.STATE.percent = 0;
        PcSetup.STATE.step = "Starting";
        PcSetup.STATE.detail = "";
        new Thread(() -> new PcSetup(paths).run(image), "setup").start();
        showTab(0);
    }

    private void play(Discs.Disc disc, boolean vr) {
        if (game != null || PcSetup.STATE.running) {
            return;
        }
        playFailure = null;
        new Thread(() -> {
            try {
                Process process = startGame(paths, config, disc, vr, null);
                game = process;
                SwingUtilities.invokeLater(() -> {
                    showTab(0);
                    frame.setState(Frame.ICONIFIED);
                });
                int code = process.waitFor();
                PcRunLog.saveLastRun(paths, "ended with exit code " + code);
            } catch (IOException e) {
                playFailure = "The game could not be started: " + e.getMessage();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            } finally {
                game = null;
                SwingUtilities.invokeLater(() -> {
                    frame.setState(Frame.NORMAL);
                    frame.toFront();
                    showTab(tab);
                });
            }
        }, "game").start();
    }

    /**
     * The game for `disc`, as Play starts it: the choices written for it, its output kept for the run's log. The same
     * arguments as the headset's launcher passes, with the cards named so that the game keeps them in this launcher's
     * folder, not OpenRAC's.
     */
    static Process startGame(PcPaths paths, PcConfig config, Discs.Disc disc, boolean vr, Integer level)
            throws IOException {
        File program = paths.game(disc.version);
        if (!program.isFile()) {
            throw new IOException("this release has no " + program.getName());
        }
        paths.data().mkdirs();
        paths.plugins().mkdirs();
        config.writeForGame(!vr);
        paths.statusFile().delete();
        paths.runPending().createNewFile();
        List<String> arguments = new ArrayList<>(Arrays.asList(program.getPath(),
            "--data", paths.data().getPath(),
            "--window",
            "--levels", paths.levels(disc.version).getPath(),
            "--settings", paths.settingsFile().getPath(),
            "--plugins", new File(paths.plugins(), "plugins.json").getPath(),
            "--cards", paths.cards(disc).getPath()));
        if (level != null) {
            arguments.addAll(Arrays.asList("--level", level.toString()));
        }
        ProcessBuilder builder = new ProcessBuilder(arguments);
        // Started in the data folder, as OpenRAC's own launcher starts its carried copy's game: what it writes beside
        // itself (a screenshot) has a place there, and the release's folder is never written.
        builder.directory(paths.data());
        builder.redirectErrorStream(true);
        builder.redirectOutput(paths.gameOutput());
        builder.environment().clear();
        builder.environment().putAll(paths.environment());
        builder.environment().put("QUEST_VR_STATUS", paths.statusFile().getPath());
        return builder.start();
    }

    // ---- Updates ----

    private String updateLine() {
        String line = PcUpdater.STATE.percent >= 0 ? "Downloading: " + PcUpdater.STATE.percent + "%" : PcUpdater.status(paths);
        return line + "\nInstalled: " + PcBuild.VERSION
            + ". The launcher asks GitHub for this project's releases; nothing of yours is sent.";
    }

    private void showUpdates() {
        Card card = card();
        card.add(text("Updates", 16, PcArtwork.INK, true));
        if (!updateNote.isEmpty()) {
            card.add(text(updateNote, 14, PcArtwork.ORANGE, false));
        }
        updateText = text(updateLine(), 14, PcArtwork.MUTED, false);
        card.add(updateText);
        JPanel buttons = row(6);
        buttons.setBorder(new EmptyBorder(8, 0, 0, 0));
        String tag = PcUpdater.STATE.tag;
        if (tag != null) {
            if (PcUpdater.STATE.ready) {
                buttons.add(weighted(new Btn("Install " + tag.substring(1), false, this::installUpdate), 2f));
            } else {
                buttons.add(weighted(new Btn("Download " + tag.substring(1), false, () -> {
                    PcUpdater.download(paths, this::updatesChanged);
                    updateWatcher.start();
                }), 2f));
            }
        }
        buttons.add(weighted(new Btn("Check now", false, () -> {
            PcUpdater.check(paths, this::updatesChanged);
            updateWatcher.start();
        }), 1f));
        buttons.add(weighted(new Btn("Automatic checks: " + (PcUpdater.automatic(paths) ? "On" : "Off"), false, () -> {
            PcUpdater.setAutomatic(paths, !PcUpdater.automatic(paths));
            redraw();
        }), 1f));
        buttons.add(weighted(new Btn("What's new", false, this::showWhatsNew), 1f));
        card.add(buttons);
        page.add(card);
    }

    private void installUpdate() {
        if (game != null) {
            updateNote = "Close the game first: the update replaces it.";
            redraw();
            return;
        }
        String why = PcUpdater.install(paths, true);
        if (why == null) {
            frame.dispose();
            System.exit(0);
        }
        updateNote = why;
        redraw();
    }

    private void watchUpdate() {
        if (updateText != null) {
            updateText.setText(updateLine());
            page.revalidate();
        }
        if (!PcUpdater.STATE.busy) {
            updateWatcher.stop();
        }
    }

    /** A check or a download ended: the Play tab drawn again, unless something else is on it. */
    private void updatesChanged() {
        SwingUtilities.invokeLater(() -> {
            if (frame != null && tab == 0 && !PcSetup.STATE.running && !PcSetup.STATE.starting) {
                redraw();
            }
        });
    }

    // ---- Options ----

    /** What a choice does on a PC, where it differs from the headset. */
    private static String pcNote(Options.Choice choice) {
        if (choice == Options.REFRESH) {
            return "On a PC, where the headset's software lets a game choose its rate (the game's log lists the rates "
                + "it offered); elsewhere, such as in SteamVR, set the rate in that software.";
        }
        if (choice == Options.RESOLUTION) {
            return "On a PC, of the size your headset's software recommends, which its own resolution setting changes too.";
        }
        return "";
    }

    private void showChoices(Options.Choice[] choices) {
        for (Options.Choice choice : choices) {
            Card card = card();
            card.add(text(choice.label, 16, PcArtwork.INK, true));
            if (!choice.help.isEmpty()) {
                card.add(text(choice.help, 13, PcArtwork.MUTED, false));
            }
            String note = pcNote(choice);
            if (!note.isEmpty()) {
                card.add(text(note, 13, PcArtwork.MUTED, false));
            }
            String current = config.get(choice);
            card.add(choiceRow(choice.names, choice.values, current, value -> {
                if (choice == Options.PRESET) {
                    config.applyPreset(value);
                } else {
                    config.set(choice, value);
                }
                redraw();
            }));
            page.add(card);
        }
    }

    // ---- Game: the port's options for play, the cheats, and OpenRAC's own mods (PcMods) ----

    private void showGame() {
        showChoices(Options.GAME);
        Card cheats = card();
        cheats.add(text("Cheats", 16, PcArtwork.INK, true));
        cheats.add(text("In the game: pause, Options, VR Options, then Cheats, further down its list. Invincible, infinite ammo, infinite bolts, every "
            + "weapon, every gadget, every planet, and the game's own big heads and trippy contrails, without beating "
            + "the game first. All off until you turn them on, and on until you turn them off; a save keeps what they "
            + "gave you.", 13, PcArtwork.MUTED, false));
        page.add(cheats);
        Card about = card();
        about.add(text("OpenRAC's mods", 18, PcArtwork.ORANGE, true));
        about.add(text("Mods made for OpenRAC, the PC port this game runs on, built for Windows. Each counts "
            + "from the game's next start. A save made with the Item randomizer belongs to its seed.", 13,
            PcArtwork.MUTED, false));
        about.add(text("Add a mod file takes another OpenRAC mod's .zip: a script mod, a data pack, or a native mod "
            + "with a windows-x86_64 build. Mods are programs: add only ones you trust. Made for a flat screen, some may "
            + "not suit VR.", 13, PcArtwork.MUTED, false));
        about.add(new Btn("Add a mod file", false, this::chooseMod));
        if (!modNote.isEmpty()) {
            about.add(text(modNote, 13, PcArtwork.ORANGE, false));
        }
        page.add(about);
        for (PcMods.Mod mod : mods.list()) {
            showMod(mod);
        }
    }

    private void chooseMod() {
        FileDialog pick = new FileDialog(frame, "Choose an OpenRAC mod (.zip)", FileDialog.LOAD);
        pick.setFile("*.zip");
        pick.setVisible(true);
        if (pick.getFile() == null) {
            return;
        }
        File file = new File(pick.getDirectory(), pick.getFile());
        modNote = "Adding the mod...";
        redraw();
        new Thread(() -> {
            String note;
            try (InputStream in = Files.newInputStream(file.toPath())) {
                note = "Added " + mods.add(in) + ". It counts from the game's next start.";
            } catch (IOException | RuntimeException e) {
                note = "Not added. " + e.getMessage();
            }
            final String said = note;
            SwingUtilities.invokeLater(() -> {
                modNote = said;
                redraw();
            });
        }, "add-mod").start();
    }

    private void showMod(PcMods.Mod mod) {
        Card card = card();
        card.add(text(mod.name, 16, PcArtwork.INK, true));
        card.add(text(mod.description, 13, PcArtwork.MUTED, false));
        boolean on = mods.on(mod);
        if (on) {
            String problem = PcMods.lastStart(paths.plugins(), mod);
            if (!problem.isEmpty()) {
                card.add(text("At the game's last start it did not run: " + problem, 13, PcArtwork.ORANGE, false));
            }
        }
        if (mod.added) {
            card.add(new Btn("Remove this mod", false, () -> {
                mods.remove(mod);
                redraw();
            }));
        }
        card.add(choiceRow(new String[] {"On", "Off"}, new String[] {"true", "false"}, on ? "true" : "false", value -> {
            mods.setOn(mod, Boolean.parseBoolean(value));
            redraw();
        }));
        if (on) {
            if (!mod.presets.isEmpty()) {
                card.add(text("Presets", 14, PcArtwork.INK, true));
                String[] names = new String[mod.presets.size()];
                for (int i = 0; i < names.length; i++) {
                    names[i] = mod.presets.get(i).name;
                }
                card.add(choiceRow(names, names, "", value -> {
                    mods.apply(mod, mod.presets.get(Arrays.asList(names).indexOf(value)));
                    redraw();
                }));
            }
            for (PcMods.Setting s : mod.settings) {
                card.add(text(s.name, 14, PcArtwork.INK, true));
                if (!s.help.isEmpty()) {
                    card.add(text(s.help, 12, PcArtwork.MUTED, false));
                }
                String current = mods.get(mod, s);
                if (s.stepped()) {
                    card.add(stepper(s, current, value -> {
                        mods.set(mod, s, value);
                        redraw();
                    }));
                } else if (s.type.equals("string")) {
                    card.add(PcWidgets.field(current, s.maxLength, value -> mods.set(mod, s, value)));
                } else {
                    card.add(choiceRow(s.names, s.values, current, value -> {
                        mods.set(mod, s, value);
                        redraw();
                    }));
                }
            }
        }
        page.add(card);
    }

    /** A button for each value, the current one lit. */
    private JPanel choiceRow(String[] names, String[] values, String current, Consumer<String> chosen) {
        JPanel row = PcWidgets.row(6);
        row.setBorder(new EmptyBorder(8, 0, 4, 0));
        for (int i = 0; i < names.length; i++) {
            final String value = values[i];
            Btn option = new Btn(names[i], false, () -> chosen.accept(value));
            if (value.equals(current)) {
                option.chosen();
            }
            row.add(weighted(option, 1f));
        }
        return row;
    }

    /** A number's value between a step down and a step up, its manifest's range. */
    private JPanel stepper(PcMods.Setting s, String current, Consumer<String> chosen) {
        JPanel row = PcWidgets.row(6);
        row.setBorder(new EmptyBorder(6, 0, 4, 0));
        int at = Math.max(0, Arrays.asList(s.values).indexOf(current));
        Btn less = new Btn("−", false, () -> chosen.accept(s.values[Math.max(0, at - 1)]));
        Btn more = new Btn("+", false, () -> chosen.accept(s.values[Math.min(s.values.length - 1, at + 1)]));
        less.setPreferredSize(new Dimension(72, less.getPreferredSize().height));
        more.setPreferredSize(new Dimension(72, more.getPreferredSize().height));
        row.add(weighted(less, 0f));
        row.add(weighted(PcWidgets.centred(current + (current.equals(s.fallback) ? "  (default)" : ""), 16,
            PcArtwork.INK), 1f));
        row.add(weighted(more, 0f));
        return row;
    }

    // ---- Controls, Logs, Credits ----

    /** The VR tab's first card: what is not on it (a player could not find the Moves page here, 0.1.15). */
    private void showInGameOptions() {
        Card card = card();
        card.add(text("Looking for Moves, VR Hands, VR Body or Cheats?", 16, PcArtwork.INK, true));
        card.add(text("They are in the game, not here. In a level, press Y, then Options, then VR Options. The "
            + "Controls tab says how the menus work and where every option is.", 13, PcArtwork.MUTED, false));
        page.add(card);
    }

    private void showControls() {
        Card menus = card();
        menus.add(text("Menus, and where every option is", 18, PcArtwork.ORANGE, true));
        menus.add(text(readText("menus.txt"), 15, PcArtwork.INK, false));
        page.add(menus);
        Card card = card();
        card.add(text("Touch controllers", 18, PcArtwork.ORANGE, true));
        card.add(text(readText("controls.txt"), 15, PcArtwork.INK, false));
        page.add(card);
        Card others = card();
        others.add(text("Other controllers", 18, PcArtwork.ORANGE, true));
        others.add(text(readText("pc_controls.txt"), 15, PcArtwork.INK, false));
        page.add(others);
    }

    private void showLogs() {
        Card card = card();
        card.add(text("Logs", 18, PcArtwork.ORANGE, true));
        card.add(text("Every run and set-up is saved in " + paths.logs + ". Send the newest file with any report. Below "
            + "is the summary of the last run.", 15, PcArtwork.INK, false));
        card.add(gap(6));
        card.add(new Btn("Open the logs folder", false, () -> {
            try {
                paths.logs.mkdirs();
                Desktop.getDesktop().open(paths.logs);
            } catch (IOException | RuntimeException e) {
                // The path is on the page to open by hand.
            }
        }));
        page.add(card);
        Card last = card();
        last.add(text("Last run", 16, PcArtwork.INK, true));
        last.add(PcWidgets.selectable(PcRunLog.lastRunTail(paths), 11, PcArtwork.MUTED));
        page.add(last);
    }

    private void showCredits() {
        Card card = card();
        card.add(text("Credits", 18, PcArtwork.ORANGE, true));
        card.add(text(readText("credits.txt"), 14, PcArtwork.INK, false));
        page.add(card);
        Card licences = card();
        licences.add(text("Open-source licences", 18, PcArtwork.ORANGE, true));
        licences.add(PcWidgets.selectable(readText("third_party_notices.txt"), 11, PcArtwork.MUTED));
        page.add(licences);
    }

    private String readText(String name) {
        File file = new File(paths.texts(), name);
        try {
            return new String(Files.readAllBytes(file.toPath()), StandardCharsets.UTF_8).replace("\r\n", "\n");
        } catch (IOException e) {
            return "(" + name + " is missing)";
        }
    }

    // ---- Tests ----

    /** A tab drawn into a picture, for the tests: no window shown. */
    private void render(int index, File out, int w, int h) throws IOException {
        frame = new JFrame(TITLE);
        buildFrame();
        frame.setContentPane(root);
        frame.addNotify();
        root.setSize(w, h);
        showTab(index);
        for (int i = 0; i < 3; i++) {
            root.validate();
            root.doLayout();
            layoutAll(root);
        }
        // At the screen's own scale (Windows' 125 % or 200 %): text is measured at it, so drawn at another it would
        // not wrap where it does in the window.
        java.awt.geom.AffineTransform screen = frame.getGraphicsConfiguration().getDefaultTransform();
        double sx = screen.getScaleX(), sy = screen.getScaleY();
        BufferedImage image = new BufferedImage((int) Math.ceil(w * sx), (int) Math.ceil(h * sy),
            BufferedImage.TYPE_INT_RGB);
        Graphics2D g = image.createGraphics();
        g.scale(sx, sy);
        root.paint(g);
        g.dispose();
        ImageIO.write(image, "png", out);
        frame.dispose();
    }

    private static void layoutAll(java.awt.Container c) {
        c.doLayout();
        for (java.awt.Component child : c.getComponents()) {
            if (child instanceof java.awt.Container) {
                layoutAll((java.awt.Container) child);
            }
        }
    }

    /** The set-up from `image` with no window, saying how far it is: the tests' run of what Choose your .iso does. */
    private static int testSetup(PcPaths paths, File image) throws InterruptedException {
        Thread thread = new Thread(() -> new PcSetup(paths).run(image), "setup");
        thread.start();
        String said = "";
        while (thread.isAlive()) {
            String now = PcSetup.STATE.step + " " + PcSetup.STATE.percent + "%";
            if (!now.equals(said)) {
                System.out.println(now);
                said = now;
            }
            thread.join(500);
        }
        System.out.println(PcSetup.STATE.done ? "set up: " + PcSetup.installedVersion(paths)
            : "failed: " + PcSetup.STATE.failure);
        return PcSetup.STATE.done ? 0 : 1;
    }

    /** What Play does, with no window, for `seconds`: the tests' run of the game as the launcher starts it. */
    private static int testPlay(PcPaths paths, boolean vr, int seconds, Integer level) throws Exception {
        String version = PcSetup.installedVersion(paths);
        if (version == null) {
            System.out.println("not set up");
            return 1;
        }
        Process process = startGame(paths, new PcConfig(paths), Discs.byVersion(version), vr, level);
        boolean ended = process.waitFor(seconds, java.util.concurrent.TimeUnit.SECONDS);
        if (!ended) {
            process.descendants().forEach(ProcessHandle::destroyForcibly);
            process.destroyForcibly();
            process.waitFor();
        }
        File report = PcRunLog.saveLastRun(paths, ended ? "ended with exit code " + process.exitValue()
            : "was stopped by the test after " + seconds + " s");
        System.out.println("report: " + report);
        return 0;
    }
}
