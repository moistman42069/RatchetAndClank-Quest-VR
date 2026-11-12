// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// The headset's controls in place of the PS2 pad icons in the game's texts (Rerac-vr's
// pad_glyphs.rs, npiriou, ISC): the icons (text bytes 0x10..0x19) of the three fonts, each a 24 x 16
// cell of the font's texture, show the control that sends that button here, named as an Xbox
// controller's (0.1.19, the owner: Xbox style buttons the normal in first and third person; the
// grips are its bumpers, LB and RB). In first person, pad.h's table: A Cross, the right trigger
// Circle, the right stick's click Triangle, the right grip Square, the left grip L1, the left
// trigger R1, X L2, B R2. Where the controllers are a plain pad (third person with Gamepad, the big
// screen), the Xbox controller's own: A Cross, B Circle, X Square, Y Triangle, LB and RB L1 and R1,
// LT and RT L2 and R2.
//
// With OpenRAC's Xbox button icons mod on (on by default from 0.1.19), the fonts already hold its
// coloured Xbox buttons in the game's style, each on the PS2 button it stands for: then nothing is
// painted, and in first person each icon's glyph record is pointed at the cell of the Xbox button
// that does it there (kVrCells). Without it (off, or fonts it does not know), the icons are painted
// here: the game keeps its font textures as 8-bit indices into 256 colours, so each texel painted
// takes the nearest colour the texture has. Plain math (tests/rig_pad_test.cpp).

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace quest::glyphs {

inline constexpr int kCellW = 24;
inline constexpr int kCellH = 16;
inline constexpr float kOpaque = 128.0f;  // the GS's alpha of an opaque texel

enum class Shape { Button, Key, Stick, StickClick };

struct Glyph {
    std::uint8_t code;
    Shape shape;
    const char* label;
};

// The pad icon codes, with the control that sends each in first person (Rerac-vr's table; pad.h's mapping is the same).
inline constexpr std::array<Glyph, 10> kGlyphs = {{
    {0x10, Shape::Button, "A"},       // Cross: jump, confirm
    {0x11, Shape::Key, "RT"},         // Circle: weapons and gadgets
    {0x12, Shape::StickClick, "R"},   // Triangle: talk, the quick select, back
    {0x13, Shape::Key, "RB"},         // Square: the wrench (the right grip)
    {0x14, Shape::Key, "LB"},         // L1: the previous page in menus (the left grip)
    {0x15, Shape::Key, "LT"},         // R1: crouch, the packs; the next page in menus
    {0x16, Shape::Button, "X"},       // L2
    {0x17, Shape::Button, "B"},       // R2: hover, strafe, the Hydro-Pack's boost
    {0x18, Shape::Stick, "R"},        // the right stick
    {0x19, Shape::Stick, "L"},        // the left stick
}};

// Where the controllers are a plain pad (pad.h's plain_pad): an Xbox controller's names, the grips its bumpers.
inline constexpr std::array<Glyph, 10> kPadGlyphs = {{
    {0x10, Shape::Button, "A"},       // Cross
    {0x11, Shape::Button, "B"},       // Circle
    {0x12, Shape::Button, "Y"},       // Triangle
    {0x13, Shape::Button, "X"},       // Square
    {0x14, Shape::Key, "LB"},         // L1: the left grip
    {0x15, Shape::Key, "RB"},         // R1: the right grip
    {0x16, Shape::Key, "LT"},         // L2: the left trigger
    {0x17, Shape::Key, "RT"},         // R2: the right trigger
    {0x18, Shape::Stick, "R"},        // the right stick
    {0x19, Shape::Stick, "L"},        // the left stick
}};

// The controls a plain pad's buttons can be on (pad.h's PadControl order: A, B, X, Y, the right and left triggers, the
// right and left grips, the right and left sticks' clicks): the cell of the Xbox mod's that shows each, and each painted.
// The Third person buttons (0.1.21) point icon i at the control that presses its button (layout_glyph_controls).
inline constexpr std::array<std::uint8_t, 10> kControlCells = {0x10, 0x11, 0x13, 0x12, 0x17, 0x16, 0x15, 0x14, 0x18, 0x19};
inline constexpr std::array<Glyph, 10> kControlGlyphs = {{
    {0, Shape::Button, "A"},
    {0, Shape::Button, "B"},
    {0, Shape::Button, "X"},
    {0, Shape::Button, "Y"},
    {0, Shape::Key, "RT"},
    {0, Shape::Key, "LT"},
    {0, Shape::Key, "RB"},
    {0, Shape::Key, "LB"},
    {0, Shape::StickClick, "R"},
    {0, Shape::StickClick, "L"},
}};

// Each icon's control under a layout (pad.h's PadLayout: the button each control presses), -1 where none presses its
// button (the icon is then drawn as an Xbox controller has it) and for the sticks, which are themselves. `buttons` are the
// PS2 pad bits of the icons 0x10..0x17: Cross, Circle, Triangle, Square, L1, R1, L2, R2.
inline std::array<int, 10> layout_glyph_controls(const std::array<std::uint16_t, 10>& layout,
                                                 const std::array<std::uint16_t, 8>& buttons) {
    std::array<int, 10> controls;
    controls.fill(-1);
    for (std::size_t icon = 0; icon < buttons.size(); ++icon) {
        for (std::size_t control = 0; control < layout.size(); ++control) {
            if (layout[control] == buttons[icon]) {
                controls[icon] = static_cast<int>(control);
                break;
            }
        }
    }
    return controls;
}

// With the Xbox mod's buttons in the fonts (each on the PS2 button an Xbox controller has in its place), the icon whose
// cell each icon shows in first person, from 0x10 on: Cross the A (its own), Circle the RT (R2's), Triangle the right
// stick (the mod keeps the game's sticks: the click is shown as the stick), Square the RB (R1's), L1 the LB (its own),
// R1 the LT (L2's), L2 the X (Square's), R2 the B (Circle's), the sticks their own. Where the controllers are a plain
// pad, each its own.
inline constexpr std::array<std::uint8_t, 10> kVrCells = {0x10, 0x17, 0x18, 0x15, 0x14, 0x16, 0x13, 0x11, 0x18, 0x19};

// A cell being painted: straight colour and the GS's alpha (0..128), all clear to start.
struct Cell {
    float rgba[kCellH][kCellW][4] = {};

    // Source over, in straight colour.
    void blend(int x, int y, const float rgb[3], float cover) {
        if (x < 0 || y < 0 || x >= kCellW || y >= kCellH || cover <= 0.0f) {
            return;
        }
        float* px = rgba[y][x];
        const float sa = std::min(cover, 1.0f), da = px[3] / kOpaque;
        const float a = sa + da * (1.0f - sa);
        for (int c = 0; c < 3; ++c) {
            px[c] = std::clamp(std::round((rgb[c] * sa + px[c] * da * (1.0f - sa)) / a), 0.0f, 255.0f);
        }
        px[3] = std::round(a * kOpaque);
    }

    // Fills what `inside` covers (4 x 4 samples a texel) with `shade(x, y)`.
    template <class Inside, class Shade>
    void fill(Inside inside, Shade shade) {
        for (int y = 0; y < kCellH; ++y) {
            for (int x = 0; x < kCellW; ++x) {
                int n = 0;
                for (int sy = 0; sy < 4; ++sy) {
                    for (int sx = 0; sx < 4; ++sx) {
                        n += inside(x + (sx + 0.5f) / 4.0f, y + (sy + 0.5f) / 4.0f) ? 1 : 0;
                    }
                }
                float rgb[3];
                shade(x + 0.5f, y + 0.5f, rgb);
                blend(x, y, rgb, n / 16.0f);
            }
        }
    }
};

inline void grey(float v, float out[3]) {
    out[0] = v;
    out[1] = v;
    out[2] = v * 1.06f;
}

// A face button: a dark rim, a body lit from above.
inline void disc(Cell& cell, float cx, float cy, float r) {
    const auto d = [=](float x, float y) { return std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)); };
    cell.fill([=](float x, float y) { return d(x, y) <= r; }, [](float, float, float out[3]) { grey(30.0f, out); });
    cell.fill([=](float x, float y) { return d(x, y) <= r - 1.2f; },
              [=](float, float y, float out[3]) { grey(86.0f - (y - cy + r) / (2.0f * r) * 40.0f, out); });
}

