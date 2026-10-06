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

## R-007 · Editor de script fecha ao digitar / fonte perdida no lifecycle (FASE 9 · G0-1/G0-2)

| campo | valor |
|---|---|
| ID | R-007 |
| Reportado | device 0.9.3 (checklist do dono, FASE 9): "script editor fecha ao digitar" + "script novo não abre com função base" |
| Sintoma exato | o editor de script fechava ao tentar digitar; a fonte editada não voltava ao reabrir; script novo abria vazio |
| Causa raiz | TRÊS camadas (main.cpp `openScriptEditor`/`closeScriptEditor` + ScriptEditor.cpp): (1) o pedido portrait (`jniSetOrientation`) e o `jniImeShow()` corriam no MESMO frame — a rotação (TERM→INIT, o ciclo testado no wiring087) matava o IME pedido contra a janela pré-rotação e o editor ficava SEM caminho de texto (nenhum teclado in-app existia); (2) o handle do TIC dono morria no reload de cena do INIT_WINDOW e NINGUÉM re-validava (só `g_editor.selected` tinha re-validação 0.8.12) — `closeScriptEditor` fazia `get(stale)==null` e a fonte NUNCA gravava; `scriptEditorRun` dava "TIC inválido"; (3) o modelo append-only sem caret + o back 56×56 como único comando fazia as tentativas de digitar fecharem a janela |
| Fix | (1) caret livre (offset em bytes; Left/Right/Up/Down do IME; inserção/DEL no caret); (2) TECLADO IN-APP desenhado pelo editor (2 páginas ABC/123; teclas emitem pelo MESMO `applyEvent` do IME — fonte única de edição); (3) toque no corpo → result 5 → o main RE-PETE o IME (sem perder foco); (4) INIT_WINDOW re-valida `scriptWin.tic` por NOME + RE-PETE o IME após a rotação; (5) `closeScriptEditor`/`scriptEditorRun` re-validam por nome antes de usar; (6) script SEM fonte abre com o esqueleto `central main { on moment { } allmoments { } }` e cursor NO INTERIOR (G0-2); fonte guardada abre intacta com cursor no fim |
| Teste sentinela | `regress_script_typing` (tests/test_sentinels.cpp — 20 teclas IME + 20 teclas do teclado in-app + draw portrait + re-validação por nome + fonte intacta) |
| Linha do replay | FASE 9 do c33_virtual (9.1 esqueleto+par portrait/IME · 9.2 20 teclas do IME sem fechar · 9.3 rotação → IME re-pedido + handle re-validado + teclado in-app digitável · 9.4 fonte grava no componente + .goni · 9.5 lupa→Docs+pesquisa+fonte intacta · 9.6 linha Docs do Settings) |
| Padrão proibido | (nenhum — o sintoma é UI, não log; a vigília é o sentinela + FASE 9) |

Prova de mutação (colada em /home/z/my-project/mutacao-G0-1-*.txt e no RELATÓRIO-0.9.4): re-validação DESLIGADA nas duas camadas → 4 [FAIL] no harness; reposta → 177/177 verde.

## R-008 · Acentos não renderizavam (atlas ASCII) (FASE 9 · G1-2)

| campo | valor |
|---|---|
| ID | R-008 |
| Reportado | device 0.9.0→0.9.3 (screenshots do dono): "ÁUDIO"→"UDIO", "Física"→"Fisica", "Animação"→"Animacao", consola "sem sele  o" |
| Sintoma exato | glifos acentuados NÃO desenhavam; cada byte UTF-8 fora do range ASCII avançava a pena 0.30·altura SEM emitir quad (daí os "gaps" em "sele  o") |
| Causa raiz | `ui/FontAtlas` assava APENAS o range ASCII 32..126 (kFirstChar/kNumChars) e a iteração de texto (`UiContext::labelStyled`, `FontAtlas::widthOf`) era byte-a-byte — nenhum code point >126 tinha glifo |
| Fix | atlas 1024×512 com QUATRO ranges via `stbtt_PackFontRanges` (ASCII + Latin-1 Supplement 0xA0..0xFF + Latin Extended-A 0x100..0x17F + U+2026 …); iteração UTF-8 por CODE POINT (`utf8Decode` inline — malformado avança 1 byte, U+FFFD, nunca loop); COBERTURA EXIGIDA no load (ç ã Ã õ é í Á ó Ç Ú ü — fonte OEM sem eles é rejeitada, a próxima da lista entra); truncagem `textfit` com "…" e FRONTEIRAS de code point (nunca corta um acento ao meio); TODAS as strings de UI sem acento corrigidas (Física/Animação/Rotação/projeção/visível/chão/Permissões/Diagnóstico/licenças/versão/após/área/não…) |
| Teste sentinela | `regress_glyph_coverage` (tests/test_sentinels.cpp — cobertura ç ã Ã õ é í Á ó Ç Ú ü + …, larguras positivas, UTF-8 decode 1/2/3/4 bytes + malformado, a string de teste do dono INTEIRA sem gaps, emissão de glifos acentuados) + `textfit_fase9_truncagem_respeita_code_points` |
| Linha do replay | FASE 9 do c33_virtual (passo dos acentos: atlas do boot com cobertura + frame com "ÁUDIO" emitindo glifos) |
| Padrão proibido | GATE DE FONTE `scripts/glyph_source_check.py` (ci: job core-tests — literais de UI sem acento no C++/Java = vermelho; "Audio" fora da lista: é NOME de TIC serializado no .goni) |

## R-009 · Tocar o corpo de um TIC não selecionava (FASE 9 · G1-6)

| campo | valor |
|---|---|
| ID | R-009 |
| Reportado | device 0.9.3 (dono): "tocar num cubo na viewport seleciona-o" — tocar o CORPO de um objeto grande NÃO fazia nada (e o tap limpava a seleção se houvesse) |
| Sintoma exato | `pickSceneTic` media a distância do toque ao CENTRO projetado com teto de 44 px — objetos grandes tinham o corpo inteiro "morto"; o tap no corpo caía no deselect e LIMPAVA a seleção |
| Causa raiz | ui/CamGizmo.cpp `pickSceneTic`: hit-test só pelo centro projetado (a regra mínima de propósito da 0.7.10, sem ray-cast) |
| Fix | o AABB do mesh é projetado (8 cantos locais pela matriz world → rect de ecrã); o toque dentro do rect + margem 8 px de dedo SELECIONA; 44 px do centro fica como PISO (objetos pequenos/longe); entre acertados ganha o MAIS PRÓXIMO DA CÂMERA (clip.w — antes era "o mais próximo do toque", que trocava na sobreposição); TIC sem mesh carregado mantém a regra do centro. BÔNUS apanhado pela mutação: o log "seleção limpa" não disparava (viewportTapClearsSelection limpa DENTRO antes do main ver) — o handle é fotografado antes (selAntesTap) |
| Teste sentinela | `cameratic_pick_pelo_corpo_g16` (tests/test_cameratic.cpp — corpo seleciona fora dos 44 px · fora do corpo = nada · dois cubos sobrepostos: o mais próximo da câmara vence dos DOIS lados · sem mesh: centro 44 px) |
| Linha do replay | FASE 9 do c33_virtual passo 9.10: canto do corpo >60 px do centro · pickSceneTic direto · o TAP pelo caminho REAL da UI seleciona · cada mudança de seleção LOGA com motivo ("seleção: TIC 'X' (toque no viewport)" / "seleção limpa: toque no vazio (era 'X')") |
| Padrão proibido | (nenhum — o sintoma é interação, não log; a vigília é o sentinela + o replay 9.10) |

Prova de mutação (colada em /home/z/my-project/mutacao-G1-6-*.txt e no RELATÓRIO-0.9.4): `insideRect = false` (corpo desligado — o comportamento 0.9.3) → 2 FALHOU no core (test_cameratic.cpp:718/751) + 4 [FAIL] no harness 9.10; reposto → 755/0 + 200/200 verde.

## R-011 · Ciclo de linkers não rejeitado (FASE 11 · METADE 1)

| campo | valor |
|---|---|
| ID | R-011 |
| Reportado | spec fechada da entrega 0.9.5 (METADE 1): "ciclos a→b + b→a rejeitados com erro legível; profundidade 256 abort legível; nunca crash" |
| Sintoma vigiado | dois linkers que se fecham em ciclo (a→b + b→a, ou a→b→c→a) no MESMO RF deixariam o grafo de links ambíguo (comportamento indefinido por frame); uma cadeia sem teto poderia crescer sem fim |
| Causa (classe vigiada) | a ausência de guarda no registo de RFs — o `Registry::addLinker` aceitaria qualquer link sem olhar ao grafo |
| Fix | `voni/VoniTykers.cpp` `Registry::addLinker`: (1) auto-link a→a rejeitado; (2) ciclo DIRETO b→a no mesmo RF rejeitado com "ciclo: 'a'→'b' e 'b'→'a' no RF 'x'"; (3) ciclos LONGOS por DFS (back-edge) rejeitados; (4) o DFS tem teto `kMaxDepth=256` → "o RF 'x' excede a profundidade 256" (abort legível, nunca stack overflow). O linker que fecha o ciclo é REMOVIDO e o runStart falha com a linha (o script não corre ambíguo) |
| Teste sentinela | `regress_linker_ciclo_rejeitado` (tests/test_sentinels.cpp — ciclo direto com linha + nomes · ciclo longo · 3 linkers VÁLIDOS correm limpo (o guard não caça inocentes) · cadeia de 300 com abort de profundidade) |
| Linha do replay | FASE 11 passo 11.4 do c33_virtual: o editor Run com o script do ciclo → a run NÃO arranca + a BARRA DE ERRO acende com linha + a palavra "ciclo" + os lados nomeados |
| Padrão proibido | (nenhum — a vigília é a sentinela + o replay 11.4) |

Prova de mutação (colada em /home/z/my-project/mutacao-R011-*.txt e no RELATORIO-0.9.5): checks de ciclo desligados (`if (false && …)` nos dois ramos de addLinker) → 27 FALHOU no core (ciclo_direto/ciclo_longo/profundidade_256/exemplo_8_5/R-011/motor) + 4 [FAIL] no harness 11.4; repostos → 796/0 + 230/230 verde.

## R-012 · RF em falta não desligava o tyker (FASE 11 · METADE 1)

| campo | valor |
|---|---|
| ID | R-012 |
| Reportado | spec fechada da entrega 0.9.5 (METADE 1): "RF inexistente → erro legível `RF 'x' não encontrada`, tyker não corre; vários tykers partilham RF" |
| Sintoma vigiado | um tyker com `find(rf)` de um RF que nenhum linker declarou tentaria correr sobre links inexistentes — sem guarda era comportamento indefinido; com guarda FATAL mataria o script inteiro (a spec manda só o tyker parar) |
| Causa (classe vigiada) | a resolução do find sem verificação + sem canal de erro NÃO fatal |
| Fix | `voni/VoniVm.cpp` `initTykers`: o `find` resolve contra o registo de RFs do script; RF ausente → `ts.missing = true` + UMA linha no engine.log pelo canal voni: ("voni: RF 'x' não encontrada — o tyker 'nome' não corre (linha N)") — o tyker fica desligado, o RESTO do script (outros tykers + allmoments) SEGUE; nunca fatal, nunca crash |
| Teste sentinela | `regress_rf_em_falta` (tests/test_sentinels.cpp — o erro EXATO da spec com o nome do RF e do tyker · o tyker BOM corre (o script não morre) · o allmoments segue · o log acontece 1× (não spam por frame) · RF totalmente vazio idem · o estado `missing` desliga o tyker) |
| Linha do replay | FASE 11 passo 11.3 do c33_virtual: o editor Run com um tyker bom + um tyker de RF fantasma → o log exato + o Ator MOVIDO pelo tyker bom (o script vivo) |
| Padrão proibido | (nenhum — a vigília é a sentinela + o replay 11.3) |

Prova de mutação (colada em /home/z/my-project/mutacao-R012-*.txt e no RELATORIO-0.9.5): a deteção desligada (`if (false && …)` no initTykers) → 9 FALHOU no core (rf_em_falta/R-012/exemplo_8_5) + 2 [FAIL] no harness 11.3; reposta → 796/0 + 230/230 verde.

## R-013 · A bijeção da ajuda quebrada (FASE 11 · METADE 2)

| campo | valor |
|---|---|
| ID | R-013 |
| Reportado | spec fechada da entrega 0.9.5 (METADE 2): "Uma só fonte alimenta tudo: o registo da metade 1 alimenta a lista de comandos, a tabela de equivalências, os erros-que-ensinam, os tooltips, as Docs, o completamento e o copiar-referência. Teste de bijeção registo↔Docs↔erros↔tooltips" |
| Sintoma vigiado | uma entrada NOVA no registo sem Docs (ou Docs alterada à mão sem o registo mudar); um erro-que-ensina apontando a uma entrada inexistente; a referência pública (VONI_referencia.md/llms-full.txt) desatualizada vs o registo — qualquer destes QUEBRA a promessa "uma só fonte" |
| Causa (classe vigiada) | fontes de dados DUPLICADAS — a deriva entre o registo e as suas vistas (Docs/erros/referência) |
| Fix | TUDO é vista do registo: `docs::all()` é o registo mapeado 1:1 (o VoniDocs deixou de ter tabela própria); os erros-que-ensinam vêm do `kForeign` do registo; `fullReferenceMarkdown()` gera a referência; os esqueletos/strip/completamento consultam `reg::find/prefixMatch` |
| Teste sentinela | `regress_bijeção_da_ajuda` (tests/test_sentinels.cpp — (1) registo↔Docs com MESMOS campos e MESMO número; (2) Docs obrigatória + EQUIV em TODA a entrada + skeletons bem-formados; (3) toda a palavra estrangeira aponta a uma entrada REAL; (4) prefixMatch acha toda a entrada; (5) os 4 esqueletos da spec com texto EXATO; (6) a referência commitada == à gerada BYTE A BYTE + o llms.txt aponta para ela) |
| Linha do replay | FASE 11.B do c33_virtual: o erro do 'if' ENSINA exist · a strip acende desde 'exi' · o Tab expande o esqueleto exato · o toque explica com exemplo · Silencioso apaga a strip · o clipboard recebe a referência COMPLETA (a mesma string do teste de sincronia) |
| Padrão proibido | (nenhum — a vigília é a sentinela; o teste de sincronia corre no CI pelo ctest) |

