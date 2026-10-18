# CODEMAP.md — onde vive o quê

Atualizado: 2026-10-19 · vc: 95

> 0.12-E (E-1 · EMERGÊNCIA): o shader SDF de widget (D-1), o vidro
> real/falso (D-2) e o fogo do shader único (D-3) estão DORMENTES — a
> ordem do dono abortou o SDF 3D (estragou texto/layout no device). Os
> símbolos continuam no código com ZERO chamadores no produto (o
> app/src/main/cpp/ui/Sdf.h, o app/src/main/cpp/render/SdfBatch.h, o
> app/src/main/cpp/ui/Glass.h, o app/src/main/cpp/ui/Fire.h, o
> progSdf_/captureGlass do app/src/main/cpp/render/Renderer.cpp e os
> emissores UiContext::sdfWidget/sdfWidgetGlass/glassPanelFake/
> sdfFireBorder/fireGlowBelow/drawEmbers).
> O desenho do produto voltou ao SIMPLES 2D (o padrão Y-1: painel +
> gradiente + frame + glassTopLine). A FASE 3 do E (E-3) ligou os
> EFEITOS 2.5D do dono por cima: o app/src/main/cpp/ui/Noise2D.h (a
> FONTE ÚNICA do ruído tileável + o fireScrollOffset), a textura de
> ruído GL_REPEAT no init do app/src/main/cpp/render/Renderer.cpp, o
> emissor UiContext::fireBorder2D (as 4 bandas no batch de imagens), o
> token dropShadow (a sombra 2.5D no drawButtonVisual/iconButton), o
> surface α0.85 e o glassTopLine ÂMBAR a 20%. O LOGO de D-4 (Brand.h/.cpp) segue
> VIVO (segmentos 2D — nunca foi SDF).
>
> Convenção de paths: TODOS os caminhos deste ficheiro são relativos à
> RAIZ do repo (exemplo: o registo da linguagem vive em
> app/src/main/cpp/voni/VoniRegistry.cpp) — nunca relativo à pasta cpp/,
> nunca só o nome da camada. O índice é ÍNDICE, não é a verdade — se
> divergir do código, investiga, reporta, corrige. Nunca inventa.

## §1 Quero... (mapa rápido)

| Quero... | Onde | Símbolos-chave |
|---|---|---|
| mexer no editor de script | app/src/main/cpp/ui/ScriptEditor.cpp | (UI da 0.9.2) |
| mexer na linguagem V.ONI | app/src/main/cpp/voni/VoniRegistry.cpp | `kEntries` |
| mexer no runtime V.ONI | app/src/main/cpp/voni/VoniVm.cpp | `kCommands` |
| mexer nos linker/tyker | app/src/main/cpp/voni/VoniTykers.cpp | o motor dos tykers |
| serializar a cena (.goni) | app/src/main/cpp/core/SceneSerializer.cpp | `dump` / `loadText` |
| mexer no formato .gmesh | app/src/main/cpp/assets/GOwnFormats.cpp | `writeGMesh` / `readGMeshV3Meta` |
| mexer no import streaming | app/src/main/cpp/assets/GmeshV3Stream.cpp | conversor v3 |
| render por blocos | app/src/main/cpp/render/BlockMesh.cpp | `open` / `materializeBlock` / `draw` |
| impostores (3.75b) | app/src/main/cpp/assets/ImpostorIndex.cpp + app/src/main/cpp/render/Impostor.h | `ensureImpostor` / `relFor` |
| instancing de blocos (3.5b) | app/src/main/cpp/render/BlockMesh.cpp | draw por assinatura, teto 64 |
| a despachante principal | app/src/main/cpp/platform/main.cpp | drawTics, ordem por frame |
| adicionar um TU novo | DOIS CMakeLists (ver §3) | — |
| a sentinela R-NNN | tests/test_sentinels.cpp | `regress_*` |

## §2 Mapa por camada