// A trigger or grip: a bevelled key, its top and left edges lit.
inline void key(Cell& cell, float x0, float y0, float w, float h) {
    const auto rounded = [=](float x, float y, float inset) {
        const float l = x0 + inset, t = y0 + inset, r = x0 + w - inset, b = y0 + h - inset;
        const float k = 1.5f;
        const float dx = std::max(std::max(l + k - x, x - (r - k)), 0.0f);
        const float dy = std::max(std::max(t + k - y, y - (b - k)), 0.0f);
        return x >= l && x <= r && y >= t && y <= b && dx * dx + dy * dy <= k * k;
    };
    cell.fill([=](float x, float y) { return rounded(x, y, 0.0f); },
              [=](float x, float y, float out[3]) { grey(x - x0 < 1.0f || y - y0 < 1.0f ? 150.0f : 28.0f, out); });
    cell.fill([=](float x, float y) { return rounded(x, y, 1.0f); },
              [=](float, float y, float out[3]) { grey(66.0f - (y - y0) / h * 30.0f, out); });
}

// A thumbstick: the dark base and the cap's lighter rim.
inline void stick(Cell& cell, float cx, float cy, float r) {
    const auto d = [=](float x, float y) { return std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)); };
    cell.fill([=](float x, float y) { return d(x, y) <= r; }, [](float, float, float out[3]) { grey(24.0f, out); });
    cell.fill([=](float x, float y) { return d(x, y) <= r - 1.0f; },
              [](float, float, float out[3]) { grey(118.0f, out); });
    cell.fill([=](float x, float y) { return d(x, y) <= r - 2.2f; },
              [=](float, float y, float out[3]) { grey(70.0f - (y - cy + r) / (2.0f * r) * 34.0f, out); });
}

