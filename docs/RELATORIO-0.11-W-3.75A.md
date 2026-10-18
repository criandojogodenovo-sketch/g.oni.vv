# 0.11-W 3.75a — STREAMING PROGRESSIVO (o freeze do import morre por orçamento)

> versionCode 75 · 0.9.6 · a 2.ª sub-fase da campanha 0.11-W (render
> systems; a abertura: docs/RELATORIO-0.11-W-ABERTURA.md) · 8 pins novos
> (tests/test_fase375a_streaming.cpp) + a sentinela R-048
> (regress_streaming) + as 3 mutações coladas · c33 FASE 29 (+34 checks,
> 891→925) + o 23.2 na convergência · test_core verde (os 2 flakes
> documentados no §4) · gates locais verdes.

## §1 O que foi feito

### O diagnóstico (o freeze não era onde parecia)

A abertura do .gmesh JÁ era pela tabela (o PASSO 4: 192 B + tabela +
materiais — zero dados) e o conversor JÁ corre na thread do import com
progresso (o PASSO 3). O freeze do dono (5 FPS no C33 com a cena dos 6
modelos de 203 MB) nasce no PRIMEIRO frame em que a cena fica visível: o
lazy de sempre do BlockMesh carrega TODOS os blocos visíveis não
residentes NO MESMO frame — na cena do dono, 863 leituras do disco +
decodes + uploads ao GPU de uma vez, segundos de frame único. O pin
`fase375a_sem_freeze_1a_frame` reproduz a mecânica em miniatura (12
blocos × 6 clones) e a FASE 29 a aferra no drawTics REAL do device.

### A cura: orçamento por frame + camadas por distância + a porta

- **`render/BlockMesh.h/.cpp`** — a API do stream (a política vive AQUI,
  testável sem o main):
  - `kBlockStreamBudgetPerFrame = 8` — o ORÇAMENTO declarado: 8 blocos
    por frame no CAMINHO INTEIRO (a soma de todos os meshes). ~1-3 ms
    por bloco (leitura+decode+upload) = um frame de trabalho honesto a
    30-60 FPS; o fill de 863 blocos espalha-se por ~108 frames (~2-4 s)
    em vez de UM frame de segundos.
  - `struct StreamReq { BlockMesh* bm; u32 entry; f32 dist; }` — UM
    pedido de vaga (o orquestrador colhe; a dist é do chamador: o centro
    do AABB do bloco NORMAL transformado pelo model do TIC contra o
    camEye — o MESMO centro que o classify usa para o z dos blends; a
    distância MÍNIMA dos clones quando o mesmo entry serve N TICs).
  - `streamIn(reqs, count, budget)` — o coração: ordena os pedidos por
    distância ASCENDENTE (ESTÁVEL — os empates ficam na ordem da tabela:
    o fill respeita o ficheiro) e carrega até ao orçamento, PERTO
    PRIMEIRO (as camadas). Os já residentes NÃO consomem a vaga (o dedup
    é o próprio resident()); os que FALHAM (CRC/faixa) TAMBÉM não (um
    bloco corrupto não esfomeia os vizinhos — o retry por frame é o de
    sempre do lazy). Devolve os carregados. NÃO desenha nada.
  - `resident(entry)` — a PORTA da submissão: o entry tem GPU vivo
    AGORA? O orquestrador SÓ submete o residente.
  - `streamLoad(entry, err)` — o load ÚNICO do stream: o MESMO
    ensureBlock do lazy de sempre (a mesma faixa, o mesmo decode, o
    mesmo upload, o mesmo anti retry-storm — zero caminhos novos).
  - ZERO mudança aos caminhos de draw (draw/drawInstancedGroup/
    drawBlendPicks intocados — o lazy inline de sempre é o caminho dos
    chamadores antigos, byte a byte).
- **`platform/main.cpp`** — o drawTics ganha a fase do STREAM entre o
  agrupamento e a submissão: (1) os grupos e os blends NÃO residentes
  pedem vaga (a dist = MÍNIMO sobre as instâncias do grupo — o clone
  mais perto manda); (2) `streamIn` corre com o orçamento do frame;
  (3) a submissão SÓ desenha o residente (a PORTA) — o resto conta
  `BlockFrameStats::pending` («a transmitir») e chega nas vagas dos
  frames seguintes; (4) os blends filtrados de forma ESTÁVEL (a ordem
  longe→perto sobrevive — 1 draw por bloco, depois dos opacos, igual).
  O log throttled diz «a transmitir=N» (a fonte do HUD).
- **`components/MeshRenderer.h`** — `bool streaming = true` (o default
  ON — a cura é o comportamento jogável). Com o OFF o TIC sai do batch
  e desenha individualmente (o MESMO fallback do instancing OFF: o
  caminho de sempre, com o lazy inline e o freeze de sempre — assumido).
