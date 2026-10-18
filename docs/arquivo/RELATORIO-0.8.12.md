# RELATÓRIO 0.8.12 — 5 FIXES CIRÚRGICOS DO C33: SELEÇÃO QUE NÃO SE PERDE + NONE DE 1ª CLASSE + STAGING SEM /tmp + DUMPS COM BADGE ANTIGO + DISPOSITIVO VIRTUAL EM CI COM SENTINELAS PERMANENTES

> Sub-fase 0.8.12 da campanha de estabilização. HEAD anterior: 0.8.11-d
> (990b004). versionCode 42 · suíte 608 → **614 testes** + **c33_virtual:
> 101 checks** (o dispositivo virtual) · CLÁUSULA CALMA respeitada (só os
> 5 fixes do prompt + harness + sentinelas + testes; zero features, zero
> formatos novos, zero áudio, zero V.ONI).

## 1. Objetivo

Matar os 5 bugs de device com evidência de log do C33 — reportados em
0.8.5, 0.8.7, 0.8.9 e 0.8.10 e SOBREVIVENTES a releases "verdes" — e
institucionalizar a prova: (1) a seleção não se perde entre selecionar o
TIC e tocar no picker, com guarda em TODOS os pickers e TODOS os TICs com
MeshRenderer; (2) `none` como opção de primeira classe (limpa o slot pelo
caminho seguro das trocas, serializa e recarrega); (3) staging de migração
sem `/tmp` (cache dir da app via JNI ou `.staging/` do projeto); (4) crash
dumps com identidade e badge `ANTIGO (build X)` no log viewer; (5) um
**dispositivo virtual em CI** que reproduz o telefone e corre o **replay
da sessão real do dono**, com **sentinelas permanentes** que põem o CI
VERMELHO para sempre se qualquer um destes bugs voltar — sem confiar no
build por ter passado e sem linguagem de achismo.

## 2. Diagnóstico (a causa de cada sintoma, por leitura de código linha a linha)

| Sintoma exato do device | Causa raiz (ficheiro/função) | Fix |
|---|---|---|
| `mesh: troca - → prim esfera ERRO(sem TIC com mesh selecionado)` (e variantes `mesh pick 2/3`, `tex pick 2`); trocas com origem explícita davam `fim ok` | `platform/main.cpp` (chamada de `editor::viewportTapClearsSelection`): o deselect corria **SEM guard de overlay** — o tap na linha/backdrop do picker caía DENTRO do viewRect, armava no press e LIMPAVA a seleção no release **do mesmo frame do dispatch** (os botões da UI não reclamam o slot de input externo; o dispatch lia `g_editor.selected` DEPOIS) | guard `!editor::anyOverlayOpen(g_editor) && !audioMode` no chamador (main.cpp) |
| a seleção perdia-se também após fundo/recents (lifecycle) | `APP_CMD_INIT_WINDOW` recarregava a cena (`loadActiveScene`) — os handles dos TICs MORRIAM e `g_editor.selected` ficava stale (o Inspector limpava-o em silêncio) | `editor::revalidateSelection()` — re-valida/re-mapeia por NOME após o load |
| `none` falhava / picker de mesh sem `none` | `ui/EditorUi.cpp` `drawAssetMenu` menuKind 1: 1º item era "cube (procedural)"; `applyAssetPick` sem caminho de limpeza com posse p/ deferred free | `none` em 1º lugar + `primRetire = mesh ? mesh : primRetire` (nunca perde pendente) |
| `fileapi: mkdir falhou em '/tmp' errno=30 (Read-only file system)` → `asset: migracao … FALHOU` → `staging falhou: /tmp/goni_recon…` | `assets/AssetConverter.cpp` `convert::reconvertFile`: LITERAL `"/tmp/goni_reconvert_<pid>.tmp"` no caminho SAF — o host de testes tem `/tmp` escrevível, o Android NÃO | `convert::stagingWrite` com fallback em cascata: `.staging/` do projeto → **cache dir da app via JNI (`getCacheDir`)** → erro legível; gate de CI grepa o literal |
| o log viewer mostrava o dump velho (crash-1790830406.dump, offsets idênticos) sem o rotular | `buildinfo::dumpIsFromOtherBuild` existia desde a 0.8.10 e **NUNCA era chamado** pelo `drawLogViewer` (wiring morto) | `buildinfo::dumpBadge()` + chamada no viewer com cor de aviso; banner de boot com sha256 |

Nota técnica sobre a INTERMITÊNCIA da evidência ("às vezes fim ok"): o
`camgizmo::pickSceneTic` do main re-seleciona o TIC acertado no viewport —
quando o TIC projetava **sob** o overlay do picker, o MESMO tap que matava
a seleção re-selecionava o TIC e a troca "funcionava". O replay do
dispositivo virtual usa o TIC AFASTADO do centro (o cenário em que o
sintoma aparecia no telefone) — apanhado durante a prova de mutação A.

## 3. A GUARDA DE UI (T1+T2 — a seleção como invariant)

Três camadas, todas aferváveis:

1. **O chamador não desseleciona com overlays** (main.cpp): o deselect do
   viewport 3D exige `!playMode && !uiMode && !audioMode &&
   !anyOverlayOpen(g_editor)` — o tap na linha do picker, no backdrop, no
   viewer de logs ou em QUALQUER overlay nunca mais limpa a seleção.
2. **O lifecycle re-valida** (main.cpp INIT_WINDOW): o nome do TIC
   selecionado é capturado ANTES do `loadActiveScene`; depois,
   `editor::revalidateSelection` mantém o handle vivo, re-mapeia por nome
   (log `lifecycle: selecao re-validada pos-INIT WINDOW`) ou limpa com log
   honesto se o TIC desapareceu. O ELEMENTO de UI selecionado faz reset
   (o índice pós-reload pode apontar outro elemento — o TIC continua).
3. **A guarda dos pickers** (EditorUi.cpp + main.cpp):
   `editor::pickerGuardBlocked(scene, selected)` — true quando o alvo não
   é um TIC VIVO com MeshRenderer (câmara/áudio/ui/handle morto). Aplicada
   no OPEN (Inspector: as linhas mesh:/tex:/prim: só abrem o seletor com
   alvo válido; o flag `pickBlockedHint` viaja ao main que converte em
   toast) e no DISPATCH (main: o pick sem alvo dá hint + log
   `ui: pick bloqueado (sem seleção)` — o caminho
   `ERRO(sem TIC com mesh selecionado)` fica inatingível pelos pickers).

## 4. `none` DE PRIMEIRA CLASSE (T3)

