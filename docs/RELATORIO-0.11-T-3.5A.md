# RELATORIO-0.11-T-3.5A — SKINNED CULL + LOD POR DISTÂNCIA

> versionCode 72 · 0.9.6 · a sub-fase 3.5a da campanha 0.11-T (FASE 3.5
> GPU Efficiency) · 5 pins novos (tests/test_fase35a_lod_cull.cpp) ·
> sentinela R-045 (regress_lod_cull) + as 2 mutações coladas · c33
> FASE 27 (+12 checks, 857→869) · suíte inteira 997 testes no harness
> local · gates locais verdes

## 1. O que foi feito

### O skinned cull (3.5a.1)

- **`core/AnimationSystem.h/.cpp`** — o system ganha a entrada
  `frustum` (os 6 planos do VP — o MESMO formato de
  `BlockMesh::frustumPlanes`) + `frustumValid` (o main injeta por frame)
  e os resultados `skinnedCulled`/`culledOwners` do último tick. O tick
  salta os TICs com SkeletonComp cujo mesh skinado (bounds do
  `BlockMesh`/do `Mesh`) tem o AABB inchado 25% (`kSkinCullAabbSwell`)
  inteiro fora do frustum: NEM `advance` NEM `apply` — o tempo do clip
  congela fora do ecrã e retoma do mesmo ponto ao voltar (o personagem
  invisível não gasta CPU de animação; o pin manda «skinned fora do
  frustum a animar: 0»). Sem frustum injetado o teste nem corre — custo
  zero, o comportamento de sempre byte a byte.
- **`render/BlockMesh.h/.cpp`** — `worldAabbVisible(model, mn, mx,
  planes)` PURA nova (o corpo do `blockVisible` de sempre, extraído —
  o R-040 continua a aferrar o MESMO cálculo através do blockVisible) +
  `kSkinCullAabbSwell = 0.25f` + `lodActive(model, mn, mx, camPos,
  dist)` pura.
- **`platform/main.cpp`** — os planos do vp do frame extraídos UMA vez
  (`g_skinFrustum`) e entregues ao `g_animSystem` (o vp nasce DEPOIS
  dos systems — o cull da animação usa os planos do frame anterior; o
  inchamento conservativo cobre a diferença de 1 frame, documentado). O
  `drawTics(vp, camEye)` REPETE o teste com os planos FRESCOS do frame
  e salta o TIC culled ANTES do `computeSkinMatrices` e do draw (os
  dois caminhos: blocos e mesh único) — o ping-pong de planos evita o
  pop de 1 frame. O contador do HUD/log é o do lado do DRAW
  (`SkinFrameStats` — somar os dois lados duplicava 5+5 o mesmo TIC).
- **`render/DrawStats.h`** — o struct `SkinFrameStats` (a fonte única:
  main, harness c33 e pins leem o MESMO).
- **HUD + log** — o chip «skinned: culled N/M» na status line (o padrão
  do chip `bl desenhados/totais`; SÓ nasce com skinados em cena — sem
  eles a linha fica byte a byte) + o log throttled 1×/s e só quando o
  número muda («skinned: culled=%u de %u skinados (fora do frustum —
  nem animam nem desenham; AABB +25%)»), o MESMO padrão do log dos
  blocos.

### O LOD por distância (3.5a.2)

- **`assets/GOwnFormats.h/.cpp`** — `kGmeshV3FlagLOD = 0x4` no meta;
  `gmeshDecimateHalf` PURA (o parity-weld determinístico: o vértice
  ímpar solda ao vizinho PAR mais próximo dentro de 25% da diagonal do
  AABB do bloco — fora disso é MANTIDO, nunca um sliver; degenerados
  caem; os atributos do representante vencem, a pele incluída — a
  emenda de pesos é a fronteira documentada; < 8 verts/AABB
  degenerado/nada soldável = sem LOD para o bloco); o escritor
  `gmeshV3AppendLod` + o leitor `gmeshV3ReadLod` (o MESMO contrato de
  cauda dos irmãos: sem magic = ok vazio, CRC/version/count mau = erro
  legível); `gmeshV3ReadProps` ganha o out-param `consumed` (o padrão
  do `gmeshV3ReadBinds`) e `gmeshV3ReadMaterialsOnly`/`readGMeshV3Meta`
  ganham o out-param opcional `lod` (os chamadores antigos byte a
  byte); `writeGMesh` ganha `withLod = false` (o default mantém os
  round-trips/exports byte a byte).
