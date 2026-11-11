// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.math.BigInteger;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.Instant;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Base64;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.function.Function;

/**
 * OpenRAC's mod list, as both launchers browse it (0.1.18, the owner: a mod browser, as openrac.dev/mods has them). It
 * matches OpenRAC's own launcher (third_party/OpenRAC/launcher/core/src/index.rs, resolver.rs, remote.rs): the one
 * index openrac-mods publishes (format 1), fetched with its minisign signature and checked with OpenRAC's own key
 * before any of it is read; a lower serial than one seen is refused; a good copy is kept and used offline; past its
 * expiry it is only said. A mod's newest version that is not a prerelease, not taken back (revoked), for this game,
 * of a mod interface this game has, with an archive for this platform (a native mod's) or for any (a script's), is the
 * one offered; its archive comes from its repository's release, its size and SHA-256 checked against the list.
 *
 * Plain Java (no Android, no Swing): the PC launcher compiles it too (tools/build_pc.py SHARED_JAVA). The JSON is read
 * by each launcher's own reader into maps and lists (`parse`).
 */
final class ModIndex {
    /** openrac-mods' index (index.rs INDEXES). */
    static final String URL = "https://github.com/OpenRAC/openrac-mods/releases/download/index/index.json";
    /** The same mods on the web. */
    static final String SITE = "https://openrac.dev/mods";
    /** index.rs INDEX_PUBLIC_KEY: minisign key 5466767758D8C8D1, as base64 of its .pub file. */
    static final String PUBLIC_KEY = "dW50cnVzdGVkIGNvbW1lbnQ6IG1pbmlzaWduIHB1YmxpYyBrZXkgNTQ2Njc2Nzc1OEQ4QzhEMQpSV1RSeU5oWWQzWm1WRDZEN2Jwd2d4MGdrYlZ6THJlUDc4dm5LeHRMVkp6UWZ6aGFnM0piWDFaYwo=";
    /** The index format read here (index.rs FORMAT). */
    static final int FORMAT = 1;
    static final String GAME = "rac1";
    private static final int MOST_INDEX = 32 << 20;
    private static final int MOST_ARCHIVE = 1 << 30;

    /** One mod as offered: the version to add, or why none can be. */
    static final class Offer {
        final String id, name, author, description, kind, homepage;
        /** The version offered; "" when none can be added here. */
        final String version;
        /** Why none can be added here, in the player's words; null when one can. */
        final String why;
        final String url, asset, sha256;
        final long size;
        final boolean aiAssisted, featured;
        final List<String> impact;
        final long downloads;

        Offer(String id, String name, String author, String description, String kind, String homepage, String version,
              String why, String url, String asset, String sha256, long size, boolean aiAssisted, boolean featured,
              List<String> impact, long downloads) {
            this.id = id;
            this.name = name;
            this.author = author;
            this.description = description;
            this.kind = kind;
            this.homepage = homepage;
            this.version = version;
            this.why = why;
            this.url = url;
            this.asset = asset;
            this.sha256 = sha256;
            this.size = size;
            this.aiAssisted = aiAssisted;
            this.featured = featured;
            this.impact = impact;
            this.downloads = downloads;
        }
    }

    final List<Offer> offers;
    final long serial;
    /** Past its `expires`: said quietly, nothing blocked. */
    final boolean stale;
    /** The copy kept from before was used: the fetch failed (`problem` says why). */
    final boolean offline;
    final String problem;
    private final List<Object> revoked;

    private ModIndex(List<Offer> offers, long serial, boolean stale, boolean offline, String problem, List<Object> revoked) {
        this.offers = offers;
        this.serial = serial;
        this.stale = stale;
        this.offline = offline;
        this.problem = problem;
        this.revoked = revoked;
    }

    /** Why `id` at `version` was taken back, or null. */
    String revoked(String id, String version) {
        for (Object each : revoked) {
            Map<String, Object> r = map(each);
            if (r != null && id.equals(text(r, "id", ""))) {
                for (Object v : list(r.get("versions"))) {
                    if ("*".equals(v) || version.equals(v)) {
                        return text(r, "reason", "");
                    }
                }
            }
        }
        return null;
    }

    Offer get(String id) {
        for (Offer offer : offers) {
            if (offer.id.equals(id)) {
                return offer;
            }
        }
        return null;
    }

