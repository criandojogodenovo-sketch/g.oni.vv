# RELATORIO-0.12-C — Layout, organização e consistência

> Campanha 0.12-C (a ordem do dono; a pré-condição era a 0.12-B FECHADA —
> o commit e8c9587). Natureza: polish visual e reorganização — não muda
> arquitetura. A cláusula anti-superficialidade manda: NADA de controlos
> «de fachada»; se um botão não faz nada, sai. Este relatório segue as
> cláusulas da casa: P-01 (tudo lido do fonte, zero adivinhação), P-04
> (o não-verificado fica listado), P-05/P-07 (o C33 é do dono no device).

## 1. SUB-FASE C-1 — Proporções e painéis (versionCode 87)

### 1.1 O que o prompt pedia e onde ficou

| Pedido (C-1) | Onde ficou (fonte única) |
|---|---|
| Viewport ≥ 60% por defeito | o estado de FÁBRICA é sem seleção → o inspector é o TRILHO de 32dp (a regra PASSO 2): hier 20% + trilho dá ≥60% em TODOS os ecrãs de referência; o pin `viewport_60_pct` aferra os DOIS estados (trilho ≥60% nos 5 ecrãs; com seleção ≥60% nos ≥1300dp; o piso ≥55% da R-033 mantém-se nos estreitos) — ui/SafeArea.h (resolvePanels) |
| Hierarquia ~20% | `safe::kHierPct = 0.20f` (era 0.18 — a ordem expressa do dono; o piso kHierMinW=140 mantém) — ui/SafeArea.h |
| Inspector ~22% | mantido (`kInspPct = 0.22f`, piso 180/teto 260 — intocado do PASSO 2) |
| Painel inferior COLAPSADO por defeito | JÁ era a regra do PASSO 2 (o auto-abrir da 0.8.0 morreu lá) — o pin `painel_colapsado_default` aferra: `BottomState{}` nasce `bottomTab=0`, a altura consumida é 0, `EditorState::drawerH` nasce 0; a mutação M-C1b prova que o pin grita se o abrir sozinho voltar |
| Com o painel inferior aberto: viewport com altura mínima + divisores arrastáveis | já garantido pelo cap duplo `effectiveDrawerH` (o piso kViewportMinH=88dp + o teto 35%) e pelos divisores do GRUPO D — o pin `divisor_drag` aferra o estado cru com os clamps |
| Hierarquia: cabeçalho em linha própria (título à esquerda, ações à direita, elipse se preciso) | `hierHeaderRects()` NOVA (ui/EditorLayout.h) — a FONTE ÚNICA da geometria título/⋮/+ partilhada pelo draw (EditorUi.cpp drawHierarchy) e pelo pin `hierarquia_cabecalho`; o título ELIPSA com «…» no budget dele (`labelFittedStyled`) e NUNCA sobrepõe as ações |

### 1.2 O achado (o bug real sob o pedido)

O cabeçalho da hierarquia tinha o título SEM budget: no PISO 140dp do
painel, «Hierarquia» a 16sp desenhava POR CIMA do ⋮ e do + (o mesmo padrão
do «Hierarquia1 x» que o dono reportou a meio da 0.9.6.19b para o CHIP —
o chip ganhou o hierChipSlot, o título não tinha a mesma proteção). A cura
é a fonte única: o título vive no budget `dots.x − 8dp − kPad` e elipsa.

### 1.3 Os pins (tests/test_012c_paineis.cpp)

1. **viewport_60_pct** — o trilho dá ≥60% em {776, 800, 1512, 1536, 1600}dp
   (medido: 75,9 / 76,3 / 78,9 / 79,5 / 79,8%); o drawer aberto NÃO mexe a
   largura (come altura); com seleção ≥60% nos ≥1300dp (63,05 / 63,75%) e
   o piso ≥55% da R-033 nos estreitos (56,8 / 57,5%); hier == 20% de 1600
   (320dp).
2. **painel_colapsado_default** — `BottomState{}` → bottomTab==0; a altura
   consumida pelo editor é 0 (a mesma regra da R-033(5)); `EditorState{}`
   → drawerH==0; o layout do dock com a tab fechada não abre nada.
