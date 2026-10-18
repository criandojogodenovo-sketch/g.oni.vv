# RELATÓRIO 0.12-D — IDENTIDADE VISUAL (fogo, vidro, 3D)

> A campanha do dono: shaders + renderer — o material de UI e o renderer do
> viewport ganham vida (o SDF único de widget, o vidro real e falso, o fogo
> nos ativos, o logo como identidade única, o viewport 3D com luz, o
> movimento, o orçamento de performance e o ecrã de projetos).
> Cláusulas do dono: anti-overengineering (cada efeito com toggle «Efeitos:
> Alto / Médio / Baixo») e anti-superficialidade (um botão ativo mostra
> bisel + brilho + fogo + sombra SIMULTANEAMENTE).
> PRÉ-CONDIÇÃO cumprida: 0.12-C FECHADA (vc 90, CI verde no 406871f).

## 1 · D-1 — O SHADER SDF ÚNICO DE WIDGET (vc 91)

### 1.1 O que foi feito

- **`ui/Sdf.h` NOVO — a FONTE ÚNICA da matemática** (GL-free, header-only):
  o SDF analítico do retângulo arredondado (`roundRectSdf`), a NORMAL
  derivada do gradiente da distância (`roundRectNormal` — a ordem do
  prompt) e o `shade()` — a função ÚNICA que compõe, por píxel: os CANTOS
  PERFEITOS (a cobertura é a própria distância, AA de 1px), o BISEL 3D
  (a banda junto à aresta acende segundo n·L — a luz do TOPO-ESQUERDA,
  `kLightX/kLightY = normalize(-0.55, -0.83)`: o topo fica ~+30% e o fundo
  ~−45%), o REALCE ESPECULAR no topo (banda de 5dp com `up²` — só a aresta
  superior), o BRILHO INTERNO nas bordas (anel exp a seguir ao bisel,
  mistura para branco quente) e a SOMBRA PROJETADA por baixo (o SDF do
  widget DESLOCADO 2.5dp, penumbra 3.5dp, α 0.35, composta STRAIGHT sobre
  o destino — o contato forte, o fade para fora).
- **O TOGGLE da cláusula anti-overengineering**: `sdf::g_level`
  (LEVEL_BAIXO/MEDIO/ALTO, default de fábrica ALTO) + `paramsFor()` — em
  BAIXO: só bisel + sombra SIMPLES (dy 1.5dp, soft 1.5dp, α 0.30 — a ordem
  expressa); MÉDIO: + especular (0.22); ALTO: + brilho interno (0.15) e
  especular cheio (0.35). A D-7 liga isto ao Settings.
- **`render/SdfBatch.h` NOVO** — o batch de WIDGETS SDF: UM widget = UM
  quad (6 vértices, 18 floats: aPos + aLocal(lx,ly,hw,hh) + aShape(raio,
  nível) + aColTop/aColBot — o gradiente vertical da Y-1 viaja nos
  vértices; o FS interpola de graça).
- **O 3.º PROGRAMA da engine** (render/Renderer.cpp): `progSdf_` com o VS
  (aLocal/aShape/aColTop/aColBot) e o FS **kSdfFsSrc — a RÉPLICA GLSL de
  `ui/Sdf.h::shade()` LINHA A LINHA** (documentada nos DOIS lados: GLSL
  não inclui C++; os pins provam o stub e o C33 do dono prova o device).
  Os parâmetros do nível são UNIFORMS por frame (uSdfBevel/uSdfSpecDepth/
  uSdfGlowW/uSdfShadow*/uSdfSpecOn/uSdfGlowOn/uSdfSpecAdd/uSdfGlowAdd/
  uSdfAA) lidos de `paramsFor(g_level)` NO END FRAME — o toggle vale no
  frame inteiro. O `endFrame` despacha as submissões pelo flag `sdf` do
  Run (o estado GL do pass UI mantém-se; os buffers/VAOs re-afirmam-se POR
  submissão — o bug do rebind, §1.4).
