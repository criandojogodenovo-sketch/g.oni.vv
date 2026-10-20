# RELATORIO-0.12-Y.md — Campanha 0.12-Y · O Salto Visual e de Assets

Atualizado: 2026-10-20 · versionCode de partida: **105** · a abertura vive
no `RELATORIO-0.12-Y-ABERTURA.md` (o §0.2 verde + o incidente do ambiente +
as 5 perguntas + as decisões fechadas).

Cláusula calma: SÓ o que está no prompt da campanha — o Pan da câmara (A),
o ícone gerado por IA (B) e o pipeline de texturas PBR (C). A V.ONI fica
INTACTA (o registo não ganha entradas — o R-013 segue aferrado; a
referência pública NÃO regenera).

## §1 · 3.Y-A — O PAN DA CÂMARA (a MÃOZINHA)

**A ordem do dono**: «O utilizador só consegue orbitar e fazer zoom. Falta
o Pan (arrastar a cena nos eixos X/Y/Z). 1. Botão "Mãozinha" na barra do
viewport junto ao Mover/Rodar/Escalar. 2. Modo Pan ativo: 1 dedo move a
câmara (o ponto de foco no plano da câmara). 3. 2 dedos a arrastar = Pan em
qualquer momento (mesmo no modo Orbit), mantendo o Pinch para Zoom.»

### O que mudou

- **`render/Camera.{h,cpp}`** — a FONTE ÚNICA da matemática do pan (o
  padrão gridFadeForDist: puras no header, o draw e os pins aferem a MESMA
  função): `camWorldPerPixel(dist, fovY, viewH)` = 2·dist·tan(fovY/2)/viewH
  (a velocidade do dedo 1:1 em QUALQUER zoom); `camRightVec(yaw)` =
  (cos yaw, 0, −sin yaw); `camUpVec(yaw, pitch)` = (−sp·sy, cp, −sp·cy) —
  os eixos do PLANO da câmara. O `Camera::pan(dxPx, dyPx, viewHPx)` NOVO
  desloca o TARGET: dedo → direita ⇒ target −= right·Δ (a cena segue o
  dedo); dedo ↓ ⇒ target += camUp·Δ (o y do ecrã cresce para baixo);
  viewH ≤ 0 = degenerado (nada — nunca divide por zero); o yaw/pitch/dist
  ficam INTACTOS.
- **`ui/EditorUi.{h,cpp}`** — o `OrbitState` ganha `pinchMidX/pinchMidY`
  (o midpoint dos 2 dedos; o reset vive nos MESMOS portões do pinchPrev —
  0 dedos, playMode, 1 dedo). O `updateCameraOrbit` ganha o parâmetro
  `panMode` (DEFAULT false — os chamadores existentes ficam byte a byte):
  1 dedo com panMode → `cam.pan` em vez de `cam.orbit`; **2 dedos em
  QUALQUER modo** → pinch (a distância) E pan (o midpoint) ao mesmo tempo
  — o 1.º frame de 2 dedos SÓ ancora (o MESMO contrato do pinch: nada de
  salto). O `EditorState::panMode` NOVO (junto ao selectMode).
- **`ui/Icons.{h,cpp}`** — o glifo `Hand` NOVO (a MÃOZINHA: contorno
  fechado de mão aberta com 3 dedos + polegar, segmentos retos — a regra
  do conjunto; nPts 17, 1 polilinha fechada; o nome no índice é "mao").
- **`ui/ViewportChrome.{h,cpp}`** — o rail passa a **6 FERRAMENTAS**:
  Selecionar/Mover/Rodar/Escalar/**PAN**/Íman (o Pan entra entre o Escalar
  e o Íman — «junto ao Mover/Rodar/Escalar», a ordem do dono);
  `kVpPanId = 33` (o reuso segue o precedente do 37 — o 38 aposentado do
  D21 fica morto com o pin caçador). O toque na MÃOZINHA arma
  `st.panMode` (e desarma o selectMode); os chips Mover/Rodar/Escalar
  desarmam-no (os modos são exclusivos); a legenda transitória diz «Pan».
