# G.One VV 0.6.7 — lifecycle GL + gestão de projetos (fix dos "cubinhos" + apagar projeto + Sair para projetos)

Engine com editor, projeto `.goni` e maturação de assets (compressão ETC2/ASTC
com cache, extração de texturas glTF/GLB, import OBJ/glTF/GLB/PNG, export
OBJ). Mobile-first: arm64-v8a, minSdk 24, landscape travado
(`sensorLandscape`). Devices de teste: Realme C33 (720x1600) e Realme
RMX3624 (Android 13).

## Escopo 0.6.7 (implementado)

**1 — LIFECYCLE GL (fix dos "cubinhos" do C33).** Sair do editor
(home/recents) e reentrar SEM matar a app deixava TODO o texto em quads
brancos: o `APP_CMD_TERM_WINDOW` destrói o contexto EGL (o
`EglContext::shutdown` mata surface E contexto — todos os ids GL ficam
órfãos), mas o `FontAtlas` guardava `tex_ != 0` (guard "já carregado") e o
2º `APP_CMD_INIT_WINDOW` NUNCA re-upava o atlas → o pass UI amostrava uma
textura órfã = quads brancos. O mesmo acontecia aos mapas do `GpuAssets`
(`releaseAll()` existia mas ninguém chamava). FIX: `FontAtlas::destroy()`
novo (`glDeleteTextures` + reset do id E das métricas — chamado no
TERM_WINDOW com o contexto ainda corrente; o guard passa a impedir só o
upload duplicado no MESMO contexto); TERM_WINDOW com ordem obrigatória
detach dos MeshRenderers → `g_gpu.releaseAll()` → `g_font.destroy()` →
cubo/grid → renderer → `g_egl.shutdown()` POR FIM; INIT_WINDOW loga a
RE-CRIAÇÃO do contexto e confirma o RE-BAKE + RE-UPLOAD. NENHUM recurso GL
é assumido vivo entre term/init (a cena é recarregada do disco e os
resolvers re-upam os assets).

**2 — APAGAR PROJETO no gestor.** Long-press numa entrada → diálogo com o
nome e a pasta e DUAS ações: "Remover da lista" (a pasta fica — como
antes) e "Apagar projeto" → confirmação EXPLÍCITA separada ("Apagar
projeto X? Não pode ser desfeito — a PASTA e TODOS os ficheiros são
apagados") → `VvProjects.deleteProject` novo remove a pasta via File API
(`DocumentsContract.deleteDocument` no URI de documento da raiz) + sai da
lista; a permissão persistente é libertada e a lista refresca.

**3 — "Sair para projetos" no editor.** Menu → 6º item: auto-save da cena
(cena + manifesto + assets de runtime materializados) → a activity
termina-se (`VvActivity.bridgeFinish` novo, chamado por
`jniFinishToLauncher` na ponte JNI) e o GESTOR retoma da back stack SEM
matar a app. Reentrar arranca um NOVO `android_main` — que agora faz RESET
de estado na entrada (TickGroups.clear() novo — sem isto a física seria
registada 2× e daria dois passos por frame; InputState.resetAll() novo;
cena/editor/play/projeto/catalog resetados) com um contexto EGL novo
(re-upload de tudo — o fix 1 cobre a reentrada).

**4 — fix do nome do projeto invisível.** O diálogo de novo projeto herda
o tema CLARO do manifest (`Theme.NoTitleBar.Fullscreen`) → painel branco;
o EditText tinha só texto quase-branco = texto branco sobre fundo branco
durante a digitação. Fix determinístico sem `res/`: fundo escuro
explícito (0xFF1E222A) + texto claro + hint + `requestFocus()` pós-show.

## Escopo F5.4 (histórico)

**PARTE 1 — fix do `UnsatisfiedLinkError` (RMX3624, Android 13).** O
logcat mostrava `No implementation found for void
vv.goni.VvActivity.nativeRegisterActivity(...)` no onCreate E no onResume —
o handshake nunca ficava OK e todo import/export falhava com "ponte Java
indisponível (handshake)". Causa raiz (docs/HANDSHAKE_AUDIT.md §6): o
`android.app.NativeActivity` carrega a lib com `dlopen(RTLD_LOCAL)` CRUO
(`loadNativeCode_native`) — um dlopen cru NÃO corre o `JNI_OnLoad` nem
registra a lib no mapa de resolução do JVM, logo o `RegisterNatives` da
0.6.3 nunca correu no device e a busca por nome não via a lib (a premissa
"super.onCreate faz System.loadLibrary" era FALSA — o hospedeiro testava o
C++ direto, a RESOLUÇÃO não era modelada). FIX em 2 camadas: (1)
`static { System.loadLibrary("goni_vv"); }` na VvActivity — o JNI_OnLoad
corre de verdade, o RegisterNatives executa e a lib entra no mapa do JVM;
(2) `ensureNativesRegistered()` idempotente no 1º nativeRegisterActivity
via GetObjectClass + `JNI_OnLoad` TOLERANTE (FindClass/RegisterNatives a
falhar NUNCA devolvem JNI_ERR — logam e adiam para a 2ª camada). GATE NOVO
pedido pelo dono: `scripts/jni_parity.py` no CI afere TODO native da
VvActivity.java contra a tabela RegisterNatives (nome+assinatura), o static
loadLibrary obrigatório e, no release, os símbolos `Java_vv_goni_VvActivity_*`
no `.dynsym` do .so real — na 1ª execução apanhou um bug latente real
(assinatura de nativeOnActivityResult com um 'I' a mais; corrigida).

**PARTE 2 — Gestor de Projetos (estilo Godot, múltiplas pastas).** A app
abre agora no ecrã **"Projetos"** (ProjectManagerActivity, launcher novo):
lista dos projetos já criados (vazia na 1ª instalação), toque abre direto
no editor, long-press remove da lista (a pasta NÃO é apagada). "+" → nome →
seletor de pastas do sistema (SAF, `ACTION_OPEN_DOCUMENT_TREE`) → a pasta
de CADA projeto é escolhida por ele: cada escolha gera um URI próprio,
guardado com `takePersistableUriPermission` — projetos diferentes em
pastas diferentes, NUNCA se misturam, MESMO SEM All Files Access. A
estrutura (project.goni, scenes/, meshes/, textures/) é criada na pasta
colhida e o editor abre esse projeto (SafStorage: ProjectStorage sobre a
árvore SAF — o editor não sabe a diferença). A lista vive em
`projects.json` no app-private (não depende do handshake nem de
permissões). O All Files Access mantém-se EXATAMENTE como estava, só para
import/export de assets soltos DENTRO de um projeto aberto
(Download/Documents) — as duas coisas COEXISTEM.

## Escopo F5.3 (histórico — handshake invertido)
Objetivo: a ponte Java↔native LIGAR DE VERDADE em runtime (handshake
INVERTIDO: a activity Java registra-se no native), para o fluxo All Files
Access funcionar no C33: diálogo → settings do app → permissão ativada →
File API direta. Mensagens de erro honestas (causa real).

No C33 (0.6.2), o engine.log mostrava `jni: initJava env/activity
indisponíveis (vm=0x…)` → `supported=0 manager=0 modo app-private` →
`export falhou (copied=0)`. Causa única: o `android_main` corre no thread do
glue (pthread) que NÃO está anexado à VM — `GetEnv` devolvia `JNI_EDETACHED`
e a ponte morria no arranque. As auditorias estáticas (manifest/dex/símbolos)
estavam corretas — o bug era o handshake runtime
(docs/HANDSHAKE_AUDIT.md tem o despejo do manifest do APK 0.6.2 REAL).

1. **APK real verificado (item 1)** — o 0.6.2 do CI despejado:
   `hasCode=true`, launcher=`vv.goni.VvActivity`, `lib_name=goni_vv` —
   o sistema instancia a VvActivity; o dex não tinha onCreate/onResume
   (nunca se registrava). Gate NOVO no CI (`aapt2 dump xmltree`) afere o
   manifest binário em TODO build.
2. **Handshake invertido (item 2)** — `VvActivity.onCreate()` chama
   `nativeRegisterActivity(this,"onCreate")` (após `super.onCreate`, quando
   o loadLibrary/JNI_OnLoad já correu); `onResume()` reforça (idempotente —
   GlobalRefs substituídos). O native guarda `JavaVM` + `GlobalRef` + métodos
   e loga: `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` → no boot `handshake=1`. A chamada
   `initJava` do android_main foi REMOVIDA (o native não descobre a
   activity sozinho — era a causa).
3. **Attach de threads (item 3)** — qualquer thread da engine que chame
   Java faz `AttachCurrentThread` NOMEADO (`goni-engine`, via
   `JavaVMAttachArgs`) e PERMANENTE (sem Detach); decisões na tabela pura
   `platform/JniAttach.h` (JNI_OK→usa; EDETACHED→attach; resto→falha com o
   CÓDIGO no log). Nenhum `JNIEnv*` assumido não-nulo.
4. **Fluxo All Files pós-handshake + mensagens honestas (item 4)** —
   diálogo → `ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` →
   `isExternalStorageManager()` no retorno → File API direta. Se o
   handshake falhar, toast/log diz **"ponte Java indisponível (handshake)"**
   — NUNCA "sistema sem All Files Access" (0.6.2 mentia sobre a causa;
   `blockReason`/`blockMessage` em `StoragePerm.h` distinguem ponte vs
   sistema, testados no CI).
5. **Testes CI (item 5)** — `tests/test_handshake.cpp` + a ponte JNI
   INTEIRA compilando na suíte com um fake JNI controlável
   (`tests/stub/jni.h`): handshake simulado (o teste chama
   `nativeRegisterActivity` — o papel do "stub Java"), re-registro
   onResume, registo parcial, attach com/sem falha (rc logado), fluxo de
   permissão completo (diálogo → intent 4301 → onActivityResult → fila →
   Granted + ação retomada), regressão das mensagens honestas e
   `JNI_OnLoad` com 2 nativos. 254→264 testes.

## Escopo F5.2 (implementado)
Objetivo: o fluxo correto de permissões de armazenamento (o mesmo do Godot) +
diagnóstico in-app — no C33, o 0.6.1 mostrava "SAF indisponível" sem nunca
pedir permissão; o SAF tree picker foi REMOVIDO.

1. **Diálogo in-app (item 1)** — ao tentar Importar/Export pela primeira vez:
   overlay mono "ARMAZENAMENTO — Precisa de acesso a todos os ficheiros para
   importar/exportar projetos" com **Permitir/Cancelar** (mensagem quebrada
   pela fonte real; toque fora fecha). Máquina de estado GL-free em
   `platform/StoragePerm.h` (testada no CI: diálogo só na 1ª tentativa,
   cancelar volta a Idle, sem nag em loop).
