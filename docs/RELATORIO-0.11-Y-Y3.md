# RELATORIO-0.11-Y-Y3.md — OS CARTÕES (o cristal: borda brilhante,
# separadores com gradiente)

Campanha: 0.11-Y (identidade visual) · Sub-fase: Y-3 · vc 80 → 81 ·
Data: 2026-10-17 · Commit: este

## §1 O que foi feito

- **A LINHA DO TOPO aplicada** (a primitiva glassTopLine da Y-1 — 1dp do
  glow a 20 %) depois do painel de vidro em: a HIERARQUIA
  (EditorUi::drawHierarchy), o INSPECTOR (drawInspector — com seleção e
  no trilho), o DRAWER (BottomPanel — a borda de baixo fica a hairline
  âmbar de sempre), o card CRIAR (o modal com a moldura ACCENT) e os 3
  sheets ancorados (o menu da hierarquia, o do inspector e o das
  cenas).
- **O DIVISOR com gradiente** (a primitiva NOVA dividerGradientV no
  UiContext — 1dp de largura, o âmbar da aresta a CAIR do topo:
  accentDim α0.90 no topo → α0.15 no fundo, o gradiente vertical via
  quadGradientV/emitGradientV da Y-1): o divisor DIREITO da hierarquia
  (x+w−1), o ESQUERDO do inspector (x) e a linha em repouso de
  drawPanelDividers (a linha do strip).
- **A decisão D11 SUPERADA com nota** — o 0.9.6.18 (HOTFIX D11) fechou
  «divisor = linha sólida 1dp na cor border + pill só no drag» (decisão
  do dono). A ordem expressa da Y-3 («separadores com gradiente»)
  SUPERADA a linha sólida; a pill de grip só-no-drag FICA intacta (o
  comportamento de drag não muda uma linha).
- **Zero audit entries novas** — a linha do topo e os divisores
  gradientes são quads decorativos emitidos fora do audit (como o
  drawLine dos gizmos): o registo do layout continua a descrever os
  WIDGETS, não o brilho; a FASE 31 (a varredura de 13 ecrãs) confirma
  o contrato do layout intocado.
- **A paleta continua fechada no Theme** (R-030): os gradientes usam os
  TOKENS (glow para o topo, accentDim com rampa de ALFA para o
  divisor) — nenhum hex novo fora da tabela.

## §2 Provas

- **Pins novos: 3/3 VERDE** (tests/test_fase_y3_cristal.cpp — TU novo,
  o padrão do scan de quads do batch):
  y3_cristal_hierarquia (a linha do topo no TOPO do painel com a
  LARGURA dele + o divisor direito com o gradiente α0.90→α0.15
  aferrados no batch + A LINHA SÓLIDA MORTA: nenhum quad 1dp-wide
  full-height com cor uniforme α0.60) · y3_cristal_inspector (o MESMO
  contrato no painel direito: topo + divisor esquerdo) ·
  y3_cristal_divisores (a linha em repouso de drawPanelDividers é o
  GRADIENTE no stripX esperado).
- **M-Y3a — a linha do topo MORTA na hierarquia** (mutação de CÓDIGO
  compilado: a chamada glassTopLine removida do drawHierarchy, rebuild,
  corrida, REVERSO):

  ```
  y3_cristal_hierarquia  FALHOU  (1 EXPECT — a linha do topo em falta)
  ```
  Reposto: «0 teste(s) com falha». ✓
- **M-Y3b — o divisor de volta à linha sólida** (o mesmo padrão: o
  dividerGradientV trocado pelo ui.panel 1dp na cor border):

  ```
  y3_cristal_hierarquia  FALHOU  (2 EXPECTs — o gradiente em falta E a
                                  linha sólida de volta)
  ```
  Reposto: «0 teste(s) com falha». ✓
- **A FASE 31 continua VERDE** com o cristal aplicado: o device virtual
  inteiro **1006 checks, 0 falhas** (a varredura dos 13 ecrãs corre
  com as linhas do topo e os divisores gradientes no ar — o contrato
  do layout não mexeu).
- **O CI apanhou a sentinela D11 desatualizada** (o run do commit
  011791e, job Testes do core): a `regress_hotfix_defeitos` (7) exigia
  a linha sólida do D11 como ENTRADA de audit — o gradiente é
  decorativo fora do audit (como os gizmos). A recalibração honesta
  NESTE commit: o check (7) aferra o QUAD GRADIENTE no batch (1dp,
  accentDim α0.90→α0.15 — o mesmo padrão dos pins y3_*); o check dos
  pontos flutuantes fica intacto. A suíte INTEIRA local: 0 falhas
  (GONI_SKIP dos 2 flakes conhecidos r039-disco e
  streaming_paralelo-corrida-suspensa).
- **versionCode: 80 → 81** (app/build.gradle:29, declarado no §14 do
  RELATORIO-0.9.6.md para o gate R-015).

## §3 Os limites / não-verificado

1. **A percepção final é do device** — o pin C33 do dono (instalar o
   APK vc 81 e ver o cristal) é o árbitro da BELEZA; os pins aferram a
   GEOMETRIA e as cores dos tokens no batch.
2. **O trilho do inspector (32dp sem seleção)** recebe o MESMO
   tratamento (o drawInspector corre nos dois estados) — o pin usa o
   painel com seleção; o trilho partilha o mesmo código.
3. **Os cards da tela de projetos (Java)** ficam fora — o espelho Java
   tem o sistema de tokens próprio (o mesmo limite declarado na Y-1).

## §4 Ficheiros tocados

- app/src/main/cpp/ui/UiContext.h/.cpp — a primitiva dividerGradientV
- app/src/main/cpp/ui/EditorUi.cpp — o cristal na hierarquia, no
  inspector, nos divisores (drawPanelDividers), no card CRIAR e nos 3
  sheets
- app/src/main/cpp/ui/BottomPanel.cpp — a linha do topo do drawer
- tests/test_fase_y3_cristal.cpp — NOVO TU (os 3 pins)
- tests/CMakeLists.txt — o TU na suíte
- tests/test_sentinels.cpp — o check (7) D11 recalibrado para o QUAD
  GRADIENTE (o fix do CI: a sentinela do hotfix exigia a linha sólida
  como entrada de audit; o divisor é o gradiente decorativo — o pin
  aferra o quad no batch, o check dos pontos flutuantes fica)
- app/build.gradle — versionCode 80 → 81
- docs/RELATORIO-0.9.6.md — a entrada Y-3 no §14 (o par 0.9.6/81)
- STATE.md — o §1 (Y-3 FECHADA), §2 (+linha), §8 (+relatório), vc 81
- CODEMAP.md — o TU novo + a primitiva, vc 81
- docs/RELATORIO-0.11-Y-Y3.md — este relatório

## §5 Pin C33 (o dono preenche)

- [ ] Instalar o APK **vc 81** (o artifact goni-vv-0.9.6-release-signed
      do run verde deste commit).
- [ ] Ver o cristal: a linha de brilho no topo dos painéis/drawer/
      cards; os divisores com o âmbar a cair (forte no topo, quase
      apagado em baixo); o drag dos divisores com a pill intacta.
- [ ] PNG do editor com o cristal → docs/y3-c33.png.
