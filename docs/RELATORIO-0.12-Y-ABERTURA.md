# RELATORIO-0.12-Y-ABERTURA.md — Campanha 0.12-Y · O Salto Visual e de Assets (Pan, Ícone IA, PBR)

Atualizado: 2026-10-20 · versionCode de partida: **105** · último commit:
`d9b62a1` (docs QoL §5, CI **success**).

Cláusula calma: SÓ o que está no prompt da campanha — o Pan da câmara, o
ícone gerado por IA e o pipeline de texturas PBR. Sem refactors fora disto,
sem mexidas na V.ONI (o registo não ganha entradas — o R-013 segue
aferrado), sem reescritas do render além do que as sub-fases pedem.

## §1 · O estado real (VERIFICA §0.2 — as 3 verificações)

| # | Verificação | Resultado |
|---|---|---|
| 1 | O vc está no 105 ou superior? (QoL fechou) | **SIM** — `app/build.gradle:29` declara `versionCode 105`; o STATE.md fecha a 0.12-QoL («Campanha 0.12-QoL (UI POLISH + NAVEGADOR + AUTO-SAVE) — FECHADA (A..D, vc 102–105)») e a 0.12-X («FECHADA (3.9x, vc 101)») |
| 2 | Os gates verdes? (docs-lint, R-013) | **VERDE** — CI GitHub: run do `d9b62a1` = **success** (workflow release, 5 jobs) · docs-lint local: «VERDE — 117 ficheiros» · release-identity/R-015: «GATE VERDE … versionCode 105 · docs/RELATORIO-0.9.6.md declara o par» |
| 3 | O STATE.md tem a anterior como fechada? | **SIM** — §1 do STATE.md (288 linhas): a 0.12-QoL FECHADA (A vc 102 · B 103 · C 104 · D 105) e a 0.12-X FECHADA |

**O INCIDENTE DO AMBIENTE (documentado para o dono):** esta sandbox
herdou um clone DESATUALIZADO (HEAD `bf47dec` — a ABERTURA da 0.12-X —
com o WIP do camrig/camdrag por commitar no working tree, 309 linhas).
O §0.2 da 0.12-Y FALHOU à primeira (vc 100/101 ≠ 105; R-015 vermelho) e a
campanha PAROU como manda a regra. O dono explicou: a sessão anterior
(noutra sandbox) fechou a 0.12-X (vc 101, commit `69382a6`) e a 0.12-QoL
(A..D, vc 102–105, commits `1befd71`/`98d9da8`/`4a03d64`/`d707d2c`+fixes).
`git fetch origin` confirmou TODOS os hashes no remoto (`origin/main`
`c7cf843..d9b62a1`) e `git reset --hard origin/main` sincronizou o
ambiente. ZERO trabalho perdido — o WIP local era a mesma implementação
que o remoto já tinha fechada com pins/R-055/mutações. A verificação §0.2
acima correu DEPOIS do reset, contra o estado sincronizado.

## §2 · REPORTAR ANTES DE CODAR (as 5 perguntas, com ficheiro:linha)

**1. Onde vive a orbit da câmara do editor?**
`editor::updateCameraOrbit` em `app/src/main/cpp/ui/EditorUi.cpp:4661`
(extraída para ser afervel no CI; o estado do gesto é
`editor::OrbitState` em `ui/EditorUi.h:597` — active/x/y/pinchPrev/
gestureInView). O main chama-a POR FRAME em `platform/main.cpp:6267` com
`claimed | gizmoClaimed | canvasClaimed` e o guard do play/ui/overlay.
1 dedo → `Camera::orbit(dYaw, dPitch)` (`render/Camera.cpp:35`, yaw−=dx,
pitch+=dy clamp ±89°); ≥2 dedos → pinch `cam.zoomBy(pinchPrev/d)`
(`EditorUi.cpp:4723`). A câmara é `Camera g_camera`
(`platform/main.cpp:115`) — target/yaw/pitch/dist fontes da verdade.

**2. Onde vivem os botões Mover/Rodar/Escalar do viewport?**
O RAIL esquerdo: `vpchrome::Layout::rail[5]` (`ui/ViewportChrome.h:121`) —
Selecionar/Mover/Rodar/Escalar/Íman; a geometria é `vpchrome::layout()`
(`ui/ViewportChrome.cpp:131`, alvo `kRailBtn` 40dp, gap 8dp, degradação
em colunas) e o draw é `vpchrome::draw()` (`ViewportChrome.cpp:301-364` —
`toolButton` com o chip individual de vidro, o pai do rail JÁ SAÍU na
QoL C). Os ids vivem em `ViewportChrome.h:70-87` (faixa 30..39; o 38
aposentado pelo D21 com pin caçador — o Pan NÃO o usa). O estado da
ferramenta: `EditorState::selectMode` (`ui/EditorUi.h:236`) +
`toolbar::GizmoModeState::mode` (`ui/Toolbar.h:44`). A legenda
(transitória 1,5s da 0.12-A) nomeia a ferramenta em
`ViewportChrome.cpp:369-382`.

