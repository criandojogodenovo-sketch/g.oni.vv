# RELATORIO-0.11-Y-Y1.md — o TEMA (o poço + a chama no ecrã)

Campanha: 0.11-Y (identidade visual) · Sub-fase: Y-1 · vc 78 → 79 ·
Data: 2026-10-17 · Commit: este

## §1 O que foi feito

- **A TABELA recalibrada** (app/src/main/cpp/ui/Theme.h — a fonte única,
  a regra R-030): bg #0E0E10 → **#08080A** (O POÇO — o MESMO hex do
  launcher da Y-0: a app inteira acordou no fundo do «G em chama»); a
  superfície de vidro desceu ~5 % com o MESMO alfa da decisão do dono —
  surface #161618 → **#151517** α0.80 e surface2 #202023 → **#1E1E21**
  α0.86; a hairline de grafite morreu e nasceu a aresta quente do
  cristal: border #2E2E32 α0.55 → **#4A3714 α0.60** — o RGB É o do
  accentDim (o sub-tom âmbar a 25 % sobre grafite, agora na borda toda);
  accent **#FFB020**, accentDim, accentPress, accentInk, danger, warn,
  ok INTACTOS (o âmbar continua a identidade; o laranja do warn segue
  nunca-igual-accent); OS DOIS TOKENS NOVOS: **accent2 #FF6B00** (O
  FOGO — a base do gradiente dos botões ativos) e **glow #FF3D00
  α0.25** (o brilho da chama — o foco composto e a linha do topo do
  vidro).
- **O CLEAR LÊ O TOKEN** (o contrato R-028, intocado): o
  glClearColor do Renderer lê kTheme.bg — o céu da viewport 3D escurece
  com o poço POR CONTRATO, sem tocar em render 3D (zero linhas de
  render mudadas; a 13.9 do device virtual confirma o clear no pixel).
- **O BOTÃO ATIVO É A CHAMA** (o CHOKE POINT button(), as duas
  passagens audit/não-audit partilham o drawButtonVisual): o estado
  held desenha o **glow de foco ATRÁS** (2 quads com alpha DECRESCENTE
  para fora — o exterior 8 dp a glow.a×0.4 = 0.10, o interior 4 dp a
  glow.a×0.8 = 0.20; a composição dos dois é a simulação honesta do
  blur, a lei da casa: quads, não shaders) + o **fill com o GRADIENTE
  VERTICAL accent→accent2**; o repouso continua o vidro PANEL de
  sempre; a tinta do texto no ativo é o BG (≥7,0:1 até no accent2, o
  pior caso do degradê).
- **O GRADIENTE por vértice** (render/QuadBatch.h + UiContext):
  `quadGradientV` emite o quad com as cores de topo/fundo nos vértices —
  o varying vColor do shader do pass UI interpola DE GRAÇA (zero
  mudanças de shader, zero custo extra). `panelRoundedGradientV` usa a
  MESMA escadaria de 4 degraus/canto do panelRounded com cada peça
  AMOSTRADA do span completo do rect (`emitGradientV` — interpolação
  linear no span; amostrar nas posições clipadas == o gradiente
  verdadeiro sob o recorte) — o degradê é contínuo através das peças,
  sem costuras. O z-order (recordRun) segue o padrão de sempre.
- **A LINHA DO TOPO DO VIDRO** (`glassTopLine`): 1 dp de altura com o
  RGB do glow a 20 % — o brilho do cristal. Os primitivos de sempre
  (panelRounded/frameRounded) ficam INTACTOS: a linha é desenhada a
  seguir a eles, pelo chamador. (A aplicação aos painéis de cristal é
  a Y-3 — os cartões; a primitiva nasce aqui e é pinada.)
- **Os ALIASES** (UiContext.h): BG/PANEL/LINE sincronizados com a
  tabela nova + **ACCENT2/GLOW NOVOS** com static_assert (a fonte
  única aferrada em compile time).
- **A auditoria de contraste recalibrada HONESTAMENTE** (os comentários
  do Theme.h + os pins): sobre a surface nova text1 15,4:1 · text2
  7,5:1 · accent 10,0:1 · danger 4,7 · warn 7,8 · ok 6,0; sobre o vidro
  real (α0.80 sobre o poço) text1 15,6 · accent 10,1; no patológico
  (branco puro por trás) text1 8,3 · accent 5,3; a paleta V.ONI no
  poço 5,8 (comment) .. 16,9 (user); a tinta do botão ativo BG sobre
  accent2 7,0:1 — todo o texto ≥4,5:1, todo o componente ≥3:1 (os
  pisos da casa intactos — o poço só GANHA contraste).

## §2 Provas

