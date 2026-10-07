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
| TopNavigationBar | TOP BAR 36dp (PASSO 1) | `safe::toolbarRect` em `ui/SafeArea.h`; desenhada por `toolbar::draw` em `ui/Toolbar.cpp` |
| ViewportContainerLayout | viewportRect (entre a top bar e a tab bar) | `safe::viewportRect` em `ui/SafeArea.h` |
| GLSurfaceView | o passe 3D (glViewport+glScissor no centerRect) | o bloco `scissor3d` em `platform/main.cpp` |
| TransformToolbar | toolbar inferior do viewport (Sel/Mov/Rod/Esc/Ímã) | `vpchrome::layout` em `ui/ViewportChrome.cpp` |
| ViewportRail | o stack vertical esquerdo (undo/redo/save/dup/paste) | `vpchrome::layout` — `L.stack` em `ui/ViewportChrome.cpp` |
| BottomDockLayout | o drawer de baixo + tab bar 32dp (Ficheiros/Assets/Consola/Animação + «FPS · TICs» à direita — PASSO 1). PASSO 2: o drawer NUNCA abre sozinho (o auto-abrir da 0.8.0 saiu) e a altura efetiva é capada a 35% da altura do content | `bottom::layout` em `ui/BottomPanel.cpp` |
| BottomBar | REMOVIDA no PASSO 1 (0.9.6.14): a faixa extra de 24dp saiu — o «FPS · TICs» vive no canto direito da TAB BAR (`bottom::layout`); a versão/commit em Settings › Sobre. `safe::statusRect` devolve uma faixa de ALTURA ZERO (compat de testes; nada desenha nela) | `safe::statusRect` em `ui/SafeArea.h` (LEGACY, altura 0) |

## 1. A ÁRVORE (pai → filhos; a ordem de DESENHO é a ordem de z)