2. **Abrir settings (item 2)** — "Permitir" → `VvActivity.openAllFilesSettings(4301)`
   lança **`ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION`** com `package:vv.goni`
   (a janela de permissões DO app). Constante `kSettingsAction` afervel no CI.
3. **Verificar + File API direta (item 3)** — o retorno chega por
   `onActivityResult` → fila JNI → thread da engine → `Environment.isExternalStorageManager()`
   re-verificada; concedido → **File API POSIX direta**: import varre
   `Download/` + `Documents/` (obj/gltf/glb/png, ordenado, overlay de escolha)
   e copia para `meshes/`/`textures/`; export grava
   `Download/GOneVV/export/export_<nome>.obj` (visível no gestor).
4. **Log viewer in-app (item 4)** — Settings → "Ver logs": viewer com SCROLL
   (id 43, auto-scroll para o fim) desenha o tail do `engine.log` + a lista de
   `crash-*.dump` — **funcional sem export**.
5. **Boot self-check (item 5)** — `fileapi::logStorageSelfCheck` no arranque:
   `getExternalFilesDir` (null?) + errno de cada opendir/fopen falhado no
   `engine.log` — a CAUSA de qualquer falha de storage fica registrada.
6. **Fallbacks (item 6)** — recusa ou API < 30 → modo **app-private**
   (`getExternalFilesDir`, sem permissões); o Settings mostra sempre
   "armazenamento: all files / app-private" + botão "Acesso a ficheiros…".
7. **Testes CI (item 7)** — 26 novas verificações no hospedeiro: constantes
   do fluxo, resolveMode, diálogo (mostrado 1×/permitir/cancelar/negado),
   intent (consumeOpenSettings), permissão verificada (onSettingsReturn +
   ação retomada), fallback sem suporte, File API (lista/roundtrip/errno),
   self-check, readTail/listDumps, e 4 testes de UI (diálogo, import,
   viewer, settings) com fonte real e taps injetados.

SAF removido: `SafIo.java`, `SafIoJni.cpp`, `core/SafStorage` e o item
"Pasta (SAF)" saíram; `docs/SAF_EXCEPTION.md` documenta o All Files Access
como caminho primário (com nota de Play Policy). A ponte Java mínima
(VvActivity) continua — `NativeActivity` não lança a janela de permissões nem
reencaminha `onActivityResult`.

## Escopo F5.1-hotfix (implementado)
Objetivo: o 0.6.0 crasha no arranque no C33 e o dono não tem PC/logcat — a
engine passa a **autodiagnosticar-se em ficheiros legíveis** e a ponte JNI/SAF
foi auditada e corrigida (docs/AUDIT_SAF_JNI.md).