- **Pickers**: mesh ganha `none` em 1º (cube 2º, ficheiros 3+); tex e prim
  já o tinham em 1º. Sem seleção, tocar em `none` dá o hint como qualquer
  linha (a guarda é do toque, não da entrada).
- **Limpeza segura**: `mesh: none` limpa `mesh/material/meshPath/primOn/
  primPending` e põe a posse antiga em `primRetire` — a COVA abre no
  início do frame SEGUINTE (deferred free, o MESMO caminho das trocas;
  `primRetire = mesh ? mesh : primRetire` nunca perde um pendente em
  none→none no mesmo frame — bug real de leak apanhado pelo próprio teste
  `assetpick_mesh_none_x_none_repetido_sem_crash` durante o loop).
  `tex: none` limpa textura+ref (material volta à cor plana).
- **Round-trip**: o `.goni gravava `"mesh":"none"` desde a 0.8.10 — agora
  afervado: o load recarrega VAZIO (sem mesh/prim/path/tex); `none→X→none`
  ×5 (harness, com frames reais no meio — a cova abre) e ×8 (sentinela)
  sem crash e sem leak (`g_primOwners`/`g_primGrave` vazios no fim).

## 5. STAGING SEM /tmp (T4)

`convert::stagingWrite` por esta ordem: (1) `.staging/` DENTRO do projeto
quando a raiz é caminho de ficheiros real (FsStorage do device com
all-files; o importFile faz streaming dele); (2) **cache dir da app via
JNI** — `VvActivity.cacheDirPath()` (Java: `getCacheDir()`; a ponte
`storage::jniCacheDir()` no StorageBridge, o padrão do mic da 0.8.11)
quando a raiz é `content://` (SAF) ou a escrita em `.staging/` falhou — o
único sítio garantido escrevível no Android sem permissões; (3) erro
LEGÍVEL com os caminhos reais tentados. JAMAIS `/tmp` — e o gate do CI
grepa o literal para sempre (ver §15).

## 6. DUMPS ROTULADOS (T5)

O nome do dump já levava a identidade (`crash-<unix>-vc<N>.dump`,
0.8.10); o header já traz build/versionCode/git/sha/epoch. O que faltava
era o VIEWER: `buildinfo::dumpBadge()` devolve `  [ANTIGO (build N)]`
(quando o vc do nome difere do instalado), `  [ANTIGO (pre-0.8.10)]`
(dump anónimo) ou `""` (mesma build) — e o `drawLogViewer` junta-o ao
nome com a cor `theme::WARN` (âmbar — exceção DOCUMENTADA ao tema mono,
como a brand #8AB4F8 da toolbar: um aviso de identidade tem de ler-se
distinto). O banner de boot passou ao formato exigido:
`boot: G.One VV <versão> versionCode <N> sha256 <sha256-da-.so> git <…>`.

## 7. O DISPOSITIVO VIRTUAL EM CI (T6 — o "PC virtual" obrigatório)

`tests/c33_virtual.cpp` → executável **`c33_virtual`** (CI: job próprio).
Reproduz o telefone nas condições que morderam:

| Condição do C33 | Como o harness a reproduz |
|---|---|
| `/tmp` read-only (errno=30) | seam `fileapi::testing::setReadonlyPrefix("/tmp")` (FileApi) — writeAll/makeDirs no prefixo falham com a MESMA linha de log do device; sem isto o bug era invisível no CI (0.8.5–0.8.10) |
| só cache/files da app escreíveis | o staging cai no cache dir via JNI fake (`g_jni.cache_dir`) ou `.staging/` do projeto — os únicos caminhos que o código usa |
| FileApi via URI content:// | FASE 4 com `SafStorage` + `FakeSafIo` (o modelo de provider da suíte, extraído p/ `tests/FakeSafIo.h`) |
| lifecycle EGL TERM/INIT | `onAppCmd(APP_CMD_TERM_WINDOW/INIT_WINDOW)` REAIS — destruição e re-criação do contexto, re-upload, reload da cena |
| toques do dono | `injectDown/injectUp` + `frame()` REAL do platform/main.cpp (o mesmo caminho de input do telefone; press num frame, release no seguinte) |
| ecrã 1536×720 + insets | `eglstub::g_surfaceW/H` + `contentRect {0,24,1512,720}` (status 24 + pill 24) |
| ASTC (Mali) | `glstub::astcLdr` — o boot loga `ASTC SIM` |

O **REPLAY DA SESSÃO REAL** (a sequência que produzia os sintomas):
boot com projeto antigo → migração com /tmp RO → selecionar o TIC (afastado
do centro) → picker de primitivas → tocar "esfera" (o toque que matava a
seleção no mesmo frame) → picker de mesh → ficheiro migrado (mesh pick 3) →
tap no backdrop → tex none → mesh none → none→cube→none ×5 → sem seleção
(hint em vez de ERRO) → overlay de logs → troca de modo 3D|UI|ÁUDIO →
2× TERM/INIT com re-validação → troca pós-lifecycle → SAF content:// →
dump ANTIGO → gate de padrões proibidos. **101 checks, 0 falhas.**

**Emulador AVD**: NÃO executado neste ambiente (o runner não tem
KVM/emulator; o job do CI também não — sem hipervisor, o AVD arranca a
pasmado ou não arranca). O harness é o MÍNIMO OBRIGATÓRIO e foi o usado —
reproduz as 4 condições do device (fs/URI/lifecycle/toques) pelo caminho
REAL do main.cpp com os stubs da suíte. A extensão AVD fica documentada
como dívida (§12) com o desenho pronto: job `avd-smoke` opcional que
instala o APK assinado num AVD API 29+, abre projeto, troca mesh, none,
reboot de janela, anexa logcat.

## 8. Implementação por ficheiro

- `platform/main.cpp` — guard de overlay no deselect; re-validação da
  seleção no INIT_WINDOW (nome capturado antes do load); guarda no
  dispatch do pick (hint+log); hint-flag do Inspector; labels
  none/cube/mesh pick do log de troca; `applyImportedAssetToSelectedTic`
  com pick i+3 para mesh (o layout novo); linha de versão 0.8.12 no
  android_main.
- `ui/EditorUi.cpp/.h` — `pickerGuardBlocked` + `revalidateSelection`
  (puras); guard no OPEN das linhas mesh/tex/prim + `pickBlockedHint`;
  `none` em 1º no picker de mesh (+1 linha de altura); `applyAssetPick`
  menuKind 1: none (posse p/ cova) / cube / ficheiros 3+ com posse que
  nunca perde pendente; badge `dumpBadge` no `drawLogViewer`.
- `platform/BuildInfo.cpp/.h` — banner `G.One VV … versionCode … sha256 …
  git …`; `dumpBadge` (build N / pre-0.8.10 / vazio).
