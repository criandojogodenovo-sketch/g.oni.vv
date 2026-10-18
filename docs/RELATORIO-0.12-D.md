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

## 3 · D-3 — O FOGO (a borda animada dos ativos, o glow por baixo, as brasas) (vc 93)

### 3.1 O que foi feito

- **`ui/Fire.h` NOVO — a FONTE ÚNICA da matemática do fogo** (GL-free,
  header-only, o padrão do ui/Sdf.h da D-1 com TRÊS consumidores: o FS do
  pass SDF replica linha a linha, o rasterizador honesto do stub INCLUI o
  header e corre as MESMAS funções, os pins chamam diretamente):
  `hash21`/`valueNoise`/`fbm` (o ruído de 2 oitavas — a 2.ª desloca no
  tempo: a chama «a correr» pela borda), `fireBorder` (a máscara da banda
  da aresta × o fbm animado — devolve a MÁSCARA e o fbm cru, o mix da cor
  usa-o; a DISTÂNCIA vem de `sdf::roundRectSdf`, a fonte da D-1 — zero
  duplicação) e `ember` (as brasas paramétricas: o mesmo t dá as MESMAS
  brasas — zero estado, zero alocação).
- **A RAMPA DO DONO no Theme** (R-030 — a cor vive SÓ na tabela):
  `fire0 #7A1F0A → fire1 #E8530F → fire2 #FFA21F → fire3 #FFE08A`. A
  proveniência da extração é o `scripts/d3_palette.py` NOVO (o PNG real
  docs/iconegonevv.png — o mesmo «G em chama» da Y-0; o núcleo quente
  lum≥90, R>B, 4 âncoras por quartil de luminância). A saída colada:
  ```
  amostra quente: 55054 pixels (lum ≥ 90, R > B)
  fire0 extraído #D04D03 · ordem do dono #7A1F0A · família
  fire1 extraído #E66F04 · ordem do dono #E8530F · família
  fire2 extraído #FC9800 · ordem do dono #FFA21F · família
  fire3 extraído #FECC62 · ordem do dono #FFE08A · família
  família quente (R ≥ G > B nas 4 âncoras): SIM
  ```
  (a extração mede a família quente; a VERDADE é a ordem do dono — o pin
  fogo_rampa aferra os 4 tokens aos hexes da ordem.)
- **A BORDA DE FOGO do elemento ativo** — `UiContext::sdfFireBorder`: o
  MESMO quad do widget em modo FOGO-ONLY (o `aShape.w=1` novo no
  SdfBatch::vertex) submetido em **MISTURA ADITIVA** (o par
  GL_SRC_ALPHA/GL_ONE — o run leva o flag `blendAdd` e o endFrame do
  Renderer liga-o à volta do drawArrays, reposto no fim). O FS ganha o
  ramo `vShape.w > 0.5` (o corpo transparente, a chama na banda) e os
  uniforms `uSdfTime`/`uFire1`/`uFire3`.
- **O GLOW POR BAIXO** — `UiContext::fireGlowBelow`: 2 quads α decrescente
  (o padrão honesto da casa, o mesmo do focusGlow da Y-1) a CAIR por baixo
  do rect (a reflexão da chama no «chão» do elemento).
- **AS BRASAS** — `UiContext::drawEmbers`: 16 partículas (o meio do
  intervalo 10-20 do prompt) emitidas como quads com a RAMPA do tema, com
  **O PORTÃO**: `celebration=false` emite ZERO — o editor NUNCA as vê. Os
  consumidores chegam com as suas campanhas (a splash é da D-6; o ecrã de
  projetos da D-8 — o Brand.h já documenta o mesmo padrão «quando
  existirem»).
- **A COMPOSIÇÃO SIMULTÂNEA (a cláusula anti-superficialidade)**: o premido
  do choke point `drawButtonVisual` mostra glow de foco + GLOW POR BAIXO +
  widget (o gradiente accent→accent2) + BORDA DE FOGO animada + bisel +
  especular + sombra NO MESMO frame; o chip ATIVO da barra de topo
  (iconButton) ganha o mesmo tratamento.
- **O relógio do fogo**: `sdf::g_time` (ui/Sdf.h) avança no main loop
  (`sdf::g_time += g_frameDt`) — a chama corre com o frame; os testes põem
  o t diretamente (determinismo).
