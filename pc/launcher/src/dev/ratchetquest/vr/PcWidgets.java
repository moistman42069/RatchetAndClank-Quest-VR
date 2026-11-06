// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.Insets;
import java.awt.LayoutManager;
import java.awt.Rectangle;
import java.util.function.Consumer;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JPanel;
import javax.swing.JScrollBar;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.JViewport;
import javax.swing.Scrollable;
import javax.swing.SwingConstants;
import javax.swing.border.EmptyBorder;
import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;
import javax.swing.plaf.basic.BasicScrollBarUI;

/**
 * The pieces the PC's launcher is drawn with, after the headset's LinearLayouts, TextViews and Buttons: a column that
 * wraps its text to the width it is given, a row of weighted buttons, the plates they are drawn on.
 */
final class PcWidgets {
    private PcWidgets() {}

    static final String FAMILY = "Segoe UI";

    static Font font(float size, boolean bold) {
        return new Font(FAMILY, bold ? Font.BOLD : Font.PLAIN, 1).deriveFont(size);
    }

    /** Children one under another, each as wide as the column, each as tall as it needs at that width. */
    static final class Column implements LayoutManager {
        private final int gap;

        Column(int gap) {
            this.gap = gap;
        }

        static int heightFor(Component c, int width) {
            if (c instanceof JTextArea) {
                // A wrapping text area answers its height for the width it has: give it the width first.
                c.setSize(width, Short.MAX_VALUE);
                return c.getPreferredSize().height;
            }
            if (c instanceof Container) {
                LayoutManager layout = ((Container) c).getLayout();
                if (layout instanceof Column) {
                    return ((Column) layout).height((Container) c, width);
                }
                if (layout instanceof Row) {
                    return ((Row) layout).height((Container) c, width);
                }
            }
            return c.getPreferredSize().height;
        }

        int height(Container parent, int width) {
            Insets in = parent.getInsets();
            int inner = Math.max(1, width - in.left - in.right);
            int h = in.top + in.bottom;
            boolean first = true;
            for (Component c : parent.getComponents()) {
                if (!c.isVisible()) {
                    continue;
                }
                h += (first ? 0 : gap) + heightFor(c, inner);
                first = false;
            }
            return h;
        }

        @Override
        public Dimension preferredLayoutSize(Container parent) {
            int w = parent.getWidth() > 0 ? parent.getWidth() : 800;
            return new Dimension(w, height(parent, w));
        }

        @Override
        public Dimension minimumLayoutSize(Container parent) {
            return preferredLayoutSize(parent);
        }

        @Override
        public void layoutContainer(Container parent) {
            Insets in = parent.getInsets();
            int inner = Math.max(1, parent.getWidth() - in.left - in.right);
            int y = in.top;
            for (Component c : parent.getComponents()) {
                if (!c.isVisible()) {
                    continue;
                }
                int h = heightFor(c, inner);
                c.setBounds(in.left, y, inner, h);
                y += h + gap;
            }
        }

        @Override public void addLayoutComponent(String name, Component comp) {}
        @Override public void removeLayoutComponent(Component comp) {}
    }

    /**
     * Children side by side, as tall as the tallest: a child's "weight" (a client property) shares out the width left
     * after those of weight 0, which keep their own.
     */
    static final class Row implements LayoutManager {
        private final int gap;

        Row(int gap) {
            this.gap = gap;
        }

        static float weight(Component c) {
            Object weight = ((JComponent) c).getClientProperty("weight");
            return weight instanceof Float ? (Float) weight : 1f;
        }

        int height(Container parent, int width) {
            Insets in = parent.getInsets();
            int h = 0;
            for (Component c : parent.getComponents()) {
                h = Math.max(h, c.getPreferredSize().height);
            }
            return h + in.top + in.bottom;
        }

        @Override
        public Dimension preferredLayoutSize(Container parent) {
            int w = parent.getWidth() > 0 ? parent.getWidth() : 800;
            return new Dimension(w, height(parent, w));
        }

