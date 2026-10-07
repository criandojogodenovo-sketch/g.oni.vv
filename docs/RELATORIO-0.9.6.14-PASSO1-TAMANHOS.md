# RELATORIO 0.9.6.14 — UI PASSO 1: TAMANHOS (spec do dono)

> A spec do dono (REGRAS): medir antes de mexer; a TABELA DE MEDIDAS no fim
> de cada passo em px/dp/%; commit + CI verde; o validador (a auditoria
> FASE 13) com 0 violações; ZERO alterações em V.ONI, selection, physics,
> TICs. PASSO 1 = TAMANHOS — só dp, sem lógica nova.

## 1. O QUE MUDOU (a tabela da spec → o código)

| Medida da spec | Era | Fica | Fonte única |
|---|---|---|---|
| Top bar UMA linha | 56dp | **36dp** | `safe::kTopBarH` (ui/SafeArea.h) |
| Ícones da barra | 24dp | **20dp** | `toolbar::kIconS` / `theme::kIcon` |
| Desenho do botão da barra (chip) | o alvo todo | **28dp** no alvo = a linha | `toolbar::kChipH` |
| Cabeçalho dos painéis | 48dp | **28dp** | `kHeaderH` (ui/EditorUi.cpp defs) |
| Linha de lista | 48dp | **36dp** | `kRowH` |
| Campo/pesquisa | 48dp | **32dp** | `kSearchRowH` |
| Caixas X/Y/Z do Transform | 48dp | **32dp** (o R: 40×32) | `inspTransformRowH` (ui/EditorLayout.h) |
| Cabeçalhos de secção do Inspector | 48dp | **28dp** | `inspSectionH` |
| Miniaturas de Material | 64dp | **44dp** (+legenda 12sp) | `inspThumbsH` |
| Tab bar de baixo | 48dp | **32dp** | `safe::kBottomTabH` |
| «FPS · TICs» | faixa de status 24dp | **canto direito da tab bar** (reserva 112dp) | `bottom::kFpsW` (ui/BottomPanel.h) |
| STATUS BAR extra | 24dp | **REMOVIDA** (altura 0; versão/commit → Settings › Sobre) | `safe::kStatusH = 0`; `drawStatusBar`/`StatusBarData` apagados |
| Stack undo/redo/save/dup/paste | 48dp | **40dp** alvo / 32 desenho | `vpchrome::kStackBtn` (ui/ViewportChrome.h) |
| Toolbar Selecionar/Mover/Rodar/Escalar/Ímã | 48dp | **40dp** / 32 | `vpchrome::kToolBtn`/`kBottomH` |
| Strip do topo da viewport | 48dp | **40dp** | `vpchrome::kStripH` |
| Piso do viewport com drawer | 104dp | **88dp** (strip 40 + toolbar 40 + folga 8) | `safe::kViewportMinH` |
| Menus (plus/settings) | linhas 56/passo 64 | **40/passo 48** | `drawPlusMenu`/`drawSettingsMenu` (ui/EditorUi.cpp) |
| Campo de pesquisa das Docs | 48dp | **32dp** | ui/DocsScreen.cpp |

**A LEI DE OURO** (agora constante, não comentário): `layout::kTouchFloorDp
= 40` (botão solto), com os pisos DE LINHA `kRowFloorDp = 36`,
`kFieldFloorDp = 32`, `kHeadFloorDp = 28` (ui/LayoutDump.h). `theme::kTarget
40 / kIcon 20` (ui/Theme.h). NADA no editor chega a 48dp.

## 2. O VALIDADOR (a vara de medir acompanha a spec — 0 violações)

O piso único de 48dp da auditoria (ToquePequeno) media a casa VELHA: com o
layout novo, TODOS os alvos de 40dp davam aviso. O validador passou a aferir
o piso DA CLASSE da entrada (a tabela da spec do dono):

- botão SOLTO: 40dp (era 48);
- LINHA (top bar, listas, consola): 36dp via `Entry.rowFloorDp`;
- CAMPO (X/Y/Z, tabs de baixo, pesquisa): 32dp;
- CABEÇALHO (chips de cabeçalho): 28dp;
- o piso compacto de 40dp (GRUPO E/R-027) mantém-se no validador.

O mecanismo é o mesmo da tecla compacta (`auditCompactNext_`): o desenhista
chama `ui.auditRowFloorNext(layout::kRowFloorDp)` ANTES do hit; a flag vive
SÓ até à 1ª entrada interativa (nunca vaza). As constantes são NOMEADAS do
LayoutDump.h — números frouxos não passam.

