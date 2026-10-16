#!/usr/bin/env python3
"""y0_gen_icons.py — o ícone do launcher da Y-0 (0.11-Y).

A FONTE: docs/iconegonevv.png (o «G em chama âmbar» do dono, 1254×1254,
RGB SEM alpha — fundo preto sólido). A extração do glifo é o threshold de
luminância previsto no prompt da Y-0:

  • alpha = clamp((lum - T0) / (T1 - T0))  ·  T0=28 · T1=84
    (o fundo mede 0–10; o corpo do G mede ≥84; a banda 28–84 é a borda
    suave do brilho — o ramp preserva-a como antialiasing);
  • UN-PREMULTIPLY: o fundo é preto puro, pelo que o pixel observado é
    c = a·cor — dividir por max(alpha, 0.35) devolve a cor âmbar (o piso
    limita o ganho de ruído nos pixels mais escuros).

SAÍDAS (a regra do prompt):
  • mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher_foreground.png
    (108/162/216/324/432 — o canvas adaptativo 108dp × densidade; o glifo
    cabe na ZONA SEGURA: bbox máx = 43 % do canvas — um quadrado de 43 %
    tem diagonal 60,8 % < 66 dp do círculo seguro, nada clipa em máscara
    circular);
  • mipmap-*/ic_launcher.png  (legacy: bg #08080A + glifo a 68 % do lado);
  • mipmap-*/ic_launcher_round.png (o mesmo + máscara circular antialiased).
O gone_logo.png (drawable-nodpi) e o docs/icon-gone-512.png NÃO são
tocados (o histórico e o logo in-app ficam como estão — o prompt manda).
"""
import os
import struct
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "iconegonevv.png")
RES = os.path.join(ROOT, "app", "src", "main", "res")

T0, T1 = 40.0, 90.0          # o threshold de luminância (o ramp da borda)
FLOOR = 0.35                 # o piso do un-premultiply (anti-ruído)
FG_FIT = 0.43                # o glifo/canvas na zona segura (66 dp)
LEGACY_FIT = 0.68            # o glifo/lado no legacy (diagonal 0.96 < 1)
BG = (8, 8, 10)              # #08080A — o fundo sólido (a ordem do prompt)

DENS = [("mdpi", 108, 48), ("hdpi", 162, 72), ("xhdpi", 216, 96),
        ("xxhdpi", 324, 144), ("xxxhdpi", 432, 192)]


def extract_rgba(path):
    """RGB → RGBA com alpha por luminância + un-premultiply."""
    im = Image.open(path).convert("RGB")
    w, h = im.size
    px = im.load()
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    po = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            lum = 0.299 * r + 0.587 * g + 0.114 * b
            if lum <= T0:
                continue
            a = (lum - T0) / (T1 - T0)
            if a > 1.0:
                a = 1.0
            s = 1.0 / (a if a > FLOOR else FLOOR)
            nr = min(255, int(r * s + 0.5))
            ng = min(255, int(g * s + 0.5))
            nb = min(255, int(b * s + 0.5))
            po[x, y] = (nr, ng, nb, int(a * 255 + 0.5))
    return out


def bbox_of(im, amin=10):
    """a bbox dos pixels com alpha ≥ amin (o glifo visível)."""
    mask = im.split()[3].point(lambda a: 255 if a >= amin else 0)
    return mask.getbbox()


def fit_paste(glyph, canvas_size, fit):
    """recorta o glifo à bbox, escala para fit×canvas e cola ao centro."""
    bb = bbox_of(glyph)
    crop = glyph.crop(bb)
    bw, bh = crop.size
    target = fit * canvas_size
    s = target / float(max(bw, bh))
    nw, nh = max(1, int(round(bw * s))), max(1, int(round(bh * s)))
    scaled = crop.resize((nw, nh), Image.LANCZOS)
    off = ((canvas_size - nw) // 2, (canvas_size - nh) // 2)
    return scaled, off


def make_foreground(glyph, n):
    fg = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    scaled, off = fit_paste(glyph, n, FG_FIT)
    fg.paste(scaled, off, scaled)
    return fg


def make_legacy(glyph, side, round_mask):
    canvas = Image.new("RGBA", (side, side), BG + (255,))
    scaled, off = fit_paste(glyph, side, LEGACY_FIT)
    canvas.paste(scaled, off, scaled)
    if not round_mask:
        return canvas
    # a máscara circular antialiased (supersampling 4×)
    ss = 4
    m = Image.new("L", (side * ss, side * ss), 0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(m)
    d.ellipse((0, 0, side * ss - 1, side * ss - 1), fill=255)
    m = m.resize((side, side), Image.LANCZOS)
    r, g, b, a = canvas.split()
    a = Image.composite(a, Image.new("L", (side, side), 0), m)
    return Image.merge("RGBA", (r, g, b, a))


def main():
    glyph = extract_rgba(SRC)
    bb = bbox_of(glyph)
    print(f"fonte: {SRC}")
    print(f"extração: T0={T0} T1={T1} floor={FLOOR} | bbox glifo: {bb}")
    n_old = 0
    for name, n, side in DENS:
        d = os.path.join(RES, f"mipmap-{name}")
        os.makedirs(d, exist_ok=True)
        fg = make_foreground(glyph, n)
        p = os.path.join(d, "ic_launcher_foreground.png")
        fg.save(p, "PNG", optimize=True)
        sq = make_legacy(glyph, side, False)
        p2 = os.path.join(d, "ic_launcher.png")
        sq.save(p2, "PNG", optimize=True)
        rd = make_legacy(glyph, side, True)
        p3 = os.path.join(d, "ic_launcher_round.png")
        rd.save(p3, "PNG", optimize=True)
        for f in (p, p2, p3):
            print(f"  {os.path.relpath(f, ROOT)}")
        n_old += 3
    print(f"{n_old} PNGs escritos (5 densidades × 3)")


if __name__ == "__main__":
    sys.exit(main())