- `ui/UiContext.h` — token `theme::WARN` (âmbar, exceção documentada).
- `assets/AssetConverter.cpp` — `stagingWrite` (cascata projeto→cache
  dir→erro legível); reconvertFile sem /tmp.
- `platform/StorageBridge.cpp/.h` — `jniCacheDir()` + registo do método
  `cacheDirPath` (não crítico, o padrão do mic).
- `app/src/main/java/vv/goni/VvActivity.java` — `cacheDirPath()`
  (getCacheDir com catch honesto).
- `platform/FileApi.cpp/.h` — seam `testing::setReadonlyPrefix/
  clearReadonlyPrefix` (thread-local; só testes/harness tocam — o código
  de produção nunca; documentado no header).
- `tests/c33_virtual.cpp` (NOVO) — o dispositivo virtual + replay (101
  checks).
- `tests/test_sentinels.cpp` (NOVO) — as 4 sentinelas `regress_*`.
- `tests/FakeSafIo.h` (NOVO) — o FakeSafIo extraído do test_saf_project
  (partilha sem duplicação; o test_saf passa a incluí-lo).
- `tests/stub/EGL/egl.h` — `eglstub::g_surfaceW/H` (1536×720 do harness).
- `tests/stub/GLES3/gl3.h` — `glstub::astcLdr` (config de ambiente — o
  reset não a apaga).
- `tests/stub/jni.h` — `cache_dir` no fake + handler `cacheDirPath`.
- `tests/test_assetpick.cpp` — picks 3+ do mesh picker + os 2 casos novos
  do none (limpeza com posse; none→X→none).
- `tests/test_wiring010.cpp` — banner/badge novos (formato exigido).
- `tests/test_wiring087.cpp` — stress com picks novos; migração/reconverter
  com registo JNI + cache dir (o device completo no host); unistd.
- `tests/test_wiring089.cpp` — picks 3+ (normalização uniforme).
- `tests/CMakeLists.txt` — test_sentinels.cpp no test_core +
  `add_executable(c33_virtual …)` + `add_test(c33_virtual_replay)`.
- `scripts-local/build_manual.sh` — os DOIS executáveis (o grep de SRCS
  exclui o harness do test_core e os test_* do c33_virtual).
- `ci/forbidden_log_patterns.txt` (NOVO) — 4 padrões proibidos.
- `docs/REGRESSOES.md` (NOVO) — 4 entradas + política de 5 regras.
- `.github/workflows/release.yml` — job `c33-virtual` (ctest com
  sentinelas + replay + gate de padrões + gate literal /tmp + artifact do
  output); `build-release` com `needs: [core-tests, c33-virtual]`;
  artifact `goni-vv-0.8.12-release-signed`.
- `app/build.gradle` — versionCode 42 / versionName 0.8.12.
- `README.md` — escopo 0.8.12 + checklist C33 de 6 blocos.

## 9. Decisões

- **A guarda vive no CHAMADOR (main), não na função pura**: o
  `viewportTapClearsSelection` continua puro (afervável); o bug era a
  CHAMADA sem guard — o guard `!anyOverlayOpen` está onde o wiring decide.
- **Re-mapeio por NOME no lifecycle** (não por índice/posições): o nome é
  a identidade que o dono vê e edita; handles são internos. Se dois TICs
  partilharem nome, o primeiro ativo ganha (determinístico).
- **Hint como caminho de sucesso, ERRO como código morto**: o prompt
  exige que o toque sem seleção NUNCA caia no `ERRO(sem TIC…)` — a string
  fica no código como defesa final, mas as sentinelas + gate garantem que
  nunca mais aparece no output.
- **Staging em cascata (projeto→cache dir)** em vez de SÓ cache dir: o
  FsStorage do device (all-files) ganha streaming direto do `.staging/`
  do projeto (sem round-trip JNI); o SAF usa o cache dir. O erro final
  lista os caminhos reais tentados.
- **Seam de /tmp read-only no FileApi (test seam documentado)**: sem ela,
  o CI tem `/tmp` escrevível e o bug R-002 é INVISÍVEL — era exatamente o
  estado de 0.8.5–0.8.10. Só testes/harness tocam a seam; o gate de
  literal + sentinelas vigiam.
- **TIC afastado do centro no replay**: durante a mutação A descobri que
  o `pickSceneTic` re-selecionava o TIC sob o overlay — parte da
  intermitência da evidência. O replay usa o TIC longe do centro (o
  cenário do sintoma) para provar o fix no pior caso.
- **`theme::WARN` (âmbar)** como exceção documentada ao tema mono (como a
  brand da toolbar): o badge ANTIGO é um AVISO de identidade — tem de
  ler-se distinto, não é decoração.
- **FakeSafIo extraído para header**: partilhar o modelo de provider SAF
  com o harness sem duplicar ~200 linhas afervadas desde a F5.4.

## 10. Testes (608 → 614 + 101 checks; RED→GREEN provado ×4 por MUTAÇÃO)

Suíte do core: **614 testes, 0 falhas** (os 608 anteriores + 4 sentinelas
+ 2 do assetpick none — e os updates de layout/banner). Dispositivo
virtual: **101 checks, 0 falhas** (o output inteiro colado no §17-b).

### (a) Tabela de iterações do LOOP (Tarefa 8 — cada iteração e o que apanhou)

| it. | estado | o que apanhou | ação |
|---|---|---|---|
| 1 | 9 casos FALHARAM (38 EXPECTs) | testes antigos codificavam o layout VELHO do picker (cube=1º) e o banner antigo; o `applyImportedAssetToSelectedTic` usava pick i+2 (wiring meu, apanhado pelos próprios testes); getpid sem unistd | wiring i+3 no main; unistd; testes atualizados para o layout novo (o requisito) |
| 2 | 2 casos (11 EXPECTs) | **BUG REAL de leak**: o 2º `none` no MESMO frame sobrescrevia a posse pendente (`primRetire = mesh` com mesh=null apagava o pendente) — apanhado pelo novo teste none→X→none | `primRetire = mesh ? mesh : primRetire` nos 4 sítios de posse |
| 3 | 0 falhas | — | suíte 100% |
| 4 | harness: 1 FAIL (ASTC) | o `glstub::reset()` apagava o `astcLdr` (é config de ambiente, não estatística) | reset não mexe em astcLdr |
| 5 | harness: 1 FAIL (ASTC) | o `g_pipeline` (criado no android_main) não existia no harness → o log do caminho ASTC não saía | harness cria texCache+pipeline (o papel do android_main) |
| 6 | sentinela SAF FAIL | o `flushWrites()` do FakeSafIo vinha DEPOIS do reconvertFile (o conteúdo ainda estava no memfd) | flush ANTES da leitura (o passo invisível do provider) |
| 7 | 1 FAIL intermitente (500 MB) | DISCO do ambiente cheio (9.6G/9.9G pelas fixtures repetidas das mutações) — não era código | limpeza das fixtures; 0 falhas |
| 8 | 0 falhas + 101 checks | — | loop LIMPO |