**ACHADO (o falso «SANGRA»)**: o `auditLabel_` gravava a largura SEM o
fator de estilo (sp) — as labels caption (12sp) ficavam ~14% mais largas NO
REGISTO do que os glifos desenhados. O primeiro caso real foi o «FPS · TICs»
da tab bar (a 4px da borda direita). CORREÇÃO: o registo grava a largura
DESENHADA (`widthOf × k` total) — o registo É o draw (a lição R-020).

## 3. O QUE SAIU / ENTROU NO «SOBRE»

- Settings › Sobre ganhou a linha **git** (o commit curto —
  `buildinfo::g_git`; «—» no dev) ao lado do «so sha256».
- A status bar (versão · projeto · FPS · TICs · estado · Mobile First)
  saiu do ecrã. O nome do projeto longo deixou de se desenhar — o
  `ellipsizeMiddle` do J4 (R-025) fica nas unidades puras do
  test_sentinels + o walkthrough da FASE 14.4 passou ao rodapé NOVO (a
  tab bar).

## 4. MUTAÇÕES (a prova de que as sentinelas vigiam)

| Muta | O que partiu | Vermelho |
|---|---|---|
| M1 | o consumo da flag `auditRowFloorNext_` desligado (`if (false && …)` em UiContext::auditAdd_) | `regress_toque_48dp_validador` FALHOU (o [+] e a pesquisa saem do seu piso) + c33 FASE 13.2 «o editor esta VERDE» FALHOU (4 checks) — o validador dispara ToquePequeno nos 36/32/28 sem flag |
| M2 | `kBottomTabH` 32→48 (o mundo velho de volta) | 17 falhas: safearea (painéis), R-025 (`tb.h == 32`), c33 12.10 |

Repostas → test_core 0 falhas · c33_virtual 456/456 · gates verdes.

## 5. SENTINELAS REESCRITAS (a casa não guarda sentinelas vazias)

- **R-025 `regress_rodape_intocavel`**: a TAB BAR de 32dp é a última faixa
  viva em 2 densidades × 3 estados do drawer (a status saiu; a faixa compat
  é de altura ZERO); as unidades puras do `ellipsizeMiddle` ficam intactas.
- **R-027 (a exceção compacta)**: reescrita ao novo piso — 39dp FALHA em
  todas as classes; o botão REGULAR de 40dp PASSA (a lei de ouro); os pisos
  DE LINHA 36/32/28 passam flagados e falham 1dp abaixo, SEM flag a mesma
  linha de 36 falha no piso 40 (a flag nunca vaza).
- **FASE 14.4 (c33)**: o rodapé do replay passou à banda da tab bar —
  labels presentes, NENHUMA truncada, com o projeto de 120 glifos.
- **FASE 12.10/13.7**: a vara da densidade mede 72px @2.0 (36dp) e os alvos
  do chrome ≥40dp no device.

## 6. TABELA DE MEDIDAS — PASSO 1 (o script: scripts/passo1_medidas.cpp,
mede o SafeArea.h REAL + as constantes citadas do fonte)