- **`assets/GmeshV3Stream.cpp`** — o corte decima CADA bloco durante o
  cut (o blob LOD vai para o temporário a seguir ao normal — o pico de
  RAM mantém-se POR BLOCO); o assembly emite a cauda GLOD depois dos
  materiais (índice 1:1 + blobs 16-alinhados nos offsets absolutos
  precomputados); o `outFileBytes` do progresso conta a cauda; o meta
  acende o flag ANTES da esqueleto.
- **`assets/AssetConverter.cpp`** — os TRÊS caminhos de import (OBJ,
  glTF fundido, glTF por-mesh) passam `withLod=true` — a geração de LOD
  é do import.
- **`render/BlockMesh.h/.cpp`** — a abertura lê a cauda (a faixa dos
  materiais cresce pelo índice `gmeshV3LodIndexBytes`; os blobs ficam
  no disco para o lazy); `entries_` cobre normais + LOD (o MESMO
  LRU/orçamentos — `blockAt(index)` resolve normal vs LOD); o draw
  troca bloco→LOD quando o TIC está além da distância
  (`lodActive` sobre o AABB do bloco) e `Stats::lodDrawn` conta; a
  partição opaco/blend continua keyed pelo bloco NORMAL (o LOD partilha
  o material e o AABB é subconjunto).
- **`components/MeshRenderer.h`** — `lodDistance` (0 = OFF, o default
  jogável manda).
- **`core/SceneSerializer.cpp`** — "lodDist" SÓ quando != 0 (a 0.11-S
  fica byte a byte); o loader recusa ≤ 0.
- **`core/UndoStack.h/.cpp`** — o TicSnap transporta `lodDistance`
  (snap/restore/paste); o `snapEq` NÃO compara (o MESMO contrato do
  tint — consistência com o comportamento existente).
- **`ui/EditorLayout.h` + `ui/EditorUi.cpp`** — a linha "lod distance:
  N u" no inspector (slider 0..500, o label diz "off" no 0 — nunca um 0
  silencioso; o id `kInspectorLodDist`).

## 2. Provas

- **Pins novos em tests/test_fase35a_lod_cull.cpp: 5 casos**
  1. `fase35a_skinned_cull` — 10 personagens skinados, 5 fora do
     frustum: `skinnedCulled == 5`; os culled com o tempo CONGELADO (os
     visíveis avançaram dt); sem frustum injetado o custo é zero.
  2. `fase35a_lod_gera` — o import gera a cauda GLOD: a grelha 32×32
     (1024 verts) decima para **EXATAMENTE 512**; o AABB do LOD contido
     no do bloco; a entrada materializa com CRC verde; o `withLod=false`
     fica sem flag e sem cauda (o ficheiro menor).
  3. `fase35a_lod_usa` — perto (5.5 u < 20 u) desenha o normal
     (`lodDrawn == 0`, todos os índices); longe (104 u > 20 u) desenha a
     ENTRADA LOD (`lodDrawn == 1`, os índices são os do LOD — a metade);
     `lodDistance = 0` NUNCA liga; a entrada LOD materializa 512 verts.
  4. `fase35a_lod_off_default` — TIC novo nasce OFF; o .goni sem edição
     NÃO tem a chave; com 120 u a chave nasce e volta no load; o
     negativo carrega 0; o round-trip sem lod é byte a byte.
  5. `fase35a_compat_011s` — o .gmesh SEM cauda desenha o normal com o
     LOD "ligado" (`hasLod() == false`, `lodDrawn == 0`); o .goni da
     0.11-S abre com o LOD off e o save(load(goni)) é byte a byte.
