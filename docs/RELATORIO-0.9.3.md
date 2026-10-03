# RELATÓRIO 0.9.3 — HOTFIX: OS DOIS CRASHES DO DEVICE (REG-001/REG-002 = R-005/R-006)

> **EVIDÊNCIA PRINCIPAL**: CI run **37143431795** (commit `a099dc3`, branch
> main) — 4/4 jobs verdes, **APK assinado** `goni-vv-0.9.3-release-signed`
> publicado como artifact. Mutações: runs **37142037670** (R-006, VERMELHO
> pela sentinela) e **37142039349** (R-005, VERMELHO pelo check estrutural).

---

## 1. OBJETIVO

Corrigir os dois crashes críticos reportados com evidência forense:
**REG-001/R-005** — `ConcurrentModificationException` no
`ProjectManagerActivity.reload()` (23 ocorrências num dia, process
`vv.goni`, crash loop ao abrir) e **REG-002/R-006** — `SIGSEGV`
(`SEGV_ACCERR`) em `AAudio_createStreamBuilder` no chipset Unisoc do
realme RMX3624 (tombstones 00-17/21-31 da app antiga `com.goni.runtime`),
com **migração para Oboe** como caminho primário + fix robusto em AAudio
como fallback, sob loop de auto-verificação, dispositivo virtual em CI,
sentinelas permanentes e prova de mutação. **CLÁUSULA CALMA respeitada**:
só os 2 crashes + testes + sentinelas; zero features, zero UI novo.

## 2. FONTE DE VERDADE

- O prompt do dono (stack traces exatos, contagens, exigências T1-T8).
- O código real do repo (auditoria linha a linha pré-fix): a causa do
  REG-001 está em `ProjectManagerActivity.java` **linhas 348-371 da
  0.9.2** — `reload()` fazia `all.clear()+all.addAll(VvProjects.load())`
  na MAIN thread (chamado do onCreate L169 + onResume L376 +
  pós-operações L654/714/841/897/913) enquanto a lambda do
  `io.execute` (L354-355, single-thread executor = `pool-2-thread-1`
  do stack trace) percorria a MESMA `ArrayList` — o for-each lançava a
  exceção e o processo morria. A linha 355 da 0.9.2 é exatamente o
  `lambda$reload$4` do stack.
- O REG-002 é da app ANTIGA (`com.goni.runtime`/`libgoni.so`/
  `eng::editor::EditorHost`) — este repo (`vv.goni`/`libgoni_vv.so`) já
  tinha o arranque fora do onResume (INIT_WINDOW), mas os backends
  0.8.11 tinham as PORTAS ABERTAS para a MESMA classe de bug:
  `AAudioBackend::start()` sem guarda atómica (2ª chamada = overwrite de
  `stream_` = **fuga do 1º stream**), `AudioTrackBackend::start()` sem
  guarda (2ª thread de escrita + track vazado), `errCb` sem fechar o
  stream morto, `pause()/resume()` sem verificar retorno.

## 3. IMPLEMENTAÇÃO (Tarefas 1-4)

**Tarefa 1 — REG-001** (`ReloadGate.java` novo + `ProjectManagerActivity.java`):
portão single-flight (`AtomicBoolean compareAndSet`) com **coalesce
trailing** (1 reload de cada vez; o ÚLTIMO pedido é sempre publicado;
`abandon()`/`safeRun()` nunca deixam o portão preso); `doReload()` corre
na thread io e constrói lista **NOVA local** (`fresh`) — NUNCA toca em
`all`/`shown`/`missingUris`; publicação em DUAS fases na main thread
(`main.post`): lista primeiro (mesmo timing de UX de sempre), estado
em-falta depois; **try/catch POR ENTRADA** (projeto corrompido/
inacessível = saltado com `Log.w` + contadores no `Log.i` final);
`VvProjects.load` (defensivo desde 0.7.x: corrompido → lista vazia) passa
a correr FORA da main thread.

