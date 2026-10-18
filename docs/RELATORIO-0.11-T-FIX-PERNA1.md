# 0.11-T-FIX Perna 1 — O CONTORNO SEGURO (fecha o risco do dono já)

> Campanha 0.11-T-FIX (a abertura: docs/RELATORIO-0.11-T-FIX-ABERTURA.md)
> · o objetivo da perna: o dono importa 200 MB ×10 seguidos sem crash no
> C33 — NÃO é a cura, é fechar a ferida enquanto a Perna 2 prova o
> culpado · versionCode 72 → 73 · 6 pins novos + sentinela R-046 + 3
> mutações coladas · test_core 1003 testes · c33_virtual 869/869.

## §1 O que foi feito

### 1.1 — O teto de workers

- **`assets/GmeshV3Stream.h:64-73`** — o contrato do
  `v3StreamWorkerCount` documentado: núcleos−1 com TETO DE 4, nunca 0;
  a reversão para o núcleos−1 sem teto é explícita (quando a cura real
  chegar).
- **`assets/GmeshV3Stream.cpp:730-741`** — a implementação:
  `clamp(núcleos−1, 1, 4)` com o comentário de código a explicar o
  contorno e a apontar o BACKLOG 0.11-T-FIX-Perna3.
- **`assets/GmeshV3Stream.cpp:742-760`** — o override de EXPERIMENTO
  `GONI_STREAM_WORKERS` (1..64): a precedência sobre o teto é POR
  DESENHO (a utilidade é recriar a zona de perigo para os pins e para
  o job streaming-stress da Perna 2); fóra de faixa/lixo = ignorado;
  produção nunca define a variável. Consumido em
  `GmeshV3Stream.cpp:1079-1081`.

### 1.2 — O mutex no onProgress (a escolha declarada)

- **ESCOLHA: `std::mutex`** (a opção "protegido por um std::mutex" do
  prompt) — a menos invasiva: 3 linhas, zero mudança de estrutura; a
  alternativa "callback só na thread do chamador" exigia reescrever o
  ciclo do corte (a chamadora bloqueia no join) para a mesma proteção.
- **`assets/GmeshV3Stream.cpp:346-360`** — `cutPrimitive` recebe
  `std::mutex* onProgressMu` (null = chamador single-thread — a
  verificação).
- **`assets/GmeshV3Stream.cpp:484-507`** — a invocação SERIALIZADA: o
  callback nunca corre de N workers em paralelo; o cancelamento
  continua a fluir pelo atómico `canceled` (fora do mutex).
- **`assets/GmeshV3Stream.cpp:1073-1075`** — o `progMu` da conversão (a
  MESMA instância para todos os workers); **`:1120-1124`** — o worker
  passa `&progMu`; **`:1685`** — a verificação (single-thread) passa
  `nullptr`.
- Efeito colateral honesto: a monotonicidade partida do progresso (N
  workers fazem `store` de valores lidos em instantes diferentes — a
  barra pode andar para trás na UI) fica com a janela reduzida (as
  entradas deixam de ser concorrentes); a cura completa é da Perna 3 se
  o onProgress for o culpado provado.

### 1.3 — O madvise só em mmap (defesa POR CONSTRUÇÃO)

- **`platform/FileApi.h:165-186`** — as declarações:
  `rangeIsFileBacked(addr, len)` (a proveniência: true = mapeamento
  file-backed inteiro) e `dropRange(base, off, len)` (o PONTO ÚNICO da
  higiene).
- **`platform/FileApi.cpp:668-730`** — `rangeIsFileBacked`: lê
  `/proc/self/maps`, exige o range INTEIRO dentro de UM mapeamento com
  pathname NÃO-pseudo (os `[heap]`/`[stack]`/`[anon:*]` do kernel NÃO
  contam); cache thread-local de UMA entrada (os chamadores perguntam
  em sequência ascendente — o verifySink bloco a bloco, o corte por
  janela de 32 MB); /proc indisponível → false (o madvise NUNCA corre
  — a higiene perde-se, a correção não).
