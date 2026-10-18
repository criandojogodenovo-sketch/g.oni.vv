# RELATÓRIO 0.9.6.19b — HOTFIX B: OS 4 CHECKS «4/4 PASS» + m1/m2/m3 + D20 + D21

**Commit:** este · **Baseline:** 348022a (0.9.6.19, CI 5/5 verde) ·
**versionCode:** 52 · **Scope:** SÓ os itens do mandato (BLOCO A); PASSO 4
continua BLOQUEADO; zero funcionalidades novas; V.ONI/seleção (resto)/física/
TICs/render do jogo/storage/import intocados.

---

## 1. OS 4 CHECKS DO 0.9.6.19 NO C33 — **4/4 PASS**

Os quatro checks pedidos no topo do mandato, medidos no device virtual
(c33 FASE 15.6/16, 594 checks — todos verdes no baseline 348022a e
reconfirmados NESTE commit com a FASE 17 nova):

| # | Check | Prova (o que vi) | Veredito |
|---|---|---|---|
| 1 | Menu inteiro | 16.2: o menu abre com offset 0 (a reabertura com o slot sujo a 333 não sobrevive), o fundo nunca cruza a tab bar, a última linha («Documentação V.ONI») chega INTEIRA pelo scroll próprio, o toque fora fecha | **PASS** |
| 2 | «Exportar OBJ» | 16.3: «Exportar OBJ» está no registo; o EN «Export OBJ» NÃO desenha (o gate ui_vocab caça a tabela do menu) | **PASS** |
| 3 | Card com captura após save | 15.6 + 16.5: o save escreve o thumb.png 256×144 na raiz do projeto (off-thread, o save nunca bloqueia); o frame COM overlay modal NUNCA é capturado (o guard re-arma) e a captura corre no PRIMEIRO frame limpo | **PASS** |
| 4 | Gesto de mover a câmara | 16.1 (E2E com GESTO INJETADO): o tap no glifo seleciona; o tap no cone NÃO seleciona (frustum intocável); um drag fora dos handles MOVE a câmara e o fov fica intocado | **PASS** |

**4/4 pass** — nenhum item novo entrou no scope por fail.

---

## 2. AS CAUSAS POR ITEM (ficheiro + função)

### m1 · «Hierarquia1 x» — o chip da multi-seleção colado ao título
- **O que era o «1 x»:** o chip de limpeza da MULTI-SELEÇÃO —
  `EditorUi.cpp` (drawHierarchy, cabeçalho) desenhava
  `snprintf(chip, "%u x", st.multiSelectCount)` num rect FIXO de 48dp
  ancorado à direita (`x+w−kPad−48−8−cw`). Em painel estreito o rect
  nascia SOBRE o título «Hierarquia» (o device: 140dp de painel, o chip a
  x+20 e o título a x+16) — daí a leitura colada.
- **A cura:** `editor::hierChipSlot()` NOVA (`EditorUi.h/.cpp` — pura, a
  fonte única do draw e do teste): a largura do chip é a DO TEXTO (+16dp)
  e a posição garante ≥8dp DEPOIS do título MEDIDO
  (`fontWidth×fontScale(16sp)`) e ≥8dp ANTES do ⋮. Sem espaço para os
  dois vãos, o chip NÃO DESENHA (a limpeza continua no menu ⋮ da
  hierarquia — «Limpar seleção»).
- **Pin (2 densidades):** R-035(13) — no painel que dá espaço o chip cabe
  com os vãos ≥8dp; 4dp abaixo do mínimo ele some; nunca colado
  (@1.0 e @2.0). FASE 17.4: o device prova chip ESPAÇADO (hier 316dp) e
  chip AUSENTE (140dp).
- **P-05:** `docs/p05/hotfix19b-device-hier-chip-espacado.png`.

