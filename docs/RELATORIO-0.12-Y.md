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

## §3 · 3.Y-C — O PIPELINE PBR (preenchido na sub-fase)

## §4 · As mutações (índice)

Ver §1 (M-Ya1/M-Ya2) e as secções §2/§3 quando fechadas.

## §5 · O fecho da campanha (preenchido no FECHO)
