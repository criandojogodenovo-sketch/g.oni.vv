#!/usr/bin/env python3
# scripts/gizmo_camera_medidas.py — O GATE DO PIN MEDÍVEL DO D20 (0.9.6.19b).
#
# O dono (D20): «Pin: bounding do gizmo da câmara ≤6% da área do viewport
# com seleção e ≤4% sem, medido pelo script de medidas (método PASSO 0), em
# 2 densidades.»
#
# O MÉTODO PASSO 0: as medidas nascem do device virtual (c33_virtual FASE
# 17.5 — o MESMO código do draw: camgizmo::previewCapWorld + computeFrustum +
# gizmoBoundsPx projetados pelo vp real do editor) e são despejadas em
# docs/hotfix19b-gizmo-medidas.json. Este gate REVALIDA o ficheiro commitado:
# os pins ≤4%/≤6%, a proveniência dos campos e a ordem do preview (48..120dp
# constante em ecrã — o extent real do far plane NÃO volta).
#
# UM pin violado = VERMELHO = release bloqueada.
import json
import sys

TOL = 0.05  # tolerância de formatação (o JSON traz 3-4 casas)


def fail(msg: str) -> None:
    print(f"GATE VERMELHO (D20 medidas): {msg} — release BLOQUEADA")
    sys.exit(1)


def main() -> None:
    try:
        with open("docs/hotfix19b-gizmo-medidas.json", "r") as f:
            d = json.load(f)
    except Exception as e:  # noqa: BLE001 — o gate reporta a causa real
        fail(f"docs/hotfix19b-gizmo-medidas.json ilegível ({e}) — corre o "
             "c33_virtual (FASE 17.5) e copia o dump para docs/")

    # proveniência
    if d.get("release") != "0.9.6.19b" or d.get("item") != "D20":
        fail("o dump não é do item D20 / release 0.9.6.19b "
             f"(release={d.get('release')!r} item={d.get('item')!r})")
    for k in ("viewport_px", "bounding_sem_px", "bounding_com_px",
              "pct_area_sem", "pct_area_com", "preview_cap_mundo",
              "preview_dp_alvo", "densidade"):
        if k not in d:
            fail(f"campo ausente no dump: {k}")
    if float(d["densidade"]) not in (1.0, 2.0):
        fail(f"a densidade do dump ({d['densidade']}) não é uma das 2 "
             "densidades do pin (1.0/2.0)")

    # os pins do dono
    pin_sem, pin_com = float(d["pin_sem_pct"]), float(d["pin_com_pct"])
    if abs(pin_sem - 4.0) > TOL or abs(pin_com - 6.0) > TOL:
        fail(f"os pins no dump ({pin_sem}/{pin_com}) divergem do contrato "
             "(4/6)")
    sem = float(d["pct_area_sem"])
    com = float(d["pct_area_com"])
    if sem > pin_sem + TOL:
        fail(f"o bounding SEM seleção ocupa {sem:.2f}% da viewport (pin ≤4%)")
    if com > pin_com + TOL:
        fail(f"o bounding COM seleção ocupa {com:.2f}% da viewport (pin ≤6%)")

    # a área do viewport confere com os bounding dumps (a conta é a do pin)
    vw, vh = (float(x) for x in d["viewport_px"])
    if vw <= 0 or vh <= 0:
        fail(f"viewport_px inválido: {d['viewport_px']}")
    area = vw * vh
    for nome in ("bounding_sem_px", "bounding_com_px"):
        x, y, w, h = (float(v) for v in d[nome])
        if w <= 0 or h <= 0:
            fail(f"{nome} vazio (o gizmo não projetou?)")
        pct = (w * h) / area * 100.0
        alvo = sem if nome == "bounding_sem_px" else com
        if abs(pct - alvo) > 0.01 + TOL:
            fail(f"{nome}: o pct no dump ({alvo:.2f}%) não confere com o "
                 f"rect ({pct:.2f}%) — o dump está contraditório")

    # a ordem do preview (constante em ecrã; o far real não volta)
    cap = float(d["preview_cap_mundo"])
    if cap <= 0:
        fail(f"preview_cap_mundo degenerado ({cap})")
    lo = float(d["preview_dp_alvo"][0])
    hi = float(d["preview_dp_alvo"][1])
    if abs(lo - 48.0) > TOL or abs(hi - 120.0) > TOL:
        fail(f"os alvos dp do preview ({lo}/{hi}) divergem do contrato "
             "(48..120dp)")

    print(f"GATE VERDE (D20 medidas): bounding {sem:.2f}% sem / {com:.2f}% "
          f"com seleção (pins ≤4%/≤6%) — preview na ordem {lo:.0f}..{hi:.0f}dp "
          f"constante em ecrã @densidade {d['densidade']}")


if __name__ == "__main__":
    main()
