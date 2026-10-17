# RELATORIO-0.12-A.md — Campanha 0.12-A: os bugs P0 (A-1..A-4)

2026-10-17 · vc 82 · o commit deste relatório fecha as 4 sub-fases com CI
local verde (o CI do push é o árbitro final).

Natureza: bug fix. Zero features novas. Zero refactors estruturais. A
cláusula calma foi respeitada — os bugs encontrados FORA da lista estão no
§6 documentados e NÃO fixados.

---

## 1. A-1 — as linhas LOD e impostor no Inspector

### §4 O diagnóstico obrigatório (os 4 pontos do prompt)

1. **MeshRenderer.h tem os campos?** SIM: `lodDistance` em
   `app/src/main/cpp/components/MeshRenderer.h:66` (default `0.0f`, o
   comentário da 0.11-T FASE 3.5a) e `impostorDistance` em
   `app/src/main/cpp/components/MeshRenderer.h:96` (default `0.0f`, o
   comentário da 0.11-W 3.75b).
2. **EditorLayout.h tem os InspRow?** SIM: o enum `InspRow::Kind` tem
   `LodDistance` em `app/src/main/cpp/ui/EditorLayout.h:306` e
   `ImpostorDistance` em `:313`; o plano os EMPURRA em `:557`
   (`kInspectorLodDist = 5311`, declarado em `:122`) e `:567-568`
   (`kInspectorImpostorDist = 5314`, `:127`), ambos dentro do bloco
   `p.fileMesh` (a condição em `:551`; `fileMesh` = `!mr->primOn &&
   !mr->meshPath.empty()`, calculado em `EditorLayout.h:240`).
3. **EditorUi.cpp desenha-os na secção do MeshRenderer?** SIM: o draw do
   LOD está em `app/src/main/cpp/ui/EditorUi.cpp:1968-1985` (o slider
   "lod distance" 0..500, o label honesto «off» a 0) e o do impostor em
   `:2008-2025` (o slider "impostor" 0..500); ambos ESCREVEM no
   `MeshRenderer` do TIC selecionado (`mrEdit->lodDistance = lod` /
   `mrEdit->impostorDistance = imp`) e armam `edited` (o save).
4. **Os pins da 3.5a/3.75b testam a UI ou só o campo?** SÓ O CAMPO. O
   `tests/test_fase35a_lod_cull.cpp` aferra a cauda GLOD (geração, uso no
   draw, default off, compat) e o `tests/test_fase375b_impostors.cpp`
   aferra o PNG/impostor — NENHUM dos dois (nem outro na suíte) refere
   `kInspectorLodDist`, `kInspectorImpostorDist` ou os Kinds das rows:
   `rg -l 'kInspectorLodDist|InspRow' tests/` devolve 10 TUs e nenhum tem
   os símbolos. **A UI das rows nunca esteve pinada** — é exatamente o gap
   que deixou a dúvida do dono sem árbitro.

### A decisão (o contrato do prompt: «Se o campo já existir na UI: documentar em §4 e PÁRA — não faz fix cego»)

O campo JÁ EXISTE na UI completa (row + draw + tap→write + persistência)
desde o vc 72 (LOD) e o vc 76 (impostor) — as rows nascem com o TIC com
`meshPath` e secção MALHA expandida. **NENHUMA linha de produção foi
trocada nesta sub-fase.** O que faltava era a PROVA — os 5 pins do prompt
foram criados em `tests/test_012a_lodui.cpp` aferrando o contrato
EXISTENTE (zero mudanças de produção; as mutações M-A1a/M-A1b provam que
os pins mordem):

- `lodui_row_existe` — o plano do TIC com mesh file tem as 2 rows com os
  ids da faixa (5311/5314), a ORDEM dims→escala→lod→instancing→streaming→
  impostor (6 rows no bloco), o y cumulativo intacto, e o NEGATIVO: o TIC
  de primitiva (sem meshPath) NÃO tem as rows.
