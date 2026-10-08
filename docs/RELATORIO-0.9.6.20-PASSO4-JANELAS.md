# RELATÓRIO 0.9.6.20 — PASSO 4: AS JANELAS (BLOCO B)

> Spec do dono (a linha PASSO 4 · JANELAS do BACKLOG): «logs opacos com
> corte/scroll/wrap (80% do ecrã); menus ancorados com fundo 40%; settings
> linhas 36dp, secundários só-contorno, «concedido» como texto; cantos
> suaves». O hotfix 0.9.6.18 (D4) já tinha adiantado os 3 sub-itens do
> Settings (o botão outline compacto, «concedido» como texto e a tabela
> localizada) — este bloco fecha o RESTO, num commit, sem misturar blocos.
> NADA mais tocado: V.ONI, seleção, física, TICs, render do jogo, storage,
> import. Zero funcionalidades novas.

## 1. O QUE ENTROU (o contrato J-01..J-04)

| Item | A spec do dono | O que o código faz agora |
|---|---|---|
| **J-01 · LOGS** | «logs opacos com corte/scroll/wrap (80% do ecrã)» | o card do `drawLogViewer` é OPAQUE (`theme::kTheme.bg` α1.00 — o vidro surface α0.80 deixava a cena atravessar o log de crash), mede 80% × 80% da faixa útil (era 86% × 80% — a LARGURA passava o cap) e as linhas longas QUEBRAM por palavras (`textwrap::wrap`, a fonte única das Docs) com o contentH a somar as LINHAS VISÍVEIS; o corte (`labelFitted` por linha) fica como rede e o scroll (`beginScroll` id 43) é o de sempre |
| **J-02 · O VÉU DOS ANCORADOS** | «menus ancorados com fundo 40%» | token NOVO `theme::kTheme.scrimMenu` (preto α0.40) nos TRÊS menus ancorados da casa (fileMenu ⋯ / scenesMenu Cena ▾ / hierMenu ⋮); o `scrim` 60% fica sendo o véu MODAL (sem utilizadores vivos hoje — os cards centrados não têm véu; o token fica para o futuro, documentado) |
| **J-03 · SETTINGS** | «linhas 36dp» | `settings::settingsRowH()` 48→36dp (a FONTE ÚNICA, agora EXPORTADA no header); os cabeçalhos de secção mantêm 48dp (a spec manda nas LINHAS) e os controlos de linha mantêm os 28dp da casa — o toggle e o botão compacto D4b centram a dp(4) na linha de 36 |
| **J-04 · CANTOS** | «cantos suaves» | os últimos 5 cards com painel/frame DUROS ganham `panelRounded/frameRounded` com `dp(kRadiusCard)` — drawStorageDialog, drawImportMenu (EditorUi) e drawRemoveDialog, drawTextInput (o buffer interno com `dp(kRadiusField)`), drawApplyDialog (UiEditor) — e os TRÊS sheets ancorados passam a medir o raio em dp REAL (era o token cru 8px = 4dp a 2.0, a classe R-018) |

## 2. AS CAUSAS (ficheiro + função)

- **J-01** `ui/EditorUi.cpp` `drawLogViewer` — (a) o card usava
  `theme::PANEL` (o alias do surface = vidro 80%): sobre a cena viva o
  log de crash lia-se através dos TICs; (b) `w = aw * 0.86f` (o dono
  mandou 80%); (c) cada linha entrava num `labelFitted` único — o
  textfit CORTA com «…» no fim (`TextFit.h ellipsize`), nunca quebra:
  a cauda de uma linha de crash longa morria. A cura usa o
  `textwrap::wrap` (o MESMO quebrador por palavras das Docs, medidor
  injetado, fronteiras de code point) DUAS vezes: para o contentH do
  scroll (as linhas visíveis) e para o draw (cada linha visual num
  `labelFitted` — o passo-through é literal quando cabe, o corte fica
  de rede para o caso patológico de um glifo mais largo que o painel).
- **J-02** `ui/EditorUi.cpp` `drawFileMenu`/`drawHierMenu`/
  `drawScenesMenu` — as três desenhavam o véu full-area com
  `theme::kTheme.scrim` (preto 60%, o véu modal de sempre). O dono
  manda 40% nos menus ANCORADOS (a cena continua legível à volta do
  menu). O token novo vive na TABELA do Theme.h (a fonte única de
  sempre; um flip muda a app toda).
- **J-03** `ui/SettingsPage.cpp` `settingsRowH()` — 48dp desde a 0.9.0
  (spec I). A linha do dono do PASSO 4 manda 36dp. A fonte única era
  interna ao .cpp — EXPORTADA no header (o padrão `actionBtnRect` do
  D4b) para o draw, o contentH e os testes partilharem. As duas
  constantes mortas do header (`kSectionH`/`kRowH` constexpr sem
  utilizadores) saíram no mesmo commit.
- **J-04** os cinco vivos listados na tabela §1 + o ACHADO: os 3 sheets
  ancorados passavam `theme::kRadiusCard` CRU ao `panelRounded` (8px —
  nos 2.0 do device são 4dp de raio: metade do pedido; a exata classe
  R-018 que o D14 curou nas MEDIDAS dos sheets). **`drawSettingsMenu`
  é código morto** (a página Settings 0.9.0 substituiu-a; só o
  test_ui a chama) — NÃO tocado; candidata a remoção (decisão do dono,
  no BACKLOG).

## 3. AS PROVAS (suítes + gates + pixels)

- **test_core**: `regress_janelas_passo4` (R-037) — os pins por item
  (§4 do contrato, `docs/LAYOUT_HIERARCHY.md` §2.25-§2.28). **0 falhas**
  na suíte inteira.
