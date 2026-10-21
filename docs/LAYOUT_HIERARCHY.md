# docs/LAYOUT_HIERARCHY.md — O CONTRATO DA HIERARQUIA (P-08)

> Como este documento é mantido: a árvore aqui é a FONTE OFICIAL da
> hierarquia do editor. Qualquer região nova entra PRIMEIRO aqui (PR +
> gate `hierarchy-check` no CI) e só depois no código. O gate compara a
> tabela «Região → fonte no código» contra os símbolos reais; a sentinela
> `regress_hierarquia_contrato` (R-022, tests/test_sentinels.cpp) afere a
> geometria (containment, pisos, z-order) em múltiplos ecrãs/densidades.
> Alterar um pai, um piso ou a ordem SEM atualizar este ficheiro = CI
> vermelho.

## 0. A VERDADE DA PLATAFORMA (lida antes de qualquer "view")

O editor NÃO TEM árvore de views Java. A `VvActivity`
(app/src/main/java/vv/goni/VvActivity.java) é uma `NativeActivity` com
UMA ÚNICA superfície EGL — zero `View`s, zero layouts XML, zero
`GLSurfaceView`. Todo o editor é UI immediate-mode desenhada em C++
(cpp/ui) sobre essa superfície, e o passe 3D renderiza ANTES da UI num
retângulo controlado por `glViewport`+`glScissor`.

As «views» do contrato são portanto as REGIÕES do layout C++ (rects
puros, matemática host-testável em ui/SafeArea.h). Os nomes que a casa
usou noutros contextos (RootLayout, TopNavigationBar, ViewportContainer,
TransformToolbar, ViewportRail, BottomDock, BottomBar) mapeiam assim:

| Nome conceptual | Região real | Fonte no código (símbolo que manda) |
|---|---|---|
| RootLayout | o contentRect (superfície − insets) | `safe::contentRect` em `ui/SafeArea.h` |
| TopNavigationBar | TOP BAR 40dp (FASE UI B) | `safe::toolbarRect` em `ui/SafeArea.h`; desenhada por `toolbar::draw` em `ui/Toolbar.cpp` |
| ViewportContainerLayout | viewportRect (entre a top bar e a tab bar) | `safe::viewportRect` em `ui/SafeArea.h` |
| GLSurfaceView | o passe 3D (glViewport+glScissor no centerRect) | o bloco `scissor3d` em `platform/main.cpp` |
| TransformToolbar | REMOVIDA no PASSO 3 (0.9.6.17): a toolbar inferior HORIZONTAL saiu — as ferramentas vivem no RAIL ESQUERDO vertical (a spec do dono: nada full-width sobre a cena; o duplicar/colar saíram da viewport e vivem no menu ⋯ itens 9/10) | `vpchrome::layout` em `ui/ViewportChrome.cpp` |
| ViewportRail | o RAIL ESQUERDO de ferramentas (Selecionar/Mover/Rodar/Escalar/Íman, col-major) + a FILA DO TOPO (undo/redo/save/⋯) + o GIZMO 40dp do topo-direito + a legenda — PASSO 3 (0.9.6.19b · D21: o [+] do fundo-direito foi REMOVIDO — o canto fica limpo), TUDO a 60% de alfa (`vpchrome::kChromeAlpha`) | `vpchrome::layout` — `L.rail`/`L.quick`/`L.gizmoBtn` em `ui/ViewportChrome.cpp` |
| BottomDockLayout | o drawer de baixo + tab bar 32dp (Ficheiros/Assets/Consola/Animação + «FPS · TICs» à direita — PASSO 1). PASSO 2: o drawer NUNCA abre sozinho (o auto-abrir da 0.8.0 saiu) e a altura efetiva é capada a 35% da altura do content | `bottom::layout` em `ui/BottomPanel.cpp` |
| BottomBar | REMOVIDA no PASSO 1 (0.9.6.14): a faixa extra de 24dp saiu — o «FPS · TICs» vive no canto direito da TAB BAR (`bottom::layout`); a versão/commit em Settings › Sobre. `safe::statusRect` devolve uma faixa de ALTURA ZERO (compat de testes; nada desenha nela) | `safe::statusRect` em `ui/SafeArea.h` (LEGACY, altura 0) |