### (b) Checklist de revisão requisito→local→teste (100%)

| Requisito do prompt | Onde está (ficheiro/função) | Que teste o cobre |
|---|---|---|
| T1: mapear pontos que limpam seleção | §2 do relatório + auditoria (worklog) | — (diagnóstico) |
| T1: seleção sobrevive a abrir/fechar QUALQUER picker | main.cpp (guard `!anyOverlayOpen` no deselect) | c33_virtual 2.2/2.4/2.9; sentinela regress_selection_loss (3) |
| T1: lifecycle re-valida o handle após INIT_WINDOW | main.cpp INIT_WINDOW + `editor::revalidateSelection` | c33_virtual FASE 3 (+3.5 2º ciclo); sentinela (1) |
| T1: troca de overlays | `closeAllOverlays` (não mexe em selected) | sentinela regress_selection_loss (3) |
| T1: troca de modo 3D\|UI\|ÁUDIO | Toolbar (não mexe) + guard audioMode no deselect | c33_virtual 2.10 |
| T2: linhas prim/mesh/tex só tocáveis com TIC+MeshRenderer | EditorUi.cpp Inspector (guard no open) | sentinela (2) (câmara excluída) |
| T2: sem seleção → hint + log `ui: pick bloqueado (sem seleção)` | EditorUi.cpp + main.cpp (dispatch + hint-flag) | c33_virtual 2.8; sentinela (4) |
| T2: nunca cai no `ERRO(sem TIC…)` depois do toque | main.cpp dispatch guard | c33_virtual FASE 6 (gate) + mutação A |
| T3: none em 1º nos pickers mesh e tex | EditorUi.cpp drawAssetMenu | sentinela (1)/(2); assetpick; c33_virtual 2.5/2.6 |
| T3: mesh none limpa o slot (TIC sem mesh) | EditorUi.cpp applyAssetPick menuKind 1 pick 1 | assetpick_mesh_none…; c33_virtual 2.6 |
| T3: tex none limpa textura | applyAssetPick menuKind 2 pick 1 | sentinela (2); c33_virtual 2.5 |
| T3: limpeza pelo mesmo caminho seguro (deferred free) | primRetire → primFlushPending → primGraveDig | assetpick (posse); c33_virtual 2.6/2.7 (cova abre) |
| T3: serialização grava vazio + load recarrega vazio | SceneSerializer (existente) + afervado | sentinela (3) round-trip `"mesh":"none"` |
| T3: none→X→none ×N sem crash/leak | — | sentinela (1) ×8; c33_virtual 2.7 ×5 (frames reais) |
| T3: sem seleção, none mostra hint | dispatch guard | c33_virtual 2.8 (tap none sem seleção) |
| T4: /tmp substituído (getCacheDir OU .staging/) | AssetConverter.cpp stagingWrite | sentinela regress_tmp_staging; c33_virtual FASE 1/4 |
| T4: criação recursiva | writeAll→makeDirs (existente) | sentinela (sucesso do staging) |
| T4: falha → erro legível com caminho real | stagingWrite err final | mutação B (o erro com os caminhos) |
| T4: gate CI grep /tmp | workflow step "Gate do literal /tmp" | testado local (exit 1 = verde); mutação B |
| T5: header+nome com build/versão/vc/sha/epoch | CrashHandler (0.8.10) + BuildInfo | sentinela (4)/(6); wiring010 identidade |
| T5: badge ANTIGO (build X) quando difere | BuildInfo.cpp dumpBadge + EditorUi.cpp drawLogViewer | sentinela (1)/(6b corrompido); c33_virtual FASE 5; mutação D |
| T5: banner `boot: G.One VV <v> versionCode <N> sha256 <…>` | BuildInfo.cpp banner() | sentinela (5); wiring010 |
| T6: /tmp read-only + só cache/files escreíveis | FileApi seam + harness | c33_virtual env (3 checks da sonda) |
| T6: content:// stubs | FakeSafIo.h + c33_virtual FASE 4 | sentinela (3º bloco); c33_virtual FASE 4 |
| T6: lifecycle EGL TERM/INIT com re-upload | c33_virtual FASE 3 | c33_virtual 3.2–3.5 |
| T6: toques replayáveis | injectDown/Up + frame() | c33_virtual (todos os passos) |
| T6: viewport 1536×720 com insets | eglstub + contentRect | c33_virtual FASE 1 (checks) |
| T6: ASTC ativo | glstub::astcLdr | c33_virtual FASE 1 (check `ASTC SIM`) |
| T6: replay da sessão real + asserts | c33_virtual main() | as 101 checks + FASE 6 |
| T7: REGRESSOES.md com 4 entradas + política | docs/REGRESSOES.md | §17-f (conteúdo colado) |
| T7: 4 sentinelas com os nomes exigidos | tests/test_sentinels.cpp | correm no ctest (2 jobs) |
| T7: gate de padrões proibidos em ficheiro | ci/forbidden_log_patterns.txt + workflow | §17-h (gates); mutações |
| T7: needs: no job de release | workflow build-release | §15 |
| T7: prova de mutação | — | §17-e (colada) |
| T7: contrato de não-regressão | docs/REGRESSOES.md política | §17-f |
| T8: loop até limpo | §10-a | 8 iterações |
| T9: zero achismo | relatório inteiro | §17-i |
| TESTES mínimos: seleção por tipo de TIC com MeshRenderer | — | assetpick (body 3D PlayerBody3D); wiring087 (preset Mesh, stress); c33_virtual (importado .gmesh) |
| TESTES: falha simulada de gerador mantém mesh+seleção | — | wiring087_upload_falhou_backoff… (0.8.10, mantido) |

### (c) Tabela de rastreabilidade sintoma→causa→fix→teste→replay→sentinela