- **`UiContext::sdfWidget()` NOVA** — a emissão: UM quad crescido para
  CONTER a sombra (fundo: AA + offset + penumbra) e a AA à volta; as
  coords LOCAIS mantêm-se relativas ao topo-esquerda do widget (a sombra
  vive fora); o recorte é por interseção (o MESMO contrato do emitTo — o
  scroll recorta o widget inteiro); o audit regista o rect do WIDGET.
- **OS DRAWS ATUAIS SUBSTITUÍDOS** (a ordem do prompt): o CHOKE POINT
  `drawButtonVisual` (o `button()`/`buttonCompact()` da app inteira) e o
  `iconButton` da barra de topo (o play/pause/stop/gear) emitem pelo
  WIDGET SDF. O PREMIDO: glow de foco da Y-1 + o FOGO (accent→accent2) +
  bisel + especular + sombra SIMULTANEAMENTE (a cláusula
  anti-superficialidade). O REPOUSO: o vidro PANEL α0.80 com bisel+sombra.
  O `frameRounded` de 1dp dos botões MORREU (a aresta do bisel É o
  tratamento da borda agora); o `glassTopLine` do chip ativo MORREU (o
  especular É o brilho do topo). Os painéis/planos de vidro de sempre
  ficam INTACTOS (o vidro real é a D-2).
- **O rasterizador honesto do stub** (tests/stub/glstub_fb.h) aprendeu o
  3.º programa (a chave é o uniform `uSdfBevel` — resolvido ANTES do
  uProj, que o SDF também tem) e rasteriza o widget pela FONTE ÚNICA: o
  `glstub_fb.h` INCLUI `ui/Sdf.h` e corre o MESMO `shade()` por píxel —
  os pins aferem os MESMOS pixéis que o device desenha. As sondas
  `probeProgram()`/`probeVertex()` ficam (os pins do programa).

### 1.2 Os pins (test_012d_sdf.cpp — o caminho REAL: sdfWidget → endFrame →
rasterizador honesto → glReadPixels; o α do fb é inútil contra o clear
α1.0 do bg — as sondas leem o canal R contra o bg pelo blend do pass)

1. **sdf_widget_cantos** — os cantos PERFEITOS: a cobertura lida segue
   `clamp01(0.5 − d)` (interior/outside/AA da aresta), o pixel NO arco 45°
   do canto BR tem cobertura PARCIAL (o que a escadaria binária nunca deu)
   e a diagonal inteira do canto BR É a matemática do SDF: os 6 pixéis
   lidos são previstos pela FUNÇÃO ÚNICA `shade()` + o blend do pass
   (±0.06 de quantização 8-bit) — o pipeline inteiro obedece à distância.
   NOTA de método honesta: a sonda de cobertura usa o canto INFERIOR-
   DIREITO (o escuro — o brilho ADITIVO do especular satura a estimativa
   pelo canal R no topo-esquerda; o bisel/especular são provados pelo
   sdf_bisel).
2. **sdf_bisel** — a aresta de TOPO brilha sobre o interior (+30 de
   luminância: o bisel + o especular) e a aresta de FUNDO escurece (o
   volume — a luz vem de cima).
3. **sdf_sombra** — o pixel por baixo da aresta é mais escuro que o fundo
   (a sombra de contato) e 12dp abaixo o fundo está INTACTO (a penumbra
   não vaza).
4. **sdf_toggle_baixo** — o MESMO pixel da banda do especular comparado
   entre ALTO e BAIXO: em Alto brilha (+20 de R), em Baixo é o bisel puro;
   os parâmetros da fonte única (Alto: spec+glow; Baixo: nenhum dos dois,
   sombra seca).

### 1.3 As mutações (coladas)