1. **Log writer (parte 1.1)** — `vv::elog::info/warn/error` faz DUAS coisas:
   `__android_log_write` (logcat) + append a `Android/data/vv.goni/files/logs/`
   (= `getExternalFilesDir("logs")`) com rotação por tamanho: 3 ficheiros de
   1MB (`engine.log` + `.1` + `.2`, o mais velho descartado). Thread-safe;
   ativo desde a 1ª linha (o `JNI_OnLoad` usa o caminho fallback antes do
   `android_main`).
2. **Boot progress (parte 1.2)** — cada passo crítico do arranque escreve um
   marcador: `[boot 2/6] storage`, `[boot 5/6] physics` (android_main),
   `[boot 1/6] contentRect`, `[boot 3/6] fonts`, `[boot 4/6] renderer`,
   `[boot 6/6] scene OK → editor` (INIT_WINDOW). O dono vê EXATAMENTE onde o
   boot parou.
3. **Crash dump (parte 1.3)** — handlers C++ para SIGSEGV/SIGABRT/SIGBUS/
   SIGFPE (SA_ONSTACK + sigaltstack): recolhem o stacktrace via
   `_Unwind_Backtrace` (bionic `backtrace()` só existe API 33+; C33 é 31/32)
   e resolvem módulo+função+offset com `dladdr()` → `crash-<timestamp>.dump`
   no MESMO diretório, formato legível sem ndk-stack
   (`#03 pc 0x1a2b3c  libgoni_vv.so (função+0x88)`) + pc/lr/sp do fault
   (arm64). Ativo em TODAS as builds (release incluída); re-raise mantém o
   tombstone do sistema.
4. **Export de logs (parte 1.4)** — botão **Settings → "Exportar logs"**:
   copia TODOS os ficheiros de `getExternalFilesDir("logs")` para
   `Downloads/GOneVV/logs/` via MediaStore (API 29+, SEM
   WRITE_EXTERNAL_STORAGE; export repetido substitui). Caminho definido pela
   constante única `vv::elog::kDownloadsRelPath` (afervel no CI).
5. **Auditoria JNI/SAF (parte 2)** — evidência no APK 0.6.0 real: manifest,
   dex e símbolos OK (crash é runtime). Fixes: `JNI_OnLoad` com
   `RegisterNatives` explícito (falha morre no arranque COM log), higiene de
   exceções JNI em toda a ponte, resultado SAF DIFERIDO para o thread da
   engine (o handler antigo corria no thread da UI e chamava GL sem contexto
   EGL), guardas nulos + logging por método. Detalhe em
   **docs/AUDIT_SAF_JNI.md**.

## Escopo F5 (implementado)
- **Projeto e storage (F5-A)**: `core/ProjectStorage` (interface única —
  F5.2/SAF entra como outra implementação) + `core/FsStorage` (POSIX; no
  device a raiz é `getExternalFilesDir()`, sem permissões) + `core/Project`
  (manifesto `project.goni`: nome/versão/cenas/settings verbatim; refs
  RELATIVOS; `scenes/` `meshes/` `textures/`). Boot abre o projeto existente
  ou cria "projeto"; a cena ativa volta do disco a cada arranque; Menu
  Save/Load opera no projeto. Guarda de caminhos: `../`, absoluto e `//`
  recusados em toda a interface.
- **Import OBJ (F5-B)**: `assets/ObjImporter` (v/vn/vt, todas as formas de
  canto, fan de polígonos, dedup de cantos, grupos g/o + usemtl com ranges,
  índices negativos, CRLF, limite u16 sobre vértices ÚNICOS) +
  `assets/ObjExporter` (round-trip testado) + `assets/ResourceManager`
  (cache CPU por ref: 1 carga, ponteiro estável).
- **Import glTF/GLB (F5-C)**: `assets/GltfImporter` — accessors
  bounds-checked (POSITION/NORMAL/TEXCOORD_0, índices u8/u16/u32),
  bufferViews com byteStride, data URIs base64, buffers externos relativos à
  pasta do .gltf, container GLB validado, materiais básicos (name +
  baseColorFactor), nós → hierarquia (TRS). `assets/GltfInstantiate` cria
  TICs com parent/Transform3D/MeshRenderer com ref `path#i`.
- **Texturas (F5-D)**: `assets/PngLoader` (stb_image vendor, RGBA 8-bit) com
  gate 1K/2K OK, 4K+ reduzido para 2K com aviso; `assets/TextureCompressor`
  (interface; passthrough na F5 — ASTC/ETC2 é F5.1); `render/Texture`
  (glTexImage2D + glGenerateMipmap, LINEAR_MIPMAP_LINEAR, REPEAT).
- **Export + editor (F5-E)**: Menu ganha "Export OBJ" (mesh do TIC →
  `meshes/export_<tic>.obj`); Inspector mostra `mesh:`/`tex:` com a origem do
  asset e abre SELETORES de `meshes/` e `textures/`; status line mostra
  objetos de GPU (`am`/`at`); `render/GpuAssets` garante 1 ref = 1 objeto GL
  (memória de GPU não duplica); `MeshRenderer` persiste `meshPath`/`texPath`
  no `.goni` com resolvers no load.
- CI: parsers OBJ/glTF/GLB, round-trip, cache, caminhos relativos e gate 4K
  cobertos por testes (196 no total).

Sem skinning/animação (F7), sem editor de materiais (F8), sem streaming (F9),
sem compressão ASTC/ETC2 (F5.1), sem SAF (F5.2) — CLÁUSULA CALMA.

## Escopo F4.2 (implementado)
Três bugs vistos no C33, corrigidos com CLÁUSULA CALMA (só layout/UiContext/
EditorUi + testes; zero física nova, zero componentes novos, zero render 3D;
mono/3 botões/landscape intactos):
- **B1 — safe-area (fix raiz do scroll morto)**: a UI assumia a superfície
  EGL inteira, mas a nav bar tapava o fundo dos painéis → o Inspector media
  contentHeight 536 contra um visibleHeight inflado (540) → maxOffset 0 →
  scroll nunca ativava. O main lê `android_app->contentRect`
  (`APP_CMD_CONTENT_RECT_CHANGED`) e injeta `safe::Insets` (novo
  `ui/SafeArea.h`, GL-free, fonte única das constantes de layout) — toolbar,
  painéis, viewport, status line, toast, TouchControls e overlays vivem
  DENTRO do contentRect; com a altura real do painel o overflow é detetado e
  o scroll ativa. Diagnóstico no logcat: `safearea: surface … content …
  insets …`
- **B2 — labels cortadas**: novo `ui/TextFit.h` (GL-free) +
  `UiContext::labelFitted` — toda label mede antes de desenhar e trunca com
  reticência ASCII `...` (o atlas não tem U+2026) mantendo o maior prefixo
  que caiba: nome do TIC, `body: rigid - sphere - chao: sim` (o caso
  reportado), mesh/input/tc, texto dos botões (linhas da Hierarchy), status
  line e toast