## 1. A ÁRVORE (pai → filhos; a ordem de DESENHO é a ordem de z)

```
contentRect (superfície EGL − insets do sistema)
├── PASS 3D (a "cena") — glViewport/glScissor = centerRect; aspect do rect
│   └── (a câmara projeta com o aspect DO RECT, nunca do ecrã todo)
├── TOP BAR (40dp — FASE UI B, faixa de topo)        [toolbar::draw]
├── VIEWPORT REGION (safe::viewportRect)             [pai lógico]
│   ├── STRIP do topo — REMOVIDA no PASSO 3 (a barra full-width é
│   │   PROIBIDA pela spec do dono; os chips [Cena][Perspetiva][Global]
│   │   eram SEM FUNÇÃO desde o inventário do PASSO 0)
│   ├── RAIL ESQUERDO (PASSO 3: as 5 ferramentas Selecionar/Mover/Rodar/
│   │   Escalar/Íman em coluna(s) + pai de vidro a 60% — a fila do TOPO
│   │   undo/redo/save/⋯ vive ao lado (ou abaixo, na degradação))
│   │                                                 [vpchrome::layout]
│   ├── GIZMO 40dp (canto SUPERIOR direito — o atalho mostrar/esconder o
│   │   gizmo: a transição selectMode↔gizmo que já existia) [vpchrome]
│   ├── LEGENDA (o nome da ferramenta ativa, à direita do rail; some na
│   │   degradação)                                   [vpchrome::layout]
│   └── [+] Adicionar TIC — REMOVIDO no 0.9.6.19b (D21, decisão do
│       dono): era 40dp REDONDO no canto inferior direito e morreu
│       (redundante com o + da hierarquia e o «Novo objeto» do menu ⋯;
│       interceptava toques de orbit/seleção). O id 38 fica APOSENTADO
│       (`vpchrome::kVpAddTicIdRetired`) — o PIN: NENHUM widget com ele
│       desenha dentro do rect do viewport, em estado nenhum
│       (FASE 17.1 do device; a mutação M-D21 repõe-no → vermelho)
├── PAINEL HIERARQUIA (esquerda; PASSO 2: 18% da largura, piso 140dp;
│   pesquisa com placeholder degradado + árvore)   [drawHierarchy]
├── PAINEL INSPECTOR (direita; PASSO 2: 22% da largura, piso 180dp /
│   teto 260dp — SEM seleção colapsa ao TRILHO de 32dp, a área junta-se
│   ao viewport e volta ao selecionar; P2-bis: o PIN fixa o painel
│   aberto MESMO sem seleção — persiste no layout.json)
│                                                   [drawInspector]
│   ├── TRILHO (32dp; P2-bis: o ícone no topo É o toggle do PIN — a
│   │   célula de toque 32×40dp, id 7432: tocar ABRE E FIXA; a fonte
│   │   única do colapso é editor::inspectorCollapsed)
│   └── CABEÇALHO 28dp UMA LINHA (0.9.6.18 · D1+D5): título à esquerda +
│       o recolher do pin à DIREITA (célula 40×28dp, id 7433, só enquanto
│       fixado). As TABS [Inspector][Nós] MORRERAM (D5: a vista «Nós» era
│       duplicado da hierarquia — nenhum botão partilha a faixa do título)
├── BOTTOM DOCK: drawer (altura variável 0..cap duplo) + TAB BAR (32dp —
│   PASSO 1)
│   └── tabs Ficheiros · Assets · Consola (chips internas Logs/Erros/
│       Avisos 28dp — a chip «Consola» duplicada saiu no PASSO 2;
│       P2-bis: a LISTA de log ocupa ≥60% do conteúdo do drawer — o
│       campo de comando SAI quando a lista com ele fica <60%, a fonte
│       única é bottom::conCmdVisible) +
│       «FPS · TICs» à direita                         [bottom::draw]
├── (a STATUS BAR de 24dp foi REMOVIDA no PASSO 1 — a tab bar é a última
│   faixa viva; safe::statusRect devolve altura ZERO por compat)
└── OVERLAYS (por cima de TUDO): menus, Settings, browser, teclado,
    modais — a camada é: cena < painéis < dock < modais < teclado
```