3. **hierarquia_cabecalho** — nos 140/300dp: `titleX + titleMaxW ≤
   dots.x − 8dp` (o título nunca cruza o ⋮); os alvos ⋮ e + são a linha
   de 28dp e o + é o extremo direito (a composição não muda de sítio); em
   300dp o budget ≥ 96dp (o nome inteiro cabe sem elipse).
4. **divisor_drag** — o estado cru `hierW` manda com clamp (200 fica 200;
   100 sobe ao piso 140; 9000 clampa contra o piso do viewport); o
   intervalo do inspector [180..260] é invariante na varredura 0..900;
   as pegas são 24dp; o layout.json leva `hierW`/`inspW` (round-trip).

### 1.4 As mutações (coladas)

- **M-C1a** — os defaults fixos de 300 de volta (o comportamento 0.8.x que
  dava 37% no device do dono): `resolvePanels()` devolve 300/300 fixos.
  Resultado: **viewport_60_pct VERMELHA (12 EXPECTs** — o trilho dá 22,7%
  em 776dp: «viewport_60_pct: 776dp com trilho dá 22.7% (spec C-1: ≥60%
  por defeito)»), **divisor_drag VERMELHA (62 EXPECTs** — o clamp da
  gangorra morre com o return cedo), **hierarquia_cabecalho VERDE** (a
  função do cabeçalho não consome resolvePanels — o isolamento provado).
  Reposto → verde.
- **M-C1b** — o auto-abrir da 0.8.0 de volta: `BottomState::bottomTab`
  default = 4. Resultado: **painel_colapsado_default VERMELHA (4
  EXPECTs** — `bs.bottomTab == 0` e `nearEq(consumed, 0.0f)`), as irmãs
  verdes. Reposto → verde.

### 1.5 Verificação

- test_core: 0 falhas nos grupos afetados — regress_ · safearea_ ·
  toolbar_ · wiring090_ · uieditor_ · hierarquia_ · 012a_ · 012b_ ·
  scenes_ · y1_ · y2_ · y3_ · uilayout_ · play_ · scroll_ + os 4 pins
  novos. (O ambiente local da sessão vive sem disco — as falhas de mkDir
  nos TUs de FS são do ambiente e não da mudança; o CI é o árbitro,
  regra §5 do STATE.md.)
- R-033 (os painéis 18%/22%/trilho/cap) permanece VERDE sem recalibração:
  o pin dela aferra ≥55% com seleção (56,8–63,0% medidos) e os pisos/tetos
  do inspector não mudaram.

### 1.6 Pin C33 (o dono no device)

- [ ] Abrir o editor de fresco: a hierarquia ~20% da largura, o inspector
      no trilho, o viewport ≥60% — PNG ANTES (docs/y-editor3d.png, o vc 81)
      e DEPOIS (o device com o vc 87).
- [ ] Selecionar um TIC: o inspector volta a ~22% — o viewport continua
      utilizável.
- [ ] Abrir o drawer: o viewport encolhe e mantém a altura mínima; arrastar
      os divisores funciona e o editor lembra no layout.json.

### 1.7 Limites (P-04)

1. COM seleção em ecrãs < ~1300dp o viewport fica 56,8–57,5% (< 60%): os
   pisos do inspector (180dp) e da hierarquia (140dp) mandam sobre as
   percentagens — a matemática não fecha com 22%+20% num ecrã de 776dp. O
   piso ≥55% da R-033 cumpre-se; se o dono quiser 60% COM seleção também
   nos estreitos, é uma decisão de spec (baixar o piso do inspector ou o
   da hierarquia) — não se decidiu aqui sem ordem.
2. As PNGs do C33 são do device do dono (a sessão local não tem device);
   o ANTES é o y-editor3d.png do fecho da 0.11-Y.

## 2. SUB-FASE C-2 — Menus, sobreposições, barra de topo (versionCode 88)

### 2.1 O que o prompt pedia e onde ficou