- **B3 — sandbox do Play**: novo `core/PlaySnapshot.h` — ENTRAR no Play
  captura pos/rot/scale de todos os Transform3D + velocity/grounded de todos
  os BodyComp; SAIR repõe a pose de editor EXATA e descarta a simulação
  (TIC destruído no Play é saltado; handle generacional nunca recria). A
  física (TickGroup::Physics) continua a correr SÓ durante o Play (gate
  `enabled` inalterado)
- CI: fix do workflow — o output `artifact_name` era descartado pelo
  mascaramento do secret `VV_KEY_ALIAS` ("vv"); o gate verify-entry-symbols
  passa a baixar por `pattern` (robusto a segredos e a mudanças de versão)

## Escopo F4.1 (implementado)
Scroll como primitivo de UI, aplicado ao Inspector e à Hierarchy — conteúdo
mais alto que o painel fica alcançável (desbloqueia a verificação da F4 no
device). Só scroll (CLÁUSULA CALMA): sem safe-area, sem colapsáveis, sem
física, sem render 3D.
- `ui/ScrollMath.h` (GL-free, testada no CI): clamp `[0, content − visible]`,
  drag-to-scroll com clamp nos dois extremos, limiar tap (12 px) vs drag,
  protocolo de claim (slider > scroll > botão dentro de região), clip de
  quads por interseção com UV proporcional, geometria do indicador
- `UiContext::beginScroll(id, region, contentHeight)` / `endScroll()` —
  slots por id (offset persiste), botões dentro da região só desenham (o tap
  é re-despachado pelo painel — "add TouchControls" clicável mesmo após
  scroll), sliders mantêm prioridade, indicador fino ACCENT só com overflow;
  recorte por interseção (sem glScissor no quad batch único)
- Hierarchy: lista TODOS os TICs (fim do corte em ~10 linhas da F3); tap na
  linha seleciona via `hierarchyRowAtTap`
- Inspector: BodyComp, `velx` e o botão "add TouchControls" no fundo ficam
  sempre alcançáveis; `ui/EditorLayout.h` é a fonte ÚNICA das alturas de
  conteúdo (desenho e testes partilham os números)
- Gestos sem conflito: drag na região = scroll; drag no slider = slider;
  tap = seleção/ação; viewport central NÃO é região de scroll (orbit/pinch
  intactos)
- CI: suíte do core (122 testes) + APK assinado

## Escopo F4 (implementado)
Física core num novo `TickGroup::Physics` (entre Update e PostUpdate) —
`physics/` é C++ puro, host-testável, sem GL:
- Formas: `Sphere`, `AABB`, `OBB` (rotQuat), `Capsule` (eixo Y local do TIC);
  interseções puras (sphere×{sphere,AABB,OBB,Capsule}, AABB×AABB, OBB×OBB SAT
  15 eixos, Capsule×Capsule) e sweep contínuo com TOI+normal para sphere/
  capsule contra AABB/OBB com CCD adaptativo (substeps máx 8 + avanço
  conservador exato via gradiente da SDF — nunca tunela paredes finas)
- `BodyComp` (Static/Character/Rigid + shape variant + velocity + grounded);
  `PhysicsSystem`: Character com input e slide estilo `move_and_slide`
  (remove a componente normal; iterações extra para cantos), repel entre
  Characters, Rigid com gravidade + colisão primitiva contra Static +
  amortecimento no chão. PLACEHOLDER DE SOLVER: dois Rigid empilhados
  intersectam (sem stacking/resting, documentado)
- `TouchControls` mínimo e FIXO (joystick esquerda + botão JUMP direita, tema
  mono, desenhado só em modo Play) como fonte de `InputSource`; `InputMap`
  liga zero ou uma fonte (sem fonte → sem input); UI criável = F6
- Presets: Player/Character/Static ganham Body, NOVO RigidBody3D; TouchControls
  adicionável via Inspector; Inspector mostra BodyComp (tipo·forma·chão) com
  slider `velx` para lançar corpos no device (testes de CCD/empurrão)
- CI: suíte do core (113 testes) + APK assinado

## Escopo F3.1 (implementado)
A pedido do dono no C33: remover as barreiras artificiais de câmara e baratear
o grid — só câmara + grid + testes (CLÁUSULA CALMA; nada de componentes,
presets, serialização, física ou UI de editor).
- Câmara: zoom 1..300 (era 1.5..40), pitch ±89° (era ±83° — top-down quase
  total sem degenerar o lookAt), yaw livre; near 0.5 / far 450 (zoom máx ×
  1.5, rácio 900:1 — sem z-fighting no depth de 24 bits)
- Grid: a malha de linhas virou UM quad de 4 vértices que segue a câmara, com
  linhas procedurais no fragment shader (`fract` + `fwidth`), anti-moiré por
  minificação e fade radial adaptativo ao zoom; extent ≥ 2000, borda nunca
  visível; interface (`init/draw/vertexCount/ok`) e tema mono mantidos
- CI: suíte do core (74 testes) + APK assinado; o build NDK valida o GLSL
  embutido

## Escopo F3 (implementado)
- `components/`: `Transform3D` (pos/Quat/scale + cache world TRS), `MeshRenderer`
  (Mesh* + Material* não-donos), `InputMap` (vazio — F4 preenche)
- `core/`: `Component` (owner + hooks attach/detach), `ComponentStorage<T>` SoA
  (deque — endereços estáveis), `ComponentRegistry` (nome→id→factory),
  `ComponentStore` (bolsa da Scene), `Tic` container
  (`addComponent<T>/getComponent<T>/removeComponent<T>`), `Presets`
  (PlayerBody3D/CharacterBody3D/StaticBody3D, nomes únicos .001),
  `Tick` (PreUpdate→Update→PostUpdate→Render), `TransformSystem`,
  `Json` mínimo + `SceneSerializer` (.goni v1, migração v0→v1, tipos
  desconhecidos ignorados no load)
- `math/`: `Quat` (unitário, euler YXZ, `toMat4` reproduz `rotX/Y/Z` da F2)
- `ui/`: `UiContext.slider` + `EditorUi` — Hierarchy esquerda (seleção + botão
  "+"), Inspector direita (9 sliders pos/rot graus/scale), overlays
  ("+" → 3 presets; Menu → Save/Load), toasts; tema mono; 3 botões F1 mantidos
- `platform/main.cpp`: pass 3D desenha todos os TICs com MeshRenderer
  (fim do cubo hardcoded); TickGroups por passo fixo; gate da câmara
  (gestos só nascem no viewport central)
- CI: `verify-entry-symbols` + suíte do core (113 testes) + APK assinado

Sem física (BodyComp é F4), sem luzes, sem assets, sem animação, sem linguagem
(CLÁUSULA CALMA).