    // ---- The list's version of a mod a player has, built in or added (0.1.19) ------------------------------------
    // The owner: "we shouldnt have mismatches" (a party needs every game's Multiplayer, and each gameplay mod, at the same
    // version: a release's built-in one fell behind OpenRAC's list, or went ahead of it). A mod's card offers the list's
    // version whenever it differs from the one the player has; added, it takes a built-in's place (BundledMods, PcMods)
    // until the player goes back to the built-in.

    /** Built-in mods this project changes, which the list's version would undo: RAC-Board (its Weapons on the board). */
    static boolean keepsBuiltIn(String id) {
        return "rac-board".equals(id);
    }

    /** The list's version of a mod the player has at `have`, built in or added; null when there is none to take. */
    Offer differing(String id, String have, boolean builtIn) {
        Offer offer = get(id);
        if (offer == null || offer.why != null || offer.version.isEmpty() || offer.version.equals(have)
            || (builtIn && keepsBuiltIn(id))) {
            return null;
        }
        return offer;
    }

    /** The button for it: "Update to 0.12.3", or, the list's being older than the one the player has, its own words. */
    static String takeLabel(String have, Offer offer) {
        return compareVersions(offer.version, have) > 0 ? "Update to " + offer.version
            : "Use " + offer.version + ", as OpenRAC's mod list has it";
    }

    /** Versions number by number ("0.12.10" after "0.12.9"); a part that is not a number by its text. */
    static int compareVersions(String a, String b) {
        String[] x = a.split("[.+-]"), y = b.split("[.+-]");
        for (int i = 0; i < Math.max(x.length, y.length); i++) {
            String p = i < x.length ? x[i] : "0", q = i < y.length ? y[i] : "0";
            int c = p.matches("\\d{1,18}") && q.matches("\\d{1,18}") ? Long.compare(Long.parseLong(p), Long.parseLong(q))
                : p.compareTo(q);
            if (c != 0) {
                return c < 0 ? -1 : 1;
            }
        }
        return 0;
    }

    // ---- Fetching, with the copy kept ----------------------------------------------------------------------------

    /**
     * The index: fetched and checked, kept in `cache` (index.json, its .minisig, the highest serial); when the fetch
     * fails or is refused, the copy kept, if it checks. An IOException when there is neither, its message the
     * player's words.
     */
    static ModIndex fetch(File cache, String agent, String platform, int apiMajor, int apiMinor,
                          Function<String, Map<String, Object>> parse) throws IOException {
        cache.mkdirs();
        File json = new File(cache, "index.json"), sig = new File(cache, "index.json.minisig"), seen = new File(cache, "serial");
        long highest = 0;
        try {
            highest = Long.parseLong(new String(Files.readAllBytes(seen.toPath()), StandardCharsets.UTF_8).trim());
        } catch (IOException | NumberFormatException e) {
            // None seen yet.
        }
        String problem;
        try {
            byte[] body = get(URL, agent, MOST_INDEX);
            String signature = new String(get(URL + ".minisig", agent, 1 << 16), StandardCharsets.UTF_8);
            ModIndex index = read(body, signature, PUBLIC_KEY, highest, platform, apiMajor, apiMinor, parse, false, "");
            Files.write(json.toPath(), body);
            Files.write(sig.toPath(), signature.getBytes(StandardCharsets.UTF_8));
            Files.write(seen.toPath(), Long.toString(Math.max(highest, index.serial)).getBytes(StandardCharsets.UTF_8));
            return index;
        } catch (IOException | RuntimeException e) {
            problem = e.getMessage() == null ? e.toString() : e.getMessage();
        }
        if (json.isFile() && sig.isFile()) {
            try {
                return read(Files.readAllBytes(json.toPath()), new String(Files.readAllBytes(sig.toPath()), StandardCharsets.UTF_8),
                    PUBLIC_KEY, 0, platform, apiMajor, apiMinor, parse, true, problem);
            } catch (IOException | RuntimeException e) {
                // The copy does not check either.
            }
        }
        throw new IOException("OpenRAC's mod list could not be read (" + problem + ").");
    }

