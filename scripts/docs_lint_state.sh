#!/usr/bin/env bash
# scripts/docs_lint_state.sh — GATE STATE.md (0.11-CTX §1.2, job core-tests).
#
# A REGRA (§1.2 da 0.11-CTX): se um commit modifica ficheiros de
# código-fonte (app/, cpp/, tests/, tests-java/) E a mensagem declara
# fecho de sub-fase (prefixo <sub-fase>:, feat: ou fix:), então o diff
# do MESMO commit TEM de incluir STATE.md. Sem ele: VERMELHO com
# "sub-fase declarada sem atualização do STATE.md".
#
# Commits puramente documentais (só docs/, *.md, ci/, etc.) NÃO são
# afetados — o code-path é condição NECESSÁRIA (o STATE.md viaja com o
# código da sub-fase, nunca com docs).
#
# Uso:
#   scripts/docs_lint_state.sh <before> <after>   # o modo do CI e dos pins
#   scripts/docs_lint_state.sh                    # fallback: HEAD^..HEAD
# (o fallback de before inexistente/zero segue o padrão do scope-check)
#
# Exit 0 = VERDE · Exit 1 = VERMELHO · Exit 2 = uso incorreto.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
GIT="git -C $REPO"

BEFORE="${1:-${GITHUB_EVENT_BEFORE:-}}"
AFTER="${2:-${GITHUB_SHA:-HEAD}}"
if [ -z "$BEFORE" ]; then BEFORE="HEAD^"; fi
if ! $GIT cat-file -e "$BEFORE" 2>/dev/null; then BEFORE="HEAD^"; fi

# O padrão de mensagem que DECLARA fecho de sub-fase: "3.75b:",
# "0.11-W:", "0.10.7:", "feat:", "fix:". "docs:", "chore:", "ci:" e
# merge commits NÃO declaram (a cláusula calma dos commits de docs).
SUBFASE_RE='^([0-9]+(\.[0-9A-Za-z]+)*[-.0-9A-Za-z]*|feat|fix):'
ISEXEMPT_RE='^(docs|chore|ci|merge):'
CODE_RE='^(app|cpp|tests|tests-java)/'

range="$BEFORE..$AFTER"
commits=$($GIT rev-list --reverse "$range" 2>/dev/null || true)
if [ -z "$commits" ]; then
  echo "docs-lint(state): VERDE — gama vazia ($range), nada a verificar"
  exit 0
fi

red=0
n=0
for c in $commits; do
  n=$((n + 1))
  subject=$($GIT log -1 --format=%s "$c")
  if printf '%s' "$subject" | grep -Eq "$ISEXEMPT_RE"; then
    continue
  fi
  if ! printf '%s' "$subject" | grep -Eq "$SUBFASE_RE"; then
    continue
  fi
  files=$($GIT show --name-only --format= "$c")
  if ! printf '%s\n' "$files" | grep -Eq "$CODE_RE"; then
    continue
  fi
  if printf '%s\n' "$files" | grep -Fxq "STATE.md"; then
    continue
  fi
  echo "docs-lint(state): VERMELHO — sub-fase declarada sem atualização do STATE.md"
  echo "  commit: $c"
  echo "  assunto: $subject"
  echo "  ficheiros de código tocados sem o STATE.md no diff (regra §1.2 da 0.11-CTX)."
  red=1
done

if [ "$red" -ne 0 ]; then
  echo "docs-lint(state): o commit de sub-fase tem de viajar COM o STATE.md"
  exit 1
fi
echo "docs-lint(state): VERDE — $n commit(s) verificado(s), a regra §1.2 cumpre-se"
exit 0