- **`core/SceneSerializer.cpp`** — «streaming»:false SÓ quando false (o
  default omitido — os .goni da 0.11-T ficam byte a byte; ausente = ON).
- **`core/UndoStack.h/.cpp`** — o TicSnap transporta (snap/restore/
  paste; o MESMO contrato do instancing).
- **`ui/EditorLayout.h` + `ui/EditorUi.cpp`** — a linha «streaming:
  sim/não» no inspector (o MESMO padrão do toggle instancing; o id
  kInspectorStreaming 5313).

### ZERO mudança ao formato .gmesh

As camadas NÃO são um campo novo no ficheiro: calculam-se da TABELA em
runtime (os AABB dos blocos ordenados por distância ao olho). O .gmesh
de 203 MB que o dono já importou NÃO precisa de reimport; os ficheiros
pré-3.75a ganham o streaming de graça. (A leitura da spec: «camadas no
.gmesh» = o .gmesh a chegar por camadas — a mecânica que o orçamento +
distância produz sobre a tabela existente.)

## §2 Provas

- **Pins novos em tests/test_fase375a_streaming.cpp: 8 casos**
  1. `fase375a_sem_freeze_1a_frame` — 12 blocos × 6 clones: o 1.º frame
     carrega SÓ 8 (a vaga), desenha 48 (8 grupos × 6), pending 24 (4 ×
     6), 8 dc instanciados; os residentes são EXATAMENTE os entries
     0..7 (os 8 mais pertos; os empates na ordem da tabela).
  2. `fase375a_camadas_perto_primeiro` — vaga 3: os residentes são os 3
     mais pertos (0,1,2 a 10×i unidades); com o olho NO bloco 11, os
     residentes são 11,10,9 (as camadas seguem o olho, não a tabela).
  3. `fase375a_budget_por_frame` — o fill em vagas: 8 → 4 → 0 (Σ loads
     == 12 = os blocos do ficheiro; o frame 2 fica COMPLETO com 12 dc;
     o frame 3 é o steady state sem loads).
  4. `fase375a_toggle_off` — o streaming OFF: os 12 carregam INLINE no
     1.º frame (o caminho de sempre), 72 desenhados, ZERO draws
     instanciados, zero «a transmitir».
  5. `fase375a_compat_011t` — o TIC novo nasce ON; o save sem edição
     não contém a chave; com false a chave nasce; o load restaura; o
     round-trip da forma 0.11-T é byte a byte.
  6. `fase375a_blend_pending` — os blends pendentes pedem vaga TAMBÉM
     (senão nunca chegavam): frame 1 carrega 2 de 3, desenha 2 (1 draw
     por bloco, ZERO fundição), pending 1; frame 2 completa.
  7. `fase375a_falha_nao_consome` — o bloco 0 corrupto (CRC32 flipado —
     o materialize recusa «CHECKSUM CORROMPIDO») NÃO gasta a vaga: com
     a vaga 2, os blocos 1 e 2 (os vivos mais pertos) enchem o frame;
     se a falha gastasse a vaga, só 1 carregava (vermelho).
  8. `fase375a_stats_corretos` — a contabilidade fecha: Σ loads == 12,
     Σ pending == 24 (só o frame 1), o steady state desenha o merge
     completo (12 dc, instancesMerged 72).
- **Sentinela R-048** (`regress_streaming` em tests/test_sentinels.cpp)
  — o orçamento respeitado (8 de 12 no 1.º frame), as camadas (vaga 3 =
  os 3 mais pertos), o toggle no .goni (ausente = ON, false restaura) e
  o fill que fecha (8 → 4 → 0; drawn+pending == visible).
- **Mutações 3 vermelho→verde (coladas)**

M-3.75a1 — o orçamento removido (o streamIn carrega TUDO o que pedem —
o freeze volta):

```
$ GONI_ONLY=fase375a_sem_freeze ./test_core
fase375a_sem_freeze_1a_frame                   ...  FALHOU  tests/test_fase375a_streaming.cpp:379  run.loads == BlockMesh::kBlockStreamBudgetPerFrame
  FALHOU  tests/test_fase375a_streaming.cpp:380  run.loads == 8
  FALHOU  tests/test_fase375a_streaming.cpp:381  run.instDraws == 8
  FALHOU  tests/test_fase375a_streaming.cpp:382  run.total.drawCalls == 8
  FALHOU  tests/test_fase375a_streaming.cpp:383  run.total.instancesMerged == 48
  FALHOU  tests/test_fase375a_streaming.cpp:384  run.drawn == 48
  FALHOU  tests/test_fase375a_streaming.cpp:385  run.pending == 24
  FALHOU  tests/test_fase375a_streaming.cpp:389  bm.resident(i) == (i < 8)
fase375a_sem_freeze_1a_frame                   FALHOU
```

