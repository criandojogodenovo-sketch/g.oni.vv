#!/usr/bin/env python3
# scripts/hierarchy_check.py — 0.9.6.12 (P-08 · GRUPO J1 · R-022) — O GATE
# HIERARCHY-CHECK: docs/LAYOUT_HIERARCHY.md é o CONTRATO da hierarquia do
# editor; este gate afere que o contrato e o CÓDIGO não divergem:
#   (1) o ficheiro do contrato existe e descreve a árvore;
#   (2) cada linha da tabela «Nome conceptual → fonte no código» aponta
#       para um símbolo REAL no ficheiro indicado (o contrato não documenta
#       fantasmas);
#   (3) as sentinelas que vigiam o contrato EXISTEM no fonte (remover uma
#       sentinela sem apagar a linha do contrato = vermelho).
# UM desvio = CI VERMELHO = release bloqueada. Acrescentar uma região:
# primeiro o contrato (este ficheiro afere), depois o código.
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CONTRACT = REPO / "docs" / "LAYOUT_HIERARCHY.md"

# (id, ficheiro do teste) — as sentinelas do contrato (§4 do documento).
# A lista cresce COM os grupos (J2 → R-023, J3 → R-024, J4 → R-025).
SENTINELS = [
    ("regress_hierarquia_contrato", "tests/test_sentinels.cpp"),
    ("regress_texto_strip_campo", "tests/test_sentinels.cpp"),   # J2/R-023
    ("regress_viewport_rect_segue", "tests/test_sentinels.cpp"),  # J3/R-024
    ("regress_rodape_intocavel", "tests/test_sentinels.cpp"),     # J4/R-025
]

# os símbolos que o contrato menciona, por ficheiro — extraídos da tabela
# §0 («Fonte no código»), formato `ficheiro` → `símbolo`
CODE_BASE = REPO / "app" / "src" / "main" / "cpp"


def fail(msg: str) -> None:
    print(f"::error::hierarchy-check: {msg}")
    print(f"GATE VERMELHO: {msg} — release BLOQUEADA")


def main() -> int:
    if not CONTRACT.exists():
        fail("docs/LAYOUT_HIERARCHY.md ausente — o contrato P-08 é "
             "obrigatório antes de qualquer região nova")
        return 1

    text = CONTRACT.read_text(encoding="utf-8")
    if "A ÁRVORE" not in text or "AS REGRAS DO CONTRATO" not in text:
        fail("o contrato não tem a árvore (§1) ou as regras (§2) — "
             "ficheiro truncado ou renomeado")
        return 1

    # (2) a tabela §0: linhas «| nome | região | `ficheiro` `símbolo` |»
    # — cada PAR (ficheiro, símbolo) citado tem de existir no fonte
    row_re = re.compile(r"^\|\s*[^|]+\|\s*[^|]+\|\s*(.+)\|\s*$", re.M)
    tick_re = re.compile(r"`([^`]+)`")
    checked = 0
    for m in row_re.finditer(text):
        cells = m.group(1)
        if "Fonte no código" in cells or "---" in cells:
            continue
        ticks = tick_re.findall(cells)
        if len(ticks) < 2:
            continue  # linha sem referências de código (prosa)
        # a célula é uma lista de pares «`símbolo` em `ficheiro`»
        # separados por «;» — cada segmento: o símbolo é o tick que NÃO é
        # ficheiro, o ficheiro é o tick .h/.cpp/.java do MESMO segmento
        files = [t for t in ticks if t.endswith(".h") or t.endswith(".cpp")
                 or t.endswith(".java")]
        syms = [t for t in ticks if t not in files]
        if not files or not syms:
            continue
        for seg in re.split(r";", cells):
            seg_files = [t for t in tick_re.findall(seg)
                         if t.endswith(".h") or t.endswith(".cpp")
                         or t.endswith(".java")]
            seg_syms = [t for t in tick_re.findall(seg)
                        if t not in seg_files]
            if not seg_files or not seg_syms:
                continue
            f = seg_files[-1]
            path = REPO / "app" / "src" / "main" / "cpp" / f
            if not path.exists():
                path = REPO / f
            if not path.exists():
                path = REPO / "app" / "src" / "main" / "java" / f
            if not path.exists():
                fail(f"o contrato cita o ficheiro '{f}', que não existe")
                return 1
            src = path.read_text(encoding="utf-8", errors="replace")
            for s in seg_syms:
                sym = s.split("::")[-1].split("(")[0].strip()
                if not sym:
                    continue
                checked += 1
                if not re.search(re.escape(sym), src):
                    fail(f"o contrato cita '{s}' em '{f}', mas o símbolo "
                         f"não existe no ficheiro (contrato a MENTIR)")
                    return 1
    if checked == 0:
        fail("a tabela §0 do contrato não cita nenhum símbolo de código — "
             "o contrato perdeu a ligação ao fonte")
        return 1
    print(f"hierarchy-check: {checked} referências contrato→código "
          f"verificadas")

    # (3) as sentinelas do contrato existem
    for name, tfile in SENTINELS:
        path = REPO / tfile
        if not path.exists():
            fail(f"a sentinela {name} devia viver em '{tfile}' — ficheiro "
                 f"ausente")
            return 1
        if name not in path.read_text(encoding="utf-8"):
            fail(f"a sentinela '{name}' (§4 do contrato) NÃO existe em "
                 f"'{tfile}' — o contrato ficou sem guarda")
            return 1
        print(f"hierarchy-check: sentinela {name} presente ({tfile})")

    print("GATE VERDE: o contrato da hierarquia e o código estão em "
          "sincronia")
    return 0


if __name__ == "__main__":
    sys.exit(main())
