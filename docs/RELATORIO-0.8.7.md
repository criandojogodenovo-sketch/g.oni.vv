# RELATÓRIO 0.8.7 — HOTFIX CIRÚRGICO: import + troca de mesh (com auditoria de existência)

Hotfix fora de campanha (F8 fechada na 0.8.6). DOIS alvos apenas — o botão
Import abrir o navegador e a troca de mesh não travar — mais a auditoria de
existência pedida pelo dono (a suspeita: as primitivas nem existiam no build
do device) e logging embutido passo-a-passo. ZERO features, zero layout,
zero animação, zero física.

## 1. Objetivo

(1) Auditoria de existência: provar que o gerador de primitivas e o caminho
de troca estão compilados no APK e são chamados no device (gate de CI novo).
(2) Fix do botão Import: o toque abre o navegador de ficheiros (all-files),
importa obj/gltf/glb e aplica ao TIC selecionado. (3) Fix da troca de mesh:
destroy/recreate seguro e determinístico (primitiva↔primitiva,
importado↔primitiva, com/sem textura), sem erro e sem freeze, em sequências
variadas. (4) Logging embutido: `import: <passo>`, `mesh: troca <de>→<para>
inicio/fim ok/ERRO(razão)` e `mesh: prim <tipo> verts=N idx=M` no engine.log
— o log viewer revela a linha exata da falha no C33. (5) Testes host
realistas com stub GL + lifecycle e stress anti-freeze com guarda de tempo.

## 2. Estado inicial (HEAD de entrada)

`5048cd5` (0.8.6-a, suíte 532 OK; CI 0.8.6 verde com APK assinado,
versionCode 37). Sintomas do C33: o botão Import não abre nada (não leva aos
ficheiros — wiring morto do toque→handler→abrir overlay); a troca de mesh
trava a engine INTERMITENTEMENTE (às vezes sim, às vezes não) ou dá "falha
ao gerar primitiva"; suspeita fundada de que as primitivas nem existissem no
build do device (precedente real: FileApi.cpp nos testes mas fora do CMake
da app). A animação funciona (testada só com o cubo da engine, porque o
import não deixa carregar modelos). Segunda vez que import e troca eram
"corrigidos" (0.8.5) sem funcionarem no device — os testes host stub-am a
ponte Java e o lifecycle GL, mas o device percorre caminhos reais não
cobertos.

## 3. Diagnóstico (a auditoria de existência + as três causas raiz)

**AUDITORIA DE EXISTÊNCIA — RESPOSTA À SUSPEITA: o gerador ESTÁ no APK e É
chamado.** (a) `render/Primitives.cpp` está no CMake da APP (linha 137) desde
a 0.8.0 — NÃO é o precedente FileApi; (b) o seletor lista a partir dos dados
do gerador (`primLabel(PrimKind i)` no grelha 2×4 — EditorUi.cpp:1327), não
uma lista hardcoded desligada; (c) o handler chama o gerador no caminho real:
`drawAssetMenu` → `applyAssetPick(menuKind 4)` → `res.prim` (main.cpp) →
`primMesh` → `makePrimMesh` + `Mesh::create`. O que FALTAVA era a prova no
binário (gate) e no device (log) — ambas adicionadas. Os bugs eram reais
mas viviam NOS TUs device-only:

**CAUSA 1 — IMPORT: o overlay desenha com DUAS flags, o handler só punha
UMA.** `attemptImport()` (caminho concedido) chamava `browserOpen()` — que
põe `g_browser.open = true` — mas NUNCA tocava em `g_editor.fileBrowser`. O
gate do desenho no frame() é `g_editor.fileBrowser && g_browser.open`
(main.cpp). Resultado: o toque CHEGAVA ao handler, o estado "abria", e o
overlay NUNCA desenhava — "o botão Import não abre nada", exatamente. O
outro call-site (o "importar…" do seletor de textura) setava as duas — o
único caminho que funcionava. A retoma pós-concessão (`resumePendingAfterReturn`)
tinha o mesmo vício pelo caminho oposto (abría o overlay de SCAN, não o
navegador).