- **`platform/main.cpp`** — o `updateCameraOrbit` do frame recebe
  `g_editor.panMode`.

### Pins (tests/test_012y.cpp NOVO — 5)

- `pan_camera_math` — a fonte única (wpp/exatidão, eixos unitários e
  ortogonais, a direção AGARRA a cena, a magnitude = px·wpp, o
  yaw/pitch/dist intactos, o viewH degenerado).
- `pan_modo_um_dedo` — panMode: o target anda 100·wpp para a ESQUERDA com
  yaw/pitch EXATOS; sem panMode o MESMO gesto orbita e o target fica;
  o vertical sobe o target.
- `pan_dois_dedos_livre` — SEM panMode: o midpoint arrasta (50 px →
  50·wpp), d constante = zero zoom, 2 dedos NUNCA orbitam; o vertical idem.
- `pan_pinch_intacto` — o zoom de sempre (6 → 3 na razão 100/200); o
  midpoint parado = zero pan; o 1.º frame de 2 dedos não salta.
- `pan_rail_sexta` — a geometria (rail[4] sob o Escalar, o passo 48dp, o
  Íman em rail[5], o 280dp cabe, a degradação 2 colunas), o glifo Hand no
  conjunto, a FONTE (Icon::Hand + "Pan" no draw), o toque ARMA o panMode
  (no largar — o padrão da casa) e o Mover desarma-o.

### Sentinela R-059 `regress_pan` (tests/test_sentinels.cpp)

Vigia a CLASSE: o pan invertido (o "agarra a cena" morre), o pan de 2
dedos morto (o midpoint deixa de alimentar o Camera::pan) e a órbita a
vazar para o target (a exclusividade dos modos). Entrada NOVA no
`docs/REGRESSOES.md`.

### Mutações (vermelho → reposto → verde)

- **M-Ya1** (o pan INVERTIDO — `target = target + right·Δ` em Camera.cpp):
  `pan_camera_math` VERMELHO («pan para a direita tem de levar o target
  para a ESQUERDA (agarra a cena) — target.x **0.7942**»),
  `pan_modo_um_dedo` VERMELHO («o target não andou 100·wpp para a
  esquerda (**1.15470**)»), `pan_dois_dedos_livre` VERMELHO
  («0.57735 vs -0.57735»), `regress_pan` VERMELHA («target.x **1.1547**»).
  REPOSTO → os 4 VERDES. Colada acima.
- **M-Ya2** (o pan de 2 dedos MORTO — o `cam.pan` do midpoint fora no
  updateCameraOrbit): `pan_dois_dedos_livre` VERMELHO («o midpoint não
  arrastou o target (**0.00000** vs -0.57735)»), `regress_pan` VERMELHA
  («R-059: o pan de 2 dedos MORREU (o midpoint não arrasta — target.x
  **0.00000**)»). REPOSTO → os 2 VERDES (e os afetados pela M-Ya1
  continuaram VERDES — as mutações são independentes). Colada acima.

### Verificação

- test_core local: os 5 pins pan_ VERDES ×2 (pré/pós-mutação reposta);
  as únicas falhas locais são os flakes da sandbox DOCUMENTADOS — o crash
  do `regress_streaming_paralelo` (SIGSEGV no thread::join — PROVADO
  preexistente: o MESMO crash corre no HEAD pristine 9f58992 sem as
  mudanças desta campanha; 2 CPUs da sandbox) e o fork do
  `010m_medicoes` (o par documentado desde a QoL D). **O CI é o árbitro.**
- Os achados fora da lista (documentados, NÃO mexidos nesta campanha):
  o `kCheckPts` do Icons.cpp tem um ponto duplicado latente (o 2.º traço
  do ✓ tem comprimento ZERO no desenho — o glifo desenha só a perna
  esquerda); a cura cabe numa campanha de glifos.

### Ficheiros