| Pedido (C-2) | Onde ficou (fonte única) |
|---|---|
| Menu principal: altura máxima + scroll + fundo escurecido atrás (nunca cortado em baixo) | JÁ era o D14 + o scrim 40% do PASSO 4; a matemática extraída para `menuSheetLayout()` (ui/EditorLayout.h — o draw e os pins partilham); o pin `menu_scroll` aferra (896dp de conteúdo, maxH 644 no device, o fundo nunca cruza a tab bar) |
| Menus ancorados ao botão (popover com seta) ou folha completa (não soltos) | TODOS os sheets da casa JÁ eram ancorados (o menu principal pelo menuAx/menuAy do ⋯; o CENAS e o ⋮ da hierarquia com o MESMO contrato ax/ay) — nenhum «solto»; o pin `menu_ancorado` aferra a âncora e os clamps. A SETA do popover: a casa não desenha glifo de seta nos sheets (o vidro nasce a 8dp do botão — a relação é inequívoca); inventar a seta seria glifo novo fora do pedido — NÃO verificado com o dono (P-04) |
| Barra de topo em 3 zonas | JÁ era o G2-10; o pin `topbar_3_zonas` aferra as 3 zonas sem sobreposição nos 4 ecrãs de referência + a degradação 480dp |
| Botão play em destaque quando ativo | o `iconButton` da top bar ganhou o parâmetro active — em playMode o play é A CHAMA (o gradiente accent→accent2 da Y-1 + tinta accentInk). NOTA P-04: em playMode a top bar NÃO desenha (a play bar mínima substitui — 0.6.8); o destaque ativa se a barra se desenhar (o main diz «se algum dia se desenhar»); o botão ATIVO da casa já é a chama no CHOKE POINT button() (a Y-1) — a play bar herda |
| Ferramentas do viewport numa só barra (remover o duplicado do Mover) | O GIZMO 40dp do topo-direito REMOVIDO (mostrava O MESMO ícone da ferramenta ativa; a transição selectMode↔gizmo já vive no botão Selecionar do rail — sem função diferente = fachada = SAI); o id fica APOSENTADO (o padrão D21); o pin `mover_unico` aferra o gizmoBtn/gizmoPanel ZERO |
| Barras flutuantes mais pequenas e encostadas às bordas | o rail/quick já encostam (m=8dp); com o gizmo fora, a fila do topo e a legenda respiram até à borda direita; os pais de vidro mantêm o padding 4dp e os alvos 40dp (a LEI DE OURO — não encolhem) |

### 2.2 O achado (o que a remoção mudou)

O gizmo do canto era o ÚLTIMO elemento do lado direito do viewport: com ele
fora, a fila undo/redo/save/⋯ e a legenda da ferramenta ativa deixam de
competir por largura em ecrãs estreitos (o topFits agora mede até à borda).
O RAIL continua a ser a ÚNICA barra de ferramentas do viewport — «numa só
barra», literal.

### 2.3 Os pins (tests/test_012c_chrome.cpp)

1. **menu_scroll** — o contentH real do menu (15 itens + 6 cabeçalhos =
   896dp) num ecrã 720 → h=644 < contentH, maxOffset=252 > 0, o fundo do
   sheet ≤ 688 (o topo da tab bar); o ecrã alto (1400) cabe inteiro com
   maxOffset 0; o retrato 336 idem (h ≤ 304).
2. **menu_ancorado** — y = ay+8 (o clamp não ativa com a âncora de 20dp);
   a âncora baixa (600dp) ativa o clamp do fundo (y = 720−644−32); o clamp
   horizontal com a âncora à direita extrema; sem âncora → centrado.
3. **topbar_3_zonas** — esquerda ≤ centro ≤ direita em {1600, 1536, 776,
   800}; o gear ancorado à borda; as tabs contíguas e iguais; a degradação
   480dp tira o NOME e as zonas continuam sem tocar.
4. **mover_unico** — o rail 5 ferramentas (o Mover é o rail[1]); o
   gizmoBtn/gizmoPanel ZERO; a fila e os pais DENTRO do rect; o canto
   sub-mínimo limpo.

### 2.4 As mutações (coladas)

