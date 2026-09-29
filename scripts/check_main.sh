#!/bin/sh
# scripts/check_main.sh — check de PARIDADE DE COMPILAÇÃO de
# platform/main.cpp no hospedeiro (stubs de GLES3/EGL/android). O main.cpp
# não é compilado pela suíte de testes; erros de sintaxe/símbolo só
# apareciam no CI do NDK. Isto apanha-os localmente antes do push.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
exec g++ -std=c++17 -fsyntax-only \
    -I "$ROOT/tests/stub" \
    -I "$ROOT/app/src/main/cpp" \
    -I "$ROOT/app/src/main/cpp/vendor/etcpak" \
    -I "$ROOT/app/src/main/cpp/vendor/astc-encoder" \
    "$ROOT/app/src/main/cpp/platform/main.cpp"