- **Sentinela R-045** (`regress_lod_cull` em tests/test_sentinels.cpp)
  — o cull conta certo (2 skinados, 1 fora → culled=1 + o tempo
  congelado), o LOD gerado (512 = ½ exatos) + usado (longe/perto/off) e
  o default off + o compat byte a byte.
- **Mutações 2 vermelho→verde (coladas)**

M-3.5a1 — o skinned cull removido (a condição do salto morta):

```
$ GONI_ONLY=regress_lod_cull ./test_core
== testes do core — G.One VV 0.7.7 ==
regress_lod_cull                               ...  FALHOU  tests/test_sentinels.cpp:5893  sys.skinnedCulled == 1
  FALHOU  tests/test_sentinels.cpp:5894  sys.culledOwners.size() == 1
regress_lod_cull                               FALHOU

2 teste(s) com falha
```

Reposto (cp do backup):

```
$ GONI_ONLY=regress_lod_cull ./test_core
== testes do core — G.One VV 0.7.7 ==
regress_lod_cull                               ...regress_lod_cull                               OK

0 teste(s) com falha
```

M-3.5a2 — o default do LOD ON (o MeshRenderer nasce com 300.0f):

```
$ GONI_ONLY=fase35a_lod_off_default ./test_core
== testes do core — G.One VV 0.7.7 ==
fase35a_lod_off_default                        ...  FALHOU  tests/test_fase35a_lod_cull.cpp:313  fresh.lodDistance == 0.0f
  FALHOU  tests/test_fase35a_lod_cull.cpp:322  std::strstr(text1.c_str(), "lodDist") == nullptr
fase35a_lod_off_default                        FALHOU

2 teste(s) com falha
```

Reposto (cp do backup):

```
$ GONI_ONLY=fase35a_lod_off_default ./test_core
== testes do core — G.One VV 0.7.7 ==
fase35a_lod_off_default                        ...fase35a_lod_off_default                        OK

0 teste(s) com falha
```

- **Suíte inteira: test_core 0 falhas** (997 testes com as sentinelas —
  ver §4 o FIX do harness; a `sentinela_r039` flutua POR DISCO na
  sandbox — ver §4, o CI é o árbitro).
- **c33_virtual: 869/869 checks VERDE** (857 → 869: a FASE 27 nova).
- **versionCode: 71 → 72** (app/build.gradle + o §14 do
  RELATORIO-0.9.6.md — o gate release-identity/R-015 verde com o par).

### A FASE 27 do c33 virtual (+12 checks, 857→869)

No caminho REAL do device (a cena da FASE 26 viva — Pele + 9 clones
skinados com mesh/UBO a funcionar):

- 27.1 o LOD: a grelha .gmesh COM a cauda GLOD escrita no storage REAL
  do device abre POR BLOCOS; **o LOD tem EXATAMENTE metade dos
  vértices** (1024 → 512); longe (104 u > 20 u) a ENTRADA LOD desenha e
  os índices consumidos são os do LOD (a metade); o **default OFF
  desenha o normal mesmo longe**.
- 27.2 o skinned cull: 10 skinados no `drawTics` REAL (o caminho do
  device), 5 atrás do plano near → **skinned: culled=5** (o contador
  `SkinFrameStats` — a MESMA fonte do HUD), **5 draws**, 5 drawElements
  + 5 uploads do UBO (os culled não pagam NEM skin NEM draw).

## 3. Pin C33 (o dono preenche)

- Antes: <dc/verts/fps>
- Depois: <dc/verts/fps>
- PNG do HUD

(a cena do pin 3.5a.6: o corredor com 50 personagens a andar — o
«skinned: culled N/M» no HUD reflete os fora do frustum; o bl desenhados
desce com a distância; fps ≥ 58. O device real é o árbitro.)

## 4. Limites / não-verificado (nenhum escondido)

