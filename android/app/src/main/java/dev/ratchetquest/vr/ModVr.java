// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

/**
 * What OpenRAC's mods are in VR, for both launchers' Mods (0.1.18, the owner: the launchers show the VR bindings): the
 * pad's buttons a mod names are pressed by the controllers as vr/src/pad.h presses them on foot, and a mod the VR mod
 * knows (RAC-Board) has VR controls of its own. Plain Java: the PC launcher compiles it too (tools/build_pc.py).
 */
final class ModVr {
    private ModVr() {}

    /** The pad's buttons on the controllers: first person's VR table, then the plain pad (pad.h). */
    static final String PAD = "The pad buttons a mod names are on your controllers as follows. In third person with "
        + "Third person controls on Gamepad (and on the Big screen) your controllers are an Xbox controller: Cross A, "
        + "Circle B, Square X, Triangle Y, L1 and R1 the grips, L2 and R2 the triggers, Start the menu button, Select a "
        + "left stick click, R3 a right stick click, and the D-pad as the launcher's D-pad says (a controller held up "
        + "beside your head, to start). In first person (VR controls): Cross A, Circle the right trigger, Square a wrench "
        + "swing (the right grip), Triangle a right stick click, R1 the left trigger, R2 B, Start Y, Select X; L1, L2 and "
        + "the D-pad have no button of their own there, so a mod that needs them may want third person with Gamepad "
        + "unless it is listed here with VR controls of its own.";

    /** A mod's own VR controls, or null for one the VR mod does nothing special for. */
    static String controls(String id) {
        if ("rac-board".equals(id)) {
            return "In first person (whatever pad buttons you pick below): reach behind your back with either hand and "
                + "click its stick to get on the board, and again to get off; the hand buzzes when it is there. Riding: "
                + "the left stick steers to where you look, A jumps. With Weapons on the board (on to start) he keeps his "
                + "weapon: the right trigger fires it and the wheel opens as on foot, the right grip takes the wrench "
                + "(swing your hand), B does tricks (the left stick picks which: back for a flip; with A by a rail, a "
                + "grind), the left grip, or the left trigger with no gun in that hand, grabs the board; the Swingshot "
                + "fired at a target takes him off the board, back on as you let go of a swing in the air (a pull ends "
                + "on foot). With it off, he takes the wrench as he gets on, the right trigger does tricks and the right "
                + "grip grinds a rail. Your view goes with the board's flips and spins, smoothly. Y pauses. In third "
                + "person with Gamepad the buttons below are your controllers as an Xbox controller's (B fires, Y opens "
                + "the quick select, X swings "
                + "the wrench; Board on and off on D-pad Up: hold a controller up beside your head and push its stick "
                + "up), and the reach behind your back works too.";
        }
        if ("multiplayer".equals(id)) {
            return "In VR: play with flat players and VR players alike, hosting or joining (the Multiplayer item in the "
                + "title's menu, under Load Game). Everyone on Ratchet Quest VR 0.1.19 to 0.1.23, on a Quest or on PC, "
                + "sees each other's games in Browse games. A game takes only players whose games match the host's: the same "
                + "Multiplayer (0.12.2), and Difficulty, Item randomizer, Ammo at the most and RAC-Board on or off for "
                + "everyone alike, so 0.1.18 cannot play with 0.1.19 or later. Your wrench swings, punches and aimed guns, "
                + "in either hand, count for everyone as a flat player's do. The weapon wheel's slow motion slows only "
                + "you, in the air, while the others and the world run on; when every player in the party has the wheel "
                + "up at once (all on 0.1.19 or later), time slows for all. With a host on 0.1.19 or later: each VR player's head and hands "
                + "are shown on their figure for the others (a gun in their left hand is not), and the Swingshot "
                + "catches enemies when you join too, the host's game moving them as your hand does. Players on 0.1.23 "
                + "or later also show their fingers and their height as they move (crouching, Feet on the floor, World "
                + "scale), and with a host on 0.1.23 or later a bolt you turn when you join turns the host's too, so "
                + "what it opens opens for everyone. With a host on "
                + "another build, no VR bodies are shown and the Swingshot does not catch enemies when you join. A cheat "
                + "counts only if the host plays in VR with it on too. It goes online only when you choose Multiplayer "
                + "there.";
        }
        return null;
    }
}