## 2. AS REGRAS DO CONTRATO (o que a sentinela R-022 e o gate afere)

1. **A TAB BAR é intocável (PASSO 1).** A faixa de 32dp no fundo
   (`safe::bottomTabRect`) é a ÚLTIMA faixa viva do contentRect — nem o
   drawer, nem o viewport, nem overlays não-modais a cobrem. (A status
   bar de 24dp que ocupava esse papel foi REMOVIDA no PASSO 1;
   `safe::statusRect` devolve uma faixa de altura ZERO, por compat.)
2. **O chrome do viewport vive DENTRO da viewport region (PASSO 3)**:
   o rail, a fila do topo e os cantos derivam do rect POR CONSTRUÇÃO —
   nunca sobem à top bar, nunca saem do rect, e NADA atravessa a largura
   (nenhum pai de vidro ≥90% da largura — a strip morreu). A fila do topo
   desce para baixo do rail quando não cabe ao lado do gizmo; o que não
   cabe, ESCONDE (a degradação honesta). TUDO a 60% de alfa
   (`vpchrome::kChromeAlpha`, o multiplicador é público e pinado na
   R-034).
3. **O drawer come o viewport por baixo, com CAP DUPLO**: a altura
   efetiva do drawer é `safe::effectiveDrawerH` — UMA fonte usada tanto
   pelo draw do drawer (bottom::layout) como pelos rects do centro
   (main.cpp `currentDrawerH`). O cap é o MENOR de: o piso do viewport
   central (`safe::kViewportMinH`) e o teto de 35% da altura do content
   (`safe::kDrawerMaxPct` — spec PASSO 2). O viewport central com o
   drawer aberto nunca fica abaixo de `safe::kViewportMinH` (strip +
   toolbar cabem sem sobreposição) e o drawer nunca passa 35% da altura.
   O drawer é SEMPRE fechado no arranque (bottomTab=0; o auto-abrir da
   0.8.0 saiu no PASSO 2).
4. **As larguras dos painéis são PERCENTAGEM com pisos (PASSO 2)**: a
   hierarquia 18% (piso `kHierMinW`=140dp), o inspector 22% (piso
   `kInspMinW`=180dp, teto `kInspMaxW`=260dp) — o viewport central fica
   ≥55% nos ecrãs de referência (critério a do dono). SEM seleção o
   inspector É o TRILHO `kInspTrackW`=32dp (a MESMA pergunta —
   `editor::inspectorCollapsed` — alimenta os rects `safe::`, o draw, o
   divisor direito — que desaparece — e o centerRect). O drag dos
   divisores clampa o inspector a [180..260] e mantém o piso da toolbar
   (`kViewportMinW`=288dp). **P2-bis — o PIN**: o ícone do trilho (id
   `kInspTrackPinId` 7432, célula 32×40dp) ABRE e FIXA o painel
   (`inspPinned`, persiste no layout.json); a seta de recolher (id
   `kInspUnpinId` 7433) DESFAZ o pin — 0.9.6.18 (D1): a seta vive na
   LINHA 1 do cabeçalho, à direita do título (célula 40×28dp,
   `kInspUnpinCellW`) — fixado, o painel fica aberto MESMO sem seleção;
   com seleção a regra PASSO 2 manda (o painel volta a fechar ao limpar
   a seleção).