render/Camera.h + render/Camera.cpp + ui/EditorUi.h + ui/EditorUi.cpp +
ui/Icons.h + ui/Icons.cpp + ui/ViewportChrome.h + ui/ViewportChrome.cpp +
platform/main.cpp + tests/test_012y.cpp (NOVO) + tests/test_sentinels.cpp
(R-059) + tests/CMakeLists.txt + docs/REGRESSOES.md (R-059) +
docs/RELATORIO-0.12-Y.md (este) + docs/RELATORIO-0.9.6.md (§14, o par
vc 106) + STATE.md + app/build.gradle (versionCode 106).

## §2 · 3.Y-B — O ÍCONE IA (obsidiana + magma)

**A ordem do dono**: «USA A TUA FERRAMENTA DE GERAÇÃO DE IMAGEM para
criar: "A premium 3D app icon, a stylized letter G made of dark obsidian
glass with glowing red magma veins inside. Studio lighting, pure black
background, high resolution, Unreal Engine 5 style." Processa: remove o
fundo preto (threshold de luminância ou canal alpha), aplica a Safe Zone
do Android (o G ocupa ~65% do centro), gera os 5 tamanhos de mipmap (mdpi
a xxxhdpi), atualiza o ic_launcher_foreground.png e o ic_launcher.xml
(fundo #0A0A0C).»

### O que mudou

- **`docs/iconegonevv-ai.png` (NOVO, 1024×1024)** — a FONTE gerada pela
  ferramenta de imagem AI com o prompt EXATO do dono (o padrão D-3/Y-0:
  a fonte vive no repo, é história).
- **`scripts/y_gen_ai_icon.py` (NOVO)** — o gerador da 0.12-Y (o irmão do
  y0_gen_icons.py). O ACHADO da extração: o threshold de luminância da
  Y-0 NÃO serve a fonte nova (a face da obsidiana é lum≈9 — comeria o
  tile; o reflexo vermelho do chão é lum≈52 — passaria como conteúdo).
  A EXTRAÇÃO é por TEXTURA (high-pass |im − blur| ≥ 6 no canal máximo):
  o fundo gradiente e o reflexo são SUAVES (morrem no high-pass — a
  densidade cai a 0.00–0.03), o tile é TEXTURADO (rebordo/veias/grão —
  a densidade 0.57–0.69); a projeção com o piso de 25% dá a bbox,
  encolhe 4 px, quadrifica-se pelo menor lado centrado e recebe a
  máscara round-rect antialiased (raio 13%) — os pixels do tile ficam
  INTACTOS (nada de un-premultiply: a fonte não é um glifo sobre preto).
  O realce mild do launcher (saturação +18%, contraste +10%, brilho +5%)
  para a legibilidade nos tamanhos pequenos — NADA de retouche da arte.
- **A ESCALA** — o MESMO contrato Y-0-FIX que o dono validou no device:
  o tile ocupa 65% da ZONA SEGURA do adaptativo (66 dp de 108 dp ⇒
  39,72% do canvas — a diagonal 43 dp cabe na zona segura e no viewport
  de 72 dp); no legacy 65% do lado.
- **Os 15 PNGs** (5 densidades × 3): foreground 108/162/216/324/432 (o
  canvas 108 dp) + legacy 48/72/96/144/192 (ic_launcher + ic_launcher_
  round com a máscara circular, o círculo cheio do bg).
- **`values/ic_launcher_background.xml`** — o fundo passa a **#0A0A0C**
  (A ORDEM DO DONO, supera o #08080A da Y-0; a nota da superação fica no
  comentário). O manifest JÁ aponta `@mipmap/ic_launcher`
  (AndroidManifest.xml:48) e o adaptativo compõe o foreground — o APK do
  CI é a prova do link (o aapt2 morre se faltar recurso; o pin
  y0_aapt2/y0_manifest da Y-0 continua a aferrar).

### Pins

- `icon_ia_mipmaps` (test_012y.cpp) — as dimensões EXATAS dos 15 PNGs
  (o header IHDR parseado à mão: legacy 48/72/96/144/192, foreground
  108/162/216/324/432), a FONTE IA no repo, o GERADOR com o contrato
  (TILE_FILL 0.65 · SAFE_ZONE_DP 66 · o bg (10,10,12)), o manifest a
  apontar, o fundo #0A0A0C na LINHA da cor (a menção histórica no
  comentário fica) e o adaptativo a compor o foreground.
