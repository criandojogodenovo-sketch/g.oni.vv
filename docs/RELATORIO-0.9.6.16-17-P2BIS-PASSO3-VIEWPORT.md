# RELATORIO-0.9.6.16-17 — UI PASSO 2-BIS + PASSO 3 (PAINÉIS · VIEWPORT)

> Campanha UI do dono — os dois trabalhos deste push: o **P2-bis** (as duas
> decisões do dono sobre o PASSO 2, commit próprio `0.9.6.16`) e o
> **PASSO 3 · VIEWPORT** (`0.9.6.17`). A regra da casa: medir, não olhar;
> zero adivinhação; o que não foi verificado fica na §7.

## 1. P2-BIS — AS DUAS DECISÕES DO DONO (0.9.6.16, commit próprio)

### 1.1 Fixar aberto (o pin do inspector)

A interação decidida pelo dono: **tocar no ícone do trilho abre E fixa
(persistido no layout.json); a seta de recolher desfaz o pin e fecha. Sem
long-press. Enquanto fixado, o painel mantém-se aberto mesmo sem seleção.**

- `EditorState.inspPinned` NOVO, PERSISTE (`inspPinned=0/1` no layout.json;
  ausente = false — o formato é retrocompatível; o «Repor layout» limpa-o).
- `editor::inspectorCollapsed()` — a FONTE ÚNICA do colapso — passa a
  mandar o pin: `!pinned && sem seleção && sem multi-seleção`. Fixado, o
  painel fica aberto MESMO sem seleção. Com seleção, a regra PASSO 2 manda
  (a seta limpa o pin; o painel fecha ao limpar a seleção — a leitura
  mínima e sem contradição da regra do dono, documentada na §7).
- O ícone do trilho (que era INFORMATIVO — sem widgetHit) é agora o toggle:
  a célula de toque é o topo do trilho, 32dp de largura (o elemento 32dp da
  spec PASSO 2) × 40dp de altura (a LEI DE OURO no toque), piso
  `kFieldFloorDp` — a mesma classe das tabs de baixo de 32dp. Id 7432.
- A SETA DE RECOLHER vive numa linha PRÓPRIA de 28dp sob o cabeçalho
  (id 7433, piso `kHeadFloorDp`), só existe ENQUANTO fixado. **A decisão de
  posição é medida**: os rects REAIS do device (extraídos do PNG do
  harness) mostram a chip «Inspector» a começar a 34,5dp do painel de
  180dp — a 1ª linha do cabeçalho não tem espaço para mais um alvo sem
  sobreposição (o validador teria apanhado; a prova está no commit).
- ACHADO honesto (pré-existente, FORA do scope, documentado): a título
  «Inspector» do cabeçalho SOBREPÕE-SE à chip «Inspector» no painel de
  180dp («Insppector» garatuja no PNG do device) — a sobreposição
  label×chip não é apanhada pelo validador (só interativo×interativo é
  regra). Fica registado para o dono decidir (§7).

### 1.2 Consola ≥60%

A regra decidida pelo dono: **com a tab Consola ativa, a lista de log ocupa
≥60% da altura de conteúdo do drawer (chips+extras ≤40%). Sem exceções:
drawer pequeno = chips 28dp + lista no resto.**

- `bottom::conCmdVisible(contentHdp)` NOVO — a decisão PURA (em dp,
  densidade-invariante, fonte ÚNICA partilhada pelo draw e pelos testes):
  o CAMPO DE COMANDO só existe se a lista com ele mantiver ≥60%
  (`kConListMinPct`). O drawer pequeno vive com chips 28dp + a lista no
  resto — exatamente as palavras do dono.
- O topo compacta 4+28+8 → **2+28+2 = 32dp** (`kConChipPad`/`kConListGap`
  NOMEADOS; a linha separadora vive dentro do intervalo; os chips ficam
  28dp intactos — o piso do dono).
- O campo de comando mantém os seus 40dp (a LEI DE OURO: toque 40 — nunca
  encolhe) e 4dp de folga de baixo (`kConCmdH`/`kConCmdPad`).
- A fronteira: conteúdo ≥ ~190dp mantém o campo; abaixo, o campo sai.
  **device (80dp de conteúdo) e harness @2.0 (88dp): sem campo**; drawer
  240 @1.0 (216dp): com campo (lista 64,8%).
- ACHADO: no device o drawer capado a 35% deixava a lista com ALTURA
  NEGATIVA — o campo de comando desenhava-se POR CIMA das chips e a
  consola não tinha lista nenhuma. É exatamente o que a regra do dono cura.