- **O sign-off no C33 físico é do dono** (o padrão de todas as fases) —
  os números do harness são o PROXY; o device real (PowerVR GE8320)
  tem a palavra final sobre o swell do AABB (se algum personagem
  visível saltar, é UMA constante a subir: `kSkinCullAabbSwell`), o
  custo real do LOD no GPU e o pin do corredor.
- **A qualidade visual do parity-weld** — o decimate ½ é determinístico
  e barato mas NÃO é um simplificador de superfície (sem preservação de
  arestas/UV seams); a distância do LOD esconde, o C33 confirma. Um
  simplificador melhor é fase futura se o dono pedir.
- **A emenda de pesos do LOD skinado** — os atributos do representante
  vencem; um rig com pesos divergentes entre vértices soldados pode
  mostrar uma emenda à distância do LOD (documentado no §GLOD).
- **O CUSTO DE DISCO da cauda GLOD** — o LOD acrescenta ~40-50% aos
  bytes de geometria do .gmesh (o temp E o final no import). O pico de
  RAM NÃO mudou (streaming por bloco; a r039 mediu 144 MB ≤ 512 MB).
  Mas o pico de DISCO dos artefactos subiu ~40%: a `sentinela_r039`
  (o perfil de 50M verts) flutua POR DISCO nesta sandbox (9.9 GB no
  volume; a corrida com folga medida passou: 920 blocos,
  verificado=1, RAM pico 144 MB). O CI (runner com mais disco) é o
  árbitro. Se o CI também apertar, as opções estão declaradas para o
  dono decidir: (a) runner maior, (b) withLod opt-in para modelos
  gigantes — NUNCA decidido sem o OK do dono.
- **FIX de passagem no harness LOCAL (fora do repo)** — o awk do
  `scripts/build_testcore.sh` do sandbox excluía a ÚLTIMA fonte do
  test_core (`test_sentinels.cpp)`) — as sentinelas NUNCA tinham
  corrido localmente (o CI corria-as; o sandbox não). Corrigido: a
  suíte local subiu de ~948 para 997 testes com as sentinelas dentro.
  O repo não muda por isso (o CMakeLists estava sempre certo).