5. **As PEGAS de arrasto medem 24dp (PASSO 2)**: a pega do drawer
   (`kDrawerHandleH`, era 12) e o hit dos divisores (`kDividerHitW`, era
   20). O strip VISÍVEL dos divisores segue 12dp — a folga vai PARA
   DENTRO do painel, nunca para o viewport.
6. **Um rect de UI nunca sai do contentRect** (`safe::rectInside`,
   validador do harness — FASE 13) nem do pai imediato documentado aqui.
7. **A strip nunca corta texto**: os chips medem o texto (wrap-content
   com piso) e degradam por ordem — Perspetiva esconde primeiro, depois
   Cena; o Global é o ÚLTIMO a ceder e tem fallback de reticência
   honesta (nunca «Glob+» por cobertura de outro widget). O MESMO
   padrão de degradação alimenta o placeholder da pesquisa («pesquisar
   TIC» → «pesquisar») e o convite da hierarquia vazia (PASSO 2).
8. **O passe 3D segue o rect**: glViewport/glScissor/aspect vêm do MESMO
   centerRect que o input e o chrome consomem; qualquer mudança do rect
   LOGA `vp3d: viewport set to (x, y, w x h)` (R-024).
9. **Todo o texto usa sp** (`theme::fontScale`) e todas as medidas dp
   (`theme::dp`) — px cru é violação (R-018).
10. **A CONSOLA cumpre o piso de 60% (P2-bis)**: com a tab Consola
   ativa, a lista de log ocupa ≥60% da altura de CONTEÚDO do drawer
   (chips+extras ≤40% — a regra do dono, SEM exceções). O extra que
   cede é o campo de comando: `bottom::conCmdVisible` (a fonte ÚNICA,
   pura e afervel) esconde-o quando a lista com ele fica <60% — o
   drawer pequeno vive com chips 28dp + a lista no resto. Os números
   são NOMEADOS (`kConListMinPct` 0.60, `kConChipPad` 2, `kConListGap`
   2, `kConCmdH` 40, `kConCmdPad` 4).
11. **O cabeçalho do inspector é UMA linha (0.9.6.18 · D1+D5)**: título à
   esquerda, o recolher do pin à direita, NUNCA outra faixa a partilhar
   o y (as tabs [Inspector][Nós] morreram — a vista era duplicado da
   hierarquia; nenhum botão desenha na faixa do título — a FASE 15.1 do
   device virtual afere).
12. **A linha Transform obedece ao ORÇAMENTO (0.9.6.18 · D2)**: a fonte
   única `editor::transformRowBudget` (pura) decide as larguras — painel
   largo: caixas 64 + reset chip 40; encolhe ao piso 48 com o chip;
   abaixo do piso o reset é ÍCONE inline 20 (caixas ao piso 40; no painel
   180dp dá 40 exato). NADA desenha fora do rect do painel — o draw e o
   tap partilham a MESMA matemática.
13. **O divisor é a LINHA 1dp (0.9.6.18 · D11)**: em repouso só a linha
   sólida 1dp na cor border existe (a coluna de pontos flutuantes morreu);
   a pill de grip só desenha DURANTE o drag.
14. **A marca é o glifo da função única (0.9.6.18 · D6)**: brand::drawIcon
   (G com 4 setas, LOD: ≥32dp completa / <32dp simplificada com traço
   ×1.2) — SEM fundo/tile com letra; o espelho Java é UiIcons.drawBrand.
15. **A miniatura do projeto sai da captura OFF-thread (0.9.6.18 · D7**;
     0.9.6.19 · D16: a captura SÓ corre em frame SEM overlay modal — o
     frame do gesto (menu aberto) cede a vez ao frame limpo seguinte,
     o arm é o `armThumbCapture` ponto único e o log diz «thumb: captura
     ok/falhou (<motivo>)»**): o frame do save só faz glReadPixels; o
     encode 256×144 (≤60KB) corre no thread de jobs (o padrão ImportJob);
     card sem thumb.png desenha o tile de iniciais (paleta fixa de 6; a
     leitura Java REPETE — 250/500/1000ms — porque a escrita chega depois
     do onResume) — o ícone da app NUNCA é conteúdo de card. Os
     empty-states das 4 tabs do drawer são centrados e CLIPADOS ao rect
     de conteúdo (D3/D9 — o helper partilhado bottom::drawEmptyState).