- **c33 FASE 18 NOVA** (o device @2.0 — 631 checks no total, **0
  falhas**): provas por PIXEL com o `loadPng` de produção sobre o PNG
  do export (o método 13.1/13.9) — e o ACHADO do export: ele nomeia os
  ficheiros pelo ECRÃ corrente (`currentScreenName()`):
  `menu_ficheiro`/`cenas`/`logs`/`settings`/`storage` — ler
  `layout/editor.png` com um overlay aberto devolvia o PNG STALE do
  frame anterior (a 18.1-18.4 leem o nome CERTO por estado).
  - **18.1 (J-02)** o véu do menu ⋯ (aberto pelo TAP real do botão
    [Menu] — o caminho do dono, com o offset 0 do D14) mede
    `blendOver(scrimMenu, bg)` no pixel; o vidro do sheet mantém
    surface 80% SOBRE o véu (o composto duplo); o menu de cenas mede o
    MESMO token.
  - **18.2 (J-01)** o card do viewer mede ≤80% da faixa nos dois eixos
    (do REGISTO, excluindo o quad full-screen da cena) e o pixel
    interior é o `#0E0E10` EXATO (opaco — a cena não atravessa).
  - **18.3 (J-03)** a linha (o alvo do toggle Imersivo) mede 36dp e o
    cabeçalho de secção 48dp no registo do device.
  - **18.4 (J-04)** o canto do diálogo de armazenamento é SUAVE (o
    pixel (2,2) fica fora do raio 8dp e mostra o fundo — o painel duro
    mostraria o vidro) e o interior é o vidro surface sobre o fundo.
  - **18.5** o editor limpo passa o VALIDADOR INTEIRO (0/0).

## 4. AS MUTAÇÕES (vermelho → verde, repostas por cp dos backups)

| Mutação | A regressão | O pin que fica vermelho |
|---|---|---|
| M-J1 | o wrap morre (uma linha visual por entrada no draw) | `regress_janelas_passo4` (a contagem de labels != o wrap puro) |
| M-J1b | o card volta ao vidro (PANEL) | c33 18.2 (o pixel deixa de ser #0E0E10 exato) |
| M-J2 | o véu volta ao 60% (`scrim`) | c33 18.1 (2 checks: o véu + o composto duplo) |
| M-J3 | a linha volta a 48dp | `regress_janelas_passo4` + o tap do test_wiring091 |
| M-J4 | o diálogo volta ao painel duro | c33 18.4 (o canto mostra o vidro) |

## 5. OS PNGS P-05 (docs/p05, com o device @2.0)

- `hotfix20-device-menu-veu40.png` — o menu ⋯ aberto pelo tap (offset
  0, cantos dp, o véu 40%, o «Novo objeto» do D21 visível).
- `hotfix20-device-logs.png` — o viewer de logs opaco com a linha
  longa quebrada.
- `hotfix20-device-settings-36.png` — a página de Settings com as
  linhas de 36dp (todas as secções abertas).
- `hotfix20-device-storage-cantos.png` — o diálogo de armazenamento
  com os cantos suaves.

## 6. RECALIBRAÇÕES (o preço honesto da spec)

- `tests/test_ui.cpp` — o teste do viewer media a geometria antiga
  (86%); recalibrado a 80%.
- `tests/test_wiring091.cpp` — o tap na linha do Diagnóstico (a
  fórmula 48dp); recalibrado a 36dp (a linha de 36 + meia linha 18).
- `tests/c33_virtual.cpp` — os taps do Settings da 9.6 (a linha Docs)
  e da 12.9 (o bench: a linha do Correr bench + a linha seguinte do
  Copiar) recalibrados a 36dp.

## 7. O SIGN-OFF PARA O DONO (o PORTÃO B)

Com o APK do commit 0.9.6.20 (versionCode 53), verificar no device:

1. **Logs**: menu ⋯ → Diagnóstico → «Ver logs» — o fundo NÃO deixa a
   cena ver-se; uma linha longa de log QUEBRA (não termina em «…»);
   a janela ocupa ≤80% do ecrã.
2. **O véu**: abrir o menu ⋯ e o menu de cenas — a cena à volta do
   menu escurece LEVEMENTE (40%; antes 60%).
3. **Settings**: as linhas mais compactas (36dp), os cabeçalhos como
   estavam; o toggle e os botões centram nas linhas.
4. **Cantos**: o diálogo de armazenamento (Permissões → All Files),
   o seletor de importação, o diálogo de remover TIC, o diálogo de
   renomear e o de aplicar — TODOS com os cantos arredondados dos
   cards (8dp).

## 8. NÃO VERIFICADO (honestidade da casa)

- **O véu 40% sobre a CENA REAL no device físico**: as provas de pixel
  são do device virtual (@2.0, cena controlada). No device com o jogo
  a correr atrás, a leitura visual do 40% cabe ao dono (o §7.2).
- **A interpretação do «fundo 40%»**: li como o VÉU (a área à volta do
  menu ancorado). A alternativa (o vidro do próprio menu a 40%)
  quebraria as pisos de contraste da casa (text2 cai abaixo de 4,5:1
  sobre vidro 40% sobre cena clara) — se o dono a quiser, é uma
  decisão nova (no BACKLOG).
- **O `drawSettingsMenu` morto**: nenhum gate o apanha; a remoção é
  decisão do dono (BACKLOG).
- **O `scrim` 60% ficou sem utilizadores vivos** (os cards centrados
  nunca tiveram véu): o token mantém-se documentado como o véu MODAL.
