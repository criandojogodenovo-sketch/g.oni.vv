# RELATORIO-0.10-M-HOTFIX-P0-SUBTICS.md

**0.10-M HOTFIX P0** — a spec do dono (dono, C33): o «expandir em sub-TICs»
nos settings fazia a engine TRAVAR / não responder / crashar. Diagnóstico
por probabilidade (a)-(d), cura completa, + (e) o termo e (f) o import
cria a sub-árvore no próprio import. NADA mais tocado.

## 1. A causa raiz (ficheiro + função, as 4 probabilidades julgadas)

| # | Causa | Veredicto | Onde vivia |
|---|---|---|---|
| a | expansão na thread de UI com modelo grande (ANR) | **CONFIRMADA** — o `importJobFinish` (main.cpp) corre no frame (thread da UI) e o bloco do expand fazia TUDO de uma vez: `gltfExpandInstantiate` (cria+configura+liga N TICs), o `bindMesh` = `blockHull` por peça (abre a tabela do .gmesh), o `matBindsOf` por peça (peek+statBytes+leitura da faixa), o composeWorlds e o fit — no city eram 122 peças num frame; num modelo maior, minutos de UI bloqueada | main.cpp `importJobFinish` (o bloco «A SUB-ÁRVORE ENTRA NA CENA») + `GltfExpandBatch` não existia |
| b | walk de nós sem proteção de ciclos | **CONFIRMADA** — TRÊS walks subiam pelos `parent` SEM conjunto de visitados: `worldChainOf` (GmeshV3Stream.cpp:185), o walk do ancestral registado e o walk da cadeia local (o 2.º passe de `convertGltfToV3Nodes`). Um glTF malformado com pais em ciclo (n1.parent=n2, n2.parent=n1 — o parse aceita: cada children[i] escreve parent) gira PARA SEMPRE; no walk da cadeia o `push_back` ilimitado era OOM→crash | GmeshV3Stream.cpp (os 3 walks; a mutação M-P0a prova: sem visitados = `std::bad_alloc`) |
| c | criação ilimitada de TICs | **CONFIRMADA** — a emissão de peças (`convertGltfToV3Nodes`) iterava TODOS os nós com mesh sem teto: 5000 nós = 5000 .gmesh + 5000 TICs (a explosão de entidades) | o loop de emissão em GmeshV3Stream.cpp (sem limite declarado) |
| d | reentrância (expandir durante import/play) | **CONFIRMADA PARCIAL** — não havia guard: um import novo podia arrancar com o expand a meio (o worker sobrescreveria `g_importJob.out` por baixo do job) e o finalize podia correr em PLAY | `importJobStart`/`browserExtractArchive` (sem guard do expand) + o finalize (sem guard do play) |

## 2. A cura (as 5 pernas + (e)/(f) da spec)

1. **expansão assíncrona com progresso+cancelamento · lotes fora da
   thread de UI** — o `GltfExpandBatch` NOVO (GltfInstantiate.h/.cpp): o
   `start()` cria TODOS os TICs (a 1.ª passada de sempre — barata, sem
   GL/I/O) e as `tick()` configuram NO MÁXIMO `kGltfExpandBatchTics` = 32
   registos por chamada (parent+TRS+MeshRenderer+bind + o gancho
   `perPiece`). O `gltfExpandInstantiate` (one-shot) fica como WRAPPER
   (todas as ticks em sequência + composeWorlds) — UMA fonte de verdade:
   o estado final é o MESMO de sempre (o pin da sub-árvore real passou
   pelo wrapper). O main.cpp ganha o `g_expandJob` (dono das cópias dos
   registos/refs): o `importJobFinish` só o ARRANCA; o frame corre UM
   `expandJobTick()` no PONTO SEGURO (o mesmo do importJobFinish — antes
   de qualquer submissão GL); o finalize (fit + composeWorlds + seleção +
   toast/log) corre na última tick. O overlay do import ganha a fase
   «A EXPANDIR… materializando sub-TICs: d / t» com a barra pelo lote e o
   MESMO botão cancelar.
2. **deteção de ciclos (conjunto de visitados)** — os TRÊS walks ganham
   `std::vector<char> seen[]`: o ciclo corta no primeiro re-visit e o log
   DIZ («asset: ciclo nos nós do glTF (pai em ciclo em N) — a cadeia do
   mundo corta no visitado»). A cadeia válida (acíclica) é byte a byte.
3. **limites declarados (máx de nós por operação, aviso claro acima)** —
   `convert::kGmeshV3ExpandMaxTics = 2000` (AssetConverter.h, a fonte
   única): a emissão para de emitir peças no limite, conta os que ficaram
   fora em `Stats::expandLimited` e loga o aviso («LIMITE de 2000 peças
   atingido…»); o toast do finalize diz «expandido: N TIC(s) — LIMITE (k
   peça(s) fora)». Os filhos dos nós de fora re-atracam ao ancestral
   mantido ou ficam FLAT (o 2.º passe resolve — a sub-árvore fica correta
   no que entrou).
4. **guardas de estado (não expandir durante import/play)** — o
   `importJobFinish` recusa o expand em PLAY (toast + log honesto: os
   ficheiros ficam nos assets, reimporta fora do play); o
   `importJobStart`/`browserExtractArchive` recusam-se com o expand a
   correr («expansao em curso — espera ou cancela no overlay»).
