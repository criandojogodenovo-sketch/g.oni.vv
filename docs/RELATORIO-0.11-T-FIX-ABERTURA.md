# 0.11-T-FIX — ABERTURA (corrupção no corte paralelo)

> Campanha P0 · o bug vive no caminho mais quente do dono (import de
> assets grandes no C33 com 8 cores → 7 workers) · corre ANTES de
> qualquer sub-fase nova (3.5b incluída) · a FASE 3.5 não se declara
> fechada enquanto este bug não fechar com repro no CI + cura provada.

## §0 Estado de entrada

- 3.5a fechada — versionCode 72, CI verde (run 38573108989 no commit
  `b0613af`: core ✓ · c33 ✓ · APK arm64 assinado ✓ · symbol gate ✓;
  init-keystore skipped por desenho). O APK `goni-vv-0.9.6-release-signed`
  está publicado (artifact do run, não expirado).
- HEAD do main = `196ccbd` (o relatório-evidência da 3.5a sobre o
  `b0613af`) — working tree limpo, local == origin/main.
- Bug descoberto durante a 3.5a: heap corruption no `convertGltfToV3`
  (o corte paralelo). O §4.1 do RELATORIO-0.11-T-3.5A.md documenta a
  caça inteira.
- Reproduzido: com ≥7 workers forçados (o C33 tem 8 cores = 7 workers)
  — 2 crashes em ~5 suítes, o MESMO teste, a MESMA stack.
- Pré-3.5a (`3575344`, verde no run 38466867752) crasha IGUAL com 8
  workers forçados — o bug é pré-existente, não do LOD.
- TSAN no CI (run 38573108989, passo diagnóstico): 4 avisos, todos do
  handler do próprio TSAN (malloc dentro do signal — conhecido);
  NENHUMA data race reportada → o perfil é WILD-WRITE (corrupção de
  heap), não corrida de variáveis partilhadas.
- **ACHADO DESTE DIA (o run do HEAD)**: o run 38574932894 (o push do
  `196ccbd`) está VERMELHO — o job core-tests morre com SIGSEGV no
  `safstream_fase_ranges_linha_propria` (o 3.º vítima do streaming,
  depois de `safstream_data_uri_sem_ficheiro_tambem_streama` e de
  `safstream_saf_importa_city_em_blocos_pelo_fd_do_bridge` sob TSAN).
  O runner reporta `hw_concurrency=4` → 3 workers. O MESMO código foi
  VERDE no `b0613af` — a premissa «CI estável a 3 workers» do §4.1
  está FALSIFICADA: a corrida é flaky e JÁ dispara no CI com 3
  workers. Isto NÃO bloqueia a abertura (os 5 itens do §0.2 passam; o
  vermelho É o próprio bug desta campanha) — é o argumento mais
  forte para a Perna 2 existir e para o teto da Perna 1 ser
  imediato.
- Env do sandbox: **2 cores** (`nproc` = 2 → 1 worker) — a caça local
  NÃO reproduz; Toda a Perna 2 é no CI (o runner de 4 vCPU viu 3
  crashes até hoje; o job novo força 8).
- Env do device: C33 real RMX3624, Android 13, 8 cores → 7 workers no
  import streaming — a zona de crash no DEVICE do dono (o §4.1 da
  3.5a já o dizia; continua de pé).
- O §0.2 verificado: (1) HEAD = o último da 3.5a ✓; (2) run
  38573108989 verde no main com o vc 72 publicado ✓; (3) sandbox com
  2 cores ✓; (4) o §4.1 relido — o pré-3.5a crasha igual com 8
  workers forçados, TSAN sem data race, o crash é wild-write ✓;
  (5) o alvo real C33 = RMX3624 / Android 13 / 8 cores ✓.

## §1 Hipóteses do arquiteto (por ordem de probabilidade) — com o estado da prova da leitura 0.1