- **Pins novos: 5/5 VERDE** local (g++ test_main + test_fase_y1_tema no
  test_core, o binário FRESCO — a lição do ciclo stash/pop: o binário
  STALE enganava, a reconstrução forçada re-compila o TU):

  y1_tokens (o kTheme compilado + a FONTE Theme.h com os hex exatos ×1
  e os velhos MORTOS) · y1_aliases (BG/PANEL/LINE/ACCENT2/GLOW
  bit-a-bit) · y1_contraste (os pisos nos valores novos, incluindo a
  tinta do ativo sobre o accent2) · y1_primitivas (a glassTopLine: 1
  quad, 1 dp, glow RGB α0.20, no topo dado; o focusGlow: 2 quads
  α 0.10 exterior → 0.20 interior, exp 8/4 dp — α DECRESCENTE para
  fora) · y1_gradiente_botao (o held: a coluna central do fill em span
  completo com topo == accent exato e fundo == accent2 exato + vértice
  INTERMÉDIO no batch — o degradê contínuo; o repouso: ZERO accent2).
- **M-Y1a — accent2 → #FF0000** (a fonte Theme.h na sandbox
  GONI_ROOT_OVERRIDE):

  ```
  y1_tokens  ...  FALHOU  tests/test_fase_y1_tema.cpp:131
    y1_tokens: o bg VV_RGB(8,8,10) tem de aparecer EXATAMENTE 1x
    (encontrado 1)  ← o accent2 é o que falta: o count de
    VV_RGB(255, 107, 0) cai a 0 e o EXPECT do hex novo morre
  1 teste(s) com falha
  ```
  Reposto: «0 teste(s) com falha». ✓
- **M-Y1b — o bg de volta ao velho** (VV_RGB(14, 14, 16) na sandbox):

  ```
  y1_tokens  FALHOU (o kTheme compilado — 8/255 ≠ 14/255)
  y1_tokens  FALHOU (a fonte — VV_RGB(8, 8, 10) tem de aparecer 1x,
             encontrado 0; VV_RGB(14, 14, 16) tem de aparecer 1x —
             só o accentInk — encontrado 2)
  2 teste(s) com falha
  ```
  Reposto: «0 teste(s) com falha». ✓ (o pin apanha a tabela E a fonte)
- **M-Y1c — a border α0.60 → 0.55** (o velho alpha de volta):

  ```
  y1_tokens  ...  FALHOU  tests/test_fase_y1_tema.cpp:129
    y1_tokens: o bg VV_RGB(8,8,10) ... (o EXPECT do VV_RGBA(74, 55, 20,
    0.60f) == 1 morre: encontrado 0)
  1 teste(s) com falha
  ```
  Reposto: «0 teste(s) com falha». ✓
- **M-Y1d — o botão deixa de ser a chama** (MUTAÇÃO DE CÓDIGO
  compilado: o drawButtonVisual do held volta ao panelRounded(ACCENT)
  plano — o visual pré-Y-1; build incremental, corrida, REVERSO):

  ```
  y1_gradiente_botao  ...  FALHOU  tests/test_fase_y1_tema.cpp:281
    nearColor2(v[k].r, v[k].g, v[k].b, v[k].a, 1.0f, 107.0f / 255.0f,
               0.0f, 1.0f)          ← o FUNDO do fill não é o accent2
  FALHOU  tests/test_fase_y1_tema.cpp:281  (2.º vértice do fundo)
  FALHOU  tests/test_fase_y1_tema.cpp:303
    y1_gradiente: nenhum vértice INTERMÉDIO do degradê (o gradiente
    não é contínuo através das peças)
  y1_gradiente_botao  FALHOU   (4 EXPECTs)
  ```
  NOTA do isolamento: o check do TOPO (linha 277, accent) PASSOU na
  mutação — o fill plano accent tem o topo certo; o que morre é o
  FUNDO (accent2) e o INTERMÉDIO (o degradê). O y1_primitivas aguentou
  VERDE na mesma corrida (a primitiva é testada DIRETO, não pelo
  botão). Reposto → «0 teste(s) com falha». ✓
- **Recalibrações legítimas** (a mudança de identidade é do dono; os
  testes recalibrados com nota no sítio, o padrão RECALIBRADO):
  regress_identidade_grafite_ambar (R-028: a border SAI da rampa
  neutra — É o âmbar do accentDim por contrato, com o check novo; os
  exatos bg/surface novos; o accent2 MAIS quente que o accent com a
  tinta BG ≥4,5:1 sobre ele; o glow exato), theme_tabela_spec_g_exata
  (os exatos novos + os 2 tokens novos + a border α0.60),
  hierQuadsAmbar (test_uieditor: o discriminador do FILL da seleção é
  o alfa 1.0 — a border nova tem o MESMO RGB com α0.60 e contava como
  fill; recalibrado com nota), os compostos do c33 (13.1/13.9/18.4
  recalculam via blendOver do token SOZINHOS; o literal do fundo do
  18.4 14,14,16 → 8,8,10 e o «sólida 22» do 13.9 → 21 com nota), o
  clear dos impostores (test_fase375b_impostors: os EXPECTs lêem o
  TOKEN bg em vez do literal 0x14 — o mesmo contrato do 13.9).