## Histórico
- **F5.4-hotfix (0.6.5)**: SAF SEM duplicação ("main.goni (1).json", "project.goni (2)" em TODO boot): (1) causa raiz = mime `application/json` para `.goni` → o provider renomeia no createDocument (FileUtils.buildUniqueFile acrescenta extensão canónica) e o nome no disco divergia do nome procurado → verificação falhava → createDocument de novo; (2) `bridgeFindFile` NOVA — pesquisa EXATA por displayName com query fresca ao provider ANTES de qualquer createDocument (método CRÍTICO do handshake; repetição em falha de query); (3) contrato TRI-ESTADO — "não sei" (provider em falha) NUNCA decide criação: `Presence::probe` em FsStorage/SafStorage + createNew só cria com ausência CONFIRMADA; (4) mime octet-stream para tudo menos .json (nome verbatim); (5) abertura "wt" (truncate) no doc EXISTENTE — nunca create por cima; (6) CURA dos projetos 0.6.4: ficheiros já renomeados ("x.goni.json") reabrem e continuam a ser usados — as cópias " (1)"/" (2)" são lixo a apagar manualmente; Salvar materializa assets em-runtime: cubo procedural → meshes/cube.obj (formato OBJ já definido, idempotente) e todo write loga `saf: write <rel> — N bytes` (visível no Ver logs); fake do SAF agora MODELA rename+colisão do provider — 279 testes.
- **F5.4 (0.6.4)**: Gestor de Projetos (SAF multi-pasta, takePersistableUriPermission por projeto, SafStorage = ProjectStorage sobre SAF, fallback app-private intacto) + fix do UnsatisfiedLinkError (JNI_OnLoad nunca corria no device — dlopen do NativeActivity não chama; static loadLibrary na VvActivity + registo idempotente pós-handshake + gate jni_parity.py que apanhou um bug latente de assinatura) — 272 testes (docs/HANDSHAKE_AUDIT.md §6).
- **F5.2 (0.6.2)**: All Files Access (diálogo → settings → File API direta) + remoção do SAF tree picker + log viewer in-app + boot self-check com errno — 254 testes.
- **F5.3 (0.6.3)**: handshake Java↔native INVERTIDO (VvActivity regista-se no native — onCreate + onResume; causa única das pontes mortas 0.6.0→0.6.2: GetEnv EDETACHED no thread do glue) + attach de threads nomeado + mensagens honestas ("ponte Java indisponível (handshake)") + gate do manifest binário no CI — 264 testes (docs/HANDSHAKE_AUDIT.md).
- **F5.1 (0.6.0)**: maturação de assets em 4 sub-blocos.
- **F5.1-hotfix (0.6.1)**: crash dump permanente + engine.log com rotação e boot progress por passos + export p/ Downloads/GOneVV/logs (MediaStore) + auditoria/fix da ponte JNI SAF (JNI_OnLoad/RegisterNatives, higiene de exceções, resultado SAF no thread da engine) — 243 testes. **A** — vendors
  etcpak 2.1 (BSD) e astc-encoder 5.3.0 (Apache-2.0), CompressedImage com
  cadeia de mips completa em CPU (blob contíguo), HardwareCompressor (ASTC
  se GL_KHR_texture_compression_astc_ldr, senão ETC2; <256px fica RGBA),
  cache em disco `textures/cache/` (header LE + hash FNV-1a do PNG; hit/miss
  contados; corrupção → miss), gate 4K REAL (com compressão 4K entra inteira
  — ETC2 4K ≈ 8 MB; fallback sem compressão reduz para 2K com aviso),
  `glCompressedTexImage2D` por nível. **B** — parser glTF lê `images`
  (data:image/png;base64 e bufferView do GLB) + `textures` + materiais com
  `baseColorTexture`; extração para `textures/gltf_<hash>.png` com DEDUP por
  hash (N materiais → 1 ficheiro); aplicar um mesh glb no Inspector aplica
  logo a textura. **C** — SAF com exceção documentada à regra zero-Java
  (docs/SAF_EXCEPTION.md): VvActivity (pickers + takePersistableUriPermission
  + JNI), SafIo sobre DocumentsContract (sem androidx), SafStorage com
  backend injetável + RoutingStorage (fallback getExternalFilesDir).
  **D** — menu: Pasta (SAF)/Importar…/Export SAF; status line mostra o
  formato da textura (etc2/eac/astc4/astc6/rgba) e hits/misses do cache;
  round-trip completo testado no CI (glb → extração → ETC2+cache → export
  OBJ → reimport → cena recarregada). 232 testes.
- **F5.0-fix (0.5.1)**: bug do C33 no Inspector (texto sobreposto em pilhas:
  nome×Transform3D, mesh/tex/input/body×sz, tc×velx) — o cursor Y SEMPRE foi
  partilhado e sequencial; a causa raiz eram as linhas de 26/30 px (baseline
  '+8') para uma fonte de 28 px: o bloco de glifos invadia a linha de cima.
  O Inspector agora vem de um PLANO único (EditorLayout.h) com cursor Y
  cumulativo, alturas derivadas das MÉTRICAS REAIS da fonte (ascent/descent
  medidos no bake), baseline centrado, contentHeight = fundo da última linha
  e tap re-despachado POR REGIÃO (a Hierarchy comia o tap do Inspector).
  UI real testada no CI com fonte embutida: 0 colisões glifo-a-glifo (o
  código antigo apanhava 13 com a mesma fonte); 203 testes.
- **F5 (0.5.0)**: projeto `.goni` com pasta (manifesto + scenes/meshes/
  textures, refs relativos, reopen persistente), import OBJ e glTF/GLB
  (hierarquia + materiais básicos), PNG com mipmaps e gate 4K→2K, export
  cena/.goni + OBJ round-trip, ResourceManager com cache (CPU e GPU sem
  duplicação), seletores de assets no Inspector; 196 testes.
- **F4.2 (0.4.2)**: correções do device — safe-area do sistema (scroll do
  Inspector ativa com a nav bar contabilizada), truncagem de labels com
  ellipsis e sandbox do Play (pose de editor restaurada ao sair); 135 testes.
- **F4.1 (0.4.1)**: scroll como primitivo de UI — Inspector e Hierarchy com
  ScrollRegion (clamp, drag-vs-tap, indicador, tap re-despachado); fundo dos
  painéis alcançável no device.
- **F4 (0.4.0)**: física core — 4 formas + sweep/CCD, BodyComp, slide do
  Character, Rigid primitivo, TouchControls (modo Play) e preset RigidBody3D.
- **F3.1 (0.3.1)**: porto do editor — clamps generosos de câmara (zoom 300,
  pitch 89°) e grid de linhas → quad de shader (4 vértices, fade adaptativo,
  anti-moiré); near/far recalibrados.
