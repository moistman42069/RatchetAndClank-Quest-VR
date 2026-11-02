// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
//
// A line of words the headset can show with nothing of the game's: the port's own notices are drawn
// on its window, which a headset does not see. Capitals and spaces in a 5 by 7 font, on an RGBA8
// picture whose first row is the bottom, as a GL texture's. Plain data (tests/rig_pad_test.cpp).

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace quest {

inline constexpr int kNoticeWidth = 1120;
inline constexpr int kNoticeHeight = 72;

// Each letter's five columns, the low bit at the top.
inline constexpr std::uint8_t kNoticeFont[26][5] = {
    {0x7C, 0x12, 0x11, 0x12, 0x7C}, {0x7F, 0x49, 0x49, 0x49, 0x36}, {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x41, 0x51, 0x73}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01}, {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x1C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F}, {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x26, 0x49, 0x49, 0x49, 0x32}, {0x03, 0x01, 0x7F, 0x01, 0x03}, {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x03, 0x04, 0x78, 0x04, 0x03}, {0x61, 0x59, 0x49, 0x4D, 0x43},
};

// `text` in white on a dark bar, centred; what the font lacks is a space.
inline std::vector<std::uint8_t> render_notice(const char* text) {
    constexpr int kScale = 4;
    constexpr int kAdvance = 6 * kScale;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(kNoticeWidth) * kNoticeHeight * 4);
    for (std::size_t i = 0; i < rgba.size(); i += 4) {
        rgba[i] = 0x70;
        rgba[i + 1] = 0x10;
        rgba[i + 2] = 0x10;
        rgba[i + 3] = 0xE0;
    }
    int count = 0;
    while (text[count] != 0) {
        ++count;
    }
    int x = (kNoticeWidth - count * kAdvance) / 2;
    const int top = (kNoticeHeight - 7 * kScale) / 2;
    for (int i = 0; i < count; ++i, x += kAdvance) {
        const char c = text[i];
        const int letter = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' : -1;
        if (letter < 0) {
            continue;
        }
        for (int column = 0; column < 5; ++column) {
            for (int row = 0; row < 7; ++row) {
                if ((kNoticeFont[letter][column] >> row & 1) == 0) {
                    continue;
                }
                for (int py = 0; py < kScale; ++py) {
                    for (int px = 0; px < kScale; ++px) {
                        const int at_x = x + column * kScale + px;
                        const int at_y = kNoticeHeight - 1 - (top + row * kScale + py);
                        if (at_x < 0 || at_x >= kNoticeWidth || at_y < 0 || at_y >= kNoticeHeight) {
                            continue;
                        }
                        std::uint8_t* pixel = &rgba[(static_cast<std::size_t>(at_y) * kNoticeWidth
                                                     + static_cast<std::size_t>(at_x)) * 4];
                        pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0xFF;
                    }
                }
            }
        }
    }
    return rgba;
}

}  // namespace quest