- `lodui_aplica` — o gesto REAL (down→frame→up) no trilho do slider escreve
  no MeshRenderer (250 u no LOD a 50 % do trilho; 100 u no impostor a 20 %)
  e o tap FORA do trilho não escreve.
- `lodui_persiste` — o round-trip .goni transporta `lodDist`/`impostorDist`
  quando != 0 e NÃO nasce quando 0 (o contrato byte a byte do dump).
- `lodui_compat` — .goni sem os campos abre com defaults 0 e re-dump BYTE A
  BYTE (os ficheiros da era 0.11-S ficam intocados).
- `lodui_default_off` — TIC novo (preset e componente nu) nasce 0/0.

**MUTAÇÕES (vermelho→verde, coladas):**
- **M-A1a** (o push do `ImpostorDistance` removido do `inspectorPlan`):
  `lodui_row_existe … FALHOU` + `lodui_aplica … FALHOU (imp != nullptr)` —
  2 testes vermelhos; reposto → 5/5 verde.
- **M-A1b** (o tap deixa de escrever: `mrEdit->impostorDistance = imp`
  comentado no case do EditorUi.cpp): `lodui_aplica … FALHOU
  (mr->impostorDistance > 90 && < 110)` — 1 teste vermelho; reposto → 5/5
  verde.

### Por que o dono não vê as rows no device (as hipóteses do §4, em ordem de probabilidade)

1. **A secção MALHA está colapsada** — `inspCollapsed` PERSISTE no
   layout.json (spec G; main.cpp:313/350/413): se a secção foi colapsada
   nalguma sessão, as rows somem e ficam somidas.
2. **O TIC não tem mesh file** — `p.fileMesh` exige `meshPath` não vazio E
   `primOn` falso: um TIC "cube"/primitiva NÃO tem as rows (o contrato).
3. **As rows estão abaixo da dobra** — vivem DEPOIS de mesh:/prim:/tex:/
   dims:/escala: dentro da secção MALHA; no device é preciso scroll no
   Inspector.
4. **O APK do device é anterior ao vc 72** — as rows nasceram na 0.11-T.
5. O valor a 0 mostra «off» (não «0») — o label honesto do slider.

**O C33 do dono (o árbitro no device):** abrir o Inspector de um TIC com
mesh file com o APK vc 82; confirmar que a secção MALHA está EXPANDIDA e
que o scroll mostra `lod distance: off` e `impostor: off`; PNG do
Inspector. Se não aparecer: o engine.log com o dump do plano (`rg lodui
docs/RELATORIO-0.12-A.md` — os pins §2 provam que o plano as tem) e o
export do layout.json (o bit do `inspCollapsed`).

---

## 2. A-2 — os contadores coerentes (Ficheiros/Assets) + o ícone da cena

### §4 O diagnóstico

- **Como o Ficheiros conta as cenas?** O painel lê `tree.sceneCount`
  (BottomPanel.cpp:320) — preenchido por `refreshFilesTree()`
  (main.cpp:3558-3561) = `g_projectReady ? g_project.scenes.size() : 0` —
  LÊ o manifesto em memória (Project::scenes): a FONTE está certa. **O bug
  é o TIMING**: `refreshFilesTree` só corre dentro de `refreshCatalog()`
  (main.cpp:3593), e o `refreshCatalog()` só é chamado no sucesso de
  import (3101), refsChanged no load (4056), quarentena (4192), ABERTURA
  de seletor (6787), import legado (4644) e troca de cena (1290) — **NO
  ARRANQUE (projeto pronto em 8145/8176) e em `createSceneNamed` (1370)
  NINGUÉM refresca**: o drawer nasce «0 cena(s)» mesmo com a cena main
  (o estado fica stale até ao 1.º seletor/import).