16. **O ORÇAMENTO DA LINHA TRANSFORM TEM O VALOR INTOCÁVEL (0.9.6.19 ·
     R1)**: a regra do dono — «se a largura aperta, dropa primeiro a
     letra do eixo, depois o padding, nunca o valor». A fonte única
     `transformRowBudget` degrada POR DENTRO: (a) letra+valor; (b) sem
     letra (`axisLabels=false` — o 1.º a ceder); (c) sem o padding da
     linha (`rowPadDropped` — as margens 16→4dp); (d) a última defesa é
     a scissor. O espaço do valor (`transformValueSpace`) tem o piso
     `kTfValueMinDp` 26dp nos 3 campos em 180/220/260dp — o draw e o tap
     partilham a MESMA fórmula (a fórmula antiga maxVw=boxW−28 deixava os
     campos SEM valor a 180dp — a mutação M-R1 é caçada pela R-035(10)).
17. **A CÂMARA É UM OBJETO PEQUENO (0.9.6.19 · D17)**: no passe 3D da
     viewport, cada câmara desenha GLIFO ~24dp constante em ecrã (o ícone
     Camera, na pos do Transform3D) + FRUSTUM FINO 1-2px — cinza mudo a
     ~35% alfa sem seleção, âmbar com seleção — e handles de CANTO 12dp
     SÓ com seleção (editam o fov; o handle do centro/far morreu — o far
     edita-se no Inspector). Hit-test: gizmo de mover (que ancora ao
     glifo) > handles de canto > frustum INTOCÁVEL — as linhas do frustum
     nunca interceptam toque e a seleção da câmara é pelo GLIFO
     (`camgizmo::kGlyphDp/kHandleDp/kMutedAlpha` — a FASE 16.1 afere com
     drag injetado).
18. **OS MENUS SHEET SÃO CONTIDOS E EM dp (0.9.6.19 · D14)**: o menu de
     ficheiro abre SEMPRE com offset 0 (o reset vive nos ABRIDORES da
     top bar/⋯ — o slot do scroll persiste/recicla), o fundo do sheet
     NUNCA cruza a tab bar (o maxY reserva `kBottomTabH`) e o scroll
     próprio torna a última linha alcançável INTEIRA. As medidas dos
     três sheets (ficheiro 280dp, hier 260dp, cenas 280dp; linhas 48dp,
     cabeçalhos 28dp) são dp REAL — eram px cru (a violação R-018 que
     desenhava o menu a meia medida com o texto a 2× no device).
19. **O TOGGLE «visível» MOSTRA O ESTADO (0.9.6.19 · D19)**: a linha
     VisToggle do Inspector 3D e do editor de UI desenha o SWITCH da casa
     (`editor::drawVisSwitch` — a fonte única) além do par Eye/EyeOff:
     trilho-pílula 32×16dp + knob 12dp — ON: trilho âmbar, knob à
     DIREITA; OFF: trilho border, knob à ESQUERDA. A posição do knob É o
     estado (pin: estado desenhado nos dois valores).
20. **O CHIP «N x» TEM SLOT PRÓPRIO (0.9.6.19b · m1)**: no cabeçalho da
     hierarquia, o chip da multi-seleção é dimensionado pelo TEXTO e
     posicionado por `editor::hierChipSlot` (pura, a fonte única do draw e
     do teste): ≥8dp DEPOIS do título medido e ≥8dp ANTES do ⋮. Quando o
     painel não dá os dois vãos, o chip NÃO DESENHA (degradação honesta —
     a limpeza continua no menu ⋮ «Limpar seleção»). NUNCA nasce sobre o
     título («Hierarquia1 x» do dono morreu) — R-038/FASE 17.4.