```
contentRect (superfície EGL − insets do sistema)
├── PASS 3D (a "cena") — glViewport/glScissor = centerRect; aspect do rect
│   └── (a câmara projeta com o aspect DO RECT, nunca do ecrã todo)
├── TOP BAR (36dp — PASSO 1, faixa de topo)          [toolbar::draw]
├── VIEWPORT REGION (safe::viewportRect)             [pai lógico]
│   ├── STRIP do topo (40dp — PASSO 1): [Cena][Perspetiva][Global] + [+] nos
│   │   ecrãs estreitos (plusTopRight)                [vpchrome::layout]
│   ├── RAIL esquerdo (stack undo/redo/save/dup/paste, pai de vidro)
│   ├── TRANSFORM TOOLBAR (fundo: 5 botões 40dp + legenda acima — PASSO 1)
│   └── [+] Adicionar TIC (canto inferior direito; sup-dir na strip
│       quando a largura não dá — plusTopRight)
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
│   └── LINHA «recolher» (28dp sob o cabeçalho, id 7433 — SÓ existe
│       enquanto fixado: a seta DESFAZ o pin e o trilho volta sem
│       seleção — P2-bis)
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
2. **A transform toolbar vive DENTRO da viewport region**, abaixo da
   strip: `by ≥ view.y + stripH` e `by + 40dp ≤ view.y + view.h`. Nunca
   sobe à top bar (o cap do drawer garante o piso; o clamp em
   `vpchrome::layout` garante POR CONSTRUÇÃO).
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
   `kInspUnpinId` 7433, linha de 28dp sob o cabeçalho) DESFAZ o pin —
   fixado, o painel fica aberto MESMO sem seleção; com seleção a regra
   PASSO 2 manda (o painel volta a fechar ao limpar a seleção).
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

## 3. OS PISOS (fontes únicas em ui/SafeArea.h)

| Constante | Valor | Significado |
|---|---|---|
| `kTopBarH` | 36dp | a top bar (PASSO 1 — era 56) |
| `kStatusH` | 0 | a status bar saiu (PASSO 1; faixa compat de altura zero) |
| `kBottomTabH` | 32dp | a tab bar do dock (PASSO 1 — era 48; o FPS·TICs vive nela) |
| `kHierPct` / `kInspPct` | 18% / 22% | os DEFAULTS dos painéis (PASSO 2 — spec do dono) |
| `kHierMinW` | 140dp | piso da hierarquia (PASSO 2 — era 200) |
| `kInspMinW` / `kInspMaxW` | 180 / 260dp | piso e teto do inspector (PASSO 2 — o piso era 272) |
| `kInspTrackW` | 32dp | o TRILHO do inspector sem seleção (PASSO 2) |
| `kDrawerMaxPct` | 35% | o teto da ALTURA do drawer aberto (PASSO 2 — o cap vive em `effectiveDrawerH`) |
| `kDrawerHandleH` / `kDividerHitW` | 24dp | as PEGAS de arrasto (PASSO 2 — eram 12/20) |
| `kViewportMinW` | 288dp | piso da LARGURA do viewport central no DRAG (a toolbar cabe) |
| `kViewportMinH` | 88dp | piso da ALTURA do viewport central com o drawer aberto (strip 40 + toolbar 40 + folga 8 — PASSO 1) |
| `kDrawerMin`/`kDrawerMax` | 160..400dp | o ESTADO CRU da pega do drawer (o cap duplo §2.3 manda no efetivo) |
| `layout::kTouchFloorDp` | 40dp | piso de toque do botão SOLTO (desenho 32 — a LEI DE OURO do PASSO 1; era 48) |
| `layout::kRowFloorDp` | 36dp | piso de LINHA (top bar, listas, consola) |
| `layout::kFieldFloorDp` | 32dp | piso de CAMPO (caixas X/Y/Z, tabs de baixo, pesquisa) |
| `layout::kHeadFloorDp` | 28dp | piso de CABEÇALHO/chips (kHeaderH, tabs Inspector/Nós, chips da consola, a linha «recolher» do pin — PASSO 2/P2-bis) |

## 4. SENTINELAS E GATE

| ID | Onde | O que afere |
|---|---|---|
| R-022 | tests/test_sentinels.cpp `regress_hierarquia_contrato` | a árvore §1 inteira: containment, regras §2.1-2.6, pisos §3 (o cap DUPLO do drawer incluído), em ecrãs 776×336/800×360/1536×720 e densidades 1.0/2.0, drawer fechado/aberto/tapado |
| R-023 | tests/test_sentinels.cpp `regress_texto_strip_campo` | chips da strip + campo «pesquisar» sem recorte em 3 densidades (mdpi/hdpi/xhdpi) |
| R-024 | tests/c33_virtual.cpp FASE 14.3 + sentinela `regress_viewport_rect_segue` | abrir/fechar o dock → o rect muda, o log `vp3d: viewport set to` aparece com os números certos, o aspect segue |
| R-025 | tests/test_sentinels.cpp `regress_rodape_intocavel` | DESDE O PASSO 1: a TAB BAR de 32dp é a última faixa em todos os estados; nada a cobre; o «FPS · TICs» vive no canto direito (o middle do projeto ficou nas unidades puras de TextFit — a status bar saiu) |
| R-033 | tests/test_sentinels.cpp `regress_paineis_passo2` | A SPEC PASSO 2 MEDIDA: defaults 18%/22% (piso 140/180, teto 260), viewport ≥55% nos ecrãs de referência, o trilho de 32dp sem seleção (e o centerRect a devolver a área), o intervalo [180..260] no drag, as pegas 24dp, o drawer default FECHADO e o teto de 35% |
| R-034 | tests/test_sentinels.cpp `regress_p2bis_pin_e_consola` + c33 FASE 13.7i | P2-bis: o pin (o predicado com o pin, o round-trip inspPinned retrocompatível, os ids do par 7432/7433) + a regra da consola (conCmdVisible pura; a lista ≥60% nos conteúdos reais 80/88/136/191/216/336dp; o E2E no harness @2.0 — tap no trilho fixa, a seta desfaz, o campo de comando sai do drawer pequeno, os PNGs+JSON) |
| gate | scripts/hierarchy_check.py (job core-tests do CI) | este ficheiro existe, a tabela §0 aponta símbolos REAIS, as sentinelas R-022..R-025 + R-033 existem no fonte |

Qualquer região nova: acrescenta AQUI (§1 + §0 se for topo de ramo) com
a fonte no código, e a sentinela R-022 estende-se ao novo pai — o gate
não deixa o código passar sem o contrato.