- **O TOGGLE da D-7 morde os três emissores**: em `LEVEL_BAIXO` a borda
  NEMEMITE (a borda É o ruído), o glow morre e as brasas morrem — o ativo
  continua legível pelo fill accent→accent2 do widget (o que a D-1 deixou
  em todos os níveis).
- **O stub aprende o aditivo**: o `glBlendFunc` do gl3.h alimenta o
  `fb::setBlendFunc` (o par SRC_ALPHA/GL_ONE liga o aditivo) e o
  rasterizador soma `dst += out·outA` no ramo aditivo — o fogo no fb é a
  MESMA aritmética que o device faz.

### 3.2 Os pins ×5 (tests/test_012d_fogo.cpp — pelo caminho REAL)

1. **fogo_rampa** — os 4 tokens EXATOS da ordem do dono + a luminância
   estritamente crescente (a base escura → o bico amarelo) + a família
   quente (R>B nas 4).
2. **fogo_borda_anima** — 3 frames pelo caminho REAL (t=0, t=1.7, widget
   sós): a banda da aresta MUDA com o tempo (Δ>10 — o fbm corre), ACENDE
   sobre o widget sós (Δ>15 — a flama aditiva) e o centro morto é EXATO
   (Δ≤1 — a máscara morre a bevelW para dentro); um pixel da banda obedece
   à ARITMÉTICA ADITIVA lida = widget + flama·mask² (±0.04 — se o blend
   fosse o clássico, o destino multiplicaria por (1−α) e o lido ficaria
   ABAIXO por C·mask²).
3. **fogo_glow_below** — 12 vértices (2 quads), o pé QUENTE (r>b+20), o
   decaimento monotónico e NADA acima do rect.
4. **brasas_portao** — o PORTÃO: celebration=false emite ZERO (o editor
   chama e não nasce UMA); true emite 96 (16×6); o determinismo (o mesmo
   t = as mesmas brasas), a SUBIDA (y decresce), o tamanho 1.5..4px e o x
   dentro do rect com a folga do wobble.
5. **fogo_toggle_baixo** — em BAIXO: a borda emite 0 (Alto 6), as brasas 0
   (Alto 96), o glow 0 (Alto 12) — a cláusula D-7 aferrada nos três.

### 3.3 As mutações (coladas)

- **M-D3a** — o fbm MORTO (`return 0.5f;` na fonte única):
  ```
  fogo_borda_anima ... FALHOU test_012d_fogo.cpp:177 fogo_borda_anima:
  a banda da aresta é IGUAL entre t=0 e t=1.7 (Δmáx=0.0) — o fbm morreu,
  a chama não corre
  1 teste(s) com falha
  ```
  reposto → **0 teste(s) com falha** (o isolamento: só o anima morreu —
  o aritmética-aditiva e o resto da suíte ficaram verdes).
- **M-D3b** — o PORTÃO removido (drawEmbers ignora `celebration`):
  ```
  brasas_portao ... FALHOU test_012d_fogo.cpp:303 brasas_portao: o portão
  está ABERTO (o editor emitiu brasas) — a cláusula «nunca no editor»
  morreu
  FALHOU test_012d_fogo.cpp:309 brasas_portao: a celebração emitiu 192
  vértices, as 16 brasas são 96 — a contagem do prompt (10-20) morreu?
  2 teste(s) com falha
  ```
  reposto → **0 teste(s) com falha**.

### 3.4 Os achados reais (o «não confia no build» do dono)

1. **O `continue` que saltava o blend** — a 1.ª versão do ramo de fogo no
   rasterizador do stub terminava em `continue` («o fogo não passa pelo
   shade») — que salta TAMBÉM o bloco de escrita: **o fogo nunca chegava
   ao framebuffer**. O ramo é if/else agora (o pixel segue para o blend
   comum). Apanhado à mão na auditoria ao WIP (o pin ainda não existia).
2. **O `0x0304` no lugar de `0x0302`** — o `fb::setBlendFunc` comparava o
   src com `0x0304` (GL_DST_ALPHA!) comentado «GL_SRC_ALPHA»: o aditivo
   NUNCA ligava. GL_SRC_ALPHA é 0x0302.