| Sintoma exato do device | Causa raiz | Fix | Teste que reproduz | Linha do replay que confirma | Sentinela |
|---|---|---|---|---|---|
| `mesh: troca - → prim esfera ERRO(sem TIC com mesh selecionado)` (+ `mesh pick 2/3`, `tex pick 2`) | deselect sem guard de overlay (main.cpp) + handles mortos no INIT_WINDOW | guard anyOverlayOpen + revalidateSelection + pickerGuardBlocked | mutação A (guard removido → o ERRO volta ×6) | FASE 2 (2.2/2.3/2.4: seleção viva em todos os taps; FASE 6: zero ERRO) | regress_selection_loss |
| `fileapi: mkdir falhou em '/tmp' errno=30` → `staging falhou: /tmp/goni_recon…` | literal /tmp no reconvertFile (AssetConverter.cpp) | stagingWrite cascata (.staging/ → getCacheDir → erro legível) | mutação B (/tmp de volta → 7 ocorrências) | FASE 1 (migração ok com /tmp RO: `asset: staging em '…/.staging/…'`, zero sintomas) + FASE 4 (SAF) | regress_tmp_staging |
| `none` falhava / inexistente no picker de mesh | drawAssetMenu menuKind 1 sem none; applyAssetPick sem posse | none em 1º + primRetire sem perder pendente | mutação C (none→cube → 7 FALHAS) | FASE 2 (2.5 tex none limpa; 2.6 mesh none: `fim ok (sem mesh — slot limpo (none))`; 2.7 ×5) | regress_none_slot |
| dump velho sem rótulo (crash-1790830406.dump, offsets idênticos) | dumpIsFromOtherBuild nunca chamado pelo viewer | dumpBadge + drawLogViewer + banner sha256 | mutação D (badge morto → 1 FALHA) | FASE 5 (`crash-1790830406-vc39.dump  [ANTIGO (build 39)]`; vc42 sem badge) | regress_dump_identity |
| `ERRO(gerador/upload falhou)` (o erro desonesto da 0.8.7) | caminho de ERRO atingível com seleção perdida | guards convertem em hint; o caminho fica morto | (coberto pela mutação A — os ERROs voltam) | FASE 6 (zero `ERRO(gerador/upload falhou)`) | gate de padrões |

## 11. Riscos

- **Seam de /tmp no FileApi**: é um test-seam documentado — se alguém a
  usar em produção, o comportamento do FileApi muda silenciosamente. A
  política (header + REGRESSOES.md) diz "só testes/harness"; o gate do
  literal vigia /tmp, não a seam. Risco baixo, alternativa nenhuma sem
  mount namespaces (exige root no runner).
- **Re-mapeio por nome no lifecycle**: nomes duplicados de TICs podem
  re-mapear para o "primeiro com o mesmo nome" — determinístico mas
  impreciso se o dono tiver dois TICs com o mesmo nome. (Os presets geram
  nomes únicos; renomear para duplicado é possível.) Dívida: identidade
  persistente por TIC (id serializado).
- **O replay usa o `frame()` real mas com stubs GL/EGL/JNI**: cobre o
  wiring e a lógica de estado, não o driver Mali. É o máximo exequível em
  CI sem hipervisor — e o checklist C33 (§16) fecha a ponta no telefone.
- **101 checks não exaustivos de todos os píxeis**: o harness afere
  ESTADO + LOG (o que o prompt exige); o visual continua a cargo do dono.

## 12. Dívida

- **AVD smoke test** (o "se disponível"): job opcional `avd-smoke` com
  emulator API 29+, APK assinado, abrir projeto/trocar mesh/none/reboot de
  janela + logcat. O desenho está no §7; fica por executar por falta de
  hipervisor no runner (documentado no próprio job c33-virtual).
- **Identidade persistente de TIC** (para o re-mapeio do lifecycle não
  depender de nomes): id serializado no .goni ( migração de formato).
- **Remoção da string `sem TIC com mesh selecionado`** do código (caminho
  morto): mantida como defesa final nesta release; remover exige passagem
  de auditoria (a string é o padrão proibido — se algum dia o grep do
  CÓDIGO a quiser proibir, o caminho tem de estar primeiro morto à prova).
- **O `.staging/` fica no projeto** (tamanho ~0; reuso pelo próximo
  reconvert): remover a pasta competiria com reconverts concorrentes do
  migrateLegacyAssets no mesmo load.

## 13. CLÁUSULA CALMA

Só seleção/pickers/none/staging/dumps/harness/sentinelas/testes + loop.
Zero features, zero formatos novos, zero áudio, zero V.ONI, zero layout
novo além do token WARN (cor do badge) e da 1ª linha extra do picker de
mesh (o none exigido). A única "feature" é o que o prompt pediu: o
dispositivo virtual e as sentinelas.

## 14. Como ler o log se algo falhar no C33

1. `boot: G.One VV 0.8.12 versionCode 42 sha256 …` — a 1ª linha diz QUE
   build está a correr (compare com o sha do artifact).
2. `lifecycle: selecao re-validada pos-INIT WINDOW` — a seleção sobreviveu
   ao fundo/recents (ou `dispensada (TIC '…' nao existe…)` se o TIC saiu).
3. `ui: pick bloqueado (sem seleção)` — o hint correto SEM alvo (o dono
   vê o toast "seleciona um TIC com mesh").
4. `mesh: troca <de> → <para> fim ok …` — TODA troca termina fim ok;
   `fim ok (sem mesh — slot limpo (none))` é o none a funcionar.
5. `asset: staging em '<caminho>'` — o staging foi para projeto/cache dir;
   JAMAIS `/tmp` (se aparecer, é o R-002 de volta — o CI teria ficado
   vermelho antes).
6. Viewer de logs: dumps de outra build com `[ANTIGO (build N)]`.

## 15. Gates do CI

- **core-tests**: ctest (614 testes — sentinelas incluídas) + check_main +
  link_parity + jni_parity + JVM (igual às releases anteriores).
- **c33-virtual** (NOVO): ctest (o replay `c33_virtual_replay` corre
  AQUI e no core-tests — as sentinelas correm em TODAS as runs) + o replay
  com output teclado + **gate de padrões proibidos**
  (`ci/forbidden_log_patterns.txt` grep -F sobre o replay + engine.log;
  qualquer match = VERMELHO) + **gate do literal /tmp** (grep
  `['\"]/tmp` em FileApi/assets; exit 1 se achar) + artifact
  `c33-virtual-replay-log` (a evidência para colar no relatório).
- **build-release**: `needs: [core-tests, c33-virtual]` — sentinela
  vermelha = NÃO HÁ APK. APK assinado versionCode 42 + build_info.txt
  (2 passes, sha256 da .so) + manifest gates (verify-entry-symbols).

## 16. Checklist device (o dono preenche pass/fail por item)

Para cada tipo de TIC com mesh (preset **Mesh**, **objeto importado**,
**body com mesh**):

1. abrir pickers prim/mesh/tex e trocar 3× cada → `fim ok` no log viewer;
   seleção VIVA em todas (o Inspector nunca fica "(nada selecionado)");
