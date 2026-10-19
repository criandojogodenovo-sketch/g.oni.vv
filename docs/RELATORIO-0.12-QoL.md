# RELATORIO-0.12-QoL — UI Polish, Navegador de Ficheiros e Auto-Save

A campanha 0.12-QoL (a ordem do dono): refinamento de UI 2.5D e
persistência, com a CLÁUSULA CALMA — sem quebrar lógica de motor (o
V.ONI, a física, o .goni/.gmesh byte a byte e os TICs ficam intactos).

Cada sub-fase fecha com commit + push próprios, o STATE.md no MESMO
commit (regra §6) e CI verde. Este relatório cresce por secções: o A no
§1, o B no §2, o C no §3, o D no §4.

## §0 A abertura (as verificações que bloqueiam tudo)

1. **vc 101 ou superior?** SIM — o STATE.md declara vc 101 e o
   `app/build.gradle` tinha `versionCode 101` (a 0.12-X fechou o par).
2. **Os gates verdes?** SIM — o run do CI do commit `69382a6` (o fecho
   da 0.12-X) acabou em **success** nos 5 jobs (core ✓ c33 ✓
   entry-symbols ✓ APK assinado ✓ keystore skip esperado).
3. **O STATE.md tem a 0.12-X como fechada?** SIM — o §1 declara
   «Campanha 0.12-X (CÂMARA DE TERCEIRA PESSOA) — FECHADA (3.9x, vc
   101)».

Leitura obrigatória feita antes de codar: STATE.md, CODEMAP.md,
ui/Theme.h, ui/EditorUi.cpp, ui/Toolbar.cpp, ui/SettingsPage.cpp (mais
os contratos que as sub-fases tocam: ui/Icons.h, ui/EditorLayout.h,
ui/ViewportChrome.cpp/h, ui/CamGizmo.h/.cpp, render/Grid.h/.cpp,
core/Project.h, core/ProjectStorage.h, tests/test_012c_*.cpp,
tests/test_sentinels.cpp, tests/c33_virtual.cpp, ci/scope.txt).

Decisão de compat registada já na abertura: o QoL-B manda ocultar
FICHEIROS que começam por ponto — a decisão 0.8.5/0.12-C (os ficheiros
ponto ficam) é SUPERADA pela ordem expressa do dono neste prompt; a
entrada do §4 do STATE.md atualiza-se no commit do B.

## §1 A — Limpeza Visual da UI (o feedback do Redbr1)

**O que mudou** (vc 102):

1. **Top bar sem o texto do nome.** O slot `name` e o flag `showName`
   MORRERAM do `TopBarLayout` (ui/Toolbar.h — o padrão D21: sem
   chamadores, sem fantasmas). O `topbarLayout` já não reserva os 48dp
   do nome (o degrau k<0.78 da degradação morreu — o CHIP da plataforma
   passou a ser o primeiro a sair, k<0.62) e o `drawTopBar` desenha
   APENAS o «G em chama» 24dp (o glifo da D-4 segue vivo — é a
   identidade). A barra respira: o Menu começa onde o logo acaba.
2. **A pesquisa da hierarquia recolhida numa LUPA.** A barra «pesquisar
   TIC» (o campo 32dp sempre visível) vive RECOLHIDA num chip 32×32 com
   o ícone de lupa, à direita da linha (a coluna das ações do
   cabeçalho). O toque EXPANDE para o campo de texto (e abre o teclado,
   propósito 8 — o mesmo de sempre); com texto o campo mantém-se aberto;
   o × (28dp, dentro do campo à direita, id 5503) limpa e RECOLHE. O
   estado novo `EditorState::hierSearchOpen` (ui/EditorUi.h) guarda a
   expansão; a altura da linha (kSearchRowH) fica IGUAL — a geometria da
   lista não muda.