3. **A banda SEM TETO para dentro** — a fórmula
   `edge = 1 − (d+1)/(bw+1)` só mata o lado de FORA; para dentro (d<−1) o
   edge CRESCIA sem teto — **o widget inteiro ardia** (o pin
   fogo_borda_anima leu Δ=73 no centro morto e o y1_gradiente_botao leu G
   saturado a 30%/70% da altura). A fórmula certa (replicada nos TRÊS
   lados): `edge = clamp01(min(1 − d, 1 + d/bw))` — 1 na aresta, morre a
   bevelW para dentro e a 1px para fora.
4. **A duplicação do SDF no stub** — o WIP replicava a distância inline
   com uma divergência real (o rr negativo caía no min(hw,hh) em vez de 0)
   — substituída pela chamada à FONTE ÚNICA `fireBorder` (o header que o
   FS replica).
5. **O «play em destaque» da C-2 é inalcançável no replay** — em Play o
   frame() retorna ANTES do top bar (a janela própria do Play): o chip do
   play com `st.playMode` nunca desenha. A FASE 33 usa o caminho que a
   cláusula descreve: o gesto REAL de premir o Stop da play bar (um
   ui.button verdadeiro) — a chama entra no batch. O achado fica
   registado para o dono decidir o destino do parâmetro (a C-2 é
   campanha fechada).

### 3.5 As recalibrações (com nota no sítio)

- **y1_gradiente_botao** (test_fase_y1_tema.cpp) — o premido emite AGORA
  TAMBÉM o quad da chama (o mesmo hw/hh do widget): a varredura distingue
  pelo `aShape.w` (o widget é o de fireOn<0.5) e ganhou a afirmação NOVA
  de que a chama carrega fire1→fire3 nos vértices. O gradiente
  accent→accent2 do widget continua EXATO (as EXPECTs originais intactas);
  as sondas de píxel voltaram ao verde com a banda certa (a queimadura do
  interior era o achado 3, não mudança de contrato).

### 3.6 O estado da suíte

- test_core: **0 falhas** (os 2 skips de disco conhecidos:
  sentinela_r039 e regress_streaming_paralelo — os artefactos /tmp/goni_tb*
  do texbind cheiam a sandbox; o CI é o árbitro).
- c33_virtual: **1014 checks, 0 falhas** — a FASE 33 NOVA (4 checks: a
  chama do premido no replay, as cores fire1→fire3 nos vértices, o PORTÃO
  no editor, o Baixo a matar a chama).
- **O pin C33 REAL fica para o dono**: o PNG do botão premido com a chama
  a correr pela aresta (os 3 modos de Efeitos).

## 4 · D-4 — O LOGO (o «G em chama» — a identidade única) (vc 94)

### 4.1 O que foi feito

