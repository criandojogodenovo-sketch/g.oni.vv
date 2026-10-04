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