    /** The index from its bytes and signature: refused unless the signature checks, the format is read, the serial is new enough. */
    static ModIndex read(byte[] body, String signature, String key, long highest, String platform, int apiMajor, int apiMinor,
                         Function<String, Map<String, Object>> parse, boolean offline, String problem) throws IOException {
        if (!Minisign.verify(body, signature, key)) {
            throw new IOException("its signature does not check");
        }
        Map<String, Object> root;
        try {
            root = parse.apply(new String(body, StandardCharsets.UTF_8));
        } catch (RuntimeException e) {
            throw new IOException("it cannot be read");
        }
        if (root == null) {
            throw new IOException("it cannot be read");
        }
        long format = number(root.get("format"));
        if (format > FORMAT) {
            throw new IOException("it is a newer format (" + format + "): this launcher needs an update to read it");
        }
        long serial = number(root.get("serial"));
        if (serial < highest) {
            throw new IOException("it is older than one already seen");
        }
        boolean stale = false;
        String expires = text(root, "expires", "");
        if (!expires.isEmpty()) {
            try {
                stale = Instant.parse(expires).isBefore(Instant.now());
            } catch (RuntimeException e) {
                // Unreadable: not stale.
            }
        }
        List<Object> revoked = list(root.get("revoked"));
        ModIndex index = new ModIndex(new ArrayList<>(), serial, stale, offline, problem, revoked);
        for (Object each : list(root.get("mods"))) {
            Map<String, Object> mod = map(each);
            if (mod != null && !text(mod, "id", "").isEmpty()) {
                Offer offer = offer(index, mod, platform, apiMajor, apiMinor);
                if (offer != null) {
                    index.offers.add(offer);
                }
            }
        }
        return index;
    }

    /** What a mod of the index is here: null for one not for this game at all. */
    private static Offer offer(ModIndex index, Map<String, Object> mod, String platform, int apiMajor, int apiMinor) {
        String id = text(mod, "id", "");
        Map<String, Object> games = map(mod.get("games"));
        if (games != null && !games.isEmpty() && !games.containsKey(GAME)) {
            return null;
        }
        String kind = text(mod, "type", "script");
        String name = text(mod, "name", id);
        String why = null;
        String repo = text(mod, "repo", "").replaceAll("/+$", "");
        for (Object each : list(mod.get("versions"))) {  // newest first
            Map<String, Object> v = map(each);
            if (v == null || Boolean.TRUE.equals(v.get("prerelease"))) {
                continue;
            }
            String version = text(v, "version", "");
            Map<String, Object> vGames = map(v.get("games"));
            String api = text(v, "api", "");
            String reason;
            Map<String, Object> asset = map(map(v.get("platforms")) == null ? null
                : map(v.get("platforms")).get(kind.equals("native") ? platform : "any"));
            if (index.revoked(id, version) != null) {
                reason = name + " " + version + " was taken back: " + index.revoked(id, version);
            } else if (vGames != null && !vGames.isEmpty() && !vGames.containsKey(GAME)) {
                reason = name + " " + version + " is for another game.";
            } else if (!api.isEmpty() && !apiFits(api, apiMajor, apiMinor)) {
                reason = name + " " + version + " needs OpenRAC's mod interface " + api + "; this game has " + apiMajor + "."
                    + apiMinor + ".";
            } else if (kind.equals("pack")) {
                reason = name + " is a set of other mods: add each of them here.";
            } else if (asset == null) {
                reason = kind.equals("native") ? name + " " + version + " has no build for this " + where(platform) + " (" + platform + ")."
                    : name + " " + version + " has no archive to add here: its page on " + SITE + " says where it is.";
            } else if (repo.isEmpty() || text(v, "tag", "").isEmpty() || text(asset, "asset", "").isEmpty()
                       || text(asset, "sha256", "").length() != 64) {
                reason = name + " " + version + " is listed without its archive's place or checksum.";
            } else {
                String file = text(asset, "asset", "");
                String url = repo + "/releases/download/" + text(v, "tag", "") + "/" + file;
                return new Offer(id, name, text(mod, "author", ""), text(mod, "description", ""), kind,
                    text(mod, "homepage", repo), version, null, url, file, text(asset, "sha256", "").toLowerCase(Locale.ROOT),
                    number(asset.get("size")), Boolean.TRUE.equals(mod.get("aiAssisted")), Boolean.TRUE.equals(mod.get("featured")),
                    strings(mod.get("impact")), number(mod.get("downloads")));
            }
            if (why == null) {
                why = reason;  // the newest's
            }
        }
        return new Offer(id, name, text(mod, "author", ""), text(mod, "description", ""), kind, text(mod, "homepage", repo), "",
            why == null ? name + " has no version to add." : why, "", "", "", 0, Boolean.TRUE.equals(mod.get("aiAssisted")),
            Boolean.TRUE.equals(mod.get("featured")), strings(mod.get("impact")), number(mod.get("downloads")));
    }