**Tarefa 2 — REG-002, MIGRAÇÃO PARA OBOE** (o caminho primário):
`platform/OboeBackend.cpp` (TU COMUM — o padrão StorageBridge/jni.h da
casa: **oboe REAL no APK, stub `tests/stub/oboe/Oboe.h` na suíte** — o
mesmo código de produção vigiado pelo CI nos dois lados). Oboe
**1.9.3 PINADA** via CMake FetchContent (`app/src/main/cpp/CMakeLists.txt`,
`if(ANDROID)`, `GIT_SHALLOW`); LICENÇA **Apache-2.0** (biblioteca oficial
do Google). AAudio na API 27+ com fallback **OpenSL ES automático** nos
devices problemáticos (a razão de ser do oboe); `oboe::Result` verificado
em TODAS as chamadas; callback completo: `onAudioReady` SEM locks/alocações
(só puxa o misturador), `onErrorBeforeClose` loga, `onErrorAfterClose`
larga a referência + abre o portão; log informativo do stream escolhido
(`rate/ch/burst/perf/api`); `platform/AudioStartGate.h` — portão
`std::atomic<bool>` em TODOS os backends (oboe, aaudio, audiotrack,
stub): `start()` é **idempotente** (2× NÃO abre 2º stream — a fuga era a
porta do SIGSEGV); cadeia de boot `audioBackendBoot()`:
**Oboe → AAudio → AudioTrack → SEM SOM** (o editor NUNCA crasha por
causa do áudio); arranque no INIT_WINDOW, nunca no onResume; o probe
(Settings → Diagnóstico) corre contra o Oboe.

**Tarefa 3 — AAudio fixado como FALLBACK** (`AudioOutDevice.cpp`):
`stream_` protegido por `std::mutex` (a corrida errCb × stop — quem tira
o stream do slot primeiro fecha, o outro vê vazio: nunca duplo close,
nunca use-after-free); `errCb` **FECHA o stream morto** (o padrão que o
próprio oboe segue internamente) e abre o portão; TODOS os
`aaudio_result_t` verificados e logados (requestStop/close/requestPause/
requestStart); AudioTrack também com portão (o arranque duplo criava 2ª
thread de escrita + track vazado).

**Tarefa 4 — Problema 3 (memória)**: load da lista + queries SAF fora da
main thread (a fonte da pressão do arranque); memória logada no arranque
(Java `Debug.getMemoryInfo` no Gestor e no editor; nativo RSS+pico do
`/proc/self/status` no fim do boot — `logBootMemory`); tetos mantidos e
afervados (THUMBS `LruCache(24)`; prim mesh cache REMOVIDO por decisão
0.8.10; TextureCache é disco). O crash loop do REG-001 era em si o maior
motor de pressão (cada crash = restart = novo arranque).

## 4. FIXES REAIS (apanhados pelo loop — tabela de iterações na secção 7)

1. Off-by-one no check de fugas da FASE 8 (o stream VIVO do último boot
   é legítimo — invariante corrigido para `fechados == abertos - 1`).
2. Mutação 2 não-compilável produziu FALSO VERDE (o javac falhou e o
   teste correu contra classes antigas) → sentinela JVM reforçada com o
   **contrato determinístico do portão** (zero timing).
3. YAML do workflow com linha sem `#` (o run morreu antes de qualquer
   job) → validação local `yaml.safe_load` antes do push.
4. `getXRunCount()` do oboe REAL devolve `ResultWithValue<int32_t>`
   (não `int32_t`) — apanhado pelo build NDK do CI.
5. O acessor do `ResultWithValue` REAL é `.value()` (não `.result()`) —
   apanhado pelo build NDK; **todos os acessores re-verificados 1:1
   contra os headers do tag 1.9.3** do repositório google/oboe.
6. `totalPss` é MÉTODO (`getTotalPss()`), não campo — apanhado pelo
   javac do APK; nasceu o `scripts-local/check_java.sh` (a app Java
   TODA compila contra o android.jar 34 real antes do push).
7. 4× falhas de INFRA do GitHub runner ("shutdown signal", exit 143) —
   distinguidas de falhas reais pelo log e re-executadas.

## 5. TESTES (suíte 743 → 744 core; JVM +3+adversários; harness 115 → 147)

- **JVM** (`tests-java/ReloadGateTest.java`, corre com `java -Xmx48m`):
  `regress_concurrent_reload` (contrato determinístico do portão +
  10 threads + zero exceções + sem sobreposição de publicações +
  estado final correto + **o MECANISMO do crash original reproduzido e
  apanhado**), `regress_corrupt_projects` (4/10 corrompidos → 6 bons
  publicados + contadores; todos corrompidos → lista vazia; portão
  sobrevive), `adversarios` (100 threads; heap ~83% do teto; worker que
  lança não deixa o portão preso).
