#!/usr/bin/env python3
"""y0_gen_icons.py — o ícone do launcher da Y-0 (0.11-Y) + o FIX da escala
(Y-0-FIX: o dono mediu no C33 o G a ~25 % do ícone; o contrato é ~65 %).

A FONTE: docs/iconegonevv.png (o «G em chama âmbar» do dono, 1254×1254,
RGB SEM alpha — fundo preto sólido). A extração do glifo é o threshold de
luminância previsto no prompt da Y-0:

  • alpha = clamp((lum - T0) / (T1 - T0))  ·  T0=40 · T1=90
    (o fundo/tile mede 0–28 e a moldura do tile morre inteira no T0 —
    sem rim light acima de 40; a banda 40–90 é a borda suave do fogo —
    o ramp preserva-a como antialiasing);
  • UN-PREMULTIPLY: o fundo é preto puro, pelo que o pixel observado é
    c = a·cor — dividir por max(alpha, 0.35) devolve a cor âmbar (o piso
    limita o ganho de ruído nos pixels mais escuros).

A ESCALA (o fix Y-0-FIX) — o bug da Y-0: a bbox usada para enquadrar era
a do HALO (alpha ≥ 10 — o brilho suave do fogo), e o G sólido vivia lá
dentro a ~69 % da caixa: no launcher o glifo media ~25 %. A regra agora:

  • a bbox de ENQUADRAMENTO é a do CONTEÚDO SÓLIDO — alpha == 255
    (⇔ lum ≥ T1: o núcleo opaco do fogo, sem halo nem transição);
  • o G sólido passa a ocupar 65 % da ZONA SEGURA do adaptativo
    (GLYPH_FILL × 66 dp de 108 dp ⇒ 39,7 % do canvas) — o halo acompanha
    à MESMA escala (o brilho não é espremido) e o CENTRO é o do sólido;
    a diagonal do sólido (56,1 dp) cabe na zona segura (66 dp) e a do
    halo (71,3 dp) cabe no viewport de 72 dp — nada clipa;
  • no LEGACY o G sólido ocupa 65 % do lado (o ícone inteiro é o lado).

SAÍDAS (a regra do prompt):
  • mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher_foreground.png
    (108/162/216/324/432 — o canvas adaptativo 108 dp × densidade);
  • mipmap-*/ic_launcher.png  (legacy: bg #08080A + glifo a 65 % do lado);
  • mipmap-*/ic_launcher_round.png (o mesmo + máscara circular antialiased).
O gone_logo.png (drawable-nodpi) e o docs/icon-gone-512.png NÃO são
tocados (o histórico e o logo in-app ficam como estão — o prompt manda).
"""
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "iconegonevv.png")
RES = os.path.join(ROOT, "app", "src", "main", "res")

T0, T1 = 40.0, 90.0          # o threshold de luminância (o ramp da borda)
FLOOR = 0.35                 # o piso do un-premultiply (anti-ruído)
SOLID_AMIN = 255             # a bbox do CONTEÚDO SÓLIDO: alpha == 1
                             # (⇔ lum ≥ T1 — o núcleo opaco do fogo, sem
                             # halo nem rampa da transição)
HALO_AMIN = 10               # a extensão do halo (o recorte acompanha)
SAFE_ZONE_DP = 66.0          # o círculo seguro do adaptativo (108 dp)
CANVAS_DP = 108.0
GLYPH_FILL = 0.65            # O G SÓLIDO preenche 65 % da zona segura
                             # (o número do dono no Y-0-fix: ~65 %, não 25 %)
FG_FIT = GLYPH_FILL * SAFE_ZONE_DP / CANVAS_DP   # 0.3972 do canvas
LEGACY_FIT = 0.65            # o G sólido / lado no legacy
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


def bbox_of(im, amin):
    """a bbox dos pixels com alpha ≥ amin (amin=10 halo · 255 sólido)."""
    mask = im.split()[3].point(lambda a: 255 if a >= amin else 0)
    return mask.getbbox()