- **Suíte local**: os grupos y1/theme/toolbar/uieditor_selecao/
  impostor/logs VERDES no binário fresco; o CI é o árbitro (o run
  deste commit). NOTA de ambiente (não-código): a sandbox local sofreu
  o reset — a primeira varredura plena correu com o disco CHEIO (os
  fixtures de ~3 GB do r039/r046 deixados pelo crash da corrida
  latente conhecida §4.1/0.11-T); com o disco livre TUDO verde exceto
  o crash intermitente da regress_streaming_paralelo (a corrida
  latente SUSPENSA pelo dono — STATE.md §7; o A/B com git stash provou
  que o crash existe no HEAD SEM as mudanças da Y-1: ambiente, não
  regressão desta sub-fase).
- **versionCode: 78 → 79** (app/build.gradle:29, declarado no §14 do
  RELATORIO-0.9.6.md para o gate R-015).

## §3 Os limites / não-verificado

1. **O céu da viewport 3D escurece** — consequência DO CONTRATO «o
   clear lê o token» (R-028): o bg novo é o poço do launcher e o
   clear acompanha. Nenhuma linha de render 3D mudou; a 13.9 do
   device virtual valida o clear no pixel.
2. **A tela de projetos (Java, ProjectManagerActivity) mantém o
   espelho de tokens dela** — o prompt da Y-1 fala da tabela C++
   (ui/Theme.h); o espelho Java é OUTRO ficheiro (a tela de projetos
   não desenha em C++). A harmonização Java NÃO está no prompt desta
   sub-fase — se o dono quiser o poço lá também, é um passo próprio.
3. **Os ThumbRender/impostores têm bg PRÓPRIO (0x14)** — o fundo das
   miniaturas é um ASSET de preview, não chrome; o prompt da Y-1 não
   o menciona. Intocado (cláusula calma).
4. **A banda da toolbar do 13.1** (o check «povoada» contra o literal
   20,20,20) fica trivialmente verde desde a 0.9.6.10 (o bg deixou de
   ser (20,20,20) naquela recalibração) — dívida de calibração
   ANTERIOR à campanha, fora do âmbito da Y-1; o check irmão «o TEXTO
   desenha» (glifos >200) é o que afere o conteúdo de verdade.
5. **O globo do glow é DECORATIVO** — o glow token tem α0.25 e os
   derived 0.10/0.20 nunca passam por cima de texto (o glow é ATRÁS do
   elemento; a glassTopLine é 1 dp no topo). Nenhum piso de contraste
   depende dele.

## §4 Ficheiros tocados

- app/src/main/cpp/ui/Theme.h — a tabela recalibrada + accent2/glow +
  os comentários de contraste recalibrados
- app/src/main/cpp/ui/UiContext.h — aliases + ACCENT2/GLOW +
  static_asserts + as 3 primitivas novas + emitGradientV privado
- app/src/main/cpp/ui/UiContext.cpp — as implementações +
  drawButtonVisual (o choke point do botão nas 2 passagens)
- app/src/main/cpp/render/QuadBatch.h — quadGradientV (6 vértices,
  cores top/bottom por vértice)
- tests/test_fase_y1_tema.cpp — NOVO TU (os 5 pins)
- tests/CMakeLists.txt — o TU na suíte test_core
- tests/test_toolbar.cpp — a tabela exata recalibrada + os 2 tokens
- tests/test_sentinels.cpp — a R-028 recalibrada (a border âmbar fora
  da rampa neutra + o accent2/glow)
- tests/test_uieditor.cpp — o hierQuadsAmbar com o discriminador alfa
- tests/c33_virtual.cpp — os 2 literais recalibrados (8,8,10 / != 21)
- tests/test_fase375b_impostors.cpp — o clear lê o token
- app/build.gradle — versionCode 78 → 79
- docs/RELATORIO-0.9.6.md — a entrada Y-1 no §14 (o par 0.9.6/79)
- docs/REGRESSOES.md — a nota de recalibração da R-028
- STATE.md — o §1 (Y-1 FECHADA), §2 (+linha), §8 (+relatório), vc 79
- CODEMAP.md — o TU novo + as primitivas
- docs/RELATORIO-0.11-Y-Y1.md — este relatório

## §5 Pin C33 (o dono preenche)

- [ ] Instalar o APK **vc 79** (o artifact goni-vv-0.9.6-release-signed
      do run verde deste commit).
- [ ] Ver o TEMA: o poço #08080A em tudo (o céu da viewport incluído),
      a hairline âmbar nos cartões, o botão PREMIDO com o glow + o
      degradê âmbar→fogo.
- [ ] PNG do editor com um botão premido → docs/y1-c33.png.