// A downward arrow over the stick, centred on column `cx`.
inline void press_arrow(Cell& cell, int cx) {
    float light[3];
    grey(235.0f, light);
    const int rows[3][2] = {{0, 2}, {1, 1}, {2, 0}};
    for (const auto& row : rows) {
        for (int x = cx - row[1]; x <= cx + row[1]; ++x) {
            cell.blend(x, row[0], light, 1.0f);
        }
    }
}

struct Letter {
    char c;
    int width;
    const char* rows[9];
};

// Bold letters, two-texel stems.
inline constexpr Letter kLetters[] = {
    {'A', 7, {"..###..", ".##.##.", "##...##", "##...##", "##...##", "#######", "##...##", "##...##", "##...##"}},
    {'X', 7, {"##...##", "##...##", ".##.##.", "..###..", "..###..", "..###..", ".##.##.", "##...##", "##...##"}},
    {'B', 7, {"######.", "##...##", "##...##", "##...##", "######.", "##...##", "##...##", "##...##", "######."}},
    {'R', 6, {"#####.", "##..##", "##..##", "##..##", "#####.", "##.##.", "##..##", "##..##", "##..##"}},
    {'T', 6, {"######", "######", "..##..", "..##..", "..##..", "..##..", "..##..", "..##..", "..##.."}},
    {'G', 6, {".####.", "##..##", "##....", "##....", "##.###", "##..##", "##..##", "##..##", ".####."}},
    {'L', 6, {"##....", "##....", "##....", "##....", "##....", "##....", "##....", "######", "######"}},
    {'Y', 7, {"##...##", "##...##", ".##.##.", "..###..", "..###..", "..###..", "..###..", "..###..", "..###.."}},
};

// The stick caps' smaller letters (seven rows).
inline constexpr Letter kSmallLetters[] = {
    {'R', 5, {"####.", "##.##", "##.##", "####.", "##.#.", "##.##", "##.##"}},
    {'L', 5, {"##...", "##...", "##...", "##...", "##...", "#####", "#####"}},
};

inline void ink(Cell& cell, const Letter& letter, int rows, int x0, int y0) {
    const float white[3] = {238.0f, 238.0f, 244.0f};
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; letter.rows[y][x] != '\0'; ++x) {
            if (letter.rows[y][x] == '#') {
                cell.blend(x0 + x, y0 + y, white, 1.0f);
            }
        }
    }
}