- **Como o Assets lista os .gmesh?** A tab 2 lê `catalog.meshes/textures/
  audio` (BottomPanel.cpp:432-439); o mesmo `refreshCatalog()` lista
  `assets/` via `g_storage->listDir(Project::kDirAssets)` (main.cpp:3598)
  — a FONTE está certa; o MESMO gap de timing (sem refresh no arranque, o
  modelo importado de sessões anteriores não aparece até abrir um seletor).
- **O ícone partido ao lado de "main"**: as linhas do seletor CENAS
  desenhavam `icons::Icon::Clapper` (EditorUi.cpp:3327) — a CLAQUETE é o
  ícone da ANIMAÇÃO (a hierarquia usa-a para AnimationPlayer) e a 24 dp as
  3 listras diagonais dentro da faixa da régua leem-se como glifo
  corrompido. A ativa já tinha o Check (3340) — o «partido» é a claquete.

### O fix (o mínimo que a lista manda)

1. `main.cpp`: `refreshCatalog()` DEPOIS de o projeto ficar pronto (o bloco
   `if (g_storage)` do arranque — cobre SAF e app-private num ponto único)
   e no FIM de `createSceneNamed` (o padrão do `doSwitchScene:1290`).
2. `EditorUi.h/.cpp`: `icons::Icon sceneRowIcon()` NOVA — a FONTE ÚNICA do
   ícone da linha de cena (o padrão `hierIconFor`), devolve `Icon::Scene`
   (camadas); o draw das rows e o empty-state consomem-na.

### Os pins (tests/test_012a_contadores.cpp) + MUTAÇÕES (coladas)

- `ficheiros_conta_cenas` — com 1 cena o painel DIZ «1» (a identidade do
  glifo pelo UV do atlas: o dígito desenhado é o «1» da fonte; com 0 cenas
  diz «0» — um contador hardcoded morre nos DOIS lados). **M-A2a**
  (`"%u cena(s)"` → literal `"0 cena(s)"`): `ficheiros_conta_cenas …
  FALHOU (band[0].fp.sameAs(um))` — vermelho; reposto → verde.
- `assets_lista_mesh` — com `assets/casa.gmesh` no catálogo o painel
  MOSTRA-o (a sequência de glifos «casa.gmesh» no batch) e o empty-state
  «sem ficheiros» some; sem catálogo o empty-state honesto aparece.
  **M-A2b** (o loop `catalog.meshes` removido do pushCard):
  `assets_lista_mesh … FALHOU (containsSequence(all, casa) + !contains
  Sequence(all, semFich))` — 2 EXPECTs vermelhas; reposto → verde.
- `cena_ativa_marcador` — `sceneRowIcon() == Icon::Scene`, a def do ícone
  tem geometria válida (pts/nPts/nLines), o Check da ativa também, e o
  sheet REAL desenha a linha «main» (o glifo no batch).

---

## 3. A-3 — a grelha que não vaza + o tooltip que morre

### §4 O diagnóstico

- **A grelha**: no modo UI o `viewRect` é calculado com `inspRight =
  showInspector && !(uiMode || collapsed)` (main.cpp:5958-5960) — o painel
  direito do modo UI vive DENTRO da área do viewRect (o contrato
  WYSIWYG «o painel de UI mantém a largura em ambos»); o scissor do pass
  3D cortava AO viewRect (main.cpp:6176-6179) — a grelha (e as malhas)
  renderizavam POR TRÁS do vidro do Inspector (surface α0.80 da Y-1) e
  vazavam no canto superior direito. O pass 3D corre sempre (o drawTics +
  grid em 6211/6280 não têm gate de uiMode) — o vazamento era sistémico.
- **O tooltip**: a legenda do gizmo («Mover»/«Rodar»/«Escalar»/
  «Selecionar», ViewportChrome.cpp:342-346) era desenhada SEMPRE que o
  layout a cabia (`L.legendVisible`) — permanente, sem timer nem ciclo de
  toque: o «tooltip preso» do dono.