    private static String where(String platform) {
        return platform.startsWith("android") ? "headset" : "PC";
    }

    /** `api` "MAJOR.MINOR" runs on a game of the interface apiMajor.apiMinor. */
    static boolean apiFits(String api, int apiMajor, int apiMinor) {
        String[] parts = api.split("\\.");
        try {
            int major = Integer.parseInt(parts[0].trim());
            int minor = parts.length > 1 ? Integer.parseInt(parts[1].trim()) : 0;
            return major == apiMajor && minor <= apiMinor;
        } catch (NumberFormatException e) {
            return false;
        }
    }

    /** An offer's archive, its size and SHA-256 those the list gives: an IOException in the player's words when not. */
    static byte[] download(Offer offer, String agent) throws IOException {
        if (offer.why != null || offer.url.isEmpty()) {
            throw new IOException(offer.why == null ? "Nothing to add." : offer.why);
        }
        byte[] bytes = get(offer.url, agent, MOST_ARCHIVE);
        if (offer.size > 0 && bytes.length != offer.size) {
            throw new IOException(offer.name + "'s file is not the size OpenRAC's mod list gives: not added.");
        }
        if (!sha256(bytes).equals(offer.sha256)) {
            throw new IOException(offer.name + "'s file is not the one OpenRAC's mod list names (its SHA-256): not added.");
        }
        return bytes;
    }

    /** A GitHub address's bytes, each redirect checked against UpdatePolicy before it is followed. */
    private static byte[] get(String address, String agent, int limit) throws IOException {
        for (int i = 0; i < 6; i++) {
            if (!UpdatePolicy.allowedUrl(address)) {
                throw new IOException("an address outside GitHub");
            }
            HttpURLConnection connection = (HttpURLConnection) new URL(address).openConnection();
            try {
                connection.setConnectTimeout(20000);
                connection.setReadTimeout(30000);
                connection.setInstanceFollowRedirects(false);
                connection.setRequestProperty("User-Agent", agent);
                int code = connection.getResponseCode();
                if (code >= 300 && code < 400) {
                    String next = connection.getHeaderField("Location");
                    if (next == null) {
                        throw new IOException("a redirect to nowhere");
                    }
                    address = new URL(new URL(address), next).toString();
                    continue;
                }
                if (code != 200) {
                    throw new IOException("GitHub answered " + code);
                }
                try (InputStream in = connection.getInputStream(); ByteArrayOutputStream out = new ByteArrayOutputStream()) {
                    byte[] buffer = new byte[65536];
                    for (int n = in.read(buffer); n != -1; n = in.read(buffer)) {
                        if (out.size() + n > limit) {
                            throw new IOException("the file is too large");
                        }
                        out.write(buffer, 0, n);
                    }
                    return out.toByteArray();
                }
            } finally {
                connection.disconnect();
            }
        }
        throw new IOException("too many redirects");
    }

    static String sha256(byte[] bytes) {
        try {
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(bytes);
            StringBuilder out = new StringBuilder();
            for (byte b : digest) {
                out.append(String.format(Locale.ROOT, "%02x", b & 0xff));
            }
            return out.toString();
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException(e);
        }
    }

    // ---- The JSON as maps and lists ------------------------------------------------------------------------------

    @SuppressWarnings("unchecked")
    static Map<String, Object> map(Object value) {
        return value instanceof Map ? (Map<String, Object>) value : null;
    }

    @SuppressWarnings("unchecked")
    static List<Object> list(Object value) {
        return value instanceof List ? (List<Object>) value : new ArrayList<>();
    }

    static String text(Map<String, Object> object, String key, String fallback) {
        Object value = object == null ? null : object.get(key);
        return value instanceof String ? (String) value : fallback;
    }

    static long number(Object value) {
        if (value instanceof Number) {
            return ((Number) value).longValue();
        }
        return 0;
    }

