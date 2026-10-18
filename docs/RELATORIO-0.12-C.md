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