- **M-D1a — o bisel removido** (`mul = 1.0` no shade):
  ```
  sdf_bisel ... FALHOU  test_012d_sdf.cpp:211
    sdf_bisel: a aresta de fundo (301) devia escurecer sob o interior (256)
  1 teste(s) com falha
  ```
  reposto → verde (o isolamento provado: os outros 3 pins ficam verdes —
  a detecção é do bisel, não dos vizinhos).
- **M-D1b — o toggle ignorado** (`paramsFor` devolve SEMPRE Alto):
  ```
  sdf_toggle_baixo ... FALHOU  test_012d_sdf.cpp:270
    !lo.spec && !lo.glow && lo.specAdd == 0.0f
  sdf_toggle_baixo ... FALHOU  test_012d_sdf.cpp:274
    sdf_toggle_baixo: o especular não aparece em Alto (r=143 vs Baixo r=143)
  sdf_toggle_baixo ... FALHOU  test_012d_sdf.cpp:278
    lo.shadowSoft < hi.shadowSoft && lo.shadowDy < hi.shadowDy
  3 EXPECTs vermelhas — o pin pega o toggle nos PARÂMETROS e no PÍXEL
  ```
  reposto → verde.

### 1.4 Os achados reais (o «não confia no build» do dono)

1. **O REBIND do ARRAY_BUFFER (bug REAL da D-1, apanhado pela suíte)**: no
   endFrame, após uma submissão SDF o `GL_ARRAY_BUFFER` corrente ficava no
   `vboSdf_` — o upload do UI seguinte (`glBufferData`) ia para o buffer
   ERRADO (o VAO do UI lia dados velhos). FIX: o bind do `vbo_`
   re-afirma-se POR submissão (o mesmo padrão do VAO).
2. **A CORRIDA FINA DOS JOBS DE IMPORT (bug PRÉVIO exposto, o mesmo padrão
   «não confia no build»)**: os dois loops de espera do c33
   (`12.8b`/`19.3`) tinham o teto de **3000 FRAMES** — no harness os
   frames custam ~1.7 µs e o teto expirava em **5 ms de relógio**, antes
   do worker terminar (medido: o loop 3001 frames = 5.0 ms; o
   `importFile` = 5.9 ms). A intenção do código («espera o CICLO COMPLETO
   do job») é tempo real. FIX: o teto é o RELÓGIO (10 s, contra hangs) +
   `sleep_for(1ms)` entre frames (o main cede o CPU ao worker). Os 23
   falhas em cascata (12.8b/12.9/26.5/27.2 — os jobs e os contadores das
   cenas que deles dependem) voltaram ao verde com ESTE fix.
3. **A sonda de α contra o clear**: o α do fb é inútil como sonda (o clear
   do bg escreve α=1.0) — os pins leem o CANAL R contra o bg (documentado
   no cabeçalho do pin; o método fica para as sub-fases seguintes).

### 1.5 A FASE 32 do device virtual (o replay REAL)

- **32.1** o replay emite widgets SDF (36 vértices = 6 widgets: os botões
  da barra de topo)
- **32.2** o raio do widget é o 8dp do card (spec A)
- **32.3** o nível ALTO viaja no vértice (o toggle Efeitos)
- **32.4** o vidro PANEL α0.80 no repouso (a cor do tema)
- **32.5** Efeitos: Baixo chega ao batch (o toggle é REAL — o `frame()`
  com `g_level = BAIXO` muda os vértices)

### 1.6 O estado da suíte

- test_core: **0 falhas** (com os 3 skips de disco conhecidos: r039/
  medicoes_010m/streaming_paralelo — os artefactos de bench de 963 MB por
  run encheram o disco da sandbox durante a D-1; limpos, os verdes
  repuseram-se; o CI é o árbitro).
- c33_virtual: **1010 checks, 0 falhas** (1005 + os 5 da FASE 32).
- **Os 4 pins do C33 REAL ficam para o dono**: o PNG do botão nos 3 modos
  (Alto/Médio/Baixo) — a prova de píxel do device.

