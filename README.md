# G.One VV 0.4.1 — F4.1

Engine com editor. A F2 deixou o pipeline 3D mínimo (cubo hardcoded, câmara de
orbit, grid com fade); a F3 transformou os TIC em entidades reais com
componentes, presets de criação, editor (Hierarchy + Inspector) e serialização
de cena `.goni` com migrações. Mobile-first: arm64-v8a, minSdk 24, landscape
travado (`sensorLandscape`). Device de teste: Realme C33 (720x1600).

## Escopo F4.1 (implementado)
Scroll como primitivo de UI, aplicado ao Inspector e à Hierarchy — conteúdo
mais alto que o painel fica alcançável (desbloqueia a verificação da F4 no
device). Só scroll (CLÁUSULA CALMA): sem safe-area, sem colapsáveis, sem
física, sem render 3D.
- `ui/ScrollMath.h` (GL-free, testada no CI): clamp `[0, content − visible]`,
  drag-to-scroll com clamp nos dois extremos, limiar tap (12 px) vs drag,
  protocolo de claim (slider > scroll > botão dentro de região), clip de
  quads por interseção com UV proporcional, geometria do indicador
- `UiContext::beginScroll(id, region, contentHeight)` / `endScroll()` —
  slots por id (offset persiste), botões dentro da região só desenham (o tap
  é re-despachado pelo painel — "add TouchControls" clicável mesmo após
  scroll), sliders mantêm prioridade, indicador fino ACCENT só com overflow;
  recorte por interseção (sem glScissor no quad batch único)
- Hierarchy: lista TODOS os TICs (fim do corte em ~10 linhas da F3); tap na
  linha seleciona via `hierarchyRowAtTap`
- Inspector: BodyComp, `velx` e o botão "add TouchControls" no fundo ficam
  sempre alcançáveis; `ui/EditorLayout.h` é a fonte ÚNICA das alturas de
  conteúdo (desenho e testes partilham os números)
- Gestos sem conflito: drag na região = scroll; drag no slider = slider;
  tap = seleção/ação; viewport central NÃO é região de scroll (orbit/pinch
  intactos)
- CI: suíte do core (122 testes) + APK assinado

## Escopo F4 (implementado)
Física core num novo `TickGroup::Physics` (entre Update e PostUpdate) —
`physics/` é C++ puro, host-testável, sem GL:
- Formas: `Sphere`, `AABB`, `OBB` (rotQuat), `Capsule` (eixo Y local do TIC);
  interseções puras (sphere×{sphere,AABB,OBB,Capsule}, AABB×AABB, OBB×OBB SAT
  15 eixos, Capsule×Capsule) e sweep contínuo com TOI+normal para sphere/
  capsule contra AABB/OBB com CCD adaptativo (substeps máx 8 + avanço
  conservador exato via gradiente da SDF — nunca tunela paredes finas)
- `BodyComp` (Static/Character/Rigid + shape variant + velocity + grounded);
  `PhysicsSystem`: Character com input e slide estilo `move_and_slide`
  (remove a componente normal; iterações extra para cantos), repel entre
  Characters, Rigid com gravidade + colisão primitiva contra Static +
  amortecimento no chão. PLACEHOLDER DE SOLVER: dois Rigid empilhados
  intersectam (sem stacking/resting, documentado)
- `TouchControls` mínimo e FIXO (joystick esquerda + botão JUMP direita, tema
  mono, desenhado só em modo Play) como fonte de `InputSource`; `InputMap`
  liga zero ou uma fonte (sem fonte → sem input); UI criável = F6
- Presets: Player/Character/Static ganham Body, NOVO RigidBody3D; TouchControls
  adicionável via Inspector; Inspector mostra BodyComp (tipo·forma·chão) com
  slider `velx` para lançar corpos no device (testes de CCD/empurrão)
- CI: suíte do core (113 testes) + APK assinado

## Escopo F3.1 (implementado)
A pedido do dono no C33: remover as barreiras artificiais de câmara e baratear
o grid — só câmara + grid + testes (CLÁUSULA CALMA; nada de componentes,
presets, serialização, física ou UI de editor).
- Câmara: zoom 1..300 (era 1.5..40), pitch ±89° (era ±83° — top-down quase
  total sem degenerar o lookAt), yaw livre; near 0.5 / far 450 (zoom máx ×
  1.5, rácio 900:1 — sem z-fighting no depth de 24 bits)
- Grid: a malha de linhas virou UM quad de 4 vértices que segue a câmara, com
  linhas procedurais no fragment shader (`fract` + `fwidth`), anti-moiré por
  minificação e fade radial adaptativo ao zoom; extent ≥ 2000, borda nunca
  visível; interface (`init/draw/vertexCount/ok`) e tema mono mantidos
