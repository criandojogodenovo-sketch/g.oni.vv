// tests/stub/android/configuration.h — stub de hospedeiro do
// <android/configuration.h> (0.9.6.1 · PASSO 0): o main.cpp lê a densidade
// do device por AConfiguration_getDensity(app->config); no CI/harness o
// stub devolve g_stubDensityDpi (default 160 = densidade 1.0 — o layout de
// sempre nos testes). O c33_virtual (FASE 12.10) INJETA 320 (densidade 2.0
// — o par do C33) para provar o mecanismo dp→px com números reais.
#pragma once
#include <stdint.h>

namespace vvstub {
// a densidade "do device" no hospedeiro (dpi; 160 = densidade 1.0)
inline int g_stubDensityDpi = 160;
} // namespace vvstub

// constantes reais do AConfiguration (o NDK as define em aconfig.h)
enum { ACONFIGURATION_DENSITY_DEFAULT = 0 };

struct AConfiguration;

inline int32_t AConfiguration_getDensity(AConfiguration*) {
    return vvstub::g_stubDensityDpi > 0 ? vvstub::g_stubDensityDpi : 0;
}
