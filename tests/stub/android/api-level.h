// tests/stub/android/api-level.h — stub do header do NDK para o HOST.
// O TU device-only platform/AudioOutDevice.cpp chama
// android_get_device_api_level() para decidir se o AAudio existe (API 26).
// No host devolvemos 0 (< 26): o aaudioLoader() loga "AAudio
// indisponivel" e devolve null — o caminho de fallback, exatamente como
// um device API 24/25. Isto permite ao check_main.sh verificar a SINTAXE
// do TU do AAudio no hospedeiro (antes só o job do NDK o compilava —
// erros de dedo no AudioOutDevice.cpp só apareciam no build-release).
#pragma once

static inline int android_get_device_api_level() {
    return 0;   // host: força o caminho "API < 26 → sem AAudio"
}