- **C++** (`tests/test_sentinels.cpp`): `regress_audio_lifecycle` — o
  OboeBackend de PRODUÇÃO contra o stub: arranque duplo (openCount==1),
  50× start, pause/resume ×10, erro→degradação (ready cai, portão abre),
  **zero fugas** (`closeCount==openCount`), falha de openStream/
  requestStart = `false` + portão aberto, probe 5+3 ciclos verde.
- **Core**: 744 casos OK (681→743 na 0.9.2; +1 = a sentinela nova).
- **c33_virtual**: 115 → **147 checks** (FASE 8 nova, abaixo).

## 6. HARNESS — O DISPOSITIVO VIRTUAL EM CI (d)

O executável `c33_virtual` (CMake/ctest `c33_virtual_replay`, job
`c33-virtual`) já reproduzia o C33/RMX3624: 1536×720 + insets, **/tmp
READ-ONLY (errno=30)**, cache dir via JNI fake, content:// SAF, lifecycle
EGL TERM/INIT, ASTC, taps pelo `frame()` REAL (inclui o main.cpp REAL).
A **FASE 8 nova** (replay do REG-002 + Sequências 2/3 do prompt):

```
== FASE 8 — replay REG-002: lifecycle agressivo do audio + corruptos ==
  > 8.1 boot real: INIT_WINDOW arranca o backend OBOE (o primario)
  > 8.2 a sequencia do tombstone: startAudio repetido sem guarda
  > 8.3 adversario: 50 onResume sem onPause + 10 startAudio
  > 8.4 ciclo duro TERM->INIT x3 (o Android mata/recria a surface)
    [streams] abertos=4 fechados=3 vivos=1
  > 8.5 disconnect no meio da sessao (o headset desligou)
  > 8.6 cena corrompida no disco: boot completa sem crash
  > 8.7/8.8 memoria do arranque + gate interno de assinaturas
    [streams] fim da fase: abertos=6 fechados=6 vivos=0
== C33 VIRTUAL: 147 check(s), 0 falha(s) ==
HARNESS VERDE — o dispositivo virtual confirma os fixes vigiados (R-001..R-006; REG-001/REG-002 do hotfix 0.9.3)
```

O backend que o replay usa é o **OboeBackend de produção** contra o stub
do oboe (o MESMO TU que o APK compila contra o oboe real). A Sequência 1
(3 reloads em 500ms → zero exceções) corre na SUÍTE JVM (o replay C++
não instancia Java) dentro do `regress_concurrent_reload` (10/100
threads são MAIS agressivos que 3 em 500ms). O output do CI (colado):
`147 check(s), 0 falha(s)` + `ReloadGateTest: OK` + `GATE VERDE: zero
padrões proibidos` + `GATE VERDE: zero assinaturas de crash` +
`GATE VERDE: nenhum literal /tmp`.

## 7. ITERAÇÕES DO LOOP (a)

| # | O que correu | O que apanhou | Ação |
|---|---|---|---|
| 1 | build + ctest local | FASE 8 RED: check de fugas com off-by-one (o stream vivo do último boot) | Invariante corrigido (`fechados==abertos-1` no meio, `==` no fim) + contadores no output |
| 2 | Suíte completa local | NADA — 744 + 147 + gates verdes | Ronda de mutação |
| 3 | Mutação 1 (Activity revertida) | Check estrutural RED (5 falhas) ✓ | Restaurado → verde |
| 4 | Mutação 2 (portão partido) | FALSO VERDE — a mutação não compilou e o teste correu contra classes velhas | **Sentinela reforçada** com o contrato determinístico; mutação refeita compilável → 6 falhas ✓ |
| 5 | Mutação 3 (StartGate removido) | Sentinela RED (5) + harness RED (abertos=17 fechados=3 **vivos=14**) + **SIGSEGV REAL exit 139** no dispositivo virtual | Restaurado → verde |
| 6 | Push main → CI | YAML quebrado (linha sem `#`) — run morreu antes dos jobs | Fix + validação local de YAML |
| 7 | CI | `getXRunCount` → `ResultWithValue<int32_t>` no oboe real | Fix + stub atualizado |
| 8 | CI | Acessor `.value()` (não `.result()`) | Fix + verificação 1:1 contra os headers 1.9.3 |
| 9 | CI | `totalPss` é método | Fix + `scripts-local/check_java.sh` (paridade android.jar) |
| 10 | CI | 4× infra runner (shutdown signal) | Re-runs (distinto de falha real pelo log) |
| 11 | CI final | **4/4 jobs verdes; APK assinado publicado** | Fecho |