    static List<String> strings(Object value) {
        List<String> out = new ArrayList<>();
        for (Object each : list(value)) {
            if (each instanceof String) {
                out.add((String) each);
            }
        }
        return out;
    }

    // ---- minisign: Ed25519, legacy or over BLAKE2b-512 (prehashed), and its trusted comment ----------------------

    /** minisign's check of a file, as OpenRAC's launcher makes it (minisign-verify): both signatures. */
    static final class Minisign {
        private Minisign() {}

        /**
         * `signature`: the .minisig's text, or base64 of it; `key`: base64 of the .pub file, its text, or its key line.
         * False for any that does not check or cannot be read.
         */
        static boolean verify(byte[] data, String signature, String key) {
            try {
                byte[] pub = publicKey(key);
                String[] lines = lines(textOrBase64(signature));
                if (pub == null || lines.length < 4 || !lines[2].startsWith("trusted comment: ")) {
                    return false;
                }
                byte[] sig = Base64.getDecoder().decode(lines[1].trim());
                byte[] global = Base64.getDecoder().decode(lines[3].trim());
                if (sig.length != 74 || global.length != 64 || !Arrays.equals(Arrays.copyOfRange(sig, 2, 10), Arrays.copyOfRange(pub, 2, 10))) {
                    return false;
                }
                byte[] message;
                if (sig[0] == 'E' && sig[1] == 'd') {
                    message = data;
                } else if (sig[0] == 'E' && sig[1] == 'D') {
                    message = Blake2b.digest512(data);
                } else {
                    return false;
                }
                byte[] key32 = Arrays.copyOfRange(pub, 10, 42);
                byte[] signed = Arrays.copyOfRange(sig, 10, 74);
                if (!Ed25519.verify(key32, message, signed)) {
                    return false;
                }
                byte[] comment = lines[2].substring("trusted comment: ".length()).getBytes(StandardCharsets.UTF_8);
                byte[] both = new byte[64 + comment.length];
                System.arraycopy(signed, 0, both, 0, 64);
                System.arraycopy(comment, 0, both, 64, comment.length);
                return Ed25519.verify(key32, both, global);
            } catch (IllegalArgumentException e) {
                return false;
            }
        }

        private static String textOrBase64(String text) {
            String trimmed = text.trim();
            if (trimmed.startsWith("untrusted comment:")) {
                return trimmed;
            }
            return new String(Base64.getDecoder().decode(trimmed.replaceAll("\\s", "")), StandardCharsets.UTF_8).trim();
        }

        private static String[] lines(String text) {
            return text.replace("\r", "").split("\n");
        }

        /** The key's 42 bytes ("Ed", its id, the Ed25519 key), or null. */
        static byte[] publicKey(String key) {
            String text = key.trim();
            String line;
            if (text.startsWith("untrusted comment:")) {
                String[] lines = lines(text);
                line = lines.length > 1 ? lines[1] : "";
            } else {
                byte[] raw = Base64.getDecoder().decode(text.replaceAll("\\s", ""));
                String decoded = new String(raw, StandardCharsets.UTF_8);
                if (decoded.startsWith("untrusted comment:")) {
                    String[] lines = lines(decoded.trim());
                    line = lines.length > 1 ? lines[1] : "";
                } else {
                    line = text;
                }
            }
            byte[] pub = Base64.getDecoder().decode(line.trim());
            return pub.length == 42 && pub[0] == 'E' && pub[1] == 'd' ? pub : null;
        }
    }

    /** BLAKE2b (RFC 7693), unkeyed, 64 bytes out: minisign's prehash. */
    static final class Blake2b {
        private static final long[] IV = {0x6a09e667f3bcc908L, 0xbb67ae8584caa73bL, 0x3c6ef372fe94f82bL, 0xa54ff53a5f1d36f1L,
            0x510e527fade682d1L, 0x9b05688c2b3e6c1fL, 0x1f83d9abfb41bd6bL, 0x5be0cd19137e2179L};
        private static final byte[][] SIGMA = {
            {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}, {14, 10, 4, 8, 9, 15, 13, 6, 1, 12, 0, 2, 11, 7, 5, 3},
            {11, 8, 12, 0, 5, 2, 15, 13, 10, 14, 3, 6, 7, 1, 9, 4}, {7, 9, 3, 1, 13, 12, 11, 14, 2, 6, 5, 10, 4, 0, 15, 8},
            {9, 0, 5, 7, 2, 4, 10, 15, 14, 1, 11, 12, 6, 8, 3, 13}, {2, 12, 6, 10, 0, 11, 8, 3, 4, 13, 7, 5, 15, 14, 1, 9},
            {12, 5, 1, 15, 14, 13, 4, 10, 0, 7, 6, 3, 9, 2, 8, 11}, {13, 11, 7, 14, 12, 1, 3, 9, 5, 0, 15, 4, 8, 6, 2, 10},
            {6, 15, 14, 9, 11, 3, 0, 8, 12, 2, 13, 7, 1, 4, 10, 5}, {10, 2, 8, 4, 7, 6, 1, 5, 15, 11, 9, 14, 3, 12, 13, 0}};

