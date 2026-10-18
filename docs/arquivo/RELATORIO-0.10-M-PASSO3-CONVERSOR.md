# RELATÓRIO 0.10-M · PASSO 3 — O CONVERSOR STREAMING E PARALELO

Fase: **0.10-M PASSO 3** (o BLOCO C do mandato do dono; o fecho do trio
`v3 + streaming + verificação`). O que o PASSO 3 manda: ler por
primitiva/range do ficheiro mmapado — **nunca o modelo inteiro na RAM**;
agrupar por material e cortar em blocos ≤ 65 535 vértices (índices locais
u16); pool de threads (núcleos−1) que ordena o disco e decide a saída no
assembly; **sem perda** (float32 exato da origem, sem soldadura, sem
simplificação); verificação **bit a bit** obrigatória no fim (a linha
`gmesh: v3 blocos=<n> verts=<v> tris=<t> verificado=1` no log); progresso
por fase com cancelamento sem bloquear a UI.

Este relatório fecha o PASSO 3 **após o crash de sessão** e cobre: o
diagnóstico na ordem que o dono mandou (a→d), os fixes (ficheiro+função),
a tabela de medições reais, as três mutações M1/M2/M3 (vermelho→verde) e
o que fica NÃO VERIFICADO. **O PASSO 4 segue BLOQUEADO à espera do OK
explícito do dono** (PÁRO ABSOLUTO — §9).

---

## §1 · O ESTADO NA RETOMA (o que sobreviveu ao crash)

A sessão anterior esgotou o contexto a meio do PASSO 3. O que o dono
reportou na retoma e que se **confirmou medido aqui**:

| afirmação do dono | verificação desta sessão |
|---|---|
| «código completo» | confirmado: o working tree tinha os 10 ficheiros do PASSO 3 (GmeshV3Stream.h/.cpp novos, AssetConverter/GOwnFormats/GltfImporter/medicoes editados, test_gmeshv3stream.cpp novo) não-commitados |
| «test_core verde» | reproduzido no arranque: `0 teste(s) com falha` |
| «medições 4/4 verificado=1» | reproduzido: os 4 perfis completam com `verificado=1` (§4) |
| «scene 228 MB → 55 MB RAM» | reproduzido EXATO: `scene-213MB ... RAM pico=55 MB` (§4) |
| «falham 11 e2e no c33 FASE 12.8/12.9» | reproduzido: `C33 VIRTUAL: 630 check(s), 11 falha(s)` — todas do fluxo de import glb (4 no 12.8, 4 no 12.8b, 3 no 12.9) |

---

## §2 · O DIAGNÓSTICO (a ordem do dono, cumprida)

### (a) O engine.log de uma das falhas — a mensagem exata

O log do c33 (`build/c33-virtual-logs/engine.log`) mostra DUAS causas
distintas — não uma:

```
10-08 10:41:57.724 I/GONI: asset: glTF 'par' (594 B) — parse ok: 1 nó(s), ...
10-08 10:41:57.724 I/GONI: asset: v3 streaming 'par' — 1 tarefa(s), 1 triângulo(s), ...
10-08 10:41:57.724 E/GONI: import: FALHOU — não consegui criar o ficheiro
      temporário '/fake/.staging/goni_v3stream_619.tmp' (errno 2)
10-08 10:41:57.728 W/GONI: bench: import FALHOU — não consegui criar o
      ficheiro temporário '/fake/.staging/goni_v3stream_619.tmp' (errno 2)
```

- **`par` (12.8b) e `benchref` (12.9)**: o parse streaming correu, o mmap
  da fonte correu — a morte foi no **TEMP STORE** (os blocos do corte
  appendam-se a um ficheiro temporário): `fopen("/fake/.staging/...")`
  → **errno 2 (ENOENT)**.
- **`robo` (12.8)**: morto EM SILÊNCIO — nem `parse ok` nem
  `import: FALHOU` no log. Causa do silêncio: o 12.8 chama
  `convert::importFile` DIRETAMENTE (síncrono, sem o job da UI que é quem
  loga `import: FALHOU` — main.cpp:2619) e consome o `err`. O repro
  autónomo (ver §2.4) imprimiu o err real: **`glTF sem meshes`**.

### (b) `mapFile64` content:// vs POSIX — a hipótese do dono, e o que era