**CAUSA 2 — TROCA INTERMITENTE: cache de primitivas SEM LIMITE + retry por
frame.** Cada assinatura distinta (tipo+parâmetros clampados) = 1 mesh GL
vivo para sempre (só `primMeshDestroy` no TERM_WINDOW libertava). O slider de
raio gera DEZENAS de assinaturas por arrasto → exaustão progressiva de
memória de GPU → `glGen*`/`glBufferData` começam a falhar (o toast "falha ao
gerar primitiva" — o "dá erro", INTERMITENTE porque depende de quanto se
editou antes) e o driver engasga (o "trava"). E quando `primMesh` devolvia
null, o `rebindPrimMeshes` — que corre TODOS os frames — re-gerava e
re-uplodava A CADA FRAME para sempre: geração + upload por frame = o
freeze visto pelo utilizador. (A suspeita "não é chamada" era compreensível:
quando o upload falha, nada muda no ecrã — parece que o seletor morreu.)

**CAUSA 3 — GESTO ÓRFÃO: `active_` do UiContext nunca morre sozinho.** O
`active_` só é limpo quando o widget DONO é desenhado com o dedo levantado.
Um widget que desaparece a meio do gesto (overlay fechado antes do release,
por exemplo) deixa o active_ preso PARA SEMPRE — e como TODO widget exige
`active_ == 0` para capturar um press novo, a UI inteira morre: o render 3D
continua, nada responde ("a engine trava"). O beginFrame não o resetava (e
não PODE resetar aí: o clique dispara no frame do release, o active_ tem de
sobreviver até ao widget o ver).

## 4. Implementação (por ficheiro)

- `platform/main.cpp` — o coração do hotfix:
  - `attemptImport()`: caminho concedido abre o navegador com AS DUAS FLAGS
    (`browserOpen` + `g_editor.fileBrowser = true`) + logging `import:`;
  - `resumePendingAfterReturn()`: a retoma do Import abre o MESMO navegador
    (as duas flags); `openImportScan()` REMOVIDO (código morto — o scan
    perdeu o único call-site);
  - `browserImportFile()`: logging `import:` passo-a-passo (escolhido/lidos
    N bytes/gravado no projeto/diálogo aberto/concluído);
  - `applyImportedAssetToSelectedTic()`: o corpo do "Sim" do diálogo
    EXTRAÍDO do frame() para função com nome (o mesmo código corre no device
    e na suíte); fecha as DUAS flags do applyAsk (a doença da causa 1 não
    regressa por aqui) + logging com verts/idx;
  - `primMesh()`: cache com CAP (kPrimCacheMax=48) + EVICÇÃO (falhados
    primeiro, depois os não-referenciados por NENHUM MeshRenderer — o mesh
    em uso NUNCA sai) + NEGATIVE-cache (assinatura que falhou = resposta
    estável null, zero retry por frame; nova tentativa só após
    primMeshDestroy = contexto novo) + a LINHA DE PROVA
    `mesh: prim <tipo> verts=N idx=M (upload novo; cache k/48)`;
  - `primCacheEvict()`/`primSigEq()` novos; `primMeshDestroy()` tolera
    entradas negativas (mesh null);
  - dispatch da troca no frame(): logging `mesh: troca <de>→<para> inicio`
    / `fim ok verts=N idx=M` / `ERRO(razão)` — as contagens lidas do mesh
    APLICADO (o applyAssetPick permanece PURO: nunca desreferencia o Mesh,
    o contrato dos testes 0.8.5 intacto).
- `ui/UiContext.cpp` — `endFrame()`: sem dedo em cima, o `active_` órfão
  morre no FIM do frame (depois de todos os widgets terem tido a frame do
  release). Mata a classe inteira do bug, não um call-site.
- `ui/EditorUi.h` — sem mudanças de contrato (AssetPickOutcome intacto).
- `.github/workflows/release.yml` — verify-entry-symbols ganha o GATE DA
  AUDITORIA: makePrimMesh/primDefaults/primName/primClamp (gerador) +
  applyAssetPick (dispatch) têm de estar no .dynsym do APK, senão ::error.
  Verificado no hospedeiro: a lib de paridade exporta os 5 (mangled, grep
  literal apanha).
- `tests/test_wiring087.cpp` — NOVO (13 casos): `#include
  "platform/main.cpp"` traz o caminho REAL do device para a suíte.
- `tests/CMakeLists.txt` — acrescenta EglContext.cpp e Grid.cpp (o main.cpp
  precisa deles; eram device-only) + test_wiring087.cpp.
- `tests/stub/EGL/egl.h` — o stub passa a ter init FELIZ + eglQuerySurface
  1280×720 (a paisagem do C33): o boot do INIT_WINDOW e o frame() correm
  no hospedeiro.