5. **cancelamento limpo** — `GltfExpandBatch::cancel()` destrói os TICs
   já nascidos (ordem inversa — os filhos primeiro) e fecha o job: a cena
   fica COMO ESTAVA antes do expand; o toast diz «expansão cancelada (a
   cena ficou como estava)». O átomo `g_importJob.cancel` serve as duas
   fases (o mesmo botão do overlay).
- **(e) o termo** — os últimos «expandir nós» vivos morrem: a linha de log
  dos clips (AssetConverter.cpp) e o comentário do driver (GmeshV3Stream.cpp)
  passam a «expandir em sub-TICs» (o settings já estava do hotfix 1).
- **(f) o import cria a sub-árvore no próprio import** — JÁ era o caminho
  (o hotfix 1 fez o 2.º passe + o instantiate no finalize; o pin do 5B:
  o city expandido = 72) — o P0 mantém-no e torna-o EM LOTES: o import
  com a opção ativa continua a nascer com a sub-árvore correta (nomes do
  glTF, TRS local por TIC, o fit nas raízes) SEM nenhum botão pós-import;
  o que muda é o RITMO (32 peças por frame, com progresso e cancelamento).

## 3. As provas (os pins novos, test_texbind.cpp)

- `expand_p0_ciclo_de_pais_termina_e_diz` — glTF com 2 nós com mesh e
  pais em CICLO (0→1→0 pelos children): o import TERMINA (ok=true, as 2
  peças saem) e o log diz o ciclo — o mutante M-P0a SEM visitados morre
  com `std::bad_alloc` no MESMO pin (a cadeia cresce sem fim).
- `expand_p0_limite_de_pecas_e_aviso` — 2100 nós com mesh: as peças
  paramos no limite declarado (`expandNodes.size() ==
  kGmeshV3ExpandMaxTics`), `stats.expandLimited == 100` e o log diz
  «LIMITE de 2000».
- `expand_p0_lote_progresso_e_cancelamento_limpo` — 100 registos: o
  `total()==100/done()==0`; CADA tick configura ≤32 (`done` 32/64/96/100 —
  a LEI DO LOTE; o mutante M-P0b «tudo num tick» morre NESTE assert ×3);
  o gancho `perPiece` corre dentro do lote (32 chamadas na 1.ª tick); os
  TICs existem desde o start; o `cancel()` limpa (a cena volta ao
  anterior); e o one-shot `gltfExpandInstantiate` produz o MESMO estado
  (o wrapper).
- Os pins do expand de sempre mantêm-se verdes (a sub-árvore real
  parentRec+TRS local, o compose no TransformSystem, o city do 5B).

## 4. As mutações (vermelho → verde)

| Mutação | O que morre | Prova |
|---|---|---|
| M-P0a «walk sem visitados» | o `seen[]` do `worldChainOf` sai (o pai em ciclo gira) | `expand_p0_ciclo_…` **VERMELHA por ABORT** (`std::bad_alloc` — o chain sem fim) — **reposta: verde** |
| M-P0b «expansão na thread de UI» | o lote morre: a tick configura TUDO num frame (`end = nodes->size()`) | `expand_p0_lote_…` **VERMELHA ×3** (`!tick()`, `done()==32`, `perPieceCalls==32`) — **reposta: verde** |

## 5. Os gates + suítes

- test_core: **947/948** (944 + 3 pins novos; full rebuild limpo) — o 1
  vermelho é a `sentinela_r039` de SEMPRE desta sandbox (errno 28 DISCO
  CHEIO na escrita de ~4,75 GB; correu verde com o disco limpo na corrida
  tc2 — o diff não toca na escrita do convert). O CI é o árbitro.
- c33_virtual: **797/797** HARNESS VERDE (o main.cpp compila no TU do
  device virtual — o replay inteiro corre por cima do job em lotes).
- Gates: scope VERDE · docs-lint 79 · gmesh-docs 25 · ui-vocab · theme ·
  hierarchy · gizmo · projects-ui · reload · glyph · jni-parity ·
  check_main · link-parity 122 TUs · release-identity **versionCode 67**
  (declarado no RELATORIO-0.9.6 §14).

## 5-A. O CI (o selo)

- **CI VERDE À PRIMEIRA — run 38322688785 (c7a1638)**: core-tests SUCCESS
  · c33 virtual SUCCESS · APK release arm64 assinado SUCCESS
  (versionCode 67) · verify-entry-symbols SUCCESS. Artefacto:
  **goni-vv-0.9.6-release-signed** 1,7 MB (id 11779463544, expira
  2027-01-10) — o APK que o dono instala para o sign-off (o city com o
  overlay «A EXPANDIR… d/t» sem ANR + o cancelamento a meio).

## 6. NÃO VERIFICADO (honesto)

- O sign-off no DEVICE: o city (122 nós) com a opção ativa — o overlay
  «A EXPANDIR… d/t» sem ANR, os 72 TICs na hierarquia com nomes+TRS local
  (o pin do 5B em vida), e o cancelamento a meio com a cena limpa.
- O sintético de 5000 nós + ciclo no DEVICE (no hospedeiro o ciclo morre
  no pin; o ANR de 5000 TICs não é reproduzível fora do device — os lotes
  de 32/frame são a cura, a medição de frame-time real é do device).
- A renomeação (e) está feita nos LOGS VIVOS + settings + docs vivos; os
  screenshots/docs antigos guardam o termo de época (história intacta).
