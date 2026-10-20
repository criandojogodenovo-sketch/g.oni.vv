#!/usr/bin/env python3
"""y_gen_ai_icon.py — o ícone do launcher da 0.12-Y (3.Y-B · A ORDEM DO
DONO): «um ícone premium 3D, a letra G em vidro de obsidiana escura com
veias de magma vermelho a brilhar dentro» — GERADO POR IA (o prompt
EXATO do dono) em docs/iconegonevv-ai.png (1024×1024).

A FONTE não é um glifo sobre preto puro (o caso Y-0): é um TILE de
obsidiana (quadrado de cantos arredondados) sobre um fundo gradiente
escuro COM um reflexo vermelho no chão. O threshold de luminância puro
NÃO serve (a face da obsidiana é lum≈9 — comeria o tile; o reflexo é
lum≈52 — passaria como conteúdo). A EXTRAÇÃO é por TEXTURA (high-pass):

  • o fundo e o reflexo são SUAVES (o gradiente + o glow) — o high-pass
    |im − blur(im)| ≥ 6 mata-os;
  • o tile é TEXTURADO (o rebordo, as veias, o grão da obsidiana) — a
    densidade das suas linhas é 0.57–0.69 contra 0.00–0.03 do fundo;
  • a PROJEÇÃO da máscara nos eixos com o piso de densidade 25% dá a
    bbox do tile; encolhe 4 px (o halo da aresta), quadrifica-se pelo
    MENOR lado centrado e recebe uma máscara ROUND-RECT antialiased
    (o raio ~13% do lado) — os pixels do tile ficam INTACTOS (nada de
    un-premultiply: a fonte não é um glifo sobre preto).

A ESCALA (o MESMO contrato Y-0-FIX que o dono validou no device):
  • o tile ocupa 65% da ZONA SEGURA do adaptativo (66 dp de 108 dp ⇒
    39,72% do canvas); a diagonal do tile (43,0 dp) cabe na zona segura
    (66 dp) e no viewport de 72 dp — nada clipa;
  • no LEGACY o tile ocupa 65% do lado.

SAÍDAS (a regra do prompt):
  • mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher_foreground.png
    (108/162/216/324/432 — o canvas adaptativo 108 dp × densidade);
  • mipmap-*/ic_launcher.png  (legacy: bg #0A0A0C + tile a 65% do lado);
  • mipmap-*/ic_launcher_round.png (o mesmo + máscara circular
    antialiased, o círculo preenchido com o bg).
O fundo do adaptativo passa a #0A0A0C (values/ic_launcher_background.xml
— A ORDEM DO DONO na 0.12-Y, supera o #08080A da Y-0).
"""
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "iconegonevv-ai.png")
RES = os.path.join(ROOT, "app", "src", "main", "res")

HP_PX = 6.0            # o piso do high-pass (a textura contra o fundo suave)
HP_BLUR = 10           # o raio do blur do high-pass
DENSE = 0.25           # o piso de densidade da banda do tile (o fundo é
                       # 0.00–0.03, o tile 0.57–0.69 — o abismo decide)
INSET = 4              # o recuo da bbox (o halo do fundo na aresta)
RADIUS = 0.13          # o raio do canto do tile (fração do lado)
# o REALCE mild (o padrão dos pipelines de launcher — a obsidiana é
# escura por natureza e os tamanhos pequenos pedem vibrância; NADA de
# retouche da arte: saturação +18%, contraste +10%, brilho +5%)
SAT_BOOST = 1.18
CONTRAST_BOOST = 1.10
BRIGHT_BOOST = 1.05

SAFE_ZONE_DP = 66.0    # o círculo seguro do adaptativo (108 dp)
CANVAS_DP = 108.0
TILE_FILL = 0.65       # O TILE ocupa 65% da zona segura (o contrato Y-0-FIX)
FG_FIT = TILE_FILL * SAFE_ZONE_DP / CANVAS_DP   # 0.3972 do canvas
LEGACY_FIT = 0.65      # o tile / lado no legacy
BG = (10, 10, 12)      # #0A0A0C — o fundo sólido (A ORDEM DO DONO, 0.12-Y)

DENS = [("mdpi", 108, 48), ("hdpi", 162, 72), ("xhdpi", 216, 96),
        ("xxhdpi", 324, 144), ("xxxhdpi", 432, 192)]


def content_mask(im):
    """a máscara por TEXTURA (numpy): |im − blur| ≥ HP_PX no canal máximo —
    o tile é texturado (rebordo/veias/grão), o fundo e o reflexo são
    suaves (o gradiente e o glow morrem no high-pass)."""
    a = np.asarray(im, dtype=np.float32)
    blur = np.asarray(im.filter(ImageFilter.GaussianBlur(HP_BLUR)),
                      dtype=np.float32)
    hp = np.abs(a - blur).max(axis=2)
    return hp >= HP_PX


