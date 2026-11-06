// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.math.BigDecimal;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * JSON for the PC's launcher, which has no org.json: objects read as LinkedHashMap (their keys in order), arrays as
 * ArrayList, numbers as BigDecimal, null as null. Writing indents by two spaces, as org.json's toString(2) does for the
 * headset's plugins.json.
 */
final class Json {
    private final String text;
    private int at;

    private Json(String text) {
        this.text = text;
    }

    static Object parse(String text) {
        Json json = new Json(text);
        json.space();
        Object value = json.value(0);
        json.space();
        if (json.at != text.length()) {
            throw json.error("more after the value");
        }
        return value;
    }

    @SuppressWarnings("unchecked")
    static Map<String, Object> object(String text) {
        Object value = parse(text);
        if (!(value instanceof Map)) {
            throw new IllegalArgumentException("not a JSON object");
        }
        return (Map<String, Object>) value;
    }

    @SuppressWarnings("unchecked")
    static Map<String, Object> object(Object value) {
        return value instanceof Map ? (Map<String, Object>) value : null;
    }

    @SuppressWarnings("unchecked")
    static List<Object> array(Object value) {
        return value instanceof List ? (List<Object>) value : null;
    }

    /** A member's text, or `fallback` when it is missing or not text (org.json's optString). */
    static String string(Map<String, Object> object, String key, String fallback) {
        Object value = object == null ? null : object.get(key);
        if (value instanceof String) {
            return (String) value;
        }
        if (value instanceof BigDecimal || value instanceof Boolean) {
            return plain(value);
        }
        return fallback;
    }

    /** A number or other value as the launcher spells it: 1.0 is "1". */
    static String plain(Object value) {
        if (value instanceof BigDecimal) {
            return ((BigDecimal) value).stripTrailingZeros().toPlainString();
        }
        if (value instanceof Double || value instanceof Float) {
            return new BigDecimal(value.toString()).stripTrailingZeros().toPlainString();
        }
        return value == null ? "" : value.toString();
    }

    private IllegalArgumentException error(String what) {
        return new IllegalArgumentException("JSON: " + what + " at character " + at);
    }

    private void space() {
        while (at < text.length() && " \t\r\n".indexOf(text.charAt(at)) >= 0) {
            at++;
        }
    }

    private Object value(int depth) {
        if (depth > 200) {
            throw error("too deep");
        }
        if (at >= text.length()) {
            throw error("the end too soon");
        }
        char c = text.charAt(at);
        switch (c) {
            case '{': return object(depth);
            case '[': return array(depth);
            case '"': return string();
            case 't': return word("true", Boolean.TRUE);
            case 'f': return word("false", Boolean.FALSE);
            case 'n': return word("null", null);
            default: return number();
        }
    }

    private Object word(String word, Object value) {
        if (!text.startsWith(word, at)) {
            throw error("an unknown word");
        }
        at += word.length();
        return value;
    }

    private Map<String, Object> object(int depth) {
        Map<String, Object> map = new LinkedHashMap<>();
        at++;
        space();
        if (at < text.length() && text.charAt(at) == '}') {
            at++;
            return map;
        }
        while (true) {
            space();
            if (at >= text.length() || text.charAt(at) != '"') {
                throw error("a key that is not text");
            }
            String key = string();
            space();
            if (at >= text.length() || text.charAt(at) != ':') {
                throw error("no ':' after a key");
            }
            at++;
            space();
            map.put(key, value(depth + 1));
            space();
            if (at >= text.length()) {
                throw error("the end inside an object");
            }
            char c = text.charAt(at++);
            if (c == '}') {
                return map;
            }
            if (c != ',') {
                throw error("no ',' between members");
            }
        }
    }

    private List<Object> array(int depth) {
        List<Object> list = new ArrayList<>();
        at++;
        space();
        if (at < text.length() && text.charAt(at) == ']') {
            at++;
            return list;
        }
        while (true) {
            space();
            list.add(value(depth + 1));
            space();
            if (at >= text.length()) {
                throw error("the end inside an array");
            }
            char c = text.charAt(at++);
            if (c == ']') {
                return list;
            }
            if (c != ',') {
                throw error("no ',' between values");
            }
        }
    }