- **`platform/FileApi.cpp:732-750`** — `dropRange`: o guard da
  proveniência ANTES do `madvise(MADV_DONTNEED)` — heap/anónima é
  NO-OP INDEPENDENTE dos flags dos chamadores (a lição F3 à prova de
  chamador futuro).
- **`assets/GmeshV3Stream.cpp:248-251`** — o dropRange local da engine
  MORREU; os 4 call sites chamam o `fileapi::dropRange`
  (`:510`, `:525`, `:714`, `:1713`). Os guards existentes
  (`binDroppable=false` no data-uri — AssetConverter.cpp:988;
  `VerifyCtx.droppable=false` por defeito) ficam como PRIMEIRA camada;
  a verificação de proveniência é a SEGUNDA.
- Nota ao modelo do arquiteto (honestidade da ABERTURA §1.2): o
  FakeSafIo devolve MEMFDS (file-backed — o drop é legítimo) e um PIPE
  na sonda; NÃO devolve heap com canDrop=true. O heap de verdade é o
  data-uri e o readback virtual — ambos guardados desde o F3; esta
  perna torna o guard à prova de quem venha depois.

### 1.4/1.5 — Os 6 pins + a sentinela

- **`tests/test_fase35fix_p1.cpp`** (NOVO — 6 casos, CMakeLists.txt:87):
  1. `fix_p1_teto_workers` (:297) — 8/9/16/1024 → 4; 3 → 2; 5 → 4.
  2. `fix_p1_teto_workers_min` (:307) — 2 → 1; 1 → 1; 0 → 1 (o
     contrato M3 do PASSO 3 continua de pé).
  3. `fix_p1_onprogress_seguro` (:314) — 1000 imports com 8 workers
     FORÇADOS e 3 primitivas × 17000 tris (CADA tarefa cruza o
     `kProgressStride` de 16384 — os WORKERS disparam o callback de
     verdade; a 1.ª versão do pin com fixture pequeno só atingia as
     chamadas single-thread do assembly/verify — a lição fica
     registada). O callback adversário deteta SOBREPOSIÇÃO
     (`inFlight`) e perdas aritméticas (`total` não-atómico ×8 por
     chamada vs `calls` atómico): zero sobreposições, zero perdas.
  4. `fix_p1_madvise_so_mmap` (:346) — heap: `rangeIsFileBacked`
     false + `dropRange` NO-OP (o conteúdo byte a byte); mmap de
     ficheiro real: true + o drop corre + o conteúdo re-lê bit a bit;
     memfd (a realidade do provider SAF): true.
  5. `fix_p1_compat_011s` (:411) — o .goni da forma 0.11-S abre com o
     save(load(goni)) BYTE A BYTE e o MeshRenderer com os defaults de
     sempre (lodDistance 0); o .gmesh write→read→write é
     determinístico (bytes1 == bytes2).
  6. `fix_p1_streaming_ok` (:457) — o MESMO caminho da vítima do CI
     (data: URI — o bin HEAP), 10 imports seguidos, todos com
     `verificado=1`.
- **`tests/test_sentinels.cpp:6217`** — a sentinela **R-046**
  `regress_streaming_paralelo`: teto ≤ 4 + callback seguro (100
  imports × 8 workers FORÇADOS, o MESMO detector) + madvise só em
  mmap + o streaming 10×. O registo em docs/REGRESSOES.md entra no
  FECHO da campanha (o padrão do prompt PARTE 3.4) — juntamente com o
  R-045 a backfillar (a dívida documental declarada na ABERTURA §4).
- **`tests/test_gmeshv3stream.cpp:864-874`** — o teste do contrato
  antigo (`v3stream_worker_count_nunca_zero`) atualizado para o novo
  contrato: 8 → 4 e 16 → 4 (era 7/15) — com o comentário a apontar os
  pins novos.

## §2 Provas

- **Pins novos: 6 casos** (test_fase35fix_p1.cpp) + o contrato antigo
  atualizado (2 asserts).