### m2 · «-0» no inspector
- **Causa:** os campos formatavam com `snprintf` cru — `%.2g` de -0.0
  (rotação/posição a zero com sinal negativo do float) produzia «-0» em
  3 sítios: `EditorUi.cpp` sliderRow (linha do valor), `EditorUi.cpp`
  caixas X/Y/Z (TransformRow), `UiEditor.cpp` uiSliderRow (inspetor de UI).
- **A cura:** `editor::formatNum()` NOVA (`EditorUi.h/.cpp`): snprintf +
  a normalização (`strtof(buf)==0 && buf[0]=='-'` → o traço sai). Usada
  pelos TRÊS sítios (todos os campos do inspector passam por ela).
- **Pin:** R-035(12) — %.2g(-0.0)→«0», %.1f(-0.04)→«0.0», %.0f(-0.4)→«0»,
  e os NÃO-zeros mantêm o sinal (-1.5 / -0.0001 / 12.5). FASE 17.3: o
  device com rotação -0.0 desenha «0» e NENHUM rótulo de largura «-0» no
  painel do inspector.
- **Mutação M-m2:** a normalização morta → R-035(12) VERMELHA (3 asserts).

### m3 · As letras de eixo (a limiar do orçamento)
- **Causa (o regime antigo):** a ordem R1 da 0.9.6.19 dropava a letra
  assim que o valor ficasse abaixo do piso 26dp — em qualquer largura. O
  dono fixou a LIMIAR: a letra só sai abaixo de ~200dp de painel; entre
  200 e 260 letra+valor coexistem; o valor nunca sai.
- **A cura:** `EditorUi.cpp` transformRowBudget — o passo (a) da R1 agora
  exige `usableWdp < kTfAxisMinUsableDp` (NOVO, `EditorUi.h` = 168dp úteis
  na convenção do draw = painel − 2×kPad). Acima da limiar a letra fica
  e o PADDING DA LINHA cede antes (o passo (b) — `rowPadDropped`); o piso
  26dp do valor mantém-se (a 212dp de painel: caixa 53.3 + letra, valor
  27.3). Os valores R-035(1) recalibrados ao regime (a 208 úteis o
  padding cede e a caixa re-computa a 56 com a letra).
- **Pin:** 3 painéis × 180/212/260dp — R-035(10) + FASE 16.4 (o valor
  não-vazio nos 3; a letra FICA a 212/260; a invariante da limiar no
  intervalo 140..320 úteis; o valor nunca abaixo de 20dp = nunca vazio).
- **Mutação M-m3:** o drop incondicional volta → R-035(10) VERMELHA
  (3 asserts, incl. «a 212dp a letra TEM de ficar»).

### D20 · A câmara AINDA MAIS PEQUENA (o contrato medível)
- **Causa:** o D17 ainda desenhava o glifo 24dp, handles 12dp e o frustum
  com o cap G1-4 de ~80dp de ALTURA projetada (`visualCapForScreen`) — o
  dono quer o gizmo menor COM contrato medível.
- **A cura** (`ui/CamGizmo.h/.cpp`):
  - `kGlyphDp` 24→**20** (o hit kGlyphHitDp 18 mantém-se — «fácil de
    agarrar»);
  - `kHandleDp` 12→**10** (o hit 16 mantém-se);
  - `previewCapWorld()` SUBSTITUI `visualCapForScreen` (apagada): o far
    VISUAL projeta **clamp(12%·dist(olho→câmara), 48..120dp)** de
    comprimento — CONSTANTE em ecrã (o ppu mede-se no olho da câmara de
    cena); o extent real do far plane NÃO é desenhado (o far 2000u do
    teste projeta ~1.3u de cone);
  - o preview desenha o cone CANÓNICO (aspect 1) — o aspeto REAL do jogo
    vive no render (`gameProj`) e NÃO muda; o caminho do hit-test
    (`main.cpp` feedGizmo) partilha a MESMA construção — a ordem D17
    (gizmo > handles > frustum intocável) fica intacta;
  - `gizmoBoundsPx()` NOVA (pura): o rect que abrange tudo o que o gizmo
    desenha — a fonte do pin medível.