**3. Como o import glTF trata texturas HOJE?**
O parse já lê os 5 índices de slot: `GltfMaterial` (baseColorTex/normalTex/
metalRoughTex/emissiveTex/occlusionTex) em `assets/GltfImporter.h:62-77`.
A EXTRAÇÃO só leva o baseColor: `extractGltfTextures`
(`assets/GltfTextures.cpp:19`) — escreve `textures/gltf_<hash>.png` NA
RAIZ do projeto (dedup FNV-1a) e devolve 1 rel-path POR MESH
(`outMeshTex[m]`). O GTB2 transporta os 5 slots
(`kBindBaseColor..kBindOcclusion`, `assets/GOwnFormats.h:283-306`) com
transform UV por slot, e as props (baseColor/alphaMode/metallic/roughness)
em `GMeshMatProps` (`GOwnFormats.h:380`). O BIND no TIC criado: o
auto-bind do import liga baseColor SÓ quando o ficheiro já está na cache
GPU (`platform/main.cpp:2984-3015` — a grade `matBindsOf` + o fallback
«auto do import» com o warn «SEM ficheiro»); normal/metalRough ficam nos
binds SEM consumidor visual. O SHADER amostra SÓ o unit 0
(`render/Material.cpp:77-110` — kFsSrc: uTex/uHasTex; luz difusa FIXA +
ambient; SEM normal map, SEM roughness/metallic).

**4. Onde nasce o .gmesh do import?**
`AssetConverter.cpp:337` — `rel = "assets/" + stem + ".gmesh"` (a pasta
`assets/` JÁ é a convenção do projeto). O pedido do dono acrescenta a
SUBPASTA de texturas por modelo (`assets/<stem>/textures/`) e o BIND
AUTOMÁTICO dos 4 slots no MeshRenderer do TIC criado
(`GltfInstantiate.cpp:44-60` — a 2.ª passada cria o MeshRenderer; o ctx
não leva texturas hoje).

**5. Onde vivem os presets do «+» e o clear do fundo?**
Os presets: `core/Presets.cpp` (119 linhas — os tipos vivos do menu +
). O fundo: o clear da app lê o token bg do Theme (`kTheme.bg`) — o
WorldEnvironment novo passa a SOBREPOR-I quando ativo. A seriação de
componentes vive no `SceneSerializer` (o dump/loadText — o componente
novo entra no mesmo caminho dos outros).

## §3 · As SUB-FASES e as decisões fechadas

**3.Y-A — PAN DA CÂMARA (vc 106)**
- O Pan desloca o TARGET no plano da câmara: `Camera::pan(dxPx, dyPx,
  viewHPx)` NOVO (`render/Camera.{h,cpp}`) — a fonte única da matemática:
  `worldPerPixel = 2·dist·tan(fovY/2)/viewH`; direita = −right·dx (agarra
  a cena); baixo = +camUp·dy (o y do ecrã cresce para baixo); right =
  (cos yaw, 0, −sin yaw), camUp = cross(right, dirParaOFrente). PURA,
  determinística, sem GL.
- O gesto: `updateCameraOrbit` ganha o parâmetro `panMode` (default false
  — os chamadores existentes byte a byte) e o OrbitState ganha
  panActive/panX/panY + pinchMidX/pinchMidY. 1 dedo com panMode → pan;
  2 dedos EM QUALQUER modo → pinch (distância) E pan (midpoint) ao
  mesmo tempo — o Zoom mantém-se.
- A 6.ª FERRAMENTA no rail: `kVpPanId = 33` (id 33/34 removidos sem pin
  caçador; o 38 aposentado NUNCA volta), `rail[6]`, ícone Hand NOVO no
  `Icons.h`, legenda «Pan». O panMode comporta-se como o selectMode no
  que toca ao gizmo (o gizmo não arrasta em Pan) mas a seleção por tap
  mantém-se.
- Pins (tests/test_012y.cpp): pan_camera_math (direita/baixo/escala/
  zero) · pan_modo_um_dedo (target mexe, yaw/pitch intactos) ·
  pan_dois_dedos_livre (sem panMode: midpoint desloca, dist só muda com
  pinch) · pan_pinch_intacto (zoom de sempre) · pan_rail_sexta_ferramenta
  (o layout + o id + a legenda). Sentinela **R-059** `regress_pan`.