- **M-C2a** — o menu cortado de volta (`menuSheetLayout` devolve
  h=contentH sempre): **menu_scroll VERMELHA (14 EXPECTs** — «m.h <
  contentH» e o teto 644 morrem) e **menu_ancorado VERMELHA (8** — a
  mesma função). topbar_3_zonas e mover_unico VERDES — o isolamento das
  irmãs provado. Reposto → verde.
- **M-C2b** — o segundo Mover volta (gizmoBtn/gizmoPanel repostos no
  layout): **mover_unico VERMELHA (6 EXPECTs** — os rects ZERO deixam de
  ser zero). As irmãs verdes. Reposto → verde.

### 2.5 Recalibrações legítimas (com nota no sítio)

1. O canto direito deixou de ter alvo: test_sentinels (regress_chrome_passo3:
   os alvos N−2, o ltiny, a escada 140/72dp, a coerência do rect) e
   c33_virtual (FASEs 13.7/14.1/G2/D21: o canto limpo é o gizmoBtn ZERO).
2. test_wiring090 ×3 testes (o vpchrome g11, a topbar g29, a fila N−2).
3. test_timeline — a strip antiga (ÓRFÃ desde a 0.9.0: a timeline real
   vive no drawer) vive no mundo do wrapper 300-fixo; o teste passa a
   usar o MESMO raw nos painéis (a hier 20% de 1600 = 320 ultrapassava o
   300 do wrapper e o «overlap» era um artefacto de dois mundos).
4. test_uieditor — o tap do + vem da FONTE ÚNICA hierHeaderRects (o
   hardcoded 300−16−14 caía no ⋮ com a hierarquia 20%). O crash do teste
   era o tap a armar o ⋮ em vez do + (o canvas ficava null e o teste
   desreferenciava) — curado pela fonte única, não pelo número.

### 2.6 Verificação

test_core: 0 falhas nos 21 grupos afetados — menu_ · topbar_ · mover_ ·
viewport_ · painel_ · divisor_ · hierarquia_ · regress_ · 012a_ · 012b_ ·
safearea_ · toolbar_ · wiring090_ · uieditor_ · uilayout_ · y2_ · y3_ ·
scenes_ · play_ · scroll_ · timeline_. (O grupo uieditor_ corria ainda
com o binário ASan para caçar o crash do tap — o crash era o teste, não a
app; curado acima.)

### 2.7 Pin C33 (o dono no device)

- [ ] Abrir o menu principal (≡ ou ⋯ do viewport): o scroll funciona (15
      itens), o fundo escurece, nada cortado em baixo — PNG do topo.
- [ ] Confirmar: só há UM botão de mover no viewport (o rail esquerdo); o
      canto superior direito está limpo.
- [ ] A top bar: as 3 zonas não se sobrepõem; o play destaca-se durante o
      play (se a barra estiver visível).

## 3. SUB-FASE C-3 — Tipografia, idioma, superfícies (versionCode 89)

### 3.1 O que o prompt pedia e onde ficou

| Pedido (C-3) | Onde ficou |
|---|---|
| Escala tipográfica fixa 12/14/16/20 | JÁ era a escala do Theme (kFontCaption/Body/Section/Screen desde a 0.9.6); o pin `escala_tipografica` SCANEA os fontes de UI (o stripComments do gate ui_vocab) e aferra que nenhum literal fora do conjunto existe |
| «Exportar Downloads» → «Exportar para Downloads» | EditorUi.cpp (a tabela do menu) + o pin `idioma_consistente` (a ausência da forma antiga, a presença da nova) |
| Maiúsculas uniformes: «ÁUDIO» → «Áudio» | Toolbar.cpp (a tab) + o pin `maiusculas_uniformes` («3D»/«UI» ficam — siglas) |
| «TICs 2» → «2 TICs» | BottomPanel.cpp (o chip «FPS · N TICs») + o pin |
| «Perspetiva»/«Global»/«Nós» | JÁ não existem como controlos (a strip morreu no PASSO 3, a vista Nós no D5 — a ordem «se são placeholders, saem» JÁ estava executada); o pin vigia o regresso dos literais |
| Superfícies: 4 tons distintos com borda âmbar | a ESCADA NOVA no Theme: bg #0A0A0C · surface(painel) #131316 α0.80 · card #1B1B20 (NOVO) · elevated #24242A (NOVO); aplicados nos cards reais (browser/diálogos/seletor/teclado) e nos sheets (menus/toast); a border âmbar mantém (#4A3714 α0.60); o pin `superficies_4_tons` aferra os hexes exatos, a distinção e o contraste |