        @Override
        public Dimension minimumLayoutSize(Container parent) {
            return preferredLayoutSize(parent);
        }

        @Override
        public void layoutContainer(Container parent) {
            Insets in = parent.getInsets();
            Component[] children = parent.getComponents();
            int inner = parent.getWidth() - in.left - in.right - gap * Math.max(0, children.length - 1);
            float weights = 0;
            for (Component c : children) {
                float weight = weight(c);
                if (weight > 0) {
                    weights += weight;
                } else {
                    inner -= c.getPreferredSize().width;
                }
            }
            int h = parent.getHeight() - in.top - in.bottom;
            float x = in.left;
            for (Component c : children) {
                float weight = weight(c);
                float w = weight > 0 ? Math.max(0, inner) * weight / Math.max(weights, 0.0001f) : c.getPreferredSize().width;
                c.setBounds(Math.round(x), in.top, Math.round(x + w) - Math.round(x), h);
                x += w + gap;
            }
        }

        @Override public void addLayoutComponent(String name, Component comp) {}
        @Override public void removeLayoutComponent(Component comp) {}
    }

    static JPanel column(int gap) {
        JPanel panel = new JPanel(new Column(gap));
        panel.setOpaque(false);
        return panel;
    }

    static JPanel row(int gap) {
        JPanel panel = new JPanel(new Row(gap));
        panel.setOpaque(false);
        return panel;
    }

    static <T extends JComponent> T weighted(T component, float weight) {
        component.putClientProperty("weight", weight);
        return component;
    }

    /** The page in the scroll pane: as wide as the window, as tall as its cards. */
    static final class Page extends JPanel implements Scrollable {
        Page(int gap) {
            super(new Column(gap));
            setOpaque(false);
        }

        @Override
        public Dimension getPreferredSize() {
            int w = getParent() instanceof JViewport && getParent().getWidth() > 0 ? getParent().getWidth() : getWidth();
            w = w > 0 ? w : 800;
            return new Dimension(w, ((Column) getLayout()).height(this, w));
        }

        @Override public Dimension getPreferredScrollableViewportSize() { return getPreferredSize(); }
        @Override public int getScrollableUnitIncrement(Rectangle visible, int orientation, int direction) { return 28; }
        @Override public int getScrollableBlockIncrement(Rectangle visible, int orientation, int direction) {
            return Math.max(28, visible.height - 40);
        }
        @Override public boolean getScrollableTracksViewportWidth() { return true; }
        @Override public boolean getScrollableTracksViewportHeight() { return false; }
    }

    /** A card: a translucent plate, its contents one under another. */
    static final class Card extends JPanel {
        private final Color fill, edge;
        private final float radius;

        Card(Color fill, Color edge, float radius, int padX, int padY) {
            super(new Column(3));
            this.fill = fill;
            this.edge = edge;
            this.radius = radius;
            setOpaque(false);
            setBorder(new EmptyBorder(padY, padX, padY, padX));
        }

        Card() {
            this(PcArtwork.PANEL, PcArtwork.CARD_EDGE, 12, 20, 16);
        }

        @Override
        protected void paintComponent(Graphics g) {
            Graphics2D g2 = (Graphics2D) g.create();
            PcArtwork.plate(g2, getWidth(), getHeight(), fill, edge, radius);
            g2.dispose();
        }
    }

    /** Text that wraps at word ends, as a TextView does. */
    static JTextArea text(String words, float size, Color colour, boolean bold) {
        JTextArea area = new JTextArea(words);
        area.setEditable(false);
        area.setFocusable(false);
        area.setOpaque(false);
        area.setBackground(new Color(0, 0, 0, 0));
        area.setLineWrap(true);
        area.setWrapStyleWord(true);
        area.setBorder(null);
        area.setFont(font(size, bold));
        area.setForeground(colour);
        area.setHighlighter(null);
        return area;
    }

