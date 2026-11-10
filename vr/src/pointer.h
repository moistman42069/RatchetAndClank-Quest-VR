// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// The menu pointer (Rerac-vr's menu_pointer.rs and menus/pause/pointer.rs, npiriou, ISC): the right
// hand's ray onto the menu's panel, as a point of the game's menu canvas, and the row of a menu list
// under it, laid out as the game draws a list (pause.c draw_menu_text_list). Plain math
// (tests/rig_pad_test.cpp).

#pragma once

#include "rig.h"

namespace quest {

// Where the ray from `hand` (along its -Z) meets the panel at `panel` (`width` by `height`, facing
// its +Z): `u` from its left edge, `v` from its top, each 0..1 across it. False when it points away
// or past an edge.
inline bool panel_point(const Pose& panel, float width, float height, const Pose& hand, float& u, float& v) {
    const Quat back = conjugate(panel.orientation);
    const Vec3 origin = rotate(back, hand.position - panel.position);
    const Vec3 along = rotate(back, rotate(hand.orientation, {0, 0, -1}));
    if (along.z > -1e-4f || origin.z <= 0.0f) {
        return false;
    }
    const float t = -origin.z / along.z;
    const float x = origin.x + along.x * t, y = origin.y + along.y * t;
    u = x / width + 0.5f;
    v = 0.5f - y / height;
    return u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f;
}

// A quad along a beam from `from` to `to`, turned to face `eye`: its X runs along the beam, its Z toward
// the eye, as near as a beam allows. False for a beam too short or seen end on.
inline bool beam_pose(Vec3 from, Vec3 to, Vec3 eye, Pose& out, float& length_out) {
    const Vec3 span = to - from;
    length_out = length(span);
    if (length_out < 1e-3f) {
        return false;
    }
    const Vec3 x = span * (1.0f / length_out);
    const Vec3 middle = (from + to) * 0.5f;
    Vec3 z = eye - middle;
    z = z - x * dot(z, x);
    const float away = length(z);
    if (away < 1e-3f) {
        return false;
    }
    z = z * (1.0f / away);
    const Vec3 y{z.y * x.z - z.z * x.y, z.z * x.x - z.x * x.z, z.x * x.y - z.y * x.x};
    // The rotation with those columns.
    const float trace = x.x + y.y + z.z;
    Quat q;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        q = {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s};
    } else if (x.x > y.y && x.x > z.z) {
        const float s = std::sqrt(1.0f + x.x - y.y - z.z) * 2.0f;
        q = {0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s};
    } else if (y.y > z.z) {
        const float s = std::sqrt(1.0f + y.y - x.x - z.z) * 2.0f;
        q = {(y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s};
    } else {
        const float s = std::sqrt(1.0f + z.z - x.x - y.y) * 2.0f;
        q = {(z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s};
    }
    out = {middle, q};
    return true;
}

// The row of a window of `shown` rows spread down a box of `height`, for a point `y` down it: -1 past
// the first row's reach, `shown` past the last's, so a hand at either edge can scroll the window.
inline int window_row_at(int shown, float height, float y) {
    const float step = height / static_cast<float>(shown + 1);
    const int row = static_cast<int>(std::lround(y / step)) - 1;
    return std::clamp(row, -1, shown);
}

// The cell of a grid of `rows` by `cols` spread evenly over a box, for a point in it; -1 outside.
inline int grid_cell_at(int rows, int cols, float width, float height, float x, float y) {
    if (rows <= 0 || cols <= 0 || width <= 0.0f || height <= 0.0f || x < 0.0f || y < 0.0f || x >= width || y >= height) {
        return -1;
    }
    const int col = std::min(static_cast<int>(x * static_cast<float>(cols) / width), cols - 1);
    const int row = std::min(static_cast<int>(y * static_cast<float>(rows) / height), rows - 1);
    return row * cols + col;
}

// The row of `rows` evenly spread down a box of `height` (the game's option lists), each `span` tall,
// for a point `y` down it; -1 between or past them.
inline int spread_row_at(int rows, float height, float span, float y) {
    if (rows <= 0 || height <= 0.0f) {
        return -1;
    }
    const float step = height / static_cast<float>(rows + 1);
    for (int i = 0; i < rows; ++i) {
        const float centre = step * static_cast<float>(i + 1);
        if (y >= centre - span * 0.5f && y < centre + span * 0.5f) {
            return i;
        }
    }
    return -1;
}

// A menu list as the game keeps it: its flags (4 large rows, 8 small, 0x10 rows as high as the font),
// its size on the canvas, its scroll, and which rows have a second line.
struct MenuList {
    int flags = 0;
    int width = 0, height = 0;
    int scroll = 0;
    int rows = 0;
    bool second_line[40] = {};
};

// The row of `list` under the point (`x`, `y`) of the list's own box, or -1.
inline int list_row_at(const MenuList& list, float x, float y) {
    if (list.rows <= 0 || x < 4.0f || x > static_cast<float>(list.width - 4)) {
        return -1;
    }
    const int size = (list.flags & 4) != 0 ? 14 : (list.flags & 8) != 0 ? 10 : 12;
    const int step = (list.flags & 0x10) != 0 ? size + 3 : list.height / (list.rows + 1);
    int top = step - size / 2 - 1 - list.scroll;
    for (int i = 0; i < list.rows && i < 40; ++i) {
        const int span = list.second_line[i] ? step + size : size;
        if (y >= static_cast<float>(top - 4) && y < static_cast<float>(top + span + 4)) {
            return i;
        }
        top += step * (list.second_line[i] ? 2 : 1);
    }
    return -1;
}

// The ship's planet list, which has its own draw (pause.c draw_menu_text_list at the planet select; Rerac-vr's
// planet_select.rs list_draw): each name a line of the font's size plus 2 (one line: a place's name fits), its
// planet's a step below, then 8 more. The row under (`x`, `y`) of the list's box, or -1.
inline int planet_row_at(const MenuList& list, float x, float y) {
    if (list.rows <= 0 || x < 2.0f || x > static_cast<float>(list.width - 2)) {
        return -1;
    }
    const int size = (list.flags & 4) != 0 ? 14 : (list.flags & 8) != 0 ? 10 : 12;
    const int step = (list.flags & 0x10) != 0 ? size + 3 : list.height / (list.rows + 1);
    int top = step - size / 2 - list.scroll;
    for (int i = 0; i < list.rows && i < 40; ++i) {
        int next = top + size + 2;
        if (list.second_line[i]) {
            next += step;
        }
        const int span = next - top + 8 > size ? next - top + 8 : size;
        if (y >= static_cast<float>(top - 2) && y < static_cast<float>(top - 2 + span)) {
            return i;
        }
        top = next + 8;
    }
    return -1;
}

// The Sound page's bars (pause.c draw_sound_menu; Rerac-vr's pointer.rs Volume): the effects' in the first row,
// the music's in the second, a quarter of the box apart; true over one, with its row and the volume (0..1024) at
// that point of it, as the bar is filled.
inline bool volume_at(int width, int height, float x, float y, int& row, int& value) {
    const float left = static_cast<float>(width / 2 + 9);
    const float span = static_cast<float>(width - (width / 2 + 0x4A) > 1 ? width - (width / 2 + 0x4A) : 1);
    for (int r = 0; r < 2; ++r) {
        const float cy = static_cast<float>(height / 4 * (r + 1));
        if (y >= cy - 10.0f && y < cy + 10.0f && x >= left - 4.0f && x < left + span + 4.0f) {
            float f = (x - left) / span;
            f = f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
            row = r;
            value = static_cast<int>(f * 1024.0f + 0.5f);
            return true;
        }
    }
    return false;
}

// The quit box's two lines at its foot (pause.c draw_quit_game_menu; Rerac-vr's pointer.rs Quit): 1 for the
// upper, which Triangle answers, 2 for the lower, Circle's; 0 for neither.
inline int quit_line_at(int width, int height, float x, float y) {
    if (x < 4.0f || x >= static_cast<float>(width - 4)) {
        return 0;
    }
    for (int line = 1; line <= 2; ++line) {
        const float cy = static_cast<float>(height - (line == 1 ? 0x28 : 0x14) + 8);
        if (y >= cy - 10.0f && y < cy + 10.0f) {
            return line;
        }
    }
    return 0;
}

// The memory card's slots (pause.c draw_save_slot_list): five, 0x54 apart on PAL (0x4B on NTSC-U), each lit
// 0x38 high from its top. The slot under `y` of the box `height` high, or -1.
inline int save_slot_at(bool pal, int height, float y) {
    const int step = pal ? 0x54 : 0x4B;
    for (int i = 0; i < 5; ++i) {
        const float top = static_cast<float>(step * i);
        if (top + 0x38 > static_cast<float>(height) + 4.0f) {
            break;
        }
        if (y >= top && y < top + 0x38) {
            return i;
        }
    }
    return -1;
}

// An icon list (pause.c draw_menu_icon_list): `count` icons of 32 pixels down the box's middle, the first at
// `first_y` sixteenths and 0x260 apart. The icon under (`x`, `y`), or -1.
// The vendor's item strip (Rerac-vr's vendor.rs; the box is 56 a cell, from 12 in): under eight items they
// sit from the left of a box centred on the canvas, from eight a carousel with the selected one in the middle,
// `scroll` the 56 a step it eases back over. The item under canvas `x`, or -1.
inline int vendor_strip_at(int count, int selected, int scroll, float x) {
    constexpr float kMiddle = 256.0f, kCell = 56.0f, kInset = 12.0f;
    constexpr float kCarouselLeft = 46.0f, kCarouselRight = 466.0f;
    if (count <= 0) {
        return -1;
    }
    if (count < 8) {
        const float left = kMiddle - (kCell * static_cast<float>(count) + kInset * 2.0f) * 0.5f;
        if (x < left || x >= kMiddle * 2.0f - left) {
            return -1;
        }
        const int cell = static_cast<int>(std::floor((x - left - kInset) / kCell));
        return cell < 0 ? 0 : cell >= count ? count - 1 : cell;
    }
    if (x < kCarouselLeft || x >= kCarouselRight) {
        return -1;
    }
    const int k = static_cast<int>(std::lround((x - kCarouselLeft - static_cast<float>(scroll) + 76.0f) / kCell)) - 2;
    return (2 * count + k + selected - 3) % count;
}

// Where a ray from `from` along `along` (of length 1) meets the rectangle at `corner` with the edges `across` and
// `down`: how far, and the place on it from 0 to 1 each way. Rerac-vr's vendor.rs ray. False when it misses.
inline bool ray_on_quad(Vec3 from, Vec3 along, Vec3 corner, Vec3 across, Vec3 down, float& distance, float& u, float& v) {
    constexpr float kNearest = 0.02f, kFurthest = 8.0f;
    const auto dotted = [](Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    const Vec3 normal{across.y * down.z - across.z * down.y, across.z * down.x - across.x * down.z,
                      across.x * down.y - across.y * down.x};
    const float facing = dotted(normal, along);
    if (std::abs(facing) < 1e-6f) {
        return false;
    }
    const float t = dotted(normal, corner - from) / facing;
    if (t < kNearest || t > kFurthest) {
        return false;
    }
    const Vec3 at = from + along * t - corner;
    const float aa = dotted(across, across), bb = dotted(down, down), ab = dotted(across, down);
    const float determinant = aa * bb - ab * ab;
    if (std::abs(determinant) < 1e-8f) {
        return false;
    }
    const float x = (dotted(at, across) * bb - dotted(at, down) * ab) / determinant;
    const float y = (dotted(at, down) * aa - dotted(at, across) * ab) / determinant;
    if (x < 0.0f || x > 1.0f || y < 0.0f || y > 1.0f) {
        return false;
    }
    distance = t;
    u = x;
    v = y;
    return true;
}

// Which way the fewest steps of the strip's keys go from `selected` to `wanted`: 1 right, -1 left, 0 there. The
// carousel wraps; a shorter strip stops at its ends.
inline int vendor_strip_step(int count, int selected, int wanted) {
    if (count <= 0 || selected == wanted) {
        return 0;
    }
    if (count < 8) {
        return wanted > selected ? 1 : -1;
    }
    const int right = (wanted - selected + count) % count;
    return right <= count - right ? 1 : -1;
}

inline int icon_at(int width, int count, int first_y, int step, float x, float y) {
    const float left = (static_cast<float>(width) - 32.0f) * 0.5f;
    if (x < left || x >= left + 32.0f) {
        return -1;
    }
    for (int i = 0; i < count && i < 32; ++i) {
        const float top = static_cast<float>(first_y + i * step) / 16.0f;
        if (y >= top && y < top + 32.0f) {
            return i;
        }
    }
    return -1;
}

// ---- The weapon wheel at the hand (0.1.18, the owner's ask: with Akimbo the rings sat at the HUD's edges, which
// could be out of view) ----

// Where `point` is seen from `eye` on the panel at `panel` (`width` by `height`, facing its +Z): `u` from its left
// edge, `v` from its top, 0..1 across it and past its edges as far as it is. False when the point is not ahead of the
// eye, as the panel faces.
inline bool panel_point_seen(const Pose& panel, float width, float height, Vec3 eye, Vec3 point, float& u, float& v) {
    const Quat back = conjugate(panel.orientation);
    const Vec3 origin = rotate(back, eye - panel.position);
    const Vec3 along = rotate(back, point - eye);
    if (along.z > -1e-3f || origin.z <= 0.0f) {
        return false;
    }
    const float t = -origin.z / along.z;
    u = (origin.x + along.x * t) / width + 0.5f;
    v = 0.5f - (origin.y + along.y * t) / height;
    return std::isfinite(u) && std::isfinite(v);
}

// A ring on the canvas: the canvas's size, the ring's box (`ring_w` by `ring_h`) and its icons' middle in it (`mid_x`,
// `mid_y` from its corner), the least gap to the canvas's edges, and the canvas point straight ahead of the eyes
// (`look_x`, `look_y`) with how far from it the ring's middle stays, so that it is in clear view. All canvas pixels.
struct WheelFit {
    float canvas_w = 512.0f, canvas_h = 416.0f;
    float ring_w = 192.0f, ring_h = 192.0f;
    float mid_x = 96.0f, mid_y = 96.0f;
    float edge = 8.0f;
    float look_x = 256.0f, look_y = 208.0f;
    float reach_x = 1e9f, reach_y = 1e9f;
};

// The middle of the ring's icons for a hand seen at (`u`, `v`) of the canvas: there, no further from straight ahead
// than the reach, and the whole box inside the canvas by the gap (in its middle if it cannot be).
inline void wheel_middle_at(const WheelFit& fit, float u, float v, float& x, float& y) {
    x = std::clamp(u * fit.canvas_w, fit.look_x - fit.reach_x, fit.look_x + fit.reach_x);
    y = std::clamp(v * fit.canvas_h, fit.look_y - fit.reach_y, fit.look_y + fit.reach_y);
    const auto inside = [](float at, float canvas, float box, float middle, float edge) {
        const float least = edge + middle, most = canvas - edge - box + middle;
        return least <= most ? std::clamp(at, least, most) : (least + most) * 0.5f;
    };
    x = inside(x, fit.canvas_w, fit.ring_w, fit.mid_x, fit.edge);
    y = inside(y, fit.canvas_h, fit.ring_h, fit.mid_y, fit.edge);
}

// A ring opened while the other hand's is up (its middle at `other_x`, `other_y`): moved off it to its own hand's side
// (`left`), `gap` between them, or above or below it if the canvas has no room that side; as it was if neither fits.
inline void wheel_beside(const WheelFit& fit, bool left, float other_x, float other_y, float gap, float& x, float& y) {
    const auto apart = [&](float ax, float ay) {
        return std::abs(ax - other_x) >= fit.ring_w + gap - 0.5f || std::abs(ay - other_y) >= fit.ring_h + gap - 0.5f;
    };
    if (apart(x, y)) {
        return;
    }
    float sx = 0, sy = 0;
    wheel_middle_at(fit, ((other_x + (left ? -1.0f : 1.0f) * (fit.ring_w + gap)) / fit.canvas_w), y / fit.canvas_h, sx, sy);
    if (apart(sx, sy)) {
        x = sx;
        y = sy;
        return;
    }
    const float down = y >= other_y ? 1.0f : -1.0f;
    for (const float way : {down, -down}) {
        wheel_middle_at(fit, x / fit.canvas_w, (other_y + way * (fit.ring_h + gap)) / fit.canvas_h, sx, sy);
        if (apart(sx, sy)) {
            x = sx;
            y = sy;
            return;
        }
    }
}

}  // namespace quest
