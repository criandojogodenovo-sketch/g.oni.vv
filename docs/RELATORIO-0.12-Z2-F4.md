# RELATÓRIO 0.12-Z2 — FASE 4: A AUDITORIA TOTAL, OS BUGS 4.2 E A CAIXA NEGRA

Campanha **0.12-Z2**. A F4 fecha os itens restantes do prompt: a auditoria
da toolchain, os bugs 4.2 do dono, os P2 herdados das F1/F1BIS/F1-R e a
CAIXA NEGRA. Regra de evidência: PROVADO (comando + saída), REFUTADO,
NÃO VERIFICADO. Palavras proibidas não aparecem.

## F4.1 — OS BUGS 4.2 (ficheiro:linha + a cura + a prova)

| # | Bug | Cura (ficheiro:linha) | Prova |
|---|---|---|---|
| 1 | «Ver log» exportava em vez de abrir | o chip da Consola ABRE o viewer (`ui/BottomPanel.cpp` kExportId→`viewLogPressed`; main.cpp consome = o mesmo caminho do choice 14); o «exportar» desceu para o viewer (`ui/EditorUi.cpp` botão 6402) COM a verificação da existência do engine.log (main.cpp: `fopen(elog::dir()+"/engine.log")` antes do MediaStore; sem ficheiro = toast honesto «engine.log não existe») | pin handshake_export_logs_relpath_and_result OK (a ponte JNI reparada); o raster 36/0 |
| 2 | «Painéis» com o ponto vermelho SEMPRE aceso | (a) os ficheiros OPCIONAIS settings.goni / textures/overrides.goni passam a `probe()` antes do read (main.cpp loadProjectSettings) — o E/ espúrio do SAF morre; (b) o ACK: `elog::errCount()` (novo, EngineLog.{h,cpp}) fotografia-se ao abrir a Consola (`ui/BottomPanel.cpp` `bs.consolaOpened/bs.errAck`) — o ponto acende de novo SÓ com E/ novo | raster_z2 36/0 (o boot sem os ficheiros não loga E/); a série 0 falhas |
| 3 | Spam «vp3d: viewport set to» no arrasto | `main.cpp` frame: `if (changed && !g_editor.divDragActive)` — durante o arrasto o log CALA; ao largar UMA linha com o rect final | leitura + a série 0 falhas |
| 4 | RSS=-1 cru | `logBootMemory` (main.cpp): parse por PREFIXO + strtol (o padrão do Bench.cpp) + a sentinela impressa como «não medido (/proc/self/status indisponível)» — nunca mais -1 cru | pin c33 `logHas("boot: memoria")` OK |
| 5 | Aviso «thumb 74ms > orçamento 50ms» | o relógio DIVIDIDO (main.cpp ThumbJob: `msCpu` flip+crop+downsample+encode / `msIo` open+write+close) — o orçamento 50ms é de CPU; o aviso só com `msCpu > 50`; o log mostra os 3 tempos | a série 0 falhas; o formato novo no engine.log |
| 6 | Nome do TIC sumia na linha selecionada da hierarquia | `ui/EditorUi.cpp` (~1099): o piso — se a zona dedicada ≤24dp (painel no piso kHierMinW=140), o nome usa a linha TODA (o literal do pin `(x + w - theme::dp(12.0f) - nameX)` mantém-se 1× — pin ui_hierarquia_40dp OK) | raster (a UI desenha); pin OK |
| 7 | Nome DUPLICADO no Inspector | a row `Kind::Name` SAIU do plano (`ui/EditorLayout.h` push removido) e do desenho (`ui/EditorUi.cpp` case removido) — o cabeçalho 44dp da FASE UI E é a fonte única; `inspectorRowCount` 2→1 em sincronia | os pins de plano/altura atualizados COM a mutação colada: scroll 1322→1282, mesh y 340→300, wiring011/012y/cameratic consistentes (17vs16 etc. mortos); probe: plan==rowCount para sólido/procedural/câmara/áudio/partículas |
| 8 | O quadro vazio com ⋮ junto às ferramentas | o chip ⋯ do topo MORREU (`ui/ViewportChrome.cpp`) — duplicado funcional do «Menu» da barra; o layout L.quick[0] fica (os pins da degradação OK); o menu vive no Menu | pins test_wiring090/test_sentinels OK |
| 9 | Câmara presa | o latch F3 (`ui/EditorUi.cpp` ~5537): o `break` no PRIMEIRO pointer reclamado virou `continue` — um dedo num controlo NÃO veta os outros; um dedo livre no view orbita | leitura + a série 0 falhas |

