# RELATÓRIO 0.12-E — EMERGÊNCIA + REDESIGN 2.5D (FOGO AVERMELHADO)

> A ordem de EMERGÊNCIA do dono: o shader SDF único de widget (0.12-D D-1)
> e o vidro/fogo do shader único (D-2/D-3) **estragaram o TEXTO e o LAYOUT
> no device real** (texto invisível, painéis sem tamanho, caixas pretas
> flutuantes). A ordem literal: «Desliga o SDF e o cálculo 3D. Volta ao
> desenho simples no `UiContext`» + o REDESIGN 2.5D com a paleta de FOGO
> AVERMELHADO (brasa). Cláusulas do dono: ZERO matemática 3D (sem normais,
> sem luz 3D), ZERO mexidas no `FontAtlas` (texto SEMPRE por cima), NÃO
> alterar a arquitetura de TICs.
> A campanha 0.12-D fica PARCIALMENTE ABORTADA: D-1/D-2/D-3 dormentes (a
> maquinaria fica no código SEM chamadores); o D-4 (o logo «G em chama»,
> desenho por SEGMENTOS 2D — sem SDF) SOBREVIVE intacto; D-5..D-8
> cancelados pela emergência (a fila morre com a ordem).

## 1 · E-1 — A RESTAURAÇÃO DA UI BÁSICA (abortar SDF 3D)

### 1.1 O que foi feito

- **O CHOKE POINT `drawButtonVisual` (UiContext.cpp) voltou ao desenho
  simples 2D de sempre (o padrão Y-1)**: o ativo é o glow de foco +
  `panelRoundedGradientV` accent→accent2; o repouso é `panelRounded` PANEL
  + o `frameRounded` 1dp. Os 4 emissores SDF/3D saem do caminho: `sdfWidget`,
  `fireGlowBelow`, `sdfFireBorder` — zero chamadores no produto. A nota de
  aborto vive NO SÍTIO (o comentário no código cita a ordem do dono).
- **O `iconButton` da barra de topo (Toolbar.cpp)** volta ao chip 2D: o
  ativo é o gradiente accent→accent2 + o `glassTopLine` (o brilho 1dp do
  topo regressa); o held/ativo é o `panelRounded` surface2/accentPress. O
  glifo do LOGO do cabeçalho (D-4, `brand::drawLogo`) fica INTACTO — é
  desenho por segmentos 2D, nunca foi SDF.
- **O VIDRO REAL (D-2) MORREU no produto**: o bloco de CAPTURA do
  `main.cpp` (o `glReadPixels` + o blur dual Kawase + a cache — o
  `setGlassTexture`/`captureGlass`) saiu do frame inteiro. A causa real
  das «caixas pretas flutuantes» é o ramo de textura do shader abortado
  (a captura/UV no device); sem captura e sem shader, não há caixas.
- **O VIDRO FALSO (D-2) MORREU**: a hierarquia e o inspector voltam ao
  `ui.panel` (surface/PANEL) + `glassTopLine` + o divisor gradiente Y-3.
- **Os 6 menus flutuantes** (plusMenu, fileMenu, hierMenu, scenesMenu,
  importMenu, assetMenu) voltam ao `panelRounded` (o ELEVADO da 0.12-C nos
  sheets, o PANEL nos cards) + `glassTopLine` + `frameRounded`.
- **A maquinaria DORMENTE** (documentada, NÃO apagada — a reversão
  cirúrgica mantém o diff mínimo e o rollback possível): `ui/Sdf.h`,
  `render/SdfBatch.h`, `render/Renderer` progSdf_/glass (o captureGlass e
  os uniforms SDF continuam compilados), `ui/Glass.h`, `ui/Fire.h`, os
  emissores `UiContext::sdfWidget/sdfWidgetGlass/glassPanelFake/
  sdfFireBorder/fireGlowBelow/drawEmbers` e o `setGlassTexture`. Todos com
  ZERO chamadores no produto (o grep do commit prova). O relógio
  `sdf::g_time` sobrevive no loop (o consumidor novo é a textura de ruído
  em scroll da FASE 3 do E — o mesmo t = a mesma textura, o determinismo).
- **O `FontAtlas` NÃO FOI TOCADO** (cláusula do dono). A arquitetura de
  TICs NÃO FOI TOCADA (cláusula do dono). Zero shaders novos; zero
  matemática 3D nova.