21. **O FORMAT ÚNICO DOS NÚMEROS (0.9.6.19b · m2)**: todos os campos
     numéricos do inspector (caixas X/Y/Z + sliders do 3D + sliders do
     editor de UI) formatam por `editor::formatNum` — a função única que
     normaliza o zero negativo («-0» → «0», qualquer formato que o
     produza). Pin: rotação -0.0 desenha «0» — R-039(12)/FASE 17.3.
22. **A LIMIAR DA LETRA DO EIXO (0.9.6.19b · m3)**: no orçamento da linha
     Transform, a letra X/Y/Z SÓ sai abaixo de ~200dp de painel
     (`kTfAxisMinUsableDp` 168 úteis na convenção do draw = painel −
     2×kPad); entre 200 e 260dp letra e valor COEXISTEM (o padding da
     linha cede antes — `rowPadDropped`); o VALOR nunca sai (o piso 26dp
     da R1 mantém-se). Pin nos 3 painéis 180/212/260dp — R-039(10)/FASE
     16.4.
23. **A CÂMARA AINDA MAIS PEQUENA — O CONTRATO MEDÍVEL (0.9.6.19b ·
     D20)**: glifo 20dp (era 24); handles 10dp (era 12); o frustum de
     PREVIEW tem comprimento CONSTANTE EM ECRÃ = clamp(12% da distância
     olho→câmara, 48..120dp) (`camgizmo::previewCapWorld`) e desenha o
     cone CANÓNICO (aspect 1 — o aspeto REAL vive no render/gameProj, que
     NÃO muda). O gizmo NÃO desenha o extent real do far plane. PIN
     medível: o bounding do gizmo (`camgizmo::gizmoBoundsPx`) ocupa ≤6%
     da área do viewport COM seleção e ≤4% SEM — medido pelo script de
     medidas (método PASSO 0) em 2 densidades — R-040/FASE 17.5 +
     scripts/gizmo_camera_medidas.py.
24. **O [+] DO VIEWPORT MORRE (0.9.6.19b · D21)**: o botão redondo do
     fundo-direito foi REMOVIDO do chrome (redundante — o + da hierarquia
     e o item «Novo objeto» do menu ⋯ abrem o MESMO plusMenu; o botão
     cobria a cena e interceptava toques de orbit/seleção). Os alvos de
     40dp do chrome passam a N−1 (10). PIN: NENHUM widget com o id
     `vpchrome::kVpAddTicIdRetired` (38) desenha dentro do rect do
     viewport, em estado nenhum (drawer aberto/fechado × trilho/
     inspetor) — R-041/FASE 17.1; a mutação M-D21 repõe-no → vermelho.
     O §1 perde o [+] e o §3 perde o `kCornerBtn` (NESTE commit).
25. **J-01 — OS LOGS SÃO OPAQUES, 80%, COM CORTE/SCROLL/WRAP (PASSO 4 ·
     0.9.6.20)**: o card do viewer de logs (`EditorUi.cpp
     drawLogViewer`) passa a 80% × 80% da faixa útil do overlay (era
     86% × 80% — a LARGURA passava do cap do dono) e o fundo é OPAQUE
     (`theme::kTheme.bg` α1.00 — o vidro surface deixava a cena
     atravessar o log de crash). As linhas longas QUEBRAM por palavras
     (`textwrap::wrap` — a FONTE ÚNICA das Docs; corte = `labelFitted`
     por linha visual como rede; scroll = `beginScroll` kLogsScrollId
     de sempre) e o contentH soma as LINHAS VISÍVEIS (o auto-fundo do
     1.º frame segue certo). PIN: o card no registo ≤80% da faixa nos
     dois eixos + a contagem de labels da lista == o wrap puro + os
     pixels OPAQUE #0E0E10 no device — R-037/FASE 18.2; a mutação
     M-J1 (o wrap morto) e M-J1b (o vidro de volta) → vermelho.