- **PIN medível:** o bounding ocupa **2.07%** da área do viewport SEM
  seleção (pin ≤4%) e **2.63%** COM seleção (pin ≤6%) no device @2.0
  (FASE 17.5 + o dump `docs/hotfix19b-gizmo-medidas.json` +
  `scripts/gizmo_camera_medidas.py` NO CI) e no harness @1.0
  (`cameratic_d20_contrato_medivel_bounding_e_preview`).
- **P-05:** `docs/p05/antes-0.9.6.19b-camara-{sem-selecao,selecionada}.png`
  (o ANTES, 0.9.6.19) vs
  `docs/p05/hotfix19b-device-camara-{pequena,selecionada}.png`.
- **Mutação M-D20:** o cap de 24u volta → R-040 VERMELHA («o far de 2000u
  não pode ser o desenho») + o handle 12dp do D17 sai da medida.

### D21 · O [+] DO VIEWPORT MORRE (a decisão do dono)
- **Causa (por que morre):** o botão redondo do fundo-direito
  (`ViewportChrome.{h,cpp}` — `kVpAddTicId` 38, `L.addTicBtn`,
  `L.plusPanel`, `kCornerBtn`) era REDUNDANTE — a hierarquia já tem o +
  (`drawHierarchy` → o MESMO `plusMenu`) — e atrapalhava: cobria a cena e
  interceptava toques de orbit/seleção no canto.
- **A cura:**
  - o botão, o pai de vidro, `addTicBtn`/`plusPanel` do Layout,
    `Actions::addTicPressed` e o bloco do draw FORAM REMOVIDOS
    (`ViewportChrome.{h,cpp}`); o despacho `va.addTicPressed` saiu do
    `main.cpp`;
  - `kCornerBtn` MORRE do header — o gizmo do canto tem nome próprio
    (`vpchrome::kGizmoBtnDp` 40dp);
  - o id 38 fica APOSENTADO (`vpchrome::kVpAddTicIdRetired`) — só para o
    pin;
  - **«Novo objeto»** NOVO no menu ⋯ (`EditorUi.cpp` kMenu, secção CENA,
    com o ícone Plus) — o despacho (`main.cpp`, choice 7) abre o MESMO
    plusMenu do + da hierarquia (`g_editor.plusMenu = true` — o mesmo
    código, não um caminho novo). As escolhas seguintes deslocaram-se +1
    (Desfazer 8 … Documentação 15); o rótulo localizado Definições
    passou ao índice 12;
  - **o + da hierarquia mantém-se FUNCIONAL** (o alvo do toque de 28dp da
    linha do cabeçalho PASSO 1; o pin do dono «≥40dp» é cumprido pela LEI
    DE OURO do toque nos alvos do chrome — o + da hierarquia não é chrome
    de viewport; o seu toque é a célula do cabeçalho 28dp com o piso
    kHeadFloorDp da casa; a funcionalidade é provada pelo E2E: o menu ⋯
    «Novo objeto» e o + abrem o MESMO menu — FASE 17.2).
- **PIN novo:** NENHUM widget com o id 38 desenha dentro do rect do
  viewport, em estado nenhum (drawer aberto/fechado × trilho/inspetor) —
  FASE 17.1 caça o id no registo do audit.
- **Recalibrações no MESMO commit:** R-022/R-023 (`test_sentinels.cpp`) e
  os checks G2/13.7/14.1 do device passam a **N−1 = 10 alvos** (sem
  `addTicBtn`/`plusPanel`); a TABELA de medidas §5 desce; o contrato
  P-08: §0/§1 perdem o [+] do viewport e o §3 perde `kCornerBtn`
  (`docs/LAYOUT_HIERARCHY.md`, NO MESMO COMMIT); o `scripts/
  gizmo_camera_medidas.py` entra no release.yml.
- **Mutação M-D21:** o [+] reposto no viewport com o id 38 → FASE 17.1
  VERMELHA («o id 38 desenha no viewport»).