- **§4.1 — O CI VERMELHO (run 38548829262) — a causa medida e o estado
  da caça** (a regra 1.7: PÁRA e reporta; nada fica escondido):
  - O SINTOMA: o job core-tests morre com SIGSEGV (sinal 11) no teste
    `safstream_data_uri_sem_ficheiro_tambem_streama` — a stack do crash
    (do handler) é a MAIN thread dentro de `std::thread::join` do corte
    paralelo do `convertGltfToV3`. 2 attempts, 2 crashes no MESMO sítio
    (901 testes OK antes em ambos).
  - **A PROVA DE QUE NÃO É O DIFF DA 3.5a**: o código PRÉ-3.5a (o
    commit `3575344` — VERDE no CI no run 38466867752) reproduz o crash
    IDÊNTICO (mesmo teste, mesma stack: join dentro do convertGltfToV3)
    quando o nº de workers é FORÇADO a 8 — 2 crashes em ~5 suítes.
    Com 3 workers (o que um runner de 4 vCPU dá), o pré-3.5a E o meu
    push correm 3 suítes cada SEM crash (local, -O3).
  - A CAUSA RAIZ: uma **corrida latente PRÉ-EXISTENTE** no corte
    paralelo do streaming (o caminho multi-worker existe desde a
    0.10-M PASSO 3). A probabilidade depende do nº de workers real e
    do intercalamento (o meu push mudou o layout do heap/binário com os
    testes novos — a lotaria do CI virou). O sandbox (2 cores → 1
    worker) NUNCA a viu; o CI (3 workers) passava e agora perde 2×.
  - **O RISCO REAL PARA O DEVICE**: o C33 tem 8 cores → 7 workers no
    import streaming → o crash pode acontecer NO DEVICE do dono (um
    SIGSEGV a meio de um import grande). NÃO é só CI.
  - **O QUE EU MUDEI ENTRETANTO (à prova de bala para a 3.5a)**: a
    decimação LOD saiu do caminho dos WORKERS — corre no ASSEMBLY
    (single-thread) em DUAS PASSADAS: a A calcula tamanhos/CRC/aabb por
    bloco (o índice GLOD), a B re-lê o blob NORMAL do temporário,
    RE-DECIMA (o decimate é determinístico) e CONFERE o CRC contra a A
    antes de emitir — o pico de RAM continua a ser UM bloco e o
    intercalamento dos workers fica byte a byte pré-3.5a. Local: a
    suíte inteira + c33 869/869 verdes com esta versão.
  - **O QUE NÃO CONSEGUI**: apanhar a corrida em flagrante — o sandbox
    não tem gdb; TSAN (a -O1 isolado ×4 e a suíte sem as medições)
    não reportou a corrida (o TSAN não apanha o SIGSEGV de um wild
    write no TCB de outra thread; a suíte TSAN completa não coube no
    tempo). A caça continua com o dono a decidir o caminho.
  - **AS OPÇÕES PARA O DONO** (nenhuma executada sem OK):
    (a) caçar a corrida com TSAN num machine multi-core real (o CI
        mesmo, com um passo temporário de TSAN);
    (b) CAPAR os workers do streaming (ex.: min(4, núcleos)) — 1 linha,
        custo real no tempo de import dos modelos grandes no device,
        muda o envelope de performance declarado no relatório 0.10-M;
    (c) cortar SEQUENCIAL o caminho HEAP/data-URI (o que o teste que
        crasha usa) — mais cirúrgico, o mmap paralelo fica;
    (d) aceitar a lotaria (re-runs até verde) — NÃO recomendo: o device
        8-core do dono está na zona de crash.
- **O frustum do cull da animação é o do frame anterior** — o vp nasce
  DEPOIS dos systems (a ordem do Tick); o inchamento conservativo de
  25% cobre a diferença; o drawTics re-testa com os planos frescos (o
  pop de 1 frame não existe no draw).
- **O V.ONI não ganhou entradas** (a regra 1.6 — o VoniRegistry.cpp não
  foi tocado; VONI_referencia.md intacto).

## 5. Ficheiros tocados

| Área | Ficheiros |
|---|---|
| assets | `assets/GOwnFormats.h/.cpp` (o flag GLOD, a decimação, o append/read Lod, o consumed do props, o withLod no writeGMesh), `assets/GmeshV3Stream.cpp` (o LOD no corte + a cauda no assembly), `assets/AssetConverter.cpp` (os 3 imports com withLod) |
| render | `render/BlockMesh.h/.cpp` (a cauda na abertura, as entradas LOD, o draw com camPos/distância, os puros worldAabbVisible/lodActive/kSkinCullAabbSwell), `render/DrawStats.h` (o SkinFrameStats) |
| core | `core/AnimationSystem.h/.cpp` (o skinned cull), `core/SceneSerializer.cpp` (lodDist), `core/UndoStack.h/.cpp` (o TicSnap) |
| components | `components/MeshRenderer.h` (lodDistance) |
| ui | `ui/EditorLayout.h` + `ui/EditorUi.cpp` (a linha do inspector) |
| platform | `platform/main.cpp` (os planos, o drawTics com cull+LOD, o HUD+log) |
| testes | `tests/test_fase35a_lod_cull.cpp` (NOVO, 5 pins), `tests/test_sentinels.cpp` (R-045), `tests/c33_virtual.cpp` (FASE 27 + os 3 call sites do drawTics), `tests/CMakeLists.txt` (o TU novo) |
| docs | `docs/RELATORIO-0.11-T-3.5A.md` (este), `docs/GMESH_formato.md` (§GLOD), `docs/RELATORIO-0.9.6.md` (§14 vc 72), `README.md`, `BACKLOG.md` |
| build | `app/build.gradle` (versionCode 71→72) |
