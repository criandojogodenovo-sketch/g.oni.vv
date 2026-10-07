# RELATÓRIO 0.9.6.13 — PASSO 0: INVENTÁRIO DO EDITOR 3D (spec UI · sem alterações)

> Spec do dono: «PASSO 0: INVENTÁRIO (sem alterar nada) — Lista TODOS os
> controlos visíveis no editor 3D, com o nome, o que fazem (a função
> chamada, ou "SEM FUNÇÃO") e onde estão». Este passo NÃO altera uma
> linha de código do editor: o diff é documentação (este ficheiro +
> README + BACKLOG). O inventário foi feito POR LEITURA DO FONTE
> (P-01/P-04: zero adivinhação) e as medidas PELA EXECUÇÃO DO CÓDIGO
> REAL de layout (`ui/SafeArea.h`, a fonte única do contrato P-08) num
> programa de medição fora do repo — os números px/dp/% são saída do
> código, não estimativas.

## 1. A verdade da plataforma (lida antes do inventário)

O editor NÃO tem árvore de views Java (P-08, `docs/LAYOUT_HIERARCHY.md`
§0): uma `NativeActivity` com UMA superfície EGL e todo o chrome é UI
immediate-mode C++. «Controlo visível» aqui = cada widget desenhado com
alvo de toque (`ui.widgetHit`) ou cada texto de informação. A posição é
a região do contrato; o tamanho é a constante do fonte (citada).

## 2. O INVENTÁRIO COMPLETO (estado 0.9.6.12g, commit 5edcbde)

### 2.1 TOP BAR 56dp — `toolbar::drawTopBar` (ui/Toolbar.cpp)

| # | Controlo | Função real (símbolo chamado) | Posição | Tamanho |
|---|---|---|---|---|
| T1 | Logo «G» âmbar | **SEM FUNÇÃO** (marca; comentário do fonte: «o toque não faz nada») | extrema esquerda | 48×48dp, ícone/rótulo |
| T2 | Nome «G.One» | SEM FUNÇÃO (texto de identidade) | esquerda, após o logo | 58dp larg; some com k<0.78 |
| T3 | [≡ Menu] | `a.menuDropdown` → main.cpp 5097: alterna `fileMenu` (dropdown Guardar/Settings/Sair) | esquerda | 112×48dp |
| T4 | [Cena ▾] | `a.cenaDropdown` → main.cpp 5104: alterna `scenesMenu` (overlay CENAS) | esquerda | 120×48dp |
| T5 | Tab [3D] | `a.modeChanged` → `st.uiMode=false; st.audioMode=false` | centro | 96×48dp, ícone 24 |
| T6 | Tab [UI] | → `st.uiMode=true` | centro | 96×48dp |
| T7 | Tab [ÁUDIO] | → `st.audioMode=true` | centro | 96×48dp |
| T8 | [▶ play] | `a.playPressed` → entra no modo play | direita | 48×48dp |
| T9 | [⏸ pause] | `a.pausePressed` | direita | 48×48dp |
| T10 | [■ stop] | `a.stopPressed` | direita | 48×48dp |
| T11 | Chip [Android ▾] | `a.platformPressed` → main.cpp 5208: toast «plataforma alvo: Mobile (Android)» | direita | 92×48dp; some com k<0.62 |
| T12 | [⚙ gear] | `a.gearPressed` → main.cpp 5111: `settingsMenu=true` (página Settings) | extrema direita | 48×48dp |

### 2.2 VIEWPORT — `vpchrome::draw` (ui/ViewportChrome.cpp)