3. **As bordas dos inativos em cinza #24242A.** O token `border` do
   Theme.h deixa de ser a hairline ÂMBAR #4A3714 α0.60 (a aresta do
   cristal da 0.11-Y/0.12-C) e passa a **#24242A α1.00** — o cinza
   escuro do ELEVADO, o RGB que o dono pediu. A mudança de UM token
   arrasta: o alias `LINE` do UiContext.h (ligado por static_assert), o
   divisor ENTRE PAINÉIS (`dividerGradientV` — a cor cai do border ao
   invés do accentDim, o gradiente α0.90→0.15 mantém-se) e TODAS as
   molduras `frameRounded` de chips/campos/panéis inativos (o [+] e o ⋮
   do cabeçalho, os campos X/Y/Z, o campo de pesquisa, o chip de
   plataforma, o card do NAVEGADOR, o diálogo APLICAR, o painel da
   Timeline, os menus CLIPS/NOVO TRACK/SETTINGS/PRIMITIVA, o botão JUMP).
   O ÂMBAR fica APENAS para o ativo/selecionado: o gradiente fogo dos
   botões ativos, o underline da tab, o fill accentDim da seleção, o
   gravador de áudio ativo, o waveform do clip SELECIONADO, a moldura do
   device no Play (pinada pela B-3), o glow de foco.
4. **Ar no Inspector.** Os vãos ENTRE os campos X/Y/Z e antes do Reset
   sobem 8→12dp (`kTfGapBox`/`kTfGapReset`); a margem mínima da linha
   (o regime do padding cedido) sobe 4→8dp (`kTfRowPadMin` — o Reset
   NUNCA cola à borda do ecrã); a folga vertical de cada linha Transform
   sobe 4→8dp (`inspTransformRowH` 64→68 — mais ar entre as linhas e os
   botões). O preço fechado com o dono em vista: o piso do valor 26→22dp
   (`kTfValueMinDp` — «-12»/«45»/«1.2e» continuam a ler-se) e a escada
   R1 ganha o passo (c) letra→padding→LETRA→nunca o valor (a coexistência
   letra+valor começa a 216dp de painel, era 212).
5. **O X no menu de cenas.** O botão «Nova cena» encolhe 4dp à direita e
   o **X de fechar** (48×48, o alvo da casa, id 6799) fica bem visível
   ao lado — fechar deixa de exigir acertar no véu lá fora. O toque no
   X devolve −1 (fecha SEM ação; o main trata −1/0 como «nada»), o arm
   do apagar morre com o menu como sempre.

**Pins** (tests/test_012qol.cpp NOVO, registado no tests/CMakeLists.txt):
`qol_a_topbar_so_logo` (o Menu encosta ao logo + o chip é o primeiro a
ceder + a FONTE sem o literal do nome), `qol_a_lupa_expande` (o chip
32×32 na coluna das ações + o toque expande com propósito 8 + o × limpa
e recolhe), `qol_a_border_cinza` (o token #24242A == o ELEVADO, o
accentDim intacto, o divisor lê o border), `qol_a_inspector_ar` (os vãos
12dp MEDIDOS nos quads desenhados + a linha+reset cabe com margem + a
FONTE com os kTfGap*/RowPadMin), `qol_a_cenas_x_fecha` (o X 48×48 à
direita do Nova cena, sem sobrepor + o toque fecha com retorno −1).

