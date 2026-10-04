#!/usr/bin/env python3
# scripts/scope_check.py — GATE scope-check (CLÁUSULA P-01, FASE 0.9.6).
#
# O CONTRATO (P-01, permanente): "Scope: só caminhos em ci/scope.txt; tocar
# fora = PARAR e perguntar; relatório traz tabela ficheiros tocados vs
# scope; gate scope-check no CI."
#
# Este gate compara os ficheiros ALTERADOS no push (diff before..after, com
# fallback para o commit anterior) contra a lista de caminhos permitidos em
# ci/scope.txt. UM ficheiro fora = CI VERMELHO = a campanha tocou fora do
# scope aprovado e tem de PARAR e perguntar ao dono.
#
# Uso (CI e local):
#   python3 scripts/scope_check.py <before_sha> <after_sha>
#   python3 scripts/scope_check.py --diff <file>     # lista de ficheiros
#                                                    # (um por linha)
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SCOPE = REPO / "ci" / "scope.txt"


def load_scope():
    if not SCOPE.is_file():
        print("::error::ci/scope.txt ausente — o scope da campanha tem de "
              "estar declarado (CLÁUSULA P-01)")
        sys.exit(1)
    allowed = []
    for line in SCOPE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        allowed.append(line.rstrip("/"))
    return allowed


def in_scope(path: str, allowed) -> bool:
    p = path.strip()
    if p.startswith('"') and p.endswith('"'):
        p = p[1:-1]
    p = p.rstrip("/")
    for a in allowed:
        if p == a or p.startswith(a + "/"):
            return True
    return False


def main() -> None:
    if len(sys.argv) == 3 and sys.argv[1] != "--diff":
        before, after = sys.argv[1], sys.argv[2]
        try:
            out = subprocess.run(
                ["git", "diff", "--name-only", before, after],
                cwd=REPO, capture_output=True, text=True, check=True)
            changed = [l for l in out.stdout.splitlines() if l.strip()]
        except subprocess.CalledProcessError:
            print(f"::error::git diff {before}..{after} falhou")
            sys.exit(1)
    elif len(sys.argv) == 3 and sys.argv[1] == "--diff":
        changed = [l for l in Path(sys.argv[2]).read_text(
            encoding="utf-8").splitlines() if l.strip()]
    else:
        print("uso: scope_check.py <before> <after> | "
              "scope_check.py --diff <ficheiro>")
        sys.exit(2)

    allowed = load_scope()
    if not changed:
        print("GATE VERDE (scope-check): nenhum ficheiro alterado")
        return
    offenders = [p for p in changed if not in_scope(p, allowed)]
    print(f"scope-check: {len(changed)} ficheiro(s) alterado(s) no push")
    for p in changed:
        mark = "ok " if in_scope(p, allowed) else "FORA"
        print(f"  [{mark}] {p}")
    if offenders:
        print("::error::GATE VERMELHO (scope-check): ficheiro(s) FORA do "
              "scope da campanha (ci/scope.txt) — CLÁUSULA P-01: PARAR e "
              "perguntar ao dono antes de continuar")
        for p in offenders:
            print(f"::error::  fora do scope: {p}")
        sys.exit(1)
    print("GATE VERDE (scope-check): todo o diff está dentro de ci/scope.txt")


if __name__ == "__main__":
    main()
