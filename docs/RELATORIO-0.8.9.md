# RELATÓRIO 0.8.9 — CRASH-PROOF: recursão + primitivas à prova de falha + fit uniforme + espaço sem tetos

## 1. Objetivo

Uma release, quatro fixes cirúrgicos, zero features: (1) corrigir o **crash
por recursão infinita** revelado pelo crash dump (com addr2line no CI a
nomear a função); (2) troca de primitivas **à prova de falha** (nunca
desseleciona, nunca crasha, mantém mesh anterior se algo falhar); (3)
normalização de import **uniforme** (preserva proporções, nunca espalma);
(4) editor e Play **sem tetos de espaço** (far dinâmico, zoom amplo, campos
sem limite, grelha adaptativa).

## 2. Estado inicial (HEAD de entrada)

HEAD = 0.8.7-b (3f0e838, versionCode 38, CI verde, APK assinado sha256
be327853…). Evidência do C33 sobre o build 0.8.7: o log provava
`mesh: troca prim esfera → prim box fim ok verts=24 idx=36` mas
`mesh: troca - → prim capsula ERRO(gerador/upload falhou)`; ao trocar para
cone/cápsula o TIC **desseleciona e continua cubo**; e o dump
`crash-1790830406.dump`: **SIGSEGV com stack exhaustion**
(`si_addr 0x723abb4fe0 ≈ sp 0x723abb4fb0`), frames #04–#63 todos no mesmo
pc `0xe2f24` (60 frames idênticos), entrada em `0xe2de8`, `#00 = 0xf2938`.
A interpretação do dono: "recursão direta sem caso de base ou com ciclo…
o 'desseleciona e fica cubo' = activity a reiniciar após este crash".

## 3. Diagnóstico (dump-driven — a função NOMEADA pelo addr2line)

### 3.1 A resolução no CI (a exigência do prompt, cumprida à letra)

O .so do APK assinado é STRIPPED (addr2line não resolve nada nele), MAS o
build é **reprodutível** — os dois runs 0.8.7 produziram .so byte-idênticos
(sha256 5e4e024f… conferido). O workflow novo
`.github/workflows/resolve-crash.yml` (branch `forensics-087`, run
37014729599, verde) refaz o build do commit exato **3f0e838** com o mesmo
NDK r26d, retém o .so NÃO-STRIPPADO dos intermediários do gradle, confere o
**BuildID contra o build assinado — CASADO byte-a-byte
(79363917aa0ce6580b9e0f93cf7493762cd27940)** e resolve:

```
addr2line -e libgoni_vv.so 0xe2de8 0xe2f24 0xf2938
0xe2de8 → vv::SceneSerializer::(anonymous namespace)::appendComponentJson(
           vv::Json&, vv::SkeletonComp const*)   [0xe0f9c+0x2a14] +0x1e4c
0xe2f24 → vv::SceneSerializer::(anonymous namespace)::appendComponentJson(
           vv::Json&, vv::SkeletonComp const*)   [0xe0f9c+0x2a14] +0x1f88
           (cadeia inline: std::__ndk1::vector<vv::Json>::__destroy_vector::
            operator() / ~vector — vector:445)
0xf2938 → stbtt_GetGlyphKernAdvance                 [0xf2828+0x3c4] +0x110
```

**A leitura**: os pcs da "recursão" vivem DENTRO do
`std::vector<vv::Json>::~vector` INLINED no serializador de esqueletos — **o
stack exauriu na CADEIA DE DESTRUTORES do Json** (a recursão que o dono
suspeitava, suspeito (a): "destrutor recursivo"). O `#00` (kerning) cai numa
função que **NADA chama no binário inteiro** (verificado por varredura de
bl/b em todo o .text + PLT) — frame de unwind a andar stack corrompida
(pcs parcialmente não-fiáveis depois da exaustão; `si_addr ≈ sp` é a
verdade do kernel). O "desseleciona e continua cubo" = a activity a
reiniciar e a recarregar a cena gravada; e o `ERRO(gerador/upload falhou)`
com origem `-` era **seleção perdida** (applyAssetPick sem alvo — o MR é
null), não gerador: o gerador estava são (CI 0.8.7 provava geometria por
primitiva).

### 3.2 Onde a recursão podia nascer (três portas, TODAS fechadas)

