// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

/**
 * What OpenRAC's mods are in VR, for both launchers' Mods (0.1.18, the owner: the launchers show the VR bindings): the
 * pad's buttons a mod names are pressed by the controllers as vr/src/pad.h presses them on foot, and a mod the VR mod
 * knows (RAC-Board) has VR controls of its own. Plain Java: the PC launcher compiles it too (tools/build_pc.py).
 */
final class ModVr {
    private ModVr() {}

    /** The pad's buttons on the controllers, on foot (pad.h's play mapping). */
    static final String PAD = "In VR, the pad buttons a mod names are on your controllers as: Cross A, Circle the right "
        + "trigger, Square a wrench swing (the right grip), Triangle a right stick click, R1 the left trigger, R2 B, Start "
        + "Y, Select X. L1, L2 and the D-pad have no button of their own while you play, so a mod that needs them may "
        + "not work in the headset unless it is listed here with VR controls of its own.";

    /** A mod's own VR controls, or null for one the VR mod does nothing special for. */
    static String controls(String id) {
        if ("rac-board".equals(id)) {
            return "VR controls (whatever pad buttons you pick below): reach behind your back with either hand and click "
                + "its stick to get on the board, and again to get off; the hand buzzes when it is there. Riding: the "
                + "left stick steers, A jumps, the right trigger does tricks (the left stick picks which: back for a "
                + "flip), the right grip grinds a rail, the left trigger or left grip grabs the board. Your view goes "
                + "with the board's flips and spins, smoothly. Y pauses.";
        }
        if ("multiplayer".equals(id)) {
            return "In VR: play with flat players and VR players alike, hosting or joining (the Multiplayer item in the "
                + "title's menu, under Load Game). Your wrench swings, punches and aimed guns, in either hand, count for "
                + "everyone as a flat player's do. In a party there is no slow motion at the weapon wheel (the others' "
                + "world would not slow with yours). When you join someone's party, the Swingshot does not catch "
                + "enemies (the host's game runs them), and a cheat counts only if the host plays in VR with it on too. "
                + "It goes online only when you choose Multiplayer there.";
        }
        return null;
    }
}