1. **`onProgress` callback a escrever em estado partilhado sem mutex
   (corre dentro dos workers)** — a invocação CONCORRENTE está provada
   no código: `GmeshV3Stream.cpp` `cutPrimitive` (linha 350) invoca o
   callback a cada `kProgressStride` (linhas 482-486) e o `worker`
   (linha 1091) corre `cutPrimitive` em TODOS os workers do pool — o
   MESMO par `(onProgress, user)` entra em N threads em paralelo. O
   callback de produção (`main.cpp:2605` `importJobProgress`) só faz
   `store` em atómicos — memória-seguro em si, mas o CONTRATO do
   callback nunca disse «podes chamar-me de N threads» e o callback do
   DEVICE fica à mercê disso em cada import. Nota honesta: os DOIS
   testes que crasham no CI passam `onProgress = nullptr` — esta
   hipótese SOZINHA não explica os crashes do CI; explica um defeito
   real do caminho do device e a monotonicidade partida do progresso
   (N workers fazem `store` de valores lidos em instantes diferentes →
   a barra anda para trás). A Perna 1.2 fecha a classe inteira.
2. **`madvise(MADV_DONTNEED)` em memória HEAP** (a lição F3 do
   AUDIT_SAF_JNI / RELATORIO-0.10-M PASSO 3 §2e) — o PERFIL do crash
   atual bate certo com o F3: morte dentro de `std::thread::join` e
   dentro de `~FakeSafIo` (a destruição do `std::map` — heap metadata
   corrompido), TSAN sem data race. Os guards do F3 existem e a
   leitura NÃO encontrou caminho vivo com heap + drop hoje:
   `TaskCtx.canDrop = haveMap && binDroppable` (heap/data-uri →
   `binDroppable=false`, AssetConverter.cpp:988), `VerifyCtx.droppable`
   é false por defeito e só acende no mmap do final. ESCLARECIMENTO ao
   modelo do arquiteto: o FakeSafIo NÃO devolve heap com
   `canDrop=true` — os fds dele são `memfd_create` (mmap file-backed
   legítimo) e a sonda do mmap é um PIPE (recusado de verdade pelo
   SO). O guard atual é POR CONVENÇÃO do chamador — um chamador futuro
   com `binDroppable=true` sobre heap reabre o F3. A Perna 1.3 torna o
   guard POR CONSTRUÇÃO: o `dropRange` (o ÚNICO chamador de madvise da
   engine) auto-verifica a proveniência e NUNCA corre em memória
   anónima/heap.
3. **Ponteiro mau/buffer fora de limites dentro de um worker** — a
   leitura não revelou OOB óbvio: os accessors são validados por
   `resolveAccessor` (todas as leituras do base ficam dentro dos
   limites declarados), `remap` é indexado por índices verificados
   `< posCount`, os índices u16 são limitados pelo cap do bloco. O
   TSAN é cego a wild-writes através de ponteiro podre. O árbitro é a
   Perna 2: o repro a 8 workers + o core dump como artefacto.

Adenda da leitura (fato novo, fora das três): o teste
`v3stream_worker_count_nunca_zero` (test_gmeshv3stream.cpp:866) afirma
o contrato ANTIGO (`cores−1` sem teto: 8→7, 16→15) — a Perna 1.1 muda
o contrato e o teste atualiza-se com o pin novo. E a dívida
documental: a sentinela R-045 (`regress_lod_cull`) existe no
test_sentinels.cpp:5831 com as provas coladas no §2 do relatório 3.5a,
MAS o registo em docs/REGRESSOES.md NÃO existe (o doc termina em
R-044) — registo feito no FECHO da campanha (com o padrão de sempre: o
registo entra quando o pin + a mutação já estão provados).

## §2 Estratégia em 3 pernas

