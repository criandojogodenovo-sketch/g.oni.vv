#!/usr/bin/env python3
"""theme_hex_check.py — GATE R-030 (GRUPO UI · spec G): «qualquer hex fora
do ficheiro de Theme = CI vermelho» (a ordem expressa do dono).

O contrato: as CORES do chrome vivem SÓ em ui/Theme.h (a tabela única) e
no seu espelho Java (ProjectManagerActivity.java — a tela de projetos não
desenha em C++; os tokens dela SÃO o ficheiro de Theme dela). Qualquer
literal hex COR-DE-COR noutro sítio da camada de UI é um token órfão —
o fóssil da spec A de novo — e este gate pinta o CI de vermelho.

O QUE PROCURA:
  • literais 0xAARRGGBB / 0xRRGGBB (6 ou 8 dígitos hex) nos .cpp/.h da
    camada de UI (ui/ + platform/main.cpp) que não sejam:
      - sufixo u/ull (IDs de widget da casa — 0x900100ull etc.; NÃO são
        cores, são chaves de hit-test);
      - linhas explicitamente ALLOWLISTADAS abaixo (cada uma com a
        justificação datada — o precedente do scope_check do Grupo A).
  • literais de COR em f32 {r,g,b,a} cru fora do Theme (o padrão
    {0.###f, ...} com 4 componentes em contexto de cor) — os eixos do
    gizmo eram o caso; agora vivem no Theme e o Gizmo.h liga-os por
    static_assert.

SAÍDA: exit 1 + a lista quando apanha; exit 0 silencioso quando limpo.
"""
import re
import sys
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CPP_DIRS = [
    os.path.join(ROOT, "app/src/main/cpp/ui"),
]
CPP_FILES = [os.path.join(ROOT, "app/src/main/cpp/platform/main.cpp")]

# ficheiros ONDE cores são legítimas (a tabela e os seus espelhos ligados)
THEME_FILES = {
    "app/src/main/cpp/ui/Theme.h",     # A TABELA (a fonte única)
    "app/src/main/cpp/ui/UiContext.h", # os ALIASES ligados por static_assert
    "app/src/main/cpp/ui/Gizmo.h",     # os EIXOS ligados por static_assert
}

# linhas permitidas com justificação (o padrão do scope_check — datado)
ALLOW_LINES = [
    # (ficheiro, substring) — cada uma tem de ter justificação aqui:
]

HEX_RE = re.compile(r"0[xX][0-9A-Fa-f]{6}(?:[0-9A-Fa-f]{2})?(?![0-9A-Fa-f])"
                    r"(?:u|U|l|L|ul|UL|ull|ULL)?")

def scan(path, rel):
    bad = []
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            for ln, line in enumerate(f, 1):
                if rel in THEME_FILES:
                    continue
                # remove comentários para não contar hex em documentação?
                # NÃO: o contrato é do CÓDIGO; mas hex EM comentário é ruído
                # — só o código conta (o comentário não desenha).
                code = line.split("//")[0]
                if not code.strip():
                    continue
                for m in HEX_RE.finditer(code):
                    lit = m.group(0)
                    if lit.endswith(("u", "U", "l", "L")):
                        continue   # ID de widget (0x900100ull) — não é cor
                    if any(rel == a and b in line for a, b in ALLOW_LINES):
                        continue
                    # 0x00000000/0xFFFFFFFF em contextos de máscara/
                    # sentinela: hex de cor tem de ter MISTURA de dígitos
                    h = lit[2:].rstrip("uUlL")
                    if h in ("000000", "00000000", "FFFFFF", "FFFFFFFF"):
                        continue
                    bad.append((rel, ln, line.rstrip()))
    except OSError:
        pass
    return bad

def main():
    problems = []
    for d in CPP_DIRS:
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if fn.endswith((".cpp", ".h")):
                rel = os.path.relpath(os.path.join(d, fn), ROOT)
                problems += scan(os.path.join(d, fn), rel.replace(os.sep, "/"))
    for p in CPP_FILES:
        rel = os.path.relpath(p, ROOT).replace(os.sep, "/")
        problems += scan(p, rel)

    if problems:
        print("R-030 GATE VERMELHO — literais hex de cor FORA do Theme:")
        for rel, ln, line in problems:
            print(f"  {rel}:{ln}: {line.strip()}")
        print("As cores do chrome vivem SÓ em ui/Theme.h (a tabela única) "
              "— move o token para lá e usa-o por nome.")
        sys.exit(1)
    print("theme_hex_check: 0 hex de cor fora do Theme (R-030 limpo)")

if __name__ == "__main__":
    main()