### 1.3 TABELA DE MEDIDAS P2-BIS (device RMX3624, 776×336dp @2.0, insets 12dp)

| Região | px | dp | % do conteúdo do drawer |
|---|---|---|---|
| Drawer efetivo (o teto 35% manda) | 208 | 104 | 35,0% do content |
| Conteúdo do drawer (− pega 24) | 160 | 80 | 100% |
| Bloco do topo (pad 2 + chips 28 + intervalo 2) | 64 | 32 | 40,0% |
| LISTA DE LOG | 96 | 48 | **60,0%** (o piso do dono — exato) |
| Campo de comando | — | — | **AUSENTE** (a regra: 80 < 190) |
| Célula do pin no trilho (toque) | 64×80 | 32×40 | — |
| Linha «recolher» | 360×56 | 180×28 | — |

Harness @1.0 (drawer 240 → conteúdo 216dp): campo PRESENTE; lista
140dp = 64,8% ✓. Harness @2.0 (drawer 112 → conteúdo 88dp): sem campo;
lista 56dp = 63,6% ✓ (medido no c33: 112px de 176px).

## 2. PASSO 3 · VIEWPORT (0.9.6.17)

A spec confirmada pelo dono: **controlos ≤10% da área do viewport a 60%
alfa · rail esquerdo · [+] 40dp redondo · gizmo 40dp** — mais a spec de
origem: desfazer/refazer/guardar/⋯ no topo-esquerdo, legenda da
ferramenta, PROIBIDO barra full-width sobre a cena, PROIBIDO sobreposição.

### 2.1 O que mudou (a geometria toda do chrome)

| Elemento | ANTES (PASSO 2) | DEPOIS (PASSO 3) |
|---|---|---|
| Strip [Cena][Perspetiva][Global] | faixa FULL-WIDTH de 40dp no topo | **REMOVIDA** (a barra full-width é proibida pela spec; os chips eram SEM FUNÇÃO desde o inventário do PASSO 0) |
| Ferramentas (Sel/Mov/Rod/Esc/Ímã) | toolbar HORIZONTAL no fundo | **RAIL ESQUERDO vertical** (col-major; a degradação em colunas do stack antigo — viewports baixos dividem) |
| Desfazer/refazer/guardar | stack vertical esquerdo (com dup/paste) | **FILA DO TOPO-esquerdo** + o **⋯** novo (abre o menu de ficheiro ANCORADO a ele — `EditorState.menuAx/menuAy`); dup/paste saíram da viewport (as ações vivem no menu ⋯ itens 9/10, o caminho da 0.9.6.10) |
| [+] Adicionar TIC | chip de 56dp | **40dp REDONDO** (raio 20) no canto inferior direito |
| Gizmo | sem atalho | **40dp no canto superior direito** — o atalho mostrar/esconder o gizmo: a transição `selectMode↔gizmo` que JÁ existia (zero lógica nova; a interpretação do «gizmo 40dp», documentada para o dono vetar na §7) |
| Legenda | acima da toolbar | à DIREITA do rail, sob a fila do topo; some na degradação |
| Alfa | opaco (vidro α0.86 do tema) | **60%** (`kChromeAlpha` + `chromeCol` públicos no header — os pais, os chips, os ícones e a legenda; os glifos desativados ficam nos 0.4 de sempre) |

### 2.2 A degradação (a regra da casa: o que não cabe ESCONDE, nunca transborda)

- O rail 1 coluna precisa 232dp de altura; viewports baixos dividem em 2
  colunas (device com o drawer aberto) e a fila do topo DESCE para baixo
  do rail quando não cabe ao lado do gizmo; se nem assim cabe, a fila
  ESCONDE (os cantos ficam — cabem sempre no piso `kViewportMinH`).
- A legenda só existe no layout primário (rail 1 coluna) com largura de
  sobra.

### 2.3 TABELA DE MEDIDAS PASSO 3 (o script `passo3_medidas` — grelha 2px, o método do PASSO 0)

**Device RMX3624 (1600×720 @2.0, insets 12dp):**

| Estado | Viewport central | Cobertura pelo DESENHO (chips 32 + círculos + legenda) | Cena visível |
|---|---|---|---|
| Drawer fechado + TRILHO (o arranque) | 1228×560 px (614×280dp) = 79,1% larg | **7,7%** ✓ ≤10% | **92,3%** ✓ ≥85% |
| Drawer fechado + seleção (insp. 22%) | 932×560 px (466×280dp) = 60,1% larg | **10,2%** (marginal) | **89,8%** ✓ |
| Drawer aberto (teto 35%) | 932×352 px (466×176dp) = 60,1% larg | **15,1%** | **84,9%** (marginal) |

