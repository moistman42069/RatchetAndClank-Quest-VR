// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.security.SecureRandom;
import java.util.Locale;

/**
 * The player's Multiplayer name and key, as OpenRAC's launcher keeps them (launcher/core/src/multiplayer.rs): the name
 * online mods show the other players (the bundled Multiplayer's), a random one until the player gives their own, and
 * a key of this installation, which a host's ban is against and which never changes. Both go to the game as
 * OPENRAC_PLAYER_NAME and OPENRAC_IDENTITY_KEY. A mod's string setting whose default is ${player_name} shows the name
 * until the player sets a value of their own (docs/port/MODS.md). Shared by both launchers (tools/build_pc.py).
 */
final class PlayerName {
    static final String ENV = "OPENRAC_PLAYER_NAME";
    static final String KEY_ENV = "OPENRAC_IDENTITY_KEY";
    /** The only placeholder a mod's setting may have for its default. */
    static final String PLACEHOLDER = "${player_name}";
    static final int MIN = 3, MAX = 16;

    /** OpenRAC's halves of a made-up name: an adjective of at most 6 letters, a noun of at most 7. */
    private static final String[] ADJECTIVES = {
        "Brave", "Swift", "Lucky", "Rusty", "Sneaky", "Bold", "Clever", "Mighty", "Shiny", "Quick", "Wild", "Cosmic",
        "Plasma", "Turbo", "Hyper", "Silent", "Fuzzy", "Atomic", "Jolly", "Grumpy", "Daring", "Nimble", "Stormy", "Zesty"};
    private static final String[] NOUNS = {
        "Lombax", "Robot", "Wrench", "Bolt", "Blarg", "Comet", "Nebula", "Rocket", "Gadget", "Hover", "Raritan", "Taunter",
        "Glove", "Visor", "Drone", "Pilot", "Ranger", "Plumber", "Blaster", "Mutant", "Sprout", "Quasar", "Galaxy", "Crate"};
    private static final SecureRandom RANDOM = new SecureRandom();

    private PlayerName() {
    }

    /** 3 to 16 letters (A to Z, either case), digits and underscores. */
    static boolean valid(String name) {
        return name != null && name.length() >= MIN && name.length() <= MAX && name.matches("[A-Za-z0-9_]+");
    }

    /** Why `name` cannot be one, in the player's words; null when it can. */
    static String refusal(String name) {
        return valid(name) ? null : "A Multiplayer name is " + MIN + " to " + MAX + " letters, digits or underscores (_), "
            + "no spaces.";
    }

    static boolean validKey(String key) {
        return key != null && key.matches("[0-9a-fA-F]{32}");
    }

    /** A name such as SwiftLombax427: OpenRAC's random_name_from, for any seed always a valid one. */
    static String fromSeed(long seed) {
        String adjective = ADJECTIVES[(int) Long.remainderUnsigned(seed, ADJECTIVES.length)];
        long rest = Long.divideUnsigned(seed, ADJECTIVES.length);
        String noun = NOUNS[(int) Long.remainderUnsigned(rest, NOUNS.length)];
        rest = Long.divideUnsigned(rest, NOUNS.length);
        boolean three = Long.remainderUnsigned(rest, 2) == 1 && adjective.length() + noun.length() + 3 <= MAX;
        long half = Long.divideUnsigned(rest, 2);
        String digits = three ? String.format(Locale.ROOT, "%03d", Long.remainderUnsigned(half, 1000))
                              : String.format(Locale.ROOT, "%02d", Long.remainderUnsigned(half, 100));
        String name = adjective + noun + digits;
        return name.length() > MAX ? name.substring(0, MAX) : name;
    }

    static String random() {
        return fromSeed(RANDOM.nextLong());
    }

    /** 128 random bits as hex, nothing of the player in them. */
    static String newKey() {
        return String.format(Locale.ROOT, "%016x%016x", RANDOM.nextLong(), RANDOM.nextLong());
    }
}
