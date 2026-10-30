// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

/**
 * The discs the game can be set up from: OpenRAC's own table (games/rac1/game.json). Everything the
 * launcher says about which disc is needed comes from here, so the words and the check cannot part.
 */
final class Discs {
    private Discs() {}

    static final class Disc {
        final String version, region, serial, discVersion, sha1, library, hint;
        final long size;

        Disc(String version, String region, String serial, String discVersion, long size, String sha1,
                String hint) {
            this.version = version;
            this.region = region;
            this.serial = serial;
            this.discVersion = discVersion;
            this.size = size;
            this.sha1 = sha1;
            this.library = "openrac-rac1-" + version;
            this.hint = hint;
        }

        String line() {
            return region + "  ·  " + serial + "  ·  disc " + discVersion + "  ·  "
                + String.format(java.util.Locale.US, "%,d", size) + " bytes" + hint;
        }
    }

    static final Disc PAL = new Disc("pal", "PAL (Europe)", "SCES-50916", "v2.00", 4214784000L,
        "ef304c641c7cd426ba492707dda5974679befbc0", "  ·  best supported");
    static final Disc NTSC = new Disc("ntsc", "NTSC-U (North America)", "SCUS-97199", "v1.00", 4214095872L,
        "aee9275384f739c9b24cd5de6f7e200292db7b01", "");
    static final Disc[] ALL = {PAL, NTSC};

    /** The one game this app plays, in the words the player's own copy is listed under. */
    static final String TITLE = "Ratchet & Clank (2002, PlayStation 2)";

    static final String REQUIREMENT =
        "You need your own disc image of " + TITLE + " as an .iso file.\n\nExactly one of these two:\n\n"
        + "•  " + PAL.line() + "\n\n•  " + NTSC.line() + "\n\n"
        + "Nothing else works: not Greatest Hits or Platinum releases, not other regions or languages, "
        + "and not the sequels. No game files come with this app.\n\n"
        + "Put the .iso in the headset's Download folder (or anywhere the file picker can see) before you "
        + "choose it. Setting up needs about 6 GB free, and the disc is checked before anything is kept.";

    static Disc bySize(long size) {
        for (Disc disc : ALL) {
            if (disc.size == size) {
                return disc;
            }
        }
        return null;
    }

    /** The sentence for a file of another size: what it is, what was wanted. */
    static String wrongSize(long size) {
        return "This file is " + String.format(java.util.Locale.US, "%,d", size)
            + " bytes, which is not a supported disc.\n\nSupported:\n•  " + PAL.line() + "\n•  " + NTSC.line()
            + "\n\nGreatest Hits, Platinum and other regional releases have different sizes and do not work.";
    }

    static Disc byVersion(String version) {
        for (Disc disc : ALL) {
            if (disc.version.equals(version)) {
                return disc;
            }
        }
        return null;
    }
}