**Mutações** (Policy #5 — vermelho → repor → verde):

- **M-QoLa1** (o texto do nome de volta no `drawTopBar`):
  ```
  qol_a_topbar_so_logo                           ...  FALHOU  tests/test_012qol.cpp:137  countSub(src, "\"G.One\"") == 0
  qol_a_topbar_so_logo                           FALHOU
  (reposto → qol_a_topbar_so_logo OK)
  ```
- **M-QoLa2** (o border de volta ao âmbar — no PAR que a fonte única
  manda: Theme.h + o alias LINE do UiContext.h): a 1.ª tentativa (só o
  Theme.h) morreu no BUILD — o `static_assert` do UiContext.h («LINE
  desincronizado de Theme.h — fonte única violada») apanhou-a ANTES do
  teste, a defesa a funcionar. Com o par mutado:
  ```
  qol_a_border_cinza                             ...  FALHOU  tests/test_012qol.cpp:235  nearEq(theme::kTheme.border[0], 36.0f / 255.0f)
  y3_cristal_divisores                           ...  FALHOU  tests/test_fase_y3_cristal.cpp:226  y3_divisores: a linha em repouso do drawPanelDividers tem de ser o GRADIENTE (a0.90 -> a0.15) — a sólida do D11 voltou?
  regress_identidade_grafite_ambar               ...  FALHOU  tests/test_sentinels.cpp:3752  nearEqF(theme::kTheme.border[0], 36.0f / 255.0f, 1e-4f)
  (reposto → os três VERDES)
  ```

**Recalibrações com nota** (todas pela mesma ordem do dono — mais
espaçamento e o cinza dos inativos; o precedente da casa: as 8 da
0.12-B, as 6 da 0.12-C, as 6 da 0.12-E):

1. `test_012c_chrome.cpp` (e) — o degrau do NOME morreu: o pin passa a
   exigir `!showPlatform` no ecrã apertado.
2. `test_fase_y1_tema.cpp` — o border é #24242A (a igualdade com o
   accentDim morreu); a FONTE: `VV_RGB(36, 36, 42)` ×2 (elevado+border),
   o âmbar `VV_RGBA(74, 55, 20, 0.60f)` ×0.
3. `test_012c_idioma.cpp` — superficies_4_tons: o border cinza.
4. `test_fase_y3_cristal.cpp` — os 3 pins do divisor medem o gradiente
   do BORDER (36/36/42 α0.90→0.15; era o âmbar 74/55/20).
5. `test_toolbar.cpp` — theme_tabela_spec_g_exata: border #24242A α1.
6. `test_sentinels.cpp` regress_identidade_grafite_ambar — o border
   voltou à rampa neutra (R==G, B−R=6 ≤ 25 — o zero-azul segue verde).
7. `test_sentinels.cpp` regress_hotfix_defeitos (D2+R1+m3) — os números
   do orçamento com os vãos 12: a 164 úteis caixas 36 exato (letra fora,
   padding intacto, valor 24 ≥ piso 22); a 208 o reset é ÍCONE com letra
   (boxW 50.7); o piso do CHIP com letra+valor fica a 220 úteis; o largo
   a 260 dá boxW 61.3; o cheio do padding dropado passa a útil+16; o m3
   recalibra a limiar da letra para 184 úteis (216dp de painel).
8. `test_safearea.cpp` + `test_scroll.cpp` — as alturas de conteúdo
   +12dp (3 linhas Transform 64→68): 1234→1246, 1228→1240, a mesh desce
   320→332. E o `c33_virtual.cpp` (16.4/m3) recalibra a limiar para 216dp
   — o device virtual fecha 1010/0.

**Verificação**: test_core 1178 OK, 0 falhas com o bench r039 saltado
(`GONI_SKIP` — o flake de disco/RAM conhecido do sandbox; no CI corre
inteiro) · c33_virtual **1010/0** · docs-lint VERDE · os pins ×5 novos
verdes · as mutações vermelho→reposto→verde. **versionCode 102** — o
build vigente da campanha 0.12-QoL. O 101 fica INTACTO como história.

## §2 B — O Navegador de Ficheiros (ZArchiver Style)

**O que mudou** (vc 103):

1. **As linhas compactas.** Os atalhos de raiz ([Raiz][Download]…[Music])
   deixam de ser os SEIS BOTÕES do choke point `ui.button` (o gradiente
   âmbar + glow — os «botões gigantes» do Redbr1) e passam a CHIPS
   QUIETOS: surface α0.85 + borda cinza (o border do QoL-A) + texto
   text2; premido = surface2. O **Subir** deixa de ser o botão
   «^ Subir» gigante e passa a LINHA COMPACTA: o chevron-up (2 traços)
   + «Subir» à esquerda, o mesmo chip quieto. A GEOMETRIA não muda —
   os ids (6850..6855, 6860), a linha 48dp, o Subir FIXO fora do scroll
   (o pin picker_compacto da C-4 segue aferrado sem recalibração).