- **F1 (0.1.0)**: fundação — NativeActivity, EGL/GLES3, core (Handle/Tic/Scene/
  Time), UI immediate-mode, crash log, CI assinado.
- **F2 (0.2.0)**: primeiro 3D — Vertex/Mesh/LitMaterial, cubo procedural,
  grid com fade ("espaço infinito"), câmara de orbit (1 dedo + pinch, clamp),
  depth test, status line com vértices/draw calls.

## CI — fluxo da assinatura (uma vez)
1. Actions → **release** → *Run workflow* → preencha `store_pw` / `key_pw`
   → o job **init-keystore** gera o artifact `vv-release-keystore` (baixe e guarde OFFLINE).
2. `base64 -w0 vv-release.jks` → configure os secrets:
   `VV_KEYSTORE` (base64), `VV_STORE_PW`, `VV_KEY_ALIAS`, `VV_KEY_PW`.
3. Todo push publica o artifact `goni-vv-0.5.1-release-signed` (APK arm64).
   Sem secrets: sai `-UNSIGNED` (nunca debug).

Trocar a keystore muda a assinatura — exige desinstalar/reinstalar no device.

## Build local (opcional)
Android SDK + NDK 26.3 + CMake 3.22.1 + JDK 17 → `./gradlew assembleRelease`.

## Testes do core (Linux)
`cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests`

## Verificação no Realme C33 (dono) — F5.1-hotfix (diagnóstico sem PC)
1. Instalar o APK **0.6.1** → abrir. **Se abrir**: ir a
   `Android/data/vv.goni/files/logs/` (gestor de ficheiros) → `engine.log`
   deve mostrar a sequência completa:
   `[boot 2/6] storage OK` … `[boot 1/6] contentRect OK` …
   `[boot 3/6] fonts OK` … `[boot 4/6] renderer OK` …
   `[boot 6/6] scene OK → editor`.
2. **Export**: toolbar **Settings → "Exportar logs"** → toast "logs
   exportados: N" → abrir **Downloads/GOneVV/logs/** no gestor de ficheiros →
   `engine.log` visível e abrível no telefone.
3. **Se ainda assim crashar**: o mesmo gestor de ficheiros mostra
   `Android/data/vv.goni/files/logs/crash-<data>.dump` (e a marca `CRASH` no
   fim do `engine.log`) — enviar o dump; ele diz o SINAL e a função C++
   exata com offset. Exportar também pelo botão Settings (o dump vai junto).
4. **SAF depois do fix**: Menu → Pasta (SAF) → escolher pasta → a app
   continua viva (o resultado agora é processado no thread certo) → Importar
   um .obj/.glb/.png → Export SAF. Reiniciar → pasta SAF reaberta.
5. **Regressões**: F5.1 (status line `etc2/astc4`, cache `c1/1`, glb com
   textura), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — 0.6.7 (lifecycle GL + gestão de projetos)

Instalar o APK 0.6.7 (artifact `goni-vv-0.6.7-release-signed` do CI). O
roteiro cobre os três fixes; o log viewer in-app (Settings → Ver logs)
mostra as linhas `lifecycle:` se algo falhar.

**A — lifecycle GL (cubinhos):**
1. Abrir um projeto no gestor → criar/mover um TIC → **botão home** (ou
   recents) → voltar à app SEM a matar;
2. Esperado: o texto (toolbar, Hierarchy, Inspector, status line) renderiza
   NORMAL — sem quads brancos;
3. Settings → Ver logs: procurar `lifecycle: INIT_WINDOW #2 — contexto
   EGL RE-CRIADO` e `[boot 3/6] fonts OK … RE-UPLOAD no contexto novo`;
   e no sair: `lifecycle: TERM_WINDOW #1 — … NENHUM recurso GL assumido
   vivo`.

**B — apagar projeto (com confirmação):**
1. No gestor, criar um projeto de teste (ex.: "lixo") com uma pasta à
   escolha → entrar nele → criar um TIC → Menu → Sair para projetos;
2. Long-press na entrada "lixo" → "Apagar projeto" → confirmação
   ("Não pode ser desfeito") → "Apagar";
3. Esperado: Toast "projeto apagado", a entrada SAI da lista e a pasta
   escolhida desaparece do gestor de ficheiros do sistema;
4. Repetir com "Remover da lista" noutro projeto: a pasta PERMANECE
   (comportamento antigo intacto).

**C — Sair para projetos (sem matar a app):**
1. No editor: mover/rodar um TIC → Menu → "Sair para projetos";
2. Esperado: volta ao GESTOR (a app não fecha/reinicia — sem splash), a
   cena foi auto-salva (toast "cena salva — a sair…");
3. Reentrar no MESMO projeto: cena carregada com o TIC na pose deixada,
   texto normal, física a 1× velocidade (play não acelerado);
4. Ver logs: `editor: sair p/ projetos — auto-save OK` e `lifecycle:
   REENTRADA do android_main`.

**D — nome do projeto visível:**
1. Gestor → "+ Novo projeto" → digitar: o texto digitado TEM de estar
   visível (campo escuro, texto claro, cursor a piscar).

## Verificação no Realme C33 E RMX3624 (dono) — F5.4-hotfix (0.6.5)
> A regressão do handshake APARECEU no RMX3624 (Android 13) — a fase só
> fecha VERIFIED depois de passar nos DOIS devices.

1. Instalar o APK **0.6.5** (por cima da 0.6.4 SEM apagar os dados — os
   projetos da lista continuam lá).
2. **ANTI-DUPLICAÇÃO (o bug desta fase)**: abrir um projeto criado pela
   0.6.4 (ou criar um novo) → adicionar/editar algo → Salvar → fechar a
   app (swipe) → abrir de novo → Salvar outra vez → repetir 2–3×. NO
   gestor de ficheiros, a pasta do projeto tem de continuar com EXATAMENTE
   um `project.goni`, um `scenes/main.goni` — NENHUM ` (1)`/` (2)`. O
   projecto 0.6.4 que tinha `project.goni.json`/`main.goni.json` continua
   a abrir (cura) SEM criar ficheiros novos; as cópias ` (1)`/` (2)` antigas
   são lixo inofensivo — podem ser apagadas à mão.
3. **Salvar materializa assets**: criar um TIC (preset Cube) → Menu →
   Salvar → Settings → Ver logs: tem de aparecer
   `saf: write meshes/cube.obj — N bytes` (e `file: write …` no modo
   app-private). No gestor de ficheiros: `meshes/cube.obj` existe com
   conteúdo (OBJ de texto — abre em qualquer editor). Salvar de novo NÃO
   reescreve (idempotente).
4. **Ecrã inicial = "Projetos"** (NÃO o editor). **Criar projeto**: "+
   Novo projeto" → nome → seletor de pastas (SAF) → Documents/JogoA →
   editor. No log viewer: `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` → `jni: handshake OK — … saf=5/5` →
   `java: openProject → fila` → `projeto: '<nome>' pronto (SAF)`.
