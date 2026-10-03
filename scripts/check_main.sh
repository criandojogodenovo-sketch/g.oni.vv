#!/bin/sh
# scripts/check_main.sh — check de PARIDADE DE COMPILAÇÃO no hospedeiro
# (stubs de GLES3/EGL/android/jni) dos TUs que só a build Android compila:
# platform/main.cpp e StorageBridge.cpp. Erros de sintaxe/símbolo nesses TUs
# só apareciam no job NDK do CI; isto apanha-os antes do push.
# 0.9.3 (hotfix REG-002): OboeBackend.cpp entra no check — o <oboe/Oboe.h>
# resolve para tests/stub/oboe/Oboe.h (o MESMO TU que a suíte compila e
# testa). O AudioOutDevice.cpp NÃO entra: o jni.h fake é deliberadamente
# mínimo e não tipa NewObject/etc (documentado no 0.8.11-b) — esse TU
# continua compilado-verificado pelo job NDK do build-release, como sempre.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INC="-I $ROOT/tests/stub -I $ROOT/app/src/main/cpp \
 -I $ROOT/app/src/main/cpp/vendor/etcpak -I $ROOT/app/src/main/cpp/vendor/astc-encoder"
g++ -std=c++17 -fsyntax-only $INC "$ROOT/app/src/main/cpp/platform/main.cpp"
g++ -std=c++17 -fsyntax-only $INC "$ROOT/app/src/main/cpp/platform/StorageBridge.cpp"
g++ -std=c++17 -fsyntax-only $INC "$ROOT/app/src/main/cpp/platform/OboeBackend.cpp"