        private Blake2b() {}

        static byte[] digest512(byte[] data) {
            long[] h = IV.clone();
            h[0] ^= 0x01010000L ^ 64L;
            long counted = 0;
            int at = 0;
            while (data.length - at > 128) {
                counted += 128;
                compress(h, data, at, counted, false);
                at += 128;
            }
            byte[] last = new byte[128];
            System.arraycopy(data, at, last, 0, data.length - at);
            counted += data.length - at;
            compress(h, last, 0, counted, true);
            byte[] out = new byte[64];
            for (int i = 0; i < 64; i++) {
                out[i] = (byte) (h[i / 8] >>> (8 * (i % 8)));
            }
            return out;
        }

        private static void compress(long[] h, byte[] block, int at, long counted, boolean last) {
            long[] m = new long[16];
            for (int i = 0; i < 16; i++) {
                long word = 0;
                for (int b = 7; b >= 0; b--) {
                    word = (word << 8) | (block[at + 8 * i + b] & 0xffL);
                }
                m[i] = word;
            }
            long[] v = new long[16];
            System.arraycopy(h, 0, v, 0, 8);
            System.arraycopy(IV, 0, v, 8, 8);
            v[12] ^= counted;
            if (last) {
                v[14] = ~v[14];
            }
            for (int r = 0; r < 12; r++) {
                byte[] s = SIGMA[r % 10];
                mix(v, 0, 4, 8, 12, m[s[0]], m[s[1]]);
                mix(v, 1, 5, 9, 13, m[s[2]], m[s[3]]);
                mix(v, 2, 6, 10, 14, m[s[4]], m[s[5]]);
                mix(v, 3, 7, 11, 15, m[s[6]], m[s[7]]);
                mix(v, 0, 5, 10, 15, m[s[8]], m[s[9]]);
                mix(v, 1, 6, 11, 12, m[s[10]], m[s[11]]);
                mix(v, 2, 7, 8, 13, m[s[12]], m[s[13]]);
                mix(v, 3, 4, 9, 14, m[s[14]], m[s[15]]);
            }
            for (int i = 0; i < 8; i++) {
                h[i] ^= v[i] ^ v[i + 8];
            }
        }

        private static void mix(long[] v, int a, int b, int c, int d, long x, long y) {
            v[a] = v[a] + v[b] + x;
            v[d] = Long.rotateRight(v[d] ^ v[a], 32);
            v[c] = v[c] + v[d];
            v[b] = Long.rotateRight(v[b] ^ v[c], 24);
            v[a] = v[a] + v[b] + y;
            v[d] = Long.rotateRight(v[d] ^ v[a], 16);
            v[c] = v[c] + v[d];
            v[b] = Long.rotateRight(v[b] ^ v[c], 63);
        }
    }

    /** Ed25519's check (RFC 8032 5.1.7), in BigInteger: one signature now and then, the same on the PC and the headset. */
    static final class Ed25519 {
        private static final BigInteger TWO = BigInteger.valueOf(2);  // BigInteger.TWO is newer than the headset's Android
        private static final BigInteger P = BigInteger.ONE.shiftLeft(255).subtract(BigInteger.valueOf(19));
        private static final BigInteger L = BigInteger.ONE.shiftLeft(252).add(new BigInteger("27742317777372353535851937790883648493"));
        private static final BigInteger D = BigInteger.valueOf(-121665).multiply(BigInteger.valueOf(121666).modInverse(P)).mod(P);
        private static final BigInteger I = TWO.modPow(P.subtract(BigInteger.ONE).shiftRight(2), P);
        private static final BigInteger[] B;