- **Recalibrações com nota** (a arte mudou, o CONTRATO 65% fica):
  `y0_background` (#0A0A0C — a ordem do dono), `y0_escala` (o foreground
  [38..41]% — o tile mede 39.5–39.8%; o legacy [52..66]% — o lum≥90
  apanha o G+rebordo claros DENTRO do tile: o mdpi perde o rebordo no
  kernel (54.2%), o xxxhdpi apanha-o todo (64.1%)).

### Prova de mutação

- **M-Yb1** (a safe zone VIOLADA — TILE_FILL 0.65 → 0.25, o bug Y-0
  original de volta): `icon_ia_mipmaps` VERMELHO («TILE_FILL = 0.65»
  fora da fonte) + `y0_escala` VERMELHO («o conteúdo sólido mede 14.8%
  do canvas — fora da janela [38..41]%»). REPOSTO + regenerado → os 2
  VERDES. Colada acima.

### Verificação

- test_core local: os pins VERDES (a única falha local = o fork do
  medicoes, o flake da sandbox documentado — o CI é o árbitro).
- Os 3 lançadores cobertos: adaptativo (Android 8+), legacy quadrado e
  legacy redondo.

## §3 · 3.Y-C — O PIPELINE PBR + O WORLDENVIRONMENT

**A ordem do dono**: «O utilizador importou um modelo 3D e as texturas não
apareceram. Lê o material do glTF e extrai Albedo, Normal, Roughness e
Metallic. Cria a pasta assets/NomeDoModelo/ e coloca lá o .gmesh e uma
subpasta textures/ com as imagens. Conecta-as automaticamente ao
MeshRenderer (os 4 slots). Activa as capacidades no Material.cpp. NOVO
PRESET NO "+": cria um TIC que permite carregar um HDRI (mapa 360) para
iluminar a cena e definir a cor do céu/fog.»

### O que mudou

- **`render/Pbr.h` NOVO** — a FONTE ÚNICA da matemática PBR-lite (o
  padrão camWorldPerPixel/gridFadeForDist: puras no header, o FS do lit
  replica LINHA A LINHA e os pins aferem a MESMA conta sem GL):
  `roughToShininess` (rough 0..1 → expoente 4..256, o quadrado suaviza),
  `blinnSpec` (o half-vector H = normalize(L+V)), `metallicDiffuseScale`
  (o metal não tem difusa — escala 1−metallic·0.85), `fogFactorExp2`
  (1−exp(−(d·ρ)²); density 0 = MORTA = o lit de sempre byte a byte) e
  `equirectUv` (o uv do mapa 360: u = 0.5 + atan2(z,x)/2π, v = acos(−y)/π).
- **`render/Material.{h,cpp}`** — o lit ganha o PBR-LITE a cada draw:
  uNormalTex (unit 1) com a perturbação por DERIVADAS DE ECRÃ (dFdx/dFdy
  do ES 3.0 — a TBN sem atributos novos no mesh: o mesh antigo fica byte
  a byte), uRoughMetalTex (unit 2, o pacote glTF: roughness no G,
  metallic no B), os FATORES uRoughness/uMetallic (o default glTF 1/1),
  a NÉVOA (uFogColor/uFogDensity) e o uAmbientScale (o HDRI do
  WorldEnvironment). A API nova: `setPbr(normalTex, roughMetalTex,
  metallic, roughness)` + `setFog(rgb, density, eye)` +
  `setAmbientScale(scale)` — nullptr/density 0 = os ramos mortos = o lit
  de sempre (o contrato byte a byte do draw antigo). O DrawProps ganha
  metallic/roughness (o GTB2 já os guardava desde a 0.11-M — agora ENTRAM
  no draw via fromGMesh).