2. **A seta (>) em TODAS as linhas** (a ordem do dono): as pastas
   mantinham o chevron; os ficheiros passam a tê-lo também, mais
   esbatido (α 0.4 — o desativado da casa): nas pastas diz «abre», nos
   ficheiros diz «importa».
3. **O breadcrumb ganha o separador**: «Raiz / Download» desenha o «/»
   entre os segmentos (o espaço já estava reservado — o glifo é novo);
   o exemplo literal do prompt do dono.
4. **TUDO o que começa por ponto FORA** (FileApi::listDirEntries): as
   pastas ponto já saíam desde a C-4; os FICHEIROS ponto (o .nomedia,
   o .git stray) saem TAMBÉM. A decisão 0.8.5 («os ficheiros ponto
   ficam») fica SUPERADA pela ordem expressa do dono neste prompt — o
   comentário no sítio documenta a superação.
5. **A ordenação** pastas-primeiro-alfabético-depois-ficheiros já era o
   comportamento desde a C-4 (o sort dirs/files com entryNameLess CI) —
   o R-057 aferra para nunca regredir (o test_browser.cpp já a pinava;
   agora há sentinela própria).

**Sentinela R-057** (`regress_qol_picker`, tests/test_sentinels.cpp) —
árvore própria em /tmp (2 pastas + 3 ficheiros + .thumbnails + .nomedia):
(1) ZERO entradas ocultas; (2) a ordem exata Anim, Download, Foto.PNG,
azul.txt, modelo.obj; (3) a FONTE do breadcrumb com o « / ». Entrada
completa no docs/REGRESSOES.md.

**Pins** (test_012qol.cpp): `qol_b_navegador_compacto` (os alvos de
sempre nos ids de sempre + ZERO quads do gradiente âmbar nas bandas
raízes/Subir + a FONTE sem «^ Subir»), `qol_b_ponto_fora` (a lista real
do FileApi sem pontos, Pasta+visivel.goni na ordem).