Prova indireta (a mutação aqui é a PRÓPRIA edição do ficheiro/dado): editar o VONI_referencia.md à mão (ou acrescentar uma entrada ao registo sem regenerar) → o item (6) da sentinela fica VERMELHO no CI (ficheiro != registo). O R-011/R-012 têm as suas provas coladas nas respetivas secções.

## R-015 · artefacto de release com identidade errada (0.9.6 · G0)

| campo | valor |
|---|---|
| ID | R-015 |
| Reportado | task 0.9.6 (G0): o run verde do fecho 0.9.5 (commit 522ed36) publicou o APK como `goni-vv-0.9.4-release-signed` — identidade ERRADA |
| Sintoma exato | o artifact do CI chama-se `goni-vv-0.9.4-release-signed` enquanto o build.gradle já declara versionName 0.9.5/versionCode 48 (bump do commit a0ce025): o dono baixa um ficheiro que diz 0.9.4 mas INSTALA 0.9.5; a rastreabilidade dos crash-dumps fica turva (o dump traz a versão da build — o nome do ficheiro dizia outra); o checklist VERIFIED do README não casa com a versão instalada |
| Causa raiz | `.github/workflows/release.yml` passo "Publicar artifact do APK": o `name:` era um LITERAL (`goni-vv-0.9.4-release…`) que ninguém mexia no fecho das fases — o bump do build.gradle não chegava ao nome do artifact por construção |
| Fix | (1) o nome do artifact passa a DINÂMICO: `goni-vv-${{ env.VNAME }}-release…` com VNAME exportado do build.gradle no passo build_info (GITHUB_ENV); (2) GATE `release-identity` (scripts/release_identity_check.py) no job build-release ANTES do upload: versionName/versionCode do build.gradle ↔ template do artifact no workflow (nenhum literal divergente; o env é exportado) ↔ docs/RELATORIO-<versionName>.md existe, declara o versionCode e é o MAIS RECENTE (reverter o bump fica vermelho); (3) fim-a-fim no job verify-entry-symbols: o artifact baixa no PRÓPRIO diretório (merge-multiple: false) e o gate afere o NOME REAL publicado com `--artifact-name` |
| Teste sentinela | GATE `release-identity` (CI, dois pontos: build-release pré-upload + verify-entry-symbols no artifact baixado); a script corre igual no local (`python3 scripts/release_identity_check.py`) |
| Linha do replay | (nenhuma — é gate de identidade do CI, não comportamento do device; o replay consome a identidade via build_info.txt, que já era derivada do build.gradle) |
| Padrão proibido | (nenhum — a vigília é o gate; um literal `goni-vv-X.Y.Z-release` divergente no workflow é vermelho pela regra 2a da script) |

Prova de mutação (colada em /home/z/my-project/mutacao-R015-vermelho-verde.txt e no RELATORIO-0.9.6): (A) o literal 0.9.4 volta ao `name:` do upload → gate VERMELHO ("nome de artifact LITERAL… != versionName 0.9.5"); (B) reverter o bump (versionName 0.9.4/versionCode 47) → VERMELHO ("a versão a fechar não é a mais recente documentada"); (C) bump sem RELATORIO (0.9.9) → VERMELHO ("não pode fechar sem o relatório da própria versão"); reposto → VERDE nos três casos. As mutações correram com a MESMA script que o CI corre (a lógica do gate é idêntica nos dois sítios — prova local transfere; nenhum commit vermelho foi empurrado ao repo, conforme o precedente das provas R-011/R-012).

## R-010 · o editor de script mentia no render (FASE 0.9.6 · G2-7b/c)

| campo | valor |
|---|---|
| ID | R-010 (a definição chegou com a task 0.9.6; a 0.9.5 documentou-a como ausente — RELATORIO-0.9.5 §17) |
| Reportado | task 0.9.6 (G2-7b): "texto aparece sem espaços e sem `{` (`centralmain`, `onmoment`) mas o cursor deixa o espaço"; (c) o modelo inicial tem de dar Run = 0 erros |
| Sintoma exato | o editor de script desenhava o código SEM os espaços e SEM `{ } ( ) = +` — as palavras colavam (`centralmain`) enquanto o cursor piscava no sítio do espaço (o caret media a linha INTEIRA); a spec: "ficheiro guardado == texto renderizado" |
| Causa raiz | DUAS camadas: (1) o render (ScriptEditor.cpp) desenhava token a token avançando `x` só pela largura das PEÇAS COLORIDAS — o VoniHighlight.classifyLine salta espaços (linha "espaços — avança") e pontuação ("sem classe") SEM token, e os GAPS simplesmente não desenhavam; (2) [descoberta no 12.4] o VoniSystem.editorRestart RECUSAVA TIC sem ScriptComp ("o TIC não tem componente Script") — o dono que abre um script NOVO e prime Run (antes de fechar/guardar) caía num beco sem saída, violando "Run no modelo fresco = 0 erros" |
| Fix | (1) `renderPieces()` (ScriptEditor.h/.cpp): o PLANO DE RENDER da linha com os GAPS PREENCHIDOS (Cls::User) — a concatenação das peças é a linha INTEIRA; o draw usa as peças (espaços e `{ }` desenham; o x avança pela linha real; o caret coincide com o que se vê); (2) `editorRestart` cria o ScriptComp em falta (o MESMO precedente do closeScriptEditor — "o fonte não se perde por um ciclo"); (3) bônus apanhado pela FASE 12.6: o draw das Docs usava coordenadas de CONTEÚDO num scroll que só RECORTA (não traduz) — a lista desenhava fora do sítio; agora em coordenadas de ECRÃ (o padrão do SettingsPage) com culling |
| Teste sentinela | `regress_r010_editor_roundtrip` (tests/test_sentinels.cpp — (1) as peças cobrem TODOS os bytes de uma bateria de linhas com espaços/`{ }`/strings/comentários/acentos; (2) o esqueleto compila e runStart = ZERO erros; (3) o SUBSTITUIR troca a palavra INTEIRA (if→exist, caret segue, erro limpa; 'iffy' intocada); (4) todas as linhas do kSkeleton cobertas) |
| Linha do replay | FASE 12 do c33_virtual: 12.4 Run no esqueleto fresco = 0 erros · 12.5 o erro do 'if' ENSINA + o botão SUBSTITUIR troca no buffer pelo caminho REAL · 12.6 a descrição das Docs desenha INTEIRA (wrap, sem "…") |
| Padrão proibido | (nenhum — a vigília é a sentinela + a FASE 12) |

Prova de mutação (colada em /home/z/my-project/mutacao-R010-{vermelho,verde}.txt e no RELATORIO-0.9.6): o gap-fill DESLIGADO (`if (false)` no ramo das peças — o render de 0.9.5) → `regress_r010_editor_roundtrip` FALHOU em 4 frentes (p.begin != pos; pos != strlen; as peças deixam de cobrir a linha); reposto → OK. A mutação do editorRestart (recusar sem ScriptComp) é apanhada pela 12.4 do harness (Run no esqueleto = erro "o TIC não tem componente Script").

## R-014 · o import não aparecia no seletor de malha (FASE 0.9.6 · G4; REESCRITO na FASE 0.9.6-MASTER · GRUPO A)

| campo | valor |
|---|---|
| ID | R-014 |
| Reportado | task 0.9.6 (G4): "glb/gltf importados não aparecem na 'seleção de malha' dos TICs (StaticBody/CharacterBody/PlayerBody/RigidBody/Mesh) após o import" |
| Sintoma exato | o import do glb/gltf corria (toast "importado: N mesh(es)", ficheiro escrito), mas ao abrir o seletor de malha do TIC o asset novo NÃO constava da lista |
| Causa raiz | forense (as 5 hipóteses da task confirmadas/eliminadas por leitura de código): (1) pasta ERRADA — NÃO: o importFile escreve `assets/<stem>.gmesh` e o refreshCatalog lista `Project::kDirAssets` = assets/ (PARTILHAM diretório); (2) índice stale — NÃO: o catálogo refresca NO fim do import (importJobFinish → refreshCatalog) E AO ABRIR o seletor (transição do assetMenu); (3) filtro de extensão — NÃO: ambos usam `.gmesh` (6 bytes exatos); (4) manifesto — N/A (o catálogo lê o diretório, não manifesto); (5) content:// — NÃO: a fonte SAF é lida por streaming (fileapi), o convertido escreve pelo storage do projeto. **A CAUSA REAL: o CAP de 5 ficheiros SEM scroll no drawAssetMenu** ("cap de ficheiros no overlay (mono, sem scroll — F8 traz scroll)" — a pendência da F8 nunca chegou): com 5+ .gmesh no projeto, o import novo ficava FORA da lista visível para sempre |
| Fix | a lista de ficheiros do seletor passou a listar TODOS com SCROLL (o padrão da Hierarchy/Inspector: janela encaixada na faixa do overlay — máx. o que a altura permite —, drag = scroll, tap parado = escolha pelo scrollTap, culling fora da janela); as linhas fixas (none/cube/importar) ficam fora do scroll |
| Teste sentinela | `assetpick_r014_todos_os_ficheiros_aparecem_no_seletor` + `assetpick_r014_o_scroll_alcanca_os_ficheiros_que_nao_cabem` (tests/test_assetpick.cpp — 7 meshes: a 7ª linha DESENHA (glifos no rect) e o tap APLICA meshPath; 9 meshes: o drag alcança o 9º e aplica) |
| Linha do replay | FASE 12.8 do c33_virtual: um .glb REAL (container GLB montado no harness) importado pelo convert::importFile DE PRODUÇÃO → convertido em assets/robo.gmesh → catálogo lista (com 6 meshes pré-existentes) → <1s → seletor aberto em TIC com MeshRenderer → drag até ao fim → tap → meshPath == "assets/robo.gmesh" no componente (fim-a-fim). **FASE 0.9.6-MASTER (GRUPO A/0.9.6.4): a FASE 12.8b ESTENDE o fim-a-fim ao PAR .gltf+.bin PELO BROWSER REAL (1 toque na linha → job → irmãos copiados → catálogo <1s → applyAssetPick aplica assets/par.gmesh) — o mesmo contrato, agora pelo caminho do navegador** |
| Padrão proibido | (nenhum — a vigília é a sentinela + a FASE 12.8/12.8b) |

Prova de mutação (colada em /home/z/my-project/mutacao-R014-vermelho{,2}.txt e no RELATORIO-0.9.6): o cap de 5 volta ao loop de desenho (`i < 5`) → `assetpick_r014_todos_os_ficheiros_aparecem_no_seletor` FALHOU (a 7ª linha não desenhava — o check de GLIFOS no rect da linha é o que apanha; um tap às cegas passaria porque o mapeamento do scrollTap cobre a lista toda); reposto → verde. NOTA honesta: a 1ª rodada da prova usou um teste com o check de glifos ANTES do frame desenhado (falhava por razão errada) — a prova foi REFEITA com o teste corrigido (mutacao-R014-vermelho2.txt).

## R-016 · a documentação com placeholders e achismo (FASE 0.9.6 · G5)