**A fonte**: o `canStream` (AssetConverter.cpp:697) JÁ exige raiz POSIX
real — `content://` nunca entra no streaming (o caminho de sempre é a
degradação por desenho). O mmap da fonte PROVOU-SE no log (o `v3
streaming 'par'` passou depois do mmap).

**O gap real estava no TEMP STORE, não no mmap da fonte**: o novo código
fazia `st.makeDirs(".staging")` + `fopen(root + "/.staging/...")` — e no
harness o storage é o `FakeStorage` (raiz `/fake`): o `makeDirs` é
VIRTUAL (insere num `std::set` e devolve **true** — nunca toca no disco),
e o `fopen` no caminho POSIX literal morre ENOENT. A convenção da casa
(`stagingWrite`, 0.8.12 — AssetConverter.cpp:1574) SEMPRE teve a cascata
projeto → cache dir da app (`storage::jniCacheDir()`) → erro legível; o
temp store do streaming não a seguia.

**FIX (o temp store segue a cascata)** — GmeshV3Stream.cpp:
1. raiz POSIX real: `st.makeDirs(".staging")` + `fopen` (o fopen é o
   veredito — o makeDirs virtual "aceita" e não vale nada);
2. falhou → warn com o errno REAL e queda para o cache dir da app
   (`jniCacheDir()` + `/.staging` + `fileapi::makeDirs` recursivo);
3. falhou → erro LEGÍVEL com os dois caminhos tentados. `/tmp` NUNCA.

No harness a cascata cai no `c33-virtual-cache` (o `kCacheDir` do
c33_virtual.cpp:111, injetado pela ponte JNI fake) — o MESMO sítio que o
stagingWrite da reconversão já usava (visível no engine.log linha 34:
`staging em '/fake/.staging/goni_reconvert_619.tmp' FALHOU (errno=13 ...
) — a tentar o cache dir da app`).

**E a degradação honesta do mmap da fonte (a metade (b) que faltava)**:
se o `mapFile64` falhar por qualquer razão do SO, o import NÃO morre mais:
warn com o errno + re-parse no modo integral (o MESMO padrão do fallback
de pele acima) + o caminho de sempre (merge + writeGMesh). O modelo grande
morre aí com a causa REAL no erro; o pequeno importa. `streamed` passou a
não-const para o fallback desligar o caminho.

### (c) O offset do mmap alinhado a 4096 — VERIFICADO seguro por construção

`fileapi::mapFile64` (FileApi.cpp:562-566) alinha o offset **para baixo**
à página ANTES do `mmap` (`pageOff = offset/page*page`), mapeia
`len + within` e devolve `base + within` — QUALQUER offset 64-bit funciona
em arm64 (e em x86_64, onde offset não múltiplo da página = EINVAL). Todos
os call sites do streaming usam offset 0 (o GLB próprio / o irmão .bin) —
trivialmente alinhados. O teste NOVO `mmap64_offset_nao_alinhado_le_bytes_
exatos` (test_gmeshv3stream.cpp) prova a contrato com 4 sondas não
alinhadas (4097, 8191, 8192, 511 atravessando fronteira) — e é o tripwire
da mutação M2 (§6).

### (d) O pool de threads no harness — modo sequencial com fallback

O pool era `hw > 1 ? hw - 1 : 1` inline — o fallback JÁ existia, mas não
era observável nem mutável. FIX: extraído para função PURA exportada no
header (`v3StreamWorkerCount(u32)`, GmeshV3Stream.h) com o contrato
documentado: núcleos−1, **NUNCA 0** — `hardware_concurrency()==0`
(desconhecido) ou 1 (single-core) caem para 1 worker e o corte corre
SEQUENCIAL no próprio thread do chamador (mais lento, CORRETO), com log
honesto (`asset: v3 corte em MODO SEQUENCIAL (1 worker ...)`). Pins novos
em `v3stream_worker_count_nunca_zero` (0→1, 1→1, 2→1, 8→7, 16→15) — a
tripwire da mutação M3 (§6). Nenhum bloqueio do pool foi encontrado no
harness (as 11 falhas eram as causas acima).

### (e) O ACHADO PROFUNDO — o crash `free(): invalid pointer` do repro

