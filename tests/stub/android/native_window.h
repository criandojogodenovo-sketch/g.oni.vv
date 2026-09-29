// tests/stub/android/native_window.h — stub de hospedeiro (compilação do
// main.cpp no CI; nenhuma função real).
#pragma once
#include <stdint.h>

typedef struct ANativeWindow ANativeWindow;
typedef struct ARect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} ARect;
