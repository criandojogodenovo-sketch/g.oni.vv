#!/bin/sh
# scripts/check_main.sh — check de PARIDADE DE COMPILAÇÃO no hospedeiro
# (stubs de GLES3/EGL/android/jni) dos TUs que só a build Android compila:
# platform/main.cpp, SafBridge.cpp e SafIoJni.cpp. Erros de sintaxe/símbolo
# nesses TUs só apareciam no job NDK do CI; isto apanha-os antes do push.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
INC="-I $ROOT/tests/stub -I $ROOT/app/src/main/cpp \
 -I $ROOT/app/src/main/cpp/vendor/etcpak -I $ROOT/app/src/main/cpp/vendor/astc-encoder"
g++ -std=c++17 -fsyntax-only $INC "$ROOT/app/src/main/cpp/platform/main.cpp"
g++ -std=c++17 -fsyntax-only $INC "$ROOT/app/src/main/cpp/platform/SafBridge.cpp"
g++ -std=c++17 -fsyntax-only $INC "$ROOT/app/src/main/cpp/platform/SafIoJni.cpp"