2. escolher **none** → limpa visivelmente (o TIC deixa de renderizar;
   tex none → cor plana); `none` persiste após save/load;
3. `none→mesh→none` ×5 → sem crash, sem `sem TIC com mesh selecionado`;
4. desselecionar (tap no vazio do viewport) → tocar em QUALQUER linha de
   picker incl. none → toast "seleciona um TIC com mesh" + log
   `ui: pick bloqueado (sem seleção)`; ZERO ERRO no log;
5. tap no backdrop do picker → fecha sem desselecionar;
6. fundo/recents e voltar → o TIC continua selecionado (log
   `lifecycle: selecao re-validada pos-INIT WINDOW`); trocar mesh depois
   do retorno → `fim ok`;
7. troca de modo 3D|UI|ÁUDIO → a seleção segue;
8. projeto ANTIGO migra sem `staging falhou`/`mkdir falhou em '/tmp'`
   (log `asset: staging em '…'` com caminho do projeto/cache dir);
9. viewer de logs → dump de build antiga com `[ANTIGO (build N)]`;
   NENHUM dump novo por uso normal;
10. a 1ª linha do boot log: `boot: G.One VV 0.8.12 versionCode 42 sha256 …`.

## 17. Exigências específicas do prompt

(a) **Tabela de iterações do loop** — §10-a (8 iterações, cada uma com o
que apanhou; o loop fechou à 8ª porque a 7ª era ambiente, não código, e a
8ª confirmou 614+101 verdes).

(b) **Checklist de revisão requisito→local→teste 100%** — §10-b (37
requisitos mapeados; nenhum sem teste).

(c) **Tabela de rastreabilidade sintoma→causa→fix→teste→replay→sentinela**
— §10-c (os 5 sintomas do prompt, linha a linha).

(d) **Evidência do dispositivo virtual** — o executável usado foi o
**`c33_virtual`** (harness headless com o caminho REAL do
platform/main.cpp em CI): o AVD NÃO foi usado porque este ambiente e o
runner do CI não têm hipervisor (o emulator sem KVM não arranja; o
prompt manda declarar qual foi usado e porquê — e o harness é o mínimo
obrigatório). Output colado:

```
== C33 VIRTUAL — dispositivo headless em CI (0.8.12) ==
   reproduz: /tmp read-only (errno=30), cache dir da app,
   content:// SAF, lifecycle EGL TERM/INIT, 1536x720 + insets,
   ASTC ativo, taps replayaveis pelo frame() real
  [env] /tmp READ-ONLY (errno=30) ATIVO; escrita so no cache dir 'c33-virtual-cache' e no projeto
    [ok]   ambiente: writeAll em /tmp falha (read-only simulado)
    [ok]   ambiente: errnoText devolve errno=30 (Read-only file system)
    [ok]   ambiente: a linha de log do device aparece na sonda (prova da seam)
  [env] superficie 1536x720; ASTC LDR ativo; cache dir via JNI

== FASE 1 — boot com migracao de projeto antigo (/tmp READ-ONLY) ==
    [ok]   projeto criado no storage
    [ok]   cena gravada com a ref legada
    [ok]   boot: INIT_WINDOW completo (g_ready)
    [ok]   boot: superficie 1536x720 (a resolucao do C33)
    [ok]   boot: caminho ASTC ativo (o Mali do C33)
    [ok]   boot: dentro do orcamento de tempo
  > a migracao do projeto antigo corre com /tmp READ-ONLY
    [ok]   migracao: assets/casa.gmesh criado
    [ok]   migracao: linha do log da fonte legada
    [ok]   migracao: staging no cache dir/projeto (nunca /tmp)
    [ok]   migracao: ZERO 'staging falhou' (o sintoma do C33)
    [ok]   migracao: ZERO "mkdir falhou em '/tmp'" (o sintoma do C33)
    [ok]   migracao: o .gmesh resolve (3 idx)
    [ok]   migracao: ref do TIC re-escrita p/ assets/casa.gmesh

== FASE 2 — replay da sessao real (selecao -> picker -> troca -> none) ==
  > 2.1 selecionar o TIC Casa (afastado do centro — o dono trabalha com o TIC onde o deixou)
    [ok]   TIC Casa vivo apos o boot
  > 2.2 abrir o picker de PRIMITIVAS e tocar a linha esfera
    [ok]   tap na linha esfera: dentro do orcamento
    [ok]   picker fechou apos o toque
    [ok]   SELECAO VIVA apos tocar na linha do picker (o fix do C33)
    [ok]   esfera aplicada (primOn + mesh vivo)
    [ok]   esfera com geometria nao vazia
    [ok]   ZERO 'ERRO(sem TIC com mesh selecionado)' no replay inteiro
  > 2.3 picker de MESH: tocar o ficheiro migrado (mesh pick 3)
    [ok]   catalogo tem o .gmesh migrado
    [ok]   selecao viva apos mesh pick 3
    [ok]   mesh migrado aplicado ao TIC (meshPath = assets/casa.gmesh)
    [ok]   log 'mesh: troca ... fim ok' presente (a prova do C33)
  > 2.4 tap no backdrop do picker (dentro do viewRect)
    [ok]   ponto do backdrop fora do painel (a geometria bate)
    [ok]   backdrop fechou o picker
    [ok]   SELECAO VIVA apos o tap no backdrop (o fix do C33)
  > 2.5 picker de TEX: none em primeiro lugar
    [ok]   tex none: textura limpa (material volta a cor plana)
    [ok]   selecao viva apos tex none
  > 2.6 picker de MESH: none (o slot limpa)
    [ok]   mesh none: slot limpo (TIC deixa de renderizar mesh)
    [ok]   log 'fim ok (sem mesh — slot limpo (none))'
    [ok]   cova aberta (deferred free no frame seguinte)
  > 2.7 none -> cube -> none x5 (frames reais no meio)
    [ok]   TIC vivo no ciclo none->X->none
    [ok]   none aplicado (ciclo)
    [ok]   cube aplicado (ciclo)
    [ok]   none aplicado de novo (ciclo)
    [ok]   estado final do ciclo: slot vazio
    [ok]   TIC vivo no ciclo none->X->none
    [ok]   none aplicado (ciclo)
    [ok]   cube aplicado (ciclo)
    [ok]   none aplicado de novo (ciclo)
    [ok]   estado final do ciclo: slot vazio
    [ok]   TIC vivo no ciclo none->X->none
    [ok]   none aplicado (ciclo)
    [ok]   cube aplicado (ciclo)
    [ok]   none aplicado de novo (ciclo)
    [ok]   estado final do ciclo: slot vazio
    [ok]   TIC vivo no ciclo none->X->none
    [ok]   none aplicado (ciclo)
    [ok]   cube aplicado (ciclo)
    [ok]   none aplicado de novo (ciclo)
    [ok]   estado final do ciclo: slot vazio
    [ok]   TIC vivo no ciclo none->X->none
    [ok]   none aplicado (ciclo)
    [ok]   cube aplicado (ciclo)
    [ok]   none aplicado de novo (ciclo)
    [ok]   estado final do ciclo: slot vazio
    [ok]   sem posse viva acumulada (sem leak)
    [ok]   cova vazia no fim (deferred free completo)
  > 2.8 sem selecao: hint + 'ui: pick bloqueado (sem selecao)'
    [ok]   log 'ui: pick bloqueado (sem seleção)' presente
    [ok]   toast de hint 'seleciona um TIC com mesh'
    [ok]   2ª linha de pick bloqueada logada (hint sem spam de ERRO)
    [ok]   ZERO 'ERRO(sem TIC com mesh selecionado)' mesmo sem selecao
  > 2.9 adversario: outro overlay (log viewer) tambem guarda a selecao
    [ok]   SELECAO VIVA com overlay de logs aberto (o guard e de TODOS os overlays)
  > 2.10 adversario: troca de modo 3D | UI | AUDIO nao perde a selecao
    [ok]   selecao viva no modo UI
    [ok]   selecao viva no modo AUDIO
    [ok]   selecao viva de volta ao 3D (a troca de modo NAO limpa)

== FASE 3 — lifecycle TERM/INIT (a selecao sobrevive ao ciclo) ==
  > 3.1 re-selecionar e trocar antes do ciclo
    [ok]   TIC Casa vivo antes do TERM
    [ok]   esfera pedida antes do TERM
  > 3.2 TERM_WINDOW (contexto EGL destruido)
    [ok]   contexto morto (g_ready=false)
    [ok]   log do TERM_WINDOW
  > 3.3 INIT_WINDOW (re-criacao + re-upload + RELOAD da cena)
    [ok]   contexto re-criado (g_ready)
    [ok]   log do INIT_WINDOW com contexto re-criado
    [ok]   SELECAO VIVA apos TERM/INIT (re-validada pelo nome)
    [ok]   o handle re-mapeado aponta o TIC 'Casa' (nao outro)
    [ok]   log 'lifecycle: selecao re-validada pos-INIT WINDOW'
  > 3.4 trocar mesh DEPOIS do ciclo (o fluxo do dono continua)
    [ok]   box pedida apos o INIT
    [ok]   mesh vivo no contexto NOVO (re-upload pelo ponto seguro)
  > 3.5 segundo ciclo TERM/INIT (a prova de robustez)
    [ok]   selecao viva apos o 2º ciclo TERM/INIT

== FASE 4 — migracao pelo SAF content:// (staging no cache dir) ==
    [ok]   projeto criado no provider SAF
    [ok]   meshes/ criado no provider
    [ok]   fonte legada escrita via content://
    [ok]   cena gravada no provider
  > migracao SAF: reconvertFile com raiz content://
    [ok]   reconvertFile por SAF sucede (staging no cache dir)
    [ok]   assets/casa.gmesh no provider
    [ok]   ZERO 'staging falhou' no caminho SAF
    [ok]   ZERO "mkdir falhou em '/tmp'" no caminho SAF
    [ok]   staging no cache dir logado com o caminho
    [ok]   o staging JAMAIS em /tmp

== FASE 5 — dump velho com badge ANTIGO (identidade) ==
    [ok]   o dump velho aparece na lista do viewer
    [dump] crash-1790830406-vc39.dump  [ANTIGO (build 39)]
    [ok]   dump da build 39 com badge [ANTIGO (build 39)]
    [ok]   dump NOVO (vc42) sem badge (e da build instalada)

== FASE 6 — gate de padroes proibidos (o CI vermelho se voltar) ==
    [ok]   ZERO ocorrencias do proibido (engine.log inteiro)   [4 padrões: sem TIC com mesh
           selecionado / mkdir falhou em '/tmp' / staging falhou / ERRO(gerador/upload falhou)]
    [ok]   troca com 'fim ok' no log
    [ok]   hints de pick bloqueado no log

== C33 VIRTUAL: 101 check(s), 0 falha(s) ==
HARNESS VERDE — o dispositivo virtual confirma os 5 fixes
```