### O fix (UMA via para a grelha — o scissor, como o M-A3a do prompt pede)

1. **A grelha (scissor respeitado)**: `safe::scissor3dRect()` NOVA
   (SafeArea.h — pura e pinada): fora do modo UI devolve o viewRect BYTE A
   BYTE; no modo UI devolve o `centerRect` com a largura do painel direito
   RESERVADA (o mesmo `inspW`/`hierW` do `drawUiInspector`). O `glViewport`
   fica no viewRect — só o `glScissor` corta (main.cpp:6173-6191). O painel
   direito do modo UI reserva quando `selElement >= 0 || selJoystick ||
   showInspector` (a MESMA condição do draw em 6853-6861).
2. **O tooltip**: `EditorState::legendTimer` (s restantes) +
   `legendArm` (a escolha em curso); o ciclo vive no `vpchrome::draw` (que
   ganha os parâmetros trailing `f32 dt = 0.0f, const InputState* in =
   nullptr` — o main passa `g_frameDt, &g_input`): nasce no LARGAR do
   toque que escolheu a ferramenta («após o toque»), vive 1,5 s (o decay
   com o dt REAL) e morre no LARGAR do toque SEGUINTE; o draw da legenda
   é gated por `legendTimer > 0`. Os callers antigos (defaults 0/null)
   ficam byte a byte.

### Os pins (tests/test_012a_grelha_tooltip.cpp) + MUTAÇÕES (coladas)

- `grelha_nao_vaza` — o scissor3dRect: (a) no 3D é o viewRect exato;
  (b) no modo UI sem painel é a mesma área; (c) no modo UI com painel a
  aresta direita do scissor NÃO entra no rect do `inspectorPanelRect`
  (nenhum pixel 3D por trás do vidro). **M-A3a** (o helper devolve
  viewRect sempre — «scissor removido»): `grelha_nao_vaza … FALHOU
  (s.w < view.w + s.x+s.w <= panel.x)` — 2 EXPECTs vermelhas; reposto →
  verde.
- `tooltip_morre_1500ms` — a escolha do Mover arma o tooltip no largar
  (o glifo «Mover» VISÍVEL na área da legenda), a 1,0 s ainda está vivo,
  aos 1,5 s o timer chega a 0 e os glifos SUMIRAM. **M-A3b** (o decay
  comentado — «timer removido»): `tooltip_morre_1500ms … FALHOU
  (legendTimer == 0 + !hasGlyphsIn(L.legend))` — 3 EXPECTs vermelhas — o
  pin REPRODUZ o bug do dono (o tooltip preso); reposto → verde.
- `tooltip_morre_release` — o largar de um toque NOVO mata o tooltip
  imediatamente; o largar de NASCIMENTO (o mesmo edge da escolha) não
  mata; o ciclo recomeça na escolha seguinte.

---

## 4. A-4 — a investigação RAM vs GPU (sem fix, só instrumentação)

A investigação completa vive em `docs/RELATORIO-0.12-A-INVEST-RAM.md` —
resumo do veredito:

- **A curva completa no hospedeiro** (alvo opcional `bench_ram`, OFF por
  default) REPRODUZ o salto anormal: a região 50k…4,1M é LINEAR
  (0,01→1,11 ms) e o frame EXPLODE a 8,2M (376 ms) e 12,3M (900 ms) —
  339× e 810× o custo linear — EXATAMENTE quando o conjunto visível de
  blocos cruza o teto de VRAM da cache (192 MB): 9 758 e 23 470
  EVICTIONS/frame contra ZERO na região linear.
- **O discriminador**: com a câmara PARADA o thrash é IDÊNTICO
  (`evc_par == evc_orb`) — o draw tenta materializar todo o visível a
  cada frame e a LRU cicliza indefinidamente. O RSS cresce LINEAR (sem
  penhasco) — o RSS NÃO é o mecanismo.