### 3.2 As decisões (documentadas)

1. O poço #08080A da Y-1 é SUPERADO pelo fundo #0A0A0C do dono (o padrão
   Y-3/D11: a ordem expressa mais recente manda). O launcher fica com o
   seu fundo próprio (outro domínio — os pins y0 não mudam).
2. O vidro α0.80 do painel mantém-se (a decisão de vidro é também do
   dono, Y-1; a C-3 dá o RGB do token — o pin aferra o RGB exato).
3. O toast sobe para O ELEVADO (o topo da escada; o alfa 1 do B-3
   mantém-se — o contraste não depende da cena).
4. O espelho Java tinha os hexes da 0.9.6.10 (desatualizado da Y-1!) —
   alinhado agora com a escada inteira + o hex velho inline trocado pelo
   token (o gate projects_ui_check recalibrado).

### 3.3 As mutações (coladas)

- **M-C3a** — um `fontScale(18.0f)` injetado no draw do título da
  hierarquia: **escala_tipografica VERMELHA (2 EXPECTs** — o scan apanha
  o literal no código stripado: «[escala] 1 literais fora do conjunto»).
  As irmãs verdes. Reposto → verde. (A 1.ª versão do scan tinha o teto
  de 12 chars que saltava TODOS os literais — curada no sítio, o teto é
  18: os literais da casa cabem em 16.)
- **M-C3b** — o cartão igualado ao painel (no Theme E no alias do
  UiContext — os DOIS lados mudam juntos para o static_assert não morrer
  antes do pin): **superficies_4_tons VERMELHA (4 EXPECTs** — os hexes
  exatos e a distinção morrem). As irmãs verdes. Reposto → verde.

### 3.4 Verificação

test_core: 0 falhas nos 23 grupos afetados (ver §14 do
RELATORIO-0.9.6.md) + projects_ui_check VERDE + theme_hex_check VERDE
(0 hex fora do Theme) + ui_vocab_check VERDE + docs-lint VERDE.

### 3.5 Pin C33 (o dono no device)

- [ ] Percorrer todos os ecrãs (editor 3D · modo UI · modo Áudio · o
      drawer ×4 · Settings · Docs · Script · o menu principal · o browser
      · o seletor de cenas): a escala de texto é a mesma família em tudo
      (12/14/16/20), o idioma é PT consistente («Exportar para
      Downloads», «N TICs», «Áudio»), as superfícies têm 4 níveis visíveis
      e a borda âmbar separa os painéis — PNG.

## 4. SUB-FASE C-4 — Seletor de ficheiros + estados vazios (versionCode 90)

### 4.1 O que o prompt pedia e onde ficou