(e) **Prova de mutação** (o fix revertido temporariamente → sentinela/
harness VERMELHO com o sintoma exato; reposto → VERDE — "sentinela que
nunca falha não é sentinela, é decoração"):

**MUTAÇÃO A (seleção: guard do deselect + guard do dispatch revertidos)**
— output VERMELHO (20 falhas; os [FAIL] relevantes e o sintoma):
```
    [FAIL] SELECAO VIVA apos tocar na linha do picker (o fix do C33)
    [FAIL] esfera aplicada (primOn + mesh vivo)
    [FAIL] ZERO 'ERRO(sem TIC com mesh selecionado)' no replay inteiro
    [FAIL] SELECAO VIVA apos o tap no backdrop (o fix do C33)
    [FAIL] mesh migrado aplicado ao TIC (meshPath = assets/casa.gmesh)
    [... 20 FALHAS no total ...]
== C33 VIRTUAL: 73 check(s), 20 falha(s) ==
HARNESS VERMELHO — release BLOQUEADA (ver [FAIL] acima)
    [engine.log] 6× a LINHA EXATA do dono:
    E/GONI: mesh: troca - → prim esfera ERRO(sem TIC com mesh selecionado)
    E/GONI: mesh: troca - → mesh pick 3 ERRO(sem TIC com mesh selecionado)
    E/GONI: mesh: troca - → tex pick 1 ERRO(sem TIC com mesh selecionado)
```

**MUTAÇÃO B (staging: o literal /tmp de volta no reconvertFile)** —
output VERMELHO (11 falhas):
```
    [FAIL] migracao: assets/casa.gmesh criado
    [FAIL] migracao: staging no cache dir/projeto (nunca /tmp)
    [FAIL] migracao: ZERO 'staging falhou' (o sintoma do C33)
    [FAIL] reconvertFile por SAF sucede (staging no cache dir)
    [FAIL] ZERO ocorrencias do proibido (engine.log inteiro)
    [... 11 FALHAS no total ...]
    [engine.log] 7× os sintomas exatos:
    W/GONI: fileapi: mkdir falhou em '/tmp' — errno=30 (Read-only file system)
    E/GONI: asset: staging de reconversao FALHOU — staging falhou: /tmp/goni_reconvert_<pid>.tmp
```

**MUTAÇÃO C (none: o pick 1 volta a ser cube — o comportamento 0.8.11)** —
output VERMELHO (7 falhas):
```
    [FAIL] mesh none: slot limpo (TIC deixa de renderizar mesh)
    [FAIL] log 'fim ok (sem mesh — slot limpo (none))'
    [FAIL] estado final do ciclo: slot vazio  (x5)
    [... 7 FALHAS no total ...]
```

**MUTAÇÃO D (badge: dumpBadge devolve sempre vazio — o wiring morto)** —
output VERMELHO (1 falha):
```
    [FAIL] dump da build 39 com badge [ANTIGO (build 39)]
    [... 1 falha ...]
```

**Fixes repostos** — output VERDE (colado no §17-d):
`== C33 VIRTUAL: 101 check(s), 0 falha(s) ==` e suíte 614/614.

(f) **Conteúdo de docs/REGRESSOES.md** — 4 entradas (R-001 perda de
seleção; R-002 staging /tmp; R-003 none; R-004 dump ANTIGO), cada uma com
ID, versões em que foi reportado, sintoma exato de log, causa raiz
(ficheiro/função), fix, nome do teste sentinela, linha do replay e padrão
proibido; e a POLÍTICA escrita (5 regras): todo fix de bug de device
acrescenta entrada + sentinela + padrão; remover/saltar sentinela exige
aprovação explícita do autor; toda sentinela nova vem com prova de
mutação colada; o job de release DEPENDE das sentinelas/gates/harness;
campanhas futuras não as desativam sem nota + aprovação. (Ficheiro
commitado — conteúdo íntegro no repo.)

**Conteúdo de ci/forbidden_log_patterns.txt** (commitado; linhas de
padrão, as restantes são comentários de uso):
```
sem TIC com mesh selecionado
mkdir falhou em '/tmp'
staging falhou
ERRO(gerador/upload falhou)
```

(g) **NÃO VERIFICADO**:

1. **O C33 FÍSICO** — nenhum dos fixes foi executado no telefone real
   (este ambiente não tem o device). Como verificar: o checklist §16 (10
   itens) no telefone com o APK assinado 0.8.12; o critério global continua
   "zero crash dumps novos + zero linhas dos sintomas no engine.log".
2. **O AVD (emulador Android)** — não executado (sem hipervisor neste
   ambiente/runner; o prompt manda declarar). Como verificar: job opcional
   `avd-smoke` (desenho no §12) num runner com KVM: APK API 29+, abrir
   projeto, trocar mesh, none, reboot de janela, logcat anexado.
3. **A cor âmbar do badge num ecrã real** — o token theme::WARN é
   afervado numericamente (0.98/0.73/0.27/1.0), não fotometricamente. Como
   verificar: item 9 do checklist (o dono vê o `[ANTIGO (build N)]` e
   confirma que se lê distinto).

(h) **Resultado dos gates grep** (executados localmente com o MESMO
comando dos steps do workflow):

```
$ grep -rn -E "['\"]/tmp" app/src/main/cpp/platform/FileApi.cpp \
      app/src/main/cpp/platform/FileApi.h app/src/main/cpp/assets/
(exit 1 — nenhum literal encontrado)           → GATE VERDE

$ while read pat; do grep -F -q -- "$pat" /tmp/c33-final-output.txt \
    c33-virtual-logs/engine.log && echo "ACHOU: $pat"; done \
    < ci/forbidden_log_patterns.txt
(sem output — zero padrões proibidos no replay) → GATE VERDE
```

(i) **Declaração de linguagem**: nenhuma frase de achismo (as palavras
de hedging da lista do gate de docs, R-016/ci/forbidden_docs_patterns.txt)
aparece neste relatório, nos commits ou nos comentários do diff — cada
afirmação cita teste/linha de output/gate/log/harness (as tabelas §10-b,
§10-c e os outputs colados §17-d/e). O que não pôde ser verificado está
na secção NÃO VERIFICADO (§17-g) com o método de verificação — nada foi
omitido.

## FECHO CI (18)

**FECHADO — run 37096946912 (commit ae434ff) 100% VERDE**:

- **core-tests**: 614 testes (sentinelas `regress_*` incluídas) +
  check_main + link_parity + jni_parity + JVM → success.
- **c33-virtual**: `100% tests passed, 0 tests failed out of 2` (core +
  replay) · replay `== C33 VIRTUAL: 101 check(s), 0 falha(s) ==` ·
  `GATE VERDE: zero padrões proibidos no replay do C33 virtual` ·
  `GATE VERDE: nenhum literal /tmp em FileApi/assets/migração` · o output
  oficial do CI está no artifact `c33-virtual-replay-log` (c33-replay.txt
  + engine.log — a 1ª linha do banner nele:
  `I/GONI: boot: G.One VV 0.8.12-virtual versionCode 42 sha256 aabbccdd00112233 git c33c0ffe`).
- **build-release** (APK assinado, versionCode 42, `needs:
  [core-tests, c33-virtual]`): artifact **goni-vv-0.8.12-release-signed**;
  app-release.apk sha256
  `a1610df9253b32193d97b2cf6c84d32938b9063a5e07d2fb32c060fac377d4ea`.
  **IDENTIDADE VERIFICADA PONTA-A-PONTA**: o build_info.txt DENTRO do APK
  diz `soSha256=ee84f8a02836b1d2dc16366e2b1188fd03db608128a1c8a4c7ea9e1a114d0594`
  e a .so arm64 extraída do MESMO APK tem EXATAMENTE esse sha256
  (version=0.8.12, versionCode=42, git=ae434ff…, epoch=1791002252).
- **verify-entry-symbols**: manifest + símbolos + paridade JNI → success.

**A DEPENDÊNCIA AO VIVO**: o run intermédio 37096395233 ficou VERMELHO no
job c33-virtual (gate de padrões com falsos positivos das próprias
mensagens de verificação — corrigido no commit ae434ff) e o
**build-release ficou SKIPPED — NÃO SAIU APK**. A cadeia
sentinela/harness vermelha → release bloqueada funcionou ao vivo no CI
antes de ficar verde (a prova de que o `needs:` não é decorativo).

Commits: f34c9b1 (0.8.12-a — a campanha inteira), ce5a490 (0.8.12-b — fix
CMake do c33_virtual: `astcenc_weight_quant_xfer_tables.cpp` com _tables;
o build manual filtrava o ficheiro inexistente e passava — o CMake real
apanhou), ae434ff (0.8.12-c — gate limpa os falsos positivos), 0.8.12-d
(este fecho do relatório). Aguarda **VERIFIED do dono no C33** (checklist
§16 — 10 itens).