- CI: suíte do core (74 testes) + APK assinado; o build NDK valida o GLSL
  embutido

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
- CI: `verify-entry-symbols` + suíte do core (113 testes) + APK assinado

Sem física (BodyComp é F4), sem luzes, sem assets, sem animação, sem linguagem
(CLÁUSULA CALMA).

## Histórico
- **F4.1 (0.4.1)**: scroll como primitivo de UI — Inspector e Hierarchy com
  ScrollRegion (clamp, drag-vs-tap, indicador, tap re-despachado); fundo dos
  painéis alcançável no device.
- **F4 (0.4.0)**: física core — 4 formas + sweep/CCD, BodyComp, slide do
  Character, Rigid primitivo, TouchControls (modo Play) e preset RigidBody3D.
- **F3.1 (0.3.1)**: porto do editor — clamps generosos de câmara (zoom 300,
  pitch 89°) e grid de linhas → quad de shader (4 vértices, fade adaptativo,
  anti-moiré); near/far recalibrados.
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
3. Todo push publica o artifact `goni-vv-0.4.1-release-signed` (APK arm64).
   Sem secrets: sai `-UNSIGNED` (nunca debug).

Trocar a keystore muda a assinatura — exige desinstalar/reinstalar no device.

## Build local (opcional)
Android SDK + NDK 26.3 + CMake 3.22.1 + JDK 17 → `./gradlew assembleRelease`.

## Testes do core (Linux)
`cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests`

## Verificação no Realme C33 (dono) — F4.1 (scroll)
1. Instalar o APK 0.4.1 → confirmar "0.4.1" nas infos da app.
2. **Inspector**: selecionar o PlayerBody3D → arrastar PARA CIMA na lista →
   o conteúdo desce e revela `body: …`, `velx` e o botão **add TouchControls**
   no fundo → tocar no botão (clicável após o scroll) → aparece "tc: stick +
   jump"; o indicador fino aparece à direita só quando há overflow.
3. **Hierarchy**: criar 12+ TICs (repetir "+") → arrastar a lista → TODAS as
   linhas alcançáveis; tap numa linha que estava cortada seleciona (frame
   branco + Inspector mostra o TIC).
4. **Gestos sem conflito**: arrastar na pista de um slider (px/py/velx) muda
   o valor e NÃO faz scroll; arrastar fora da pista faz scroll; o viewport
   central continua a orbitar/pinçar como sempre.
5. **Limites**: no topo e no fundo o scroll para (não passa do fim); soltar
   sem mover = tap (não scrolla).

## Verificação no Realme C33 (dono) — F4
1. Instalar o APK 0.4.0 → confirmar "0.4.0" nas infos da app.
2. **Montar a arena** (modo editor):
   - Chão: **+** → StaticBody3D → Inspector: `py −0.5`, `sx 5`, `sy 1`, `sz 5`
     (OBB 5×1×5 com topo em y=0).
   - Parede fina rotacionada: **+** → StaticBody3D → `px 1.85`, `py 0.5`,
     `sx 4`, `sy 1`, `sz 0.2`, `ry 30`.
   - Player: **+** → PlayerBody3D (nasce em (0, 0.55, 0)).
3. **TouchControls**: com o player selecionado → Inspector → botão
   **add TouchControls** → tocar **Play** (toast "modo play") → joystick à
   esquerda empurra o player; botão **JUMP** salta quando `grounded`;
   empurrar contra a parede → **colide e desliza**, nunca atravessa.
4. **CCD**: em modo editor, selecionar o player → `velx 40` → **Play** →
   o player é lançado contra a parede fina e **não tunela** (para/encosta).
5. **Rigid**: **+** → RigidBody3D → `py 3` → **Play** → cai e **para no chão**;
   `velx 8` → desliza e abranda; empurrar o player contra a bola → bloqueia
   (empurrão = `velx` no Inspector). Dois Rigid empilhados intersectam
   (PLACEHOLDER de solver aceite).
6. **Modo editor**: sem Play não há painel de controlos nem física; orbit
   (1 dedo) e pinch continuam; gestos nos controlos não giram a câmara.
7. Menu → **Save/Load cena** persiste os corpos (BodyComp round-trip).
8. Status line: fps/tics/verts/dc; crash log em
   `/data/user/0/vv.goni/files/goni_crash.log`.

## Verificação F3.1 (câmara + grid)
- Pinch afasta até 300 sem a borda do grid aparecer; aproxima até ~1; orbit
  até ~89° sem inversão; sem z-fighting; grid custa 4 vértices (status line).