### 1.2 Os pins reformados (a morte dos que aferiam o caminho morto)

O dono abortou o caminho; os pins que o aferravam saem COM ele (a
proveniência completa vive no git e nos §1..3 do RELATORIO-0.12-D.md — as
mutações M-D1a/b, M-D2a/b, M-D3a/b continuam coladas lá como história):

- **tests/test_012d_sdf.cpp APAGADO** (4 pins: sdf_widget_cantos/
  sdf_bisel/sdf_sombra/sdf_toggle_baixo — o batch sdf deixou de ter
  emissão; o arco por píxel volta a ser provado pela ESCADARIA no
  `regress_cantos_suavizados` restaurado).
- **tests/test_012d_vidro.cpp APAGADO** (4 pins: vidro_real_blur/
  vidro_falso_ok/vidro_fresnel/vidro_cache — o vidro real (captura+blur)
  e o falso (gradiente/reflexo/ruído) saem do produto).
- **tests/test_012d_fogo.cpp APAGADO** (5 pins: fogo_rampa/fogo_borda_
  anima/fogo_glow_below/brasas_portao/fogo_toggle_baixo — o fbm aditivo
  é substituído pela textura de ruído 2D da FASE 3 do E; a RAMPA
  fire0..fire3 fica no Theme para o LOGO de D-4).
- **O c33: FASE 32 (5 checks) e FASE 33 (4 checks) REFORMADAS** — o
  comentário no código documenta o aborto; o 1016 → 1007 checks (a
  FASE 34 do LOGO sobrevive intacta — 2 checks).

### 1.3 As recalibrações de volta (com nota no sítio)

- **play_play_bar_desenhada_com_stop_estado_e_aviso** (test_playui.cpp) —
  o quad do Stop volta a ser procurado no SOLIDS (a recalibração D-1 para
  o batch sdf revertida).
- **y1_gradiente_botao** (test_fase_y1_tema.cpp) — o teste inteiro volta à
  versão SOLIDS (a coluna central accent→accent2 exata + a continuidade
  pela cor intermédia); os includes do fb/rasterizador saem.
- **y3_cristal_hierarquia / y3_cristal_inspector** (test_fase_y3_cristal.cpp)
  — o topo do cristal volta a ser a LINHA 1dp do glow a 20% (a
  recalibração D-2 do gradiente do vidro falso revertida).
- **regress_cantos_suavizados** (test_sentinels.cpp) — a escadaria volta a
  ter MAIS geometria que o panel reto (a recalibração D-1 revertida). A
  D12 do LOGO (16 segmentos nos LODs) fica INTACTA — o D-4 sobrevive.
- **cameratic_criacao_e_menu_contextual** (test_cameratic.cpp) — a conta
  volta ao SOLIDS.
- **13.9 do c33** — o painel da hierarquia mede o MESMO composto
  surface@0.80 no topo E no fundo (o vidro liso voltou).

### 1.4 O estado da suíte

- test_core: **0 falhas** (o skip de disco conhecido — a sentinela R-039
  precisa de ~8 GB temporários e a sandbox local fica curta; o CI é o
  árbitro, sempre foi).
- c33_virtual: **1007 checks, 0 falhas** (1016 − 9 reformadas com a FASE
  32/33; a FASE 34 do logo intacta).
- Gates locais todos VERDES (docs-lint, theme-hex, ui-vocab, hierarchy,
  projects-ui, release-identity, glyph-source, reload-concurrency,
  gmesh-docs, jni-parity, link-parity 123 TUs, check_main).
- **versionCode 94 → 95** (o §14 do RELATORIO-0.9.6.md declara o par novo
  para o gate R-015; o C33 REAL — o PNG do editor restaurado — fica para
  o dono).

## 2 · E-2 — O TEMA FOGO AVERMELHADO (a paleta EXATA do dono)

### 2.1 A paleta (a ordem literal, aplicada ao Theme.h)

| token | ordem do dono | antes | estado |
|---|---|---|---|
| `bg` | #0A0A0C (obsidiana) | #0A0A0C | JÁ ERA EXATO — intacto |
| `surface` | #141416 (painéis) | #131316 | **TROCADO** (o α0.80 do vidro mantém até a FASE 3 o subir a 0.85) |
| `fireRed` | #D92B04 (brasa escura) | — | **TOKEN NOVO** |
| `fireOrange` | #FF5E00 (brasa quente) | — | **TOKEN NOVO** |
| `accent` (amber) | #FFB020 (destaque) | #FFB020 | JÁ ERA EXATO — intacto |
| `text1` | #F2F2F5 | #ECECEE | **TROCADO** (+ o `voniUser` segue o text1 — o contrato de sempre) |