def fit_paste(glyph, canvas_size, fit):
    """escala para o CONTEÚDO SÓLIDO ocupar fit×canvas; o halo (o recorte
    completo) acompanha à MESMA escala e o centro é o do sólido — o
    enquadramento mede o G, não o brilho à volta (o fix Y-0-FIX)."""
    sol = bbox_of(glyph, SOLID_AMIN)
    halo = bbox_of(glyph, HALO_AMIN)
    sw, sh = sol[2] - sol[0], sol[3] - sol[1]
    s = (fit * canvas_size) / float(max(sw, sh))
    crop = glyph.crop(halo)
    nw, nh = max(1, int(round(crop.size[0] * s))), \
        max(1, int(round(crop.size[1] * s)))
    scaled = crop.resize((nw, nh), Image.LANCZOS)
    # o centro do SÓLIDO (em coords do recorte escalado) vai ao centro
    cx = ((sol[0] + sol[2]) / 2.0 - halo[0]) * s
    cy = ((sol[1] + sol[3]) / 2.0 - halo[1]) * s
    off = (int(round(canvas_size / 2.0 - cx)),
           int(round(canvas_size / 2.0 - cy)))
    return scaled, off, (sw, sh), s


def make_foreground(glyph, n):
    fg = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    scaled, off, sol, s = fit_paste(glyph, n, FG_FIT)
    fg.paste(scaled, off, scaled)
    return fg, sol, s


def make_legacy(glyph, side, round_mask):
    canvas = Image.new("RGBA", (side, side), BG + (255,))
    scaled, off, sol, s = fit_paste(glyph, side, LEGACY_FIT)
    canvas.paste(scaled, off, scaled)
    if not round_mask:
        return canvas, sol, s
    # a máscara circular antialiased (supersampling 4×)
    ss = 4
    m = Image.new("L", (side * ss, side * ss), 0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(m)
    d.ellipse((0, 0, side * ss - 1, side * ss - 1), fill=255)
    m = m.resize((side, side), Image.LANCZOS)
    r, g, b, a = canvas.split()
    a = Image.composite(a, Image.new("L", (side, side), 0), m)
    return Image.merge("RGBA", (r, g, b, a)), sol, s


def main():
    glyph = extract_rgba(SRC)
    sol = bbox_of(glyph, SOLID_AMIN)
    halo = bbox_of(glyph, HALO_AMIN)
    print(f"fonte: {SRC}")
    print(f"extração: T0={T0} T1={T1} floor={FLOOR}")
    print(f"bbox SÓLIDO (alpha≥{SOLID_AMIN}): {sol} "
          f"{sol[2]-sol[0]}x{sol[3]-sol[1]}")
    print(f"bbox halo   (alpha≥{HALO_AMIN}): {halo} "
          f"{halo[2]-halo[0]}x{halo[3]-halo[1]}")
    print(f"escala: G sólido = {GLYPH_FILL:.2f} × zona segura "
          f"({SAFE_ZONE_DP:.0f} dp de {CANVAS_DP:.0f}) = {FG_FIT:.4f} do "
          f"canvas adaptativo · legacy {LEGACY_FIT:.2f} do lado")
    n_out = 0
    for name, n, side in DENS:
        d = os.path.join(RES, f"mipmap-{name}")
        os.makedirs(d, exist_ok=True)
        fg, _, _ = make_foreground(glyph, n)
        p = os.path.join(d, "ic_launcher_foreground.png")
        fg.save(p, "PNG", optimize=True)
        sq, _, _ = make_legacy(glyph, side, False)
        p2 = os.path.join(d, "ic_launcher.png")
        sq.save(p2, "PNG", optimize=True)
        rd, _, _ = make_legacy(glyph, side, True)
        p3 = os.path.join(d, "ic_launcher_round.png")
        rd.save(p3, "PNG", optimize=True)
        for f in (p, p2, p3):
            print(f"  {os.path.relpath(f, ROOT)}")
        n_out += 3
    print(f"{n_out} PNGs escritos (5 densidades × 3)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
