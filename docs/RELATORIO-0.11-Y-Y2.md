# RELATORIO-0.11-Y-Y2.md — o LAYOUT (zero overflow, nada cruza a tab bar)

Campanha: 0.11-Y (identidade visual) · Sub-fase: Y-2 · vc 79 → 80 ·
Data: 2026-10-17 · Commit: este

## §1 O que foi feito

- **As DUAS REGRAS NOVAS do validador** (ui/LayoutDump.h/.cpp — o
  validador puro da casa, o mesmo caminho do botão Auditoria do device):
  - **CruzaTabBar** — a faixa da tab bar de baixo (a ÚLTIMA faixa do
    contentRect, altura `safe::kBottomTabH` — a fonte única, × a
    densidade DO registo): uma entrada ou vive FORA da faixa ou É DELA
    (as tabs/chips contidos); ATRAVESSAR a borda superior é o defeito.
    As exceções HONESTAS: (a) conteúdo de SCROLL (clipped — o clip
    corta por desenho); (b) o VÉU da camada modal (um painel desenhado
    antes que contém a faixa INTEIRA — o ecrã-cheio do Settings/Docs/
    Script tapa a tab bar por desenho; o que desenha DEPOIS do véu
    vive sobre o modal, a filosofia «painéis sobre painéis» da casa);
    (c) o OVERLAY de propósito (a flag nova `overlay` — o toast);
    (d) o véu ISENTO na própria entrada.
  - **PainelFora** — painel/moldura NÃO-interativo que sangra o
    contentRect (os interativos já são aferidos pelo ForaDoEcra de
    sempre, as labels pelo TextoSangra). O VÉU que contém o
    contentRect inteiro fica de fora (o scrim é intencional).
- **A flag auditável `overlay`** (LayoutDump.h + UiContext):
  `auditOverlayBegin/End` (o padrão do auditComposite) marca TODAS as
  entradas do TOAST (o painel E o texto) — a decisão da casa já
  registada nas FASEs 15.x («o toast é overlay bottom-center
  INTENCIONAL — fora da conta») ganha forma de contrato: o dump JSON
  documenta `ov` e o validador não conta o overlay como
  cruzamento/overflow.
- **O BUG REAL apanhado pela varredura — o toast na densidade 2.0
  MERGULHAVA na tab bar** (a classe R-018): o drawToast subtraía
  `safe::kBottomTabH`/`kStatusH` e as margens EM PX CRUS — a 2.0 a
  faixa tem 64px e o toast subtraía 32, ficando 32px DENTRO dela; a
  altura (48px crus) media metade do 48dp da spec M. A CURA (a lei
  R-018 na origem): TODAS as medidas do toast passam por theme::dp() —
  bh2 dp(48), a margem dp(48)/dp(24)/dp(16), a folga
  dp(kBottomTabH)/dp(kStatusH).
- **O BUG REAL 2 — o back do Settings sangrava o topo**: o botão era
  um quadrado 48dp centrado no cabeçalho de 36dp — (36−48)/2 = −6px
  ACIMA do contentRect (o sweep apanhou [8,18 48x48] vs o topo 24). A
  cura pela LEI DE OURO (PASSO 1): o elemento DE LINHA toma a altura da
  linha — o back passa a hdrH×hdrH (36×36) alinhado ao topo, o MESMO
  padrão do back das Docs (que já era limpo).
- **O BUG REAL 3 — o rótulo do ÚLTIMO tick da régua da timeline
  sangrava a borda direita** (medido: [1503,530 13x14] vs 1512): o
  labelFitted(x+3, …, 44) do tick final e o timecode do playhead
  deslizavam para fora do rect. A cura: o rótulo desliza PARA DENTRO
  quando não cabe inteiro (nunca truncado — a folga entre ticks é
  ≥228px; o MESMO contrato no timecode ACCENT).
- **A recalibração honesta do Sobreposto**: os filhos CLIPPED de um
  scroll saem dos pares — o rect deles é a posição NÃO-cortada do
  scrolled-out (o visível vive dentro da região; o mesmo contrato do
  ForaDoEcra/TextoSangra). Era a fonte de 11 falsos ERROS nos drawers
  (o inspetor com scroll cheio a «pisar» os chips da tab bar com o
  rect cru de filhos fora do ecrã).
- **A VARREDURA dos ecrãs REAIS** (a FASE 31 NOVA do device virtual):
  13 ecrãs desenhados pelo frame() DE PRODUÇÃO com o audit ligado (o
  mesmo mecanismo do export do Grupo B) e o registo corrido no
  validador — editor3d · drawer-ficheiros · drawer-assets ·
  drawer-consola · drawer-animacao · modo-ui · modo-audio · settings ·
  docs · script · storage · settings+drawer · drawer-max-400 (o cap de
  35%) — cada um com 3 checks: NADA cruza a tab bar · NENHUM painel
  sangra · ZERO erros do validador no ecrã inteiro. O dump
  GONI_DUMP31=<nome> (env, desligado por default) lista as entradas da
  base do ecrã para a caça.

## §2 Provas

- **Pins novos: 3/3 VERDE** (tests/test_fase_y2_layout.cpp — TU novo na
  suíte): y2_validador_tab_bar (o defeito dá EXATAMENTE 1 CruzaTabBar;
  a tab própria contida = autorizada; acima da faixa = limpo; o clip =
  por desenho; o interativo também é aferido) · y2_validador_overflow
  (o sangrar dá 1 PainelFora; o véu que contém o contentRect =
  intencional; o frame no topo dá 1; a densidade 2.0 alarga a faixa
  para 64px) · y2_validador_soma (as regras novas contam-se ao lado das
  de sempre; o describe/report dizem as regras novas: «CRUZA a tab
  bar»/«SANGRA o contentRect» + os ruleName cruza_tab_bar/painel_fora).