### 1.7 Recalibrações com nota no sítio

- `y1_gradiente_botao` (test_fase_y1_tema.cpp) — o gradiente do botão
  premido deixou de ser a escadaria no solids: os VÉRTICES do batch sdf
  levam accent/accent2 EXATOS (aColTop/aColBot) e a CONTINUIDADE é
  provada POR PÍXEL no rasterizador honesto (o G a 30%/70% da altura
  desce dentro da faixa do degradê).
- `regress_cantos_suavizados` (test_sentinels.cpp) — o contrato de
  emissão do mundo SDF: ZERO escadaria no solids (≤ +2 quads) + o batch
  sdf com 6 vértices, raio 8dp e nível ALTO no vértice (o arco é provado
  por píxel pelos pins sdf_widget_cantos).
- `play_play_bar_desenhada...` (test_playui.cpp) — o Stop vive no batch
  sdf (a largura/centro lidos do próprio vértice hw).
- `cameratic_criacao_e_menu_contextual` (test_cameratic.cpp) — a conta de
  widgets inclui o batch sdf.

## 2 · D-2 — O VIDRO (REAL e FALSO) (vc 92)

### 2.1 O que foi feito

- **`ui/Glass.h` NOVO — as fontes PURAS partilhadas**: o BLUR DUAL KAWASE
  (`kawasePass` — o padrão de 8 offsets, k=1 na 1.ª passagem e k=2 na 2.ª,
  o "dual" do prompt; `kawaseBlurDual` com ping-pong), a DOWNSAMPLE 2×2
  (`downsample2x2` — duas reduções dão o 1/4 de resolução), a DECISÃO DA
  CACHE (`glassCacheDirty` — a câmara de orbit, a versão da cena, o rect
  capturado e a superfície; «só recalcular se o que está atrás mudou») e
  o RUÍDO FRACO do vidro falso (`glassNoise` — hash inteiro do SÍTIO, não
  do tempo: não cintila).
- **O VIDRO REAL (os painéis FLUTUANTES)**: `Renderer::captureGlass(x,y,w,h)`
  — o glReadPixels da região (y convertida para GL no sítio; o flip de
  linhas depois, para a textura ficar em ecrã y-down), a redução a 1/4,
  o blur dual e o upload GL_RGBA. A CACHE no renderer (`glassDirty_` +
  o contador `glassCaptures_`). O main (platform/main.cpp) decide com
  `glass::glassCacheDirty` (o estado da câmara/cena/rect) ANTES de
  qualquer widget (o fb ainda é a cena) e fornece a textura ao UiContext
  (`setGlassTexture`). O CONSUMO: **o MESMO widget SDF da D-1** com o
  ramo `uSdfTexOn` (o aShape.z) — o FS/stub amostra a cena borrada em
  espaço de ECRÃ sobre o rect capturado e multiplica pela TINTA ESCURA
  (`theme::kTheme.glassInk` NOVO no Theme — #6B6B76 α0.94; o produto e o
  mix do α são IDÊNTICOS nos dois lados). Os cantos, o bisel, o especular
  e a sombra do shader único continuam — o vidro não perde o tratamento.
  OS PAINÉIS FLUTUANTES TROCADOS: o menu IMPORT (kMenuW), o card do
  plusMenu e do assetMenu, e os 3 sheets ELEVADO (o fileMenu, o hierMenu
  e o scenesMenu).
- **O VIDRO FALSO (os painéis FIXOS)**: `UiContext::glassPanelFake()` —
  (1) o GRADIENTE semi-opaco surface→surface2 no corpo (o emitGradientV),
  (2) o REFLEXO DIAGONAL (3 veios brancos a α 0.05/0.03/0.02, 45°, no
  quadrante topo-esquerda — quads rotacionados clipados), (3) o RUÍDO
  FRACO (células de 16px, ~1 em 5 nasce, α 0.04, determinístico), (4) a
  BORDA CLARA 1px (o glassEdge — a FRESNEL). Aplicado à HIERARQUIA e ao
  INSPECTOR (o glassTopLine da Y-3 sai — a borda do vidro É o brilho do
  topo agora; os divisores âmbar da Y-3 ficam).