| campo | valor |
|---|---|
| ID | R-016 |
| Reportado | task 0.9.6 (G5): "novo gate docs-lint: grep em docs/, README e nos relatórios por placeholders (`<repository-URL>`, marcadores de pendência, texto de enchimento) e linguagem de achismo — a lista vive em `ci/forbidden_docs_patterns.txt`" | <!-- docs-lint:allow -->
| Sintoma exato | a documentação vista de fora (RELATÓRIOS incluídos) podia conter marcas de rascunho (pendências, placeholders de template, "lorem") e frases de hedging ("acho", "talvez", "should work") que violam a regra da casa: cada afirmação cita evidência, e o que não foi verificado está na secção de NÃO VERIFICADO — nunca no meio do texto disfarçado de certeza | <!-- docs-lint:allow -->
| Causa raiz | inexistência de vigília: nada impedia um push com "corrigir depois" ou "acho que 60fps" num RELATORIO — a revisão era só humana (e o autor é o mesmo que escreve, o pior revisor possível para as próprias fraquezas) | <!-- docs-lint:allow -->
| Fix | `scripts/docs_lint_check.py` + `ci/forbidden_docs_patterns.txt` (a lista é a fonte; o script é só o motor): 14 padrões — placeholders de template, marcadores de pendência por PALAVRA INTEIRA case-sensitive (`TODOS`/`todo` do português NÃO casam — a fronteira de palavra logo após o O), texto de enchimento, e hedging PT/EN por palavra inteira (`despacho`/`macho`/`colorem` NÃO casam). Alvo: docs/**.md recursivo + README + ESTUDOS + ARCHITECTURE + VONI_referencia + llms.txt/llms-full.txt. O CI corre o gate no job core-tests de CADA push. A limpeza inicial: 24 ocorrências em 13 ficheiros (21 eram "TODO" português em MAIÚSCULAS → minúsculas; 4 citações a marcadores reescritas por extenso; as citações às palavras de hedging passaram a apontar à lista do gate em vez de as repetir) | <!-- docs-lint:allow -->
| Teste sentinela | o gate É a sentinela (à semelhança do scope-check e do release-identity: gates de processo, não testes de unidade) |
| Linha do replay | (nenhuma — a vigília é o gate no CI; a FASE 12 não envolve docs) |
| Padrão proibido | `docs-lint: VERMELHO` no passo "Gate docs-lint (R-016)" do job core-tests — ficheiro:linha:padrão listado no log do CI |

Prova de mutação (colada em /home/z/my-project/mutacao-R016-vermelho-verde.txt e no RELATORIO-0.9.6): (A) `TODO fix this before release` no fim da README → VERMELHO (1 ocorrência, ficheiro:linha:padrão); reposta → VERDE. (B) `Acho que o render está estável.` no RELATORIO-0.9.4 → VERMELHO; reposta → VERDE. (C) `git clone <repository-URL>` no RELATORIO-0.9.4 → VERMELHO; reposta → VERDE. CONTROLO: `TODOS os testes passam e todo o texto saiu; o despacho e o macho e o colorem-se ficam.` → VERDE (zero falsos positivos do português). A MESMA script que o CI corre — nenhum commit vermelho empurrado (precedente R-011/R-012/R-015). <!-- docs-lint:allow -->

## R-017 · o bench que mente (FASE 0.9.6 · G6)

| campo | valor |
|---|---|
| ID | R-017 |
| Reportado | task 0.9.6 (G6): "Settings→Diagnóstico ganha Correr bench/Copiar relatório; todo métrico é real (medido no device) e nenhum hardcoded; o que não pôde ser medido imprime 'não medido'; o bloco colável tem 9 linhas fixas" |
| Sintoma exato | a engine não tinha NENHUMA medição reproduzível que o dono pudesse colar (o FPS da barra de estado é um número vivo sem histórico; as afirmações de desempenho dos relatórios eram transcrições manuais do logcat); o risco inverso: um "bench" que imprimisse números fixos pareceria medição sem o ser |
| Causa raiz | inexistência do instrumento: sem máquina de fases de medição, sem agregação (média/mín/1% low), sem template fixo colável — e sem SENTINELA que afirmasse que os números vêm de medições |
| Fix | (1) core/Bench (a parte PURA): Measured (cada valor sabe se foi medido — "não medido" é um ESTADO, nunca um 0 disfarçado) · aggregate (média/mín/1% low com piso de 10 amostras) · readPeakRssKb (VmHWM do /proc) · makeReferenceGlb (GLB determinístico byte a byte: grelha 16×16 + nó com scale 2.5 — a escala do relatório é o ROUND-TRIP deste valor pelo importer REAL até ao Transform3D do TIC) · format (O BLOCO DE 9 LINHAS — a fonte única do texto colável) · marcas de arranque (onCreate/onResume contra a 1ª apresentação e a 1ª pós-resume: cold/warm reais do processo; quem nunca saiu do foreground não TEM warm — e o relatório diz); (2) platform/main.cpp (a máquina de fases): DefRun (cena default) → BuildScene (64 TICs em grelha 8×8 com o mesh EMPACOTADO — determinístico, zero I/O) → Import (o convert::importFile DE PRODUÇÃO com cronómetro + o TIC do mesh importado pela gltfInstantiate com o bindMesh do GpuAssets REAL) → BenchRun (cena bench) → Texture (512×512 determinístico pela MESMA HardwareCompressor do pipeline — ASTC se a extensão existe, senão ETC2) → Audio (o MESMO probe 50+10 do Settings — só no APK, guarda de COMPILAÇÃO: o host diz "não medido" em vez de medir o stub) → Finish (RSS + device/APK pela JNI benchDeviceInfo + projeto por statBytes) → Done; os verts/draw calls saem da MESMA soma da statusLine (st3d+grid+UI) no MESMO frame; P-02: fora do bench o custo é UM if por frame; (3) as MARCAS de arranque: nativeRegisterActivity("onCreate"/"onResume") (StorageBridge) contra markFirstFrame (a CADA swap — guarda a 1ª e a 1ª pós-resume); (4) ProjectStorage::statBytes (novo método virtual: FsStorage = stat do SO, FakeStorage = bytes em memória p/ o harness MEDIR o projeto dele, SAF = não suportado → "não medido" — nunca inventa); (5) VvActivity.benchDeviceInfo (Java: Build.MODEL + SDK_INT + base.apk bytes + sha256) pela ponte g_midBenchInfo — UMA chamada por bench, nunca no caminho de frames; (6) Settings→Diagnóstico: "Correr bench" + "Copiar relatório" (actionRow + o WALK DO RE-DESPACHO — ver abaixo) |
| BUG apanhado pelo harness | a 1ª versão dos botões DESSENHAVAM mas o walk do scrollTap não re-despachava os DOIS novos alvos — botão MORTO ao toque (a MESMA classe do bug 0.9.1 "os botões da página estavam MORTOS": desenhar ≠ tocar). A FASE 12.9 apanhou no PRIMEIRO run (tap não fechava o Settings) — o fix: as 2 entradas no walk do kBitDiag com a MESMA matemática do draw |
| Teste sentinela | `regress_bench_nao_mente` (tests/test_sentinels.cpp): (a) dois Reports com TODOS os campos diferentes → as 9 linhas TODAS diferentes (nenhuma posição do bloco é constante — um número hardcodado em qualquer sítio = 2 blocos iguais nesse sítio = vermelho); (b) a honestidade: não medido é DITO por campo (um valor medido em resto vazio aparece, o resto não); (c) a agregação é matemática (5% das amostras em queda derruba o 1% low; UMA amostra em 600 não derruba); (d) o GLB é determinístico e a escala 2.5 chega ao importer. + tests/test_bench.cpp (8 casos: o bloco parseável/não medido/números vêm da medição/agregação/RSS real/GLB determinístico/round-trip da escala/marcas de arranque com resetMarksForTest — os testes de handshake/lifecycle chamam o nativeRegisterActivity com origens REAIS e partilham o processo) |
| Linha do replay | FASE 12.9 do c33_virtual (316 checks): Settings→Diagnóstico→Correr bench PELO CAMINHO DA UI (tap no botão — o mesmo walk do re-dispatch) → a máquina de fases TERMINA → fps≈60 do relógio INJETADO (o CI não tem GPU: o dt é 1/60 injetado e o relatório MEDE o relógio que lhe deram — se o formato hardcodasse, o check não casava) · verts MEDIDOS (stub GL real) e a cena bench TEM MAIS verts que a default · import MEDIDO com cronómetro real · escala 2.5 round-trip · textura com formato REAL · RSS real do /proc · projeto por statBytes · áudio/device/APK "não medido" no host (a honestidade é PARTE da prova) · a cena bench SAI (o editor volta ao que era) · o clipboard recebe o bloco EXATO do format (byte a byte — a fonte é única) |
| Padrão proibido | `bench: áudio não medido` no host é OBRIGATÓRIO (se um dia aparecer áudio medido num log do CI, alguém desligou a guarda de compilação) |

Prova de mutação (colada em /home/z/my-project/mutacao-R017-vermelho-verde.txt e no RELATORIO-0.9.6): (A) o format() HARDCODA a linha do import ("import glTF ref: 96 ms (escala 2.5)" sem ler o Report) → `bench_r017_os_numeros_vem_da_medição` FALHOU (diffs==9 — os 2 blocos passaram a ser iguais na linha 5) + `regress_bench_nao_mente` FALHOU + 3 casos do test_bench FALHARAM (5 no total); reposto → 817/0. (B') o benchTick escreve fps HARDCODADO (30.0) em vez de 1/dt na fase DefRun (o harness continua a injetar 1/60) → 12.9 "cena default: média ≈60fps" FAIL (avg=30 ∉ (55,65)) — o vermelho vem da DIFERENÇA entre o medido e o inventado; reposto → 316/0. NOTA honesta: a 1ª rodada da mutação B usava injeção de 1/30 para "distinguir" e falhava por RAZÃO ERRADA (0.3s ÷ 1/30 = 9 amostras < o piso de 10 do aggregate → ok=false, não o hardcode) — refeita com o hardcode direto (a lição da R-014 aplicada: a prova tem de falhar pela razão certa).

## R-018 · as medidas dp desenhadas como px (FASE 0.9.6.1 · PASSO 0)

| campo | valor |
|---|---|
| ID | R-018 |
| Reportado | task 0.9.6.1 (PASSO 0): "suspeito que valores em dp estão a ser usados como px: o cabeçalho do editor de script mede cerca de 56 px (devia ter cerca de 112 px num ecrã de 720 px de largura), os botões de ferramentas têm cerca de 48 px e as teclas do teclado próprio cerca de 48×65 px — metade do pedido (48dp)" |
| Sintoma exato | no C33 (densidade 2.0) o chrome inteiro saía a ~½ do dp especificado: cabeçalho 56px (~29dp reais) com rótulos cortados, alvos de 48px (~24dp reais), teclas 48×65px; o texto/ÍCONES (assados a 28px ≈ 14sp) estavam CERTOS — só o chrome estava a meia escala |
| Causa raiz | (leitura do código, antes de qualquer fix) NÃO existia função dp→px em lado nenhum: `theme::kSpace*/kTarget/kRadius*`, as constantes `safe::kTopBarH/kStatusH/kPanelW/kDrawer*`, os baselines `kHeaderTitleBase/kHeaderSubBase` e as constantes de `EditorLayout.h` eram `constexpr f32` EM DP consumidas COMO PX CRUS nos rects e nos desenhos. O atlas de fonte (28px ≈ 14sp na densidade 2.0 do C33) é que disfarçava o bug — o texto certo ao lado do chrome a metade |
| Fix | UMA função na origem: `theme::dp()` (densidade = `AConfiguration_getDensity/160` == `DisplayMetrics.density`, lida no android_main ANTES de qualquer layout e LOGADA — o log de identidade do ecrã dá densidade, superfície e insets em px E dp, Java e nativo). As FONTES ÚNICAS passam por ela: safe::toolbarRect/statusRect/viewportRect/bottomTabRect/modeTabRect/topChromeRect + kPanelW; EditorLayout.h (kPad/kHeaderH/kRowH/kSearchRowH/kMenuW) virou variáveis atualizadas por `editor::applyDensity()`; os componentes (ScriptEditor cabeçalho/teclado/gutter/barras, Docs/Settings/TextWindow, Toolbar, ViewportChrome, BottomPanel, drawAssetMenu) consomem dp() nos próprios locais. Em densidade 1.0 (testes/harness) dp(v)==v — layout de SEMPRE |
| Teste sentinela | `regress_density_escala_dp` (tests/test_sentinels.cpp): densidade 2.0 injetada → toolbarRect 112px, statusRect 48px no fundo, dp(48)==96, keyboardHeight com teclas de 96px de altura; densidade 1.0 → EXATAMENTE o layout de sempre. + FASE 12.10 do c33_virtual (o caminho REAL: `vvstub::g_stubDensityDpi = 320` → o AConfiguration do main devolve 2.0; 7 checks) |
| Linha do replay | FASE 12.10 do c33_virtual: "com densidade 2.0 a barra de cima mede 112px (56dp REAL)", "as linhas/paddings dos painéis duplicam", "o teclado mede as teclas a 96px de altura", "com densidade 1.0 o layout é EXATAMENTE o de sempre" |
| Padrão proibido | (nenhum — a vigília é a sentinela + a FASE 12.10) |

Prova de mutação (executada localmente antes do push): `dp()` revertido a identidade (`return v;`) → `regress_density_escala_dp` FALHOU em 4 asserções exatas (bar.h==112 FALHOU · dp(48)==96 FALHOU · status.h==48 FALHOU · keyboardHeight teclas 96px FALHOU); reposto → 818/0. A verificação no DEVICE (as medidas do dono: cabeçalho 112px, botões 96px, teclas ≥96px de altura) fica na checklist do APK desta fase — o que o CI prova é o mecanismo (o device é a vara de medir).

## R-019 · o toque nunca movia o cursor do editor de script (FASE 0.9.6.2)

| campo | valor |
|---|---|
| ID | R-019 |
| Reportado | task 0.9.6.2 (URGENTE): "no editor de script o cursor nunca fica dentro de 'on moment { }' nem de 'allmoments { }'; fica sempre fora. Não dá para mudar de linha nem para escrever onde se quer. Isto torna o editor inutilizável" |
| Sintoma exato | tocar no corpo do código não fazia NADA ao cursor; tudo o que se digitava caía no MESMO sítio (o fim do buffer — os scripts guardados abrem com caret = buf.size()); o Enter inseria no fim, nunca entre as chavetas |
| Causa raiz | (leitura do código, scriptwin::draw) o toque no corpo CALCULAVA o offset sob o dedo (`bo` — linha pelo y+scroll, coluna acumulando larguras) e NUNCA O APLICAVA ao caret: `bo` só alimentava a palavra da dica (`wordAtOffset(st, bo)`); faltava `st.caret = bo`. A aritmética da coluna media BYTE a byte (um acento contava 2) |
| Fix | (1) o tap aplica o offset: `st.caret = bo` + blink a zero (o dono vê o salto); (2) a coluna passou a code point inteiro (`caretInLineForX` — função pura no header com métricas injetadas: o toque no meio do carácter escolhe-o pela meia largura; UTF-8 nunca parte bytes); (3) o ENTER herda a indentação da linha (`indentationOfLine`); (4) o LOG do cursor: cada toque loga x/y em px E dp + scroll + linha/coluna/caret; cada evento IME loga o índice ANTES→DEPOIS — o dono segue o cursor no engine.log; (5) a sincronização com o IME do sistema é inerente ao desenho da casa (o EditText NUNCA acumula texto — cada commit devolve true sem super; o buffer é SÓ da engine e o sistema é um espelho que nunca repõe o cursor) |
| Teste sentinela | tests/test_editor_buffer.cpp (6 casos, pelo caminho PÚBLICO applyEvent): inserir no MEIO (não no fim) · apagar e juntar linhas · Enter herda indentação · índice↔linha/coluna · toque com métricas simuladas (fixa, proporcional e UTF-8) · o caret nunca salta para o fim + FASE 12.11 do c33_virtual (o teste EXATO do dono pela UI REAL: tocar entre as chavetas de "allmoments { }" → o caret fica aí; escrever "x" insere aí; tocar no meio da linha 2 move para lá; Enter indenta; o log do toque existe) |
| Linha do replay | FASE 12.11 do c33_virtual (323→329 checks) |
| Padrão proibido | (nenhum — a vigília é a sentinela + a FASE 12.11) |

Prova de mutação (executada localmente antes do push): o `st.caret = bo` removido (o offset volta a ser ignorado) → a FASE 12.11 FALHOU no check "(c) tocar no meio da linha 2 põe o cursor NA linha 2" (o tap deixou de mover o cursor — a razão certa; o check (a) passou por acaso: o caret inicial do esqueleto JÁ está entre as chavetas); reposto → 329/0 + 824/0 (818 + 6 novos casos do buffer).

## R-020 · a importação glTF/GLB falhava em silêncio (FASE 0.9.6.3)

| campo | valor |
|---|---|
| ID | R-020 |
| Reportado | task 0.9.6.3: "modelos importados em OBJ aparecem em 'Adicionar mesh' e selecionam-se. Os importados em glTF ou GLB não aparecem: só os OBJ e as primitivas da engine. Falha em silêncio" |
| Sintoma exato | o OBJ chegava ao seletor; o glTF/GLB não chegava (ou chegava errado) sem NENHUMA mensagem que dissesse o porquê |
| Causa raiz | (forense lado a lado OBJ↔glTF, por leitura do código: browser → browserImportFile → importJobStart → convert::importFile → convertGltfCommon → writeGMesh → importJobFinish → refreshCatalog → drawAssetMenu — os caminhos PARTILHAM tudo depois do despacho por extensão, que aceita obj/gltf/glb IGUAIS) as divergências viviam DENTRO do ramo glTF: (1) sem deteção de extensionsRequired — um ficheiro Draco/meshopt/KTX2 falhava DEPOIS com erros obscuros ("POSITION inválido"); (2) UMA textura má derrubava o import inteiro (return false no passe de texturas); (3) o TRS dos nós era IGNORADO — o .gmesh saía CRU (modelo fora do sítio/invisível) e multi-mesh partia-se em <stem>_N.gmesh (o TIC só recebia UMA parte); (4) .gltf com .bin externo ilegível falhava com erro genérico de accessor |
| Fix | (1) deteção ANTES do parse: extensionsRequired com Draco/meshopt/KTX2/quantization → "o ficheiro usa compressão X, que ainda não é suportada — exporta sem Draco/meshopt/KTX2" (extensionsUsed logada, informativo); (2) FALHA PARCIAL não esconde o modelo: textura que falha = AVISO + o mesh entra com material por defeito; (3) MERGE: um passe pela hierarquia compõe a matriz-mundo (T·R·S pela cadeia de pais) e junta TODAS as malhas/primitivas num ÚNICO assets/<stem>.gmesh (grupos preservam o material por primitiva; normais pela rotação de mundo; skin viaja com o merge; fallback cru quando não há nós); (4) .gltf com vizinho ilegível → "o seletor do Android não dá acesso aos vizinhos; exporta o modelo como GLB"; (5) os LIMITES FINAIS no log com aviso de grande/pequeno — NADA escalado automaticamente; (6) o LOG RICO do parse (nós/malhas/primitivas/verts/índices/materiais/texturas/animações/skins/extensões) + "registado na lista como assets/X.gmesh" + o CONTEÚDO da lista de "Adicionar mesh" logado quando o seletor abre |
| Teste sentinela | `regress_gltf_draco_mensagem_clara` (um glTF com extensionsRequired Draco → importFile falha com a mensagem clara que nomeia a compressão) + `regress_gltf_transforms_dos_nos_no_gmesh` (triângulo unitário + nó scale 2.5 + translation 1 → o .gmesh lido de volta tem os vértices EM MUNDO (3.5, 2.5, 2.5) e UM único ficheiro de saída) |
| Linha do replay | FASE 12.8 do c33_virtual continua verde (o import real pelo seletor); o log do catálogo existe a cada abertura |
| Padrão proibido | (nenhum — a vigília é a sentinela) |

Prova de mutação (executada localmente antes do push): a lista kUnsupported esvaziada (deteção off) → `regress_gltf_draco_mensagem_clara` FALHOU (o erro voltou ao obscuro, sem nomear a compressão — a razão certa); reposto → 826/0 + 330/330.

## R-021 · o .gltf separado não encontrava os irmãos (FASE 0.9.6-MASTER · GRUPO A)

| campo | valor |
|---|---|
| ID | R-021 |
| Reportado | task FASE 0.9.6-MASTER (GRUPO A1): "glTF separado falha: `buffer externo não resolvido: scene.bin` (só o .gltf é copiado; o .bin fica no original)" |
| Sintoma exato | importar um .gltf com .bin/texturas EXTERNOS falhava com «buffer externo não resolvido: scene.bin»; o R-020 (0.9.6.3) limitou-se a trocar a mensagem para «o seletor do Android não dá acesso aos vizinhos; exporta como GLB» — os irmãos continuavam sem ser lidos |
| Causa raiz | (leitura do código) o resolver do convertGltfFile fazia `fileapi::readAll(uri)` com o URI RELATIVO («scene.bin») — resolvia contra o CWD DO PROCESSO (no Android «/»). Os testes antigos passavam porque escreviam o .bin NO CWD do CI (a sentinela R-020 original codificava EXATAMENTE o bug; foi RECALIBRADA para diretório real + caminhos absolutos, como o browser do device) |
| Fix | (1) `collectGltfSiblingUris` + `copyGltfSiblings`: após a cópia do .gltf para source/, o JSON é lido e TODOS os URIs externos (buffers[].uri + images[].uri) são copiados DO DIRETÓRIO ORIGINAL para `source/<subcaminho>` (URI-decode %20; subpastas mantidas; «..» e esquemas absolutos recusados com erro que os nomeia; um LOG por irmão; irmão AUSENTE = ERRO QUE NOMEIA O FICHEIRO) — o projeto fica autossuficiente; (2) o resolver e o leitor de texturas externas resolvem contra o DIRETÓRIO DO FICHEIRO em conversão (import: a pasta original; reconvert: source/) — texturas externas de .gltf passam a ENTRAR no passe de texturas; (3) reconvert em SAF stageia os irmãos ao lado da fonte (sem a pasta original o reconvert diz qual irmão falta); (4) o reconvert de uma fonte que JÁ vive em source/ salta a cópia auto-referencial (a cópia sobre si mesma corrompia em cascata — o guard realpath nos ficheiros E nos irmãos) |
| Teste sentinela | `regress_gltf_irmaos_do_diretorio_original` (tests/test_sentinels.cpp): diretório REAL com o par + «tex albedo.png» (URI «tex%20albedo.png») → import ok; stats.siblings==2; source/ com .gltf+.bin+textura decodificada; a textura vira .gtext; o RECONVERT funciona SEM a pasta original; o irmão ausente = erro que o nomeia. + `regress_gltf_transforms_dos_nos_no_gmesh` RECALIBRADA (diretório real; irmãos copiados; vértices em mundo) |
| Linha do replay | FASE 12.8b do c33_virtual: o browser ABERTO na pasta do par; 1 TOQUE na linha do .gltf → job → 2 irmãos copiados → source/ autossuficiente → catálogo <1s → applyAssetPick aplica assets/par.gmesh |
| Padrão proibido | (nenhum — a vigília é a sentinela + a FASE 12.8b) |

Prova de mutação (colada em mutacao-R021-vermelho.txt): a cópia de irmãos DESLIGADA (copyGltfSiblings sai sem copiar) → 8 testes FALHARAM — `regress_gltf_irmaos_do_diretorio_original` (siblings==2, source/scene.bin ausente, «tex albedo.png» ausente, reconvert morto, «NÃO EXISTE» sem nome) + `regress_gltf_transforms_dos_nos_no_gmesh` (siblings==1, source/scene.bin ausente) — a razão certa (o contrato dos irmãos é o que falha, não um efeito colateral); reposta → 0 falhas.

## R-022 · a imagem que matava o import do GLB + o layout sem evidência (FASE 0.9.6-MASTER · GRUPO A)

| campo | valor |
|---|---|
| ID | R-022 |
| Reportado | task FASE 0.9.6-MASTER (GRUPO A2/A3): "GLB com texturas falha: `bufferView da imagem fora do buffer` (chunk BIN/validação de imagens)" e "Textura falha + geometria OK → importa sem texturas com W + toast; geometria falha → erro. Nunca silencioso" |
| Sintoma exato | um GLB com UMA imagem má (bufferView fora do buffer, leitura falhada, bytes podres) matava o import INTEIRO com a geometria boa — «glTF: bufferView da imagem fora do buffer» return false no meio do parse |
| Causa raiz | (leitura do código) o parse tratava a falha do bufferView de IMAGEM como FATAL; e o resolveView devolvia bool — «fora do buffer» cobria TAMBÉM as falhas de leitura (I/O) com a mesma mensagem enganosa; o parseGlb (em memória) caminhava os chunks SEM o alinhamento de 4 bytes (inconsistente com o convertGlbFile — ficheiros com JSON chunk não múltiplo de 4 divergiam entre os dois parsers); não havia log do layout dos chunks nem verificação da cópia (a cópia truncada disfarçava-se do mesmo erro) |
| Fix | (1) UMA SÓ rotina de validação (`resolveView` devolve `ViewFail` com a CAUSA: NoBuffer/BadLength/OutOfBounds/TooBig/ReadFail/BadStride) usada por MESHES (fatal — geometria falha = import falha, com a causa exata) e IMAGENS (warn + skip — o import SEGUE sem texturas); (2) `GltfImage.broken` marca a imagem falhada; o passe de texturas CONTA as falhas (`stats.texWarn`) e o toast do import diz «SEM N textura(s) (avisos no engine.log)» — nunca silencioso; (3) o LOG DO LAYOUT do GLB (header ver/total · JSON len/start · BIN len/binStart/alinhado 4 · ficheiro) + um log por imagem (bufferView off/len/mime ou uri externa); (4) `verifyCopyChunked`: a cópia do GLB em source/ é conferida byte a byte contra a fonte (chunks com caminho real; readBytes com guarda em SAF/FakeStorage; além do orçamento DIZ que não verificou) — «cópia truncada» com a POSIÇÃO exata do byte que difere; a cópia má sai do projeto; (5) o parseGlb alinha os chunks a 4 bytes como o caminho de ficheiro — os dois parsers, uma regra |
| Teste sentinela | `regress_glb_imagem_no_fim_com_padding` (tests/test_sentinels.cpp): (a) GLB com JSON NÃO múltiplo de 4 + imagem VÁLIDA no FIM do BIN com padding → parse em memória E import de produção → mesh + .gtext, texWarn 0; (b) imagem PODRE (bytes não-PNG dentro do buffer) → import SEGUE sem texturas, texWarn ≥ 1; (c) bufferView da imagem FORA do buffer (o defeito exato do device) → import SEGUE (mesh entra), texWarn == 1, sem textura. + `regress_glb_copia_verificada`: cópia íntegra → true; truncada → false + «cópia truncada»; 1 byte trocado → false COM A POSIÇÃO; ausente → false |
| Linha do replay | FASE 12.8/12.8b do c33_virtual (o import de produção com o log do layout no engine.log; o texto do toast «SEM N textura(s)» sai no importJobFinish) |
| Padrão proibido | (nenhum — a vigília é a sentinela) |

Prova de mutação (colada em mutacao-R022-vermelho.txt): o binStart SEM o alinhamento de 4 (`padded = jsonEnd` em vez de `(jsonEnd+3) & ~3`) → `regress_glb_imagem_no_fim_com_padding` FALHOU no import da variante (a) — o header do BIN era lido dos zeros de padding, binLen=0, «buffer sem URI fora de .glb» (a razão certa: o fixture tem JSON não múltiplo de 4 DE PROPÓSITO); reposto → 0 falhas + harness 340/0.

## R-023 · a exaustão dos slots de scroll matava o browser ao toque (FASE 0.9.6-MASTER · GRUPO A)

| campo | valor |
|---|---|
| ID | R-023 |
| Reportado | ACHADO AO VIVO pela FASE 12.8b do c33_virtual (o loop da campanha a apanhar bug que ninguém tinha reportado): o toque na linha do browser não despachava — 612 frames com regiões de scroll MORTAS na sessão do harness |
| Sintoma exato | após 8 regiões de scroll DIFERENTES usadas numa sessão (hierarquia, inspector, logs, scenes, uiInsp, ficheiros, consola, seletor, browser, settings, docs, script, texto, áudio — a casa tem 14), a 9.ª região nascia MORTA ao toque: sem slot = sem região = sem claim do gesto = sem scrollTap — no device, abrir Settings+Docs+Script+Texto+Áudio+Logs+Consola+Ficheiros e depois o BROWSER deixava o browser surdo (o «1 toque importa» morria em silêncio) |
| Causa raiz | (leitura do código) os 8 slots de scroll do UiContext eram DEFINITIVOS (`used = true` para sempre, desde a F4.1) — o comentário original dizia «não devia acontecer: 2 usos» e a casa cresceu para 14 |
| Fix | RECICLAGEM por frame-stamp: cada slot regista o último frame em que a sua região desenhou (`lastFrame`); um beginScroll sem slot livre rouba o slot STALE MAIS ANTIGO (região que não desenhou neste frame = overlay fechado). O preço documentado: o offset de uma região reciclada recomeça a zero quando ela volta — nunca a região fica morta (um scroll perdido < um botão morto) |
| Teste sentinela | `regress_scroll_slots_reciclados` (tests/test_sentinels.cpp): 10 regiões sequenciais (mais que os 8 slots) — a 10.ª TEM slot (o offset gravado volta); uma região antiga reciclada VOLTA A FUNCIONAR ao regressar |
| Linha do replay | FASE 12.8b do c33_virtual (a sessão inteira do harness usa 14 regiões; o browser toca e IMPORTA no fim — morto antes do fix) |
| Padrão proibido | (nenhum — a vigília é a sentinela + a FASE 12.8b) |

Prova de mutação (colada em mutacao-R023-vermelho.txt): a reciclagem DESLIGADA → `regress_scroll_slots_reciclados` FALHOU (a 10.ª região sem slot: o offset gravado era um no-OP — a razão certa); reposta → 0 falhas. NOTA honesta: a 1ª rodada da mutação correu SEM a env var da mutação ativada (verde por acidente) — REFEITA com a mutação ATIVA (a lição R-014/R-017 aplicada: a prova tem de falhar pela razão certa).

## R-024 · as ferramentas de verificação: o layout exportado não pode mentir (FASE 0.9.6-MASTER · GRUPO B)

| campo | valor |
|---|---|
| ID | R-024 |
| Reportado | cláusula da campanha FASE 0.9.6-MASTER (Grupo B · FERRAMENTAS DE VERIFICAÇÃO): os Grupos C-I precisam de MEDIR os ecrãs (sobreposições, texto cortado, toque < 48dp) — sem ferramenta, cada grupo re-inventa a medição e ninguém vê o ecrã de verdade |
| Sintoma exato | (classe de risco, não bug de device) o relatório de um ecrã era a descrição de QUEM escreveu o código — os números de layout viviam na cabeça do autor; no CI, os testes de UI liam os BATCHES (verdade geométrica) mas ninguém via o PIXEL; a sentinela nova apanhou DOIS bugs vivos no 1º run: (1) os botões «Exportar layout»/«Auditoria do ecrã» desenhavam mas o WALK do tap do SettingsPage não os re-despachava — NATIVOS MORTOS ao toque (a mesma classe do bug 0.9.1: desenhar ≠ tocar); (2) o validador assinalava as linhas de scroll scrolled-out como «fora do ecrã» (falso positivo) |
| Causa raiz | (dos bugs apanhados) (1) o walk de re-despacho do scrollTap em SettingsPage.cpp tinha a lista de botões DUPLICADA à mão (draw + walk) — os dois botões novos entraram no draw e não no walk; (2) o validador não distinguia conteúdo de scroll (clip) de widget solto |
| Fix | (a) o FRAMEBUFFER REAL no C33 virtual (tests/stub/glstub_fb.h + ramos fb no gl3.h): com `glstub::fb::enabled` o stub deixa de ser no-op e RASTERIZA a sério (RGBA8+depth, os DOIS shaders da casa + a grelha adaptativa com fwidth analítico, TRIANGLE_STRIP incluído) — `enabled=false` (default) = o no-op de sempre, ZERO mudança para os testes existentes; (b) o REGISTO do layout (ui/LayoutDump.h/.cpp): cada widget do UiContext (o choke point) regista o rect REAL que desenhou — o JSON exportado sai do MESMO código que desenha (a lição R-020); (c) o VALIDADOR puro com 6 regras (fora_do_ecra, sobreposto, toque_pequeno <48dp, texto_truncado, texto_sangra, rect_degenerado) — a exceção do clip (linhas scrolled-out são desenho, não defeito); (d) o EXPORT no fim do frame (layoutDumpIfPending): layout/<ecrã>.png (backbuffer full-res) + .json (as entradas) + auditoria-<ecrã>.txt — no device pelo Diagnóstico («Exportar layout»/«Auditoria do ecrã» — que FECHAM o settings e auditam o ecrã por baixo, um ecrã de cada vez), no harness pela FASE 13; (e) o walk do SettingsPage ganhou as duas linhas dos botões novos |
| Teste sentinela | `regress_layout_dump_nao_mente` (o registo é o draw: contagem exata, guard dos compostos, truncagem com fullW, filhos do scroll clipped, JSON determinístico e re-parsável) + `regress_validador_apana` (UM caso plantado POR REGRA + registo limpo fica limpo + as duas exceções) + `regress_fb_rasteriza` (o quad rasteriza no sítio certo, borda dura, scissor corta) + `regress_png_layout_ida_e_volta` (encodePngRgb→loadPng byte a byte a 1536×720) |
| Linha do replay | FASE 13 do c33_virtual (340→374 checks): editor/script(portrait com teclado)/docs/browser exportados; os PNGs RELIDOS pelo loadPng de produção; o píxel CONFIRMA o registo (o maior painel no sítio, a toolbar povoada >30%, os glifos do atlas); a auditoria pelo CAMINHO DO DEVICE (o tap no botão do Diagnóstico — que usa o rect REAL do registo, zero fórmulas de layout que driftam) |
| Padrão proibido | (nenhum — a vigília é as 4 sentinelas + a FASE 13) |

Provas de mutação (coladas em mutacao-R024a/b/c-vermelho.txt): (a) o `Record::add` virado no-op → `regress_layout_dump_nao_mente` + `regress_validador_apana` FALHARAM e a FASE 13 ficou com 8 vermelhos (o JSON exportado ficava VAZIO e a auditoria dizia «VERDE» por não ver nada — a razão certa); (b) a regra Sobreposto REMOVIDA → `regress_validador_apana` FALHOU em hasRule(Sobreposto) — nota honesta: a remoção NÃO parte mais nada (o perigo de um validador que «meio funciona»); (c) o rasterizador morto (return no drawCore) → `regress_fb_rasteriza` FALHOU (o quad não está no píxel) + FASE 13 com 3 vermelhos (PNG vazio — só o clear). NOTA honesta: um 2.º run da mutação (c) mostrou 5 falhas extra na FASE 12.8b — era o FLAKE do /tmp com fixtures stale (a lição da FASE 9: «o flake do disco não é regressão»); com /tmp limpo o verde volta por inteiro. Reposições → 0 falhas + 374/0.

O ESTADO ATUAL MEDIDO (a linha de base dos Grupos C-I, colada no docs/RELATORIO-0.9.6.5-GRUPO-B.md): o editor tem 1 ERRO (label que sangra 3px o fundo do ecrã em [8,694]) e 3 avisos (2 botões de 40px < 48dp + 1 label truncada); o browser tem 8 avisos de toque < 48dp (as linhas de 40px e a raiz de 36px); o docs e o script (152 entradas, 54 teclas) estão LIMPOS de erros.

## R-025 · a escala única: o texto era o atlas cru em qualquer densidade (FASE 0.9.6-MASTER · GRUPO C)

| campo | valor |
|---|---|
| ID | R-025 |
| Reportado | a linha de base MEDIDA pelo Grupo B (RELATORIO-0.9.6.5 secção 8 — a vara de medir do R-024 a trabalhar): o editor tinha 1 ERRO (`label SANGRA o contentRect: [8,694 166x29] vs ecrã 1512x696+0,24` — a legenda «FPS · TICs» da status bar com o bloco a descer até y=723 num ecrã que acaba em 720) e avisos `botao(id 28) 56x40` / `botao(id 157c) 268x40` / `label truncada 353px`; o browser 8 avisos < 48dp (fechar 96×36, 6 raízes 139,7×40, subir 868×44); o script 2 truncadas («ESPACO»/«ENTER» nas teclas); ERRATA do relatório B: o título 20sp do Docs e do Script TAMBÉM sangrava o TOPO do contentRect (3,9px) — a contagem manual do Grupo B não viu (o validador via; os JSONs estavam lá) |
| Causa raiz | a mesma classe em TUDO: (1) o TEXTO não tinha escala de densidade — o atlas é assado a 28px (= 14sp @2.0 do C33) e desenhado CRU: no device (2.0) ficava certo POR ACASO, no harness (1.0) saía 2× desproporcional (o bloco de 29px numa banda de 24dp sangrava; os títulos 20sp de 41px não cabiam no cabeçalho de 56dp e o fallback de baselines FIXAS empurrava-os PARA FORA do contentRect); (2) meia dúzia de ALVOS em px crus que a R-018 não cobrira (o [+] 56×40 e a pesquisa 40px da hierarquia, o fechar/raízes/subir do browser, as actionRows 152×40 do Settings, as linhas do Inspector derivadas do bloco cru ~18dp REAIS no device — a exata medição do dono «teclas 48×65px»); (3) o button() media o texto pelo ATLAS CRU (`font_->widthOf`) e desenhava pela escala do contexto — truncava/centrava errado no viewport 2D e em densidade ≠2 |
| Fix | (a) **sp()/textK() na fonte única** (ui/Theme.h): `sp(v) = v × densidade` e `textK() = densidade/2` (o fator do atlas de 28px); o UiContext aplica textK() no CHOKE POINT (fontWidth/fontHeight/textMetrics/labelStyled/labelFitted) — a escala do texto tem UM só dono; a 2.0 o k é 1 (o device de sempre, ZERO mudança visual onde o dono olha), a 1.0 o corpo são 14px (o ecrã deixa de ter texto 2× desproporcional); (b) `labelFittedStyled` — o FIT com tipografia (a legenda 12sp da status bar nunca sai do rect); a status bar desenha a 12sp REAL (o comentário antigo DIZIA «12sp» e o código chamava o label de CORPO — o ERRO medido) com a baseline centrada pelas métricas do contexto; (c) os ALVOS a 48dp REAL: [+] e pesquisa da hierarquia (comentário já dizia «48dp»), fechar/raízes/subir do browser, actionRows do Settings (kRowH inteiro), o cancelar do import; as LINHAS do Inspector com piso `kRowH` (48dp) e paddings dp; (d) o dp() nos crus que restavam: hierarquia (ícones/zonas/recuos/convite), Inspector (caixas X/Y/Z 64×48, trilho 84/118, R 48), teclas ESPACO/ENTER/TAB a 12sp (o rótulo de CORPO truncava), kErrH/kHelpStrip do editor de script, as tabs do painel de baixo (o ícone/gap 24/8 crus — a invariância da 13.6 apanhou: o rótulo deslocava 16px a 2.0), o Settings inteiro (kRowH/kSectionH eram constexpr px), os 4 CARDS modais com raios 8dp (spec A) e o button() no CHOKE POINT com panelRounded/frameRounded — TODOS os botões da app ganham cantos suavizados num só sítio; (e) o button() mede pelo CONTEXTO (o bug medir≠desenhar) |
| Teste sentinela | `regress_sp_escala_unica` (o contrato sp/textK + o choke point fontWidth/textMetrics dobram com a densidade + o bloco da legenda CABE na banda em 1.0/2.0/3.0) + `regress_toque_48dp_validador` (o validador do R-024 como VARA: hierarquia+browser desenhados com audit a 1.0 E 2.0 — zero toque_pequeno; o [+] e a pesquisa ≥48; actionBtnRect exportada e ≥48 nas duas densidades) + `regress_script_geom_unica` (ver C2 abaixo) + `regress_cantos_suavizados` (o button() emite a escadaria + arco — mais geometria que o panel reto) |
| Linha do replay | FASE 13 do c33_virtual (374→386): 13.2 o editor VERDE (o ERRO e os avisos da linha de base CURADOS); 13.3 o CULLING (800 linhas → o frame tokeniza ~50) + o roundtrip da geometria única; 13.6 A DUPLA DENSIDADE (o NÃO VERIFICADO #4 do relatório B fechado): o export a 2.0 (superfície 3072×1440, insets ×2, AConfiguration 320dpi) é o ecrã a 1.0 VISTO A 2× — entrada a entrada, dp E sp (o validador inteiro passa; a largura do TEXTO dobra) |
| Padrão proibido | (nenhum — a vigília é as 4 sentinelas + a FASE 13.2/13.6) |

Provas de mutação (coladas em mutacao-R025a/b/c-vermelho.txt): (a) o `textK()` com a densidade CEGA (`return 1.0f`) → `regress_sp_escala_unica` FALHOU em CINCO asserções (o contrato, o choke point e o bloco da legenda) e a FASE 13 perdeu o VERDE do editor e a invariância/sp da 13.6 (o ERRO da status bar VOLTOU — a razão certa); (b) o CULLING desligado (o loop volta a TODAS as linhas) → a 13.3 FALHOU (800 tokenizadas — a prova do perf); (c) o [+] de volta a 56×40 px crus → `regress_toque_48dp_validador` FALHOU nas DUAS densidades (o validador viu o <48dp) e a 13.2/13.6 perderam o VERDE e a invariância (o 40px não dobra). Reposições → 838/0 + 386/0.

### R-025b · a geometria linha→y/col→x ÚNICA do editor de script (+ o índice O(1) e o culling)

A fórmula «dp(8) + linha·lh a partir de body.y» vivia em TRÊS sítios à mão (o draw das linhas, o bloco do toque e o scroll-segue-caret) — a classe exata do R-019 (o toque calculava por uma cópia e o draw por outra). AGORA as funções PURAS vivem no header (`lineTopOnScreen`/`lineAtScreenY` — UM o inverso do outro; `codeX`/`caretInset` partilhados) e o draw, o toque e a FASE 12.11 consomem-nas. O PERF: o buffer era varrido 3×/frame (lineCount, a linha do caret, o split) e o tokenizador corria TODAS as linhas (O(buffer) por frame — o custo crescia com o script); AGORA o ÍNDICE de linhas (`ensureLineIndex` — reconstruído SÓ quando o buffer muda; `bufVersion` bumped em cada mutação: insert/apagar/Tab/SUBSTITUIR/dica/open) dá lineCount O(1), a linha do caret O(log n) e o início de linha O(1); o estado do comentário de bloco vem do cache por linha (pago por EDIÇÃO, não por frame); o draw SÓ TOKENIZA AS VISÍVEIS (`dbgLinesTokenized` — a prova afervável). A FASE 12.11 foi RECALIBRADA para consumir as MESMAS funções (os números «34»/«64» hardcoded driftavam — a lição aplicada ao próprio teste). Sentinela: `regress_script_geom_unica` (o roundtrip, o índice vs a varredura manual, a invalidação por versão, o guard de sanidade). Mutação (b) colada.

## R-026 · o orçamento do editor 3D no device: a gangorra dos divisores, a barra de toque que não cabe e o scissor que cortava o FOV (FASE 0.9.6-MASTER · GRUPO D)

| campo | valor |
|---|---|
| ID | R-026 |
| Reportado | cláusula da campanha FASE 0.9.6-MASTER (Grupo D · ORÇAMENTO DO EDITOR 3D): «topo/abas/FPS; hierarquia\|viewport\|inspector com divisores arrastáveis; barra de toque; scissor» |
| Sintoma exato | (medido pela FASE 13.7 nova — a vara ao TAMANHO do RMX3624: 1600×720@2.0 = 776×336dp) o HARNESS largo (1512dp) escondia tudo: (1) os painéis FIXOS de 300dp deixavam o viewport 3D a **176dp (22% do ecrã)**; (2) o stack vertical (5×48+4×8+8 = 280dp) TRANBORDAVA a altura (~184dp) por cima da toolbar inferior; (3) o [+] (56dp) caía POR CIMA dos botões de ferramenta (a toolbar 272dp + [+] > qualquer viewport que sobrasse); (4) o drawer default (240dp) comia o editor INTEIRO (painéis a altura 0); (5) os 3 botões de ícone da top bar mediam 47dp (a «escala graciosa» comprimia-os abaixo do alvo da casa); e em QUALQUER ecrã (6) o pass 3D projetava com o ASPECTO DO ECRÃ INTEIRO e o scissor CORTAVA a faixa central — o dono via ~22% do FOV horizontal no device (o visível era o recorte do meio de um ecrã largo) |
| Causa raiz | (leitura de código + medida) o layout do editor nasceu num ecrã de 1512dp e TODAS as medidas eram FIXAS em dp: kPanelW=300 inalterável, o chrome do viewport (stack em 1 coluna, [+] no canto inferior direito) sem adaptação, o drawer com teto 400dp absoluto, a top bar com escala que comprimia os ALVOS de 48dp — e a câmara do editor com `proj(w/h)` do ECRÃ (a 0.9.6.1 ligou o scissor ao centerRect mas o aspect continuou o do ecrã: o recorte). O orçamento era uma GANGORRA sem regras: ninguém garantia que hierarquia+inspector+viewport coubessem |
| Fix | (a) **resolvePanels()** (ui/SafeArea.h — a fonte ÚNICA): a gangorra dos TRÊS pisos — kHierMinW 200dp / kInspMinW 272dp (a linha X/Y/Z com caixas de 56dp) / kViewportMinW 288dp (a toolbar 272+margens); defaults ASSIMÉTRICOS no aperto (o inspector mantém o kPanelW, a hierarquia ABSORVE — os nomes truncam com tip; no device: hier 200 \| vp 288 \| insp 288); o drag de cada divisor clampa contra a largura EFETIVA do outro painel (o par nunca fecha o viewport); (b) **os divisores arrastáveis** (o padrão da pega do drawer): strip 12dp + hit 20dp na borda do painel, press arma (âncoras), o move redimensiona AO VIVO (snap 8dp, guard ≥0 — achado ao vivo), release fixa; o INPUT corre ANTES dos painéis no UI pass (a pega RECLAMA o gesto: nunca vira scroll/orbit/gizmo) e o DRAW depois (o conteúdo NÃO perde largura); PERSISTE no layout.json (hierW/inspW — retrocompatível); (c) **a barra de toque adaptativa** (vpchrome::layout): o stack em COLUNAS (1 nos largos — zero mudança; 2/3 nos curtos; ESCONDE no sub-mínimo — degradação honesta), o [+] sobe ao canto sup-dir quando a toolbar enche a largura; a linha X/Y/Z com caixas 64→56dp e o R ao lado do título em painel estreito; a top bar: os botões de ÍCONE nunca encolhem (o k divide só Menu/Cena/tabs); o convite do vazio curto em painel <240dp; (d) **o drawer nunca come o editor** (clamp vpH−64dp no BottomPanel::layout — deixa a faixa da toolbar viva); (e) **o scissor/aspect do RECT**: a câmara do editor projeta com o aspect DO RECT (glViewport+glScissor do rect; beginUiPass repõe o cheio), e todo o mundo↔ecrã (gizmo/frustum/glifos de áudio/pickSceneTic/tap-de-seleção/feedGizmo) mapeia RETO-LOCAL (vw,vh,ox,oy — defaults = o ecrã todo: os testes e o Play ficam IGUAIS); o frustum do camgizmo mantém o aspect do JOGO (em Play a câmara renderiza o ecrã todo) |
| Teste sentinela | `regress_orcamento_gangorra` (os defaults assimétricos + a varredura do par de drags: o viewport NUNCA fecha + os pisos + o clamp do drawer) + `regress_divisores_arrastaveis` (as pegas armam/arrastam/clampam/release; a persistência round-trip + o formato antigo; o chrome adaptativo: colunas, [+] topo, alvos ≥48dp DENTRO, a degradação) + `regress_scissor_aspecto_rect` (o NDC mapeia ao rect+origem; a PROVA DO CORTE: o ponto da borda do frustum da janela cai DENTRO com o aspect do rect e FORA com o do ecrã; o pick reto-local coerente com o draw) |
| Linha do replay | FASE 13.7 do c33_virtual (386→408 checks): o editor ao TAMANHO do RMX3624 — o validador INTEIRO verde (com os problemas NA MENSAGEM), a gangorra medida, o chrome adaptativo, os divisores AO VIVO pelo caminho real do input, a persistência; os PNGs+JSONs sobem como artefactos (layout-harness-editor-device.*) |
| Padrão proibido | (nenhum — a vigília é as 3 sentinelas + a FASE 13.7) |

Provas de mutação (coladas em mutacao-R026a/b/c-vermelho.txt): (a) o piso do viewport desligado (vpMin=0 na resolvePanels) → `regress_orcamento_gangorra` FALHOU (a hierarquia default não absorve; o viewport do device fecha; a varredura do par aponta) e **656 testes** no total — o piso é LOAD-BEARING em todo o layout do editor; (b) o drag dos divisores morto → `regress_divisores_arrastaveis` FALHOU no resize vivo (a pega arma mas o painel não mexe — a razão certa); (c) o projectPoint CEGO a (ox,oy) → `regress_scissor_aspecto_rect` FALHOU (o ponto do centro da vista deixa de cair no centro do RECT — o desenho 3D escaparia à janela). Reposições → **841/0 + 408/0**.

## R-027 · o editor de script sem saber onde está o IME, o teclado da engine a dobrar e o header com botões fora do ecrã (FASE 0.9.6-MASTER · GRUPO E)

| campo | valor |
|---|---|
| ID | R-027 |
| Reportado | cláusula da campanha FASE 0.9.6-MASTER (Grupo E · EDITOR DE SCRIPT + SÍMBOLOS): «header flexível; IME; barra de símbolos 40dp sobre o IME (teclado da engine REMOVIDO); R-018/R-010» |
| Sintoma exato | (medido por leitura de código + a FASE 13.8 nova) TRÊS defeitos da mesma família: (1) a engine NÃO SABIA onde o IME do Android está — o manifest não usa adjustResize (o teclado SOBREPÕE a superfície) e NENHUM caminho media a faixa: o CARET ficava por baixo do teclado enquanto o dono digitava (o defeito do textWin 0.9.1, herdado); (2) o teclado IN-APP da engine (280dp/54 teclas, QWERTY + página 123 + long-press de acentos + shift + setas) competia com o IME do sistema — a «política de coexistência» 0.9.6 G3 eram DOIS teclados para manter e o dono pagava a confusão; (3) o header media os botões A PARTIR DA LUPA (docsX−200/−152/−56): no device portrait (360dp de conteúdo) o botão do nível caía a **−56dp** e o do teclado a **−8dp** — DOIS botões FORA DO ECRÃ (invisíveis e intocáveis desde 0.9.6) |
| Causa raiz | (1) inexistência da ponte: nenhuma medição do IME chegava ao nativo (o contentRect não muda sem adjustResize; o InputMethodManager não dá altura por si); (2) duplicação histórica: o teclado in-app nasceu na FASE 9 (o IME do sistema era frágil no NativeActivity 0.9.1) e a política G3 transformou o custo temporário em permanente; (3) o header desenhava com offsets a partir de UM botão (docsX) numa faixa de IDs que cresceu de 3 para 6 botões sem nunca rever a ordem — a largura do device nunca entrou na conta (a mesma classe do R-026: medidas pensadas para o ecrã do harness) |
| Fix | (a) **A PONTE DO INSET DO IME**: a VvActivity MEDE a faixa real (listener de layout global; `rootHeight − visibleDisplayFrame.bottom` — a técnica clássica API 24→34, sem WindowInsets.Type; o piso rootH/7 separa a nav bar do IME) e empurra SÓ MUDANÇAS por `nativeOnImeInset` (JNI novo na tabela RegisterNatives) → `ime::setBottomInset/bottomInset/insetVisible` (o mesmo mutex da fila do IME; log UMA linha por mudança); (b) **A BARRA DE SÍMBOLOS de 40dp** (a spec E): os 22 símbolos da linguagem ({ } ( ) [ ] = + - * / < > ! , . ; : " _ # @ — o set da página «123» do teclado antigo, SEM os extras que o GBoard já tem) em páginas ADAPTATIVAS (a 1ª tecla é o seletor «1/2» — o padrão ?123 do GBoard; 9 teclas no device 360dp → 3 páginas, 14 no harness 720dp → 2; teclas sempre ≥40dp de largura), dokada em `h − inset − 40dp` (SOBRE o IME real); as teclas emitem pelo MESMO `applyEvent` do IME (a fonte única mantida) com LOG por símbolo e por página; (c) **o TECLADO DA ENGINE REMOVIDO** (drawKeyboard/kKbLetters/kKbSymbols/QWERTY/long-press/shift/setas/kbOpen…/kKbToggleId/result 7): o IME do sistema é o teclado ÚNICO — o rastreio do «desenhar ≠ tocar» morre com o teclado; (d) **O HEADER FLEXÍVEL**: os botões ancoram À DIREITA (stop, run, lupa, copiar, nível — nesta ordem) e o TÍTULO FLEXIONA (o subtítulo some <120dp, o título <48dp; no device 360dp o header é Back + 5 botões — ZERO sobreposição); Run/Stop 72→48dp de largura nos estreitos (<420dp; os alvos ≥48dp mantêm-se pela altura); (e) **o corpo reserva o IME**: `bottomLim = imeIns > 0 ? h−imeIns : h−ins.bottom` (o inset JÁ inclui a nav bar — não se soma 2×; IME fechado = layout PIXEL-IGUAL ao de sempre) — o corpo, a strip de ajuda, a barra de erro e a barra de símbolos param TODOS acima do teclado (o scroll-segue-caret faz o resto: o caret nunca fica escondido); (f) **A EXCEÇÃO COMPACTA do validador** (POLÍTICA #5 — a spec do autor manda 40dp e a casa manda 48): `Entry.compact` + `buttonCompact()` (a flag vive SÓ durante a chamada); o ToquePequeno aplica o piso 40dp às compactas e 48dp às regulares — a mensagem diz o piso que falhou, o JSON diz «compacto»; a exceção é ESTREITA e vigiada (compacta a 39dp FALHA; botão REGULAR a 40dp FALHA — a sentinela prova os dois lados) |
| Teste sentinela | `regress_barra_simbolos_ime` (tests/test_sentinels.cpp — 5 frentes): (1) a ponte do inset (set/get/insetVisible/clearForTest); (2) a geometria (40dp exatos, 9/14 teclas, 3/2 páginas, os 22 símbolos da spec primeiro/último, a densidade 2.0 dobra); (3) o DRAW REAL auditado (a barra dokada sobre o IME com 14 teclas compactas ≥40dp; o validador 0 problemas; a tecla '{' insere NO CARET pelo applyEvent; o seletor cicla e a página 2 começa em '!'; IME fechado = SEM barra no registo); (4) a exceção ESTREITA (compacta 40dp passa / compacta 39dp FALHA / regular 40dp FALHA / regular 48dp passa); (5) o header ao TAMANHO do device (720×1600@2.0: ≥5 alvos 48dp inteiros, NADA fora do ecrã, a barra 9 teclas dokada em h−880−80px, o validador 0) |
| Linha do replay | FASE 13 do c33_virtual (408→428 checks): o 9.x (a tecla '{' da barra entra NO CARET — o caminho do device), o 12.3 (a barra dokada sobre o IME injetado: 40dp exatos, o símbolo no sítio), o 12.7 REESCRITO (as 2 páginas cobrem os 22 símbolos; o seletor cicla; os LOGs; IME fechado = a barra SOME), o 12.10 RECALIBRADO (keyboardHeight morreu — a vara afere symbolBarHeight 80px @2.0 e as teclas ≥40dp), o 13.3 REESCRITO (o export com o IME: o validador 0/0, as teclas compactas no JSON, o teclado antigo AUSENTE do registo) e a 13.8 NOVA (o script ao TAMANHO do device: 720×1600@2.0, insets T48/B48, IME 880px — o validador 0/0, o header ≥5 alvos 48dp DENTRO com o título flexionado, a barra 9 teclas dokada sobre o IME, 3 páginas); os PNGs+JSONs sobem como artefactos (layout-harness-script.png/json + layout-harness-script-device.png/json) |
| Padrão proibido | (nenhum — a vigília é a sentinela + a FASE 12.3/12.7/13.3/13.8) |

Provas de mutação (coladas em mutacao-R027a/b/c/d-vermelho.txt): (a) **a tecla MORTA** (o applyEvent desligado) → `regress_barra_simbolos_ime` FALHOU em 4 asserções (as teclas mortas nem auditam; o buffer intocado) + o wiring092 (a barra não escreve) — o par desenhar≠tocar vigiado de novo; (b) **o PISO COMPACTO morto** (minCompact 40→30) → `regress_barra_simbolos_ime` FALHOU na tecla compacta de 39dp (o piso da spec E é VIGIADO — a exceção não é desculpa para medir menos); (c) **o corpo CEGO ao inset** (bottomLim ignora o IME) → `regress_barra_simbolos_ime` FALHOU na posição da barra + **8 checks do c33** (a tecla desenhada DEBAIXO do teclado é INTOCÁVEL — o tap cai no corpo: a lição do R-014 aplicada ao IME; o 12.3/12.7/13.3 vermelhos); (d) **o header flexível morto** (narrow=false) → `regress_barra_simbolos_ime` FALHOU no validador do device (os botões FORA do contentRect — o ForaDoEcra) + a 13.8 vermelha (o defeito exato dos DOIS botões invisíveis do header antigo, apanhado ao vivo). Reposições → **842/0 + 428/428**.

RECALIBRADOS pela mudança legítima (documentado em cada sítio): `regress_density_escala_dp` (R-018 — o keyboardHeight morreu com o teclado: a vara afere a BARRA: 40dp→80px @2.0), `regress_r010_editor_roundtrip` (o draw com o teclado aberto → o draw com o IME injetado), `scriptwin_teclado_in_app_digitavel` (o repertório é o da BARRA), `scriptwin_toque_no_corpo…`/`scriptwin_botao_nivel…` (as posições do header flexível: helpX=388/copyX=444/lupaX=500 @720dp), `jni_onload_registers…` (6→7 natives com a expect do nativeOnImeInset), o wiring010 NÃO mudou (o flake do /tmp da FASE 9 apareceu a meio das mutações — limpo, verde por inteiro).

## R-028 · a identidade: o tema mono perdido, o vidro inexistente, o clear fóssil e o ícone órfão (FASE 0.9.6-MASTER · GRUPO F)

| campo | valor |
|---|---|
| ID | R-028 |
| Reportado | cláusula da campanha FASE 0.9.6-MASTER (Grupo F · IDENTIDADE): «tokens mono+vidro (R-020 de tema); ícone G com 4 setas» |
| Sintoma exato | (medido por leitura de código + a FASE 13.9 nova) QUATRO defeitos da mesma família: (1) o TEMA MONO ORIGINAL DA CASA tinha-se perdido — a app nasceu F1 com a rampa NEUTRA (#141414/#1E1E1E/#2E2E2E/#E6E6E6/#F5F5F5, «tokens mono» de f4.1) e a spec A 0.9.0 trocou-a pela família NAVY + o accent AZUL #2196F3; (2) o VIDRO não existia — as superfícies eram TODAS opacas (α=1) apesar do pass UI inteiro desenhar com GL_BLEND desde a F1 (o scrim 60% e o toast 0.95 eram os únicos vidros artesanais); (3) O CLEAR ERA UM FÓSSIL — `glClearColor(0.0784…)` era o literal #141414 DA F1 que sobreviveu à spec A (o bg token passou a #0B0E13 mas o clear NUNCA acompanhou — invisível enquanto tudo era opaco; com vidro, o fundo por trás dos painéis fica À VISTA); (4) O ÍCONE ERA ÓRFÃO — o gerador (scripts-local/gen_app_icon.py) NUNCA esteve no repo: os PNGs do «G com a lâmpada» eram irreproduzíveis (dívida de processo), e o ícone novo pedido («G com 4 setas») não tinha onde nascer |
| Causa raiz | (1) a spec A substituiu a identidade em vez de a estender (o accent virou COR e a rampa virou navy — a tabela única existia PARA o flip, mas ninguém a usou); (2) o alpha dos tokens nunca foi parte do contrato (a struct Theme carregava f32[4] mas todos os tokens nasciam com a=1 — o blend do pass era usado só por exceções); (3) o clear era anterior à própria tabela (F1: o literal era o bg; 0.9.0: a tabela mudou, o literal ficou); (4) o gerador viveu sempre fora do controlo de versões (scripts-local/ nunca foi commitado) |
| Fix | (a) **A TABELA MONO+VIDRO** (ui/Theme.h, spec F): o REGRESSO à rampa neutra F1 com a estrutura da spec A — bg #141414 α1 · surface #1E1E1E **α0.88 (o vidro)** · surface2 #262626 α0.92 (denso) · border #2E2E2E α0.55 (hairline) · text1 #E6E6E6 · text2 #A6A6A6 · accent #F5F5F5 (O MONO — o branco da F1) · accentPress #DADADA · **accentInk #141414 (a tinta ESCURA sobre o fill branco — a spec A tinha accentInk=text1: com accent mono seria BRANCO SOBRE BRANCO)** · danger/warn/ok (semânticas — as exceções documentadas de sempre) · scrim 60% · a paleta V.ONI intacta com voniComment UM DEGRAU ACIMA (#757575→#8A8A8A: o bg novo é mais claro que o navy e o piso 4,5:1 mantém-se, 4,0→5,3:1); (b) **A AUDITORIA DO VIDRO** (o padrão honesto do F): o pior caso de um painel α0.88 é uma cena BRANCA PURA por trás — `blendOver()`/`contrastOnGlass()` PURAS no Theme.h provam que os pisos da casa SEGURAM lá (text1 9,25:1 · text2 4,74:1 · accent 10,59:1 · danger 3,31:1 · warn 6,74:1 · ok 4,88:1); (c) **O CLEAR LÊ O TOKEN** (Renderer.cpp: o literal morre; com o mono de volta o VALOR é o mesmo #141414 — mas por CONTRATO, não por fóssil); (d) **OS CONSUMIDORES**: os aliases do UiContext.h com os alphas + static_assert do ACCENT; o toast compõe o alpha DO TOKEN com o fade; o Java espelha (ACCENT_INK novo — o Button24 pintava TEXT1 branco sobre o fill ACCENT: branco sobre branco; SURFACE_GLA/SURFACE2_GLA: os cards/campos da tela de projetos ganham o véu); o tint do bigLogo REMOVIDO (o ícone novo já é mono); (e) **O ÍCONE G COM 4 SETAS** (scripts/gen_app_icon.py — COMMITADO no repo): o G branco #F5F5F5 (as proporções do G aprovado) + as 4 setas do Move (o verbo primeiro do editor, o MESMO motivo do ícone Move do conjunto da casa) em #B5B5B5 sobre o bg mono #141414 — vetor puro, zero gradientes, o mestre 512 + as 5 mipmaps (48/72/96/144/192) + o gone_logo; (f) **O WIPE DO STUB** (o achado ao vivo — ver a FASE 13.9): desde o Grupo D o allocFb_ do stub REALOCAVA (APAGAVA) o framebuffer a CADA glViewport de tamanho diferente — o glViewport(912×568) do pass 3D apagava o CLEAR e o glViewport(1536×720) do beginUiPass apagava o PASS 3D INTEIRO: TODOS os PNGs exportados desde a 0.9.6.7 mostravam a UI sobre PRETO (invisível: os painéis eram opacos e nenhuma check aferia conteúdo 3D); AGORA o framebuffer é da SUPERFÍCIE EGL (eglCreateWindowSurface → fb::setSurface — realloc só quando o ecrã muda a sério) e o glViewport SÓ regista a transformação |
| Teste sentinela | `regress_identidade_mono_vidro` (tests/test_sentinels.cpp — 6 frentes): (1) a TABELA mono a sério (os 9 tokens de chrome NEUTROS, ΔR=G=B≤2/255; o accent É o branco F1 #F5F5F5; a rampa de volta; accentPress mais escuro); (2) O VIDRO (os alphas 0.88/0.92/0.55/1.0 + os pisos no PIOR CASO: text1/text2 ≥4,5:1 e accent/danger/ok/warn ≥3:1 sobre o vidro-no-branco — contrastOnGlass); (3) A TINTA ESCURA sobre o fill branco (accentInk <0.5 e ≥4,5:1 sobre accent); (4) A PALETA V.ONI no bg novo (os 6 tokens ≥4,5:1 — voniComment no degrau #8A8A8A); (5) O BLEND PURO (blendOver: 50% cinza sobre branco = 0.75; o composto do painel = 28.8/255 — o valor que a 13.9 afere no PNG); (6) OS ALIASES ligados (PANEL/LINE/ACCENT bit-a-bit com a tabela) |
| Linha do replay | FASE 13.9 do c33_virtual (428→440 checks): (1) O MONO no PNG — a paleta VELHA inteira (o azul #2196F3, o accentPress azul, a família navy, o text2 azulado) AUSENTE do ecrã exportado (o azul do eixo Z do gizmo #4F92F5 é OUTRO azul — a exceção documentada, vive); (2) O CLEAR LÊ O TOKEN — o céu da viewport é o bg #141414 (não o preto do wipe); (3) O VIDRO — o painel da hierarquia PELO REGISTO é o composto blend(surface@0.88, bg)=(29,29,29) EXATO (±1) e NÃO a surface sólida (30); (4) O CHIP flutuante sobre a viewport — o MESMO composto (a cena aparece por trás: o vidro REAL); (5) o validador 0/0 re-vestido (o tema não mexe em rects); (6) A DUPLA DENSIDADE — o painel a 2.0 é o MESMO composto (as cores não escalam); RECALIBRADOS com nota: o 13.1 (o pixel do maior painel agora É a matemática do vidro; a banda da toolbar — o piso 30% contava o bg navy ≠ clear como «povoado» de graça: com o mono a banda pinta o TOKEN bg que É o clear — a população é o CONTEÚDO ≈5,5% medidos, piso novo 3% = o mesmo contrato «não vazia») |
| Padrão proibido | `glClearColor(literal)` (o clear lê o token — o fóssil morreu); `allocFb_` por glViewport no stub (o framebuffer é da SUPERFÍCIE — o wipe apagava clear E pass 3D) |

Provas de mutação (coladas em mutacao-R028a/b/c/d/e-vermelho.txt): (a) **O AZUL DE VOLTA** (accent→#2196F3 em Theme.h + o alias — a 1ª tentativa só no Theme.h MORREU no static_assert do UiContext.h: a ligação bit-a-bit FUNCIONA) → `regress_identidade_mono_vidro` FALHOU no croma (ΔR−B=210/255) + o branco exato + `theme_tabela_spec_f_exata` + `theme_contraste_auditoria_spec` + o toast — **11 testes**; (b) **O VIDRO MORTO** (surface α→1.0) → a sentinela FALHOU no alpha + a tabela exata + **a 13.9 FALHOU no pixel** (o painel desenharia 30 sólido — o translúcido é vigiado no PIXEL, não só na tabela); (c) **A TINTA CLARA de volta** (accentInk→branco) → a sentinela FALHOU em accentInk<0.5 E no contraste 1:1 (o branco-sobre-branco, o bug que o Java também tinha); (d) **O CLEAR LITERAL de volta** (0.05 hardcoded) → **a 13.9 FALHOU no céu** (o clear não segue o token) + o 13.1 (o composto do painel deslizou); (e) **O WIPE DO STUB de volta** (allocFb_ por viewport) → **CINCO checks vermelhos** (o céu preto, o composto do painel, o chip flutuante, o 2.0 E o 13.1) — o comportamento REAL do stub desde o Grupo D, apanhado pela primeira vez porque o vidro pôs o dst À VISTA. Reposições → **843/0 + 440/440**.

RECALIBRADOS pela mudança legítima (nota no sítio): `theme_tabela_spec_a_exata`→`theme_tabela_spec_f_exata` (test_toolbar: os valores EXATOS da spec F + o MONO Δ=0 + os alphas + voniComment), `theme_contraste_auditoria_spec` (+contrastOnGlass + a tinta), o wiring090 contrastes (re-validam sozinhos), `projects_ui_check.py` (TOKENS_F + ACCENT_INK + o rótulo «logo G com 4 setas»), o 13.1 (a matemática do vidro + o piso de conteúdo 3%), o comentário do manifest (o ícone novo + o gerador NO REPO).

## R-029 · o caret desenhado desalinhado relativamente ao texto da linha tocada (FASE 0.9.6-MASTER · GRUPO UI · E5)

| campo | valor |
|---|---|
| ID | R-029 |
| Reportado | device (o dono: «Ao tocar na linha N, a inserção acontece na linha N (hit-test correto) mas o caret visível fica deslocado para baixo relativamente ao texto da linha N») |
| Sintoma exato | o caret piscante aparecia ~meia linha ABAIXO dos glifos da linha («linha 3,5»); a inserção acertava na linha certa — só o DESENHO mentia |
| Causa raiz | (leitura P-04 do draw) o TEXTO desenhava com a BASELINE no `lineTopOnScreen` (os glifos pendiam ACIMA da banda: [top−21, top+5]) e o CARET desenhava `ui.panel(x, lineTop, w, lh−8)` (banda [top, top+20]) — DUAS bandas diferentes para a mesma linha; o caret ficava ~18px abaixo do centro dos glifos |
| Fix | a função ÚNICA `lineBaselineOnScreen(bodyY, i, lh, off, asc, desc)` = lineTop + (lh−bloco)/2 + ascent — os glifos CENTRADOS na banda; o nº de linha 12sp centrado na MESMA banda; o caret desenha de lineTop a lineTop+lineHeight (`caretRectOnScreen` partilha o contrato) |
| Teste sentinela | `regress_caret_na_banda_dos_glifos` (tests/test_sentinels.cpp): em TODA a linha (com e SEM scroll) o caretRect é a banda [top, top+lh], a banda dos glifos vive DENTRO dela, o hitTest do CENTRO do caret devolve a MESMA linha, e o centro do bloco de glifos == centro da banda |
| Linha do replay | o script editor ao device (FASE 13.8) — o PNG exportado mostra o caret na linha tocada |
| Padrão proibido | (nenhum — a vigília é a sentinela) |

Prova de mutação (mutacao-R029a-vermelho.txt): a fórmula antiga de volta (baseline = lineTop) → a sentinela VERMELHA (a banda dos glifos fora da banda do caret); reposta → 844/0.

## R-030 · hex de cor fora do ficheiro de Theme (FASE 0.9.6-MASTER · GRUPO UI · o gate do dono)

| campo | valor |
|---|---|
| ID | R-030 |
| Reportado | a ordem expressa do dono (spec G): «Gate R-020: qualquer hex fora do ficheiro de Theme = CI vermelho» (o R-020 histórico estava ocupado — o glTF do 0.9.6.3; o gate novo é o R-030) |
| Sintoma exato | literais hex de cor espalhados pela camada de UI (o fóssil da spec A: o clear #141414 literal, os eixos do gizmo em f32 crus) — tokens órfãos que a tabela única não governa |
| Causa raiz | a tabela única existia desde a 0.9.0 mas NADA impedia escrever `0x2196F3` num .cpp qualquer — a fonte única era um costume, não uma lei |
| Fix | `scripts/theme_hex_check.py` NOVO no CI (antes do build): qualquer literal hex de cor (6/8 dígitos) na camada de UI (ui/ + platform/main.cpp) fora do Theme.h e dos espelhos LIGADOS por static_assert (UiContext.h/Gizmo.h) = CI VERMELHO com a linha exata; os IDs de widget (sufixo u/ull) e máscaras 0x000000/0xFFFFFFFF ficam fora por contrato; os EIXOS do gizmo passam a MORAR no Theme (axisX/Y/Z/dim + o hover = accent âmbar) |
| Teste sentinela | o gate em si (CI: «Gate theme-hex (R-030 — cores so no Theme)») + os static_assert dos espelhos (mexer num token sem o alias = o build morre) |
| Linha do replay | o job de testes do CI corre o gate em CADA push |
| Padrão proibido | o output do gate (qualquer hex listado) |

Prova de mutação (mutacao-R030a-vermelho.txt): `0x2196F3` (o azul órfão da spec A) plantado no BottomPanel.cpp → o gate VERMELHO com a linha exata (exit 1 — release bloqueada); reposto → limpo (exit 0).

## R-031 · a timeline da Animação: uma ilha px-only com alvos 20×16dp no device (FASE 0.9.6-MASTER · GRUPO G · ANIMATION)

| campo | valor |
|---|---|
| ID | R-031 |
| Reportado | a auditoria exaustiva do GRUPO G (leitura de código — a classe exata do R-018 que o Grupo C curou em TUDO menos na Timeline) |
| Sintoma exato | o header da timeline media 24dp REAIS no C33 (@2.0 — kHeaderH=48 px crus) e os botões das linhas (curva/adiciona/apaga) eram 40/44/36 × 32 px = 20×16dp no device — ABAIXO de qualquer piso da casa, INVISÍVEL ao validador porque NENHUMA fase do harness abre a tab Animação |
| Causa raiz | a Timeline.cpp ficou FORA da migração dp() do Grupo C (zero chamadas theme::dp) e os alvos das linhas nunca passaram pelo orçamento de 48dp — a lição: o que o harness não desenha, o validador não vê |
| Fix | as constantes do layout/header/régua/linhas passam a theme::dp() (kHeaderH 48dp · kRowH 40px→48dp — os alvos cabem INTEIROS) e os botões das linhas a 48×48dp CHEIOS (curva/adiciona chave/apaga chave) |
| Teste sentinela | (a vigília honesta: os 21 testes do test_anim cobrem a LÓGICA — interp/playback/modos/velocidade — todos verdes; o PROVA de layout da timeline ao device fica REGISTADA como pendente: falta uma FASE do harness que abra a tab Animação com um TIC player — a dívida documentada no relatório do Grupo G) |
| Linha do replay | (idem — a fase da timeline no c33_virtual é a dívida) |
| Padrão proibido | (nenhum — a vigília é a dívida registada + a regra «nada de px crus na UI», apanhada por leitura) |


## R-022 · o contrato da hierarquia: a toolbar de transformação POR CIMA da top bar (0.9.6.12 · GRUPO J1 · P-08)

| campo | valor |
|---|---|
| ID | R-022 |
| Reportado | o dono (P-08 + os 5 defeitos de arquitetura do editor — defeitos 1 e 2: «TransformToolbar por cima do menu superior» e «estática quando painéis inferiores abrem») |
| Sintoma exato | no device, com o painel de baixo aberto (drawer persistido a 240), a toolbar de transformação desenha-se SOBRE a barra de topo e não acompanha o painel |
| Causa raiz | DUAS fontes para a altura do drawer: o `bottom::layout` aplicava um cap (viewport − 64dp) ao DESENHAR, mas o `currentDrawerH()` do main devolvia o drawer CRU aos rects do centro — no RMX3624 (viewport 208dp, drawer 240) o viewRect colapsava a ZERO e a toolbar desenhava-se ~56dp acima do topo da viewport = sobre a top bar. Falta também o piso que garantisse strip (48dp) + toolbar (48dp) sem sobreposição |
| Fix | `safe::effectiveDrawerH` — a FONTE ÚNICA da altura efetiva (clamp 160..400 → cap pelo NOVO piso `kViewportMinH` = 104dp → passos de 8dp), consumida pelo draw do drawer E pelo main; clamps no `vpchrome::layout` (a toolbar nunca sai do rect POR CONSTRUÇÃO — regra §2.2 do contrato); achados ao vivo da sentinela: o `toolPanel` transbordava 8dp o viewport de 288dp e no C33 (756dp) a fileira de botões saía 4dp (o Ímã fora do rect) — a margem esquerda agora cede antes de transbordar |
| Teste sentinela | `regress_hierarquia_contrato` (tests/test_sentinels.cpp) — o CONTRATO docs/LAYOUT_HIERARCHY.md §1-§3 inteiro: containment de todos os alvos, toolbar abaixo da strip/da top bar, fonte única do drawer, status bar intocável, pisos — nos ecrãs RMX3624@2.0 / C33@2.0 / harness@1.0 / 1024×640, com o drawer fechado/aberto/400/absurdo |
| Gate | `scripts/hierarchy_check.py` (job core-tests do CI): o contrato cita símbolos REAIS + as sentinelas do contrato existem no fonte |
| Prova de mutação | mutação R-022a: `effectiveDrawerH` a devolver o cru (a divergência de volta) → `regress_hierarquia_contrato` VERMELHA (eff ≤ cap falhado, alvos fora do rect, stackPanel fora) + 67 testes vermelhos no total; reposta → 0 falhas |
| Padrão proibido | (novos) «altura efetiva com DUAS fontes» — o cap só pode viver em `safe::effectiveDrawerH`; «região fora do pai documentado» — o gate e a sentinela vigiam |

## R-023 · o chip «Global» recortado para «Glob+» e o campo «pesquisar TIC» (0.9.6.12 · GRUPO J2)

| campo | valor |
|---|---|
| ID | R-023 |
| Reportado | o dono (defeito 3 dos 5 defeitos de arquitetura: «botão Global recortado para Glob+; campo pesquisar TIC colide com a hierarquia») |
| Sintoma exato | na strip do viewport ao device, o chip [Global] aparecia cortado (o dono leu «Glob+») — o campo de pesquisa da hierarquia, esse, NUNCA colidiu (vive confinado ao painel por construção: `w − 2×kPad` com `labelFitted`) |
| Causa raiz | os chips da strip tinham larguras FIXAS (88+112+88dp = 312dp) num viewport de piso 288dp COM o [+] a viver no fim direito da strip (plusTopRight no device): o chip Global transbordava a strip e era COBERTO pelo pai do [+] (desenhado depois) — o recorte era COBERTURA, não ellipsis |
| Fix | chips MEDIDOS (o wrap-content da spec J2): largura = texto medido pela fonte real + 2×8dp, piso 56dp; a RESERVA do [+] respeitada; a degradação por ordem — Perspetiva esconde primeiro, depois Cena, o Global é o ÚLTIMO (e só ellipsize quando nem ele cabe — o fallback honesto da spec). `vpchrome::layout(view, ChipWidths*)` — null mantém as larguras fixas (compat de testes) |
| Teste sentinela | `regress_texto_strip_campo` (tests/test_sentinels.cpp) — o encaixe em 3 densidades (mdpi 1.0 / hdpi 1.5 / xhdpi 2.0, a spec pede as três) com medidor fake density-invariante: texto inteiro, reserva do [+], sem sobreposição, degradação por ordem, fallback honesto; a FONTE REAL vive na FASE 14.1/14.2 do c33_virtual (o chip comporta «Global», o wrap-content com piso, «pesquisar TIC» cabe no piso 200dp da hierarquia) |
| Prova de mutação | mutação R-023a: as medidas ignoradas (larguras fixas de volta) → `regress_texto_strip_campo` VERMELHA (o Global fora do [+, fora da strip, a degradação morta) + 16 testes; repostas → 0 falhas |
| Padrão proibido | (novo) «largura de texto fixa em dp sem medir» — chip com texto passa pela medida da fonte (regra §2.5 do contrato) |

## R-024 · o retângulo do canvas OpenGL e o log de diagnóstico (0.9.6.12 · GRUPO J3)

| campo | valor |
|---|---|
| ID | R-024 |
| Reportado | o dono (defeito 4: «canvas OpenGL não recomputa o retângulo quando painéis mudam de tamanho»; a spec J3 pede o log «JNI: viewport set to (x, y, w, h)») |
| Sintoma exato | ao abrir/fechar o painel de baixo, o canvas 3D não acompanhava — o que restava do defeito no código atual era EXATAMENTE a causa do R-022 (o drawer cru vs tapado); o glViewport/glScissor/aspect do rect existiam desde o G1-3/Grupo D |
| Causa raiz | a arquitetura REAL não tem o salto JNI da spec (não há views Java nem GoniRenderer — a UI é toda C++ sobre UMA superfície NativeActivity); o equivalente do log pedido não existia: nenhuma linha dizia QUE retângulo o render usou — o dono não tinha como diagnosticar no device |
| Fix | (a) o log de diagnóstico `vp3d: viewport set to (x, y, w x h) — aspect N.NNN` no bloco do scissor (main.cpp), a logar SÓ na mudança do rect (nunca por frame) com os QUATRO números + o aspect; (b) a matemática seguida pela sentinela (o rect muda com o painel, o aspect é o DO RECT) |
| Teste sentinela | `regress_viewport_rect_segue` (tests/test_sentinels.cpp): abrir/fechar muda o rect pela altura efetiva, o aspect segue o rect (nunca o ecrã), coerência com o chrome, e o literal do log vigiado no fonte (REPO_ROOT) |
| Linha do replay | FASE 14.3 do c33_virtual — o log A ACONTECER: `vp3d: viewport set to (400, 160, 576 x 208)` com o painel aberto (o rect encolheu), a linha de volta ao rect cheio ao fechar, o aspect = w/h do rect |
| Prova de mutação | mutação R-024a: o log MUTADO (a linha deixa de ser a do contrato) → FASE 14.3 com 4 falhas + `regress_viewport_rect_segue` vermelha; reposto → 454/454 + 0 falhas |
| Padrão proibido | (novo) «mudança de rect visível sem linha vp3d no engine.log» |
