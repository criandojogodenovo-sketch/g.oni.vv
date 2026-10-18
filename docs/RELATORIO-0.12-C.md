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