**Mutações** (Policy #5 — vermelho → repor → verde):

- **M-QoLb1** (o repositório da 0.8.5 — o filtro volta a deixar passar
  FICHEIROS ponto):
  ```
  regress_qol_picker                             ...  FALHOU  tests/test_sentinels.cpp:7706  R-057: a entrada oculta «.nomedia» vazou para a lista
    FALHOU  tests/test_sentinels.cpp:7720  out.size() == 5u
    FALHOU  tests/test_sentinels.cpp:7724  !out[2].isDir && out[2].name == "Foto.PNG"
  regress_qol_picker                             FALHOU
  012c_picker_oculta_dot                         ...  FALHOU  tests/test_012c_picker.cpp:220  nDotFiles == 0u
  (reposto → regress_qol_picker OK + 012c_picker_oculta_dot OK)
  ```

**Recalibração com nota**: `012c_picker_oculta_dot` (test_012c_picker.cpp)
— o EXPECT(hasDotFile) da 0.8.5 morre: ZERO ficheiros ponto (o comentário
do pin documenta a superação).

**Verificação**: test_core 1180 OK / 0 falhas (o bench r039 saltado no
sandbox) · c33_virtual **1010/0** · docs-lint VERDE · as mutações
vermelho→reposto→verde. **versionCode 103** — o build vigente. O 102
fica INTACTO como história.

## §3 C — O Viewport 3D Limpo

**O que mudou** (vc 104):

1. **A teia do frustum invisível por defeito.** As 12 arestas do frustum
   (near + cone + far) SÓ desenham com a câmara SELECIONADA
   (CamGizmo.cpp — o bloco fica dentro do `if (selected)`). O GLIFO mudo
   (a 35%) continua sempre — é o alvo do toque que seleciona a câmara
   (pickCameraTic). Com seleção: a teia âmbar 2px + os handles de canto
   como sempre. O toggle «frustum» do Inspector continua a esconder
   TUDO (o gate do drawAll intacto). O comportamento D17 «mudo a 35%
   desenhava» fica SUPERADO pela ordem do dono.
2. **O rail flutuante.** O pai de vidro do rail
   (`glassPanel(L.railPanel)`) SAI do draw — os botões Selecionar/Mover/
   Rodar/Escalar/Íman flutuam SEM o fundo preto quadrado gigante; cada
   um mantém o SEU chip quieto (surface + borda). O RECT do railPanel
   continua no layout (o contrato dos pins mover_unico/d17 — nenhuma
   recalibração); só o desenho morre. A fila do topo (undo/redo/save)
   mantém o pai.
3. **O grid com fade suave.** `Grid::gridFadeForDist` NOVA — a FONTE
   ÚNICA da janela do fade radial (o draw e os pins partilham): o início
   fica 1.1×foco (piso 12) e o CAÍDO DOBRA — 2.2×foco com piso 60 (era
   1.5×/30). O foco de trabalho 6 → fade 12..72 (era 12..42); o horizonte
   perde-se gradualmente, sem a borda dura. O smoothstep do FS mantém-se.

**Sentinela R-058** (`regress_qol_viewport`) — (1) câmara sem seleção:
ZERO arestas longas no batch + o glifo presente; (2) com seleção: o
desenho volta; (3) a janela do fade nos 3 regimes + o caída ≥ 60 em
qualquer foco. Entrada completa no docs/REGRESSOES.md.

**Pins** (test_012qol.cpp): `qol_c_frustum_sob_selecao` (a teia
invisível por defeito / volta com seleção — as arestas medidas no
batch), `qol_c_rail_flutuante` (o rect do rail existe, o pai de vidro
não desenha), `qol_c_grid_fade_suave` (a janela 12..72 no trabalho, o
caída ≥ 60 em toda a escada de focos, o draw consome a fonte).

**Mutações** (Policy #5 — vermelho → repor → verde):

- **M-QoLc1** (a teia de volta para TODAS — o `if (selected)` morre):
  ```
  regress_qol_viewport                           ...  FALHOU  tests/test_sentinels.cpp:7786  R-058: a teia do frustum NÃO desenha sem seleção (8 arestas longas vazaram)
  regress_qol_viewport                           FALHOU
  qol_c_frustum_sob_selecao                      ...  FALHOU  tests/test_012qol.cpp:551  qol_c_frustum_sob_selecao: a teia invisível por defeito (8 arestas longas)
  (reposto → os dois VERDES)
  ```

**Recalibração com nota**: `cameratic_d17_objeto_pequeno_estados_e_medidas`
(test_cameratic.cpp) — o «mudo a 35% presente» passa a «ZERO linhas
longas + o glifo mudo presente» (a teia é o ruído que o dono mandou
fora). Os pins d20/play/toggle passam SEM recalibração (o bounding só
encolhe; os gates ≤4%/≤6% continuam a cumprir-se).

**Verificação**: test_core 1183 OK / 0 falhas (×2 runs, o bench r039
saltado no sandbox) · c33_virtual **1010/0** · docs-lint VERDE · as
mutações vermelho→reposto→verde. **versionCode 104** — o build vigente.
O 103 fica INTACTO como história.

## §4 D — Auto-Save e Recuperação de Crash

**O que mudou** (vc 105):

1. **O timer.** A cada **180s** (3 minutos — `autosave::kPeriodSec`), SE
   houver alterações na cena, gravar **autosave.goni NA RAIZ DO
   PROJETO** (`Project::saveAutosave` — o MESMO dump do SceneSerializer
   num ficheiro PRÓPRIO; o .goni principal INTACTO). O dirty é HONESTO:
   o dump da cena compara-se ao último persistido
   (`g_autoLastDump`) — o baseline renasce em CADA load
   (`autoSaveOnSceneLoaded`), um boot sem mudanças NUNCA grava. Corre
   NO FIM DO FRAME (`autoSaveTick(realDt)` — 1×/3min não muda o
   perfil; um thread separado complicaria o SAF sem necessidade, a
   forma que o prompt aceita). SÓ NO EDITOR: o Play muta a cena em
   runtime (spawn/física) — gravar isso era persistir o momento, não o
   trabalho.
2. **A recuperação.** Ao abrir o projeto (1× por processo — os reloads
   do lifecycle não re-perguntam), o `ProjectStorage::modifiedTime`
   (NOVO — o default −1 é honesto «desconhecido»; o FsStorage devolve o
   stat real) compara o autosave.goni com o .goni principal:
   `autosave::decide` → o autosave ESTRITAMENTE mais recente arma
   `EditorState::autoAsk` e o pop-up 2.5D pergunta «**Encontrámos
   trabalho não guardado. Desejas recuperar?**» com
   **[Recuperar]**/​**[Descartar]** (o padrão do drawApplyDialog — o
   card centrado + véu; o toque fora devolve 2 SEM destruir: o autosave
   fica para a próxima). O [Recuperar] carrega pelo MESMO loadText +
   pós-load da casa; um autosave corrompido é removido com o principal
   intacto e o toast diz o que aconteceu. O [Descartar] apaga o
   ficheiro.
3. **O indicador.** O pequeno **✓ verde** (o visto do conjunto outline)
   vive à ESQUERDA do grupo play/pause na barra de topo e **PISCA**
   durante ~2,4s quando o auto-save grava (`EditorState::autosaveFlash`
   — o seno de 2 ciclos/s com o piso 0.25; o decay é com o dt REAL).

**Sentinela R-056** (`regress_qol_autosave`) — (1) as decisões puras
(180s, os limites do due, os 6 casos do decide — o desconhecido nunca
pergunta); (2) o CICLO REAL no FsStorage: createNew → saveAutosave ×2 →
o mtime do principal atrasado 60s (a granularidade de 1s do filesystem)
→ **Recover** → o loadAutosave devolve a MUDANÇA (pos 9,9,9) → o
remove do descartar; (3) os ids do pop-up. Entrada completa no
docs/REGRESSOES.md.

**Pins** (test_012qol.cpp): `qol_d_autosave_decisoes` (o período + os
limites + a honestidade do decide), `qol_d_ciclo_real` (o ciclo real no
FsStorage: o principal intacto, o [Recuperar] volta, o [Descartar]
apaga), `qol_d_pop_up_e_indicador` (os 2 alvos do pop-up MEDIDOS na
mesma linha + o toque fora = 2 + as FONTEs do ✓ e do texto).

**Mutações** (Policy #5 — vermelho → repor → verde):

- **M-QoLd1** (o timer morto — o `due()` devolve false sempre):
  ```
  regress_qol_autosave                           ...  FALHOU  tests/test_sentinels.cpp:7706  autosave::due(180.0f)
    FALHOU  tests/test_sentinels.cpp:7707  autosave::due(360.0f)
  (reposto → regress_qol_autosave VERDE)
  ```

**Verificação**: test_core 1194 OK / 0 falhas (o bench r039 saltado no
sandbox) · c33_virtual **1010/0** · docs-lint VERDE · as mutações
vermelho→reposto→verde. **versionCode 105** — o build vigente da
CAMPANHA FECHADA. O 104 fica INTACTO como história.

## §5 O fecho da campanha

- As 4 sub-fases (A/B/C/D) fecham com commit + push próprios, o
  STATE.md no MESMO commit e CI verde — a regra §5 da casa.
- A referência V.ONI NÃO regenera: ZERO entradas no registo (a cláusula
  calma — o R-013 segue aferrado).
- O achado fora da lista (cláusula calma, NÃO fixado): o gate
  ctx_state_curto (o teto <300 linhas do STATE.md) apanhou o índice em
  300 exato no commit do B — os 2 runs vermelhos do B vivem no histórico
  do CI; o fix (B-fix + B-fix2) devolveu o teto e corrigiu a ordem do
  R-057 (o CI é o árbitro).
- O CODEMAP.md e o BACKLOG.md atualizam-se no commit do D (o fecho).
