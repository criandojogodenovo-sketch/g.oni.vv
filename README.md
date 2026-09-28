# G.One VV 0.3.0 — F3

Engine com editor. A F2 deixou o pipeline 3D mínimo (cubo hardcoded, câmara de
orbit, grid com fade); a F3 transformou os TIC em entidades reais com
componentes, presets de criação, editor (Hierarchy + Inspector) e serialização
de cena `.goni` com migrações. Mobile-first: arm64-v8a, minSdk 24, landscape
travado (`sensorLandscape`). Device de teste: Realme C33 (720x1600).

## Escopo F3 (implementado)
- `components/`: `Transform3D` (pos/Quat/scale + cache world TRS), `MeshRenderer`
  (Mesh* + Material* não-donos), `InputMap` (vazio — F4 preenche)
- `core/`: `Component` (owner + hooks attach/detach), `ComponentStorage<T>` SoA
  (deque — endereços estáveis), `ComponentRegistry` (nome→id→factory),
  `ComponentStore` (bolsa da Scene), `Tic` container
  (`addComponent<T>/getComponent<T>/removeComponent<T>`), `Presets`
  (PlayerBody3D/CharacterBody3D/StaticBody3D, nomes únicos .001),
  `Tick` (PreUpdate→Update→PostUpdate→Render), `TransformSystem`,
  `Json` mínimo + `SceneSerializer` (.goni v1, migração v0→v1, tipos
  desconhecidos ignorados no load)
- `math/`: `Quat` (unitário, euler YXZ, `toMat4` reproduz `rotX/Y/Z` da F2)
- `ui/`: `UiContext.slider` + `EditorUi` — Hierarchy esquerda (seleção + botão
  "+"), Inspector direita (9 sliders pos/rot graus/scale), overlays
  ("+" → 3 presets; Menu → Save/Load), toasts; tema mono; 3 botões F1 mantidos
- `platform/main.cpp`: pass 3D desenha todos os TICs com MeshRenderer
  (fim do cubo hardcoded); TickGroups por passo fixo; gate da câmara
  (gestos só nascem no viewport central)
- CI: `verify-entry-symbols` + suíte do core (68 testes) + APK assinado

Sem física (BodyComp é F4), sem luzes, sem assets, sem animação, sem linguagem
(CLÁUSULA CALMA).

## Histórico
- **F1 (0.1.0)**: fundação — NativeActivity, EGL/GLES3, core (Handle/Tic/Scene/
  Time), UI immediate-mode, crash log, CI assinado.
- **F2 (0.2.0)**: primeiro 3D — Vertex/Mesh/LitMaterial, cubo procedural,
  grid com fade ("espaço infinito"), câmara de orbit (1 dedo + pinch, clamp),
  depth test, status line com vértices/draw calls.

## CI — fluxo da assinatura (uma vez)
1. Actions → **release** → *Run workflow* → preencha `store_pw` / `key_pw`
   → o job **init-keystore** gera o artifact `vv-release-keystore` (baixe e guarde OFFLINE).
2. `base64 -w0 vv-release.jks` → configure os secrets:
   `VV_KEYSTORE` (base64), `VV_STORE_PW`, `VV_KEY_ALIAS`, `VV_KEY_PW`.
3. Todo push publica o artifact `goni-vv-0.3.0-release-signed` (APK arm64).
   Sem secrets: sai `-UNSIGNED` (nunca debug).

Trocar a keystore muda a assinatura — exige desinstalar/reinstalar no device.

## Build local (opcional)
Android SDK + NDK 26.3 + CMake 3.22.1 + JDK 17 → `./gradlew assembleRelease`.

## Testes do core (Linux)
`cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests`

## Verificação no Realme C33 (dono)
1. Instalar o APK release → landscape: toolbar (3 botões), Hierarchy à esquerda,
   Inspector à direita, viewport com grid no centro, status line (fps/tics/verts/dc).
2. Tocar **+** (cabeçalho Hierarchy) → escolher um preset → o TIC aparece na
   lista e o cubo nasce assente no grid.
3. Selecionar o TIC → Inspector mostra Transform3D → arrastar sliders
   (px/py/pz, rx/ry/rz, sx/sy/sz) → o cubo move/roda/escala no viewport.
4. Orbit: 1 dedo no viewport central; pinch = zoom; painéis não orbitam.
5. Menu → **Save cena** → toast "cena salva (N tics)". Matar a app, reabrir,
   criar outro TIC, Menu → **Load cena** → os TICs salvos voltam com as edições
   (ficheiro em `/data/user/0/vv.goni/files/scene.goni`).
6. Crash log: `/data/user/0/vv.goni/files/goni_crash.log` (mesma rotina da F1).