| # | Controlo | Função real | Posição | Tamanho |
|---|---|---|---|---|
| V1 | STRIP topo (pai de vidro) | pai; faixa inteira sobre a cena | topo da viewport, largura TOTAL | 48dp de altura |
| V2 | Chip [Cena] | **SEM FUNÇÃO** (indicador info da vista ativa; SEM `widgetHit`) | strip, esquerda | 32dp alt, wrap-content piso 56dp |
| V3 | Chip [Perspetiva] | **SEM FUNÇÃO** (info: projeção da câmara) | strip, centro | 32dp alt |
| V4 | Chip [Global] | **SEM FUNÇÃO** (info: espaço do gizmo, sempre mundial) | strip, direita | 32dp alt |
| V5 | [+ strip] (ecrãs estreitos) | `a.addTicPressed` (o MESMO T18, alojado no fim da strip) | fim direito da strip | 56dp |
| V6 | Rail esquerdo (pai de vidro) | pai do stack | esquerda, sob a strip | 64dp larg |
| V7 | [↶ desfazer] | `a.undoPressed` → `g_undo.undo(g_scene)` | rail, coluna | 48×48dp |
| V8 | [↷ refazer] | `a.redoPressed` | rail | 48×48dp |
| V9 | [💾 guardar] | `a.savePressed` | rail | 48×48dp |
| V10 | [⧉ copiar/duplicar] | `a.dupPressed` (glifo Copy, ação duplicar) | rail | 48×48dp |
| V11 | [📋 colar] | `a.pastePressed` | rail | 48×48dp |
| V12 | [Selecionar] | `st.selectMode=true; gz.mode=0` (cursor, sem gizmo) | toolbar fundo | 48×48dp |
| V13 | [Mover] | `gz.mode=0` | toolbar fundo | 48×48dp |
| V14 | [Rodar] | `gz.mode=1` | toolbar fundo | 48×48dp |
| V15 | [Escalar] | `gz.mode=2` | toolbar fundo | 48×48dp |
| V16 | [🧲 Ímã] | alterna `gz.snap` (toggle real) | toolbar fundo | 48×48dp |
| V17 | Legenda da ferramenta | SEM FUNÇÃO (info: nome da ferramenta ativa) | acima da toolbar | 12sp |
| V18 | [+] Adicionar TIC | `a.addTicPressed` → plus-menu de presets | canto inf. direito | 56dp botão |

### 2.3 PAINEL HIERARQUIA — `drawHierarchy` (ui/EditorUi.cpp 340)

| # | Controlo | Função real | Posição | Tamanho |
|---|---|---|---|---|
| H1 | Título «Hierarquia» | SEM FUNÇÃO (info) | cabeçalho | 16sp, cabeçalho 48dp |
| H2 | Chip «N ×» (multi) | `kHierMultiClearId` → limpa a multi-seleção | cabeçalho, direita | 56×48dp; só com N≥1 |
| H3 | [+] do painel | retorno de `drawHierarchy` → main.cpp 5415: `plusMenu=true` (presets) | cabeçalho, direita | 48×48dp |
| H4 | [⋯ ⋮ do painel] | `kHierDotsId` → alterna `st.hierMenu` (sheet com «Limpar seleção» e «Nome completo do TIC») | cabeçalho, direita | 48×48dp |
| H5 | Campo «pesquisar TIC» | `kHierSearchId` → teclado (textPurpose 8); filtra a árvore | linha sob o cabeçalho | 48dp alt |
| H6 | Linhas da árvore | `kIdRowBase+i` → seleciona o TIC (corpo) / desseleciona (vazio) | lista com scroll | 48dp/linha |
| H7 | [olho] por linha | re-despacho do scroll → alterna `t->visible` | direita da linha | ícone 24 em zona 48 |
| H8 | [⋮] por linha | menu da linha (ações do TIC) | direita da linha | ícone 24 em zona 48 |
| H9 | Long-press no nome | tip com o nome completo (toast) — só quando truncado | linha | — |

### 2.4 PAINEL INSPECTOR — `drawInspector` (ui/EditorUi.cpp 726)

| # | Controlo | Função real | Posição | Tamanho |
|---|---|---|---|---|
| I1 | Título «Inspector» | SEM FUNÇÃO (info) | cabeçalho | 16sp |
| I2 | Tab [Inspector] | `kInspTabBase+0` → `st.inspTab=0` (vista propriedades) | cabeçalho, direita | 48dp alt — **redundante: repete o título ao lado** |
| I3 | Tab [Nós] | `kInspTabBase+1` → `st.inspTab=1` (lista de nós da cena) | cabeçalho, direita | 48dp alt |
| I4 | Vista Nós (linhas) | seleciona o nó e volta às propriedades | lista | 48dp/linha |
| I5 | Campos/sliders (X/Y/Z, cor, mesh, tex, câmara, prim, áudio, anim…) | `kInspector*` → aplicam no TIC selecionado | corpo com scroll | campos 48dp |

### 2.5 BOTTOM DOCK — `bottom::draw` (ui/BottomPanel.cpp)