1. **`placeAt` do resolver de UI SEM guard** (a porta PROVADA): o `sizeOf`
   sempre teve guard de ciclos (state 1=em curso), mas o `placeAt`
   recursivo NÃO tinha NENHUM — e o seletor "colocar em:" **INCLUÍA o
   próprio elemento** (comentário antigo: "inclui o próprio — o resolver
   guarda ciclos" — só o sizeOf guardava!). Um container pai de si próprio
   (ou A→B→A) recursava infinitamente até exaurir a stack. **RED→GREEN
   provado no CI: com os guards desligados, o teste do self-ciclo
   SEGFAULTA (exit 139) — a assinatura EXATA do dump do C33.**
2. **Destrutor do Json recursivo** (a porta NOMEADA pelo addr2line): o
   `~Json` implícito recursa por nível (`~vector<Json>` → `~Json` por
   elemento → …). Uma árvore profunda/corrompida exaure a stack na cadeia
   de dtor — exatamente onde o addr2line pôs os pcs. **RED→GREEN provado:
   com o dtor implícito, uma árvore de 500 000 níveis SEGFAULTA; com o
   ITERATIVO destrói plana.**
3. **`Json::dumpTo` sem teto** (a porta latente): o parser tem cap 64 desde
   a F3; o dump recursivo NÃO tinha teto nenhum.

## 4. Implementação (por ficheiro)

- **core/Json.h** — destrutor **ITERATIVO** (cova `std::deque<Json>` de
  endereços estáveis + fila de pendentes; folhas pagam ZERO — o serializer
  destrói milhares de folhas por save); defaults EXPLÍCITOS de
  move/copy (dtor declarado SUPRIME o move implícito — sem isto o
  `std::move` vira COPY e a destruição iterativa quebra); `dumpTo` com teto
  `kMaxDumpDepth=64` (corte visível `<!max-depth>` + erro no log).
- **ui/UiRuntime.cpp** — `placeAt` com **guard de ciclo** (`placing[i]`
  marca o caminho atual; repetição = ciclo → `elog::error` LEGÍVEL com o
  nome do elemento e do parent, elemento fica órfão) + **guard de
  profundidade** (`kLayoutMaxDepth=64`); `sizeOf` com depth explícito;
  contagem `cyclesAborted` no resolver.
- **ui/UiEditor.cpp** — a lista de containers do "colocar em:"
  **EXCLUI o próprio elemento** (a fonte do ciclo deixa de ser criável);
  teclado em MODO NUMÉRICO (propósito 6: tecla de caso vira "-", traço
  vira "."); `commitTextInput` caso 6 (campo numérico sem teto:
  pos/rot-graus/escala por índice, strtof com consumo total, não-finito
  rejeitado, escala com piso 0.001).
- **core/SceneSerializer.cpp** — fillUiCanvas **CURA self-parent ao
  carregar** (parent == nome próprio → removido + `elog::error` legível;
  ciclos mútuos ficam para o guard do placeAt — sem crash).
- **render/Mesh.h/.cpp** — AABB da geometria calculado no `create`
  (`boundsMin/Max/Extent/MaxExtent` — DADOS, host-testável);
  `Mesh::create` **REJEITA vértices não-finitos** (o upload nunca mais sobe
  NaN); `destroy` zera o AABB.
- **platform/main.cpp** — primMesh **valida antes do upload** (ok(),
  coords finitas, AABB não degenerado — ERRO com a razão exata por caso);
  ERRO da troca HONESTO (`mrOld ? "gerador/upload falhou — causa acima" :
  "sem TIC com mesh selecionado"`); **clipes DINÂMICOS por frame** (editor:
  `editorClips(dist, sceneFarthest)`; Play: `playFar/playNear` com cópia do
  CameraComp — o slider do dono é PISO); `out.log` dos dispatches de mesh
  vai ao **elog** (a prova no log viewer do device, não só logcat);
  resolver `meshExtent` (AABB como DADOS — o applyAssetPick continua PURO,
  nunca desreferencia o Mesh).
- **ui/EditorUi.h/.cpp** — `kImportTargetSize=2` +
  `AssetResolvers::meshExtent` (extensão do AABB como dados);
  `applyAssetPick` menuKind 1: **fator ÚNICO s=2/maiorEixo nos 3 eixos** na
  1ª aplicação do ref (re-aplicar o MESMO não mexe na escala afinada;
  Transform3D criado se faltar), log `import: dims=… uniform scale=…`;
  Inspector: linhas **"dims: X×Y×Z"** + botão **"escala original"**
  (repõe {1,1,1}); sliders px..sz com **valor tocável** (sublinhado a
  ACCENT → teclado numérico; zona [x+206..fim], o trilho continua do
  slider).
- **ui/EditorLayout.h** — perfil `fileMesh`; rows `DimsLabel`/`ScaleOrig`;
  `kInspectorScaleOrig=5310`; plano/contagem atualizados (pior caso 39 ≤
  48 do buffer).
- **render/Camera.h/.cpp** — zoom **0.01 → 100 000** (era 1..300);
  `nearZ/farZ` MEMBROS dinâmicos com `setClips` (defaults F3.1 mantidos —
  quem não chama vê o comportamento de sempre).
- **core/SceneBounds.h (NOVO)** — `sceneAABB`/`sceneFarthest`/
  `aabbFarthestDist`/`editorClips`/`playFar`/`playNear` (fonte única do far
  dinâmico; o far contém a cena pela DISTÂNCIA ao ponto mais longe, não só
  pelo "tamanho").
- **render/Grid.h/.cpp** — `gridStepForDist` (passo em potências de 10 com
  clamps 0.1..1000; fonte única pura); draw usa o passo adaptativo + extent
  dinâmico (2.5× o foco, piso 3000) — e o **BUG LATENTE F3.1 corrigido: o
  uniform `uExtent` NUNCA era enviado** (o quad estava degenerado — o grid
  invisível desde a 0.3.1; agora vai ao shader).
- **tests/test_wiring089.cpp (NOVO)** + secção 0.8.9 no test_wiring087.cpp
  (o TU do main.cpp) — ver §6.
- **.github/workflows/resolve-crash.yml (NOVO)** — o addr2line contra o
  build exato (BuildID casado); **release.yml** — artifact
  `goni-vv-0.8.9-release-signed`; **build.gradle** — versionCode 39 /
  0.8.9; gates de símbolos do gerador MANTIDOS.

## 5. Decisões técnicas relevantes

1. **Iterativo > guard no destrutor**: o prompt dava as duas opções para a
   recursão; escolhemos a REESCRITA ITERATIVA no ~Json (a mais forte — nem
   precisa de teto) e GUARD+ERRO LEGÍVEL no placeAt (o layout precisa de
   saber qual elemento ciclou para o dono corrigir o "colocar em").
2. **O applyAssetPick continua PURO**: a extensão do AABB chega pelo
   resolver `meshExtent` COMO DADOS — os stubs-ponteiro dos testes
   antigos nunca são desreferenciados (contrato 0.8.7 preservado; a
   primeira versão do fix desreferenciava e SEGFAULTAVA a suíte — apanhado
   antes do push).
3. **O far pela DISTÂNCIA, não pelo tamanho**: um TIC a py=10 000 numa cena
   "pequena" continua LONGE — `sceneFarthest` (origem→canto mais longe) +
   dist alimenta o far do editor; no Play, `aabbFarthestDist(olho, AABB)`.
   O slider far do dono é PISO (nunca teto) — o invariante 0.7.10
   respeitado.
4. **A escala do fit vive no Transform3D** (a geometria fica intacta):
   "escala original" = repõe {1,1,1} sem tocar no mesh; o .goni round-tripa
   a escala aplicada; re-aplicar o MESMO ref não re-normaliza (a escala
   afinada pelo dono é sagrada).
5. **Isolamento de log nos testes**: o engine.log PERSISTE entre execuções
   — sem `rmrf` por caso, uma linha da run VERDE passava o teste com o fix
   REVERTIDO (falso-positivo apanhado na prova RED→GREEN desta release;
   todos os casos que aferem log agora limpam o diretório antes).
6. **Prova RED→GREEN dupla**: guards do placeAt desligados → SIGSEVA no
   teste do self-ciclo; dtor Json implícito → SIGSEGV no teste de 500 000
   níveis. As duas assinaturas do crash do C33 reproduzidas E mortas.

## 6. Testes novos (CI Linux — 545 → 568, +23)

- `wiring089_layout_selfciclo_erro_legivel_sem_crash` /
  `…_ciclo_mutuo_ab_sem_crash` / `…_cadeia_profunda_legitima_e_guard`
  (40 níveis ok; 80 aborta com "profundidade"; guardas de tempo);
- `wiring089_json_dump_teto_de_profundidade` (200 níveis corta com
  marcador) / `…_json_parse_cap_mantido_regressao` /
  `wiring089_json_destrutor_iterativo_profundeza_extrema` (500 000 níveis
  destrói plano + semânticas move/copy intatas);
- `wiring089_grelha_adaptativa_passos_por_zoom` (0.1/1/10/100/1000 +
  clamps + defesa);
- `wiring089_scene_aabb_vazia_e_populada` (py=10 000 contido; far ≥
  mais-longe+dist+10; rácio < 400) / `…_play_far_respeita_slider_como_piso`;
- `wiring089_import_normalizacao_uniforme_preserva_proporcoes` (s=0.002
  único, ratios iguais antes/depois, log dims/scale, re-aplicar intocado,
  Transform3D criado se faltar);
- `wiring089_campo_numerico_sem_teto_py_10000` (10000/-12.5/90°/piso
  0.001/inválidos rejeitados);
- `wiring089_geradores_matrix_default_e_extremos` (8×{default, seg=3,
  seg=256, raio=0.001, raio=1000}: ok, finito, AABB não degenerado) /
  `…_mesh_create_rejeita_nan_e_calcula_aabb`;
- `wiring089_load_cura_selfparent_com_erro_legivel` (o .goni cíclico
  cura ao carregar) / `…_parent_de_tic_ciclico_nao_recursa` (round-trip) /
  `…_joints_de_skin_ciclicos_iterativos_sem_hang` (a composição de joints é
  iterativa — nunca recursou; aferido com guarda de tempo);
- `wiring089_anim_e_serializer_regressao` (o dumpTo com teto não toca
  árvores legítimas);
- no TU do device (test_wiring087.cpp, secção 0.8.9):
  `…_device_sequencia_do_dump_termina_sem_crash` (esfera→box→cápsula com
  origem "-" + seleção perdida falha GRÁCEL sem tocar no estado),
  `…_device_falha_gl_mantem_mesh_anterior_e_selecao` (upload falha → mesh
  anterior + seleção + razão no log; contexto novo recupera),
  `…_device_oito_prims_tres_ordens_todas_renderizam` (8×3 ordens, draw
  calls no stub, cull afirmado, cache ≤ 48),
  `…_device_import_gigante_uniforme_sem_espalmar` (e2e browser→"Sim":
  dims/scale no ENGINE.LOG, proporções, re-aplicar),
  `…_device_zoom_far_dinamico_e_grelha_adaptativa` (BOOT INIT_WINDOW,
  zoom 0.01/6/5000/100000 com frames < 500ms, far > cena, passo da grelha
  ao shader).

## 7. Checklist device (resumo — versão completa no README)

Ciclar as 8 primitivas em ordens variadas (nenhuma desseleciona, zero
crash, cone e cápsula visíveis) · modelo gigante entra utilizável e NÃO
espalmado; "escala original" repõe · zoom até ver o modelo inteiro; TIC em
py=10 000 via campo numérico e visível lá · **zero crash dumps novos** ·
CI verde, APK assinado versionCode 39, gates intactos.

## 8. Riscos

- O dtor iterativo aloca cova/fila por Json NÃO-folha destruído (2 small
  allocs); o save/load destrói milhares de temporários → custo mensurável
  apenas na gravação/carregamento (não por frame); folhas (a maioria)
  pagam ZERO.
- O addr2line pôs os pcs no serializador de SKELETON com dtor inlined — a
  porta LOGICA provada foi o placeAt (SEM guard) + o dtor recursivo; as
  DUAS estão fechadas, mas o dump original não permite provar qual das
  duas acendeu primeiro (stack corrompida = unwind parcialmente não-fiável;
  documentado honestamente aqui e no §3).
- Grelha com passo adaptativo muda o "look" no zoom de trabalho de 0.5
  para 1 (potências de 10) — decisão da spec ("…0.1/1/10/100/1000…").
- uExtent agora é ENVIADO (o grid FICA VISÍVEL — era invisível desde a
  0.3.1): quem conhecia o editor "sem grelha" vê-na pela primeira vez.

## 9. Dívida (documentada)

- Origin rebasing (jitter de float32 > ~100 000 unidades é limite conhecido)
  = FUTURO, NÃO implementado (nota do prompt cumprida);
- ciclos MÚTUOS de parents no .goni não são curados ao carregar (só o
  self-parent — custo/benefício; o guard do placeAt aborta-nos sem crash
  com erro legível);
- o teclado numérico reutiliza a grelha QWERTY (dígitos na linha de baixo,
  "-" na tecla de caso, "." na do traço) — um pad numérico dedicado é
  futura polimento;
- ~Json da STL antiga em .so de TERCEIROS nada tem a ver (não há) — só a
  nossa árvore é defendida.

## 10. CLÁUSULA CALMA (cumprida)

Só os 4 fixes + testes: crash (dtor iterativo + guards + cura + seletor),
primitivas à prova de falha (validação + ERRO honesto), fit uniforme
(fator único + dims/escala original + log), espaço sem tetos (zoom/far/
campos/grelha + bug latente do uExtent). Zero V.ONI, zero layout novo
(dims/escala-original são LINHAS do Inspector existente), zero física nova.

## 11. Fecho do CI

(commits 0.8.9-a/0.8.9-b — preencher com os IDs/sha256 do run no fecho)
core-tests 568 OK · check_main/link_parity (85 TUs)/jni_parity OK ·
build-release assinado (versionCode 39) · verify-entry-symbols COM OS GATES
(makePrimMesh/primDefaults/primName/primClamp/applyAssetPick) ·
resolve-crash-087 verde (BuildID casado, funções nomeadas no §3).