- **A FRESNEL**: a borda mais clara que o interior — o glassEdge do tema
  (branco α0.12) no vidro falso, o especular do widget no real.

### 2.2 Os pins (test_012d_vidro.cpp)

1. **vidro_real_blur** — (a) a função pura: o delta espalha (o centro
   desce, os vizinhos a 4px sobem, o canto longe intacto); (b) o CAMINHO
   REAL: a captura + o widget de vidro sobre um quadrado brilhante — o
   centro sob o vidro desceu e o vizinho fora do quadrado acendeu contra
   o fundo (o blur espalhou no píxel).
2. **vidro_falso_ok** — o corpo com o GRADIENTE exato (topo surface α0.80,
   fundo surface2 α0.86 no rect do painel) + o REFLEXO DIAGONAL no batch
   (quads brancos fracos não axis-aligned).
3. **vidro_fresnel** — a borda esquerda do painel falso lê mais clara que
   o interior a 8px.
4. **vidro_cache** — duas capturas sem invalidação = UMA passagem (o
   contador fica em 1, a textura é a mesma); a invalidação recomeça
   (contador 2); e a DECISÃO PURA (a câmara/a cena mudam → suja).

### 2.3 As mutações (coladas)

- **M-D2a — o blur removido** (o capture devolve a redução SEM o Kawase):
  ```
  vidro_real_blur ... FALHOU  test_012d_vidro.cpp:148
    vidro_real_blur: o vizinho fora do quadrado (35) não acendeu contra o
    fundo (32) — o blur não espalhou
  1 teste(s) com falha
  ```
- **M-D2b — a cache removida** (o capture recalcula sempre):
  ```
  vidro_cache ... FALHOU  test_012d_vidro.cpp:268
    vidro_cache: a captura sem invalidação re-blurou (2) — a cache morreu?
  vidro_cache ... FALHOU  test_012d_vidro.cpp:279
    vidro_cache: a captura após a invalidação não correu (3)
  2 EXPECTs vermelhas
  ```
  ambas repostas → verde.

### 2.4 Os achados reais

1. **A ORIENTAÇÃO da captura**: o glReadPixels devolve a origem NO FUNDO
   (GL) — sem o flip de linhas o painel mostrava a cena de cabeça para
   baixo. O y do chamador (ecrã y-down) converte-se no sítio único
   (captureGlass) e o flip segue — a textura fica na convenção do ecrã.
2. **O 13.9 e o grão**: as sondas de píxel do c33 passaram a esquivar o
   RUÍDO do vidro falso (3 sondas em x — o hash é esparso, ~1 em 5
   células; a tolerância honesta ±2).
3. **O teto da cache (documentado, não escondido)**: com o overlay aberto
   o orbit está MORTE (o gate anyOverlayOpen) — a captura do OPEN é a que
   fica; a edição pelo INSPECTOR com um menu aberto só refresca no
   próximo evento da cache (a câmara/o rect/a contagem de TICs). O caso
   comum (a órbita → abrir o menu) está sempre certo.

### 2.5 O estado da suíte

- test_core: **0 falhas** (os 4 skips de disco conhecidos).
- c33_virtual: **1010 checks, 0 falhas** — com 3 recalibrações com nota
  (13.9 ×2 — o gradiente do vidro falso nos DOIS lados com as sondas que
  esquivam o grão; 18.1 J-02 — o sheet mede O VIDRO REAL: escuro e nunca
  o véu cru; y3 ×2 e o uitree no test_core — o mesmo contrato).
- **O pin C33 REAL fica para o dono**: o PNG do menu com a cena por trás.