| # | Controlo | Função real | Posição | Tamanho |
|---|---|---|---|---|
| B1 | Tab [Ficheiros] | `kTabFilesId` → `bs.bottomTab=1` (toggle: ativa fecha) | tab bar 48dp | ¼ da largura |
| B2 | Tab [Assets] | `kTabAssetsId` → `bs.bottomTab=2` | tab bar | ¼ |
| B3 | Tab [Consola] | `kTabConsoleId` → `bs.bottomTab=3` | tab bar | ¼ |
| B4 | Tab [Animação] | `kTabAnimId` → `bs.bottomTab=4` | tab bar | ¼ |
| B5 | Pega (grip) | drag REAL: `dragBaseH+(startY-py)`, 160..400dp passos de 8 | topo do drawer | 12dp + hit ±8 |
| B6 | Árvore res:// | `kTreeRowBase+i` → seleciona pasta; abre browser filtrado (tab 2) | tab Ficheiros | linhas 48dp |
| B7 | Cards de assets | `a.filePick` + `filePickKind` → aplica o asset | tab Assets | grelha |
| B8 | Chips [Consola][Logs][Erros][Avisos] | `kConsoleTabBase+i` → `bs.consoleTab` (filtro −1/1/2/3) | tab Consola | 36dp alt — **«Consola» duplica o nome da tab B3** |
| B9 | [auto: sim/não] | `kAutoScrollId` → alterna `consoleAutoScroll` | tab Consola | 88×36dp |
| B10 | [export] | `kExportId` → `a.exportPressed` | tab Consola | 88×36dp |
| B11 | Campo de comando + [▶] | `kCmdFieldId` → `a.commandPressed` (teclado; main corre limpar/ajuda/play/stop/snap) | fundo da Consola | 40dp alt |
| B12 | Linhas do log | toque EXPANDE a linha (spec K) | lista com scroll | mono 12sp |
| B13 | Timeline | desenhada pelo main na tab Animação | drawer | — |

### 2.6 STATUS BAR 24dp — `drawStatusBar` (ui/BottomPanel.cpp 706)

| # | Controlo | Função real | Posição | Tamanho |
|---|---|---|---|---|
| S1 | «G.One <versão> · <projeto> · FPS n · TICs n» | SEM FUNÇÃO (info; projeto elipsado a meio — J4/R-025) | esquerda | 12sp |
| S2 | «editor · Mobile First» / «play · …» | SEM FUNÇÃO (info de estado + selo) | direita | 12sp |

### 2.7 OVERLAYS condicionais (por cima de tudo — não contam no estado de repouso)

Menus (fileMenu, scenesMenu, plusMenu, hierMenu, menus do Inspector),
página Settings (`SettingsPage`), janela LOGS (`drawLogViewer` — card
86%×80%), browser de ficheiros, editor de script, teclado/IME, toasts.

## 3. OS SUSPEITOS DA SPEC — VEREDITO POR LEITURA DO FONTE

| Suspeito (spec) | Veredito | Fundamento (ficheiro) |
|---|---|---|
| Chip «Cena» | **SEM FUNÇÃO** — indicador de vista; duplica a [Cena ▾] da top bar | ViewportChrome.cpp: `chipLabel(L.stripCena,…)` sem `widgetHit` |
| Chip «Perspetiva» | **SEM FUNÇÃO** — info (projeção); nenhum toggle | idem |
| Chip «Global» | **SEM FUNÇÃO** — info (gizmo sempre mundial, Gizmo.h) | idem |
| Botão «···» da hierarquia | TEM FUNÇÃO (alterna `hierMenu`: limpar seleção / nome completo) — o ícone ⋮ é REAL, não placeholder | EditorUi.cpp 463-477 |
| Aba «Nós» | TEM FUNÇÃO (vista lista de nós) — mas duplica a árvore da Hierarquia ao lado | EditorUi.cpp 764-800 |
| Aba «Inspector» junto ao título «Inspector» | TEM FUNÇÃO (volta às propriedades) — mas o RÓTULO é redundante: repete o título ao lado | EditorUi.cpp 752-800 |
| Botão «G» amarelo do topo | **SEM FUNÇÃO** — é o LOGO (marca); comentário do fonte: «o toque não faz nada» | Toolbar.cpp 249-252 |
| Texto «prooksnsn» | NÃO EXISTE no código — é o NOME DO PROJETO (`StatusBarData.project`) na status bar: dado do utilizador, não controlo. Aparece tal como foi escrito no projeto | BottomPanel.cpp 738-760 |
| «editor · Mobile First» | SEM FUNÇÃO — texto de estado + selo estático da status bar | BottomPanel.cpp 744-760 |