| Pedido (C-4) | Onde ficou (fonte única) |
|---|---|
| Linhas ~48dp | `browserSheetLayout()` (ui/EditorLayout.h — a FONTE ÚNICA da geometria da folha, o draw e os pins partilham — o padrão menuSheetLayout da C-2): rowH = 48dp para raízes/subir/linhas |
| Ícone à esquerda / nome alinhado à esquerda / seta à direita | as linhas do picker redesenhadas (ui/UiEditor.cpp drawFileBrowser): o ÍCONE de função à esquerda (pasta/mesh/imagem/áudio/desconhecido), o nome text1 alinhado à esquerda (os prefixos de texto «mesh:»/«tex:»/«/» morreram — o glifo diz o que era), o ChevronRight SÓ nas diretorias (ficheiros importam, não abrem) |
| Breadcrumb | `browserCrumbs()` NOVA (ui/UiEditor.h — a FONTE ÚNICA dos pares rótulo/prefixo): o 1.º segmento é o rótulo da raiz casada (Raiz/Download/Docs/Camera/Pictures/Music), os restantes os componentes; os ANTECESSORES são ALVOS reais que saltam para a pasta (o main despacha pelo crumbPickOut — o breadcrumb NÃO é enfeite; o segmento ATUAL é texto accent, sem alvo); transbordo mostra os ÚLTIMOS segmentos com «…» |
| «Subir» fixo | a linha de 48dp vive FORA do scroll (entre as raízes e a lista); o pin picker_compacto aferra `upY + rowH ≤ listTop` |
| Pesquisa | o CAMPO 40dp na folha (kBrowserSearchId): o toque abre o teclado in-app no PROPÓSITO 10 (o título «PESQUISAR FICHEIROS»; o commit é puro — `EditorState::browserSearch`); o FILTRO é a função pura `browserFilterEntries()` (case-insensitive sobre o NOME; consulta vazia = a lista completa); o pick devolve o ÍNDICE ORIGINAL (o main despacha sobre o vetor cru — nunca dois mundos) |
| Ocultar pastas ponto | o FILTRO entra no `listDirEntries` (platform/FileApi.cpp — a FONTE ÚNICA da listagem): as DIRETORIAS que começam por ponto SAEM (.thumbnails/.android); os FICHEIROS ponto ficam (a decisão 0.8.5 «todos os ficheiros entram» não se reabre) |
| Safe-area padding | o card 92%/900dp nasce de `browserSheetLayout(sw, sh, safeL, safeT, safeR, safeB, …)` — centrado NA ÁREA ÚTIL, nunca nos insets (o pin mede 3 ecrãs) |
| Vazio do Inspector: mensagem + ação | o «Nada selecionado» ganha o botão «+ criar TIC» (kInspCreateTicId) que abre O MESMO plusMenu de presets do + da hierarquia (ui/EditorUi.cpp drawInspector — o st.plusMenu é o estado que o main já despacha) |
| Vazio dos Assets: botão Importar | mensagem «sem ficheiros no projeto» + o botão IMPORTAR (kAssetsImportId) ANCORADO AO FUNDO da grelha → `Actions::importPressed` → o main abre o NAVEGADOR na pasta Download (a kImportDirs[0]); com FILTRO de pasta ativo não há botão (o convite é limpar o filtro — o chip lá em cima já o faz) |
| Vazio da Hierarquia | JÁ existia (o ícone Scene + o convite que aponta o +); a LADDER do convite extraída para `hierEmptyInvite()` (ui/EditorLayout.h — o draw e o pin partilham); o pin aferra os DOIS lados: o convite menciona o + e o + fica VISÍVEL no cabeçalho (hierHeaderRects) |
| Vazio dos Projetos: G fraco + CTA | o espelho Java (ProjectManagerActivity.java): o glifo da marca no vazio passa de ACCENT a BORDER (o âmbar a 25% — o G não compete com o CTA) e o emptyBox ganha o BOTÃO «Novo projeto» (o MESMO askNewProject do cabeçalho) |
| Ícones únicos por função | a LUPA voltou a ser SÓ «pesquisar» (as texturas do picker usam o Image NOVO — Icons.h/.cpp: moldura+sol+serra); o vazio dos Assets usa o BOX (a função da tab — usava a PASTA, o glifo dos Ficheiros); a FONTE ÚNICA `bottom::emptyStateIcon(tab)` alimenta os três vazios do drawer |

### 4.2 Os achados (os bugs reais sob o pedido)

1. **O gesto de rolar IMPORTAVA o ficheiro debaixo do dedo** — a 1.ª versão
   das linhas novas usava `widgetHit` no loop: dentro de uma região de
   scroll o PRESS dispararia no frame em que o dedo TOCA (o gesto de
   rolar começaria um import). A cura é o padrão da casa (a fila da
   hierarquia): o alvo da linha é SÓ o re-despacho do `scrollTap` — o
   toque dispara no LARGAR e um arrasto é um arrasto.