- Mutações: **M-Ya1** (o pan INVERTIDO → pins vermelhos) · **M-Ya2** (o
  pan de 2 dedos morto → pin vermelho). vermelho→reposto→verde coladas
  no relatório §1.

**3.Y-B — O ÍCONE IA (vc 107)**
- A FONTE: gerada pela ferramenta de imagem AI com o prompt do dono
  (obsidiana + veias de magma vermelho, fundo preto puro). O PNG original
  vai para `docs/iconegonevv-ai.png` (o padrão da D-3/Y-0: a fonte vive
  no repo).
- O PROCESSAMENTO (script `scripts/y_gen_ai_icon.py`, o padrão
  y0_gen_icons.py): alpha = clamp((lum−T0)/(T1−T0)) sobre o fundo preto,
  un-premultiply (dividir por max(alpha, piso)), a bbox do NÚCLEO SÓLIDO
  manda no enquadramento, o glifo ocupa 65% da zona segura (66dp de 108)
  no adaptativo e 65% do lado no legacy; SAÍDAS: os 5 mipmaps
  (mdpi 48 · hdpi 72 · xhdpi 96 · xxhdpi 144 · xxxhdpi 192) em
  `ic_launcher.png` + `ic_launcher_round.png` + `ic_launcher_foreground.png`.
- O fundo do adaptativo passa a **#0A0A0C** (a ordem do dono — supera o
  #08080A da Y-0): `values/ic_launcher_background.xml`. O manifest já
  aponta `@mipmap/ic_launcher` (`AndroidManifest.xml:48`) — o APK do CI
  é a prova do link (a11y dos recursos morre no aapt2 se faltar algo).

**3.Y-C — O PIPELINE PBR (vc 108)**
- A PASTA por modelo: o conversor passa a escrever
  `assets/<stem>/<stem>.gmesh` + `assets/<stem>/textures/` (as 5 EXTRAÇÕES
  — baseColor/normal/metalRough/emissive/occlusion — com o dedup de
  sempre). Os imports ANTIGOS (raiz) continuam a carregar: os rel-paths
  guardados nos .goni ficam intactos.
- O BIND AUTOMÁTICO dos 4 slots: o MeshRenderer recém-nascido do import
  recebe os 5 slots da grade do GTB2 SEM depender da cache GPU (o bind
  guarda os rel-paths; a GPU carrega lazy — o warn «SEM ficheiro» morre
  para import novo).
- O SHADER lit PBR-lite (`render/Material.cpp`): uNormalTex (unit 1) com
  TBN por derivadas de ecrã (dFdx/dFdy — ES 3.0, sem atributos novos no
  mesh); uRoughMetalTex (unit 2) + os fatores uRoughness/uMetallic;
  spec Blinn-Phong com shininess de roughness + o mix metálico no albedo.
  Sem textura/fator → byte a byte o lit de hoje (os defaults do shader
  preservam os pins existentes).
- O TIC **WorldEnvironment**: componente NOVO `components/Environment.h`
  (skyColor, fogColor, fogDensity, hdriPath) + o preset no «+» +
  Inspector + seriação. O draw: o clear segue o skyColor; a névoa é exp2
  no FS (uFogColor/uFogDensity — 0 = morta, byte a byte); o HDRI
  (equirectangular 2:1) é o FUNDO (pass fullscreen com o raio da inverse
  VP) e a média CPU de luminância escala o ambient. As fontes PURAS
  (equirectUv, fogFactor, ambientFromAvg) vivem em `render/Environment.h`
  para os pins.
- Pins: gmesh_assets_folder (o conversor novo) · bind_auto_5_slots ·
  material_pbr_defaults (byte a byte sem texturas) · env_seriação +
  equirect/fog/ambient puros. Sentinela **R-060** `regress_pbr_env`.
- Mutações: **M-Yc1** (o slot normal morto no conversor) · **M-Yc2** (a
  névoa fora do FS). Coladas no relatório §3.

## §4 · O FECHO (o que a campanha deve entregar)

- Commit + push NO FIM DE CADA SUB-FASE (o STATE.md no MESMO commit;
  docs-lint verde; CI verde em todos os jobs).
- O relatório único `docs/RELATORIO-0.12-Y.md` (§1 Pan · §2 Ícone · §3
  PBR · §4/§5 o fecho) + as entradas em `docs/REGRESSOES.md` (R-059,
  R-060) + o §14 do RELATORIO-0.9.6.md com os pares vc 106/107/108.
- A referência V.ONI NÃO regenera (ZERO entradas no registo — a cláusula
  calma; o R-013 segue aferrado).
- No fim: PARAR e reportar ao dono.