- **M-Y2a — o check CruzaTabBar MORTO** (mutação de CÓDIGO compilado:
  `if (false && …)` no validate, rebuild, corrida, REVERSO):

  ```
  y2_validador_tab_bar   FALHOU  (2 EXPECTs — o defeito deixa de ser
                                  apanhado: 0 em vez de 1)
  y2_validador_overflow  FALHOU  (1 EXPECT — o caso (e) deste pin
                                  também aferra a regra CruzaTabBar)
  ```
  Reposto: «0 teste(s) com falha». ✓ (a «não-isolação» do pin overflow
  no caso (e) é o pin a fazer o TRABALHO dele — o caso (e) é sobre a
  densidade da FAIXA, é CruzaTabBar por contrato)
- **M-Y2b — o check PainelFora MORTO** (o mesmo padrão):

  ```
  y2_validador_overflow  FALHOU  (2 EXPECTs — o sangrar deixa de ser
                                  apanhado)
  ```
  Reposto: «0 teste(s) com falha». ✓
- **A FASE 31 VERDE no device virtual**: 13 ecrãs × 3 checks + o
  contador (39+ checks novos); o replay INTEIRO **1006 checks,
  0 falhas** — a primeira corrida do replay com o validador de
  identidade a correr em TODOS os ecrãs do editor.
- **O CI apanhou o leftover da Y-1** (o run do commit 3a6d934, job C33
  virtual): o probe 18.2 do card de logs comparava o pixel com o
  literal do bg VELHO (14,14,16) — o Y-1 mudou o token e EU calibrei
  os compostos do 13.1/13.9/18.4 mas ESCAPEI-ME ao 18.2 (o CI é o
  árbitro — fez exatamente o seu trabalho). A cura NESTE commit: o
  probe lê o TOKEN (8,8,10) como os irmãos. O run VERDE deste commit
  fecha a Y-1 E a Y-2.
- **versionCode: 79 → 80** (app/build.gradle:29, declarado no §14 do
  RELATORIO-0.9.6.md para o gate R-015).

## §3 Os limites / não-verificado

1. **Os avisos (Aviso) não entram no contrato da varredura** — a FASE
   31 aferra os ERROS (as regras novas + as de sempre em severidade
   Erro); os ToquePequeno/TextoTruncado (aviso) continuam no relatório
   do Auditoria do device, fora do âmbito desta sub-fase.
2. **A flag `overlay` é de confiança do chamador** — o validador não
   adivinha overlays: quem desenha um flutuante NOVO (futuro) tem de
   marcar auditOverlayBegin/End. O dump JSON documenta (`ov`), o
   abuso fica visível no dump.
3. **A faixa da tab bar é da kBottomTabH (32dp)** — se um dia a barra
   mudar de altura, o validador acompanha pela fonte única (a mesma
   matemática do SafeArea); o pin (e) da densidade prova a propagação.
4. **O cenário IME (o teclado aberto)** não entra na varredura — a
   instância do harness com o IME simulado usa outra superfície
   (retrato); o contrato das linhas de símbolos é aferrado pelas
   FASEs 12.7/13.3 de sempre.

## §4 Ficheiros tocados

- app/src/main/cpp/ui/LayoutDump.h — as regras CruzaTabBar/PainelFora
  + a flag overlay
- app/src/main/cpp/ui/LayoutDump.cpp — as implementações (a faixa pela
  fonte única, os véus, o scope modal, o Sobreposto sem clipped) + os
  describe das regras novas
- app/src/main/cpp/ui/UiContext.h/.cpp — auditOverlayBegin/End (o
  scope) + a marcação em auditAdd_/auditLabel_
- app/src/main/cpp/platform/main.cpp — o drawToast com o scope overlay
  + a LEI R-018 (as medidas em dp)
- app/src/main/cpp/ui/SettingsPage.cpp — o back do cabeçalho 36×36 (a
  lei de ouro; o overflow de 6px morre)
- app/src/main/cpp/ui/Timeline.cpp — os rótulos da régua e o timecode
  deslizam para dentro (nunca sangram)
- tests/test_fase_y2_layout.cpp — NOVO TU (os 3 pins)
- tests/CMakeLists.txt — o TU na suíte
- tests/c33_virtual.cpp — a FASE 31 (a varredura de 13 ecrãs) + o dump
  de caça + o probe 18.2 recalibrado para o TOKEN
- app/build.gradle — versionCode 79 → 80
- docs/RELATORIO-0.9.6.md — a entrada Y-2 no §14 (o par 0.9.6/80)
- STATE.md — o §1 (Y-1 verde no run da Y-2 + Y-2 FECHADA), §2 (+2
  linhas), §8 (+relatório), vc 80
- CODEMAP.md — o TU novo + as regras do validador + o FASE 31, vc 80
- docs/RELATORIO-0.11-Y-Y2.md — este relatório

## §5 Pin C33 (o dono preenche)

- [ ] Instalar o APK **vc 80** (o artifact goni-vv-0.9.6-release-signed
      do run verde deste commit).
- [ ] Ver: NADA a pisar a tab bar em nenhum ecrã; o toast inteiro
      (48dp reais) 16dp acima da barra; o back do Settings dentro do
      cabeçalho; a régua da timeline sem cortes na borda.
- [ ] PNG do editor com o drawer aberto → docs/y2-c33.png.