- `tests/stub/GLES3/gl3.h` — INJETOR DE FALHA (`glstub::failNextGenObjects`):
  glGen* devolve 0 (simula OOM/contexto doente — para aferir o
  negative-cache e a morte do retry-storm).
- `tests/stub/android_native_app_glue.h` — JNIEnv forward-declarado como o
  stub jni.h define (o TU do wiring inclui AMBOS; o typedef antigo colidia).
- `tests/test_main.cpp` — o runner imprime o nome ANTES de correr o caso
  (crash a meio da suíte deixava de ser um mistério de buffering).
- `README.md`/`app/build.gradle`/artifact — 0.8.7, versionCode 38.

## 5. Decisões técnicas relevantes

1. **`#include "platform/main.cpp"` em vez de extrair para uma lib**: o
   namespace anónimo do main dá acesso direto ao TU (primMesh/attemptImport
   são internal-linkage) — o código do DEVICE corre literalmente na suíte,
   sem duplicação nem "réplica" (o antipadrão da 0.8.5: testes com resolvers
   próprios). O link_parity já provava que o main compila+liga com os stubs;
   agora também é TESTADO.
2. **Contagens fora do applyAssetPick**: `out.verts` no outcome quebrava o
   contrato "applyAssetPick nunca desreferencia Mesh" (os stubs de teste
   usam ponteiros fake 0x10 — crash na 1ª build). As contagens vivem no
   chamador (main), onde o mesh é sempre real.
3. **Negative-cache em vez de retry com cooldown**: determinismo é o
   requisito ("sem erro e sem freeze"); uma assinatura falhada devolve null
   estável até o contexto mudar. Com o cap+evicção a exaustão desaparece, o
   failure fica RARO e LOGADO alto (a linha `mesh: prim … ERRO(upload GL…)`).
4. **Evicção nunca tira o mesh em uso**: o custo é um scan linear dos
   MeshRenderers por INSERÇÃO (trocas/edits — raro), não por frame.
5. **O active_ morre no FIM do frame, não no beginFrame**: no beginFrame
   matava o clique (o release-frame depende do active_ sobreviver até o
   widget o ver); no fim do frame, sem dedo, qualquer sobrevivente é órfão
   por definição.

## 6. Testes novos (CI Linux)

13 casos em `test_wiring087.cpp` (suíte 532→545; todos verdes; os 4 do
wiring crítico confirmados VERMELHOS com os fixes revertidos):

1. `wiring087_import_toque_abre_o_navegador` — o fix da causa 1 (RED antes:
   `g_editor.fileBrowser` falso);
2. `wiring087_import_sem_projeto_toast_honesto`;
3. `wiring087_import_pos_concessao_retoma_pelo_navegador` — o fluxo diálogo
   → concessão → navegador (a retoma pelo MESMO caminho);
4. `wiring087_browser_importa_obj_e_aplica_ao_tic` — ficheiro REAL no disco
   → `browserImportFile` (File API real) → FakeStorage → diálogo →
   `applyImportedAssetToSelectedTic` (o "Sim" extraído) → mesh aplicado pelo
   GpuAssets REAL (parse+upload) → NENHUM modal preso;
5. `wiring087_browser_formato_nao_suportado_erro_claro` — .fbx → toast+log;
6. `wiring087_troca_prim_por_tipo_geometria_nao_vazia_e_log_da_prova` — as
   8 formas pelo dispatch REAL; verts/idx > 0; a linha `mesh: prim <tipo>
   verts=` no engine.log de cada (a auditoria do lado do device);
7. `wiring087_troca_stress_antifreeze_com_guarda_de_tempo` — 600 trocas em
   6 padrões (sequencial/A→B→A/reversa/↔cube/↔importado/slider-params) com
   rebind por frame e GUARDA DE TEMPO por iteração (250 ms) e total (30 s) —
   freeze = falha;
8. `wiring087_cache_cap_e_eviccao_nao_toca_em_uso` — 200 assinaturas →
   cache ≤ cap; o mesh EM USO sobrevive e desenha; volta à assinatura =
   cache HIT (zero uploads);
9. `wiring087_upload_falhou_negative_cache_sem_retry_storm` — injetor de
   falha: upload falha → 60 frames de rebind NÃO regeneram nada (contadores
   GL congelados); assinatura NOVA funciona; contexto novo limpa a negativa
   e a MESMA assinatura consegue;
10. `wiring087_lifecycle_destroy_recreate_deterministico` — 5 ciclos
    TERM↔INIT com trocas: a MESMA geometria volta, o prim persiste, desenha;