26. **J-02 — O VÉU DOS MENUS ANCORADOS É 40% (PASSO 4 · 0.9.6.20)**:
     o token NOVO `theme::kTheme.scrimMenu` (preto α0.40) é o fundo dos
     TRÊS menus ancorados (fileMenu ⋯ / scenesMenu Cena ▾ / hierMenu ⋮
     — os únicos overlays ancorados da casa); o `scrim` 60% fica sendo
     o véu MODAL. PIN: o valor do token + o pixel do véu == o composto
     `blendOver(scrimMenu, bg)` no device (a matemática no pixel) —
     R-037/FASE 18.1; a mutação M-J2 (o 60% de volta) → vermelho.
27. **J-03 — AS LINHAS DO SETTINGS SÃO 36DP (PASSO 4 · 0.9.6.20)**:
     `settings::settingsRowH()` 48→36dp (a FONTE ÚNICA exportada no
     header; o contentH do scroll segue-a); os CABEÇALHOS de secção
     mantêm 48dp (a spec manda nas LINHAS) e os controlos de linha
     mantêm os 28dp da casa (o toggle D4b e o botão compacto centram a
     dp(4) na linha de 36). O alvo de toque é a LINHA INTEIRA (classe
     de piso 36 — kRowFloorDp). PIN: o valor da fonte + o h da linha no
     registo do device == 36dp e o cabeçalho == 48dp — R-037/FASE 18.3;
     a mutação M-J3 (o 48 de volta) → vermelho.
28. **J-04 — OS CANTOS SUAVES FECHAM (PASSO 4 · 0.9.6.20)**: os últimos
     cards com painel/frame DUROS ganham os raios da casa —
     drawStorageDialog, drawImportMenu (EditorUi) e drawRemoveDialog,
     drawTextInput (card + buffer com kRadiusField), drawApplyDialog
     (UiEditor) — e os TRÊS sheets ancorados passam a medir o raio em
     dp REAL (`theme::dp(kRadiusCard)` — era o token cru 8px = 4dp a
     2.0, a mesma classe R-018). PIN: o pixel (2,2) do card fica FORA
     do raio 8dp e mostra o fundo (o painel duro mostraria o vidro) —
     R-037/FASE 18.1(c)/18.4; a mutação M-J4 (o painel duro de volta)
     → vermelho. ACHADO honesto: `drawSettingsMenu` é código morto no
     app (a página Settings 0.9.0 substituiu-o; só os testes o
     chamam) — NÃO tocado, candidata a remoção (decisão do dono).
29. **A ÁRVORE EDITA-SE (0.11-H · FASE 1)**: a linha da hierarquia passa a
     `[chev 24][ícone 40][nome][olho 40][⋮ 40]` — o CHEVRON (só em TIC
     com filhos; `icons::ChevronDown/ChevronRight`) expande/colapsa e o
     estado vive POR NOME em `EditorState::hierCollapsed` (persiste no
     layout.json via `bottom::serializeLayout`/`parseLayout` — as linhas
     `hierColN=`/`hierCol=`; limpa na troca de cena). A zona do ÍCONE é o
     claim do DRAG-REPARTENT (`UiContext::dragClaim` — a prioridade é a
     do slider: drag na zona = o drag, no resto da linha = scroll; o tap
     sem movimento mantém o toggle da multi-seleção). O drop ARMA
     `hierReparentPending` e o main CONSUME no ponto seguro: revalida com
     `editor::canReparentTo` (ciclo/alvo morto recusados — o drop num
     alvo ilegal cancela), aplica com `editor::reparentTic` (muda SÓ o
     pai — o TRS local fica) e empurra o undo do snapshot. Apagar com
     filhos = `editor::destroyWithChildren` (o diálogo conta com
     `editor::countSubtree` e DIZ o N); duplicar com filhos =
     `editor::duplicateWithChildren` (deep clone, nomes únicos). PINs:
     test_fase1_hierarquia (12 testes — o drag real do neto para outra
     raiz com os filhos a acompanhar incluído).