Regra da spec aplicada nos passos seguintes: REMOVER só depois desta
lista (o dono aprova no fim do PASSO 0). Os chips SEM FUNÇÃO e o selo
«Mobile First» saem no PASSO 1/2; o rótulo duplicado «Inspector» sai no
PASSO 2; o destino da aba «Nós» (função real, conteúdo duplicado) fica
marcado para decisão no PASSO 2.

## 4. TABELA DE MEDIDAS — ESTADO ATUAL (medido pelo código real)

Programa de medição: inclui `ui/SafeArea.h` do repo (as MESMAS funções
que o editor corre) e mede os dois perfis de referência da casa. A
% de cobertura usa união em grelha de 2px dos pais de vidro
(strip + rail + toolbar + [+]) sobre a viewport central.

### Perfil A — harness C33 1536×720 @1.0 (FASE 13), insets 0

| Região | px | dp | % |
|---|---|---|---|
| Top bar | 1536×56 | 1536×56 | 7,8% alt |
| Tab bar de baixo | 1536×48 | 1536×48 | 6,7% alt |
| Status bar | 1536×24 | 1536×24 | 3,3% alt |
| Hierarquia | 300×592 | 300×592 | 19,5% larg |
| Inspector | 300×592 | 300×592 | 19,5% larg |
| Viewport central | 936×592 | 936×592 | **60,9% larg** |
| topo+abas+estado | 128 | 128 | **17,8% alt** |
| Cobertura da viewport por controlos | — | — | **27,1%** (strip full-width 48dp é a maior parte) |

### Perfil B — device RMX3624 1600×720 @2.0, insets 12dp (content 776×336dp)

| Região | px | dp | % |
|---|---|---|---|
| Top bar | 1552×112 | 776×56 | 16,7% alt |
| Tab bar de baixo | 1552×96 | 776×48 | 14,3% alt |
| Status bar | 1552×48 | 776×24 | 7,1% alt |
| Hierarquia | 400×416 | 200×208 | 25,8% larg |
| Inspector | 576×416 | 288×208 | 37,1% larg |
| Viewport central | 576×416 | 288×208 | **37,1% larg — FALHA o (a) ≥55%** |
| topo+abas+estado | 256 | 128 | **38,1% alt — FALHA o (c) ≤20%** |
| Cobertura da viewport | — | — | **88,5% — FALHA o (b) ≤10%/≥85% cena limpa** |
| Drawer aberto (cap) | — | 104dp efetiva | viewport central resta 104dp de altura |

Constatação medida (baseline honesta para os PASSOS 1-4): no device real
o editor falha HOJE os critérios (a), (b) e (c) da spec — a viewport
central tem 37,1% da largura, 88,5% da área está coberta por controlos
(a strip de largura total sozinha come 48/208dp de altura) e o chrome
vertical soma 38,1%. No harness largo (Perfil A) os critérios (a)/(c)
passam, a cobertura (b) não (27,1%).

## 5. O VALIDADOR (critério f — «0 violações»)

O validador da casa é a auditoria de layout do GRUPO B (FASE 13 do
c33_virtual, `layout::Record` → `layout/auditoria-editor.txt`,
veredito «0 erros, 0 avisos»), executada em cada release e nos ecrãs
776×336 / 800×360 / 1536×720. No PASSO 0 NADA mudou no editor: a linha
de base permanece a do commit 5edcbde — c33_virtual 456/456 e as gates
locais verdes (verificado no fecho de A2-2; re-verificado no CI deste
commit). Os passos 1-4 re-correm a auditoria por passo.

## 6. NÃO VERIFICADO (honesto)

- **Device real (P-07):** as medidas do Perfil B saem do código de
  layout corrido em anfitrião; nenhuma captura do RMX3624 foi tirada
  neste passo. O sign-off do dono no device permanece aberto.
- **PNG «antes»:** não há captura do estado anterior distinto do
  `j2-device-drawer-aberto.png` já no repo (esse mostra o drawer aberto
  após o J2, não o inventário). O primeiro PNG do PASSO 1 sai da FASE
  14 do harness após a mudança.
- **«prooksnsn»:** confirmado como dado (nome do projeto) por leitura;
  nenhum dispositivo do dono foi inspecionado — se o dono o escreveu
  noutra superfície (IME), o texto é dele.
- **A aba «Nós»** tem função real (lista de nós); a decisão de a remover
  ou fundir é do dono no fim deste passo (a spec pede a lista ANTES de
  remover).
