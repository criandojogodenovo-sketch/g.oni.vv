#!/usr/bin/env python3
"""docs_lint_check.py — gate docs-lint (R-016, docs/REGRESSOES.md).

A documentação vista de fora (docs/ inteiro — RELATÓRIOS incluídos — mais
README.md, ESTUDOS.md, ARCHITECTURE.md, VONI_referencia.md, llms.txt e
llms-full.txt na raiz) não pode conter:

  * placeholders de template não preenchido (<repository-URL>, XXX, TBD)
  * marcadores de trabalho pendente (TODO/FIXME como palavra inteira —
    "TODOS"/"todo" do português NÃO casam)
  * texto de enchimento (lorem/ipsum)
  * linguagem de achismo (acho, talvez, provavelmente, should work,
    I think, deve funcionar)

A lista vive em ci/forbidden_docs_patterns.txt (uma fonte — este script é
só o motor). O CI corre isto em cada push; vermelho = o commit não entra.

Uso:
  python3 scripts/docs_lint_check.py            # a partir da raiz do repo
  python3 scripts/docs_lint_check.py --root <dir>

Exit 0 = limpo · Exit 1 = padrões encontrados (lista ficheiro:linha:padrão).
"""
import argparse
import pathlib
import re
import sys

PATTERNS_FILE = "ci/forbidden_docs_patterns.txt"

# Escape de linha (o '# noqa' da casa): uma linha que TERMINE com este
# marcador é saltada — para o texto que DISCUTE os padrões (a definição da
# R-016 cita as palavras que proíbe; a citação não é hedging). Nunca um
# escape de ficheiro inteiro: linha a linha, sempre visível no diff.
LINE_ALLOW = "<!-- docs-lint:allow -->"

# Os alvos do gate: tudo o que um visitante do repo lê. docs/ é recursivo
# (RELATÓRIOS e auditorias são documentação de projeto, não rascunho).
ROOT_FILES = [
    "README.md",
    "ESTUDOS.md",
    "ARCHITECTURE.md",
    "VONI_referencia.md",
    "llms.txt",
    "llms-full.txt",
]
SCAN_DIRS = ["docs"]


def load_patterns(path: pathlib.Path):
    """Lê o ficheiro de padrões → [(fonte, regex_compilada)]."""
    pats = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("re:"):
            pats.append((line[3:], re.compile(line[3:])))
        else:
            pats.append((line, re.compile(re.escape(line))))
    if not pats:
        raise SystemExit(f"docs-lint: {path} não tem padrões — ficheiro corrompido?")
    return pats


def collect_files(root: pathlib.Path):
    files = []
    for name in ROOT_FILES:
        f = root / name
        if f.is_file():
            files.append(f)
    for d in SCAN_DIRS:
        dd = root / d
        if dd.is_dir():
            files.extend(sorted(dd.rglob("*.md")))
    return sorted(set(files))


def main():
    ap = argparse.ArgumentParser(description="Gate docs-lint (R-016)")
    ap.add_argument("--root", default=".", help="raiz do repo (default: cwd)")
    args = ap.parse_args()

    root = pathlib.Path(args.root).resolve()
    pfile = root / PATTERNS_FILE
    if not pfile.is_file():
        print(f"docs-lint: FALTA {PATTERNS_FILE}")
        return 1
    pats = load_patterns(pfile)

    hits = 0
    for f in collect_files(root):
        try:
            text = f.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError) as e:
            print(f"docs-lint: {f}: ilegível ({e})")
            hits += 1
            continue
        for lineno, line in enumerate(text.splitlines(), 1):
            if line.rstrip().endswith(LINE_ALLOW):
                continue
            for src, rx in pats:
                if rx.search(line):
                    rel = f.relative_to(root)
                    print(f"docs-lint: {rel}:{lineno}: padrão [{src}] em: {line.strip()[:120]}")
                    hits += 1

    if hits:
        print(f"docs-lint: VERMELHO — {hits} ocorrência(s) de padrões proibidos "
              f"(lista: {PATTERNS_FILE}; regra: R-016 em docs/REGRESSOES.md; "
              f"escape de linha: '{LINE_ALLOW}' no fim da linha)")
        return 1
    n = len(collect_files(root))
    print(f"docs-lint: VERDE — {n} ficheiros de documentação limpos "
          f"({len(pats)} padrões, {PATTERNS_FILE})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