- **Sentinela R-046** `regress_streaming_paralelo` (test_sentinels.cpp:6217).
- **Mutações 3 vermelho→verde (coladas)**

M-FIX1a — o teto removido (o corpo volta ao núcleos−1 sem teto):

```
$ GONI_ONLY=fix_p1_teto_workers ./test_core
== testes do core — G.One VV 0.7.7 ==
fix_p1_teto_workers                            ...  FALHOU  tests/test_fase35fix_p1.cpp:287  v3StreamWorkerCount(8) == 4
  FALHOU  tests/test_fase35fix_p1.cpp:288  v3StreamWorkerCount(9) == 4
  FALHOU  tests/test_fase35fix_p1.cpp:289  v3StreamWorkerCount(16) == 4
  FALHOU  tests/test_fase35fix_p1.cpp:290  v3StreamWorkerCount(1024) == 4
fix_p1_teto_workers                            FALHOU
fix_p1_teto_workers_min                        ...fix_p1_teto_workers_min                        OK

4 teste(s) com falha
```

Reposto (cp do backup):

```
$ GONI_ONLY=fix_p1_teto_workers ./test_core
== testes do core — G.One VV 0.7.7 ==
fix_p1_teto_workers                            ...fix_p1_teto_workers                            OK
fix_p1_teto_workers_min                        ...fix_p1_teto_workers_min                        OK

0 teste(s) com falha
```

M-FIX1b — o guard da proveniência removido (o madvise corre em tudo —
o comportamento pré-F3). O pin NÃO falha numa assert: MORRE com a
assinatura EXATA da lição F3 §2e (o madvise em heap zeroou as páginas
e o destruidor do vector morreu no free):

```
$ GONI_ONLY=fix_p1_madvise_so_mmap ./test_core
== testes do core — G.One VV 0.7.7 ==
fix_p1_madvise_so_mmap                         ...free(): invalid pointer
```

Reposto (cp do backup):

```
$ GONI_ONLY=fix_p1_madvise_so_mmap ./test_core
== testes do core — G.One VV 0.7.7 ==
fix_p1_madvise_so_mmap                         ...fix_p1_madvise_so_mmap                         OK

0 teste(s) com falha
```

M-FIX1c — o mutex do callback removido (o callback corre de N workers
em paralelo). Nota de caça: a 1.ª versão do pin (fixture de 1200 tris)
ficou VERDE com a mutação aplicada — os workers nunca atingem o
`kProgressStride` (16384) e o callback só corria nas chamadas
single-thread do assembly/verify. O pin foi REFORÇADO (fixture de
17000 tris por tarefa + o detector de sobreposição) e a mutação ficou
vermelha como mandado:

```
$ GONI_ONLY=fix_p1_onprogress_seguro ./test_core
== testes do core — G.One VV 0.7.7 ==
fix_p1_onprogress_seguro                       ...  FALHOU  tests/test_fase35fix_p1.cpp:340  stress.overlaps == 0
fix_p1_onprogress_seguro                       FALHOU

1 teste(s) com falha
```

Reposto (cp do backup):

```
$ GONI_ONLY=fix_p1_onprogress_seguro ./test_core
== testes do core — G.One VV 0.7.7 ==
fix_p1_onprogress_seguro                       ...fix_p1_onprogress_seguro                       OK

0 teste(s) com falha
```

- **test_core: 1003 testes, 0 falhas** (a suíte inteira com os 6 pins +
  a R-046 dentro; com folga de disco — a `sentinela_r039` sem folga é
  o flake POR DISCO documentado na 3.5a, o CI é o árbitro; a corrida
  com folga local: RAM pico 106 MB ≤ 512 MB, 920 blocos,
  verificado=1).
- **c33_virtual: 869/869 checks VERDE** (o FASE 27 da 3.5a intacto; o
  replay corre por cima do código de produção desta perna).
