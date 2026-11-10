# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 the Ratchet & Clank Quest VR contributors
#
# The sim's panel shots compared frame to frame (hudflicker.sim): for each <name>-panel.png in a folder, in name order,
# how much of the panel is drawn (alpha over 16) and how much changed from the shot before (alpha or colour by more
# than 48), and where the change is (its box, in pixels). Plain Python: the PNG read here (8-bit RGB or RGBA, no
# interlace) is the sim's own output. Usage: python -I vr/tests/panelcmp.py <folder> [prefix]

import os
import struct
import sys
import zlib


def read_png(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(path + ": not a PNG")
    pos, idat, width, height, kind = 8, [], 0, 0, 0
    while pos < len(data):
        length, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            width, height, depth, kind, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or kind not in (2, 6) or interlace != 0:
                raise ValueError(path + ": only 8-bit RGB or RGBA, not interlaced")
        elif tag == b"IDAT":
            idat.append(body)
        elif tag == b"IEND":
            break
        pos += 12 + length
    channels = 4 if kind == 6 else 3
    raw = zlib.decompress(b"".join(idat))
    stride = width * channels
    out = bytearray(height * stride)
    prev = bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind_row = raw[start]
        row = bytearray(raw[start + 1:start + 1 + stride])
        if kind_row == 1:
            for i in range(channels, stride):
                row[i] = (row[i] + row[i - channels]) & 255
        elif kind_row == 2:
            for i in range(stride):
                row[i] = (row[i] + prev[i]) & 255
        elif kind_row == 3:
            for i in range(stride):
                left = row[i - channels] if i >= channels else 0
                row[i] = (row[i] + ((left + prev[i]) >> 1)) & 255
        elif kind_row == 4:
            for i in range(stride):
                a = row[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                row[i] = (row[i] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        out[y * stride:(y + 1) * stride] = row
        prev = row
    return width, height, channels, out


def main():
    folder = sys.argv[1]
    prefix = sys.argv[2] if len(sys.argv) > 2 else ""
    names = sorted(n for n in os.listdir(folder) if n.endswith("-panel.png") and n.startswith(prefix))
    last = None
    for name in names:
        width, height, channels, pixels = read_png(os.path.join(folder, name))
        drawn = 0
        changed = 0
        box = [width, height, -1, -1]
        for y in range(0, height):
            base = y * width * channels
            for x in range(0, width):
                i = base + x * channels
                alpha = pixels[i + 3] if channels == 4 else 255
                if alpha > 16:
                    drawn += 1
                if last is not None:
                    was = last[i:i + channels]
                    now = pixels[i:i + channels]
                    if max(abs(now[k] - was[k]) for k in range(channels)) > 48:
                        changed += 1
                        box = [min(box[0], x), min(box[1], y), max(box[2], x), max(box[3], y)]
        where = "" if changed == 0 else " at x %d..%d y %d..%d" % (box[0], box[2], box[1], box[3])
        print("%s: %dx%d, drawn %d, changed %d%s" % (name, width, height, drawn, changed, where))
        last = pixels


if __name__ == "__main__":
    main()