def dense_bbox(mask, thresh=DENSE):
    """a bbox da banda DENSE: a projeção da máscara em cada eixo (a
    densidade média por linha/coluna) e o intervalo acima do piso — o
    tile é um bloco contíguo, o fundo/reflexo ficam abaixo do piso."""
    rows = mask.mean(axis=1)
    cols = mask.mean(axis=0)
    ys = np.where(rows >= thresh)[0]
    xs = np.where(cols >= thresh)[0]
    return (int(xs.min()), int(ys.min()), int(xs.max()) + 1,
            int(ys.max()) + 1)


def rounded_mask(side, radius_frac, ss=4):
    """a máscara round-rect antialiased (supersampling 4×)."""
    m = Image.new("L", (side * ss, side * ss), 0)
    d = ImageDraw.Draw(m)
    r = int(side * ss * radius_frac)
    d.rounded_rectangle((0, 0, side * ss - 1, side * ss - 1),
                        radius=r, fill=255)
    return m.resize((side, side), Image.LANCZOS)


def extract_tile(path):
    """o TILE recortado com alpha round-rect (os pixels intactos)."""
    im = Image.open(path).convert("RGB")
    m = content_mask(im)
    bb = dense_bbox(m)
    x0, y0, x1, y1 = bb
    x0 += INSET
    y0 += INSET
    x1 -= INSET
    y1 -= INSET
    side = min(x1 - x0, y1 - y0)          # o tile é quadrado — o MENOR lado
    cx = (x0 + x1) // 2
    cy = (y0 + y1) // 2
    x0 = cx - side // 2
    y0 = cy - side // 2
    tile = im.crop((x0, y0, x0 + side, y0 + side)).convert("RGBA")
    # o realce mild (só os canais de cor — o alpha entra depois)
    r, g, b, _ = tile.split()
    rgb = Image.merge("RGB", (r, g, b))
    rgb = ImageEnhance.Color(rgb).enhance(SAT_BOOST)
    rgb = ImageEnhance.Contrast(rgb).enhance(CONTRAST_BOOST)
    rgb = ImageEnhance.Brightness(rgb).enhance(BRIGHT_BOOST)
    r, g, b = rgb.split()
    a = rounded_mask(side, RADIUS)
    return Image.merge("RGBA", (r, g, b, a)), (x0, y0, x0 + side, y0 + side)


def fit_paste(tile, canvas_size, fit):
    """escala para o TILE ocupar fit×canvas, centrado (o tile é o
    conteúdo sólido inteiro — sem halo para acompanhar)."""
    s = (fit * canvas_size) / float(tile.size[0])
    n = max(1, int(round(tile.size[0] * s)))
    scaled = tile.resize((n, n), Image.LANCZOS)
    off = (int(round(canvas_size / 2.0 - n / 2.0)),) * 2
    return scaled, off


def make_foreground(tile, n):
    fg = Image.new("RGBA", (n, n), (0, 0, 0, 0))
    scaled, off = fit_paste(tile, n, FG_FIT)
    fg.paste(scaled, off, scaled)
    return fg


def make_legacy(tile, side, round_mask):
    canvas = Image.new("RGBA", (side, side), BG + (255,))
    scaled, off = fit_paste(tile, side, LEGACY_FIT)
    canvas.paste(scaled, off, scaled)
    if not round_mask:
        return canvas
    # a máscara circular antialiased (supersampling 4×) — o círculo inteiro
    # fica preenchido com o bg (o tile por cima)
    ss = 4
    m = Image.new("L", (side * ss, side * ss), 0)
    d = ImageDraw.Draw(m)
    d.ellipse((0, 0, side * ss - 1, side * ss - 1), fill=255)
    m = m.resize((side, side), Image.LANCZOS)
    r, g, b, a = canvas.split()
    a = Image.composite(a, Image.new("L", (side, side), 255), m)
    return Image.merge("RGBA", (r, g, b, a))


def main():
    tile, bb = extract_tile(SRC)
    print(f"fonte: {SRC}")
    print(f"tile: bbox geométrica {bb} · lado {tile.size[0]} px")
    print(f"escala: tile = {TILE_FILL:.2f} × zona segura "
          f"({SAFE_ZONE_DP:.0f} dp de {CANVAS_DP:.0f}) = {FG_FIT:.4f} do "
          f"canvas adaptativo · legacy {LEGACY_FIT:.2f} do lado · "
          f"bg #{BG[0]:02X}{BG[1]:02X}{BG[2]:02X}")
    n_out = 0
    for name, n, side in DENS:
        d = os.path.join(RES, f"mipmap-{name}")
        os.makedirs(d, exist_ok=True)
        p = os.path.join(d, "ic_launcher_foreground.png")
        make_foreground(tile, n).save(p, "PNG", optimize=True)
        p2 = os.path.join(d, "ic_launcher.png")
        make_legacy(tile, side, False).save(p2, "PNG", optimize=True)
        p3 = os.path.join(d, "ic_launcher_round.png")
        make_legacy(tile, side, True).save(p3, "PNG", optimize=True)
        for f in (p, p2, p3):
            print(f"  {os.path.relpath(f, ROOT)}")
        n_out += 3
    print(f"{n_out} PNGs escritos (5 densidades × 3)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