2. **O claim ADIADO do scroll comia o botão IMPORTAR** — com a lista
   VAZIA a grelha abria MESMO ASSIM a região de scroll (contentH 8dp); o
   endScroll resolve o claim pendente a favor do SCROLL (active_ = o id
   do scroll) e o botão desenhado a seguir nunca capta o gesto. A cura:
   lista vazia = SEM região de scroll (nada para rolar) — o estado vazio
   é dono do próprio gesto.
3. **O IMPORTAR não cabia no drawer de 240dp** — a banda de 72% da grelha
   deixava o botão fora (a guarda não o desenhava). A cura: o botão
   ANCORA-SE AO FUNDO da grelha e a banda da mensagem é o que sobra —
   existe em TODA altura de drawer.
4. **O vazio dos Assets mostrava o glifo dos Ficheiros** — a PASTA no
   vazio dos Assets é o conflito função/glifo que o pin icones_unicos
   vigia (a M-C4b reverte-a e o pin grita).
5. **O achado da REPARAÇÃO (fora da lista da C-4, curado nesta sub-fase)**:
   os commits da C-1..C-3 entraram de uma árvore INCOMPLETA e empurraram
   614 deleções (a engine inteira — core/render/voni/tests/CI) para o
   repo; a árvore completa voltou do e8c9587 (o fecho da 0.12-B) e as
   6 recalibrações que a C-1..C-3 não tinham conseguido ver (os ficheiros
   não estavam na árvore da sessão) entraram: test_toolbar
   (theme_tabela_spec_g_exata), test_sentinels (regress_identidade_
   grafite_ambar + regress_orcamento_gangorra + regress_divisores_
   arrastaveis), test_wiring086 (os rects pela MESMA resolução — o
   padrão test_timeline da C-2), c33_virtual (13.7 ×3, 15.6, 18.1 + os
   VALIDADOR INTEIRO ×4) e a flag fitByDesign no título da hierarquia
   (a elipse da C-1 É o desenho — o padrão PASSO 2 do nome de linha).

### 4.3 Os pins (tests/test_012c_picker.cpp + o gate Java)

1. **picker_compacto** — a linha 48dp a densidade 2.0; o card DENTRO da
   área útil nos 3 ecrãs (landscape+nav, device estreito, retrato); ≤8
   linhas visíveis (50 entradas → a lista encolhe, o scroll revela); a
   ORDEM vertical breadcrumb→pesquisa→raízes→subir→lista; o Subir FIXO
   (`upY + rowH ≤ listTop`); o campo de pesquisa 40dp.
2. **picker_breadcrumb** — os segmentos do caminho da raiz (Raiz/Download/
   sub: rótulos + prefixos; todo o path de segmento é PREFIXO do cwd); o SALTO no draw:
   o toque no 1.º segmento sai pelo crumbPickOut == 1, nenhuma linha
   escolhida e o picker NÃO fecha.
3. **picker_oculta_dot** — a árvore de teste com .thumbnails/.android/
   Download/.nomedia/modelo.obj: a listagem dá 1 diretoria (a normal),
   ZERO pastas ponto, o .obj e o FICHEIRO ponto ficam (a decisão 0.8.5).
4. **empty_inspector** — o painel aberto (pin P2-bis) com a cena vazia:
   antes do toque o plusMenu está fechado; o toque no «+ criar TIC»
   (a MESMA fórmula do draw) ABRE o plusMenu de presets.
5. **empty_assets** — a tab 2 com o catálogo vazio: antes do toque nada;
   o toque no IMPORTAR (ancorado ao fundo — a MESMA fórmula) liga o
   Actions::importPressed.
6. **empty_hierarquia** — a LADDER hierEmptyInvite (os 3 degraus + os 2
   da pesquisa, TODOS a apontar o + quando é o caso) e o + VISÍVEL no
   cabeçalho nos 140/220/300dp (o extremo direito, o ⋮ imediatamente à
   esquerda — a fonte hierHeaderRects).
7. **icones_unicos** — as 4 TABS do drawer pairwise distintas; o vazio
   usa o glifo da PRÓPRIA função (emptyStateIcon: 1→Folder, 2→Box,
   4→Clapper; 2 ≠ 1); as 5 linhas do picker pairwise distintas e a LUPA
   fora delas; o glifo Image NOVO existe com segmentos próprios e o
   iconByName("imagem") resolve.