- **Gates locais TODOS verdes**: scope (os ficheiros tocados estão
  todos no ci/scope.txt) · docs-lint 89 · gmesh-docs 25 · ui-vocab ·
  theme-hex · glyph · hierarchy · projects-ui · reload · check_main ·
  link_parity 122 TUs · release-identity vc 73.
- **versionCode: 72 → 73** (app/build.gradle:29 + o §14 do
  RELATORIO-0.9.6.md — o gate release-identity/R-015 verde com o par).

## §3 Pin C33 (o dono preenche — com PNG)

- import de 200 MB ×10 seguidos: <pass/fail>
- crash dumps novos: <N>

(o ficheiro de 200 MB da 0.10.3B; sem ele à mão: o city expandido
(72 blocos) + a scene de 228 MB do test_core (fixture conhecida). PNG
do HUD e do engine.log. O device de 8 cores é o árbitro do contorno.)

## §4 Limites / não-verificado

- **O contorno NÃO é a cura** — as três medidas reduzem a exposição
  (o teto tira o N de workers; o mutex tira a corrida à entrada do
  callback; o guard tira o madvise em heap) mas o culpado REAL só fica
  provado na Perna 2 (o job streaming-stress a 8 workers + o
  diagnóstico por subtração). Nenhuma declaração de cura aqui.
- **A sandbox não reproduz** — 2 cores → 1 worker (o §0.2 da
  ABERTURA); toda a caça por multiplicidade é do CI (Perna 2). O pin
  `fix_p1_onprogress_seguro` FORÇA 8 workers via env precisamente
  para exercitar a configuração do device localmente — mas a
  intercalação real de 8 cores num device é outra.
- **O CI do HEAD está vermelho ANTES desta perna** (run 38574932894,
  o SIGSEGV no `safstream_fase_ranges_linha_propria` — o próprio bug
  desta campanha). O push desta perna fecha o CI se o contorno cobrir
  a configuração de 3 workers; se a lotaria continuar (o bug pode ser
  intermitente mesmo com o teto), a Perna 2 tem o trabalho — NÃO se
  aceita "quase verde" como fecho da campanha.
- **A performance do import** — o teto de 4 workers reduz o paralelismo
  do corte no device (a 0.10-M declarou núcleos−1): no C33 (8 cores) o
  corte teórico desce para 4/7 da velocidade de corte. É o CUSTO
  DELIBERADO do contorno (a opção (b) do §4.1 da 3.5a, decidida pelo
  dono neste prompt); a medição real é do pin C33.
- **`/proc/self/maps`** — a proveniência lê o maps do processo (2
  leituras por import típicas graças ao cache; o Android expõe /proc
  às apps do próprio processo). Noutro sandbox sem /proc o dropRange
  vira NO-OP global (higiene fora, correção dentro — documentado no
  código).
- **A R-045 segue sem registo em docs/REGRESSOES.md** (o doc termina
  em R-044) — a dívida documental da 3.5a declarada na ABERTURA §4;
  o registo entra no FECHO com as provas do §2 do relatório 3.5a.

## §5 Ficheiros tocados

| Área | Ficheiros |
|---|---|
| assets | `assets/GmeshV3Stream.h` (o contrato do teto), `assets/GmeshV3Stream.cpp` (o clamp + o override de experimento + o mutex do callback + os 4 call sites do fileapi::dropRange) |
| platform | `platform/FileApi.h` + `platform/FileApi.cpp` (`rangeIsFileBacked` + `dropRange` — o ponto único com proveniência) |
| testes | `tests/test_fase35fix_p1.cpp` (NOVO, 6 pins), `tests/test_sentinels.cpp` (R-046), `tests/test_gmeshv3stream.cpp` (o contrato do teto), `tests/CMakeLists.txt` (o TU novo) |
| build | `app/build.gradle` (versionCode 72→73) |
| docs | `docs/RELATORIO-0.11-T-FIX-ABERTURA.md` (commit próprio), `docs/RELATORIO-0.11-T-FIX-PERNA1.md` (este), `docs/RELATORIO-0.9.6.md` (§14 vc 73) |