- **`assets/GltfTextures.{h,cpp}`** — `extractGltfTexSlots` NOVO: os 5
  SLOTS do material glTF (baseColor/normal/metalRough/emissive/occlusion —
  o parse já lia os índices, a extração só levava o baseColor) na PASTA
  DO MODELO `assets/<stem>/textures/` (a ordem do dono; o modelo sem nome
  usa "modelo" — a pasta nunca é a raiz); a mesma dedup FNV-1a por BYTES
  (1 ficheiro por textura única); a textura EXTERNA (uri) passa-through;
  jpeg/fora do escopo não falha o import. `GltfModel::meshTexSlots`
  (meshes × 5 achatado, "" = slot vazio) ao lado do meshTexture antigo
  (os consumidores antigos ficam byte a byte).
- **`assets/ResourceManager.{h,cpp}`** — `meshTexSlotFor(ref, slot)` NOVO
  ("meshes/x.glb#0" → assets/<stem>/textures/gltf_<hash>.png), a mesma
  regra do sub-ref do mesh().
- **`assets/AssetConverter.cpp`** — o `writeAsset` cria a PASTA do alvo
  (o makeDirs antes do writeBytes — o .gmesh pode nascer em subpasta) e o
  `migrateLegacyAssets` aceita as DUAS gerações (assets/<stem>.gmesh OU
  assets/<stem>/<stem>.gmesh — os .goni antigos intactos).
- **`components/Environment.h` NOVO** — o TIC WORLDENVIRONMENT é DADOS:
  skyColor/fogColor/fogDensity/ambient/hdriPath/active + clampFields (o
  padrão AudioPlayer); as FONTES PURAS `averageColor` (a média CPU do
  equirect, o teto ~4096 amostras) e `ambientFromAvg` (a luminância
  (0.2126, 0.7152, 0.0722) × ganho 4 — o PISO 0.25 nunca deixa o HDRI
  escuro apagar a cena; o TETO 4). GL-free (o CI prova sem GL).
- **`core/ComponentStore.{h,cpp}`** — o registry ganha "Environment" (o
  14.º componente — os ids dos 13 existentes INTACTOS) + o storage
  environments_ + o removeAll/hasAny.
- **`core/Presets.{h,cpp}`** — `PresetKind::WorldEnvironment` (o 7.º
  preset): SÓ Transform+Environment (SEM mesh, SEM física — a estrutura
  primeiro; a origem, que o "sítio" não importa — o singleton lógico da
  cena como a câmara); `presetName` → "WorldEnvironment".
- **`core/SceneSerializer.cpp`** — o Environment no .goni (append + fill:
  sky/fog/fogDensity/ambient/hdri/active; os DEFAULTS OMITIDOS = o
  componente novo — forward-compat como o resto do serializer; o
  clampFields no load).
- **`ui/EditorUi.cpp`** — o menu "+" 3D passa a 8 itens (o 8.º é o
  WorldEnvironment).
- **`platform/main.cpp`** — (a) o BIND AUTO dos slots vazios no
  `expandJobPerPiece` (o caminho é a verdade: `meshTexSlotFor` → slot, o
  slot 0 também alimenta o texPath/texture quando a cache GPU o tem);
  (b) `environmentApplyHdri` no pós-load (cada HDRI → ambient pela média
  CPU; o PNG ilegível mantém o ambient que tem — a honestidade do
  import); (c) o frame do lit: o 1.º Environment ATIVO manda (o
  invariante da câmara ativa) — o CÉU é o clear (sobreposto ao token), a
  NÉVOA + o AMBIENT entram nos uniforms 1× por frame (os draws só
  consomem); SEM Environment = os defaults de sempre (density 0, ambient
  1 — o lit byte a byte); (d) o choice 8 do menu cria o TIC (o toast
  ensina «céu/névoa/HDRI»).

