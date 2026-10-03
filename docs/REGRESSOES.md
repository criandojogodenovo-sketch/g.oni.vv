# docs/REGRESSOES.md — REGISTO DE REGRESSÕES DE DEVICE (0.8.12; hotfix 0.9.3)

O contrato desta casa: **CI verde não é prova** — entregas verdes que
falham no telefone repetidamente são inaceitáveis (0.8.5/0.8.7/0.8.9/
0.8.10). Este ficheiro é o REGISTO PERMANENTE dos bugs de device fixados:
cada entrada liga o SINTOMA exato (a linha de log que o dono viu) à causa
raiz (ficheiro/função), ao fix, ao teste sentinela que o vigia para sempre
e à linha do REPLAY do dispositivo virtual (executável `c33_virtual` em
CI) que o confirma limpo.

## POLÍTICA (a regra da casa)

1. **Todo fix de bug de device acrescenta uma entrada neste ficheiro + um
   teste sentinela** (caso `regress_*` no `tests/test_sentinels.cpp`, e
   quando o caminho é do main.cpp, um passo no replay do `c33_virtual`) +
   o padrão proibido correspondente em `ci/forbidden_log_patterns.txt`.
2. **Remover ou saltar uma sentinela exige aprovação explícita do autor**
   (o dono) e nota no relatório da release que apropria a mudança.
3. **Sentinela que nunca falha não é sentinela — é decoração**: cada
   sentinela nova vem com PROVA DE MUTAÇÃO colada no relatório (o fix
   revertido temporariamente → sentinela VERMELHA; reposto → VERDE).
4. O job de release (APK assinado) **depende** (needs:) das sentinelas, do
   gate de padrões proibidos e do harness do dispositivo virtual —
   sentinela vermelha = não há APK.
5. Campanhas futuras não podem desativar, skipar ou enfraquecer sentinelas;
   se uma sentinela precisar de mudar por mudança legítima de comportamento,
   o relatório explica e o autor aprova ANTES do merge.

---

## R-001 · perda de seleção entre o TIC e o picker

| campo | valor |
|---|---|
| ID | R-001 |
| Reportado | device 0.8.5, 0.8.7, 0.8.9, 0.8.10 |
| Sintoma exato | `mesh: troca - → prim esfera ERRO(sem TIC com mesh selecionado)` (e variantes `mesh pick 2/3`, `tex pick 2`); trocas com origem explícita davam `fim ok` — a seleção perdia-se entre selecionar o TIC e tocar no picker |
| Causa raiz | (a) `platform/main.cpp` chamava `editor::viewportTapClearsSelection` SEM guard de overlay — o tap na linha/backdrop do picker caía DENTRO do viewRect, armava o deselect no press e LIMPAVA a seleção no release do MESMO frame, ANTES do dispatch do pick (os botões da UI não reclamam o slot de input externo); (b) `APP_CMD_INIT_WINDOW` recarregava a cena (`loadActiveScene`) — os handles dos TICs morriam e `g_editor.selected` ficava stale (o Inspector limpava-o) |
| Fix | (a) guard `!editor::anyOverlayOpen(g_editor)` + `!audioMode` no chamador do deselect (main.cpp); (b) `editor::revalidateSelection()` — re-valida/re-mapeia por NOME após o load do INIT_WINDOW; (c) `editor::pickerGuardBlocked()` — guard no OPEN (Inspector) e no DISPATCH (main): sem alvo válido → hint `seleciona um TIC com mesh` + log `ui: pick bloqueado (sem seleção)`, nunca o caminho `ERRO(sem TIC…)` |
| Teste sentinela | `regress_selection_loss` (tests/test_sentinels.cpp) |
| Linha do replay | FASE 2 do c33_virtual (passos 2.2/2.4/2.8) + FASE 3 (lifecycle) |
| Padrão proibido | `sem TIC com mesh selecionado` (ci/forbidden_log_patterns.txt) |

