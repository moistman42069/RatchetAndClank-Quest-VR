// SPDX-License-Identifier: GPL-3.0-or-later
package dev.ratchetquest.vr;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.PixelFormat;
import android.graphics.RadialGradient;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.drawable.Drawable;

/**
 * The launcher's look, drawn in code and all original: deep space, a ringed planet, a hex bolt and
 * the orange and steel of a certain lombax's workshop. No art from the game is carried in the APK.
 */
final class Artwork {
    static final int ORANGE = 0xffff9a1f;
    static final int ORANGE_DARK = 0xffc65f00;
    static final int STEEL = 0xffb9c8d6;
    static final int NAVY = 0xff0b1730;
    static final int PANEL = 0xdd101f3d;
    static final int INK = 0xffeef4fb;
    static final int MUTED = 0xff9db0c6;
    static final int BAD = 0xffff6a5a;
    static final int GOOD = 0xff7be08a;

    private Artwork() {}

    /** The page behind everything. */
    static final class Space extends Drawable {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

        @Override
        public void draw(Canvas canvas) {
            Rect b = getBounds();
            float w = b.width(), h = b.height();
            paint.setShader(new LinearGradient(0, 0, w, h, new int[] {0xff050b1c, 0xff122a56, 0xff2b1436},
                null, Shader.TileMode.CLAMP));
            canvas.drawRect(b, paint);
            paint.setShader(null);
            for (int i = 0; i < 140; i++) {
                paint.setColor(i % 11 == 0 ? 0xffffd9a0 : 0xff9fb6d4);
                canvas.drawCircle((i * 137.51f) % w, (i * 97.31f) % h, i % 7 == 0 ? 1.8f : 0.9f, paint);
            }
            float cx = w * 0.84f, cy = h * 0.18f, r = h * 0.30f;
            paint.setShader(new RadialGradient(cx - r * 0.3f, cy - r * 0.3f, r * 1.4f,
                new int[] {0xffffb255, 0xffc2571b, 0xff3a1409}, null, Shader.TileMode.CLAMP));
            canvas.drawCircle(cx, cy, r, paint);
            paint.setShader(null);
            canvas.save();
            canvas.rotate(-18, cx, cy);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(h * 0.012f);
            paint.setColor(0x88ffd9a0);
            canvas.drawOval(new RectF(cx - r * 1.7f, cy - r * 0.42f, cx + r * 1.7f, cy + r * 0.42f), paint);
            paint.setStyle(Paint.Style.FILL);
            canvas.restore();
            paint.setColor(0x66050b1c);
            canvas.drawRect(b, paint);
        }

        @Override public void setAlpha(int alpha) {}
        @Override public void setColorFilter(ColorFilter filter) {}
        @Override public int getOpacity() { return PixelFormat.OPAQUE; }
    }

    /** A hex bolt, the mark beside the title. */
    static final class Bolt extends Drawable {
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);

        @Override
        public void draw(Canvas canvas) {
            Rect b = getBounds();
            float cx = b.exactCenterX(), cy = b.exactCenterY(), r = Math.min(b.width(), b.height()) * 0.48f;
            Path hex = new Path();
            for (int i = 0; i < 6; i++) {
                double a = Math.toRadians(60 * i + 30);
                float x = cx + r * (float) Math.cos(a), y = cy + r * (float) Math.sin(a);
                if (i == 0) {
                    hex.moveTo(x, y);
                } else {
                    hex.lineTo(x, y);
                }
            }
            hex.close();
            paint.setShader(new LinearGradient(cx - r, cy - r, cx + r, cy + r, new int[] {0xffffe1a8, ORANGE, ORANGE_DARK},
                null, Shader.TileMode.CLAMP));
            canvas.drawPath(hex, paint);
            paint.setShader(null);
            paint.setColor(NAVY);
            canvas.drawCircle(cx, cy, r * 0.42f, paint);
            paint.setStyle(Paint.Style.STROKE);
            paint.setStrokeWidth(r * 0.08f);
            paint.setColor(STEEL);
            canvas.drawCircle(cx, cy, r * 0.42f, paint);
            paint.setStyle(Paint.Style.FILL);
        }

        @Override public void setAlpha(int alpha) {}
        @Override public void setColorFilter(ColorFilter filter) {}
        @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    }
}
