# RELATORIO FASE UI — O REDESIGN VISUAL (a ordem do dono, envios 1/5 + 2/5)

> A campanha FASE UI: SÓ visual, layout e UX (a cláusula calma: core,
> V.ONI, importadores, física e .gmesh INTOCÁVEIS). As partes A..E, um
> commit por parte, CI verde entre elas. As regras da fase: toque mínimo
> 40dp, texto mínimo 12sp, sem blur/sombras pesadas, valores visuais SÓ do
> ficheiro de tokens, teste automático de sobreposição, relatório de 16
> itens + capturas em 3 resoluções (retrato e paisagem) + a lista do que
> NÃO foi feito (§8).

## §0 A abertura (as verificações antes do código)

- **vc**: 110 no HEAD 585217f (o 0.12-Y D + o fix do replay fechados) —
  confirmado no build.gradle e no STATE.md.
- **CI**: 4/4 SUCCESS no 585217f (Testes do core Linux · C33 virtual
  1009/0 · APK release arm64 assinado · verify-entry-symbols) — via API
  GitHub no arranque desta campanha.
- **STATE §9**: a ordem do dono É a fila (a FASE UI entra depois da 0.12-Y
  D; nenhuma decisão fechada reabre — a paleta é ORDEM NOVA do dono, o
  padrão Y-3/D11 de superação legítima).
- **Leituras**: STATE.md · CODEMAP.md · ui/Theme.h (o ficheiro único de
  tokens) · os gates que aferram a pele (theme_hex_check R-030 ·
  ui_vocab_check · projects_ui_check TOKENS_F · release_identity R-015) ·
  os testes que medem cor (y1_tema · y3_cristal · idioma · qol · efeitos ·
  toolbar · o sentinela R-030 regress_identidade_grafite_ambar).

## §1 PARTE A — TOKENS E IDENTIDADE (o ficheiro único + a brasa)

**O que mudou** (o Theme.h É o ficheiro único de tokens da casa — as
cores, os espaços, os raios e a tipografia já moravam lá; a parte A dá-lhe
a paleta do dono e completa a escala):

- **A PALETA DO DONO (hexes EXATOS)**: bg #0B0B0D (o fundo viewport) ·
  surface #121215 α0.85 (os painéis — o vidro da casa mantém o α) ·
  surface2 #1A1A1E α0.86 (vidro denso) · card #1B1B20 (OS ELEMENTOS — o
  dono deu o MESMO hex da escada 0.12-C, o token nem mexe) ·
  elevated/border #26262C (o elevado e AS LINHAS 1px) · text1 #E8E6E3 ·
  text2 #8A8A93 · **accent #FF5A1F — A BRASA, o destaque ÚNICO** (SÓ em
  seleção, aba ativa, Play, botão principal, cursor de tempo) ·
  accentDim #2B1810 (O FUNDO DE SELEÇÃO) · axisX/Y/Z #E5484D · #3FB950 ·
  #3B82F6 (os eixos EXATOS do dono — conteúdo, não chrome).
- **O ÂMBAR MORREU**: accent #FFB020 → #FF5A1F; accentPress #E09A00 →
  #D94A16; accent2 #FF6B00 → #E0440F (a brasa escura — a base do
  gradiente dos ativos, a MESMA família: o destaque continua ÚNICO); glow
  #FF3D00 α0.25 → #FF5A1F α0.25; fireRed/fireOrange re-alinhados à brasa
  (o gradiente dos botões ativos). A RAMPA do LOGO (fire0..fire3) fica
  INTACTA — o «G em chama» é identidade, não chrome. O warn #FF8A3D fica
  (semântica, nunca == accent). SEM azul como destaque (o azul do eixo Z
  e da V.ONI é CONTEÚDO — a exceção documentada de sempre).
- **A ESCALA DO DONO**: 4/8/12/16 — o kSpaceMid=12 entrou; o kSpace3
  (recuo dos filhos) desce de 24 para 16; os raios passam à banda 6-8
  (kRadiusField 4→6, kRadiusCard 8 fica). As bordas 1px mantêm-se.
- **A PAREDE DE FONTE ÚNICA**: os aliases BG/PANEL/LINE/TEXT/ACCENT/
  ACCENT2/GLOW do UiContext.h e os eixos do Gizmo.h re-ligados por
  static_assert (a pele muda NUM sítio).
- **O ESPELHO JAVA**: os tokens do ProjectManagerActivity alinhados
  (BG/SURFACE/SURFACE2/CARD/ELEVATED/BORDER/TEXT1/TEXT2/ACCENT/
  ACCENT_PRESS/ACCENT_INK/ACCENT_DIM + os 2 vidros) e os 2 hexes inline
  mortos (o matiz âmbar dos iniciais → brasa escurecida; o border do tile
  → o token BORDER); o TOKENS_F do gate projects_ui_check recalibrado.
- **A SONDA honesta do c33**: a banda da toolbar lê o fundo do TOKEN (era
  o literal (20,20,20) do mono — o achado do explorador, morto aqui).

**Contraste validado ANTES do commit** (o padrão da casa): sobre o painel
composto text1 15,1:1 · text2 5,5:1 · accent 6,1:1 · danger 4,8:1 · warn
8,0:1 · ok 6,2:1; pior caso do vidro (branco por trás) text1 9,7:1 ·
accent 3,9:1; bg↔accent 6,3:1 · bg↔accent2 4,7:1 · tinta↔brasa 6,2:1 ·
accentDim↔text1 13,6:1 — todo o texto ≥4,5:1 e todo o componente ≥3:1.

**Pins**: ui_tokens_brasa (test_fase_ui.cpp NOVO — os hexes exatos, a
regra do destaque ÚNICO pela família R>G>B e a escala 4/8/12/16) ·
ui_sobreposicao_topbar (o TESTE AUTOMÁTICO DE SOBREPOSIÇÃO da fase: a
barra em 5 ecrãs ≥640dp — 3 paisagens × 2 retratos — nada tapa nada nem
sai do ecrã; a banda <480dp é a degradação da parte B, o achado fica no
§8) · y1_tokens/y1_primitivas/y1_gradiente_botao recalibrados (a mesma
fonte). Recalibrações com nota: superficies_4_tons · qol_a_border_cinza ·
qol_a_inspector_ar (o fill bg lê-se 11/255) · qol_b_navegador_compacto ·
vidro_85_topline_ambar (a linha do topo é BRASA) · theme_tabela_spec_g_exata
· theme_escala_8dp_e_raios_da_spec (kSpaceMid 12 + kSpace3 16 + raio 6) ·
regress_identidade_grafite_ambar (a banda da rampa alarga para o branco
quente do dono: B−R ∈ [−8..12], teto do frio 10; o composto do painel
16.95/255; o warn distingue-se pelo B) · regress_janelas_passo4 +
regress_hotfix_defeitos (o divisor lê o border novo).

