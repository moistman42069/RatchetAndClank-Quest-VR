// SPDX-License-Identifier: GPL-3.0-or-later
// After the Halo CE Decomp Remix launcher's UpdatePolicy (moistman42069).
package dev.ratchetquest.vr;

import java.net.URI;

/** What may replace the app: only this project's releases, by their exact names, from GitHub. */
final class UpdatePolicy {
    static final String REPOSITORY = "moistman42069/RatchetAndClank-Quest-VR";
    static final String LATEST = "https://api.github.com/repos/" + REPOSITORY + "/releases/latest";
    static final String PAGE = "https://github.com/" + REPOSITORY + "/releases/latest";
    static final long MAX_APK = 256L * 1024 * 1024;

    private UpdatePolicy() {}

    /** GitHub's API, its pages and where it keeps release files, over HTTPS only. */
    static boolean allowedUrl(String address) {
        try {
            URI uri = new URI(address);
            String host = uri.getHost();
            return "https".equals(uri.getScheme()) && uri.getUserInfo() == null
                && (uri.getPort() == -1 || uri.getPort() == 443)
                && ("api.github.com".equals(host) || "github.com".equals(host)
                    || "release-assets.githubusercontent.com".equals(host)
                    || "objects.githubusercontent.com".equals(host));
        } catch (Exception e) {
            return false;
        }
    }

    /** The APK a release tagged `vX.Y.Z` carries (tools/build_quest.py names it), or null. */
    static String asset(String tag) {
        int[] version = tag != null && tag.startsWith("v") ? version(tag.substring(1)) : null;
        return version == null ? null : "RatchetQuestVR-" + tag.substring(1) + ".apk";
    }

    /** Whether the release tagged `tag` is newer than the installed `versionName`. */
    static boolean newer(String tag, String versionName) {
        int[] release = tag != null && tag.startsWith("v") ? version(tag.substring(1)) : null;
        int[] installed = version(versionName);
        if (release == null || installed == null) {
            return false;
        }
        for (int i = 0; i < 3; i++) {
            if (release[i] != installed[i]) {
                return release[i] > installed[i];
            }
        }
        return false;
    }

    /** GitHub's digest of a release file: "sha256:" and 64 hex digits. */
    static boolean digest(String digest) {
        return digest != null && digest.matches("sha256:[0-9a-fA-F]{64}");
    }

    private static int[] version(String text) {
        if (text == null || !text.matches("(0|[1-9][0-9]{0,5})\\.(0|[1-9][0-9]{0,5})\\.(0|[1-9][0-9]{0,5})")) {
            return null;
        }
        String[] parts = text.split("\\.");
        return new int[] {Integer.parseInt(parts[0]), Integer.parseInt(parts[1]), Integer.parseInt(parts[2])};
    }
}
