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
