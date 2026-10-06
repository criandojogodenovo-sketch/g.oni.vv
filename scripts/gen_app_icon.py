#!/usr/bin/env python3
"""gen_app_icon.py — o ícone da app G.One VV (0.9.6.9 · GRUPO F).

«G com 4 SETAS»: o G branco #F5F5F5 (o accent MONO) com as 4 setas do
Move — o verbo primeiro do editor, o MESMO motivo do ícone Move do
conjunto da casa (ui/Icons.h) — em #B5B5B5 sobre o bg mono #141414.

REGRAS DA CASA (o manifesto diz «vetor puro, zero gradientes»):
  • NADA de gradientes/blur/sombras — só preenchimentos sólidos;
  • o gerador VIVE NO REPO (o antigo scripts-local/gen_app_icon.py nunca
    foi commitado — os PNGs eram órfãos; a fonte do ícone é reproduzível
    a partir daqui);
  • geometria DERIVADA (nada de números mágicos espalhados): o G é um
    anel com abertura a direito e barra horizontal; as setas são eixo+
    cabeça em cruz (N/S/E/W), apontando PARA FORA (o «move»).

SAÍDAS (as MESMAS que o antigo gerador):
  • app/src/main/res/drawable-nodpi/gone_logo.png  (512 — header/cards)
  • docs/icon-gone-512.png                          (a cópia de referência)
  • mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/ic_launcher{,_round}.png
    (48/72/96/144/192 — downsample por box a partir do mestre 512)
"""
import os
import struct
import zlib

# ---- A PALETA (o espelho dos tokens ui/Theme.h · spec F) --------------------
BG = (20, 20, 20)        # #141414 — o token bg (o mono; == o clear da app)
G_WHITE = (245, 245, 245)  # #F5F5F5 — o token accent (o G)
ARROWS = (181, 181, 181)   # #B5B5B5 — um cinza entre text1 e text2
TRANSPARENT = (0, 0, 0, 0)

SIZE = 512          # o mestre (as mipmaps saem daqui por downsample)
CORNER = 100        # o raio dos cantos (o mesmo do ícone antigo: 37/192)


# ---- rasterização (puro — sem PIL/numpy no ambiente do CI) ------------------
def new_canvas(size=SIZE):
    return bytearray(size * size * 4)


def fill_rounded_square(px, size, radius, rgba):
    """o fundo: quadrado com cantos arredondados (o launcher recorta)."""
    r = radius
    for y in range(size):
        for x in range(size):
            # dentro do quadrado arredondado?
            cx = min(max(x, r), size - 1 - r)
            cy = min(max(y, r), size - 1 - r)
            dx, dy = x - cx, y - cy
            if dx * dx + dy * dy <= r * r:
                i = (y * size + x) * 4
                px[i:i + 4] = bytes(rgba)


def fill_polygon(px, size, pts, rgba):
    """preenchimento por scanline (even-odd) de um polígono convexo."""
    ys = [p[1] for p in pts]
    y0, y1 = max(0, int(min(ys))), min(size - 1, int(max(ys)))
    n = len(pts)
    for y in range(y0, y1 + 1):
        yc = y + 0.5
        xs = []
        for i in range(n):
            x1, y1v = pts[i]
            x2, y2v = pts[(i + 1) % n]
            if (y1v <= yc < y2v) or (y2v <= yc < y1v):
                t = (yc - y1v) / (y2v - y1v)
                xs.append(x1 + t * (x2 - x1))
        xs.sort()
        for k in range(0, len(xs) - 1, 2):
            a, b = int(xs[k] + 0.5), int(xs[k + 1] - 0.5)
            for x in range(max(0, a), min(size - 1, b) + 1):
                i = (y * size + x) * 4
                px[i:i + 4] = bytes(rgba)


def fill_annulus_sector(px, size, cx, cy, r_out, r_in, a0, a1, rgba,
                        step=0.0025):
    """o traço do G: anel (coroa circular) no setor [a0,a1) radianos
    (0 = direito, sentido anti-horário em coords de ecrã y-para-baixo:
    ângulos NEGATIVOS = acima do eixo x)."""
    import math
    rs = r_in
    re_ = r_out
    # varre em grelha polar fina (a folga de 1px cobre o anti-alias implícito
    # do downsample posterior; o estilo da casa é BORDA DURA — zero blur)
    r = rs
    while r <= re_:
        a = a0
        while a < a1:
            x = cx + r * math.cos(a)
            y = cy + r * math.sin(a)
            xi, yi = int(x), int(y)
            if 0 <= xi < size and 0 <= yi < size:
                i = (yi * size + xi) * 4
                px[i:i + 4] = bytes(rgba)
            a += step / max(r, 1.0)
        r += 0.75


def draw_arrow(px, size, cx, cy, length, shaft_w, head_w, head_l,
               direction, rgba):
    """uma seta apontando `direction` ('N'/'S'/'E'/'W') a partir de
    (cx,cy) — a base fica em (cx,cy), a ponta a `length` na direção.
    Eixo + cabeça triangular (o padrão do ícone Move da casa)."""
    if direction == 'N':
        dx, dy = 0.0, -1.0
    elif direction == 'S':
        dx, dy = 0.0, 1.0
    elif direction == 'W':
        dx, dy = -1.0, 0.0
    else:
        dx, dy = 1.0, 0.0
    # o eixo: um retângulo do comprimento (length - head_l)
    body_l = length - head_l
    bpts = [
        (cx - dx * 0 - dy * shaft_w * 0.5, cy - dy * 0 - dx * shaft_w * 0.5),
        (cx + dx * body_l - dy * shaft_w * 0.5,
         cy + dy * body_l - dx * shaft_w * 0.5),
        (cx + dx * body_l + dy * shaft_w * 0.5,
         cy + dy * body_l + dx * shaft_w * 0.5),
        (cx + dy * shaft_w * 0.5, cy + dx * shaft_w * 0.5),
    ]
    fill_polygon(px, size, bpts, rgba)
    # a cabeça: triângulo na ponta
    tipx, tipy = cx + dx * length, cy + dy * length
    basex, basey = cx + dx * body_l, cy + dy * body_l
    hpts = [
        (tipx, tipy),
        (basex - dy * head_w * 0.5, basey - dx * head_w * 0.5),
        (basex + dy * head_w * 0.5, basey + dx * head_w * 0.5),
    ]
    fill_polygon(px, size, hpts, rgba)