5. **2º projeto em pasta DIFERENTE** (Documents/JogoB) → ambos abrem
   independentemente (nada se mistura).
6. **Persistência**: fechar a app (swipe) → abrir de novo → a lista
   continua; tocar num projeto abre-o com as cenas gravadas — e ao Salvar
   NÃO crescem ficheiros na pasta (ponto 2).
7. **Sem All Files Access**: os projetos funcionam COMPLETOS (save/load/
   scene) SEM conceder All Files Access.
8. **All Files Access coexiste**: num projeto aberto, Menu → Importar… →
   import de Download/Documents para meshes//textures/ (subpasta certa);
   export para Download/GOneVV/export (igual 0.6.3).
9. **Mensagens honestas**: se algo falhar por ponte, o toast/log diz
   **"ponte Java indisponível (handshake)"** — nunca "sistema sem suporte".
10. **Sem UnsatisfiedLinkError**: no engine.log a linha
    `jni: JNI_OnLoad — G.One VV 0.6.5 … registado(s)` aparece ANTES do
    `java: onCreate → nativeRegisterActivity`.
11. **Regressões**: F5.4 (gestor, multi-pasta), F5.3 (import/export All
    Files), F5.2 (log viewer, modo no Settings), F5.1 (status line,
    cache), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.3 (histórico)
1. Instalar o APK **0.6.3** → confirmar "0.6.3" nas infos.
2. **Handshake no log viewer** (SEM PC): abrir a app → Settings → **"Ver
   logs"** → a sequência completa tem de aparecer:
   `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` →
   `jni: handshake OK — vm=… openAllFiles=1 exportLogs=1` →
   `storage: All Files Access — handshake=1 supported=1 manager=? …`
   (`manager=1` se a permissão já estava concedida; `0` na 1ª instalação).
3. **Import**: Menu → **Importar…** → o diálogo "Precisa de acesso a todos
   os ficheiros…" aparece → **Permitir** → abre a janela de permissões DO
   app ("All files access") → ativar o interruptor → voltar → toast "acesso
   concedido — File API direta" e a lista de Download/Documents abre (import
   retomado). 4. **Export**: com um TIC com mesh → Menu → **Export
   Downloads** → toast "exportado: Download/GOneVV/export/…" e o OBJ no
   gestor de ficheiros.
5. **Mensagem honesta (se algo falhar)**: se a ponte Java estiver em baixo,
   o toast/log diz **"ponte Java indisponível (handshake)"** — NUNCA
   "sistema sem All Files Access" (a causa real). O C33 (Android 12) SUPORTA
   All Files Access — se viu essa mensagem no 0.6.2, era a mentira antiga.
6. **Regressões**: F5.2 (diálogo/perm/fallback/log viewer), F5.1 (status
   line `etc2/astc4`, cache `c1/1`), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.2 (All Files Access + log viewer)
1. Instalar o APK **0.6.2** → confirmar "0.6.2" nas infos.
2. **Diálogo**: Menu → **Importar…** → aparece o overlay "ARMAZENAMENTO —
   Precisa de acesso a todos os ficheiros para importar/exportar projetos"
   com **Permitir / Cancelar** (na 1ª tentativa de import/export).
3. **Settings do sistema**: tocar **Permitir** → abre a janela de permissões
   DO G.One VV ("All files access"); ativar o interruptor → voltar à app →
   toast "acesso concedido — File API direta" e a lista de ficheiros de
   Download/Documents abre em "IMPORTAR" (o import da ação pendente é
   RETOMADO automaticamente).
4. **Import**: tocar num .obj/.glb/.png da lista → toast "importado:
   meshes/…" (ou textures/) e o ficheiro aparece nos seletores do Inspector.