        static {
            BigInteger y = BigInteger.valueOf(4).multiply(BigInteger.valueOf(5).modInverse(P)).mod(P);
            BigInteger x = recoverX(y, 0);
            B = new BigInteger[] {x, y, BigInteger.ONE, x.multiply(y).mod(P)};
        }

        private Ed25519() {}

        static boolean verify(byte[] key, byte[] message, byte[] signature) {
            if (key.length != 32 || signature.length != 64) {
                return false;
            }
            BigInteger[] a = decompress(key);
            BigInteger[] r = decompress(Arrays.copyOfRange(signature, 0, 32));
            if (a == null || r == null) {
                return false;
            }
            BigInteger s = little(Arrays.copyOfRange(signature, 32, 64));
            if (s.compareTo(L) >= 0) {
                return false;
            }
            byte[] hash;
            try {
                MessageDigest sha = MessageDigest.getInstance("SHA-512");
                sha.update(signature, 0, 32);
                sha.update(key);
                sha.update(message);
                hash = sha.digest();
            } catch (NoSuchAlgorithmException e) {
                return false;
            }
            BigInteger h = little(hash).mod(L);
            return equal(multiply(s, B), add(r, multiply(h, a)));
        }

        private static BigInteger little(byte[] bytes) {
            byte[] big = new byte[bytes.length + 1];
            for (int i = 0; i < bytes.length; i++) {
                big[bytes.length - i] = bytes[i];
            }
            return new BigInteger(big);
        }

        private static BigInteger recoverX(BigInteger y, int sign) {
            if (y.compareTo(P) >= 0) {
                return null;
            }
            BigInteger y2 = y.multiply(y);
            BigInteger x2 = y2.subtract(BigInteger.ONE).multiply(D.multiply(y2).add(BigInteger.ONE).modInverse(P)).mod(P);
            if (x2.signum() == 0) {
                return sign == 1 ? null : BigInteger.ZERO;
            }
            BigInteger x = x2.modPow(P.add(BigInteger.valueOf(3)).shiftRight(3), P);
            if (x.multiply(x).subtract(x2).mod(P).signum() != 0) {
                x = x.multiply(I).mod(P);
            }
            if (x.multiply(x).subtract(x2).mod(P).signum() != 0) {
                return null;
            }
            if (x.testBit(0) != (sign == 1)) {
                x = P.subtract(x);
            }
            return x;
        }

        private static BigInteger[] decompress(byte[] bytes) {
            BigInteger y = little(bytes);
            int sign = y.testBit(255) ? 1 : 0;
            y = y.clearBit(255);
            BigInteger x = recoverX(y, sign);
            return x == null ? null : new BigInteger[] {x, y, BigInteger.ONE, x.multiply(y).mod(P)};
        }

        private static BigInteger[] add(BigInteger[] p, BigInteger[] q) {
            BigInteger a = p[1].subtract(p[0]).multiply(q[1].subtract(q[0])).mod(P);
            BigInteger b = p[1].add(p[0]).multiply(q[1].add(q[0])).mod(P);
            BigInteger c = p[3].multiply(TWO).multiply(D).multiply(q[3]).mod(P);
            BigInteger d = p[2].multiply(TWO).multiply(q[2]).mod(P);
            BigInteger e = b.subtract(a), f = d.subtract(c), g = d.add(c), h = b.add(a);
            return new BigInteger[] {e.multiply(f).mod(P), g.multiply(h).mod(P), f.multiply(g).mod(P), e.multiply(h).mod(P)};
        }

        private static BigInteger[] multiply(BigInteger s, BigInteger[] p) {
            BigInteger[] q = {BigInteger.ZERO, BigInteger.ONE, BigInteger.ONE, BigInteger.ZERO};
            BigInteger[] at = p;
            for (int i = 0; i < s.bitLength(); i++) {
                if (s.testBit(i)) {
                    q = add(q, at);
                }
                at = add(at, at);
            }
            return q;
        }

        private static boolean equal(BigInteger[] p, BigInteger[] q) {
            return p[0].multiply(q[2]).subtract(q[0].multiply(p[2])).mod(P).signum() == 0
                && p[1].multiply(q[2]).subtract(q[1].multiply(p[2])).mod(P).signum() == 0;
        }
    }
}