**Harness-C33 (1536×720 @1.0, insets 0):**

| Estado | Viewport central | Desenho | Cena visível |
|---|---|---|---|
| Trilho | 1228×652 px = 79,9% larg | 1,7% ✓ | 98,3% ✓ |
| Inspector 22% | 1000×652 px = 65,1% larg | 2,0% ✓ | 98,0% ✓ |
| Drawer aberto | 1000×412 px = 65,1% larg | 3,2% ✓ | 96,8% ✓ |

**A leitura honesta dos critérios:**
- O estado que o dono vê AO ABRIR (trilho) cumpre os dois: desenho 7,7% ✓
  e cena 92,3% ✓.
- Com o inspector aberto no device, o desenho fica a 10,2% (a margem de
  0,2pp vem dos círculos dos cantos contados como quadrados na grelha; os
  alvos de TOQUE são invisíveis).
- Com o DRAWER aberto no device (o viewport encolhe a 176dp de altura), o
  desenho sobe a 15,1%: **os 11 alvos de 40dp são o piso da LEI DE OURO**
  — não há encaixe de 10% num viewport de 466×176dp com 11 controlos. A
  compensação é a própria spec: tudo a 60% de alfa (a cena LÊ-SE através
  dos controlos). A decisão final é do dono (§7): aceitar, encolher alvos
  (viola a lei de ouro) ou esconder a fila do topo com o drawer aberto.

### 2.4 Medidas dos elementos (dp)

| Elemento | px @2.0 | dp | Nota |
|---|---|---|---|
| Alvo dos botões do rail / da fila / dos cantos | 80×80 | 40×40 | a LEI DE OURO (toque 40) |
| Desenho dos chips (rail/fila) | 64×64 | 32×32 | desenho 32 dentro do alvo 40 |
| [+] e gizmo (círculo/chip) | 80×80 | 40×40 | o [+] é redondo (raio 20dp) |
| Passo entre alvos | 96 | 48 | 40+8 (nunca se pisam) |
| Rail (1 coluna, altura) | 464 | 232 | 5×40 + 4×8 |
| Fila do topo (largura) | 368 | 184 | 4×40 + 3×8 |
| Legenda | — | 12sp caption | text2 a 60% |
| Alfa do chrome | — | 60% | `kChromeAlpha` (pinado na R-034) |

## 3. PROVAS (mutações vermelho→verde)

### P2-bis (0.9.6.16)
- M1 `conCmdVisible` morto (o campo sempre) → c33 2 falhas (o campo devia
  estar fora + a lista <60%) + test_core 5 (R-034); **M2** o parse ignora o
  pin → R-034; **M3** o predicado sem o pin → R-034 + c33 2 falhas
  («fixado, o painel fica aberto» + «o botão do trilho saiu»). Repostas →
  verde (cp para backups; NUNCA git checkout — a lição A2-c).

### PASSO 3 (0.9.6.17)
- M-P3-1 a degradação morta (a fila do topo desenha-se sempre no topo) →
  `regress_divisores_arrastaveis` (o ltiny) + `regress_texto_strip_campo`
  (a escada 140dp: pai de 192dp a atravessar a largura + alvos fora do
  rect) VERMELHOS; **M-P3-2** a alfa morta (`chromeCol` sem o
  multiplicador) → R-034(4) VERMELHA (o pin do composto 0,60). Repostas →
  verde.

## 4. SUÍTES E GATES

- test_core: **854 testes, 0 falhas** (853 + o pin da alfa).
- c33_virtual: **486 checks, 0 falhas** (487 do P2-bis − os 2 checks de
  pixel substituídos por 1 de registo; + a FASE 14.1 nova).
- gates locais verdes: hierarchy-check (o contrato em sincronia — o gate
  APANHOU o contrato desatualizado no caminho e foi atualizado NO MESMO
  commit, P-08), docs-lint 58 ficheiros, theme-hex 0.

## 5. EVIDÊNCIAS P-05 (PNG)