## 8. SENTINELAS (permanentes — docs/REGRESSOES.md R-005/R-006)

- `regress_concurrent_reload` (JVM) — 10 threads, zero exceções, contrato
  do portão, mecanismo do crash apanhado dentro do teste;
- `regress_corrupt_projects` (JVM) — entradas corrompidas saltadas com
  contadores, reload completa;
- `regress_audio_lifecycle` (C++) — arranque duplo/erro/fugas/probe;
- `scripts/reload_concurrency_check.py` — o fonte REAL da Activity
  (confinamento io, publicação main.post, try/catch por entrada, portão,
  contadores, tetos, memória) — **apanha a reversão do fix no fonte**;
- FASE 8 do c33_virtual — o caminho REAL do main.cpp;
- **Gates de CI**: `ci/forbidden_log_patterns.txt` (0.8.12, -F) +
  `ci/forbidden_patterns.txt` (NOVO, -E):
  `ConcurrentModificationException` | `SIGSEGV.*AAudio` |
  `AAudio_createStreamBuilder\+160` | `FinalizerWatchdogDaemon` —
  grep no output COMBINADO (sentinelas JVM + replay + engine.log);
  **qualquer match = CI VERMELHO = release bloqueada** (o job do APK tem
  `needs: [core-tests, c33-virtual]`). As mensagens dos checks NUNCA
  citam os padrões (a lição 0.8.12-c do falso positivo).

## 9. COMMITS (main, ordem)

- `2989a01` 0.9.3-a: o hotfix completo (fix + sentinelas + FASE 8 + gates)
- `c1aa8fb` 0.9.3-b: fix YAML do workflow
- `90e04e0` 0.9.3-c: getXRunCount → ResultWithValue
- `19da567` 0.9.3-d: acessor .value() + verificação 1:1 dos headers
- `a099dc3` 0.9.3-e: getTotalPss() + check_java.sh local
- Branches de EVIDÊNCIA (não fazer merge): `proof/mutacao-R006` (3957863)
  e `proof/mutacao-R005` (193fc31) — vermelhos pela razão certa.

## 10. CI / APK

Run **37143431795** (main @ a099dc3): `Testes do core` ✓ ·
`C33 virtual` ✓ · `APK release arm64` ✓ · `verify-entry-symbols` ✓.
Evidência do APK: `VV_SIGNING: 1` (secrets presentes), `apksigner
verify --print-certs` → `Signer #1 certificate DN: CN=G.One VV, OU=VV,
O=G.One VV, L=Luanda, ST=Luanda, C=AO` (SHA-256
`44f39bedec9e010bd7bb0b04d62a20a959530c7469a844ca635f5c49825a7f3d`);
artifact **`goni-vv-0.9.3-release-signed`** (1.519.252 bytes) +
`c33-virtual-replay-log` publicados. O `build_info.txt` (identidade da
build) embutido no APK; o gate de símbolos confirmou `runProbe` e todos
os símbolos de áudio no `.dynsym` da `libgoni_vv.so` real (o oboe liga
estaticamente — compilado do FetchContent `_deps/oboe-src`).

## 11. NÃO VERIFICADO (honesto — como verificar no device)

1. **Arranque real do oboe no RMX3624** (o stub espelha a API, mas o
   hardware é que decide): instalar o APK assinado, abrir o editor e ver
   o logcat `audio(oboe): stream ATIVO rate=… ch=… api=…` — se falhar,
   tem de aparecer `audio: Oboe recusou — fallback AAudio direto` e o
   editor segue SEM SOM (nunca crash). [Checklist item 3]
2. **Probe no hardware Unisoc**: Settings → Diagnóstico → probe — a
   tabela com `DECISAO` tem de sair; se o oboe estiver instável o
   AudioTrack entra sozinho. [Checklist item 7]
3. **Zero tombstones novos** em `/data/tombstones/` após a sessão de
   teste (só o dono pode ver — `adb shell ls /data/tombstones/`).