def render_master():
    import math
    px = new_canvas(SIZE)
    fill_rounded_square(px, SIZE, CORNER, BG + (255,))

    # ---- O G: anel com abertura a DIREITO + a barra horizontal -------------
    # proporções do G antigo (o dono aprovou este G): anel exterior ~150,
    # traço ~48, centro exato; a ABERTURA é o quadrante superior-direito
    # (ACIMA da barra — o clássico: o traço sobe pelo lado direito até à
    # barra; a barra entra do centro até à borda)
    cx, cy = SIZE / 2, SIZE / 2
    r_out, r_in = 150.0, 102.0            # traço de 48
    # o anel cobre de +8° (logo ABAIXO da barra, lado direito) dando a volta
    # pelo fundo/esquerda/topo até 290° (= −70°, o fim da abertura)
    a_start = math.radians(8.0)
    a_end = math.radians(290.0)
    fill_annulus_sector(px, SIZE, cx, cy, r_out, r_in,
                        a_start, a_end, G_WHITE + (255,))
    # a barra do G: do centro à borda direita do anel, à altura do eixo
    bar_h = 48.0
    bar = [
        (cx - 8, cy - bar_h * 0.5),
        (cx + r_out, cy - bar_h * 0.5),
        (cx + r_out, cy + bar_h * 0.5),
        (cx - 8, cy + bar_h * 0.5),
    ]
    fill_polygon(px, SIZE, bar, G_WHITE + (255,))

    # ---- AS 4 SETAS (o Move da casa): N/S/E/W apontando PARA FORA ----------
    # cada seta: base a ~r_out+18 do centro, comprimento ~64, eixo 15,
    # cabeça 38×28 — legível no RMX3624 (o ícone corre a 144px: seta ≈18px)
    base_off = r_out + 18.0
    length, shaft_w = 64.0, 15.0
    head_w, head_l = 38.0, 28.0
    for d, ox, oy in (('N', 0.0, -1.0), ('S', 0.0, 1.0),
                      ('W', -1.0, 0.0), ('E', 1.0, 0.0)):
        ax = cx + ox * base_off
        ay = cy + oy * base_off
        draw_arrow(px, SIZE, ax, ay, length, shaft_w, head_w, head_l,
                   d, ARROWS + (255,))
    return px


# ---- PNG (encoder mínimo: RGBA8 + zlib, filtros 0 — o mesmo perfil do
# thumb::encodePngRgb da engine, com canal alpha para os cantos) --------------
def write_png(path, px, size):
    raw = bytearray()
    stride = size * 4
    for y in range(size):
        raw.append(0)   # filtro None
        raw += px[y * stride:(y + 1) * stride]

    def chunk(tag, data):
        c = struct.pack('>I', len(data)) + tag + data
        return c + struct.pack('>I', zlib.crc32(tag + data) & 0xFFFFFFFF)

    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(bytes(raw), 9))
    out += chunk(b'IEND', b'')
    with open(path, 'wb') as f:
        f.write(out)


def downsample(px, size, new_size):
    """box-filter (a média da caixa) — o mesmo espírito do downsample do
    thumb da engine (≤480)."""
    if new_size >= size:
        return px, size
    k = size // new_size
    out = bytearray(new_size * new_size * 4)
    for y in range(new_size):
        for x in range(new_size):
            r = g = b = a = 0
            for dy in range(k):
                for dx in range(k):
                    i = ((y * k + dy) * size + (x * k + dx)) * 4
                    r += px[i]
                    g += px[i + 1]
                    b += px[i + 2]
                    a += px[i + 3]
            n = k * k
            o = (y * new_size + x) * 4
            out[o] = r // n
            out[o + 1] = g // n
            out[o + 2] = b // n
            out[o + 3] = a // n
    return out, new_size


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    res = os.path.join(root, 'app/src/main/res')

    master = render_master()

    # o logo (header da tela de projetos + miniatura default dos cards) e a
    # cópia de referência nos docs — as MESMAS 512 do antigo
    write_png(os.path.join(res, 'drawable-nodpi/gone_logo.png'), master, SIZE)
    write_png(os.path.join(root, 'docs/icon-gone-512.png'), master, SIZE)

    # as mipmaps (ic_launcher == ic_launcher_round, como o antigo: byte a
    # byte iguais — o launcher recorta a forma)
    densities = [('mdpi', 48), ('hdpi', 72), ('xhdpi', 96),
                 ('xxhdpi', 144), ('xxxhdpi', 192)]
    for name, dim in densities:
        small, _ = downsample(master, SIZE, dim)
        d = os.path.join(res, 'mipmap-' + name)
        write_png(os.path.join(d, 'ic_launcher.png'), small, dim)
        write_png(os.path.join(d, 'ic_launcher_round.png'), small, dim)
        print(f'  mipmap-{name}: {dim}x{dim}')
    print('icon: G com 4 setas — gerado (512 mestre + 5 mipmaps ×2)')


if __name__ == '__main__':
    main()