Reposto (cp do backup): 0 teste(s) com falha.

M-3.75a2 — a ordenação removida (a ordem dada segue — as camadas
morrem; o olho no bloco 11 carrega 0,1,2):

```
$ GONI_ONLY=fase375a_camadas ./test_core
fase375a_camadas_perto_primeiro                ...  FALHOU  tests/test_fase375a_streaming.cpp:428  bm2.resident(11) && bm2.resident(10) && bm2.resident(9)
  FALHOU  tests/test_fase375a_streaming.cpp:429  !bm2.resident(0)
fase375a_camadas_perto_primeiro                FALHOU

2 teste(s) com falha
```

Reposto (cp do backup): 0 teste(s) com falha.

M-3.75a3 — a PORTA morta (resident() sempre true — a submissão carrega
inline tudo o que não está):

```
$ GONI_ONLY=fase375a_sem_freeze ./test_core
fase375a_sem_freeze_1a_frame                   ...  FALHOU  tests/test_fase375a_streaming.cpp:379  run.loads == BlockMesh::kBlockStreamBudgetPerFrame
  FALHOU  tests/test_fase375a_streaming.cpp:380  run.loads == 8
  FALHOU  tests/test_fase375a_streaming.cpp:381  run.instDraws == 8
  FALHOU  tests/test_fase375a_streaming.cpp:382  run.total.drawCalls == 8
  FALHOU  tests/test_fase375a_streaming.cpp:383  run.total.instancesMerged == 48
  FALHOU  tests/test_fase375a_streaming.cpp:384  run.drawn == 48
fase375a_sem_freeze_1a_frame                   FALHOU
```

Reposto (cp do backup): 0 teste(s) com falha. (O diff completo das 3
mutações: o loop do streamIn sem o teto / o stable_sort removido / o
resident() a devolver true — as 3 restauradas por cp do backup.)

- **test_core: verde** (a suíte inteira com os 8 pins + a R-048; os 2
  flakes DA SANDBOX ficam declarados no §4 — nenhum é desta sub-fase).
- **c33_virtual: 925/925 checks VERDE** (891 → 925: a FASE 29 nova, §2.1
  abaixo).
- **Gates locais TODOS verdes**: scope · docs-lint · gmesh-docs ·
  ui-vocab · theme-hex · glyph · hierarchy · projects-ui · reload ·
  check_main · link_parity · release-identity vc 75 (§2.2).
- **versionCode: 74 → 75** (app/build.gradle:29 + o §14 do
  RELATORIO-0.9.6.md — o par do gate release-identity/R-015).

### A FASE 29 do c33 (+34 checks, 891→925) e o 23.2 na convergência

No caminho REAL do device (o drawTics com o blockRebind e o storage
REAL), a cena limpa das fases anteriores e 6 clones de um .gmesh de 12
blocos EM LINHA (o bloco i a 10×i do olho):

- 29.0 o .gmesh no storage REAL + o default ON nos 6 clones.
- 29.1 O 1.º frame carrega SÓ a vaga: loaded=8 (o lazy de sempre
  carregava 12), drawn=48, instancesMerged=48, pending=24, dc=8 — o
  frame é LEVE por construção.
- 29.2 As CAMADAS: os entries 0..7 residentes, 8..11 à espera (os mais
  pertos primeiro; os empates na ordem da tabela).
- 29.3 O 2.º frame carrega a cauda (4) e fica COMPLETO: pending=0,
  drawn=72, merge=72, dc=12.
- 29.4 O steady state: 0 loads, 0 pendentes, o merge completo de novo.
- 29.5 O streaming OFF num clone: sai do batch e desenha 12
  drawElements individuais; os 5 ON fundem em 12 instanciados × 5;
  drawn=72 (60 fundidos + 12 individuais); o toggle reposto.
- O 23.2 (o city de 72 blocos sob SAF) passa a avaliar na CONVERGÊNCIA:
  o fill por camadas enche os 72 blocos em ~9 frames e o loop do pin
  corre frames até «a transmitir» secar — o contrato do SAF-SEAM («a
  câmara larga vê TODOS os blocos») mantém-se no fim. As restantes
  fases de blocos (1-2 blocos ≤ a vaga de 8) ficaram verdes à primeira.

## §3 Pin C33 (o dono preenche — com PNG)

- Antes: FPS 5 · bl 5164/5178 (o baseline da ABERTURA; o freeze do
  import ao abrir a cena dos 6 modelos)
- Depois do import: o freeze em frames ≤8 loads (o HUD/log dizem
  «a transmitir=N» até secar) · FPS <N> · bl <N>
- PNG do HUD (o chip «bl desenhados/totais» a encher por camadas; o log
  diz «a transmitir=N»)
- Zero crash dumps novos: <N>