Ao reproduzir o 12.8 com FakeStorage no test_core (o teste novo
`v3stream_c33_fakestorage_cascata_e_sem_nodes`), o import passou a correr
ATÉ AO FIM no storage virtual e **crashou** a sair:
`free(): invalid pointer`. ASAN (build separado, fora do repo) apontou o
`~vector<unsigned char>` no fim de `convertGltfToV3` libertando o ponteiro
EXATO de `vbytes.data()` — o readback da verificação virtual — e nunca um
overflow antes:

**Causa raiz**: o `verifySink` termina com a higiene de RSS
`dropRange(c->map, e.dataOffset, e.dataSize)` — `MADV_DONTNEED`. No
caminho mmap isso é legítimo (páginas de FICHEIRO: re-lêem do disco). No
caminho virtual `c->map` era o buffer do HEAP: `MADV_DONTNEED` em memória
anónima **zera páginas inteiras de 4K — com os vizinhos do heap incluídos**
(headers de chunk, strings vivas) — e os destrutores seguintes libertam
ponteiros cujos headers foram zerados.

**FIX**: `VerifyCtx.droppable` (false por defeito) — só o caminho mmap
dropa; o readback do heap NUNCA (documentado no struct). ASAN limpo
depois; o import virtual completo com `verificado=1`.

### (f) O ACHADO EM TORNO — o vazamento /tmp que matou a R-039

A 1.ª corrida da sentinela morreu com errno 28 (DISCO CHEIO): o `rmrf` do
wiring010 (a limpeza do filho de 500 MB) usava `::remove()` que **não
apaga subdiretórios com conteúdo** (`assets/`, `source/` do import
sobreviviam) — o root inteiro vazava em `/tmp` a cada corrida. FIX:
recursão real (lstat/S_ISDIR, a mesma do `rmRf` das medições) +
`#include <sys/stat.h>`. Isto é o mesmo gênero de lição w010 já registado;
o fix é deste commit.

### (g) O ACHADO FORA DE SCOPE (ASAN, registado no BACKLOG)

O ASAN global apanhou um bug PRÉ-EXISTENTE e alheio ao 0.10-M:
`test_toolbar.cpp:218` lê 4 bytes além de `kKeyboardPts`
(Icons.cpp:731, 192 B) — algum ícone declara `nPts` maior que o array
(data bug). Leitura de global adjacente, benigna em prática, ZERO efeito
nos testes normais — NÃO tocado neste commit (fora do scope do mandato);
decisão do dono no BACKLOG.

---

## §3 · OS FIXES (ficheiro + função)

| # | ficheiro · função | o que mudou |
|---|---|---|
| F1 | GltfImporter.cpp · parseGltf (bloco scene/nodes) | as refs de primitiva do modo streaming COLHEM-SE SEMPRE (fora do guard `if (jnodes)`); sem nodes — ou nenhum nó a referenciar mesh — as refs ficam com `node=-1` = IDENTIDADE, o MESMO resultado do caminho cru legado («sem nós com mesh: o caminho antigo, mesh a mesh» — AssetConverter.cpp:947); a expansão vazia já não apaga as refs; o `c.node` deixou de ter o `* 0` morto |
| F2 | GmeshV3Stream.cpp · convertGltfToV3 (o TEMP) | a CASCATA do stagingWrite: projeto `.staging/` (fopen = veredito) → cache dir da app (jniCacheDir + makeDirs recursivo) → erro legível com os caminhos tentados; cada falha LOGA a causa; /tmp NUNCA |
| F3 | GmeshV3Stream.cpp · VerifyCtx + verifySink | `droppable` (false por defeito): o MADV_DONTNEED da higiene de RSS só corre no caminho mmap — o readback do heap da verificação virtual NUNCA (o crash §2e) |
| F4 | GmeshV3Stream.cpp · convertGltfToV3 (a VERIFICAÇÃO) | o readback do final: caminho POSIX real → mmap (zero RAM, como era); storage virtual → `st.readBytes` (o caminho do runtime) com warn honesto — a comparação bit a bit é a MESMA |
| F5 | GmeshV3Stream.h/.cpp · v3StreamWorkerCount | função PURA exportada (núcleos−1, NUNCA 0) + o log do MODO SEQUENCIAL; os pins (§2d) |
| F6 | AssetConverter.cpp · convertGltfCommon | `streamed` não-const + a DEGRADAÇÃO do mmap: falha do mapFile64 → warn com errno + re-parse integral + caminho de sempre (a ordem (b) do dono) |
| F7 | tests/test_wiring010.cpp · rmrf | recursão REAL (lstat/S_ISDIR) — o vazamento /tmp (§2f) + o include sys/stat.h |
| F8 | tests/test_gmeshv3stream.cpp | `v3stream_c33_fakestorage_cascata_e_sem_nodes` (a regressão do C33: FakeStorage /fake + GLB sem nodes + cascata + verificado=1 + o gmesh abre pelo leitor de produção com 1 triângulo) + `v3stream_worker_count_nunca_zero` + `mmap64_offset_nao_alinhado_le_bytes_exatos` |

