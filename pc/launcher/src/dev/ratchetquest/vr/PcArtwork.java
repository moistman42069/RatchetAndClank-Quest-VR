// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Graphics2D;
import java.awt.LinearGradientPaint;
import java.awt.RadialGradientPaint;
import java.awt.RenderingHints;
import java.awt.geom.AffineTransform;
import java.awt.geom.Ellipse2D;
import java.awt.geom.Path2D;
import java.awt.image.BufferedImage;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;

import javax.imageio.ImageIO;

/**
 * The launcher's look, as Artwork draws it on the headset, in code and all original: deep space, a ringed planet, a
 * hex bolt and the orange and steel of a certain lombax's workshop. No art from the game is carried.
 */
final class PcArtwork {
    static final Color ORANGE = new Color(0xffff9a1f, true);
    static final Color ORANGE_DARK = new Color(0xffc65f00, true);
    static final Color STEEL = new Color(0xffb9c8d6, true);
    static final Color NAVY = new Color(0xff0b1730, true);
    static final Color PANEL = new Color(0xdd101f3d, true);
    static final Color INK = new Color(0xffeef4fb, true);
    static final Color MUTED = new Color(0xff9db0c6, true);
    static final Color BAD = new Color(0xffff6a5a, true);
    static final Color GOOD = new Color(0xff7be08a, true);
    static final Color BUTTON = new Color(0xff1b3157, true);
    static final Color BUTTON_EDGE = new Color(0xff46638c, true);
    static final Color CARD_EDGE = new Color(0x5546638c, true);

    private PcArtwork() {}

    static void smooth(Graphics2D g) {
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_RENDERING, RenderingHints.VALUE_RENDER_QUALITY);
        g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL, RenderingHints.VALUE_STROKE_PURE);
    }

    /** The page behind everything. */
    static void space(Graphics2D g, int w, int h) {
        smooth(g);
        g.setPaint(new LinearGradientPaint(0, 0, Math.max(1, w), Math.max(1, h), new float[] {0f, 0.5f, 1f},
            new Color[] {new Color(0xff050b1c, true), new Color(0xff122a56, true), new Color(0xff2b1436, true)}));
        g.fillRect(0, 0, w, h);
        for (int i = 0; i < 140; i++) {
            g.setColor(new Color(i % 11 == 0 ? 0xffffd9a0 : 0xff9fb6d4, true));
            float r = i % 7 == 0 ? 1.8f : 0.9f;
            float x = (i * 137.51f) % Math.max(1, w), y = (i * 97.31f) % Math.max(1, h);
            g.fill(new Ellipse2D.Float(x - r, y - r, 2 * r, 2 * r));
        }
        float cx = w * 0.84f, cy = h * 0.18f, r = h * 0.30f;
        if (r > 1) {
            g.setPaint(new RadialGradientPaint(cx - r * 0.3f, cy - r * 0.3f, r * 1.4f, new float[] {0f, 0.5f, 1f},
                new Color[] {new Color(0xffffb255, true), new Color(0xffc2571b, true), new Color(0xff3a1409, true)}));
            g.fill(new Ellipse2D.Float(cx - r, cy - r, 2 * r, 2 * r));
            AffineTransform before = g.getTransform();
            g.rotate(Math.toRadians(-18), cx, cy);
            g.setStroke(new BasicStroke(h * 0.012f));
            g.setColor(new Color(0x88ffd9a0, true));
            g.draw(new Ellipse2D.Float(cx - r * 1.7f, cy - r * 0.42f, r * 3.4f, r * 0.84f));
            g.setTransform(before);
        }
        g.setColor(new Color(0x66050b1c, true));
        g.fillRect(0, 0, w, h);
    }

    /** A hex bolt, the mark beside the title, centred in a square of `size`. */
    static void bolt(Graphics2D g, float x, float y, float size) {
        smooth(g);
        float cx = x + size / 2, cy = y + size / 2, r = size * 0.48f;
        Path2D.Float hex = new Path2D.Float();
        for (int i = 0; i < 6; i++) {
            double a = Math.toRadians(60 * i + 30);
            float px = cx + r * (float) Math.cos(a), py = cy + r * (float) Math.sin(a);
            if (i == 0) {
                hex.moveTo(px, py);
            } else {
                hex.lineTo(px, py);
            }
        }
        hex.closePath();
        g.setPaint(new LinearGradientPaint(cx - r, cy - r, cx + r, cy + r, new float[] {0f, 0.5f, 1f},
            new Color[] {new Color(0xffffe1a8, true), ORANGE, ORANGE_DARK}));
        g.fill(hex);
        g.setColor(NAVY);
        g.fill(new Ellipse2D.Float(cx - r * 0.42f, cy - r * 0.42f, r * 0.84f, r * 0.84f));
        g.setStroke(new BasicStroke(r * 0.08f));
        g.setColor(STEEL);
        g.draw(new Ellipse2D.Float(cx - r * 0.42f, cy - r * 0.42f, r * 0.84f, r * 0.84f));
    }

    static BufferedImage boltImage(int size) {
        BufferedImage image = new BufferedImage(size, size, BufferedImage.TYPE_INT_ARGB);
        Graphics2D g = image.createGraphics();
        bolt(g, 0, 0, size);
        g.dispose();
        return image;
    }

    /** The window's icons at the sizes Windows asks for. */
    static List<BufferedImage> icons() {
        List<BufferedImage> icons = new ArrayList<>();
        for (int size : new int[] {16, 24, 32, 48, 64, 128, 256}) {
            icons.add(boltImage(size));
        }
        return icons;
    }

    /** The .exe's icon (tools/build_pc.py --package): an .ico of PNG images, one per size. */
    static void writeIcon(File file) throws IOException {
        List<byte[]> images = new ArrayList<>();
        int[] sizes = {16, 24, 32, 48, 64, 128, 256};
        for (int size : sizes) {
            ByteArrayOutputStream png = new ByteArrayOutputStream();
            ImageIO.write(boltImage(size), "png", png);
            images.add(png.toByteArray());
        }
        int header = 6 + 16 * sizes.length;
        int total = header;
        for (byte[] image : images) {
            total += image.length;
        }
        ByteBuffer out = ByteBuffer.allocate(total).order(ByteOrder.LITTLE_ENDIAN);
        out.putShort((short) 0).putShort((short) 1).putShort((short) sizes.length);
        int offset = header;
        for (int i = 0; i < sizes.length; i++) {
            out.put((byte) (sizes[i] >= 256 ? 0 : sizes[i])).put((byte) (sizes[i] >= 256 ? 0 : sizes[i]));
            out.put((byte) 0).put((byte) 0).putShort((short) 1).putShort((short) 32);
            out.putInt(images.get(i).length).putInt(offset);
            offset += images.get(i).length;
        }
        for (byte[] image : images) {
            out.put(image);
        }
        Files.write(file.toPath(), out.array());
    }

    /** A plate: a rounded fill with an edge, as the headset's buttons and cards. */
    static void plate(Graphics2D g, int w, int h, Color fill, Color edge, float radius) {
        smooth(g);
        float arc = radius * 2;
        g.setColor(fill);
        g.fill(new java.awt.geom.RoundRectangle2D.Float(0.5f, 0.5f, w - 1f, h - 1f, arc, arc));
        g.setColor(edge);
        g.setStroke(new BasicStroke(1f));
        g.draw(new java.awt.geom.RoundRectangle2D.Float(0.5f, 0.5f, w - 1f, h - 1f, arc, arc));
    }
}