    /** Text a player can select and copy: a log's lines. */
    static JTextArea selectable(String words, float size, Color colour) {
        JTextArea area = text(words, size, colour, false);
        area.setFocusable(true);
        area.setHighlighter(new javax.swing.text.DefaultHighlighter());
        area.setSelectionColor(PcArtwork.ORANGE_DARK);
        area.setSelectedTextColor(PcArtwork.INK);
        area.setCaretColor(PcArtwork.INK);
        area.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 1).deriveFont(size));
        return area;
    }

    /** A gap of `height`, as the headset's spacer views. */
    static JComponent gap(int height) {
        JPanel gap = new JPanel();
        gap.setOpaque(false);
        gap.setPreferredSize(new Dimension(1, height));
        return gap;
    }

    /** A button on a plate: the primary one orange, the others steel blue, a chosen one dark orange. */
    static final class Btn extends JButton {
        Color fill, edge, ink;
        private final float size;

        Btn(String label, boolean primary, Runnable action) {
            super(label);
            size = primary ? 20 : 16;
            fill = primary ? PcArtwork.ORANGE : PcArtwork.BUTTON;
            edge = primary ? new Color(0xffffd9a0, true) : PcArtwork.BUTTON_EDGE;
            ink = primary ? PcArtwork.NAVY : PcArtwork.INK;
            setFont(font(size, true));
            setContentAreaFilled(false);
            setBorderPainted(false);
            setFocusPainted(false);
            setOpaque(false);
            setRolloverEnabled(true);
            setCursor(java.awt.Cursor.getPredefinedCursor(java.awt.Cursor.HAND_CURSOR));
            addActionListener(event -> action.run());
        }

        Btn chosen() {
            fill = PcArtwork.ORANGE_DARK;
            edge = PcArtwork.ORANGE;
            return this;
        }

        @Override
        public Dimension getPreferredSize() {
            FontMetrics metrics = getFontMetrics(getFont());
            return new Dimension(metrics.stringWidth(getText()) + 36, metrics.getHeight() + 20);
        }

        @Override
        protected void paintComponent(Graphics g) {
            Graphics2D g2 = (Graphics2D) g.create();
            Color plate = fill;
            if (getModel().isPressed()) {
                plate = plate.darker();
            } else if (getModel().isRollover()) {
                plate = new Color(Math.min(255, plate.getRed() + 22), Math.min(255, plate.getGreen() + 22),
                    Math.min(255, plate.getBlue() + 22), plate.getAlpha());
            }
            PcArtwork.plate(g2, getWidth(), getHeight(), plate, isFocusOwner() ? PcArtwork.INK : edge, 10);
            // A label too wide for its share of a row is drawn smaller, never cut.
            Font f = getFont();
            FontMetrics metrics = g2.getFontMetrics(f);
            while (metrics.stringWidth(getText()) > getWidth() - 12 && f.getSize2D() > 10) {
                f = f.deriveFont(f.getSize2D() - 1);
                metrics = g2.getFontMetrics(f);
            }
            g2.setFont(f);
            g2.setColor(ink);
            int x = (getWidth() - metrics.stringWidth(getText())) / 2;
            int y = (getHeight() - metrics.getHeight()) / 2 + metrics.getAscent();
            g2.drawString(getText(), x, y);
            g2.dispose();
        }
    }

    /** The set-up's or a download's bar. */
    static final class Bar extends JComponent {
        private int percent;

        Bar(int percent) {
            this.percent = percent;
            setPreferredSize(new Dimension(100, 14));
        }

        void set(int percent) {
            this.percent = percent;
            repaint();
        }

        @Override
        protected void paintComponent(Graphics g) {
            Graphics2D g2 = (Graphics2D) g.create();
            int h = Math.min(getHeight(), 10), y = (getHeight() - h) / 2;
            g2.translate(0, y);
            PcArtwork.plate(g2, getWidth(), h, PcArtwork.BUTTON, PcArtwork.BUTTON_EDGE, 5);
            int w = Math.max(0, Math.min(100, percent)) * getWidth() / 100;
            if (w > 2) {
                PcArtwork.plate(g2, w, h, PcArtwork.ORANGE, PcArtwork.ORANGE, 5);
            }
            g2.dispose();
        }
    }

    /** A line the player types (a seed), each change passed on. */
    static JTextField field(String current, int maxLength, Consumer<String> typed) {
        JTextField field = new JTextField(current);
        field.setFont(font(16, false));
        field.setForeground(PcArtwork.INK);
        field.setBackground(PcArtwork.NAVY);
        field.setCaretColor(PcArtwork.INK);
        field.setBorder(BorderFactory.createCompoundBorder(BorderFactory.createLineBorder(PcArtwork.BUTTON_EDGE),
            new EmptyBorder(6, 10, 6, 10)));
        field.setToolTipText("blank: a random one");
        ((javax.swing.text.AbstractDocument) field.getDocument()).setDocumentFilter(new javax.swing.text.DocumentFilter() {
            @Override
            public void insertString(FilterBypass fb, int offset, String text, javax.swing.text.AttributeSet attr)
                    throws javax.swing.text.BadLocationException {
                replace(fb, offset, 0, text, attr);
            }

            @Override
            public void replace(FilterBypass fb, int offset, int length, String text, javax.swing.text.AttributeSet attrs)
                    throws javax.swing.text.BadLocationException {
                int room = maxLength - (fb.getDocument().getLength() - length);
                String kept = text == null ? "" : text.length() > room ? text.substring(0, Math.max(0, room)) : text;
                super.replace(fb, offset, length, kept, attrs);
            }
        });
        field.getDocument().addDocumentListener(new DocumentListener() {
            @Override public void insertUpdate(DocumentEvent e) { typed.accept(field.getText()); }
            @Override public void removeUpdate(DocumentEvent e) { typed.accept(field.getText()); }
            @Override public void changedUpdate(DocumentEvent e) { typed.accept(field.getText()); }
        });
        return field;
    }

    /** A value between a step down and a step up. */
    static javax.swing.JLabel centred(String words, float size, Color colour) {
        javax.swing.JLabel label = new javax.swing.JLabel(words, SwingConstants.CENTER);
        label.setFont(font(size, true));
        label.setForeground(colour);
        return label;
    }

    /** A thin scroll bar in the launcher's colours. */
    static void style(JScrollBar bar) {
        bar.setOpaque(false);
        bar.setPreferredSize(new Dimension(12, 12));
        bar.setUI(new BasicScrollBarUI() {
            @Override
            protected void configureScrollBarColors() {
                thumbColor = PcArtwork.BUTTON_EDGE;
                trackColor = new Color(0, 0, 0, 0);
            }

            @Override
            protected JButton createDecreaseButton(int orientation) { return none(); }

            @Override
            protected JButton createIncreaseButton(int orientation) { return none(); }

            private JButton none() {
                JButton button = new JButton();
                button.setPreferredSize(new Dimension(0, 0));
                button.setMinimumSize(new Dimension(0, 0));
                button.setMaximumSize(new Dimension(0, 0));
                return button;
            }

            @Override
            protected void paintTrack(Graphics g, JComponent c, Rectangle bounds) {}

            @Override
            protected void paintThumb(Graphics g, JComponent c, Rectangle bounds) {
                if (bounds.isEmpty()) {
                    return;
                }
                Graphics2D g2 = (Graphics2D) g.create();
                g2.translate(bounds.x + 2, bounds.y + 2);
                PcArtwork.plate(g2, bounds.width - 4, bounds.height - 4, thumbColor, thumbColor, 4);
                g2.dispose();
            }
        });
    }
}