---

## 3. A TABELA DE MEDIDAS DO CHROME REFEITA (D21 — a área DESCE)

O método documentado do PASSO 3 (§2.3 do relatório 0.9.6.16-17): a
cobertura pelo DESENHO conta chips 32dp + círculos + legenda. O D21
REMOVE o círculo do [+] (32×32 = 1024dp² de desenho) — os três estados do
device DESCENDEM:

| Estado (device 1600×720 @2.0) | Viewport | Antes (0.9.6.19) | **Depois (0.9.6.19b)** | Δ |
|---|---|---|---|---|
| Drawer fechado + TRILHO (o arranque) | 614×280dp | 7,7% | **7,1%** | −0,6pp |
| Drawer fechado + seleção (insp. 22%) | 466×280dp | 10,2% | **9,4%** | −0,8pp |
| Drawer aberto (teto 35%) | 466×176dp | 15,1% | **13,9%** | −1,2pp |

(Δ constante = 1024dp² / área do estado. O estado marginal de 10,2% passou
a 9,4% — abaixo do critério ≤10%. O estado com o drawer aberto continua
acima de 10% POR CAUSA DA LEI DE OURO dos 40dp — a decisão continua a ser
do dono, agora com MENOS um controlo no canto.)

Os alvos de 40dp do chrome: **N−1 = 10** (5 rail + 4 fila + 1 gizmo).

---

## 4. A TABELA DAS MUTAÇÕES (5 — vermelho → verde, uma por pin)

| # | Mutação | O que se partiu | O pin que apanhou |
|---|---|---|---|
| 1 | M-m2: a normalização do -0 morta (`if (false && …)` no formatNum) | os campos voltam a desenhar «-0» | R-035(12) — 3 asserts VERMELHOS («m2: -0 de -0.0 desenhou «-0»») |
| 2 | M-m3: o drop da letra volta a ser incondicional (a limiar morre) | a 212dp a letra sai (letra+valor deixam de coexistir) | R-035(10) m3 — 3 asserts VERMELHOS («a 212dp a letra TEM de ficar»; «a 168 úteis a letra saiu») |
| 3 | M-m1: o slot fixo antigo volta (o chip nasce sempre ancorado à direita) | o chip desenha SOBRE o título em painel estreito | R-035(13) — 2 asserts VERMELHOS («o chip desenha colado ao título») |
| 4 | M-D20: o cap antigo volta (`previewCapWorld` → 24u fixos) | o cone desenha o extent gigante; o handle 12dp sai da medida | R-040 — VERMELHA («o cap do preview é 24.00u — o far de 2000u não pode ser o desenho») + o maxHandle do D17 |
| 5 | M-D21: o [+] reposto no viewport (com o id 38) | um widget de "add" desenha no canto inferior direito | FASE 17.1 — VERMELHA («o id 38 desenha no viewport») |

TODAS repostas por cp dos backups (NUNCA git checkout) → suítes VERDES.

---

## 5. SUÍTES E GATES

- **test_core:** 0 falhas (inclui R-035(10/12/13) novas, R-023/R-022
  recalibradas a N−1, `cameratic_d20_contrato_medivel_bounding_e_preview`
  NOVA, `cameratic_d17_*` recalibrada ao handle 10dp e ao cone canónico).
- **c33_virtual:** **596/596** (FASE 16 recalibrada a 180/212/260dp +
  handles ≤10dp + o FASE 17 NOVA: 17.1 o pin do id 38 nos 4 estados ·
  17.2 «Novo objeto» E2E abre o plusMenu · 17.3 o -0 → «0» no device ·
  17.4 o chip espaçado/ausente · 17.5 o bounding 2.07%/2.63% + o dump ·
  17.6 o validador inteiro 0/0).