11. `wiring087_gesto_orfao_nao_trava_a_ui` — widget desaparece a meio do
    gesto → botão NOVO volta a capturar+clique (RED antes: `clicked` falso);
    regressão do release-fora-do-botão (sem clique, como sempre);
12. `wiring087_anim_intacta_sob_trocas_de_mesh` — Once 2 s com 6 trocas a
    meio: para no fim, pose exata, mesh final válido;
13. `wiring087_boot_e_frame_smoke_com_browser_aberto` — BOOT REAL do
    INIT_WINDOW (EGL feliz 1280×720) + frame() com navegador aberto +
    seletor aberto + limpo — todos < 500 ms, UI submetida, TERM limpo.

## 7. Checklist device (resumo — versão completa no README)

Import abre o navegador (Menu → Importar…); .obj/.gltf/.glb importa e aplica
ao TIC ("Sim"); trocar primitiva em ordens variadas sem travar; sliders do
prim suaves; a UI nunca morre; animação intacta. **Se algo falhar**: o log
viewer mostra a ÚLTIMA linha `import:`/`mesh:` — parado depois de `troca …
inicio` sem `fim` = gerador/upload (a linha ERRO diz a razão); `toque no
botao` sem `navegador ABERTO` = permissão (ver `storage: all-files
granted=`); browser vazio = `browser: … opendir FALHOU`.

## 8. Riscos

- O negative-cache pode fixar uma falha TRANSITÓRIA até o próximo contexto
  (voltar do fundo). Mitigação: o cap+evicção elimina a causa dominante
  (exaustão); a falha remanescente é logada alta; trocar para OUTRA
  primitiva funciona logo (a negativa é por assinatura).
- `#include main.cpp` no TU de teste acopla a suíte ao main.cpp (quebra de
  compilação = quebra de CI imediata) — é o OBJETIVO (o link_parity já
  impunha isso implicitamente).
- O stub EGL feliz faz o boot do host seguir o caminho de sucesso; caminhos
  de FALHA do EGL continuam só no device (log de boot numerado).

## 9. Dívida (documentada)

- Evicção é por inserção (scan linear dos MeshRenderers) — suficiente para
  trocas/edits; um LRU verdadeiro fica se um dia houver pressão real.
- `frame()` no hospedeiro não simula o thread Java a disparar resultados
  (onActivityResult) — o pollResult é drenado à mão nos testes de handshake.
- O gesto do IMPORT (tap no item 5 do fileMenu) não é injetado por
  coordenadas (o layout do menu é interno); o wiring do choice 5 →
  attemptImport é código de 3 linhas coberto indiretamente pelo smoke +
  diretamente pelo teste da attemptImport.

## 10. CLÁUSULA CALMA (cumprida)

Só import + troca de mesh + auditoria de existência + logging + testes.
Zero features novas, zero layout, zero animação, zero física, zero
scripting. Nenhum mesh novo, nenhum material novo, nenhum sistema novo —
os únicos valores novos são um CAP e duas flags postas a true.

## 11. Fecho do CI (0.8.7-b)

Run 36992339344 (push `4e8552d`) — CI da sub-fase 100% VERDE:
core-tests (545 OK no cmake do CI) + build-release ASSINADO (artifact
`goni-vv-0.8.7-release-signed`) + verify-entry-symbols com o GATE NOVO
(as linhas do log do job provam os símbolos no .dynsym do APK arm64:
`_ZN2vv12makePrimMeshE…`, `_ZN2vv12primDefaultsE…`, `_ZN2vv8primNameE…`,
`_ZN2vv9primClampE…`, `_ZN2vv6editor14applyAssetPickE…` — o GERADOR DE
PRIMITIVAS E O DISPATCH DA TROCA EXISTEM NO BINÁRIO DA APP, a resposta
definitiva à suspeita da auditoria).

sha256 do APK assinado:
`be3278534c83a2ab63151338c51892aa6791ffd4453c75419a8a67eaf697875a`
(artifact goni-vv-0.8.7-release-signed, 1512909 bytes, versionCode 38).

Pendente: a VERIFICAÇÃO NO C33 pelo dono (checklist 0.8.7 no README —
import abre o navegador; trocas variadas sem travar; `mesh: prim …
verts=… idx=…` no log viewer a cada troca; se algo falhar, a ÚLTIMA linha
`import:`/`mesh:` do engine.log diz o passo exato).