- **Perna 1: contorno seguro (fecha o risco do dono JÁ)**
  - 1.1 Teto de workers: `v3StreamWorkerCount` passa a
    `clamp(cores−1, 1, 4)` — com o comentário de código a explicar o
    porquê e a apontar o BACKLOG da Perna 3. O teste existente
    atualiza-se para o novo contrato (8→4, 16→4).
  - 1.2 Mutex no onProgress: ESCOLHA declarada — `std::mutex` à volta
    da invocação no `cutPrimitive` (a opção menos invasiva: 3 linhas,
    zero mudança de estrutura; o callback deixa de ter entradas
    concorrentes; o cancelamento continua a fluir pelo atómico
    `canceled` que JÁ existe). A alternativa «callback só na thread
    do chamador» exigia reescrever o ciclo do corte (a chamadora
    bloqueia no join) — mais invasivo, mesma proteção.
  - 1.3 madvise só em mmap: a mudança vive no `GmeshV3Stream.cpp` (o
    `dropRange` é o ÚNICO ponto de madvise — o FileApi não decide
    canDrop, a verdade do código é esta); o `dropRange` passa a
    verificar a proveniência do range (`rangeIsFileBacked`, leitura de
    `/proc/self/maps` — Linux/Android/CI) e NUNCA madvise em memória
    anónima/heap, INDEPENDENTE dos flags dos chamadores. Defesa em
    profundidade no ponto único.
  - 1.4 Pins: 6 casos em tests/test_fase35fix_p1.cpp (teto ×2,
    onProgress seguro com 8 workers × 1000 iterações, madvise só
    mmap, compat 0.11-S byte a byte, streaming 10×).
  - 1.5 Sentinela R-046 `regress_streaming_paralelo` (teto + onProgress
    + madvise + streaming 10×) no test_sentinels.cpp.
  - 1.6 Mutações M-FIX1a (teto removido → vermelho), M-FIX1b (madvise
    em heap reintroduzido → vermelho), M-FIX1c (onProgress sem
    proteção → vermelho) — outputs colados no relatório da perna.
  - 1.7 Pin C33 (o dono preenche): import de 200 MB ×10 seguidas no
    device — zero crash, zero dumps novos, PNG do HUD e do log.
- **Perna 2: repro no CI com 8 workers (prova o palpite)**
  - Job novo `streaming-stress` (paralelo ao core-tests,
    `GONI_STREAM_WORKERS=8` — a env var nasce nesta campanha),
    100 iterações dos testes de streaming, core dump como artefacto
    se crashar. Verde 100× = a Perna 1 é declarada a cura.
  - Diagnóstico POR SUBTRAÇÃO: reverter UMA medida de cada vez (só o
    teto / só o mutex / só o madvise-guard) e correr o job — o que
    crashar isolado é o culpado provado. A tabela vai ao relatório.
- **Perna 3: cura real (SÓ se a Perna 2 exigir)** — o culpado da
  subtração manda: onProgress → o callback sai dos workers para
  sempre (atómico acumulado, callback na thread do chamador); madvise
  → a fonte heap deixa de existir (mmap anónimo no harness); nenhum
  dos dois → o core dump real do CI fala (nada inventado).

## §3 Cláusula calma

Zero features novas. Zero refactor. Zero mudança à linguagem.
Só o que está neste prompt. O LOD e o skinned cull da 3.5a (verdes) não
são tocados; o formato .gmesh/.goni não muda; o VoniRegistry.cpp e o
VONI_referencia.md intocados; o UBO 128 e a ordenação BLEND intactos.

## §4 Débitos declarados na abertura

- O CI do HEAD está vermelho (run 38574932894) pela PRÓPRIA corrupção
  que esta campanha fecha — a 3.5b não arranca antes do fecho (regra
  de convivência do prompt).
- O registo R-045 em docs/REGRESSOES.md (dívida documental da 3.5a,
  provas já coladas no §2 do relatório 3.5a) — entra no FECHO.
- A monotonicidade do progresso do import (a barra pode andar para
  trás com N workers — glitch benigno de UI, não corrupção) — a Perna
  1.2 reduz a janela; a cura completa é da Perna 3 se o onProgress for
  o culpado.