5. **Export**: selecionar um TIC com mesh → Menu → **Export Downloads** →
   toast "exportado: Download/GOneVV/export/export_…" → abrir
   **Download/GOneVV/export/** no gestor de ficheiros → o OBJ está lá.
6. **Log viewer**: Settings → **"Ver logs"** → o viewer mostra o tail do
   engine.log com scroll (abre no FIM) + os crash dumps; **sem export**.
   A linha "self-check: … getExternalFilesDir=… fopen(…) OK" aparece no
   arranque do log; se algo falhar, a linha diz `errno=N (causa)`.
7. **Modo no Settings**: Settings mostra "armazenamento: all files" (após
   conceder) ou "app-private" (se recusar); **"Acesso a ficheiros…"** abre a
   janela de permissões a qualquer momento.
8. **Fallback**: recusar no diálogo → toast "sem acesso — modo app-private";
   os fluxos Save/Load/Export OBJ do projeto continuam a funcionar
   (app-private não precisa de permissões).
9. **Regressões**: F5.1 (status line `etc2/astc4`, cache `c1/1`, glb com
   textura embutida), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.1 (assets maduros)
1. Instalar o APK 0.6.1 → confirmar "0.6.1" nas infos.
2. **Compressão**: importar um PNG 2K/4K (Menu → Importar…) → aplicar como
   textura → a status line mostra `etc2` (ou `astc4`/`astc6` no Mali do C33)
   no fim da linha; logcat `GpuAssets: textura … 4096x4096 ETC2 RGB via
   compress` (sem "reduzida").
3. **Cache**: aplicar a MESMA textura outra vez (ou reiniciar a app e
   aplicar) → status line mostra `c1/1` (1 hit, 1 miss) e o logcat diz
   `via cache`. Alterar o PNG e importar de novo → nova entrada (hash novo).
4. **glTF/GLB com textura embutida**: importar um .glb com textura interna →
   aplicar no TIC → toast "mesh aplicado (+textura)" e o modelo aparece
   texturizado; a pasta do projeto passa a ter `textures/gltf_<hash>.png`.
5. **SAF**: Menu → **Pasta (SAF)** → escolher/criar uma pasta no armazenamento
   → toast "pasta do projeto ativa"; Menu → **Importar…** → escolher um
   .obj/.glb/.png → toast "importado: …" e aparece nos seletores; Menu →
   **Export SAF** → gravar o OBJ noutro sítio (Downloads, p.ex.) → toast
   "exportado: …". Fechar a app e reabrir → a pasta SAF volta a ser a raiz
   do projeto (URI persistida).
6. **Regressões F5/F4.2**: cena Save/Load, gate 2K sem compressão (toast de
   aviso — agora só no fallback), Play snapshot, scroll do Inspector.

## Verificação no Realme C33 (dono) — F5.0-fix (Inspector sem sobreposição)
1. Instalar o APK 0.5.1 → confirmar "0.5.1" nas infos da app.
2. **Sem sobreposição**: selecionar o PlayerBody3D (com TouchControls e um
   asset importado) → no Inspector o nome, `Transform3D`, os 9 sliders,
   `mesh:`, `tex:`, `input:`, `body:`, `velx` e `tc:` estão cada um na SUA
   linha — nada desenhado em cima de outra linha.
3. **Scroll por cima**: arrastar para cima no painel → o conteúdo desce
   inteiro (indicador à direita); o ÚLTIMO campo (tc) chega ao fundo sem
   corte e sem saltar.
4. **Sliders/taps com scroll**: arrastar um slider horizontalmente → muda o
   valor e NÃO faz scroll; tap em `mesh:` / `tex:` abre o seletor; tap em
   `add TouchControls` cria o componente.
5. Regressão F5: repetir a verificação da F5 abaixo (projeto, import, gate
   4K, export, cache) — nada mudou nesses fluxos.

## Verificação no Realme C33 (dono) — F5 (projeto + assets)
1. Instalar o APK 0.5.1 → confirmar "0.5.1" nas infos da app.
2. **Projeto**: primeiro arranque cria a estrutura
   (`adb shell ls /sdcard/Android/data/<pkg>/files/` → `project.goni`,
   `scenes/`, `meshes/`, `textures/`); criar TICs → Menu → Save cena →
   fechar a app → reabrir → os TICs VOLVEM (cena ativa do projeto).
3. **Import**: `adb push esfera.obj /sdcard/Android/data/<pkg>/files/meshes/`
   e `adb push madeira.png .../textures/` → abrir a app → selecionar um TIC →
   no Inspector tocar em `mesh: cube` → escolher `esfera.obj` no seletor →
   o mesh importa e renderiza num TIC; tocar em `tex: none` → escolher
   `madeira.png` → a textura aplica no material.
4. **Gate 4K**: empurrar uma textura 4096×4096 → ao aplicar aparece o toast
   "textura 4096x4096 reduzida para 2048x2048 (gate 2K; compressao real =
   F5.1)" (1× por carga).
5. **Export**: selecionar o TIC com asset → Menu → Export OBJ → toast
   `export: meshes/export_<nome>.obj`; `adb shell ls .../meshes/` confirma.
6. **Cache de GPU**: dois TICs com o MESMO asset → status line mostra
   `am 1` (um objeto de GL, não dois).
7. **Regressões**: safe-area/scroll/labels/sandbox da F4.2 continuam
   funcionando; física continua só no Play; tema mono, 3 botões, landscape.

## Verificação no Realme C33 (dono) — F4.2 (safe-area + labels + sandbox)
1. Instalar o APK 0.4.2 → confirmar "0.4.2" nas infos da app.
2. **B1/scroll**: selecionar o PlayerBody3D → com a nav bar visível, o
   Inspector agora DETETA o overflow (indicador fino à direita) → arrastar
   para cima revela `body: …`, `velx` e o botão **add TouchControls** no
   fundo, SEM nada tapado pela nav bar; toolbar, status line e painéis todos
   dentro da área visível. (Opcional: `adb logcat | grep safearea` mostra a
   superfície, o contentRect e os insets detetados.)
3. **B2/labels**: com um Rigid no chão, a linha `body: rigid - sphere - chao:
   sim` aparece inteira OU termina em `...` — nunca cortada a meio; nomes
   longos de TIC na Hierarchy terminam em `...` dentro do botão.
4. **B3/sandbox**: dar **Play** → deixar o corpo cair/mover (stick + JUMP) →
   sair do Play → os TICs voltam EXATAMENTE à pose de editor (o corpo
   "desce" de volta ao sítio original); entrar/sair repetidas vezes mantém
   a pose estável.
5. **Regressões**: orbit/pinch no viewport central intactos; gestos atrás da
   nav bar não orbitam a câmara; scroll dos painéis continua com limites no
   topo/fundo; slider continua com prioridade sobre o scroll.

## Verificação no Realme C33 (dono) — F4.1 (scroll)
1. Instalar o APK 0.4.1 → confirmar "0.4.1" nas infos da app.
2. **Inspector**: selecionar o PlayerBody3D → arrastar PARA CIMA na lista →
   o conteúdo desce e revela `body: …`, `velx` e o botão **add TouchControls**
   no fundo → tocar no botão (clicável após o scroll) → aparece "tc: stick +
   jump"; o indicador fino aparece à direita só quando há overflow.
3. **Hierarchy**: criar 12+ TICs (repetir "+") → arrastar a lista → TODAS as
   linhas alcançáveis; tap numa linha que estava cortada seleciona (frame
   branco + Inspector mostra o TIC).
4. **Gestos sem conflito**: arrastar na pista de um slider (px/py/velx) muda
   o valor e NÃO faz scroll; arrastar fora da pista faz scroll; o viewport
   central continua a orbitar/pinçar como sempre.
5. **Limites**: no topo e no fundo o scroll para (não passa do fim); soltar
   sem mover = tap (não scrolla).

## Verificação no Realme C33 (dono) — F4
1. Instalar o APK 0.4.0 → confirmar "0.4.0" nas infos da app.
2. **Montar a arena** (modo editor):
   - Chão: **+** → StaticBody3D → Inspector: `py −0.5`, `sx 5`, `sy 1`, `sz 5`
     (OBB 5×1×5 com topo em y=0).
   - Parede fina rotacionada: **+** → StaticBody3D → `px 1.85`, `py 0.5`,
     `sx 4`, `sy 1`, `sz 0.2`, `ry 30`.
   - Player: **+** → PlayerBody3D (nasce em (0, 0.55, 0)).
3. **TouchControls**: com o player selecionado → Inspector → botão
   **add TouchControls** → tocar **Play** (toast "modo play") → joystick à
   esquerda empurra o player; botão **JUMP** salta quando `grounded`;
   empurrar contra a parede → **colide e desliza**, nunca atravessa.
4. **CCD**: em modo editor, selecionar o player → `velx 40` → **Play** →
   o player é lançado contra a parede fina e **não tunela** (para/encosta).
5. **Rigid**: **+** → RigidBody3D → `py 3` → **Play** → cai e **para no chão**;
   `velx 8` → desliza e abranda; empurrar o player contra a bola → bloqueia
   (empurrão = `velx` no Inspector). Dois Rigid empilhados intersectam
   (PLACEHOLDER de solver aceite).
6. **Modo editor**: sem Play não há painel de controlos nem física; orbit
   (1 dedo) e pinch continuam; gestos nos controlos não giram a câmara.
7. Menu → **Save/Load cena** persiste os corpos (BodyComp round-trip).
8. Status line: fps/tics/verts/dc; crash log em
   `/data/user/0/vv.goni/files/goni_crash.log`.

## Verificação F3.1 (câmara + grid)
- Pinch afasta até 300 sem a borda do grid aparecer; aproxima até ~1; orbit
  até ~89° sem inversão; sem z-fighting; grid custa 4 vértices (status line).