Nota técnica: parte da intermitência da evidência vinha do
`camgizmo::pickSceneTic` — quando o TIC projetava sob o overlay, o MESMO
tap que matava a seleção re-selecionava o TIC e o pick "funcionava". O
replay usa o TIC AFASTADO do centro (o cenário em que o sintoma aparecia).

## R-002 · migração morta: staging em /tmp (read-only no Android)

| campo | valor |
|---|---|
| ID | R-002 |
| Reportado | device (log da migração; reportado nas séries 0.8.5–0.8.10) |
| Sintoma exato | `fileapi: mkdir falhou em '/tmp' errno=30 (Read-only file system)` → `asset: migracao … FALHOU` → `staging falhou: /tmp/goni_reconvert…` — a migração de projeto antigo morria no device porque o HOST de testes tem /tmp escrevível e o Android NÃO |
| Causa raiz | `assets/AssetConverter.cpp` `convert::reconvertFile` usava o LITERAL `"/tmp/goni_reconvert_<pid>.tmp"` no caminho SAF (raiz `content://` sem fopen) |
| Fix | `convert::stagingWrite` com FALLBACK em cascata: 1) `.staging/` DENTRO do projeto (raiz de ficheiros real — FsStorage do device); 2) CACHE DIR da app via JNI (`VvActivity.cacheDirPath()` → `getCacheDir()` — o único sítio garantido escrevível sem permissões); 3) erro LEGÍVEL com os caminhos reais tentados — JAMAIS /tmp. Gate de CI adicional: grep garante que nenhum literal `"/tmp`/`'/tmp` resta em FileApi/código de assets |
| Teste sentinela | `regress_tmp_staging` (tests/test_sentinels.cpp) |
| Linha do replay | FASE 1 (migração no boot com /tmp READ-ONLY) + FASE 4 (caminho content:// do SAF) do c33_virtual |
| Padrão proibido | `mkdir falhou em '/tmp'` e `staging falhou` (ci/forbidden_log_patterns.txt) |

## R-003 · `none` sem caminho seguro/serializável nos pickers

| campo | valor |
|---|---|
| ID | R-003 |
| Reportado | device (as séries do picker que não limpa / cai no erro) |
| Sintoma exato | a opção `none` nos TICs falhava (não limpava o slot ou caía no caminho de erro); o picker de MESH nem tinha entrada `none` (1º item era "cube (procedural)") |
| Causa raiz | `ui/EditorUi.cpp` `drawAssetMenu`/`applyAssetPick` (menuKind 1) sem entrada none; a limpeza não era de primeira classe (sem posse p/ deferred free, sem round-trip afervado) |
| Fix | `none` em 1º LUGAR nos pickers de mesh (1) e tex (2) (prim já tinha); `mesh: none` limpa o slot (mesh/material/paths/primOn) com posse `primRetire` para deferred free no início do frame seguinte (a cova — o MESMO caminho seguro das trocas; `primRetire = mesh ? mesh : primRetire` nunca perde pendente); serialização já gravava `"mesh":"none"` — agora afervado por round-trip |
| Teste sentinela | `regress_none_slot` (tests/test_sentinels.cpp) |
| Linha do replay | FASE 2 do c33_virtual (passos 2.5/2.6/2.7) |
| Padrão proibido | (coberto pelo R-001 — o `none` sem alvo dá hint, não ERRO) |

## R-004 · dump velho no viewer sem rótulo ANTIGO

| campo | valor |
|---|---|
| ID | R-004 |
| Reportado | device (o dono com o crash-1790830406.dump de outra build no ecrã) |
| Sintoma exato | o log viewer mostrava o dump VELHO (offsets idênticos, build antiga) SEM o rotular como antigo — impossível saber QUE build o produziu |
| Causa raiz | `platform/BuildInfo.cpp` `buildinfo::dumpIsFromOtherBuild` existia desde a 0.8.10 mas NUNCA era chamado pelo `drawLogViewer` (ui/EditorUi.cpp) — wiring morto |
| Fix | `buildinfo::dumpBadge()` (badge textual `[ANTIGO (build N)]` / `[ANTIGO (pre-0.8.10)]`) + chamada no `drawLogViewer` com a cor theme::WARN (exceção documentada ao tema mono, como a brand da toolbar); banner de boot no formato exigido `boot: G.One VV <versão> versionCode <N> sha256 <…> git <…>` |
| Teste sentinela | `regress_dump_identity` (tests/test_sentinels.cpp) |
| Linha do replay | FASE 5 do c33_virtual (dump da build 39 com badge; novo sem badge) |
| Padrão proibido | `ERRO(gerador/upload falhou)` (o erro desonesto da 0.8.7 — relacionado: com os guards, o caminho nunca é atingido pelos pickers) |

## R-005 · ConcurrentModification no reload da tela de projetos (= REG-001 do hotfix 0.9.3)

| campo | valor |
|---|---|
| ID | R-005 (no relatório do hotfix: REG-001) |
| Reportado | device 0.9.2, realme RMX3624 — 23 ocorrências NUM dia (15:53–15:55 e 12:10), process vv.goni; crash loop: abria, morria, reabria, morria |
| Sintoma exato | `FATAL EXCEPTION: pool-2-thread-1 … java.util.ConcurrentModificationException at java.util.ArrayList$Itr.checkForComodification … at vv.goni.ProjectManagerActivity.lambda$reload$4(ProjectManagerActivity.java:355)` |
| Causa raiz | `ProjectManagerActivity.reload()` fazia `all.clear()+all.addAll(VvProjects.load())` na MAIN thread (chamado do onCreate E do onResume, mais os pós-criar/apagar/renomear/duplicar) ENQUANTO a lambda do `io.execute` percorria a MESMA `ArrayList` partilhada na thread de background (`pool-2-thread-1` = o single-thread executor) — o for-each lançava a exceção de modificação concorrente e o processo morria; a exceção numa thread de executor NÃO tem handler — mata o processo |
| Fix | (1) `ReloadGate` (Java puro, `app/src/main/java/vv/goni/ReloadGate.java`): portão single-flight com coalesce trailing — 1 reload de cada vez, o ÚLTIMO pedido é sempre publicado, o portão nunca fica preso (abandon/safeRun); (2) `doReload()` — a thread de fundo constrói lista NOVA local (`fresh`) e NUNCA toca em `all`/`shown`/`missingUris`; a publicação acontece na main thread (`main.post`) e substitui a lista de uma vez; (3) cada entrada lida dentro de try/catch (projeto corrompido/inacessível = saltado com log + contadores); (4) `VvProjects.load` (já defensivo: corrompido → lista vazia) corre na thread io, fora da main (Problema 3) |
| Teste sentinela | `regress_concurrent_reload` + `regress_corrupt_projects` (tests-java/ReloadGateTest.java, JVM do CI) + check estrutural `scripts/reload_concurrency_check.py` (o fonte REAL da Activity: confinamento, try/catch por entrada, portão — apanha a reversão do fix no fonte, que a JVM não vê) |
| Linha do replay | As SENTINELAS JVM correm no job c33-virtual do CI (o replay C++ não instancia Java); o mecanismo do crash é reproduzido e apanhado DENTRO do próprio `regress_concurrent_reload` (o padrão antigo lança; o novo publica sem exceções) |
| Padrão proibido | `ConcurrentModificationException` (ci/forbidden_patterns.txt — grep -E no output combinado JVM+harness) |

Nota técnica: o `pool-2-thread-1` do stack trace é o `Executors.newSingleThreadExecutor()` da Activity — a thread de fundo ÚNICA confirma que a modificação concorrente vinha da MAIN thread (o `all.clear()` do reload seguinte), não de dois executors.

## R-006 · SIGSEGV em AAudio_createStreamBuilder no Unisoc (= REG-002 do hotfix 0.9.3)

| campo | valor |
|---|---|
| ID | R-006 (no relatório do hotfix: REG-002) |
| Reportado | device, realme RMX3624 (Unisoc/Spreadtrum, Android 13/API 33) — ~20 tombstones 19-20/09 + 3 em 25-26/09, app ANTIGA com.goni.runtime (histórico) |
| Sintoma exato | `signal 11 (SIGSEGV), code 2 (SEGV_ACCERR) … #00 /system/lib64/libaaudio.so (AAudio_createStreamBuilder+160) … libgoni.so eng::editor::EditorHost::startAudio()+192 … com.goni.runtime.EditorActivity.onResume+30` |
| Causa raiz | Classe do bug (a app antiga morreu com ela; a 0.9.2 tinha as PORTAS ABERTAS para a mesma classe): `startAudio()` chamado direto do `onResume()` (repetível sem `onPause` parar o stream) abria um SEGUNDO stream sem fechar o primeiro — stream vivo FUGIA; builder/stream usados depois de mortos; retornos de `AAudio_*` sem verificação; o driver AAudio do Unisoc crasha dentro da própria `AAudio_createStreamBuilder` com este padrão (SEGV_ACCERR = escrita/leitura em página sem permissão dentro da lib do vendor) |
| Fix | (1) MIGRAÇÃO PARA OBOE (o caminho primário exigido): `platform/OboeBackend.cpp` — google/oboe PINADO 1.9.3 via CMake FetchContent (Apache-2.0), AAudio na API 27+ com fallback OpenSL ES automático nos devices problemáticos, callback completo (`onAudioReady` sem locks, `onErrorBeforeClose` loga, `onErrorAfterClose` larga a referência e abre o portão); (2) `platform/AudioStartGate.h` — portão atómico `std::atomic<bool>`: `start()` é IDEMPOTENTE em TODOS os backends (oboe, aaudio, audiotrack, stub): chamado 2× NÃO abre 2º stream (a fuga era a porta do crash); (3) AAudio fixado como fallback: `stream_` protegido por mutex (errCb × stop), errCb FECHA o stream morto (o padrão interno do oboe), TODOS os `aaudio_result_t` verificados; (4) AudioTrack também com portão (o arranque duplo criava 2ª thread de escrita + track vazado); (5) cadeia de boot: Oboe → AAudio → AudioTrack → SEM SOM (o editor NUNCA crasha por causa do áudio); o arranque corre no INIT_WINDOW, NUNCA no onResume; (6) probe de estabilidade contra o Oboe |
| Teste sentinela | `regress_audio_lifecycle` (tests/test_sentinels.cpp — o OboeBackend de PRODUÇÃO contra o stub tests/stub/oboe/Oboe.h: arranque duplo, 50× onResume, erro→degradação, zero fugas closeCount==openCount, falha de arranque = sem som) |
| Linha do replay | FASE 8 do c33_virtual (147 checks): boot real INIT→oboe ATIVO; sequência do tombstone (startAudio 3× sem guarda → 1 stream); adversário 50 onResume + 10 startAudio; TERM→INIT ×3 com contadores de fugas (abertos/fechados/vivos); disconnect → frame SEM som; cena corrompida no disco → boot completa; memória do arranque no log |
| Padrão proibido | `SIGSEGV.*AAudio` e `AAudio_createStreamBuilder\+160` (ci/forbidden_patterns.txt — a assinatura exata do tombstone) |

Nota técnica: o TU `platform/AudioOutDevice.cpp` (AAudio/AudioTrack, Android-only) continua compilado-verificado pelo job NDK do build-release (o jni.h fake do host não o tipa — documentado desde 0.8.11-b); o `OboeBackend.cpp` é TU COMUM — compila contra o oboe REAL no APK e contra o stub na suíte (o padrão StorageBridge/jni.h), logo o primário é vigiado nos DOIS lados.