(a cena dos 6 modelos de 203 MB: com o instancing ON + streaming ON (os
defaults), o 1.º frame carrega 8 blocos e o resto chega em vagas de 8 —
~108 frames para os 863 por modelo, ~2-4 s de fill progressivo SEM um
único frame congelado. O FPS durante o fill é o do draw normal: os
blocos residentes desenham todos os frames.)

## §4 Limites / não-verificado (nenhum escondido)

- **O sign-off da 3.5b no C33 não foi preenchido** (o §3 do
  RELATORIO-0.11-W-3.5B.md segue com os placeholders): o CONTINUAR do
  dono abriu esta sub-fase — assumido como sign-off pelo padrão da casa
  (o CI da 3.5b estava verde em todos os jobs, vc 74 publicado). O pin
  C33 da 3.5b continua em aberto e soma-se ao desta sub-fase.
- **O streaming cobre o caminho do BATCH (o instancing)** — os TICs com
  instancing ON (o default) + streaming ON. Um TIC com instancing OFF
  (ou skinado) desenha pelo caminho individual de sempre: lazy inline,
  SEM orçamento (o freeze de sempre, assumido e declarado). A cena do
  dono é o caminho do batch. A generalização do orçamento ao caminho
  individual fica no BACKLOG se o dono a precisar.
- **O orçamento 8 é o default declarado, não o sintonizado** — 8 ×
  ~1-3 ms ≈ 8-25 ms de trabalho por frame na sandbox; no C33 (o ARM
  mais lento) o custo por bloco sobe e o frame alonga. Se o fill
  custar FPS ao dono, a constante é UMA linha (kBlockStreamBudgetPerFrame).
- **O custo de CPU do stream por frame é o sort dos pendentes** — O(n
  log n) sobre os pedidos NÃO residentes (no pior caso do dono, 863 no
  1.º frame, a decair); barato contra os 4300 draw calls poupados pela
  3.5b (a medição real é do dono).
- **O fill nunca termina se o working set exceder o orçamento VRAM** —
  o comportamento de churn de sempre (o aviso 1× por open no log); o
  LRU continua a mandar, o stream só alimenta.
- **Os 2 flakes DA SANDBOX (9.9G de disco, nproc=2) — nenhum desta
  sub-fase:** (1) a `sentinela_r039` falha POR DISCO na suíte cheia
  (errno 28 — os fixtures das fases anteriores esgotam os 9.2G livres;
  isolada VERDE com o MESMO binário, provado 2×); (2) a
  `regress_streaming_paralelo` (R-046, o caminho do conversor — ZERO
  linhas desta sub-fase) toma SIGSEGV INTERMITENTE na suíte cheia da
  sandbox (4/6 corridas; ISOLADA 9/9 VERDE; ASAN isolado limpo) — a
  MESMA classe do flake documentado na abertura do 0.11-T-FIX («o MESMO
  código foi verde no b0613af: flaky») e a razão da Perna 2 (a cura
  real) estar ADIADA pelo dono. O CI (o árbitro, hw=4) decide: verde na
  3.5b com a mesma R-046.
- **O rasterizador do stub e os contadores do glstub não mudaram** — os
  pins existentes de contagem (FASE 20.2/28) ficaram verdes à primeira:
  com ≤8 blocos o fill completa no 1.º frame e os números são os de
  antes (a recalibração só no 23.2, §2.1).
- **O V.ONI não ganhou entradas** (a regra 1.6 — o VoniRegistry.cpp não
  foi tocado; VONI_referencia.md intacto).

## §5 Ficheiros tocados

| Área | Ficheiros |
|---|---|
| render | `render/BlockMesh.h/.cpp` (kBlockStreamBudgetPerFrame + StreamReq + streamIn/resident/streamLoad) |
| platform | `platform/main.cpp` (o stream entre o agrupamento e a submissão + a PORTA + o BlockFrameStats.pending + o log «a transmitir») |
| components | `components/MeshRenderer.h` (o toggle streaming, default ON) |
| core | `core/SceneSerializer.cpp` ("streaming":false só quando false), `core/UndoStack.h/.cpp` (o TicSnap transporta) |
| ui | `ui/EditorLayout.h` + `ui/EditorUi.cpp` (a linha «streaming: sim/não», id 5313) |
| testes | `tests/test_fase375a_streaming.cpp` (NOVO, 8 pins), `tests/test_sentinels.cpp` (R-048), `tests/c33_virtual.cpp` (FASE 29 + o 23.2 na convergência), `tests/CMakeLists.txt` (o TU novo) |
| build | `app/build.gradle` (versionCode 74→75) |
| docs | `docs/RELATORIO-0.11-W-3.75A.md` (este), `docs/REGRESSOES.md` (a R-048), `docs/RELATORIO-0.9.6.md` (§14 vc 75) |