### Pins (tests/test_012y.cpp +4)

- `pbr_fontes` — a fonte única: o shininess (0→256, 1→4, 0.5→67, o
  clamp), o especular (N=H→1, perpendicular→0), o mix metálico
  (1.0→0.15), a névoa (density 0 = MORTA; d·ρ=1 → 0.6321) e o uv
  equirect (+x→(0.5,0.5), −y→v=0, +z→u=0.75).
- `env_media` — a média CPU: vazio/zero sem crash, 1×1 vermelho →
  luminância 0.2126 → ambient 0.85, branco → o teto 4, preto → o PISO
  0.25, 2×2 misturado → 0.5.
- `gltf_5slots_pasta` — a EXTRAÇÃO: 2 meshes × 5 slots na PASTA do modelo
  (assets/cidade/textures/gltf_*.png), os slots vazios honestos, o DEDUP
  (3 imagens iguais → 1 ficheiro no disco), o stem vazio → "modelo", a
  FONTE (o caminho "/textures" no GltfTextures.cpp + a ordem
  kBindBaseColor no GTB2).
- `env_roundtrip` — o TIC sobrevive ao .goni (os 6 campos byte a byte) +
  o preset NOVO (o nome + a estrutura Transform+Environment).

### Recalibrações (com nota)

- `test_components` — o registry 13 → 14 (o Environment entra NO FIM —
  os ids dos 13 intactos, o contrato da ordem fixa).
- `test_texbind` — os meshes do catálogo na PASTA do modelo
  (assets/m/m.gmesh) — a forma nova do caminho; o CONTRATO do bind auto
  (5 slots no apply, a troca de mesh) intacto.
- `test_wiring011` — `PresetKind::Count` 6 → 7; a geometria do menu "+"
  3D passa a 8 linhas (o teste media 7 e o toque saía do card real —
  apanho honesto da recalibração).

### Sentinela R-060 `regress_pbr_worldenv` (tests/test_sentinels.cpp)

Vigia a CLASSE: a fórmula PBR fora da fonte única (o shader e os pins a
divergir às escondidas), a névoa a vazar para o lit de sempre (density 0
tem de ser MORTA), o HDRI escuro a APAGAR a cena (o piso 0.25 do
ambient), o .goni a PERDER o WorldEnvironment no roundtrip e a ORDEM dos
slots (meshes×5, a ordem kBind*) a regredir. Entrada NOVA no
`docs/REGRESSOES.md`.

### Mutações (vermelho → reposto → verde)

- **M-Yc1** (a névoa INVERTIDA — `fogFactorExp2` perde o `1−` e vira
  exp(−(d·ρ)²): perto molhado, longe limpo, e density 0 → 1.0 — a MORTA
  morreu): `pbr_fontes` VERMELHO («fogFactorExp2(1.0,1.0) 0.6321» +
  «fogFactorExp2(10.0,0.1) 0.6321»), `regress_pbr_worldenv` VERMELHO
  («R-060: a curva exp2 mudou (d·ρ=1 → 0.6321)»). REPOSTO → os 2 VERDES.
- **M-Yc2** (o .goni PERDE O HDRI — o `if (!hdriPath.empty())` do
  `appendComponentJson(Environment)` morto): `env_roundtrip` VERMELHO
  (o "atardecer.png" sai do dump), `regress_pbr_worldenv` VERMELHO
  («R-060: o .goni PERDEU o HDRI (o mapa 360)»). REPOSTO → os 2 VERDES.

### Verificação

- test_core local: os 4 pins + a R-060 VERDES (corrida completa 1206 OK;
  as únicas falhas locais = os flakes da sandbox documentados — o fork
  do medicoes e o crash intermitente do regress_streaming_paralelo,
  provado preexistente na §1; **o CI é o árbitro**).
- c33_virtual 1009/0.

### Ficheiros