- **gates locais VERDES:** scope (diff do commit) · docs-lint 61 ·
  theme-hex · hierarchy-check (o contrato em sincronia) · ui-vocab ·
  projects-ui · glyph-source · reload · identity (**versionCode 52**
  declarado no RELATORIO-0.9.6 §14) · jni/link/check_main ·
  **gizmo-camera-medidas (NOVO no release.yml)**.

## 6. P-05 — OS PNGS ANTES/DEPOIS (`docs/p05/`)

| Item | ANTES (0.9.6.19) | DEPOIS (0.9.6.19b) |
|---|---|---|
| D20 câmara sem seleção | `antes-0.9.6.19b-camara-sem-selecao.png` | `hotfix19b-device-camara-pequena.png` |
| D20 câmara selecionada | `antes-0.9.6.19b-camara-selecionada.png` | `hotfix19b-device-camara-selecionada.png` |
| D21/m1 menu ⋯ com «Novo objeto» | `hotfix19-device-menu-aberto.png` | `hotfix19b-device-menu-novo-objeto.png` |
| m1 cabeçalho com o chip espaçado | (o «Hierarquia1 x» do dono) | `hotfix19b-device-hier-chip-espacado.png` |

Os cards do gestor: SEM mudança neste bloco (a captura D16/D18 já
fechada no 0.9.6.19 — os 4 checks §1 «4/4 pass») — a evidência de
captura continua a do relatório 0.9.6.19.

## 7. MINI SIGN-OFF DO BLOCO A @C33 (4 + 4 itens — para o dono)

**Os 4 checks do 0.9.6.19:** ① menu inteiro (offset 0, contido, a última
linha alcançável) ② «Exportar OBJ» ③ card com captura após save
(modal nunca capturado; frame limpo sim) ④ gesto de mover a câmara
(drag fora dos handles move; o fov fica) — **4/4 pass**.

**Os 4 do hotfix B:** ⑤ o cabeçalho da hierarquia limpo (título + chip
espaçado ou ausente — nada colado, 2 densidades) ⑥ campos sem «-0» (a
função única; rotação -0.0 desenha «0») ⑦ as letras de eixo visíveis a
212/260dp (e o valor nunca sai, também a 180dp) ⑧ a câmara pequena mas
legível E fácil de agarrar (glifo 20dp, preview 48..120dp constante,
handles 10dp, bounding 2.07%/2.63% — pins ≤4%/≤6%).

**Os extras do D21 (mesmo sign-off):** adicionar objeto pelo + da
hierarquia funciona · o canto inferior direito do viewport fica limpo ·
orbit/drag nessa zona sem interceptação · o menu ⋯ tem «Novo objeto»
(o mesmo código do +).

## 8. NÃO VERIFICADO (honesto)

- **O device FÍSICO:** todos os números deste bloco são do device virtual
  (c33 @2.0) e do harness (@1.0). O gesto real de orbit no canto
  inferior-direito (sem o [+] a interceptar) e a leitura do glifo 20dp ao
  sol pertencem ao sign-off do dono com o APK do commit (versionCode 52).
- **A 2.ª densidade do dump commitado:** o `docs/hotfix19b-gizmo-medidas.json`
  traz o @2.0 (o device). O @1.0 é provado no test_core
  (`cameratic_d20_contrato_medivel_bounding_e_preview`, 2 densidades no
  MESMO teste) — o gate revalida a proveniência e os pins do dump.
- **O + da hierarquia como alvo de 40dp:** mantém-se o alvo 28dp da linha
  do cabeçalho (o piso kHeadFloorDp da casa — o desenho PASSO 1 do dono).
  Se o dono quiser o TOQUE ≥40dp na linha de 28dp, é uma decisão nova
  (registada como candidata ao BACKLOG; não foi implementada neste bloco
  para não violar o piso do cabeçalho).
- **A 2.ª metade da regra do chip:** com o chip escondido no painel
  estreito, a contagem da multi-seleção não se lê no cabeçalho (a limpeza
  continua no menu ⋮). Se o dono preferir a contagem SEMPRE visível
  (comprimindo o título), é uma decisão nova — registada no BACKLOG.
