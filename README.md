# G.One VV 0.1.0 — F1

Fundação mobile da VV: NativeActivity + EGL/GLES3 + core C++17, em repo próprio
(a VF segue congelada em `1e8e71c`). Mobile-first: arm64-v8a, minSdk 24,
landscape travado (`sensorLandscape`). Device de teste: Realme C33 (720x1600).

## Escopo F1 (implementado)
- Gradle mínimo + CMake/NDK, `hasCode=false` (zero Kotlin/Java próprio, shim do sistema)
- `platform/`: `android_main` (glue), EGL→GLES3, `AInputQueue`→`InputState` (multi-touch), `internalDataPath`
- `core/`: `Handle` generacional, `Tic`, `Scene` (free-list + bump de generation), `Time` (timestep fixo + guard anti-spiral)
- `render/`: PLACEHOLDER F1 — clear mono + batch de quads + shader UI (F2: pipeline Vertex/Mesh/Material)
- `ui/`: immediate-mode C++ — `FontAtlas` (stb_truetype, fonte do sistema), `UiContext`, tema mono,
  toolbar com 3 botões (Menu/Play/Settings) + viewport + status line (fps + tics)
- Diagnóstico: `LOGI/LOGE` + crash handler (SIGSEGV/SIGABRT) → `goni_crash.log`
- CI: `.github/workflows/release.yml` — APK release assinado via secrets + suíte do core no Linux

Sem componentes, física, luzes, script, assets ou presets nesta fase (CLÁUSULA CALMA).

## CI — fluxo da assinatura (uma vez)
1. Actions → **release** → *Run workflow* → preencha `store_pw` / `key_pw`
   → o job **init-keystore** gera o artifact `vv-release-keystore` (baixe e guarde OFFLINE).
2. `base64 -w0 vv-release.jks` → configure os secrets:
   `VV_KEYSTORE` (base64), `VV_STORE_PW`, `VV_KEY_ALIAS`, `VV_KEY_PW`.
3. Todo push publica o artifact `goni-vv-0.1.0-release-signed` (APK arm64).
   Sem secrets: sai `-UNSIGNED` (nunca debug).

Trocar a keystore muda a assinatura — exige desinstalar/reinstalar no device.

## Build local (opcional)
Android SDK + NDK 26.3 + CMake 3.22.1 + JDK 17 → `./gradlew assembleRelease`.

## Testes do core (Linux)
`cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests`

## Verificação no Realme C33 (dono)
- Instalar o APK release → abre em landscape: toolbar (3 botões), viewport cinza, status line (fps + tics).
- Crash log: provocar um crash nativo e conferir `/data/user/0/vv.goni/files/goni_crash.log`
  (sem root, use um build local debuggable apenas para essa verificação; o `logcat` mostra o tombstone nativo).