| Ficheiro (docs/) | O que mostra |
|---|---|
| `passo2-device-trilho-32dp.png` | ANTES — o viewport com a strip full-width e a toolbar no fundo (o chrome do PASSO 2) |
| `passo2bis-device-pin-inspector.png` | P2-bis — o painel fixado com a seta «recolher» (id 7433 no registo) |
| `passo2bis-device-consola-60.png` | P2-bis — a consola SEM o campo, a lista a 63,6% |
| `passo3-device-viewport-rail.png` | DEPOIS — o rail esquerdo, a fila do topo, o gizmo 40dp, o [+] redondo, a legenda «Mover», a strip morta, o vidro a 60% |
| `passo3-harness-viewport-rail.png` | DEPOIS — o MESMO chrome no harness @1.0 (a invariância da escala) |

## 6. CHECKLIST DO DEVICE (P-07 — POR PREENCHER PELO DONO; o sign-off do PASSO 2 e do PASSO 3 faz-se JUNTO, com o APK do commit final)

- [ ] P2-bis: tocar no ÍCONE do trilho abre o inspector E fixa; fechar a
      app e voltar: o painel continua aberto (o layout.json persistiu)
- [ ] P2-bis: a seta «recolher» desfaz o pin e o trilho volta (sem seleção)
- [ ] P2-bis: a consola (tab Consola) tem a lista a ocupar ≥60% do drawer;
      o campo de comando não aparece no drawer pequeno (o device)
- [ ] PASSO 3: nada atravessa a largura do viewport (a strip [Cena]/
      [Perspetiva]/[Global] JÁ NÃO EXISTE)
- [ ] PASSO 3: as ferramentas (Selecionar/Mover/Rodar/Escalar/Íman) vivem
      no RAIL ESQUERDO; a ferramenta ativa tem o nome à direita do rail
- [ ] PASSO 3: desfazer/refazer/guardar/⋯ no topo-esquerdo; o ⋯ abre o
      menu de ficheiro ancorado a ele; Duplicar/Colar continuam a
      funcionar (dentro do menu)
- [ ] PASSO 3: o [+] é 40dp redondo no fundo-direito e abre os presets
- [ ] PASSO 3: o gizmo 40dp no topo-direito alterna o gizmo (mostrar/
      esconder) e a cena lê-se ATRAVÉS dos controlos (o 60%)
- [ ] PASSO 3: com o drawer aberto nada do chrome pisa o drawer
- [ ] CI verde (o run deste push) + 60fps no device

## 7. NÃO VERIFICADO / PARA O DONO DECIDIR (P-04, zero adivinhação)

1. **A leitura do «fecha» da seta** — implementado: a seta DESFAZ o pin;
   o painel fecha IMEDIATAMENTE só quando não há seleção (a regra PASSO 2
   do dono: com seleção o painel fica aberto). Se o dono quiser que a
   seta feche MESMO com seleção (um estado «fechado pelo utilizador» que
   sobrevive à seleção), é um estado novo — pedir.
2. **A interpretação do «gizmo 40dp»** — implementado como o atalho
   mostrar/esconder o gizmo (a transição `selectMode↔gizmo` que já
   existia; o ícone espelha a ferramenta ativa). Se o dono quiser outro
   comportamento (ciclo de modo Mover→Rodar→Escalar, espaço local/global,
   orientação), é lógica nova — pedir.
3. **O critério ≤10% com o drawer aberto no device** — 15,1% de desenho
   (a §2.3): os 11 alvos de 40dp não encaixam em 466×176dp. Opções: (a)
   aceitar (a cena lê-se através do 60%); (b) encolher alvos (viola a lei
   de ouro); (c) esconder a fila do topo com o drawer aberto. Decidir.
4. **A título «Inspector» sobrepõe-se à chip «Inspector»** no painel de
   180dp (o PNG do device mostra o garatuja; pré-existe ao PASSO 2; a
   sobreposição label×chip não é regra do validador). Fora do scope
   destes passos — pedir se quiser a correção (ex.: a título só quando o
   painel é largo, ou o truncamento honesto).
5. **O critério ≤10% medido no DESENHO** — a métrica conta o que OBSTRUI
   (chips 32 + círculos + legenda); os alvos de toque (40dp, invisíveis)
   contados à parte (2,2–21,5% conforme o estado). Se o dono quiser a
   métrica pelos ALVOS, o critério é impossível com a lei de ouro (§2.3).
6. **O sign-off P-07** — a checklist §6 por preencher no device (o harness
   virtual confirma a matemática; o dedo no ecrã é do dono), com o APK do
   commit final.
7. **CI** — o push dispara o release.yml; o estado final fica no commit (a
   regra da casa: o relatório não declara verde sem o run).