---

## §4 · A TABELA DE MEDIÇÕES (medido, não estimado — Release, sandbox x86_64)

Os 4 perfis do dono (`medicoes_010m_perfis_do_dono_por_fase` — filho fork,
VmHWM reiniciado com clear_refs 5; cópia=0/parse≈0 porque o gerador e o
import partilham o /tmp local — as fases de rede do device não existem
aqui) + a sentinela R-039:

| perfil | fonte | gen | corte | assembly | total | RAM pico | blocos | verts (pool) | idx | verificado |
|---|---|---|---|---|---|---|---|---|---|---|
| dragao-38MB | 38,0 MB | 97 ms | 968 ms | 715 ms | 2775 ms | **75 MB** | — | 7 110 000 | 7 110 000 | **1** |
| buddha-classico | 81,4 MB | 348 ms | 408 ms | 266 ms | 1300 ms | **82 MB** | — | 543 652 | 32 010 000 | **1** |
| scene-213MB | 227,8 MB | 1009 ms | 4782 ms | 2990 ms | 13 284 ms | **55 MB** | — | 30 000 000 | 30 000 000 | **1** |
| dragao-fit | 2,9 MB | 12 ms | 18 ms | 8 ms | 48 ms | **39 MB** | — | 65 535 | 390 000 | **1** |
| **R-039 sentinela** | **1754 MB** | 4653 ms | — | — | **31 316 ms** | **107 MB** | **920** | 60 000 000 | 60 000 000 | **1** |

Leituras que o dono deve bater com a promessa:

- **A parede do 65 535 MORREU**: o dragão real (300 001 verts) que morria
  no parse no PASSO 1 agora converte em 2,8 s; a scene (228 MB, 80
  meshes, 30 M verts) em 13,3 s.
- **O pico de RAM é do algoritmo, não do ficheiro**: a scene de 228 MB
  converte com 55 MB; a sentinela de **1754 MB** com **107 MB** — o pico
  NÃO cresce com o ficheiro (o readAll daria ≥1,75 GB).
- **verificado=1 é a linha contrato**: a verificação POR TRIÂNGULO bit a
  bit correu sobre TODOS — incl. o ficheiro de 1,7 GB (920 blocos).
- Os `blocos` dos 4 perfis não constam no print deles (a linha
  `gmesh: v3 blocos=` existe no engine.log de cada filho, apagado no fim
  — a limpeza); a sentinela imprime o dela. Não é estimado: é o que o
  print prova.

---

## §5 · AS SUÍTES E OS GATES (o estado no fim)

| suíte / gate | resultado |
|---|---|
| test_core | **0 teste(s) com falha** (incl. os 8 v3stream + os 3 novos deste commit + os 4 perfis + a sentinela R-039) |
| c33_virtual | **631 check(s), 0 falha(s) — HARNESS VERDE** (as 11 do 12.8/12.8b/12.9 FECHADAS) |
| release-identity (R-015) | VERDE: versionName 0.9.6 · **versionCode 56** · o par declarado no RELATORIO-0.9.6 §14 |
| build ASAN (fora do repo) | os testes c33/v3stream sem bad-free nem overflow próprios (o achado §2g é pré-existente e alheio) |
| docs-lint / gmesh-docs | a correr no push (o gate do CI) |

---

## §6 · AS MUTAÇÕES (vermelho → verde, coladas)

Protocolo da casa: backup por cp → mutação textual → rebuild → o teste
ALVO fica VERMELHO → reposição por cp → rebuild → VERDE. Nenhum commit
vermelho foi empurrado.

### M1 — o readAll reposto no caminho streaming

Mutação: `convertGltfCommon` troca o `fileapi::mapFile64` da fonte por
`fileapi::readAll` do ficheiro INTEIRO (o caminho antigo).

