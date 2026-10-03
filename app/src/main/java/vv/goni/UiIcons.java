package vv.goni;

import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.drawable.Drawable;

/**
 * 0.9.0 — ÍCONES OUTLINE da tela de projetos (espelho Java do conjunto
 * único ui/Icons.h da engine — o MESMO vocabulário de traço uniforme:
 * polilinhas num viewBox 24, stroke width 2, zero emoji).
 *
 * Apenas os de que o Java precisa (lupa · ordenar · ⋮ · plus · upload ·
 * interrogação · pasta · check): o resto vive no C++.
 */
public final class UiIcons {

    public static final int LUPA = 0;
    public static final int SORT = 1;
    public static final int DOTS = 2;
    public static final int PLUS = 3;
    public static final int UPLOAD = 4;
    public static final int QUESTION = 5;
    public static final int FOLDER = 6;
    public static final int CHECK = 7;
    public static final int PENCIL = 8;
    public static final int DUPLICATE = 9;
    public static final int TRASH = 10;
    public static final int PLAY = 11;
    public static final int WARN = 12;

    /** desenha o ícone `which` centrado em (cx,cy) com tamanho sizePx na
     *  cor ARGB dada — direto no Canvas (sem Drawable intermédio). */
    public static void draw(Canvas c, int which, float cx, float cy,
                            float sizePx, int color) {
        final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG);
        p.setStyle(Paint.Style.STROKE);
        p.setStrokeWidth(Math.max(1.5f, sizePx / 12f));
        p.setStrokeCap(Paint.Cap.ROUND);
        p.setStrokeJoin(Paint.Join.ROUND);
        p.setColor(color);
        final float k = sizePx / 24f;
        final float x = cx - sizePx / 2f, y = cy - sizePx / 2f;
        // helper: ponto do viewBox 0..24 → px
        // (classe local p/ capturar x/y/k sem arrays)
        class V {
            final float px(float dx) { return x + dx * k; }
            final float py(float dy) { return y + dy * k; }
        }
        final V v = new V();
        switch (which) {
            case LUPA: {   // aro octogonal + cabo
                c.drawCircle(v.px(10.5f), v.py(10.5f), 5.5f * k, p);
                c.drawLine(v.px(14.6f), v.py(14.6f), v.px(19.5f), v.py(19.5f), p);
                break;
            }
            case SORT: {   // 3 linhas decrescentes + seta ↓
                c.drawLine(v.px(4), v.py(6.5f), v.px(13), v.py(6.5f), p);
                c.drawLine(v.px(4), v.py(11.5f), v.px(10.5f), v.py(11.5f), p);
                c.drawLine(v.px(4), v.py(16.5f), v.px(8), v.py(16.5f), p);
                c.drawLine(v.px(18), v.py(5), v.px(18), v.py(19), p);
                c.drawLine(v.px(14.8f), v.py(15.8f), v.px(18), v.py(19), p);
                c.drawLine(v.px(18), v.py(19), v.px(21.2f), v.py(15.8f), p);
                break;
            }
            case DOTS: {   // ⋮ vertical
                float d = 1.6f * k;
                c.drawCircle(v.px(12), v.py(6.4f), d, p);
                c.drawCircle(v.px(12), v.py(12), d, p);
                c.drawCircle(v.px(12), v.py(17.6f), d, p);
                break;
            }
            case PLUS: {
                c.drawLine(v.px(12), v.py(5), v.px(12), v.py(19), p);
                c.drawLine(v.px(5), v.py(12), v.px(19), v.py(12), p);
                break;
            }
            case UPLOAD: {   // bandeja + seta ↑
                c.drawLine(v.px(4), v.py(15), v.px(4), v.py(19.5f), p);
                c.drawLine(v.px(4), v.py(19.5f), v.px(20), v.py(19.5f), p);
                c.drawLine(v.px(20), v.py(19.5f), v.px(20), v.py(15), p);
                c.drawLine(v.px(12), v.py(15.5f), v.px(12), v.py(4.5f), p);
                c.drawLine(v.px(8.8f), v.py(7.5f), v.px(12), v.py(4.5f), p);
                c.drawLine(v.px(12), v.py(4.5f), v.px(15.2f), v.py(7.5f), p);
                break;
            }
            case QUESTION: {   // ? em círculo
                c.drawCircle(v.px(12), v.py(12), 8.2f * k, p);
                android.graphics.Path ph = new android.graphics.Path();
                ph.moveTo(v.px(9.2f), v.py(9.6f));
                ph.lineTo(v.px(9.2f), v.py(8.2f));
                ph.lineTo(v.px(11), v.py(6.6f));
                ph.lineTo(v.px(13.6f), v.py(6.8f));
                ph.lineTo(v.px(15), v.py(8.6f));
                ph.lineTo(v.px(15), v.py(10.2f));
                ph.lineTo(v.px(12.8f), v.py(12.4f));
                ph.lineTo(v.px(12.8f), v.py(14.6f));
                c.drawPath(ph, p);
                c.drawPoint(v.px(12.8f), v.py(17.8f), p);
                break;
            }
            case FOLDER: {
                android.graphics.Path ph = new android.graphics.Path();
                ph.moveTo(v.px(4), v.py(6));
                ph.lineTo(v.px(9), v.py(6));
                ph.lineTo(v.px(11), v.py(8.5f));
                ph.lineTo(v.px(20), v.py(8.5f));
                ph.lineTo(v.px(20), v.py(19));
                ph.lineTo(v.px(4), v.py(19));
                ph.close();
                c.drawPath(ph, p);
                break;
            }
            case CHECK: {
                c.drawLine(v.px(5), v.py(13), v.px(10), v.py(18), p);
                c.drawLine(v.px(10), v.py(18), v.px(19), v.py(6.5f), p);
                break;
            }
            case PENCIL: {
                android.graphics.Path ph = new android.graphics.Path();
                ph.moveTo(v.px(4.5f), v.py(19.5f));
                ph.lineTo(v.px(5.5f), v.py(15.8f));
                ph.lineTo(v.px(16.2f), v.py(5));
                ph.lineTo(v.px(19), v.py(7.8f));
                ph.lineTo(v.px(8.3f), v.py(18.5f));
                ph.close();
                c.drawPath(ph, p);
                c.drawLine(v.px(14.4f), v.py(6.8f), v.px(17.2f), v.py(9.6f), p);
                break;
            }
            case DUPLICATE: {
                c.drawRoundRect(v.px(8.5f), v.py(8.5f), v.px(19.5f), v.py(19.5f),
                        2 * k, 2 * k, p);
                c.drawLine(v.px(14), v.py(11.5f), v.px(14), v.py(16.5f), p);
                c.drawLine(v.px(11.5f), v.py(14), v.px(16.5f), v.py(14), p);
                break;
            }
            case TRASH: {
                c.drawRoundRect(v.px(9.5f), v.py(4.5f), v.px(14.5f), v.py(7),
                        1.5f * k, 1.5f * k, p);
                c.drawLine(v.px(5), v.py(7), v.px(19), v.py(7), p);
                android.graphics.Path ph = new android.graphics.Path();
                ph.moveTo(v.px(6.5f), v.py(7));
                ph.lineTo(v.px(7.4f), v.py(19.5f));
                ph.lineTo(v.px(16.6f), v.py(19.5f));
                ph.lineTo(v.px(17.5f), v.py(7));
                c.drawPath(ph, p);
                c.drawLine(v.px(10), v.py(10.5f), v.px(10), v.py(16), p);
                c.drawLine(v.px(14), v.py(10.5f), v.px(14), v.py(16), p);
                break;
            }
            case PLAY: {
                android.graphics.Path ph = new android.graphics.Path();
                ph.moveTo(v.px(8.4f), v.py(5));
                ph.lineTo(v.px(18.4f), v.py(12));
                ph.lineTo(v.px(8.4f), v.py(19));
                ph.close();
                c.drawPath(ph, p);
                break;
            }
            case WARN: {
                android.graphics.Path ph = new android.graphics.Path();
                ph.moveTo(v.px(12), v.py(4));
                ph.lineTo(v.px(20.5f), v.py(19));
                ph.lineTo(v.px(3.5f), v.py(19));
                ph.close();
                c.drawPath(ph, p);
                c.drawLine(v.px(12), v.py(9), v.px(12), v.py(14), p);
                c.drawPoint(v.px(12), v.py(17), p);
                break;
            }
            default:
                break;
        }
    }

    /** Drawable de um ícone (para setCompoundDrawables/ImageView) */
    public static Drawable drawable(final int which, final int color,
                                    final int sizeDp, final float density) {
        final int px = Math.round(sizeDp * density);
        return new Drawable() {
            @Override
            public void draw(Canvas canvas) {
                UiIcons.draw(canvas, which, px / 2f, px / 2f, px * 0.75f, color);
            }
            @Override
            public void setAlpha(int a) {
            }
            @Override
            public void setColorFilter(ColorFilter f) {
            }
            @Override
            public int getOpacity() {
                return PixelFormat.TRANSLUCENT;
            }
            @Override
            public int getIntrinsicWidth() {
                return px;
            }
            @Override
            public int getIntrinsicHeight() {
                return px;
            }
        };
    }

    private UiIcons() {
    }
}