// The label's bold letters, one texel apart, centred on column `cx` from row `y0`.
inline void letters(Cell& cell, const char* label, int cx, int y0) {
    const Letter* found[4] = {};
    int count = 0, width = 0;
    for (const char* c = label; *c != '\0' && count < 4; ++c) {
        for (const Letter& l : kLetters) {
            if (l.c == *c) {
                found[count++] = &l;
                width += l.width;
            }
        }
    }
    width += std::max(count - 1, 0);
    int x = cx + 1 - (width + 1) / 2;
    for (int i = 0; i < count; ++i) {
        ink(cell, *found[i], 9, x, y0);
        x += found[i]->width + 1;
    }
}

inline void small_letter(Cell& cell, const char* label, int cx, int y0) {
    for (const char* c = label; *c != '\0'; ++c) {
        for (const Letter& l : kSmallLetters) {
            if (l.c == *c) {
                ink(cell, l, 7, cx + 1 - (l.width + 1) / 2, y0);
            }
        }
    }
}

// Paints `glyph` in a clear cell; returns its advance (the pen's step past it).
inline int paint(Cell& cell, const Glyph& glyph) {
    switch (glyph.shape) {
    case Shape::Button:
        disc(cell, 9.5f, 8.0f, 7.9f);
        letters(cell, glyph.label, 9, 3);
        return 18;
    case Shape::Key:
        key(cell, 1.0f, 2.0f, 19.0f, 13.0f);
        letters(cell, glyph.label, 10, 4);
        return 21;
    case Shape::Stick:
        stick(cell, 8.5f, 8.5f, 7.4f);
        small_letter(cell, glyph.label, 8, 5);
        return 17;
    case Shape::StickClick:
        stick(cell, 9.5f, 9.5f, 6.4f);
        small_letter(cell, glyph.label, 9, 6);
        press_arrow(cell, 9);
        return 17;
    }
    return 0;
}

// The GS's CSM1 order: an 8-bit index's colour is at the palette entry with bits 3 and 4 swapped.
inline int csm1(int index) { return (index & 0xE7) | ((index & 0x08) << 1) | ((index & 0x10) >> 1); }

// The texture's index nearest a texel's colour, `palette` its 256 colours as they lie in memory
// (r, g, b, a bytes; CSM1 order), compared premultiplied so a clear texel takes a clear colour.
inline std::uint8_t nearest(const std::uint8_t* palette, const float rgba[4]) {
    const float a = rgba[3] / kOpaque;
    int best = 0;
    float best_d = 1e30f;
    for (int i = 0; i < 256; ++i) {
        const std::uint8_t* p = palette + 4 * csm1(i);
        const float pa = std::min(p[3] / kOpaque, 1.0f);
        float d = 0;
        for (int c = 0; c < 3; ++c) {
            const float e = rgba[c] * a - p[c] * pa;
            d += e * e;
        }
        const float ea = (a - pa) * 255.0f;
        d += 2.0f * ea * ea;
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return static_cast<std::uint8_t>(best);
}

// Whether OpenRAC's Xbox button icons mod has painted a font: its A, on the Cross's cell (at u, v in the font's 8-bit
// texture `width` wide), is green, where the game's Cross is blue and this port's own painting grey. At least
// kGreenTexels of the cell's texels opaque and plainly green, `palette` as nearest() takes it.
inline constexpr int kGreenTexels = 12;
inline bool green_a(const std::uint8_t* texels, std::uint32_t width, std::uint32_t u, std::uint32_t v, const std::uint8_t* palette) {
    int green = 0;
    for (std::uint32_t y = 0; y < static_cast<std::uint32_t>(kCellH); ++y) {
        for (std::uint32_t x = 0; x < static_cast<std::uint32_t>(kCellW); ++x) {
            const std::uint8_t* p = palette + 4 * csm1(texels[(v + y) * width + u + x]);
            green += p[3] >= 64 && p[1] >= 90 && p[1] > p[0] + 40 && p[1] > p[2] + 40 ? 1 : 0;
        }
    }
    return green >= kGreenTexels;
}

}  // namespace quest::glyphs