    private String string() {
        StringBuilder out = new StringBuilder();
        at++;
        while (true) {
            if (at >= text.length()) {
                throw error("the end inside text");
            }
            char c = text.charAt(at++);
            if (c == '"') {
                return out.toString();
            }
            if (c != '\\') {
                if (c < 0x20) {
                    throw error("a control character in text");
                }
                out.append(c);
                continue;
            }
            if (at >= text.length()) {
                throw error("the end inside text");
            }
            char e = text.charAt(at++);
            switch (e) {
                case '"': out.append('"'); break;
                case '\\': out.append('\\'); break;
                case '/': out.append('/'); break;
                case 'b': out.append('\b'); break;
                case 'f': out.append('\f'); break;
                case 'n': out.append('\n'); break;
                case 'r': out.append('\r'); break;
                case 't': out.append('\t'); break;
                case 'u':
                    if (at + 4 > text.length()) {
                        throw error("a short \\u");
                    }
                    try {
                        out.append((char) Integer.parseInt(text.substring(at, at + 4), 16));
                    } catch (NumberFormatException x) {
                        throw error("a bad \\u");
                    }
                    at += 4;
                    break;
                default:
                    throw error("an unknown escape");
            }
        }
    }

    private BigDecimal number() {
        int start = at;
        while (at < text.length() && "+-0123456789.eE".indexOf(text.charAt(at)) >= 0) {
            at++;
        }
        if (start == at) {
            throw error("an unexpected character");
        }
        try {
            return new BigDecimal(text.substring(start, at));
        } catch (NumberFormatException e) {
            throw error("a bad number");
        }
    }

    // ---- Writing ----

    static String write(Object value) {
        StringBuilder out = new StringBuilder();
        write(out, value, 0);
        return out.toString();
    }

    private static void indent(StringBuilder out, int level) {
        for (int i = 0; i < level; i++) {
            out.append("  ");
        }
    }

    private static void write(StringBuilder out, Object value, int level) {
        if (value == null) {
            out.append("null");
        } else if (value instanceof String) {
            quote(out, (String) value);
        } else if (value instanceof Boolean || value instanceof Integer || value instanceof Long
                || value instanceof BigDecimal) {
            out.append(value instanceof BigDecimal ? ((BigDecimal) value).toPlainString() : value.toString());
        } else if (value instanceof Double || value instanceof Float) {
            double d = ((Number) value).doubleValue();
            if (Double.isNaN(d) || Double.isInfinite(d)) {
                throw new IllegalArgumentException("JSON has no " + d);
            }
            // As org.json writes a double: a whole one without ".0".
            out.append(d == Math.rint(d) && Math.abs(d) < 1e15 ? Long.toString((long) d) : Double.toString(d));
        } else if (value instanceof Map) {
            Map<?, ?> map = (Map<?, ?>) value;
            if (map.isEmpty()) {
                out.append("{}");
                return;
            }
            out.append("{\n");
            int i = 0;
            for (Map.Entry<?, ?> entry : map.entrySet()) {
                indent(out, level + 1);
                quote(out, String.valueOf(entry.getKey()));
                out.append(": ");
                write(out, entry.getValue(), level + 1);
                out.append(++i < map.size() ? ",\n" : "\n");
            }
            indent(out, level);
            out.append('}');
        } else if (value instanceof List) {
            List<?> list = (List<?>) value;
            if (list.isEmpty()) {
                out.append("[]");
                return;
            }
            out.append("[\n");
            for (int i = 0; i < list.size(); i++) {
                indent(out, level + 1);
                write(out, list.get(i), level + 1);
                out.append(i + 1 < list.size() ? ",\n" : "\n");
            }
            indent(out, level);
            out.append(']');
        } else {
            throw new IllegalArgumentException("JSON cannot write a " + value.getClass().getSimpleName());
        }
    }

    private static void quote(StringBuilder out, String text) {
        out.append('"');
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            switch (c) {
                case '"': out.append("\\\""); break;
                case '\\': out.append("\\\\"); break;
                case '\n': out.append("\\n"); break;
                case '\r': out.append("\\r"); break;
                case '\t': out.append("\\t"); break;
                case '\b': out.append("\\b"); break;
                case '\f': out.append("\\f"); break;
                default:
                    if (c < 0x20) {
                        out.append(String.format("\\u%04x", (int) c));
                    } else {
                        out.append(c);
                    }
            }
        }
        out.append('"');
    }
}