- **`ui/Brand.h`/`Brand.cpp` reescritos — o «G EM CHAMA»**: a identidade
  extraída do ícone REAL (docs/iconegonevv.png — o mesmo PNG do launcher
  da Y-0 e a MESMA família da rampa fire0..fire3 da D-3):
  - a cor de cada traço é a RAMPA DO FOGO do Theme amostrada pela ALTURA
    do traço no viewBox (t: 0 na base = fire0 #7A1F0A, 1 no topo = fire3
    #FFE08A — o que o ícone real faz: a base escura, o bico amarelo); o
    âmbar MONO do glifo antigo MORREU;
  - **AS 4 SETAS MORRERAM** nos dois LODs (a ordem do dono: os glifos
    antigos das setas/mira fora — identidade única; o ícone real nunca as
    teve): `segmentCountFor` = 16 nos LODs todos (era 28/20);
  - **O `ignite` 0..1** — a fração da rampa revelada da BASE para o TOPO
    (0 = apagada no accentDim, nem emite; 1 = a chama toda): o parâmetro
    que a splash da D-6 anima; determinístico (o mesmo t = a mesma chama);
  - o LOD vive no TRAÇO (×1.2 no <32dp — o que a contagem de segmentos,
    agora igual nos dois, já não vê).
- **AS 3 VERSÕES (a ordem do dono)**: PEQUENA = o slot do LOGO na barra
  de topo (Toolbar.cpp — 24dp, ignite=1, o cabeçalho do editor); MÉDIA =
  o launcher (os PNGs da Y-0, gerados do mesmo desenho — intocados);
  GRANDE = a splash da D-6 (o `drawLogo(…, ignite)` já a serve).
- **O espelho Java alinhado (a identidade única no produto todo)**:
  `UiIcons.drawBrand` redesenhado — o MESMO G sem setas, o arco por
  SEGMENTOS com a cor da rampa por altura (o helper `fireColor(t)`), o
  `ignite` no parâmetro; os tokens `FIRE0..FIRE3` NOVOS no espelho do
  Theme (ProjectManagerActivity) e o gate projects_ui_check aceita os 4
  hexes (a edição do gate É o pedido); o BrandView leva o ignite (os 2
  chamadores passam 1f).
- **O ACHADO da auditoria (o overdraw da barra)**: a tabela antiga
  `kGBarPts` tinha 4 pontos — o loop desenhava 3 segmentos e o do MEIO
  RE-TRACEAVA a barra ao contrário (um quad desperdiçado por frame em
  TODA a história da marca; a contagem «2 segmentos» do header mentia —
  o Java já desenhava 2). A geometria certa: 3 pontos = 2 segmentos
  (14+2 = 16).

### 4.2 Os pins ×4 (tests/test_012d_logo.cpp)

1. **logo_rampa_viva** — cada quad do glifo leva a cor da rampa na SUA
   altura (a fórmula replicada no pin; divergência = vermelho); traços
   quentes no topo e escuros na base existem; a rampa exata por quad
   (±0.03) mata o mono por construção.
2. **logo_ignite_porta** — ignite=0 NÃO emite nada; ignite=0.35 divide o
   glifo (os traços com t<0.35 com a rampa, o resto accentDim); o mesmo t
   = o mesmo glifo (o determinismo da animação).
3. **logo_setas_mortas** — 16 segmentos nos DOIS LODs (a 24 e a 96dp) e
   NENHUM vértice além do G+capa (r ≤ 8.0 vb — os eixos das setas
   viviam a 8.2..11.0).
4. **logo_lod_traco** — a espessura medida nos quads: 24dp = 2.4 (×1.2),
   32dp = 2.667 (1.0) — o LOD do traço aferrado.

### 4.3 As mutações (coladas)

- **M-D4a** — a rampa MORTA (rampAt devolve o accent MONO — o fóssil):
  ```
  logo_rampa_viva ... FALHOU test_012d_logo.cpp:143 logo_rampa_viva: o
  quad a yVB=17.5 (t=0.27) leu (1.000,0.690,0.125), a rampa prevê
  (0.832,0.289,0.055) — a identidade não segue o ícone real
  ```
  (×16 quads, os irmãos verdes) reposto → **0 teste(s) com falha**.
- **M-D4b** — AS SETAS DE VOLTA (os 4 eixos desenhados outra vez):
  ```
  logo_setas_mortas ... FALHOU test_012d_logo.cpp:237 logo_setas_mortas:
  20 quads a size=24 — as setas (+12 segmentos no completo) parecem vivas
  FALHOU test_012d_logo.cpp:251 logo_setas_mortas: um vértice a r=8.29 vb
  (o G+capa termina a ~7.8; os eixos das setas começavam a 8.2) — o glifo
  antigo sobreviveu
  ```
  (2 testes vermelhos — o lod_traco também, o isolamento provado)
  reposto → **0 teste(s) com falha**.

### 4.4 As recalibrações (com nota no sítio)

- **D12 (8)** (test_sentinels.cpp) — a função do glifo: a contagem de
  segmentos é IGUAL nos dois LODs (16 — as setas morreram) e o LOD vive
  no traço; as EXPECTs «o completo tem mais segmentos» morreram com a
  ordem.

### 4.5 O estado da suíte

- test_core: **0 falhas** (os 2 skips de disco conhecidos).
- c33_virtual: **1016 checks, 0 falhas** — a FASE 34 NOVA (2 checks: os
  16 segmentos no rect do logo do cabeçalho; a CHAMA no cabeçalho — a
  base escura + o topo quente).
- **O pin C33 REAL fica para o dono**: o PNG do cabeçalho (o G em chama
  a 24dp) e do empty-state do projetos (96dp no Java).