render/Pbr.h (NOVO) + render/Material.h + render/Material.cpp +
assets/GltfTextures.h + assets/GltfTextures.cpp + assets/GltfImporter.h +
assets/ResourceManager.h + assets/ResourceManager.cpp +
assets/AssetConverter.cpp + components/Environment.h (NOVO) +
core/ComponentStore.h + core/ComponentStore.cpp + core/Presets.h +
core/Presets.cpp + core/SceneSerializer.cpp + ui/EditorUi.cpp +
platform/main.cpp + tests/test_012y.cpp + tests/test_sentinels.cpp
(R-060) + tests/test_components.cpp + tests/test_texbind.cpp +
tests/test_wiring011.cpp + docs/REGRESSOES.md (R-060) +
docs/RELATORIO-0.12-Y.md (este) + docs/RELATORIO-0.9.6.md (§14, o par
vc 108) + STATE.md + app/build.gradle (versionCode 108).

## §3-FIX · O FIX DO DONO — A UI DO ENVIRONMENT E O BIND DAS TEXTURAS (vc 109)

O dono testou o APK (vc 108) no telemóvel e reportou DOIS problemas:
(1) o Inspector do TIC WorldEnvironment só mostrava Transform/Animação/
Script — o componente existia, a UI NÃO (o C criou o TIC, o serializer e
o main, mas NENHUM controlo no Inspector); (2) o modelo continua BRANCO —
o material não mostra a textura e ninguém sabe porquê. O fix segue a
AÇÃO do dono à letra.

### O que mudou (por ficheiro)

- **ui/EditorLayout.h** — o perfil ganha `env` (o Environment presente);
  o BIT NOVO `kInspBitEnvironment` (bit8, persiste no inspCollapsed); os
  5 KINDS NOVOS (EnvSky/EnvHdri/EnvAmb/EnvFogD/EnvFogC); os ids
  5420..5424 (a faixa da câmara 5400..5412 e os fields 5530+ intocados);
  o plano empurra a secção (cabeçalho + 5 linhas NA ORDEM do dono: céu →
  hdri → ambiente → névoa → cor da névoa).
- **ui/EditorUi.cpp** — os 5 casos de draw: céu/névoa = linha hex + SWATCH
  vivo (o MESMO padrão da linha hex do tint), hdri = botão de linha
  inteira com o NOME limpo, ambiente = slider 0..2, névoa = slider 0..1
  (o clampFields da casa em cada escrita); o tap re-despacha os 3 botões
  (céu/névoa → teclado hex propósitos 11/12; hdri → a INTENÇÃO
  `st.hdriPick`); os rótulos do Environment preenchidos no pré-pass; a
  kTitles[9] ganha "Environment".
- **ui/EditorUi.h** — o campo `hdriPick` no EditorState (o contrato
  UI→main, o padrão pickBlockedHint).
- **ui/UiEditor.cpp** — os propósitos 11/12 entram no MESMO teclado hex
  da casa (a tecla "#" e o parse `uiHexParse`); o commitTextInput
  escreve skyColor/fogColor + clampFields (inválido = estado intacto —
  o contrato do hex).
- **platform/main.cpp** — o fluxo HDRI COMPLETO: a intenção abre o
  NAVEGADOR na raiz de sempre (`browserOpen` mata qualquer intenção
  antiga — o browser aberto por outra via NUNCA sequestra o pick); o
  `browserImportHdri` NOVO intercepta o pick do .png ANTES do import de
  sempre: valida (isFile + loadPng), escreve `textures/hdri_<stem>.png`
  (colisão → sufixo -2/-3), aponta o `hdriPath` do Environment do TIC
  selecionado e calcula o ambient pela MÉDIA CPU (as MESMAS fontes puras
  — sem segundo I/O); .hdr → fora do escopo HOJE com aviso honesto (o
  decoder RGBE não existe na casa — o .goni aceita o ref, o import
  precisa do decoder próprio); sem TIC WorldEnvironment → toast + log.