- **app/src/main/cpp/voni/** — a linguagem V.ONI: VoniRegistry (a fonte
  única dos metadados), VoniCompile, VoniVm (o runtime, `kCommands`),
  VoniTykers (o motor dos linkers/tykers), VoniEngineHost (a ponte com o
  motor), VoniHighlight/VoniDocs (editor + docs). A referência gerada é
  VONI_referencia.md (+ llms.txt/llms-full.txt) — regenerada pelo alvo
  `voni_refgen` de tests/CMakeLists.txt.
- **app/src/main/cpp/core/** — o motor puro: Scene/Tic (a unidade é TIC),
  SceneSerializer (`.goni`: `dump`/`loadText`), ProjectStorage/FsStorage/
  SafStorage (SAF), Tick.h (o `TickGroup` + `TickGroups::add`), UndoStack,
  VoniSystem, Bench (R-017), AssetPersist, SceneTic.h (TIC + Transform3D).
- **app/src/main/cpp/render/** — o desenho: Renderer, Mesh, Camera, Grid,
  Cube/Primitives, Material, Texture, GpuAssets (+ `impostorTexture`),
  BlockMesh (o render POR BLOCOS do .gmesh v3), Impostor.h (a decisão
  PURA do impostor), QuadBatch, DrawStats, ThumbPng (PNG via zlib).
  **0.12-D (D-1 — DORMENTE desde a 0.12-E)**: app/src/main/cpp/render/SdfBatch.h (o batch dos WIDGETS SDF — UM
  widget = UM quad de 18 floats) + o 3.º PROGRAMA no Renderer
  (progSdf_ — o FS kSdfFsSrc replica o app/src/main/cpp/ui/Sdf.h::shade() LINHA A LINHA;
  as submissões SDF viajam nos runs pelo flag `sdf` e os buffers/VAOs
  re-afirmam-se POR submissão). **0.12-D (D-3)**: o ramo FOGO-ONLY no FS
  (o aShape.w=1 — a chama na banda da aresta em MISTURA ADITIVA
  GL_SRC_ALPHA/GL_ONE; os uniforms uSdfTime/uFire1/uFire3; o run leva o
  flag blendAdd e o endFrame liga-o à volta do drawArrays) replicando o
  app/src/main/cpp/ui/Fire.h LINHA A LINHA. **0.12-D (D-2)**: app/src/main/cpp/ui/Glass.h
  (as fontes puras — o BLUR DUAL KAWASE, a downsample 2×2, a DECISÃO da
  CACHE glassCacheDirty e o hash do ruído) + o captureGlass no Renderer
  (o glReadPixels com o flip, a redução 1/4, o blur, o upload; a cache
  com o contador glassCapturesForTest) + o ramo uSdfTexOn do shader
  único (o aShape.z — o widget de vidro amostra a cena borrada × a tinta
  glassInk do Theme).
- **app/src/main/cpp/assets/** — importadores e formatos GL-free:
  GOwnFormats (.gmesh/.gtext/.gm), GmeshV3Stream (o streaming do
  conversor), GltfImporter/GltfAnim/GltfTextures/GltfInstantiate,
  ObjImporter, PngLoader, TexturePipeline/TexturePolicy/TextureCache,
  MipGen, ThumbRender/ThumbIndex (thumbs software), ImpostorIndex (o
  PNG do impostor sob demanda — o pico de RAM é UM bloco, R-041),
  AstcCompressor/Etc2Compressor + vendor/.
- **app/src/main/cpp/ui/** — a apresentação: EditorUi, EditorLayout.h (as
  constantes — InspRow, as faixas k*Base; **0.12-C C-1**: hierHeaderRects
  — a FONTE ÚNICA da geometria do cabeçalho da hierarquia: o título com
  budget que ELIPSA + o ⋮ e o + à direita, o draw e os pins partilham;
  **0.12-C C-2**: menuSheetLayout — a FONTE ÚNICA do sheet do menu
  principal: a altura máxima + o maxOffset do scroll + a âncora 8dp com
  clamp; **0.12-C C-4**: browserSheetLayout — a FONTE ÚNICA da folha do
  NAVEGADOR (a linha 48dp, a ordem breadcrumb→pesquisa→raízes→Subir
  FIXO→lista, o card 92%/900dp dentro da safe-area) + hierEmptyInvite —
  a LADDER do convite do vazio da hierarquia),
  UiContext (com as primitivas
  do cristal da 0.11-Y: panelRoundedGradientV / glassTopLine /
  focusGlow + o QuadBatch::quadGradientV; **0.12-D D-1**: o
  app/src/main/cpp/ui/Sdf.h — a FONTE ÚNICA da matemática SDF (o SDF analítico, a normal
  do gradiente, o shade() — bisel/especular/brilho/sombra — e o TOGGLE
  sdf::g_level «Efeitos Alto/Médio/Baixo»; o rasterizador honesto do
  stub inclui-o). **0.12-D D-3**: o app/src/main/cpp/ui/Fire.h — a FONTE
  ÚNICA do FOGO (o fbm de 2 oitavas animado no tempo, a máscara da banda
  da aresta — 1 na aresta, morre a bevelW dentro e 1px fora —, as brasas
  paramétricas ember; a distância vem do roundRectSdf da D-1) + os
  emissores no UiContext (sdfFireBorder/fireGlowBelow/drawEmbers COM O
  PORTÃO celebration — o editor nunca vê brasas; o Baixo mata os três) +
  a RAMPA fire0..fire3 no Theme (a proveniência: scripts/d3_palette.py) +
  o relógio sdf::g_time no main loop; **0.12-D D-4**: o
  app/src/main/cpp/ui/Brand.h/.cpp — O «G EM CHAMA» (a identidade única:
  as setas do glifo antigo FORA, a rampa por traço pela altura, o ignite
  0..1 para a splash da D-6; o espelho Java UiIcons.drawBrand por
  segmentos + os tokens FIRE no ProjectManagerActivity; o slot do LOGO
  na Toolbar é a versão pequena),
  stub INCLUI este header) e o UiContext::sdfWidget (o emissor do
  widget — UM quad crescido para conter a sombra; o CHOKE POINT
  drawButtonVisual e o iconButton da barra emitem por AQUI — o premido é
  o FOGO accent→accent2 com bisel+brilho+sombra), UiEditor (as ops
  de árvore PURAS), Theme.h (as ÚNICAS cores — a tabela 0.11-Y superada
  pela ESCADA 0.12-C: fundo #0A0A0C · painel #131316 · cartão #1B1B20 ·
  elevado #24242A + a chama accent2/glow; os tokens card/elevated são o
  que os cards/sheets/toast consomem), ViewportChrome
  (o chrome do viewport — o TOOLTIP do gizmo 0.12-A: legendTimer/
  legendArm no EditorState; o draw ganhou `dt, in` trailing),
  SafeArea.h (os rects puros — `scissor3dRect()` NOVO na 0.12-A: o
  scissor do pass 3D no modo UI; **0.12-C C-1**: kHierPct=0.20 — a
  hierarquia 20% e o viewport ≥60% por defeito),
  ViewportChrome **0.12-C C-2**: o GIZMO 40dp do canto REMOVIDO (o
  duplicado do Mover — o kVpGizmoId fica APOSENTADO como o kVpAddTicId do
  D21; os rects do layout são ZERO e o pin mover_unico aferra),
  BottomPanel (com o emptyStateIcon(tab) da **0.12-C C-4** — a FONTE
  ÚNICA do ícone do estado vazio por tab: o vazio usa o glifo da
  PRÓPRIA função; o IMPORTAR do vazio dos Assets liga o
  Actions::importPressed e o main abre o navegador na Download),
  SettingsPage, ScriptEditor, DocsScreen, Gizmo, hierarquia.
  **0.12-B (a árvore de UI)**: app/src/main/cpp/ui/UiRuntime.h/.cpp tem
  a VISTA da árvore (UiTreeView + collectUiTree — o espelho UiCanvas
  alimenta o draw/hit/layout de 0.7.x; o joystick tem rect DERIVADO do
  TouchControls) + drawUiTree/hitTestUiTree/uiElementFromHit (o Play) +
  controlRect (os 9 cantos delegam no elementRect; o esticar = pai −
  margens) + findElementAnywhere procura TICs de UI por NOME;
  app/src/main/cpp/ui/UiEditor.h/.cpp tem a CRIAÇÃO da árvore
  (ensureUiRoot/uiAddTreeElement/uiAddTreeJoystick + uniqueNewTicName —
  a variante criação; o uniqueTicName do duplicate sempre sufixa), o
  MODO ÁRVORE do viewport (espelho + sincronização de volta no fim do
  drawUiViewport) e o Inspector de CONTROLO (0.12-B B-2: as 3 secções
  colapsáveis Transform/Aparência/Comportamento em
  uiInspectorPlanControl; os campos da base — margens/pivô/rotação/
  esticar/opacidade; os campos por tipo — texto/letra p/ Label e Button,
  raio/zona morta/sens p/ Joystick; os botões de RESET por campo, o
  EditorState::inspUiCollapsed guarda o colapso; SEM "remover
  joystick" — o apagar é do TIC).
  **0.12-B B-4 (as ações de cena)**: app/src/main/cpp/ui/EditorUi.h tem as FONTES PURAS
  (sceneRenamePath — o caminho do renomear com o filtro do addScene;
  sceneUniqueName — «main.001» Godot-style, a variante DUPLICAR;
  sceneDeleteAction — a decisão do 2-tap; sceneActionPick/kScenesAct*
  — os picks de ação) + o estado (EditorState::sceneDelArm — a linha
  com o apagar armado; textScene — o alvo do propósito 8); o
  drawScenesMenu desenha as 3 ZONAS de ação de 40 dp por linha
  (lápis/duplicar/lixeira) e despacha pelo tap re-despachado do scroll
  — o apagar só sai CONFIRMADO pelo 2.º toque.
- **app/src/main/cpp/components/** — os componentes do TIC: MeshRenderer
  (+ `impostorDistance`), CameraComp, Body3D, RigidBody3D, AudioPlayer,
  ScriptComp, AnimationPlayer, UI*, Input*. **0.12-B (a árvore de UI)**:
  app/src/main/cpp/components/Control.h (a BASE 2D dos elementos de UI —
  pos2D/tamanho/âncoras 9+esticar/margens/pivô/rotação/visível/opacidade;
  o payload por tipo é o UiElement por valor no campo `elem`; os 10 kinds
  incluem Joystick; o kind tem a FONTE no Control e o elem.kind fica
  espelhado) + app/src/main/cpp/components/UIRoot.h (o marcador da
  raiz). O app/src/main/cpp/components/UiCanvas.h é DEPRECATED — só lê
  (a compat: migrateUiCanvasToTree no SceneSerializer converte para a
  árvore no load).
- **app/src/main/cpp/platform/** — Android: main.cpp (a despachante —
  ANativeActivity, o ciclo por frame, drawTics), EngineLog, JNI glue,
  FileApi **0.12-C C-4**: o listDirEntries OCULTA as pastas ponto
  (as DIRETORIAS que começam por ponto saem; os ficheiros ponto ficam —
  a decisão 0.8.5 intacta).
  **0.12-B**: o "+" em modo UI cria pela ÁRVORE (uiAddTreeElement/
  uiAddTreeJoystick); o Play arma/aplica os botões da árvore
  (hitTestUiTree/uiElementFromHit) e desenha a árvore + TODOS os
  TouchControls. **0.12-B B-4 (as ações de cena)**: main.cpp tem
  renameSceneInProject (copia → escreve o novo → remove o velho → o
  manifesto aponta o novo caminho; a ativa continua ativa),
  duplicateSceneInProject (o MESMO conteúdo num nome único — contra o
  manifesto E os órfãos do disco; a cópia não fica ativa) e
  deleteSceneInProject (o ficheiro sai, a linha sai, o activeScene
  re-assenta; a ATIVA recusa — «ative outra cena para apagar esta»);
  o RENOMEAR usa o teclado in-app no PROPÓSITO 8 (o buffer
  pré-carregado com o nome atual; o alvo em textScene).
- **tests/** — a suíte do core (C++17, GL-free): test_main.cpp (o runner,
  GONI_ONLY/GONI_SKIP), TestFramework.h (o TEST/EXPECT), test_sentinels.cpp
  (as sentinelas regress_*), test_ctx_pines.cpp (os pins dos índices da
  0.11-CTX), test_fase_y0_icone.cpp (os pins do ícone launcher da
  0.11-Y — adaptativo + legacy + o antigo removido + a ESCALA do G
  medida nos PNG reais via stb_image), test_fase_y1_tema.cpp (os pins
  do TEMA da 0.11-Y — o poço #08080A + a chama accent2/glow + o
  gradiente dos botões ativos + as primitivas do cristal),
  test_fase_y2_layout.cpp (os pins do LAYOUT da 0.11-Y — as regras
  CruzaTabBar/PainelFora do validador), test_fase_y3_cristal.cpp (os
  pins do CRISTAL da 0.11-Y — a linha do topo + os divisores com
  gradiente nos painéis reais), test_fase375b_impostors.cpp,
  test_012a_lodui.cpp (os pins da 0.12-A A-1: as rows LOD/impostor da
  UI — row_existe/aplica/persiste/compat/default_off),
  test_012a_contadores.cpp (os pins da 0.12-A A-2: os contadores pelo
  UV do atlas + o marcador de cena), test_012a_grelha_tooltip.cpp (os
  pins da 0.12-A A-3: o scissor3dRect + o ciclo do tooltip),
  test_012b_uitree.cpp (os pins da B-1: a árvore de UI),
  test_012b_inspui.cpp (os pins da B-2: o Inspector de UI),
  test_012b_canvas.cpp (os pins da B-3: a moldura + as guias + o
  toast), test_012b_cenas.cpp (os pins da B-4: cena_rename/
  cena_duplicate/cena_delete_confirm — o 2-tap, M-B4a — e
  cena_serializa — o manifesto round-trip no FakeStorage),
  test_012c_paineis.cpp (os pins da 0.12-C C-1: viewport_60_pct/
  painel_colapsado_default/hierarquia_cabecalho/divisor_drag),
  test_012c_chrome.cpp (os pins da 0.12-C C-2: menu_scroll/menu_ancorado/
  topbar_3_zonas/mover_unico),
  test_012c_idioma.cpp (os pins da 0.12-C C-3: escala_tipografica — o
  scan dos fontes com o strip do gate/idioma_consistente/maiusculas_
  uniformes/superficies_4_tons),
  test_012d_sdf.cpp (os pins da 0.12-D D-1: sdf_widget_cantos — a
  diagonal do canto BR É a função shade() por píxel; sdf_bisel;
  sdf_sombra; sdf_toggle_baixo — o mesmo pixel entre Alto e Baixo;
  sondas pelo CANAL R contra o bg — o α do fb é inútil contra o clear),
  test_012d_fogo.cpp (os pins da 0.12-D D-3: fogo_rampa — os 4 tokens da
  ordem do dono; fogo_borda_anima — a banda muda com o tempo + a
  ARITMÉTICA ADITIVA por píxel + o centro morto exato; fogo_glow_below;
  brasas_portao — o PORTÃO + o determinismo; fogo_toggle_baixo — o Baixo
  mata os três emissores),
  test_012d_logo.cpp (os pins da 0.12-D D-4: logo_rampa_viva — a rampa
  por quad pela altura; logo_ignite_porta — a revelação da base para o
  topo + o determinismo; logo_setas_mortas — 16 segmentos nos LODs +
  nenhum vértice além do G+capa; logo_lod_traco — o ×1.2 no pequeno),
  test_012c_picker.cpp (os pins da 0.12-C C-4: picker_compacto/
  picker_breadcrumb/picker_oculta_dot/empty_inspector/empty_assets/
  empty_hierarquia/icones_unicos — o 8.º, empty_projetos, é o gate
  projects_ui_check.py),
  test_voni.cpp, test_bench.cpp,
  test_gmeshv3.cpp, voni_refgen.cpp (o gerador da referência), c33_virtual.cpp
  (o dispositivo virtual), bench_ram_curve.cpp (A-4: a curva 50k…12,3M
  no hospedeiro — alvo opcional `bench_ram`, option GONI_BENCH_RAM OFF),
  CMakeLists.txt (a lista de fontes dos pins),
  stub/glstub_fb.h (o rasterizador honesto — SEM GLES real), fixtures/.
- **app/src/main/java/vv/goni/** — a casca Java: VvActivity (JNI),
  ReloadGate (R-005), ProjectsFormat/VvProjects/ProjectManagerActivity,
  UiIcons.
- **ci/** + **scripts/** — os gates: scope_check.py (ci/scope.txt),
  docs_lint_check.py (ci/forbidden_docs_patterns.txt), docs_lint_state.sh
  (a regra STATE.md do §1.2 da 0.11-CTX), release_identity_check.py,
  y0_gen_icons.py (o gerador dos PNGs do launcher da 0.11-Y a partir de
  docs/iconegonevv.png — a escala do Y-0-fix: o G SÓLIDO a 65 % da
  zona segura),
  gmesh_docs_check.py, ui_vocab_check.py, theme_hex_check.py,
  hierarchy_check.py, jni_parity.py, link_parity.sh, check_main.sh,
  glyph_source_check.py, reload_concurrency_check.py, projects_ui_check.py,
  gizmo_camera_medidas.py.
- **docs/** — os y-*.png (os 13 ecrãs da varredura da 0.11-Y, 1600×720),
  REGRESSOES.md (as sentinelas R-NNN), GMESH_formato.md e
  GTEX_formato.md (a fonte de verdade dos formatos), LAYOUT_HIERARCHY.md,
  FASE3.5-PIN.md, RELATORIO-0.12-A.md (a campanha 0.12-A),
  RELATORIO-0.12-A-INVEST-RAM.md (a investigação RAM vs GPU do A-4),
  RELATORIO-0.11-W-*.md (a 0.11-W), SAF_EXCEPTION.md,
  AUDIT_SAF_JNI.md, HANDSHAKE_AUDIT.md. (O arquivo das campanhas fechadas,
  o DIARIO e o TEMPLATE_PROMPT organizam-se na CTX-3 — entram aqui quando
  existirem.)

## §3 Pontos de extensão (receitas)

### Adicionar um COMANDO V.ONI novo
1. app/src/main/cpp/voni/VoniRegistry.cpp — entrada em `kEntries`
   (o registo é a FONTE ÚNICA: docs, sintaxe e validação nascem daí).
2. app/src/main/cpp/voni/VoniVm.cpp — handler em `kCommands`.
3. app/src/main/cpp/voni/VoniEngineHost.h/.cpp — se o comando precisa do
   motor (os hosts que não têm motor devolvem «indisponível» honesto).
4. Regenerar a referência: construir e correr o alvo `voni_refgen`
   (tests/CMakeLists.txt) — VONI_referencia.md + llms*.txt regenerados; a
   bijeção é vigiada pela sentinela R-013.
5. Pin em tests/test_voni.cpp (ou test_voni_* novo na suíte).
6. docs/REGRESSOES.md — se for estrutural, sentinela nova com a mutação
   colada no relatório.

### Adicionar um COMPONENTE de tyker
1. app/src/main/cpp/voni/VoniRegistry.cpp — a entrada do componente nos
   metadados do registo central (o kComponents e o argc/min-max vivem NO
   REGISTO — VoniRegistry.h —, não no Tykers; UMA fonte alimenta tudo).
2. app/src/main/cpp/voni/VoniTykers.cpp — o handler (o MOTOR executa; a
   tabela do registo aponta para ele).
3. Regenerar com `voni_refgen`.
4. Pin na suíte (o ciclo do linker, o find obrigatório, os args na
   ativação).
5. docs/REGRESSOES.md — sentinela se estrutural.

### Adicionar um ELEMENTO DE UI novo (0.12-B)
1. app/src/main/cpp/components/Control.h — o KIND novo no enum (o
   vocabulário em controlKindName/controlKindFromName; o .goni partilha).
2. app/src/main/cpp/components/UiCanvas.cpp — o tamanho/cor/texto
   default em defaultSize/defaultColor/elementDefaultText (a FONTE ÚNICA
   da criação).
3. app/src/main/cpp/ui/UiRuntime.cpp — o draw no drawElement (por kind)
   e o hit-test se for interativo; a vista da árvore (collectUiTree)
   deriva o que for preciso (o padrão do joystick).
4. app/src/main/cpp/ui/Icons.h + hierIconFor
   (app/src/main/cpp/ui/EditorUi.cpp) — o ícone por tipo.
5. O Inspector: uiInspectorPlanControl (app/src/main/cpp/ui/UiEditor.cpp)
   — as linhas do tipo novo nas secções certas.
6. Pins em tests/test_012b_uitree.cpp (o round-trip + o draw).

### Adicionar um CAMPO no .goni
1. app/src/main/cpp/components/<Component>.h — campo com default
   explícito (o default é o contrato de compat).
2. app/src/main/cpp/core/SceneSerializer.cpp — o `dump` escreve e o
   `loadText` lê; campo NOVO só viaja no ficheiro quando != default
   (os .goni antigos ficam byte a byte).
3. Pin de compat byte a byte: ficheiro antigo lê igual; round-trip não
   cria «edit» fantasma.

### Adicionar um CAMPO no .gmesh
1. app/src/main/cpp/assets/GOwnFormats.h — flag nova ou cauda opcional
   (o padrão GTB1/GTB2: cauda encadeada, CRC, ficheiro novo abre em build
   antiga SEM saber).
2. app/src/main/cpp/assets/GOwnFormats.cpp — `writeGMesh` +
   `readGMeshV3Meta` (e os leitores encadeados com out-param opcional —
   os chamadores antigos ficam byte a byte).
3. app/src/main/cpp/assets/GmeshV3Stream.cpp — se o streaming também
   transporta o campo.
4. app/src/main/cpp/render/BlockMesh.cpp — ler no `open`.
5. Pins: o .gmesh antigo SEM a flag lê igual; CRC podre = erro legível;
   docs/GMESH_formato.md atualizado (o gate gmesh-docs aferra).

### Adicionar um SISTEMA (por frame)
1. app/src/main/cpp/core/Tick.h — a ordem é o `TickGroup` (enum + 
   `TickGroups::add`); escolher o grupo certo (o UI desenha DEPOIS, não é
   um TickGroup).
2. app/src/main/cpp/platform/main.cpp — registar o sistema e chamá-lo na
   despachante (a ordem real por frame vive aqui — drawTics incluído).
3. Considerar a posição face a Transform/Animation/Physics (o frame é
   colheita → grupos → submissão).

### Adicionar um FICHEIRO .cpp NOVO ← CRÍTICA
1. Criar <nome>.h e <nome>.cpp na camada correta (app/src/main/cpp/<camada>/).
2. Adicionar <nome>.cpp em tests/CMakeLists.txt (para os pins).
3. Adicionar <nome>.cpp em app/src/main/cpp/CMakeLists.txt (para o APK —
   o sítio onde o bug «undefined symbol no CI» vive).
4. Verificar que AMBOS os CMakeLists têm o ficheiro:
   `git grep "<nome>.cpp" -- '*CMakeLists.txt'` → 2 resultados.
5. Se só der 1 resultado: falta um. Corrigir ANTES de commitar.

JUSTIFICAÇÃO (a ferida real): a 3.75b (commit 1524148) adicionou
app/src/main/cpp/assets/ImpostorIndex.cpp só ao tests/CMakeLists.txt. O
APK morreu no link (undefined symbol ensureImpostor/relFor) porque o
CMake do app não conhecia o ficheiro. O fix foi UMA linha (caaef02).
Esta receita previne o mesmo bug; o caso test_wiring092.cpp/lista dos
TUs de test NÃO entra no CMake do app (precedente: os test_*.cpp são
só da suíte — a regra dos DOIS CMakeLists é para TU do core).

### Adicionar uma SENTINELA
1. tests/test_sentinels.cpp — função `regress_<nome>` (o nome é o
   CONTRATO em docs/REGRESSOES.md).
2. docs/REGRESSOES.md — entrada R-NNN nova (categoria, origem, comando,
   declaração).
3. Mutação OBRIGATÓRIA colada no relatório (quebrar o fix → vermelho →
   repor → verde).

### Mexer no render de blocos
- app/src/main/cpp/render/BlockMesh.h — o contrato.
- app/src/main/cpp/render/BlockMesh.cpp `open` — abrir (header + tabela
  de blocos + materiais; ZERO bytes de dados).
- app/src/main/cpp/render/BlockMesh.cpp `materializeBlock` — carregar UM
  bloco (o pico de RAM é o span da faixa + o MeshData, R-041).
- app/src/main/cpp/render/BlockMesh.cpp `draw` — desenhar (culling + LOD
  + instancing por assinatura + ordem BLEND longe→perto).
- app/src/main/cpp/render/BlockMesh.cpp `ensureBlock`/evict — a cache LRU.

## §4 Padrões de leitura

- O header ANTES do .cpp (o contrato está no header).
- `git grep -n "símbolo"` e ler ±20 linhas — nunca ficheiros >1000 linhas
  por inteiro.
- Seguir o call chain: main → system → component → primitive.
- Procurar TABELAS (`kEntries`, `kCommands`, InspRow) — o registo central
  alimenta docs e validação.
- Ler os pins ANTES de tocar no código (o teste diz o contrato real).

## §5 Onde NÃO tocar sem ler primeiro

- app/src/main/cpp/platform/main.cpp — a despachante (a ordem por frame).
- app/src/main/cpp/core/SceneSerializer.cpp — a compat do .goni.
- app/src/main/cpp/assets/GOwnFormats.cpp — a compat do .gmesh.
- app/src/main/cpp/ui/EditorLayout.h — as constantes do editor (as leis
  de desenho/toque).
- app/src/main/cpp/voni/VoniRegistry.cpp — a fonte única da linguagem.
- tests/CMakeLists.txt — a lista de fontes dos pins.
- app/src/main/cpp/CMakeLists.txt — a lista de fontes do APK (a causa
  raiz do bug ImpostorIndex da 3.75b foi o desalinhamento entre ESTES
  DOIS ficheiros — ver a receita CRÍTICA do §3).

## §6 Atualização

Atualizar quando o código mudar. Se o CODEMAP não reflete o código, é
dívida — corrigir antes de fechar a próxima sub-fase (a receita que
falhar na prática corrige-se no MESMO commit que a usa).