4. **A permissão de microfone/RECORD_AUDIO** não mudou (já 0.8.11) —
   mas o gravador continua a ser o caminho de áudio de entrada; não
   tocado (CLÁUSULA CALMA).
5. **10 aberturas seguidas da app** no telefone (o Sentinela JVM prova o
   mecanismo com 100 threads; o telefone real é a aceitação final).
   [Checklist item 1]
6. `FinalizerWatchdogDaemon` no logcat do device após o fix (o padrão
   está no gate de CI para QUALQUER output futuro; no device, o dono
   confirma a ausência — a causa principal, o crash loop + main
   bloqueada, foi removida com evidência local).

## 12. DECLARAÇÃO DE AUSÊNCIA DE ACHISMO (j)

Nenhuma frase de hedging ("acho", "talvez", "provavelmente", "deve
funcionar", "should work", "maybe", "I think") aparece neste relatório,
nos commits ou nos comentários do código. Cada afirmação cita: nome do
teste + linha de output, gate, run de CI, ou contadores do harness. As
afirmações não verificáveis estão na secção 11 com o passo de verificação
no device.

## 13. TABELA DE RASTREABILIDADE (c)

| Stack trace exato (prompt) | Causa raiz (ficheiro/função) | Fix | Teste que reproduz | Replay no harness | Sentinela permanente |
|---|---|---|---|---|---|
| `ConcurrentModificationException … ArrayList$Itr.next … vv.goni.ProjectManagerActivity.lambda$reload$4(ProjectManagerActivity.java:355)` em `pool-2-thread-1` | `ProjectManagerActivity.reload()` (0.9.2 L348-371): `all.clear()+addAll` na MAIN (onCreate+onResume) × for-each da MESMA lista na thread io | `ReloadGate` + `doReload()` (lista local `fresh`; publicação `main.post`; try/catch por entrada; load fora da main) | `regress_concurrent_reload` (JVM): o mecanismo antigo lança ≤200 tentativas; o novo publica com ZERO exceções | A Sequência 1 corre na suíte JVM do job c33-virtual (10/100 threads > 3 em 500ms) — `ReloadGateTest: OK` no run 37143431795 | `regress_concurrent_reload` + `regress_corrupt_projects` (JVM, todas as runs) + `scripts/reload_concurrency_check.py` (fonte da Activity) + gate `ConcurrentModificationException` |
| `SIGSEGV (SEGV_ACCERR) … libaaudio.so (AAudio_createStreamBuilder+160) … EditorHost::startAudio()+192 … EditorActivity.onResume+30` | Classe: arranque repetido sem guarda → fuga de stream vivo → driver Unisoc crasha dentro da própria createStreamBuilder; + builder/stream usados após morte; + retornos não verificados (as portas estavam abertas nos backends 0.8.11) | Migração OBOE (OboeBackend.cpp, 1.9.3 pinada) + `AudioStartGate` em TODOS os backends (start idempotente) + AAudio fixado (mutex, close-no-errCb, resultados) + cadeia boot com degradação sem som + arranque no INIT_WINDOW | `regress_audio_lifecycle` (C++): arranque duplo → openCount==1; erro → degrada | FASE 8 (8.2 é a sequência exata do tombstone: startAudio 3× → 1 stream; 8.3: 50 onResume; 8.4: TERM→INIT ×3 → abertos/fechados/vivos; 8.5: disconnect → frame sem som) | `regress_audio_lifecycle` (core, todas as runs) + FASE 8 do c33_virtual + gates `SIGSEGV.*AAudio` e `AAudio_createStreamBuilder\+160` |
| `SIGABRT FinalizerWatchdogDaemon (SuspendThreadByPeer timed out)` | Processo pressionado/bloqueado (crash loop do REG-001 + I/O na main do arranque) | Load/SAF fora da main; memória logada; tetos mantidos | `adversarios` (JVM, heap 83%) + FASE 8.6/8.7 | `boot: memoria (fim do boot) — RSS=…` no engine.log do replay | Gate `FinalizerWatchdogDaemon` (qualquer output futuro) + checklist device |

## 14. PROVA DE MUTAÇÃO (e) — vermelho → verde, colado

**(M1) Fix da Activity REVERTIDO** (branch `proof/mutacao-R005`, run CI
**37142039349**, job core-tests, o MESMO output do local):

```
  FALHOU  R-005.1: reload() tem de delegar no ReloadGate.request()
  FALHOU  R-005.1: reload() mexe na lista da UI direto (all.clear()) — só o portão + doReload
  FALHOU  R-005.1: reload() mexe na lista da UI direto (all.addAll) — só o portão + doReload
  FALHOU  R-005.1: reload() mexe na lista da UI direto (missingUris.clear()) — só o portão + doReload
  FALHOU  doReload() não encontrado (o esqueleto do reload)
reload_concurrency_check: VERMELHO — o fix R-005 foi enfraquecido (ver acima)
```
Reposto (main): `reload_concurrency_check: OK — o fix R-005 está intacto`.

**(M2) Portão ReloadGate PARTIDO** (mutação local `if (true || running.compareAndSet…)`):

```
  FALHOU  contrato: o 2º pedido NAO entra (portao fechado)
  FALHOU  contrato: o 3º pedido idem (coalesce)
  FALHOU  contrato: o trailing ficou AGENDADO
  FALHOU  contrato: o portao segue FECHADO (1 de cada vez)
  FALHOU  contrato: o finished() acordou o trailing (worker 2x — nao 3x…)
  FALHOU  contrato: o worker correu 3x no total
ReloadGateTest: 6 falha(s)
```
Reposto: `ReloadGateTest: OK` (e no CI do main: `ReloadGateTest: OK`).

**(M3) StartGate do Oboe REMOVIDO** (branch `proof/mutacao-R006`, run CI
**37142037670** + local — a sentinela CI com as 5 falhas exatas):

```
regress_audio_lifecycle ...
  FALHOU  tests/test_sentinels.cpp:534  oboe::testing::hooks().openCount == abertosAteAgora
  FALHOU  tests/test_sentinels.cpp:536  logHas("audio(oboe): start ignorado — stream ja ativo")
  FALHOU  tests/test_sentinels.cpp:544  oboe::testing::hooks().openCount == abertosAteAgora
  FALHOU  tests/test_sentinels.cpp:553  oboe::testing::hooks().openCount == abertosAteAgora
  FALHOU  tests/test_sentinels.cpp:559  oboe::testing::hooks().openCount == abertosAteAgora + 1
regress_audio_lifecycle                        FALHOU
```
E o harness local com a porta removida (a fuga em números + o crash):
```
    [FAIL] o 2º/3º start NAO abrem streams (porta R-006 — a fuga de stream era a porta do crash Unisoc)
    [FAIL] ainda EXATAMENTE 1 stream (zero fugas no adversario)
    [streams] abertos=17 fechados=3 vivos=14
    [FAIL] ZERO fugas: cada stream aberto foi fechado (…)
/bin/bash: line 1:  7104 Segmentation fault      ./build-tests/c33_virtual   ← exit 139 = SIGSEGV REAL
```
Reposto: `147 check(s), 0 falha(s)` + `[streams] fim da fase: abertos=6 fechados=6 vivos=0`.

## 15. DECISÃO OBOE vs AAUDIO (i) + GATES (h)

**Oboe FOI adotado** (primário, 1.9.3, Apache-2.0, FetchContent — o
prefab/Maven é a alternativa documentada da Tarefa 2.1; FetchContent
escolhido porque a pinagem vive num só sítio, funciona com o
NDK+CMake do SDK já usados, e a suíte host testa o MESMO TU contra o
stub — o padrão da casa). O AAudio direto PERMANECE como fallback
FIXADO (Tarefa 3 cumprida na íntegra: portão, mutex, close-no-errCb,
todos os `aaudio_result_t` verificados) e o AudioTrack como fallback
final. Resultado dos gates no run verde: `GATE VERDE: zero padrões
proibidos no replay do C33 virtual` · `GATE VERDE: zero assinaturas de
crash (R-005/R-006/P3) no output combinado` · `GATE VERDE: nenhum
literal /tmp em FileApi/assets/migração`.

## 16. VEREDITO

Loop fechado com **suíte 100% verde (744 core + JVM + 147 checks do
dispositivo virtual), adversários verdes, gates verdes, prova de
mutação vermelho→verde colada (local + CI), APK assinado publicado pelo
CI**. O que o CI não prova (hardware real Unisoc) está na secção 11 com
o passo exato de verificação — o dono preenche a checklist do README
(8 itens) e o VERIFIED fecha a release.