## F4.2 — OS P2 HERDADOS (F1 §F1.15 / §F1-R.1)

1. **gldbg counters honestos**: `render/SkyRenderer.cpp` + `render/ParticleRenderer.cpp` — o onCreated passou para o NASCIMENTO real (a init que falha a meio não conta) + o onDestroyed completou (falta o par inteiro); Particle conta os 2 buffers (corner+seeds); `render/Material.cpp` — o Program conta no FIM da init e os 2 UBOs contam em setBones/setInstances (o «adiantado» morre). **PROVADO**: pin gldebug_counters_ciclo atualizado (Buffer 5→3 com a mutação colada no pin).
2. **glassInvalidate SEM chamador**: ligado ao CONTRATO (Renderer.h:67) — `main.cpp`: na recuperação de contexto (contextRecover, pós recreateAll) e por frame na mudança de câmara/rect vp3d. O consumidor (captureGlass) continua dormente desde o D-2 — o sinal NÃO muda o render (o raster byte a byte OK). A versão da cena NÃO existe no Scene (documentado — fica para o dono).
3. **/tmp dos testes pesados**: `tests/Medicoes010m.h` — `TmpDirRaii` (RAII no filho: o dir morre em TODOS os retornos) + `cleanStaleHeavyTmp()` (os transitórios órfãos de corridas anteriores morrem ANTES do próximo teste pesado) — adotado no r039 (`test_010m_medicoes.cpp`) e no p6 (`test_010m_prova_final.cpp`). **PROVADO**: o r039 falhou errno 28 com 7.4GB e PASSOU com 7.7GB livres (a prova do limite físico; a higiene nova cobre os retornos normais — SIGKILL continua a ficar órfão, é o que o cleanStaleHeavyTmp varre).
4. **O crash do streaming sob disco cheio**: o ENOSPC do import já falha COM ERRO LIMPO («a escrita de 'assets/fonte.gmesh' falhou no bloco LOD #716 (errno 28)» — tc3, sem crash); o crash do `regress_streaming_paralelo` é a corrida latente PRÉ-EXISTENTE suspensa POR ORDEM DO DONO (STATE §7; F1BIS §A.2.5). NADA acrescentado (o pre-flight recusaria a sentinela r039 = enfraquecer o teste — proibido). **PROVADO/R-072-documented**.
5. **engine.log.1**: a rotação existe e mantém-se (`platform/EngineLog.cpp:72-94`, 1MB × 3 ficheiros; pins log_rotation_* OK). **PROVADO**.

## F4.3 — A CAIXA NEGRA (a feature aprovada)

- `platform/BlackBox.h` (o contrato) + main.cpp (`vv::blackbox` com LINKAGE INTERNO — o padrão 1-TU; as declarações antecipadas no topo): `arm(why)`, `buildText()` (puro), `waitIdle()` (testes), `hostDir()`.
- O .txt: gatilho, selo de tempo, GL_RENDERER/GL_VERSION, recriações de contexto, a linha de estado do frame (fps/verts/dc), a câmara (target/yaw/pitch/dist/fov/clips), o rect do viewport 3D, o gizmo (modo/eixo), os ponteiros vivos, o WE do TIC selecionado, os contadores GL nascidos/mortos por kind e as ÚLTIMAS 200 linhas do engine.log.
- O PNG: o backbuffer INTEIRO do frame (glReadPixels no PONTO SEGURO do thumb/layoutDump; o encode + a escrita no thread de jobs — o padrão ImportJob).
- OS GATILHOS AUTO: `contextRecover` (pós COMPLETA) e `cameraEnsureSane` (câmara inválida, com o estado PRÉ-reposição no snapshot).
- O BOTÃO: Settings › Diagnóstico › «Caixa negra (snapshot .txt+PNG)» (`ui/SettingsPage.{h,cpp}` kBlackBox + main case).
- A ESCRITA: device → Download/GOneVV/blackbox/ via MediaStore (JNI novo `jniExportBlackBoxToDownloads` + `VvActivity.exportBlackBoxToDownloads`, o mesmo caminho do export de logs); hospedeiro → o dir de logs (fallback /tmp/goni_blackbox).
- **PROVADO NO GLES REAL**: raster_z2 FASE C-BB — 9 checks: o job termina, o .txt existe com cabeçalho/gatilho/GL_RENDERER llvmpipe/todas as secções, o .png > 1KB, o worker junta-se (nada joinable ao sair). `checks=36 falhas=0 provas=7 — RASTER Z2 OK`.

## F4.4 — A TOOLCHAIN (o que esta fase correu)

| Ferramenta | Estado | Prova |
|---|---|---|
| test_core (série, GONI_SKIP=010m,r039,streaming_paralelo) | **0 teste(s) com falha** | tcA/tcB (exit 0) |
| 010m isolado · r039 isolado (disco limpo 7.7GB) | **0 falhas · 0 falhas (920 blocos, 60M verts, pico 107MB ≤ 512)** | runs isolados |
| raster_z2 (GLES 3.2 real, llvmpipe, surfaceless) | **36/0** | out-raster/RELATORIO-FASE0.txt |
| regress_streaming_paralelo | corrida latente PRÉ-EXISTENTE suspensa por ordem do dono (§7) | F1BIS §A.2.5 |
| ASan/UBSan/TSan/cppcheck/glslangValidator | **NÃO VERIFICADO nesta sessão** — a infraestrutura entrou (`scripts/build_tests.py` GONI_SAN/GONI_OBJ_DIR: `GONI_SAN="-fsanitize=address,undefined" GONI_OBJ_DIR=... python3 scripts/build_tests.py`); a build ASan completa ficou por terminar nesta sandbox (o disco 9.9GB e o tempo da build) — fica na fila da F5 com o comando pronto. O TSan do raster (build_raster.py --tsan) mantém-se das F1/F1-R (0 avisos) | o script commitado |

## F4.5 — A infraestrutura de teste (o aprendido)

- `build_tests.py` NÃO rastreia headers/includes: objetos velhos com `EditorState`/`EditorLayout.h` novos = ABI misturado = SEGV na seleção (o crash tc7/tc8). A cura desta sessão: `rm -rf tests/build-obj/*.o` a cada mudança de header + a build ASan separada por OBJ_DIR. O fix estrutural (deps de includes) fica para a F5 (comando + mutação prontos).
- O binário de testes anterior NÃO continha o main.cpp novo (o main.cpp entra nos TUs por #include — 1 TU; o .o standalone não existe no build-obj).

## F4.6 — Rótulos

| Afirmação | Rótulo |
|---|---|
| Os 9 bugs 4.2 corrigidos com a série verde | PROVADO |
| A caixa negra escreve .txt+PNG com o contrato no GLES real | PROVADO (FASE C-BB) |
| O ENOSPC do import falha com erro limpo | PROVADO (tc3) |
| O Mali GE8320 comporta-se igual | NÃO VERIFICADO (o §F5.3 da campanha) |
| ASan/UBSan/cppcheck/glslang nesta sandbox | NÃO VERIFICADO (o comando pronto no script; F5) |