8. **empty_projetos** (o gate scripts/projects_ui_check.py — o espelho
   Java não roda na suíte host, o padrão da C-3): o G do vazio FRACO
   (BrandView com BORDER) e o CTA ligado ao askNewProject.

### 4.4 As mutações (coladas)

- **M-C4a** — as pastas ocultas de volta (o filtro do listDirEntries
  morto com um `if (false && …)`). Resultado: **picker_oculta_dot
  VERMELHA (2 EXPECTs** — «nDirs == 1u» e «nDotDirs == 0u»; as 6 irmãs
  VERDES — o isolamento provado). Reposto → verde.
- **M-C4b** — o vazio dos Assets volta à PASTA (o glifo dos Ficheiros:
  `case 2: return icons::Icon::Folder;`). Resultado: **icones_unicos
  VERMELHA (2 EXPECTs** — «emptyStateIcon(2) == Box» e
  «emptyStateIcon(2) != emptyStateIcon(1)»; as 6 irmãs VERDES). Reposto
  → verde.

### 4.5 Verificação

- test_core: 0 falhas nos grupos afetados — browser_ · wiring085_ ·
  wiring087_ · 012c_ · uieditor_ · bottom/files (os pins das tabs) ·
  regress_ · wiring090_ (a unicidade dos glifos) · hierarquia_ ·
  inspector_ + os 7 pins novos. (Os 2 flakes conhecidos da sandbox
  correm fora: regress_streaming_paralelo — a corrida latente §7 do
  STATE.md — e sentinela_r039/o bloco 920 — pressão de DISCO/RAM da
  sandbox; o CI é o árbitro, regra §5.)
- c33_virtual: **1005 checks, 0 falhas** — inclui as recalibrações da
  reparação (13.7 hierarquia 20% + os alvos escondidos com o rect ZERO,
  15.6 o thumb 256×143 com o viewport mais estreito, 18.1 o sheet é O
  ELEVADO opaco) e o 12.8b pela FONTE ÚNICA (o tap do par .gltf+.bin
  vem da browserSheetLayout — o re-despacho do scroll).
- theme_hex_check VERDE · ui_vocab_check VERDE · hierarchy_check VERDE ·
  projects_ui_check VERDE (com o 2c novo).

### 4.6 Pin C33 (o dono no device)

- [ ] Abrir o picker (o «importar…» de um seletor ou o IMPORTAR do vazio
      dos Assets): as linhas com ícone/nome/seta, o breadcrumb salta para
      a pasta tocada, o «Subir» fica FIXO enquanto a lista rola, a
      pesquisa filtra (o campo abre o teclado «PESQUISAR FICHEIROS») e
      as pastas .thumbnails/.android não aparecem — PNG da lista.
- [ ] Os 4 vazios: o Inspector aberto sem seleção mostra o «+ criar TIC»
      que abre o menu de presets; os Assets vazios mostram o IMPORTAR que
      abre o navegador; a Hierarquia vazia aponta o + e ele está no
      cabeçalho; os Projetos sem nada mostram o G fraco e o botão «Novo
      projeto» que abre o diálogo do nome — PNG de cada.
- [ ] Os glifos: nenhuma função partilha ícone com outra (a lupa só no
      campo de pesquisa; as texturas têm o glifo próprio).

### 4.7 Limites (P-04)

1. O vazio da CONSOLA não existe (o log vazio mostra as chips e a linha
   de comando — o vazio ali seria fachada; a emptyStateIcon devolve o
   Terminal para o caso 3 mas nada o desenha).
2. A pesquisa do picker filtra a PASTA ABERTA (não é uma busca
   recursiva no armazenamento — o alcance do prompt é a lista; uma busca
   global é outro pedido).
3. O commit do propósito 10 mantém a consulta entre pastas (procurar em
   toda a Download é o uso real); limpar é apagar o texto e confirmar —
   sem botão × próprio (menos um alvo de 8dp).
4. As PNGs do C33 são do device do dono (a sessão local não tem device).
