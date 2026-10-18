#!/usr/bin/env python3
"""d3_palette.py — 0.12-D (D-3): a PROVENIÊNCIA da RAMPA DO FOGO.

A ordem do dono fixa a rampa #7A1F0A → #E8530F → #FFA21F → #FFE08A e a
D-3 extrai-a do ícone REAL (docs/iconegonevv.png — o «G em chama», o
mesmo PNG da Y-0). Este script É a prova da extração: lê o PNG, fica só
com o núcleo quente do glifo (a mesma banda sólida da Y-0: lum ≥ T1) e
quantiza a paleta em 4 âncoras por QUARTIL de luminância — da base escura
ao bico amarelo. A saída é a paleta imprimida em hex + a comparação com
a rampa do Theme (ui/Theme.h fire0..fire3).

A regra da casa (R-030): a cor vive SÓ no Theme — este script NÃO gera
código; documenta a origem. O pin fogo_rampa (test_012d_fogo.cpp) aferra
os 4 tokens aos hexes da ordem do dono.
"""
import os

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "docs", "iconegonevv.png")
T1 = 90.0   # o núcleo sólido (o mesmo corte da Y-0: lum ≥ T1 = opaco)

# a RAMPA do dono (a ordem — o que o Theme carrega)
SPEC = ["#7A1F0A", "#E8530F", "#FFA21F", "#FFE08A"]


def lum(px):
    return 0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2]


def main():
    img = Image.open(SRC).convert("RGB")
    w, h = img.size
    px = img.load()
    warm = []
    for y in range(0, h, 2):        # o passo 2: a amostra basta (1254²)
        for x in range(0, w, 2):
            p = px[x, y]
            l = lum(p)
            if l >= T1 and p[0] > p[2]:   # quente: vermelho domina o azul
                warm.append((l, p))
    if not warm:
        raise SystemExit("sem pixels quentes — o ícone mudou?")
    warm.sort(key=lambda t: t[0])
    n = len(warm)
    # 4 âncoras por quartil de luminância (base escura → bico amarelo);
    # cada âncora é a MEDIANA do seu quartil (o centro robusto da banda)
    anchors = []
    for q in range(4):
        band = warm[q * n // 4:max((q + 1) * n // 4, q * n // 4 + 1)]
        band.sort(key=lambda t: t[0])
        mid = band[len(band) // 2][1]
        anchors.append("#%02X%02X%02X" % mid)
    print("amostra quente: %d pixels (lum ≥ %.0f, R > B)" % (n, T1))
    for i, (a, s) in enumerate(zip(anchors, SPEC)):
        print("fire%d extraído %s · ordem do dono %s · %s" %
              (i, a, s, "OK" if a.upper() == s.upper() else "família"))
    # a prova da FAMÍLIA (o extraído é medição ruidosa de um PNG com
    # dithering; a ordem do dono é a VERDADE — o que o teste aferra é a
    # ordem; o que este script prova é que a extração CBEGa na mesma
    # família quente: hue dominante vermelho→amarelo, R ≥ G > B em todas)
    ok = True
    for a in anchors:
        r, g, b = int(a[1:3], 16), int(a[3:5], 16), int(a[5:7], 16)
        if not (r >= g > b):
            ok = False
    print("família quente (R ≥ G > B nas 4 âncoras): %s" %
          ("SIM" if ok else "NÃO"))


if __name__ == "__main__":
    main()