- **Veredito: RAM (a cache de blocos a ciclizar), NÃO GPU/CPU** — no
  hospedeiro não há GPU e o salto reproduz-se no mesmo ponto e mecanismo.
- **A instrumentação entregue**: o sampler `bench_ram:` no engine.log
  (main.cpp sob `#ifdef GONI_BENCH_RAM` — tri/fps/frame_ms/rss/pico/
  livre/dc/bl/blocos/carregados/a_transmitir, 1×/s) + o alvo `bench_ram`
  (tests/CMakeLists sob `option(GONI_BENCH_RAM … OFF)`). Zero alterações
  de produção fora do ifdef — a regra A-4 respeitada.
- **O protocolo do dono** (a fechar no device) e as recomendações para a
  próxima campanha estão no relatório próprio (§3-§4).

---

## 5. O estado da verificação (o «não confiar no build»)

- **test_core: 1077 pins, 0 falhas** (a suíte inteira; 1066 no baseline +
  os 11 pins novos A-1/A-2/A-3) — com os 2 flakes conhecidos saltados
  (`regress_streaming_paralelo` e `sentinela_r039*`, o r039 escreve 1,7 GB
  de disco e o ambiente tem 8,9 GB; o skip é o MESMO do STATE.md §fecho
  0.11-Y).
- **c33_virtual: 1006 checks, 0 falhas** (a varredura dos 13 ecrãs confirma
  o layout com o A-2/A-3 dentro).
- **main.cpp** sintetizado por `g++ -fsyntax-only` COM e SEM
  `GONI_BENCH_RAM` contra os stubs da casa (o APK não compila neste
  ambiente; a sintaxe dos dois modos é a prova local).
- **Gates locais**: scope VERDE (todo o diff em ci/scope.txt) · docs-lint
  VERDE (104 ficheiros) · theme-hex 0 fora do Theme · ui-vocab OK ·
  release-identity VERDE (0.9.6 · **vc 82** declarado).
- **As 6 mutações** (M-A1a/b, M-A2a/b, M-A3a/b): todas VERMELHO→reposto→
  VERDE, coladas nas secções acima.

## 6. O achado FORA da lista (a cláusula calma: documentado, NÃO fixado)

`inspectorRowCount` (EditorLayout.h:408-485) SUBESTIMA a secção MALHA para
TICs com mesh file: conta `n += 2` (dims + escala) mas o `inspectorPlan`
empurra 6 rows (dims, escala, LOD, instancing, streaming, impostor) — o
desalinhamento entrou com a 3.5a/3.5b/3.75a/3.75b e nenhuma atualizou a
função. O contrato do comentário (:487, «rows deve ter capacidade
inspectorRowCount») está quebrado; o pior caso teórico (TIC com câmara +
mesh file + material N>1 + física + input + áudio + anim + script 8
exports) dá 65 rows contra o buffer fixo `InspRow plan[64]`
(EditorUi.cpp:1348 e EditorLayout.h:670). **Sem sintoma demonstrado**: o
`inspectorContentHeight` deriva do PLANO real (o scroll está certo) e o
chamador de produção usa 64 fixo — nenhum caminho conhecido lê a função
para dimensionar. Fica documentado para a campanha seguinte decidir (fix
trivial: +4 na conta, ou a função derivar do plano).

## 7. Os limites declarados

- O APK não foi construído neste ambiente (sem NDK) — a compilação real é
  do CI; o que este ambiente provou: a suíte inteira, o C33 virtual, os
  gates, e a sintaxe do main.cpp nos dois modos do ifdef.
- A A-1 não mudou produção (a decisão PÁRA do prompt) — se o C33 do dono
  no device ainda não mostrar as rows, a causa está no §1 (colapso
  persistido / mesh file / APK antigo), e os pins novos dão o árbitro.
- O veredito do A-4 no DEVICE depende da corrida do dono com o sampler
  (o protocolo no relatório próprio) — o hospedeiro provou o mecanismo,
  o device fecha o caso.