- **assets/GltfTextures.cpp** — O LOG DA ORDEM DO DONO, LITERAL:
  `[material] textura extraida: <nome> -> <caminho> (slot <slot>)` por
  textura escrita + o skip NUNCA mais silencioso (`[material] slot X
  IGNORADO: mime 'image/jpeg' fora do escopo` — a causa comum do modelo
  branco diz-se AGORA no engine.log).
- **assets/GltfImporter.cpp** — o log do parse do material: cada slot que
  o glTF liga sai como `[material] material N (nome): slot baseColor ->
  imagem M` (o dono vê O QUE o material pediu ANTES da extração).
- **render/Material.cpp/.h** — A GARANTIA DO BIND: o sampler do albedo
  chama-se **uBaseColorTex** (o nome do contrato, o slot 0 do PBR) no FS
  e no init (o fallback `uTex` é o retro-guarda); `setTexture` liga a
  textura ao UNIT 0 (o normal fica no 1, o rough/metal no 2 — os três
  nunca colidem) e o LOGI do init diz o contrato.
- **O THUMB DO BASECOLOR (Inspector)** — a VERDADE do slot 0: a grade do
  material pode estar vazia com a textura VIVA (o bind clássico pelo
  texPath); o slot 0 vazio agora HERDA o texPath no pré-pass — o rótulo
  e o thumb 24dp da linha baseColor mostram a textura REAL (o dono VÊ se
  carregou; o resolver que falha desenha a moldura honesta de sempre).

### Pins (test_012y.cpp)

- **inspector_worldenv** — o plano do Inspector de um TIC
  Transform+Environment: 1 cabeçalho Environment + AS 5 LINHAS na ORDEM
  (os ids 5420..5424 distintos, fora da faixa da câmara), o colapso
  esconde as 5 e deixa o cabeçalho, a altura do conteúdo reflete, e um
  TIC SEM Environment NÃO ganha a secção.
- **pbr_texture_bind** — o CONTRATO no fonte (o FS declara
  `uniform sampler2D uBaseColorTex;` e o init consulta o nome — a
  leitura é a verdade, o fb auto-regista qualquer nome) + o LitMaterial
  REAL no stub fb: `setTexture(tex)` → uHasTex=1 + o sampler no UNIT 0 +
  o bind da textura do albedo; `setTexture(nullptr)` → uHasTex=0 SEM
  bind; o setPbr (normal 1 / rough-metal 2) não rouba o unit 0.

### Mutações (vermelho → reposto → verde)

- **M-Yf1** (o NOME do contrato morre — o FS volta a `uTex`):
  `pbr_texture_bind` VERMELHO («o FS nao declara o sampler
  uBaseColorTex»). REPOSTO → VERDE.
- **M-Yf2** (a secção Environment morta — o push do plano sai):
  `inspector_worldenv` VERMELHO (5 EXPECTs — as linhas, o colapso, a
  altura). REPOSTO → VERDE.

### Verificação

- test_core local: os 2 pins NOVOS + a suíte inteira VERDES (as únicas
  falhas locais = os flakes da sandbox documentados — o fork do
  sentinela_r039 e o crash intermitente do regress_streaming_paralelo,
  provados preexistentes; **o CI é o árbitro**).
- c33_virtual 1009/0.

### Ficheiros

ui/EditorLayout.h + ui/EditorUi.h + ui/EditorUi.cpp + ui/UiEditor.cpp +
render/Material.h + render/Material.cpp + assets/GltfTextures.cpp +
assets/GltfImporter.cpp + platform/main.cpp + tests/test_012y.cpp +
docs/RELATORIO-0.12-Y.md (este) + docs/RELATORIO-0.9.6.md (§14, o par
vc 109) + STATE.md + app/build.gradle (versionCode 109).

## §4 · As mutações (índice)

Ver §1 (M-Ya1/M-Ya2), §2 (M-Yb1), §3 (M-Yc1/M-Yc2) e §3-FIX
(M-Yf1/M-Yf2).

## §5 · O fecho da campanha (preenchido no FECHO)