**Mutação M-UIa** (o âmbar de volta — o destaque ÚNICO violado; Theme.h +
o alias do UiContext.h para #FFB020):

```
y1_tokens ... FALHOU  test_fase_y1_tema.cpp:104  nearColor(theme::kTheme.accent, 1.0f, 90/255, 31/255, 1.0f)
  FALHOU  test_fase_y1_tema.cpp:132  countSub(src, "VV_RGB(255, 90, 31)") == 2
  FALHOU  test_fase_y1_tema.cpp:144  countSub(src, "VV_RGB(255, 176, 32)") == 0
ui_tokens_brasa ... FALHOU  test_fase_ui.cpp:83  nearC(theme::kTheme.accent, 255, 90, 31, 1.0f)
  FALHOU  test_fase_ui.cpp:98  theme::kTheme.accent[1] < 150.0f/255.0f
```

Reposto: 0 falhas (a suíte inteira verde local — o CI é o árbitro). A 1.ª
parede é o static_assert do UiContext.h (o build morre antes dos pins — a
fonte única funciona).

**Verificação**: test_core 0 falhas na corrida completa local (os flakes
da sandbox excluídos via GONI_SKIP — o CI é o árbitro) · theme_hex/ui_vocab/
projects_ui VERDES · **versionCode 111** (o §14 do RELATORIO-0.9.6.md
declara o par no MESMO commit).

## §2 PARTE B — BARRA SUPERIOR (40dp, a ordem do dono)

**O que mudou**:

- **A ALTURA**: safe::kTopBarH 36 → 40dp (o toque mínimo da fase vale
  para TODA a barra; os ~4dp poupados vão ao viewport).
- **A DIREITA NOVA**: [↶ desfazer][↷ refazer][💾 guardar][▶ Play][■ Stop]
  [⚙ definições] — o undo/redo/save SUBIRAM do viewport (a fila do topo
  do chrome morre e fica o chip ⋯; os ids 30/31/32 ficam APOSENTADOS, os
  novos 20/21/22 fora das faixas existentes). O despacho vive no bloco
  da barra e vale em TODOS os modos (3D/UI/Áudio — a barra é comum). O
  desativado (sem histórico) desenha o glifo a 0.35 e o toque segue para
  o toast honesto («nada a desfazer» — o caminho de sempre).
- **O PAUSE SAIU DA BARRA** (a ordem: «para dentro de um menu do Play»):
  vive na PLAY BAR (o botão [Pausar]/[Retomar], id 6, ao lado do Stop) e
  o estado é REAL — o main congela o mundo (os passos fixos não correm, o
  acumulador do tempo não cresce: o play retoma exatamente onde parou), o
  render continua e a barra diz «em pausa · fps N». O id 3 (kTbPauseId)
  fica APOSENTADO (o padrão D21).
- **O SELETOR «Android» SAIU DA BARRA** (a ordem: «para dentro das
  definições»): a linha informativa «plataforma: Mobile (Android)» na
  secção Geral do Settings (o alvo REAL da build; não há outros alvos —
  a honestidade de sempre). O id 19 fica APOSENTADO.
- **A DEGRADAÇÃO**: o chip da plataforma já não é o primeiro a ceder (não
  existe) — a escada é o k do flex com o piso 0.55; a banda <580dp é a
  degradação documentada (o editor é paisagem-first; §8).
- **O ACHADO DO CI (a lacuna dos DOIS alvos de novo)**: o commit A ficou
  VERMELHO no CI — a sonda 35.2 do c33_virtual aferrava a LINHA ÂMBAR
  (o build local não compilava o c33: o build_tests.py só leva o
  test_core). A cura no MESMO commit da B: a sonda lê A BRASA e nasce o
  **scripts/build_c33.py** (o irmão do build_tests.py — o gate da casa é
  compilar os DOIS alvos; a lição do 585217f registrada na ferramenta).
  O c33 apanhou MAIS 11 sondas stale (as geometrias dos pickers com
  linhas de 48dp sem o chrome, a fila de 4, as alturas 36) — todas
  recalibradas com o harness a MIRROR o overlayArea real (a medição dos
  quads provou o y+31/36dp).

**Pins**: ui_topbar_40dp_composicao (test_fase_ui.cpp — a altura 40 na
fonte única, os ids novos fora das faixas, o kPlayPauseId=6 na play bar,
a FONTE sem kTbPauseId/kTbPlatformId e com os 3 novos, a linha
«plataforma» nas Definições) · ui_sobreposicao_topbar estendido à direita
nova (12 alvos, 5 ecrãs ≥640dp) · recalibrações com nota: test_toolbar
(2 pins), test_012c_chrome (topbar_3_zonas + menu_scroll 640 +
menu_ancorado), test_012qol (o showPlatform morreu), test_safearea (4
valores derivados), test_wiring090 (o G29 + a fila), test_sentinels
(densidade 80px, os ids aposentados, o chip único), test_playui (a
assinatura do pause), c33_virtual (9 sondas).

**Mutação M-UIb** (a barra volta a 36 — a ordem do dono violada):

```
ui_topbar_40dp_composicao ... FALHOU  test_fase_ui.cpp:143  nearEqF(safe::kTopBarH, 40.0f, 1e-4f)
```

Reposto: 0 falhas (test_core + c33_virtual 1009/0 — os DOIS alvos, o CI
é o árbitro).

**Verificação**: test_core 0 falhas · c33_virtual 1009/0 local (os DOIS
alvos) · docs-lint VERDE · release-identity/R-015 VERDE vc 112 ·
**versionCode 112** (o §14 declara o par no MESMO commit).

## §3 PARTE C — HIERARQUIA (40dp, os controlos só na selecionada)

**O que mudou**:

- **AS LINHAS 40DP**: kRowH 36 → 40 (o toque mínimo da fase nas linhas
  todas; o conteúdo da hierarquia cresce 40×N — o scroll acompanha pela
  fonte única hierarchyContentHeight/hierarchyRowAtTap).
- **O OLHO E O ⋮ SÓ NA LINHA SELECIONADA** (a ordem do dono): o draw é
  gated por `if (selected)` e o hit-test por `controlsRow` (a MESMA
  condição — desenho e toque NUNCA divergem); nas linhas não
  selecionadas a zona é CORPO (o toque seleciona — nada de toggles
  acidentais).
- **O NOME COMPLETO**: nas linhas não selecionadas o nome usa a largura
  TODA da linha (a reticência só quando não couber — o budget do nome
  cresce; na selecionada o nome cede as zonas ao olho/⋮).
- **A INDENTAÇÃO DOS FILHOS**: o literal 24dp morre — a indentação lê a
  ESCALA DO DONO (theme::kSpace3 = 16; draw e hit-test partilham).
- **O «+» DO CABEÇALHO** já criava TIC (o menu de presets — o fluxo do
  C-4; a ordem confirma, nada a mudar).

**Pins**: ui_hierarquia_40dp (test_fase_ui.cpp — a linha 40 no extern, a
FONTE com o draw gated + o hit gated, a indentação pela escala, o nome de
largura inteira) · recalibrações com nota: test_uieditor (o teste do olho
reescrito: o Env pré-seleciona, o toggle só com seleção, a linha do Outro
prova o corpo), test_fase1_hierarquia (kTestRowH 40), test_scroll (as 5
linhas de piso + a mesh 340), test_safearea (contentH 800/1322), 
test_wiring090 (o rowY do long-press), c33 (o 12.10: as linhas 80px @2.0).

**Mutação M-UIc** (o olho em TODAS as linhas de volta — o gate morto):

```
ui_hierarquia_40dp ... FALHOU  test_fase_ui.cpp:182  ui_hierarquia_40dp: o olho/⋮ é SÓ da linha selecionada (draw gated + hit gated)
```

Reposto: 0 falhas (test_core + c33_virtual 1009/0 — o CI é o árbitro).

**Verificação**: test_core 0 falhas · c33_virtual 1009/0 local ·
docs-lint VERDE · release-identity/R-015 VERDE vc 113 · **versionCode
113** (o §14 declara o par no MESMO commit).

## §4 PARTE D — VIEWPORT E FERRAMENTAS (a coluna das 4 + os chips)

**O que mudou**:

- **A COLUNA DAS 4 FERRAMENTAS**: o rail é Selecionar/Mover/Rodar/
  Escalar (o desenho 32dp na banda 30-36 do dono, o alvo de toque 40 —
  a lei de ouro); a ferramenta ATIVA É A BRASA (o fill accent + a tinta
  escura — o padrão que a parte A re-alinhou ao #FF5A1F).
- **O ÍMAN E A MÃO (Pan) DESCERAM A CHIPS** (a ordem do dono): os chips
  pequenos SOB o railPanel (o desenho 28dp dentro do alvo 40) — os
  MESMOS ids de sempre (kVpPanId 33, kVpSnapValId 36) e o MESMO despacho
  (o toque arma o panMode / alterna o snap — zero lógica nova). A
  degradação: os chips morrem primeiro (o rail tem prioridade) e o par
  INTEIRO tem de caber (o achado da M-UId: o fit do 1.º chip só não
  chega — o regress_hierarquia_contrato apanhou o 2.º a sair do rect).
- **O VIEWPORT CONTINUA A MAIOR ÁREA**: as frações da C-1 (hierarquia
  20%, viewport ≥60%) intocadas; nada de botões grandes no centro (o
  gizmo do canto continua morto — o C-2).
- **OS DIVISORES**: a zona de toque 24dp, os pisos (hierarquia 140 ·
  inspector 180..260 · viewport 288) e a PERSISTÊNCIA layout.json
  (hierW/inspW) JÁ EXISTIAM (o Grupo D + o divisor_drag) — a ordem
  confirma; o pin novo aferra o contrato à fase.
- **O ACHADO FORA DA LISTA**: a degradação do rail de 4 é MAIS
  tolerante que a de 6 (a coluna única cabe a 184dp de altura, a linha
  única a 200dp de largura) — as sondas de stress da casa recalibraram
  para os pisos honestos novos (o ltiny 150dp, o vtt 36dp).

**Pins**: pan_rail_sexta recalibrado (os chips sob o rail, o id 33 vivo,
a degradação nova) · ui_divisores_persistencia NOVO (a zona 24dp + os
pisos + o roundtrip layout.json 200/240) · recalibrações com nota:
test_sentinels (os arrays com chips, o railCols 1 no dev rect, a escada
220, o ltiny 150, o vtt 36), test_wiring090 (os alvos 6), c33 (os 3
loops do 9.9), test_012y (o pin do pan).

**Mutação M-UId** (o fit do par volta ao 1.º chip — o 2.º sai do rect):

```
regress_hierarquia_contrato ... FALHOU  test_sentinels.cpp:4068  C33 1536x720@2.0 raw=100000: um alvo do chrome sai do rect da viewport
```

Reposto: 0 falhas (test_core + c33_virtual 1010/0 — o CI é o árbitro).

**O ACHADO GRAVE (o bad_alloc do B, fechado)**: o commit B levou um UB
real — o loop `ltiny.quick[qi]` de 4 iterações lia PASTO do array
`UiRect quick[1]` (a recalibração incompleta da fila). O Release no
runner transformava o lixo lido em `std::bad_alloc` (2× no CI); a suíte
local passava por sorte (o lixo benigno no mesmo sítio). O diagnóstico
do C (a saída ao vivo) apanhou o ciclo de falhas do OOB no runner — a
cura é a D: o loop é do chip ÚNICO (`qi < 1`) e o sub-mínimo honesto é
o ltiny 150dp. A lição na ferramenta: o build local (test_core + c33)
não vê UB de build Release do runner — o gate é o CI.

**Verificação**: test_core 0 falhas · c33_virtual 1010/0 local ·
docs-lint VERDE · release-identity/R-015 VERDE vc 114 · **versionCode
114** (o §14 declara o par no MESMO commit).

## §5 PARTE E — INSPECTOR (o cabeçalho do TIC + as barrinhas do eixo)

**O que mudou**:

- **O CABEÇALHO DO TIC** (a ordem: «nome do TIC (brasa) e o tipo por
  baixo»): com seleção o cabeçalho cresce 28 → 44dp — o NOME em BRASA
  (14sp, a reticência só quando não couber) e o TIPO por baixo (12sp —
  a lista das secções PRESENTES, lida do MESMO perfil que alimenta o
  plano: Transform · Câmara · Malha · Física · Áudio · Animação ·
  Script · Environment · Input; a lista nunca diverge do que o corpo
  mostra). Sem seleção o título «Inspector» fica como sempre (28dp).
  O orçamento do nome reserva a célula do recolher (40dp) quando o
  painel está fixado — nada tapa nada (a regra da fase). O traço
  divisor e o topo do scroll seguem a altura nova (a fonte única).
- **AS BARRINHAS DO EIXO** (a ordem: «cada um com barrinha colorida do
  eixo e valor, sem letras gigantes»): as LETRAS X/Y/Z das caixas de
  Posição/Rotação/Escala MORRERAM — a BARRINHA 3dp × 12dp na COR DO
  EIXO (os tokens axisX/Y/Z #E5484D/#3FB950/#3B82F6 — conteúdo, a
  exceção documentada de sempre) identifica o eixo, centrada na caixa
  de 32dp; o valor começa a 12dp (a barra termina a 9dp — sem toque) e
  o espaço do valor CRESCIU 8dp com a letra morta (o piso R1 do valor
  não-vazio mantém-se — o orçamento transformRowBudgetPx intocado). O
  título da linha (Posição/Rotação/Escala) identifica o campo.
- **AS SECÇÕES RECOLHÍVEIS** (a ordem: «Transform, Física,
  Material...») JÁ EXISTIAM — os 9 bits do 0.9.0 (Transform/Câmara/
  Malha/Material/Física/Áudio/Animação/Script/Environment) persistem
  no EditorState::inspCollapsed e os cabeçalhos têm a altura da casa.
  A ordem confirma; o pin prova no PLANO (os 5 cabeçalhos do perfil
  Transform+Malha+Física+Material+Script ficam, os 3 campos do
  Transform morrem com o bit).
- **O ESTADO VAZIO** (a ordem: «uma linha discreta; o botão "criar
  TIC" sai daqui»): a linha discreta «Nada selecionado» (text2) e o
  botão «+ criar TIC» sai daí — abre o MESMO plusMenu de presets do +
  da hierarquia (o caminho do C-4; nada duplicado, o convite não é de
  fachada).

**Pins**: ui_inspector_cabecalho (test_fase_ui.cpp — (a) a fonte do
cabeçalho (o nome em accent + a tabela do tipo + os 44dp), (b) as
barrinhas + as LETRAS MORTAS, (c) o criar TIC do vazio, (d) as secções
recolhíveis no plano, (e) o orçamento do nome com o pin ativo) ·
recalibrações com nota: test_wiring090 (os 2 toques do Transform sobem
para o cabeçalho 44dp), test_safearea (o scroll do Player: listH
500 → 484), test_ui (as 6 geometrias do scroll/planos com o cabeçalho
do TIC).

**Mutação M-UIe** (as letras de volta — a ordem do dono violada):

```
ui_inspector_cabecalho ... FALHOU  test_fase_ui.cpp:240  ui_inspector_cabecalho: as letras X/Y/Z morreram (a barrinha identifica o eixo — a ordem do dono)
```

Reposto: 0 falhas (test_core + c33_virtual 1010/0 — o CI é o árbitro).

**O ACHADO DO CI (o 1.º push da E ficou VERMELHO — a lição de novo)**:
o bloco (d) do pin usava os tipos do plano SEM o qualificador
(`InspProfile` em vez de `editor::InspProfile` — o estilo do irmão
ui_sobreposicao_topbar) e o build local INCREMENTAL não recompilou o TU
(o objeto stale passou a suíte local); o CI compilou limpo e morreu no
passo «Compilar» (`InspProfile was not declared in this scope`). A
cura no commit do fix: os tipos qualificados com `editor::`. A lição
aferrada: quando o pin muda, o TU recompila COM PROVA (o .o apagado à
mão) — o build incremental cala-se, o CI é o árbitro (a mesma lição do
585217f e da B, agora no TU).

**Verificação**: test_core 0 falhas (o único vermelho local = o fork
do sentinela_r039, o flake da sandbox documentado — o CI é o árbitro)
· c33_virtual 1010/0 local (os DOIS alvos) · theme_hex/ui_vocab/glyph/
hierarchy/projects_ui VERDES · release-identity/R-015 VERDE vc 115 ·
**versionCode 115** (o §14 declara o par no MESMO commit).

## §6 OS 16 ITENS DA FASE (o estado de cada um)

| # | O item da ordem | Onde vive | Estado |
|---|---|---|---|
| 1 | Tokens num ficheiro ÚNICO (cores/espaços/raios/alturas/fontes) | ui/Theme.h | ✓ A (§1) |
| 2 | Nada hardcoded nos painéis (a parede static_assert) | Theme/UiContext/Gizmo + theme_hex_check | ✓ A (§1) |
| 3 | Layout proporcional (o desktop virá depois) | safe::resolvePanels + as frações da C-1 | ✓ A (§1) |
| 4 | A paleta do dono (os hexes exatos) | ui/Theme.h + ui_tokens_brasa | ✓ A (§1) |
| 5 | O âmbar removido; brasa #FF5A1F destaque ÚNICO; sem azul de destaque | ui/Theme.h (a regra R>G>B) | ✓ A (§1) |
| 6 | Eixos X/Y/Z #E5484D / #3FB950 / #3B82F6 | ui/Theme.h + Gizmo.h | ✓ A (§1) |
| 7 | Espaços 4/8/12/16 · raios 6-8 · bordas 1px · o vidro com o brilho na seleção | ui/Theme.h (kSpaceMid 12 · kRadiusField 6) | ✓ A (§1) |
| 8 | Sans para a UI, mono para o estado e o código | ui/Theme.h (a tipografia da casa) | ✓ A (§1) |
| 9 | Barra superior 40dp: logo/Menu/Cena · os modos com a aba sublinhada a brasa · undo/redo/guardar/Play/Stop/definições | safe::kTopBarH + toolbar::topbarLayout + ui_topbar_40dp_composicao | ✓ B (§2) |
| 10 | O Pause e o «Android» para dentro (a play bar / as Definições) | kPlayPauseId 6 + a linha «plataforma» | ✓ B (§2) |
| 11 | Hierarquia 40dp: ícone + nome completo · olho/⋮ SÓ na selecionada · a indentação · o «+» cria TIC | kRowH 40 + ui_hierarquia_40dp | ✓ C (§3) |
| 12 | Viewport maior área · a coluna de 4 ferramentas à esquerda DENTRO (30-36dp, a ativa em brasa) · Íman/Mão em chips | ViewportChrome (o rail 4 + os chips 28/40) | ✓ D (§4) |
| 13 | Divisores arrastáveis (toque 24dp, os pisos) · guardar/restaurar o layout | ui_divisores_persistencia (layout.json hierW/inspW) | ✓ D (§4) |
| 14 | Inspector: o cabeçalho com o nome do TIC (brasa) e o tipo por baixo | headerH 44 + ui_inspector_cabecalho | ✓ E (§5) |
| 15 | Pos/Rotação/Escala com a barrinha colorida do eixo e o valor (sem letras gigantes) | a barrinha 3×12dp + a M-UIe fechada | ✓ E (§5) |
| 16 | O estado vazio discreto; o «criar TIC» sai daí | kInspCreateTicId (o plusMenu do C-4) | ✓ E (§5) |

As secções recolhíveis do Inspector (Transform/Física/Material…) já
viviam (os 9 bits do 0.9.0) — a ordem confirma; a prova fica no pin (d)
do §5.

## §7 AS CAPTURAS (a regra da fase)

- **O artefacto do CI** `layout-screens-c33-virtual` (o job C33
  virtual): os PNG+JSON do harness — o editor, o script, os docs e o
  browser a **1536×720** (a resolução REAL do C33, paisagem), o editor
  a densidade 2.0 (`editor-2x`) e o modo-UI device; a ROTAÇÃO retrato
  **720×1536** é provada viva nas sondas do replay (7.3) e a área do
  editor aferrada nos dois lados.
- **A regra «3 resoluções, retrato e paisagem» é o teste automático**:
  o ui_sobreposicao_topbar afere a barra em **5 ecrãs ≥640dp** —
  paisagem-S/M/L (776×336 · 1600×720 · 2560×1440) × retrato-M/L
  (720×1600 · 1440×2560) — nada tapa nada nem sai do ecrã; o pin corre
  em TODA a suíte (não é uma captura congelada: é o layout medido de
  novo a cada corrida).
- O que NÃO há: captura de device FÍSICO (o CI não tem telemóvel — a
  validação final no aparelho é do dono, ver §8).

## §8 O QUE NÃO FOI FEITO (a lista honesta da fase)

- **Desktop / ecrãs grandes**: o layout continua mobile-first
  proporcional (a ordem: «o desktop virá depois») — nenhum modo desktop
  foi inventado.
- **Blur, sombras pesadas ou animações de duração**: fora por ordem
  (custam FPS); o «vidro» mantém o padrão da casa (a borda fina + o
  brilho no topo da seleção, sem blur real).
- **Capturas em device físico**: o CI produz o dispositivo VIRTUAL
  (C33 1536×720) — a validação no telemóvel é do dono.
- **A banda <640dp**: a degradação documentada da parte B (o editor é
  paisagem-first); o paisagem-S (776×336) é o piso testado, não o alvo.
- **Temas alternativos ou um 2.º destaque**: a brasa é ÚNICA por ordem;
  não há seletor de tema nem modo claro.
- **Os envios 3/5..5/5**: o 3/5 e o 4/5 CHEGARAM (as partes F/G/H —
  §9/§10/§11 abaixo); o **5/5 NÃO chegou** — nada antecipa o que ele
  ordenar (a fila é a ordem do dono).
- **Core, V.ONI, importadores, física e .gmesh**: INTOCÁVEIS (a
  cláusula calma da fase — o diff da fase não os toca).

---

# ENVIOS 3/5 + 4/5 (as partes F, G e H — um commit por parte)

## §9 PARTE F — BARRA INFERIOR E PAINEL ENCAIXADO (a ordem do dono)

**O que mudou** (ui/BottomPanel.h/.cpp + ui/SafeArea.h + o main):

- **A BARRA FINA 32dp**: à ESQUERDA o botão **«Painéis ▾»** (o chip 24dp
  centrado na barra; o ALVO é a célula inteira — a barra É a faixa de
  toque) e à DIREITA o estado **«60 FPS · 6 TICs»** (caption 12sp text2,
  right-aligned na reserva kFpsW 112dp). O «· bl n/m» APOSENTOU (a ordem
  literal: «remover o bl 2/2») — a reserva deixou de crescer (o pin
  20.5 recalibrado: a reserva É FIXA). O chevron desenha por geometria
  (2 traços): para BAIXO fechado, para CIMA aberto.
- **O CONTORNO BRASA**: com o painel aberto o chip leva `frameRounded`
  na cor accent (a ordem: «contorno brasa quando o painel está aberto»);
  fechado, o border cinza de sempre.
- **O PAINEL ENCAIXADO** (não é janela por cima): as 4 TABS saíram da
  barra para o CABEÇALHO do painel — **fila de 40dp** (o toque mínimo da
  fase) sob a pega de 24dp, com o ícone + a palavra 12sp, o ativo em
  accent + o underline 2dp. O conteúdo é a FONTE ÚNICA `Layout::content`
  (o draw das 4 abas E o main para a timeline partilham o MESMO rect —
  a classe do R-019 nunca mais). O viewport SOBE: os rects do centro
  comem a altura pelo `panelsRect`/`currentDrawerH` (o mecanismo P-08
  intocado — o pin prova o encaixe).
- **ABRIR/FECHAR**: o Painéis abre com a **ÚLTIMA aba** (`lastTab` —
  nova no BottomState, persistida no layout.json como `lastTab=N`,
  retrocompatível: os ficheiros antigos sem a linha ficam com o default
  1). Fecha ao tocar no Painéis (▾) — a aba ativa NÃO fecha mais (a
  ordem: «fecha ao tocar em ▾ ou em Painéis»).
- **A ALTURA INICIAL por tab**: `tabDefaultHPct`/`tabDefaultH` (a fonte
  única pura) — **~30%** (Ficheiros/Assets/Consola) e **~40%**
  (Animação), clamp ao intervalo cru 160..400; aplica-se ao ENTRAR na
  tab (abrir ou trocar) enquanto o dono não arrastou a pega
  (`hCustom` — o arrasto manda e morre ao fechar). O CAP efetivo
  acompanha: a Animação tem teto 40% (`safe::kDrawerMaxPctAnim`) e
  **PISO** — a `effectiveDrawerH` cresce o drawer até caber a timeline
  (`kTimelineH` + pega + fila) quando o ecrã permite; nos ecrãs pequenos
  o cap manda e a TIMELINE CLIPA dentro do rect (`ScopedClip` na strip
  de ui/Timeline.cpp — os overlays de CLIPS/+track continuam pop-ups de
  ecrã inteiro, fora do clip). Nada cruza a barra de baixo em nenhum
  ecrã (a regra da fase).
- **O PONTO VERMELHO**: com linhas `E/` no engine.log (o MESMO
  classificador da consola), a ABA Consola e o BOTÃO Painéis acendem o
  ponto danger de 6dp (a ordem: «um ponto vermelho na aba e no botão
  quando há erro»).
- **A ANIMAÇÃO**: a timeline de sempre (régua de tempo, uma faixa por
  propriedade, keyframes em losango, o cursor de tempo EM BRASA) — a
  ordem confirma o que já vivia em ui/Timeline.cpp; o que mudou é a
  casa (o piso de altura + o clip) e o rect (a fonte única).

**A prova** (tests/test_fase_ui.cpp — o pin `ui_paineis_dock`):
(a) a barra em **5 ecrãs** (paisagem-S/M/L × retrato-M/L) × os 5 estados
do painel — o botão Painéis e o estado disjuntos DENTRO da barra;
(b) o ENCAIXE — pega/fila/conteúdo dentro do painel, o fundo do painel
== o topo da barra, a altura efetiva == a fonte única (cap + piso da
tab); (c) os defaults 30/40% + o clamp; (d) a Animação cabe a timeline
(o piso efetivo num ecrã de referência); (e) a FONTE — o rótulo
«Painéis», os ≥3 `kTheme.danger` (os 2 pontos + as linhas E/), o
`bl %u/%u` MORTO, o estado «%d FPS · %u TICs», o contorno accent aberto;
(f) o roundtrip `lastTab` + o retrocompatível sem a linha.

**M-UIf** (o encaixe frouxo: o fundo do painel +4px DENTRO da barra)
vermelho→reposto→verde: `ui_paineis_dock` «L.drawer.y + L.drawer.h <=
L.tabBar.y» VERMELHO (20 EXPECTs; capturado ao vivo). A cura é a
reposição; a lição fica.

**Recalibrações com nota**: test_012a_contadores (o `content()` do Env
lê a FONTE ÚNICA Layout::content), test_wiring090 (o tap da chip Erros
lê `L.content.y` — a fila desceu o conteúdo dp(40); o roundtrip do
layout leva lastTab), c33_virtual 13.7h (a janela dos chips +80px e o
tap a 590 — a fila de tabs), 13.7i (o contentH desconta a fila; o
degradado honesto da regra 60%: na banda 776×336dp o cap 35% deixa o
conteúdo < 80dp — geometricamente impossível; o que fica é o campo de
comando FORA e a lista como o resto inteiro), 20.5 (a reserva FIXA — o
bl saiu), FASE 31 (a timeline clipada — CruzaTabBar=0 em 13 ecrãs).

**Verificação**: test_core 0 falhas (a corrida completa local) ·
c33_virtual **1010/0** · theme_hex/ui_vocab/glyph/hierarchy VERDES ·
docs-lint VERDE 121 · release-identity/R-015 VERDE vc 116 ·
**versionCode: 115 → 116** (o §14 do RELATORIO-0.9.6.md declara o par
no MESMO commit).

**O que NÃO foi feito na F**: o «· bl n/m» não ganhou nome novo (a ordem
dizia «remover OU mostrá-lo com nome claro» — remover é o simples e foi
o escolhido); a timeline mantém o cap de 3 faixas visíveis com o «+N
tracks» de sempre (a lista honesta da 0.8.0).

**A DÍVIDA DO MONO NO ESTADO (fechada depois da H)**: a F deixou o
estado «em mono» no atlas sans porque o 2.º atlas NÃO existia (a dívida
DECLARADA da parte A — §8). A H pagou o atlas (§11) e o estado agora o
usa: o label «N FPS · N TICs» vive entre `monoMode(true/false)` (a MESMA
fonte única do código — as métricas seguem o modo, `fnt()` resolve o
mono, e sem mono carregado o modo cai no sans, a degradação honesta). O
pin `ui_paineis_dock` (e) afere o envolvimento por fonte (o
`countSub` dos DOIS liga/desliga na fonte do BottomPanel).

## §10 PARTE G — MENUS E DIÁLOGOS (as folhas contidas)

**O que mudou** (ui/EditorLayout.h + ui/EditorUi.cpp + ui/UiEditor.cpp):

- **O TETO DE 85% DO ECRÃ**: `kSheetMaxPct = 0.85` (a fonte única em
  EditorLayout.h) — o `menuSheetLayout` (o menu principal) e o
  `browserSheetLayout` (o navegador) capam a altura a 85% do ecrã (o
  MENOR do teto-85 e do cap de contentRect de sempre manda); o menu de
  Cenas também (o clamp inline com o teto da overlayArea — a lista é que
  cede e o SCROLL revela o resto; a região da lista acompanha a altura
  real do sheet).
- **NUNCA POR BAIXO DA BARRA INFERIOR**: o `browserSheetLayout` recebe
  `bottomReserve` (a reserva da barra de 32dp + 8dp de respiro) — o card
  do navegador centra-se na área útil ACIMA da barra e o fundo nunca a
  atravessa. O menu principal já reservava a tab bar (o D14) e continua;
  o menu de Cenas é ancorado com clamp ao topo da barra (o D14 de
  sempre).
- **O BOTÃO FECHAR SEMPRE ACESSÍVEL**: o MENU PRINCIPAL ganhou o X de
  fechar (o `kMenuCloseId` 7411 NOVO — a faixa fixa de 40dp no topo do
  sheet, o glifo de 2 traços cruzados como os irmãos); fechar deixa de
  exigir acertar no véu lá fora. Os outros dois JÁ tinham (o X das
  Cenas 6799 — 0.12-QoL A; o «fechar» do navegador 6899) — a ordem
  confirma.
- **O NAVEGADOR (cabeçalho fixo, caminho, pesquisa, lista com scroll)**:
  a ordem confirma o que o C-4 já fez — o breadcrumb (o caminho que
  salta), o campo de pesquisa 40dp, as raízes e o Subir FIXOS fora do
  scroll e a lista com scroll próprio (≤8 linhas visíveis); o pin prova
  a ordem vertical pela fonte única.
- **O SCROLL COM INDICADOR VISÍVEL**: o indicador da casa (`scroll::`
  `indicator` — o thumb de 3dp com o piso 24) desenha nas regiões de
  scroll das folhas (o mecanismo do endScroll, intocado) — a ordem
  confirma o que já vivia.

**A prova** (o pin `ui_sheets_conteudos`): (a) o teto de 85% no menu E no
navegador em **5 ecrãs** com conteúdo gigante (3000px/64 linhas — o teto
fala sempre); o fundo do navegador ≤ o topo da barra em todos; a ordem
vertical do cabeçalho do navegador (breadcrumb → pesquisa → raízes →
Subir → lista) aferrada; (b) a FONTE — o `kMenuCloseId` vive no menu, o
`scroll::indicator` é o da casa.

**M-UIg** (o teto de 85% morto no menuSheetLayout) vermelho→reposto→
verde: `ui_sheets_conteudos` «o menu passa do teto de 85% (h=640
teto=612)» VERMELHO nos ecrãs M/L; capturado ao vivo. A LIÇÃO da rodada
(a 4.ª vez — 585217f, a B, o TU, agora o HEADER): o build incremental
não recompila os TUs que só INCLUEM o cabeçalho mudado — a mutação em
EditorLayout.h precisou de `touch` no TU do pin para a prova ficar
honestamente vermelha.

**Recalibrações com nota**: test_012c_picker/test_browser/test_012qol
(os chamadores da fonte única levam a MESMA reserva da barra que o
draw), test_wiring090 (a 1.ª linha do menu desceu a faixa do X — o toque
da âncora soma dp(40)), c33_virtual (o tap do par.gltf leva a reserva;
o 17.2 D21 — o offset do scroll sobe para 400 e a caça do rótulo
documentada: as linhas não registam Button — o despacho é do scrollTap —
e o «FERRAMENTAS», de largura parecida, fica FORA da banda visível).

**Verificação**: test_core 0 falhas · c33_virtual **1010/0** ·
theme_hex/ui_vocab/glyph/hierarchy VERDES · docs-lint VERDE ·
release-identity/R-015 VERDE vc 117 · **versionCode: 116 → 117** (o §14
declara o par no MESMO commit).

---

## §11 PARTE H — EDITOR DE CÓDIGO (a ordem do dono + a CAUSA dos defeitos)

**A ordem**: o TOPO diz a que TIC pertence o script (o nome no título) e
os EXPORTÁVEIS do fonte; as ABAS dos scripts abertos (a ativa com o
sublinhado brasa); os nºs de linha discretos; a fonte MONO no código
(«o mono do código é o alvo da fase» — a dívida do 2.º atlas da parte A,
PAGA); a barra de símbolos ENXUTA `{ } ( ) [ ] ; = " . , Tab`; pesquisa,
Run, copiar/colar e desfazer/refazer NA MESMA FAMÍLIA de ícones; a
coloração com a BRASA SÓ nas reservadas (string/número/comentário em
neutras distintas); o fundo da linha atual mais claro; e a REPARAÇÃO do
cursor (o offset) e do desempenho — COM A CAUSA DITA.

**AS CAUSAS ENCONTRADAS** (a auditoria da UI de render; a lógica da
linguagem INTACTA):

1. **O «cursor desalinhado» — a causa real**: os buffers do draw eram
   `char[256]` (a linha, a peça e o prefixo do caret). Linhas acima de
   255 bytes RENDERIZAVAM truncadas (o invariant R-010 «renderizado ==
   guardado» morria no cap) enquanto o caret e o toque mediam o prefixo
   INTEIRO do buffer — a vista e o caret DIVERGIAM com a linha e o erro
   crescia à direita. **A cura**: os caps sobem para 512 (linha/peça/
   prefixo/índice) e a nota do cap honesto fica no código. A geometria
   x/y em si JÁ era única e correta (o R-019/C2/E5 das fases antigas —
   a largura do atlas é aditiva por code point; a auditoria confirmou).
2. **O «tremor» — a causa real**: o scroll-follow do caret era ABSOLUTO
   e corria TODOS os frames: com a linha do caret fora da vista, o
   `scrollSetOffset` SOBRESCREVIA o offset do dedo (o arrasto
   renderizava no caret, o dedo perdia o comando e, no release, a vista
   voltava — rubber-band). **A cura**: o follow só-no-EVENTO — os
   trackers `followCaret/followVersion` no State; o scroll serve o
   caret UMA VEZ por mudança do caret/versão do buffer (a abertura
   inclui), e fora disso o offset é DO DEDO e ninguém o escreve.
3. **O custo por frame** (secundário, medido no código): o culling do C
   (só as linhas visíveis tokenizam) mantém-se; a reconstrução do
   índice por tecla é O(linhas) e fica (a nota honesta — o custo é por
   EDIÇÃO, não por frame; a cura incremental tocaria o cache da
   classificação — fora do escopo da UI).
4. **O cursor «entre as letras» — a causa que a H NÃO viu** (0.9.6.11,
   reportada no device depois da H): o hit-test do toque media as
   larguras com o `fontWidth` global — mas o bloco do toque corre
   DEPOIS do `monoMode(false)` do fim do scroll, enquanto o corpo é
   desenhado em MONO desde a H. O x→coluna media o SANS (proporcional)
   sobre um corpo desenhado em MONO — o desvio acumula ao longo da
   linha e o cursor caía 1-2 caracteres ao lado (tocar entre «a» e «r»
   resolvia outra fronteira). **A cura**: `fontWidthCode` no UiContext
   (o atlas do CÓDIGO — o mesmo do corpo, com a mesma degradação
   honesta sem mono) usado pelo hit-test. Sub-causa no mesmo toque: o
   «- caretInset()» enviesava o x 2dp à esquerda — numa banda à
   esquerda de cada fronteira o cursor resolvia UM carácter cedo; a
   regra do meio (metade esquerda antes / metade direita depois) fica
   EXATA sem ele. A métrica em si já era real e aditiva (`widthOf`
   soma os avanços por code point — a auditoria re-confirmou); nem o
   scroll vertical (o inverso exato `lineAtScreenY`), nem o padding
   (`codeX` — a fonte única), nem floor/round (o meio já era o
   contrato) tinham culpa.

**O por-ficheiro** (8 ficheiros, nenhum novo):

- `ui/ScriptEditor.{h,cpp}` — o título = o NOME DO TIC + o subtítulo
  «exporta: …» (o parser puro `exportedVarsOf` dos `#@+`/`++@+`, ordem
  do fonte, sem duplicados, teto 6); as ABAS (o `TabSnap` dorme o ativo,
  o `switchTab` acorda por move, teto `kMaxTabs=6` — a 7.ª abertura
  derruba a mais antiga INATIVA, o log diz); o UNDO/REDO por EVENTO
  (`UndoStep` = buffer+caret ANTES; texto/del/enter/tab/fix/dica
  empilham UM passo; teto 100; o redo morre com a mutação nova); os
  botões da família (undo/redo/copiar-código/colar na cadeia E1 do
  header — os glifos Undo/Redo/Copy/Paste que JÁ existiam; o ⋯ recebe
  os 7 extras); a barra de símbolos ENXUTA (11 símbolos + o TAB, o
  12.º item, a emitir o MESMO `ime::Event::Tab`); a BANDA da linha
  corrente (o surface sob o texto); os caps 512; o FOLLOW só-no-evento;
  os ids do ⋯/overflow re-manscionados para fora da banda das teclas
  (a colisão latente 6570 morta).
- `ui/Theme.h` — a paleta V.ONI: as reservadas = A BRASA (o MESMO valor
  do accent) e string `#B5AFA6` / número `#D6D2C8` / comentário
  `#7E7E86` / engine `== text2` — neutras distintas, todas ≥4,5:1 no
  bg E na banda (o roxo/azul/amarelo do 0.9.2 morreram).
- `ui/UiContext.{h,cpp}` — o 2.º ATLAS: `fontMono_` + o MODO por-bloco
  (`monoMode(true/false)`) — as métricas e os glifos do mono e o batch
  PRÓPRIO `glyphsMono_` (o endFrame submete-o DEPOIS do sans, a mesma
  regra «texto por último»). Sem mono carregado o modo cai para o sans
  (degradação honesta, nunca um editor morto).
- `ui/FontAtlas.{h,cpp}` — os paths MONO do sistema (DroidSansMono →
  RobotoMono → NotoSansMono → CutiveMono → Cousine → monospace).
- `platform/main.cpp` — o boot do mono (o MESMO ciclo de vida do atlas
  sans: bake no INIT_WINDOW, destroy no TERM); os resultados 8
  (copiar o código)/9 (colar PELO applyEvent — o caminho do IME, o
  undo empilha); o `closeScriptEditor` salva TODAS as abas (o ativo E
  os snapshots, cada um revalidado pelo SEU nome).
- `platform/StorageBridge.{h,cpp}` + `VvActivity.java` — o
  `jniClipboardPaste()` (a leitura do ClipboardManager; vazio = toast
  honesto, nunca crash).

**A prova** (o pin `ui_codigo_editor` + as recalibrações): (a) os
exportáveis (ordem/dedup/teto 6); (b) o undo/redo por evento (o redo
morre com a mutação nova — a mensagem do pin É a da mutação); (c) as
abas (o snapshot acorda com buffer+caret+nome; a 7.ª derruba a mais
antiga; nada troca fora da faixa); (d) os ids fora da banda das teclas;
(e) A SOBREPOSIÇÃO em 2 retratos (as abas ≥44dp, dentro do ecrã,
disjuntas, sob o cabeçalho; o validador da casa 0 problemas). As
recalibrações com nota: R-027 (a barra enxuta — 2 páginas no device,
1 no harness; o TAB no slot 12), test_wiring092 (a família no header —
os toques nos novos x), y1_tokens (a brasa 3×: accent+fireRed+
voniReserved), theme_tabela (o comment #7E7E86), c33_virtual (11.9/12.11
— o corpo desce os 40dp da tira; 11.10/11.11 — os botões nos novos x;
12.7 — a página 1/1 e a ',' no slot 11).

**M-UIh** (o redo sobrevive à mutação nova) vermelho→reposto→verde:
`ui_codigo_editor` «o redo SOBREVIVEU à mutação nova» VERMELHO
(capturado ao vivo no estado mutado); colada aqui.

**As capturas**: o harness do c33 escreve `layout-harness-script.png`
(720×1536 retrato, o editor com a tira de abas + a família no header) e
`layout-harness-device.png` (o device 360dp) — os artefactos do CI.

**Verificação**: test_core **0 falhas** (a suíte completa + o pin novo)
· c33_virtual **1008/0** (os DOIS alvos) · theme_hex/ui_vocab/glyph/
hierarchy/jni_parity VERDES · docs-lint VERDE · scope VERDE ·
release-identity/R-015 VERDE vc 118 · **versionCode: 117 → 118** (o §14
declara o par no MESMO commit).

**O que a parte H NÃO fez** (a lista honesta):

- **Seleção de texto** (arrasto para selecionar, copiar SÓ a seleção):
  o copiar leva o buffer INTEIRO (a ordem não pedia seleção; o caret é
  único desde o G0-1).
- **Pesquisa DENTRO do código** (o find/replace do editor): a lupa é a
  pesquisa das DOCS (o que sempre foi); o find no fonte não foi
  ordenado.
- **Multi-janela/undo entre abas**: o undo é POR ABA (o passo vive no
  snapshot dela); não há histórico global.
- **O cap 512**: linhas acima disso truncam à VISTA (o caret/toque
  seguem o buffer — a nota no código); a cura completa (render em
  fatias) não foi ordenada.
- **A linguagem V.ONI**: INTOCADA (a cláusula da fase — o classificador,
  o compilador e a VM não viram uma linha).

## §12 PARTE I — ABAS ÁUDIO E UI (a ordem do dono)

**A ordem**: o ÁUDIO com a lista de clips SEM SOBREPOSIÇÃO (o rótulo
«CLIPS» nunca por baixo dos botões nem da lista), a waveform com ALTURA
FIXA ~80dp, os botões Importar/Gravar/Play/Stop no estilo da casa, as
ações do clip (renomear/apagar/atribuir a TIC) NUMA LINHA; o UI com o
estado vazio LEGÍVEL, bom contraste e um botão «+» claro.

**O que a auditoria achou** (o defeito real): o rótulo «CLIPS» vivia
DENTRO do conteúdo do scroll e a reserva fixa de baixo (196dp) era
somada a um PISO da lista de 80dp — em viewports baixos (drawer aberto
em paisagem) o empilhamento ESTOURAVA o rect: a waveform (96dp) e as
ações desenhavam PARA ALÉM do workspace (sobre a barra de baixo). A
confirmação de apagar tinha botões de 22dp (o toque mínimo da fase é
40) e o rótulo/estado vazio usavam o token LINE (#26262C — quase
invisível no fundo).

**O orçamento vertical (a cura — a fonte única no AudioWorkspace.cpp)**:
barra de ações 44 → faixa do rótulo CLIPS 22 (FIXA, FORA do scroll) →
lista FLEXÍVEL (piso 40) → waveform FIXA 80 (só entra se a lista ficar
com o piso) → ações do clip 40 ancoradas ao fundo do rect. A lista é
que CEDE; abaixo do orçamento mínimo o workspace NÃO desenha (o guard
honesto). A confirmação de apagar tornou-se MODAL no rect da waveform
(desenhada por último, borda danger, botões de 40dp) — nada cresce para
lá do viewport. O rótulo e o estado vazio («sem clips — toca em
Importar ou Gravar») passaram para o text2 (5,5:1). O overlay de
gravação entrou no orçamento (recH).

**O editor de UI**: os DOIS estados vazios (árvore sem filhos e sem
TIC/canvas) usam o MESMO `drawEmptyState` das tabs do drawer (o helper
da casa — ícone Monitor + mensagem em text2, centrados e clipados) e
ganham o BOTÃO «+» CLARO de 44dp (`kUiEmptyPlusId 6922`) que abre O
MESMO plusMenu de presets do + da barra (o caminho do C-4 — sem TIC o
dispatch cria o TIC 'UI' + o elemento; nada duplicado, o convite sem
ação é fachada e a casa não a tem).

**O por-ficheiro** (4 ficheiros, nenhum novo):
- `ui/AudioWorkspace.{h,cpp}` — o orçamento vertical (os kBtnH/kHeaderH/
  kWaveH/kActH/kListMinH como constexpr comentados); os ids EXPORTADOS
  para o header (o padrão ScriptEditor.h — os pins aférram-nos); a
  faixa CLIPS fora do scroll; a waveform 80dp fixa; o modal de apagar;
  o estado vazio legível; o guard do orçamento.
- `ui/UiEditor.cpp` — os dois estados vazios legíveis com o botão «+»
  (o plusMenu do C-4); os includes BottomPanel/LayoutDump.
- `ui/EditorLayout.h` — o `kUiEmptyPlusId 6922` (a cauda dos ids de
  estado vazio 6920/6921).

**A prova** (o pin `ui_audio_ui` no test_fase_ui.cpp): (a) o orçamento
em viewport 1000×500 — NADA fora do rect, a faixa CLIPS com O rótulo e
SEM botão nenhum, a waveform 80dp no registo, as 4 teclas ≥44, as 2
linhas ≥40 sob a faixa, as 3 ações NUMA linha ancoradas ao fundo, o
validador da casa 0 problemas; (b) o MODAL de apagar com os botões de
40dp dentro do rect; (c) o vazio sem waveform + o guard honesto no
viewport 90dp (devolve 0); (d) o vazio do editor de UI com o «+» de
44dp e O TOQUE a abrir o plusMenu (st.plusMenu — a fachada morre).
Recalibração com nota: wiring011_workspace_toques (a geometria nova —
a faixa 22, a waveform 80, as ações no fundo, o APAGAR do modal no
rect da waveform).

**M-UIi** (o rótulo CLIPS morre do draw) vermelho→reposto→verde:
`ui_audio_ui` «o rótulo CLIPS não está na faixa fixa (0 rótulos)»
VERMELHO ao vivo no estado mutado (com o .o recompilado — a lição do
TU); colada aqui.

**As capturas**: o sweep FASE 31 do c33 (5 ecrãs ≥640dp, 3 paisagens ×
2 retratos) varre o «modo-audio» e o «modo-ui» — o layout NOVO passa
pelo validador em todos (c33 1008/0); os PNGs y-modo-audio/y-modo-ui
regeneram-se na corrida.

**Verificação**: test_core 0 falhas (GONI_SKIP=r039,streaming_paralelo
— os flakes de disco da sandbox documentados; o CI é o árbitro) ·
c33_virtual 1008/0 · theme_hex/ui_vocab/glyph/hierarchy/projects_ui
VERDES · docs-lint VERDE 121 · release-identity/R-015 VERDE vc 119 ·
**versionCode: 118 → 119** (o §14 do RELATORIO-0.9.6.md declara o par
no MESMO commit).

**O que a parte I NÃO fez** (a lista honesta):

- **Trim/corta do clip** (a dívida 🔶 desde 0.8.11): continua fora do
  escopo (a ordem da parte I não o pedia).
- **A gravação a reescrever o layout**: o overlay de gravação entra no
  orçamento (a lista cede) mas não redesenha nada por cima — a barra
  de nível continua a de 10dp de sempre.
- **Viewports abaixo de ~164dp** (paisagem com o drawer na altura
  máxima + overlay de gravação): o workspace não desenha (o guard) — a
  degradação declarada; o host de hoje nunca o manda tão baixo.
- **As capturas de device reais**: as da fase vêm do harness do c33
  (FASE 31); o device físico é do dono.
- **O Java/Importador/ÁudioEngine**: INTOCADOS (a cláusula da fase —
  o workspace é só visual/layout).

## §13 PARTE J — TELA DE PROJETOS (a ordem do dono)

**A ordem**: os cartões com MINIATURA DA CENA ou O ÍCONE DA ENGINE
(acabar com as letras genéricas «TE»/«AS»), o nome, a data e o ⋮;
«Novo projeto» como o ÚNICO botão em brasa; «Importar projeto»
SECUNDÁRIO (contorno); a pesquisa e o ordenar discretos; a MESMA
paleta/tokens do editor.

**O que a auditoria achou**: o fallback do card sem thumb.png era o
`InitialsThumbView` — o tile de INICIAIS (as letras «TE»/«AS» que o
dono mandou acabar) com uma paleta de 6 matizes fora do Theme. O
«Importar projeto» era contorno com BORDO E TEXTO EM BRASA — competia
com o primário (a regra da casa: o destaque fica APENAS no
ativo/selecionado; a ordem: «Novo projeto» é o ÚNICO em brasa). O
resto — nome/data/⋮, a pesquisa discreta (vidro + border + lupa
text2), o ordenar de um só ícone 48dp e os tokens da FASE UI A — já
viviam (a ordem confirma).

**A cura (o por-ficheiro — 2 ficheiros, nenhum novo)**:
- `ProjectManagerActivity.java` — o `InitialsThumbView` MORREU (com a
  fórmula de iniciais D18 e a paleta de 6 matizes fora do Theme); o
  fallback é o `BrandThumbView` NOVO: o tile da casa (SURFACE2 + bordo
  1dp BORDER) e O ÍCONE DA ENGINE desenhado pela ÚNICA função
  (`UiIcons.drawBrand` — o espelho Java da Brand.cpp, nunca o PNG do
  launcher) na COR FRACA (o BORDER — o mesmo G fraco do estado vazio:
  o tile nunca compete com o nome). A miniatura da cena continua a ser
  o thumb.png quando existe (o D16 retry intacto). O `Button24` em
  variante contorno passou a SECUNDÁRIO NEUTRO: bordo BORDER 1dp +
  texto/ícone TEXT1 (a brasa fica SÓ no primário).
- `scripts/projects_ui_check.py` — o gate recalibrado com a ordem: o
  BrandThumbView OBRIGATÓRIO, o InitialsThumbView/initialsOf
  PROIBIDOS (as letras não voltam), o drawBrand pela ÚNICA função, o
  contorno NEUTRO (`b.setTextColor(outline ? TEXT1 : ACCENT_INK)`) e
  os 5 matizes fora da allowlist de tokens.

**A prova**: o gate `projects_ui_check` VERDE (16 checks) + **M-UIj**
(a brasa volta ao contorno do secundário) vermelho→reposto→verde —
`EXIT 1` ao vivo no estado mutado («variante contorno NEUTRA ausente
— Novo projeto é o ÚNICO botão em brasa»), VERDE depois de reposto.

**As capturas**: o ecrã de projetos é JAVA (a JVM do hospedeiro não o
instancia — a cultura do jni_parity); a prova estrutural é o gate (o
padrão 0.9.0 spec F); as capturas de device ficam para o dono (a
lista honesta abaixo).

**Verificação**: theme_hex/projects_ui/docs-lint VERDES ·
release-identity/R-015 VERDE vc 120 · a compilação do Java é o passo
APK do CI (o árbitro que apanhou o ternário do colar na H) ·
**versionCode: 119 → 120** (o §14 do RELATORIO-0.9.6.md declara o par
no MESMO commit).

**O que a parte J NÃO fez** (a lista honesta):

- **Miniatura da cena REAL gerada pelo engine para TODOS os projetos**:
  o thumb.png é escrito pelo engine a cada save (o comportamento
  atual); os projetos mais velhos sem thumb ficam com O ÍCONE DA
  ENGINE (a ordem confirma as duas opções do dono).
- **As capturas de device reais da tela**: a JVM do hospedeiro não
  desenha Android; o gate estrutural é a prova da fase (o mesmo
  padrão das fases 0.9.x).
- **A pesquisa e o ordenar**: já eram discretos (a ordem confirma) —
  nada mudou neles.
- **O core/V.ONI/importadores**: INTOCADOS (a cláusula da fase — só o
  visual do ecrã Java).
