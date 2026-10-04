#!/usr/bin/env python3
"""scripts/glyph_source_check.py — GATE DE ACENTOS (FASE 9 / R-008).

O bug (G1-2): o atlas era ASCII — os acentos não renderizavam. O FIX tem
duas metades: (1) o atlas com Latin-1 + Latin Ext-A (afervado pela
sentinela regress_glyph_coverage no C++); (2) AS STRINGS do fonte têm de
usar os acentos CERTOS — este gate apanha a regressão de um literal sem
acento a voltar ao código (copy-paste de versão antiga, teclado EN, etc).

Padrões = literais de UI conhecidos SEM acento (grep -F sobre as strings
do C++ e do Java). UM match = vermelho = release bloqueada.

As MENSAGENS deste check NUNCA citam os padrões no caminho verde (a lição
0.8.12-c — o gate do CI grepa o PRÓPRIO output de verificação).
"""
import sys
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# (ficheiro-glob, [literais proibidos — grep -F em strings "..." do fonte])
# NOTA: "Audio" NÃO está na lista — é também NOME de TIC (identificador
# serializado no .goni; acentuá-lo partiria o round-trip das cenas).
TARGETS = [
    ("app/src/main/cpp/**/*.cpp", [
        '"Fisica"', '"Animacao"', '"Rotacao"', '"projecao:',
        '"Permissoes"', '"Diagnostico"', '"licencas"', '"visivel: ',
        '"chao: ', '"versao /', '"fonte apos',
        '"area de transferencia"', '"nao suportado', '"nao existe"',
        '"clip nao', '"botao nao',
        # 2a leva (varredura completa da FASE 9): valores sim/nao, toasts
        # de assets, erros de storage, labels de animacao, logs da consola
        '"nao"', '"auto: nao"', '"autoplay: nao"', '"loop: nao"',
        '"posicional: nao"', '"nao e ', '"nao achados', '"nao resolvida',
        '"nao encontrada', '"nao encontrado', '"nao aplicado',
        '"nao finito', '"nao batem', '"nao ha ', '"nao ligado',
        '"nao-PCM', '"NAO ', '"acao', '"Botao', '"posicao', '"rotacao',
        '"ja existe', '"cena ja ', '"validacao', '"proxima',
        '"ALVO DA ACAO"', '"sao apagados',
    ]),
    ("app/src/main/cpp/**/*.h", [
        '"Fisica"', '"Animacao"', '"Rotacao"', '"Permissoes"',
        '"Diagnostico"', '"licencas"', '"posicao"', '"rotacao"',
        '"add Animacao"', '"Botao"',
    ]),
    ("app/src/main/java/**/*.java", [
        '"Ultima Edicao"', '"Animacao"', '"Permissoes"', '"Diagnostico"',
        '"nao ',
    ]),
]


def string_literals(text):
    """extrai os literais "..." (escapes \\\" resolvidos de forma simples)"""
    out = []
    for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', text):
        out.append('"' + m.group(1) + '"')
    return out


def main():
    bad = []
    for pattern, needles in TARGETS:
        for f in sorted(ROOT.glob(pattern)):
            if not f.is_file():
                continue
            try:
                text = f.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            lits = string_literals(text)
            for needle in needles:
                for lit in lits:
                    if needle in lit:
                        bad.append((str(f.relative_to(ROOT)), lit))

    if bad:
        print("GLYPH-SOURCE CHECK: FALHOU — literal(is) de UI sem acento "
              f"({len(bad)} ocorrência(s); a versão acentuada existe desde "
              "a FASE 9):")
        seen = set()
        for f, lit in bad:
            key = (f, lit)
            if key in seen:
                continue
            seen.add(key)
            print(f"  {f}: {lit}")
        return 1
    print("GLYPH-SOURCE CHECK: OK — nenhuma literal de UI sem acento "
          "(R-008, gate de fonte)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