- Os comentários do Theme.h documentam a proveniência (a ordem do dono, a
  paleta FOGO AVERMELHADO 0.12-E) e a tabela de hexes do cabeçalho
  (F2F2F5=(242,242,245) · 141416=(20,20,22) · D92B04=(217,43,4) ·
  FF5E00=(255,94,0)) fica no sítio de sempre.
- **Os NOVOS tokens ficam SEM consumidores na E-2** — o consumo é a FASE
  3 (o gradiente dos ativos fireRed→fireOrange). O Theme é a FONTE ÚNICA;
  a paleta entra primeiro, os efeitos 2.5D ligam-na em seguida (a ordem
  do dono tem as duas fases separadas).

### 2.2 Os gates aferram a fonte única (e funcionaram)

- Os **static_asserts do UiContext.h** (PANEL/TEXT) morreram COM O BUILD
  no 1.º build local — os aliases legados estavam desincronizados da
  tabela. Corrigidos no MESMO commit (a fonte única é REAL: PANEL
  #141416 α0.80, TEXT #F2F2F5).
- Os **2 pins de varredura de fonte** (y1_tokens e superficies_4_tons
  contam `VV_RGBA(20, 20, 22, 0.80f)` ÚNICO no Theme.h) recalibrados com
  nota — o contrato «a tabela tem UM só sítio» mantém-se.
- Recalibrações com nota: test_toolbar (surface[0]=20/255 + o
  voniUser==text1), test_sentinels (regress_identidade_grafite_ambar — o
  painel novo na identidade), test_fase_y1_tema (o token do painel),
  test_012c_idioma (superficies_4_tons).
- **theme_hex_check VERDE** — 0 hex fora do Theme (os 2 novos hexes
  nascem no ÚNICO sítio permitido).

### 2.3 O estado da suíte

- test_core: **0 falhas** (o skip de disco conhecido — a R-039).
- c33_virtual: **1007 checks, 0 falhas** (o 13.9 mede os compostos PELOS
  tokens — a paleta nova atravessa sem tocar no check).
- **versionCode 95 → 96** (o §14 do RELATORIO-0.9.6.md declara o par;
  o C33 REAL fica para o dono).

## 3 · E-3 — OS EFEITOS 2.5D (só desenho 2D)

### 3.1 As 4 receitas da ordem do dono (todas QUADS, zero shaders novos)

- **(3.1) O VIDRO**: o `surface` α0.80 → **α0.85** (o fundo escuro dos
  painéis) + a **LINHA 1dp do topo com o ÂMBAR a 20%** — o
  `glassTopLine` troca o RGB do glow (#FF3D00, o fóssil da Y-1) pelo
  `accent` #FFB020 ao mesmo 20% (o reflexo da luz). O preço: 0 (o emissor
  já existia; a cor vem do token da fonte única).
- **(3.2) A PROFUNDIDADE FALSA**: o **retângulo preto α0.15 deslocado
  3dp para baixo e para a direita por baixo de CADA botão** (o token
  `dropShadow` NOVO no Theme — preto 15%). O deslocamento é
  `theme::dp(3)` — o «3px do dono» à densidade de referência 1.0; a lei
  R-018 manda dp() (a sombra escala com o ecrã como todo o layout — o
  efeito é o MESMO em qualquer densidade). O CHOKE POINT
  `drawButtonVisual` emite-a em repouso E no ativo (o premido não perde
  a profundidade); o `iconButton` da barra segue o mesmo contrato.
- **(3.3) OS BOTÕES ATIVOS**: o **gradiente vertical 2D
  fireRed→fireOrange** (a paleta da E-2 finalmente consumida; o
  accent→accent2 da Y-1 sai do ativo — os tokens accent/accent2 vivem
  para os restantes usos). O glow de foco da Y-1 fica (o feedback de
  press é 2D puro e não foi revogado).
- **(3.4) O FOGO ANIMADO**: a **textura 2D de ruído em SCROLL LENTO (UV
  offset)** — `ui/Noise2D.h` NOVO, a FONTE ÚNICA (GL-free): value noise
  64×64 TILEÁVEL determinístico (o hash embrulha na CÉLULA 8×8 — o pin
  prova `pixel(x+64,y) == pixel(x,y)`; a emenda do GL_REPEAT fecha por
  construção), o `fireScrollOffset(t) = t·6px/s / 64` (o mesmo t = a
  mesma borda). O upload vive no `Renderer::init` (GL_RGBA, GL_REPEAT,
  POT 64); o emissor `UiContext::fireBorder2D` desenha as 4 BANDAS 2dp do
  perímetro no batch de imagens com o vColor = fireOrange (o FS da UI é
  `vColor·tex.r` — o ruído modula o brilho da brasa). O rasterizador
  honesto do stub HONRA o REPEAT agora: o `glTexParameteri` escreve o
  wrap na textura ligada (o fb) e os samplers fazem o fract antes do
  texel — o mesmo comportamento do driver. **ZERO shaders matemáticos**
  (a ordem do dono; o fbm da D-3 continua morto).

### 3.2 Os pins ×4 (tests/test_012e_efeitos.cpp) + as mutações (coladas)

1. **sombra_25d** — a COLUNA CENTRAL da escadaria da sombra (o span de Y
   completo do rect deslocado) com o preto α0.15, em repouso E no ativo +
   o token no Theme. **M-E3a** (a sombra removida do repouso):
   ```
   sombra_25d ... FALHOU test_012e_efeitos.cpp:89 sombra_25d: o retângulo
   preto 15% deslocado (+3,+3) não foi emitido por baixo do botão em
   repouso
   ```
   reposto → **0 teste(s) com falha**.
2. **vidro_85_topline_ambar** — o token α0.85 + a linha 1dp âmbar a 20%
   pelo emissor REAL (o mesmo que os painéis chamam).
3. **fogo2d_scroll** — o caminho REAL (Renderer+fb no rasterizador
   honesto): a banda do topo do botão premido vive no batch de imagens e
   o u0 dela é `fireScrollOffset(t)` EXATO (t=0 → 0; t=2 → 0.1875).
   **M-E3b** (o scroll morto — o offset congelado):
   ```
   fogo2d_scroll ... FALHOU test_012e_efeitos.cpp:208 fogo2d_scroll: o
   t=2.0 devia dar offset 0.1875 (leu 0.0000)
   FALHOU test_012e_efeitos.cpp:231 (o determinismo do offset)
   ```
   (2 testes vermelhos) reposto → **0 teste(s) com falha**.
4. **fogo2d_determinismo** — o tile perfeito ((x+64,y)==(x,y) numa
   varredura), o wrap do offset em [0,1), a volta completa (64/6 s) e o
   grão BRASA (os vizinhos 1px nunca saltam >64 níveis — o smoothstep).

### 3.3 As recalibrações (com nota no sítio)

- **y1_tokens / superficies_4_tons** — o `VV_RGBA(20, 20, 22, 0.85f)`
  único no Theme.h (a fonte única segue aferrada pelo countSub).
- **theme_tabela_spec_g_exata / regress_identidade_grafite_ambar** — o
  α do painel 0.85.
- **y1_primitivas / y3_cristal ×2** — a linha do topo é o ÂMBAR a 20%.
- **y1_gradiente_botao** — o gradiente do ativo é fireRed→fireOrange
  (topo 217/43/4 exato, fundo 255/94/0 exato; a continuidade pela faixa
  g 43..94 com r 217..255).
- **c33 13.9** — mede os compostos PELOS tokens (atravessou sem tocar).

### 3.4 O estado da suíte (o fecho da 0.12-E)

- test_core: **0 falhas** (o skip de disco conhecido — a R-039) — os 4
  pins novos verdes, as mutações M-E3a/M-E3b provadas.
- c33_virtual: **1010 checks, 0 falhas** — a FASE 35 NOVA (3 checks: a
  sombra no replay, a linha âmbar no replay, a textura de ruído viva).
- **versionCode 96 → 97** (o §14 do RELATORIO-0.9.6.md declara o par; o
  C33 REAL — os PNGs do editor/menus com o tema 2.5D — segue anexado à
  entrega).
- **A campanha 0.12-E FECHA** (E-1 restauração · E-2 paleta · E-3
  efeitos): a UI básica está de volta com o tema FOGO AVERMELHADO do
  dono, o texto é desenhado SEMPRE por cima (o FontAtlas intocado), a
  arquitetura de TICs intocada e ZERO matemática 3D no chrome.