```
===== PERFIL A harness-C33 (FASE 13) — superfície 1536x720 px @1.0 =====
contentRect:   1536 x    720 px  (1536.0 x  720.0 dp)  100%
TOP BAR (kTopBarH=36)            1536 x   36 px (1536.0 x  36.0 dp)  100.0%larg x  5.0%alt
BOTTOM TAB BAR (kBottomTabH=32)  1536 x   32 px (1536.0 x  32.0 dp)  100.0%larg x  4.4%alt
STATUS BAR (kStatusH=0 REMOVIDA) 1536 x    0 px                       0.0%alt
VIEWPORT REGION (lógico)         1536 x  652 px                      90.6%alt
PAINEL HIERARQUIA (default)       300 x  652 px (300 x 652 dp)        19.5%larg
PAINEL INSPECTOR (default)        300 x  652 px                       19.5%larg
VIEWPORT CENTRAL (3D)             936 x  652 px                       60.9%larg x 90.6%alt
topo(36)+tab(32)+status(0) = 68 px = 9.4% da altura   [critério c: ≤20% ✓]
viewport central: 60.9% da largura [critério a: ≥55% ✓]
COBERTURA da viewport por controlos: 19.0%  [critério b: ≤10% ✗ — PASSO 3]
drawer default 240dp → efetiva 240dp (cap 88) → viewport fica 412dp

===== PERFIL B device RMX3624 — superfície 1600x720 px @2.0 (776x336dp) =====
contentRect:   1552 x    672 px  ( 776.0 x  336.0 dp)  100%
TOP BAR (36dp)                    1552 x   72 px (776.0 x  36.0 dp)  100.0%larg x 10.7%alt
BOTTOM TAB BAR (32dp)             1552 x   64 px (776.0 x  32.0 dp)  100.0%larg x  9.5%alt
STATUS BAR (0 — REMOVIDA)            0 x    0 px                       0.0%alt
VIEWPORT REGION (lógico)          1552 x  536 px (776.0 x 268.0 dp)   79.8%alt
PAINEL HIERARQUIA (default)        400 x  536 px (200 x 268 dp)       25.8%larg
PAINEL INSPECTOR (default)         576 x  536 px (288 x 268 dp)       37.1%larg
VIEWPORT CENTRAL (3D)              576 x  536 px (288 x 268 dp)       37.1%larg x 79.8%alt
topo(36)+tab(32)+status(0) = 68 dp = 20.2% da altura  [critério c: ≤20% —
  a 0,2pp do alvo; 68dp são os FIXOS da spec (36+32+0); o resto do passo
  dos PAINÉIS/VIEWPORT decide os critérios a/b]
viewport central: 37.1% da largura [critério a: ≥55% ✗ — PASSO 2]
COBERTURA da viewport por controlos: 84.0%  [critério b: ≤10% ✗ — PASSO 3]
drawer default 240dp → cap pelo piso 88dp → viewport central fica 92dp

===== WIDGETS (dp — a LEI DE OURO: desenho 32 / toque 40) =====
TOP BAR linha / desenho / ícones        36 / 28 / 20
CABEÇALHO dos painéis                   28
LINHA de lista (hierarquia/Nós)         36
CAMPO de pesquisa / caixas X/Y/Z        32 / 32
Botão R (repõe linha)                   40x32
Cabeçalhos de secção do Inspector       28
Miniaturas de Material (+legenda)       44
TAB BAR de baixo                        32
STATUS BAR                              0 — REMOVIDA
«FPS · TICs» (reserva na tab bar)       112 de largura
STRIP do topo da viewport               40 (chips 32)
STACK undo/redo/save/dup/paste          40 (desenho 32)
TOOLBAR Sel/Mov/Rod/Esc/Ímã             40 (desenho 32)
PEGA do drawer                          12
PISOS do validador (solto/linha/campo/cabeç) 40/36/32/28
```

**Leitura honesta do baseline**: o critério (c) passou de 38,1% → 20,2%
(os 68dp fixos da spec no device de 336dp de altura); o (a) e o (b) continuam
a falhar NO DEVICE — é exatamente o trabalho dos PASSOS 2 (painéis:
hierarquia 18%/inspector 22%) e 3 (viewport: controlos ≤10% a 60% alfa).
No harness (A) os critérios a/c já cumprem; o b é do PASSO 3.

## 7. O INCIDENTE P-01 (scope-check) — o .gitignore

O commit do PASSO 1 levou DUAS linhas de higiene local no `.gitignore`
(artefactos do harness escritos no CWD dos runs do c33). O `.gitignore`
NÃO está em `ci/scope.txt` — o gate scope-check ficou VERMELHO no push
(exatamente o desenho da cláusula P-01: tocar fora do scope = CI vermelho).
RESOLUÇÃO SEM alargar o scope: as duas linhas REVERTIDAS (commit 0.9.6.14b)
— o `.gitignore` volta ao estado aprovado; os artefactos eram lixo LOCAL e
foram apagados do disco. Se o dono QUISER as linhas (evitam lixo no CWD dos
runs locais do harness), editar o ci/scope.txt É o pedido — não decidimos
por ele. O run seguinte (87dc088) permaneceu vermelho porque o gate compara
o RANGE do push (07e74da..87dc088 = só a reversão); este commit é o primeiro
cujo range não contém o .gitignore.

## 8. NÃO VERIFICADO (honesto)

- O toque no DEVICE (RMX3624) — os alvos de 40/36/32/28dp medem certo no
  código e no validador, mas o conforto do dedo é julgamento do dono
  (P-07): a checklist device fica para o sign-off.
- O CI (release.yml) corre no push — o verde local não é o verde do CI.
- O fontScale do SISTEMA (fontes grandes do Android) não foi testado —
  as linhas de 36/32 são pisos dp; com sp grandes o texto pode truncar
  (o labelFitted trunca honesto, o validador conta).
- Os 0,2pp do critério (c) no device: os 68dp são os fixos da spec; se o
  dono quiser <20% exato neste device, é decidir 1dp menos num passo
  seguinte (não decidimos por ele).
