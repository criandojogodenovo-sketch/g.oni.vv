#!/bin/sh
# scripts/link_parity.sh — PARIDADE DE LIGAÇÃO no hospedeiro: TODOS os TUs
# da lib goni_vv (lista extraída do CMake da APP, device-only incluídos:
# main.cpp/StorageBridge/EglContext) compilam E LINKAM com os stubs de
# GLES3/EGL/android/jni. O check_main.sh (sintaxe) não apanha símbolos
# ausentes — ex.: FileApi.cpp fora do CMake da app só explodia no job do
# NDK (link). Este script apanha essa classe ANTES do push.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INC="-I $ROOT/tests/stub -I $ROOT/app/src/main/cpp \
 -I $ROOT/app/src/main/cpp/vendor/etcpak -I $ROOT/app/src/main/cpp/vendor/astc-encoder"
BUILD=$(mktemp -d)
trap 'rm -rf "$BUILD"' EXIT
cd "$ROOT/app/src/main/cpp"
# fontes do CMake da app (só .cpp — a MESMA lista que o NDK compila).
# 0.9.6.5 (GRUPO B): o grep antigo exigia a linha a TERMINAR em ".cpp" —
# 10 TUs com comentário à direita (UndoStack.cpp incluído, cujo comentário
# diz "faltou no APK: ld.lld undefined symbol no CI" — a EXATA classe de
# bug que este gate existe para apanhar) estavam FORA da paridade.
# Agora: o comentário à direita é tolerado e DESCARTADO.
grep -E '^[[:space:]]+[A-Za-z0-9_/.-]+\.cpp([[:space:]]+#.*)?$' CMakeLists.txt \
  | sed 's/^[[:space:]]*//;s/[[:space:]]*#.*$//;s/[[:space:]]*$//' | sort -u > "$BUILD/srcs.txt"
N=$(wc -l < "$BUILD/srcs.txt")
while IFS= read -r s; do
  printf "g++ -std=c++17 -fPIC -c %s -w '%s' -o '%s/%s.o'\n" \
    "$INC" "$s" "$BUILD" "$(echo "$s" | tr '/' '_')"
done < "$BUILD/srcs.txt" > "$BUILD/cmds.txt"
xargs -a "$BUILD/cmds.txt" -d '\n' -P 8 -I CMD sh -c 'eval "CMD"'
g++ -std=c++17 -shared -o "$BUILD/libparity.so" "$BUILD"/*.o
echo "LINK PARITY OK ($N TUs da app ligam no hospedeiro)"