```
[VERMELHO] sentinela_r039_50m_verts_ram_pico_512mb ...
  [r039] fonte=1754 MB (gen 4572 ms) | total=18671 ms |
  RAM pico=1787 MB (base 30) | verts=0 idx=0 | ok=0 verificado=0
  → FALHOU (o pico 1787 MB > orçamento 512 MB — ×3,5)
[VERDE após reposição]
  [r039] fonte=1754 MB (gen 4684 ms) | total=32174 ms |
  RAM pico=107 MB (base 30) | blocos=920 | ok=1 verificado=1
  → 0 teste(s) com falha
```

### M2 — o alinhamento da página morto no mapFile64

Mutação: `pageOff = offset` (o offset cru vai ao mmap — EINVAL em
qualquer offset não alinhado).

```
[VERMELHO ×2]
gmeshv3_mmap_de_64_bits_le_o_ficheiro       FALHOU (test_gmeshv3.cpp:386 p2 != nullptr)
mmap64_offset_nao_alinhado_le_bytes_exatos  FALHOU (test_gmeshv3stream.cpp:809 p != nullptr)
[VERDE após reposição] 0 teste(s) com falha
```

### M3 — o pool a 0 sem fallback

Mutação: `v3StreamWorkerCount`: `: 1u` → `: 0u` (hosts single-core/desconhecidos
ficam SEM workers).

```
[VERMELHO ×2]
v3stream_worker_count_nunca_zero FALHOU (test_gmeshv3stream.cpp:772 v3StreamWorkerCount(0) == 1)
                                 FALHOU (test_gmeshv3stream.cpp:773 v3StreamWorkerCount(1) == 1)
[VERDE após reposição] 0 teste(s) com falha (+ c33 631/631)
```

---

## §7 · A NOTA DO CI — a escotilha do job separado

O dono autorizou job separado «se exceder budget de tempo». MEDIDO: a
sentinela R-039 acrescenta **~37 s** (gen 4,7 s + import 31,3 s) ao job
core-tests, que não tem timeout explícito (o default do GitHub é 6 h) —
o disco do runner (≥14 GB livres) comporta o pico de ~5,2 GB da sentinela
(fonte 1,75 GB + temporário 1,7 GB + final 1,7 GB, todos removidos pelo
filho). **DECISÃO: fica no MESMO job**; o job separado fica documentado
aqui como escotilha — se o core-tests crescer além do razoável, mover o
TEST para um job próprio é uma mudança de workflow só, sem tocar no teste.

---

## §8 · NÃO VERIFICADO (honesto)

- **O device real (C33)**: todo o PASSO 3 correu na sandbox (x86_64,
  Release). O caminho de produção no device — o FsStorage de
  all-files real, o SAF (content://) que cai no caminho de sempre, o
  thermal do PowerVR GE8320, o LMK do Android 13 sob pressão — NÃO foi
  tocado. O APK do versionCode 56 é o artefacto para o dono provar no
  device.
- **Offsets >4 GB em hardware real**: o formato e o mapFile64 provam
  contagens/offsets 64-bit em ficheiros sintéticos (<1 KB com metadados
  de 5e9 — PASSO 2); a sentinela desta fase fica em 1,75 GB. Um GLB real
  >4 GB no device fica por provar.
- **As texturas**: fora do PASSO 3 (PASSO 5 do mandato) — o import
  converte a geometria; o passe de texturas é o de sempre.
- **O OBJ sem teto**: o OBJ ≤65 535 segue o caminho vigente; o OBJ
  gigante em streaming é o PASSO 3-b no BACKLOG (os 3 modelos do dono
  são GLB — decisão da fronteira de sessão, mantida).
- **ACI (ASTC/ETC2) e o resto do pipeline de exibição**: sem mudanças.
- **O CI deste commit**: o push está na fila — o «CI verde» desta fase é
  a suíte local + os gates locais; a confirmação do run vai à API (o
  padrão da casa) e fica registada no worklog.

---

## §9 · PÁRO ABSOLUTO

O mandato manda: «EXECUTA AGORA OS PASSOS 1, 2 e 3. PÁRO ABSOLUTO antes
do PASSO 4». Os PASSOS 1 (db9c28b), 2 (e89e100) e 3 (este commit) estão
fechados com commit próprio, suítes verdes e relatório. **O PASSO 4
(render por blocos), o PASSO 5 (texturas) e o PASSO 6 (a prova final)
ficam BLOQUEADOS à espera do OK explícito do dono.**
