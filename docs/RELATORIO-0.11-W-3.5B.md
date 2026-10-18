# 0.11-W 3.5b — GPU INSTANCING (o mesmo mesh × N TICs → 1 draw)

> versionCode 74 · 0.9.6 · a 1.ª sub-fase da campanha 0.11-W (render
> systems; a abertura: docs/RELATORIO-0.11-W-ABERTURA.md — o baseline do
> dono: 5 FPS · bl 5164/5178 · 6 TICs) · 9 pins novos
> (tests/test_fase35b_instancing.cpp) + o pin da investigação do culling
> (§1.8) · sentinela R-047 (regress_instancing) + as 3 mutações coladas ·
> c33 FASE 28 (+22 checks, 869→891) · test_core 0 falhas · gates locais
> verdes.

## §1 O que foi feito

### O caminho instanciado no GPU (o shader + o renderer)

- **`render/Material.h/.cpp`** — o VS do lit ganha `uniform int uInstOn` +
  o bloco std140 `Instances { mat4 uInst[64]; }` (binding 1 — o Bones é o
  0): `m = uInst[gl_InstanceID] * uModel` quando o instancing liga
  (§3.5b.3 — o model do TIC dono × a transformação do bloco no .gmesh,
  identidade na geometria baked do import), com `uInstOn = 0` NUNCA a ler
  o bloco (o ramo morto — o caminho pré-3.5b é byte a byte). `setInstances`
  faz o upload SEMPRE de 4 KB determinísticos (n matrizes + cauda de
  identidade até 64 — o MESMO contrato do setBones) e `setInstancing` liga
  o ramo A CADA draw (o contrato das uniforms da casa — o estado nunca
  vinga). `kMaxInstances = 64` (o teto declarado do GE8320: 64 × 64 B =
  4 KB de bloco std140).
- **`render/Mesh.h/.cpp`** — `drawInstanced(instanceCount)` =
  `glDrawElementsInstanced` (GLES 3.0 core — o GE8320 suporta).
- **`render/Renderer.h/.cpp`** — `drawMeshInstanced` (o irmão do
  drawMesh): uModel IDENTIDADE (a instância leva TUDO), as mesmas
  uniforms/estado GL com o MESMO par de enable/restore, devolve
  `{indexCount × instCount, 1, instCount}` — o 3.º campo é o
  `instancesMerged` do §3.5b.5. O drawMesh de sempre define
  `setInstancing(false)` explicitamente (nunca estado a vingar).

### O BlockMesh: uma classificação, dois caminhos

- **`render/BlockMesh.h/.cpp`** — o loop de cull/LOD/partição do draw foi
  EXTRAÍDO para `classifyFrame` (privada): o draw de sempre e a colheita
  do instancing usam a MESMA função (zero drift). O draw mantém o MESMO
  comportamento (o `lodDrawn` passa a contar no passe com o ensure OK — o
  resultado é o mesmo do `++/--` do pré-3.5b; os pins existentes afervam).
  Métodos novos:
  - `collectVisible(model, vp, camPos, lodDistance, out)` — os
    `InstPick{block, entry, blend, zNdc}` VISÍVEIS deste TIC (o MESMO
    cull/LOD); NADA desenhado nem carregado (o §3.5b.2: o agrupamento
    corre depois do culling e antes do upload);
  - `drawInstancedGroup(r, entryIdx, mats, count, vp, tex, tint, st)` — o
    grupo inteiro numa entrada (o mesmo mesh GPU!): o lazy corre AQUI (a
    submissão), o mesh carrega UMA vez e serve todas as instâncias; o
    teto de 64 fatia em múltiplos draws (nunca falha);
  - `drawBlendPicks(...)` — os blends NÃO se fundem (a ordem longe→perto
    é por fragmento e um draw por bloco é a que a ordena): 1 draw por
    bloco, o MESMO blendDrawOrder, DEPOIS de todos os opacos do frame
    (mais correto para o depth; entre TICs não-instanciados nada muda);
  - `storageStats(st)` — a contabilidade resident/ram/vram 1× por mesh
    (somar por TIC duplicava na cena dos 6 clones).
- **`render/DrawStats.h`** — `instancesMerged` NOVO (a fonte única,
  §3.5b.5) + o operator+ transporta.

### O orquestrador (o drawTics do device)

- **`platform/main.cpp`** — o drawTics vira COLHEITA → AGRUPAMENTO →
  SUBMISSÃO: cada TIC com `mr.blocks` + `mr.instancing` + mesh não-skinado
  colhe os seus picks (o `skinnedCulledNow` corre ANTES, igual); os opacos
  vão para o batch e fundem por assinatura `(bm, entry, tex, tint)`
  (stable_sort — o mesh+bloco+material+textura do §3.5b.2, o tint incluído
  porque fundir TICs com cores diferentes quebrava a identidade visual);
  os blends acumulam por TIC e desenham no fim. `BlockFrameStats` ganha
  `instancesMerged` e o log throttled diz «instanciados=N» (§3.5b.5). O
  skinado NUNCA entra na colheita (a pose é por TIC — o §3.5b.2 manda não
  misturar).

### O toggle e a UI

- **`components/MeshRenderer.h`** — `bool instancing = true` (o default
  ON — visualmente idêntico ao contrário do LOD que muda a aparência;
  §3.5b.4).
- **`ui/EditorLayout.h` + `ui/EditorUi.cpp`** — a linha «instancing:
  sim/não» no inspector (o MESMO padrão do toggle posicional do áudio; o
  id `kInspectorInstancing` 5312; a exclusão do drag da lista).
- **`core/SceneSerializer.cpp`** — «instancing»:false SÓ quando false (o
  default omitido — os .goni da 0.11-T ficam byte a byte; ausente = ON).
- **`core/UndoStack.h/.cpp`** — o TicSnap transporta (snap/restore/paste;
  o MESMO contrato do lodDistance).

### O stub GL do hospedeiro (os pins provam de verdade)

- **`tests/stub/GLES3/gl3.h`** — `glDrawElementsInstanced` com contadores
  (`drawElementsInstancedCalls`/`lastInstanceCount`), o glGetUniformBlockIndex
  reconhece «Instances» → índice 1, os contadores do binding 1, e o
  glBufferSubData distingue os uploads pelo tamanho (8192 = Bones, 4096 =
  Instances — nenhum pin antigo muda de valor).
- **`tests/stub/glstub_fb.h`** — o rasterizador REAL do virtual reproduz o
  instancing: o UBO do slot 1 guarda os bytes, o drawCore itera as
  instâncias com `mEff = uInst[i] × uModel` e `mm = uVP × mEff` (o MESMO
  produto do VS — o pin visual depende disto) + o reset limpa os slots.

## §2 Provas

- **Pins novos em tests/test_fase35b_instancing.cpp: 9 casos**
  1. `fase35b_6_tics_1_dc` — 6 TICs com o mesmo mesh+material → 1 draw
     instanciado (o glstub conta 1 drawElementsInstanced com
     lastInstanceCount=6; dc=1; instancesMerged=6; verts = índices × 6).
  2. `fase35b_2_materiais_2_dc` — 6 TICs × 2 materiais → 2 draws (12
     fundidos, 12 desenhados).
  3. `fase35b_toggle_off` — com o OFF os 6 desenham individualmente (6
     drawElements, ZERO instanciados — o caminho de sempre intacto).
  4. `fase35b_skin_flag_mismatch` — o skinado diz-o pela flag (o guard do
     drawTics lê ISTO); os 5 estáticos fundem em 1 e o skinado desenha
     pelo caminho de sempre (2+ draws, nunca 1).
  5. `fase35b_lod_mismatch` — 3 perto (entry normal) + 3 longe (entry
     LOD) → 2 draws com `st.lodDrawn == 3` (o MESMO critério do
     lodActive).
  6. `fase35b_mais_de_64` — 100 TICs → 2 draws (64+36, o
     lastInstanceCount=36 da cauda), instancesMerged=100, drawn=100.
  7. `fase35b_visual_identico` — PIXEL A PIXEL no rasterizador REAL do
     stub: 6 draws individuais vs 1 draw instanciado = o MESMO framebuffer
     byte a byte (o §3.5b.4: visualmente idêntico).
  8. `fase35b_compat_011t` — o TIC novo nasce ON; o save sem edição não
     contém a chave; com false a chave nasce; o load restaura; o
     round-trip da forma 0.11-T é byte a byte.
  9. `fase35b_stats_corretos` — instancesMerged bate certo (6 → 6; 100 →
     100 em 2 draws) e os blends NÃO fundem NEM contam (1 draw por bloco).
  + `fase35b_culling_investigacao` — a investigação do §4 (o dump colado
  lá).
- **Sentinela R-047** (`regress_instancing` em tests/test_sentinels.cpp)
  — 6 TICs → 1 dc × 6 (o dc/instancesMerged/verts exatos), o toggle no
  .goni (ausente = ON, false restaura), o skin NUNCA funde (a flag que o
  guard lê), o LOD é da assinatura (3+3 → normal=3/lod=3), o teto de 64
  fatia 100 → 2 draws com a cauda 36.
- **Mutações 3 vermelho→verde (coladas)**

M-3.5b1 — o caminho instanciado removido (o drawInstancedGroup cai no
drawMesh individual — o fallback do prompt):

```
$ GONI_ONLY=fase35b_6_tics_1_dc ./test_core
== testes do core — G.One VV 0.7.7 ==
fase35b_6_tics_1_dc                            ...  FALHOU  tests/test_fase35b_instancing.cpp:240  run.instDraws == 1
  FALHOU  tests/test_fase35b_instancing.cpp:241  glstub::stats.lastInstanceCount == 6
  FALHOU  tests/test_fase35b_instancing.cpp:242  run.total.drawCalls == 1
  FALHOU  tests/test_fase35b_instancing.cpp:243  run.total.instancesMerged == 6
fase35b_6_tics_1_dc                            FALHOU

4 teste(s) com falha
```

Reposto (cp do backup):

```
$ GONI_ONLY=fase35b_6_tics_1_dc ./test_core
== testes do core — G.One VV 0.7.7 ==
fase35b_6_tics_1_dc                            ...fase35b_6_tics_1_dc                            OK

0 teste(s) com falha
```

M-3.5b2 — a skin flag ignorada no agrupamento (o `skinned()` do
BlockMesh — a fonte do guard do drawTics — devolve sempre false):

```
$ GONI_ONLY=fase35b_skin_flag_mismatch ./test_core
== testes do core — G.One VV 0.7.7 ==
fase35b_skin_flag_mismatch                     ...  FALHOU  tests/test_fase35b_instancing.cpp:372  bmSkin.skinned()
fase35b_skin_flag_mismatch                     FALHOU

1 teste(s) com falha
```

Reposto (cp do backup):

```
$ GONI_ONLY=fase35b_skin_flag_mismatch ./test_core
== testes do core — G.One VV 0.7.7 ==
fase35b_skin_flag_mismatch                     ...fase35b_skin_flag_mismatch                     OK

0 teste(s) com falha
```

M-3.5b3 — o teto de 64 removido (o fatiamento tenta fundir as 100 num
draw):

```
$ GONI_ONLY=fase35b_mais_de_64 ./test_core
== testes do core — G.One VV 0.7.7 ==
fase35b_mais_de_64                             ...  FALHOU  tests/test_fase35b_instancing.cpp:499  run.instDraws == 2
  FALHOU  tests/test_fase35b_instancing.cpp:500  run.total.drawCalls == 2
fase35b_mais_de_64                             FALHOU

1 teste(s) com falha
```

Reposto (cp do backup):

```
$ GONI_ONLY=fase35b_mais_de_64 ./test_core
== testes do core — G.One VV 0.7.7 ==
fase35b_mais_de_64                             ...fase35b_mais_de_64                             OK

0 teste(s) com falha
```

- **test_core: 0 falhas** (a suíte inteira com os 10 pins + a R-047
  dentro; a `sentinela_r039` flutua POR DISCO na sandbox — o flake
  documentado da 3.5a, VERDE com folga de disco; o CI é o árbitro).
- **c33_virtual: 891/891 checks VERDE** (869 → 891: a FASE 28 nova, §2.1
  abaixo).
- **Gates locais TODOS verdes**: scope (o diff todo dentro de
  app/src/main/cpp/ · tests/ · docs/ · app/build.gradle) · docs-lint 91 ·
  gmesh-docs 25 · ui-vocab · theme-hex · glyph · hierarchy · projects-ui ·
  reload · check_main · link_parity 122 TUs · release-identity vc 74.
- **versionCode: 73 → 74** (app/build.gradle:29 + o §14 do
  RELATORIO-0.9.6.md — o gate release-identity/R-015 verde com o par).

### A FASE 28 do c33 virtual (+22 checks, 869→891)

No caminho REAL do device (o drawTics com o blockRebind e o storage REAL):

- 28.1 os 6 clones (o MESMO .gmesh escrito no storage do device, aberto
  pelo GpuAssets, ligado pelo blockRebind): o drawTics REAL funde em
  **1 drawElementsInstanced × 6** (o glstub conta), dc=1 (o de sempre
  seria 6), ZERO drawElements individuais, e o BlockFrameStats diz
  «instanciados=6 desenhados=6» (a fonte do log/HUD).
- 28.2 o toggle OFF num clone: **6 blocos em 2 dc** (1 instanciado × 5 +
  1 drawElements individual — o orquestrador salta a colheita do TIC
  desligado).
- 28.3 o .gmesh SKINADO: desenha pelo caminho de sempre (1 drawElements
  individual; o ÚNICO draw instanciado é o dos 6 estáticos,
  lastInstanceCount=6 — se o guard morresse, o skinado entraria no batch
  e a pose quebrava).
- Recalibração honesta da FASE 20.2 (o efeito do default ON no caminho
  real): os 2 blocos do mesh de 80 000 verts desenham agora como
  drawElementsINSTANCED (1 instância por assinatura de material) — o dc é
  o MESMO 2 (1 por bloco, o check anterior passa intacto) e o HULL
  continua a nunca desenhar. O mesmo efeito que a recalibração da 19.4 na
  3.5a (a cura mudou a MECÂNICA, o contrato manteve-se).

## §3 Pin C33 (o dono preenche — com PNG)

- Antes: FPS 5 · dc 5164 · verts <N> (o baseline da ABERTURA)
- Depois: FPS <N> · dc <N> · verts <N>
- PNG do HUD (o chip «bl desenhados/totais» + o dc; o log diz
  «instanciados=N»)
- Zero crash dumps novos: <N>

(a cena dos 6 modelos de 203 MB: com os 6 TICs a apontar o MESMO
.gmesh+material, o dc desce de ~5164 para ~863 + os draws dos TICs não
instanciados — a divisão por 6 dos blocos; o instancing: on/off no
inspector alterna a fundição. Esperado do prompt: 30+ FPS e dc < 500 —
o §4 diz honestamente o que a mecânica dá com 863 blocos por modelo.)

## §4 Limites / não-verificado (nenhum escondido)

- **A INVESTIGAÇÃO DO CULLING (o §1.8 do prompt) — o bl 5164/5178
  EXPLICA-SE COM NÚMEROS, o culling está VIVO (nada a fixar):** o pin
  `fase35b_culling_investigacao` (test_fase35b_instancing.cpp) replica a
  condição do dono e imprime o dump (o output INTEIRO, colado):
  ```
  [r047-cull] AABB da cena: min(-2.0,0.0,0.0) max(3.0,0.0,1.0) · sceneFarthest=3.2 u · editor far=450.0 u near=0.30 u (o far DINÂMICO contém a cena inteira — o plano far nunca corta)
  [r047-cull] AABB do bloco 0: min(-1.00,0.00,-1.00) max(1.00,0.00,1.00) — 25 verts (o AABB é o volume REAL da geometria do bloco)
  [r047-cull] planos do VP (a·x+b·y+c·z+d < 0 = fora): L(0.87,-0.50,-0.00,6.00) R(-0.87,-0.50,-0.00,6.00) B(0.00,-0.50,-0.87,6.00) T(0.00,-0.50,0.87,6.00) N(0.00,-1.00,-0.00,11.70) F(0.00,1.00,0.00,438.01)
  [r047-cull] vista de cima + far do editor: 24/24 blocos visíveis (com os 6 modelos empilhados como o dono tem, o frustum lateral contém TUDO — o 5164/5178 do dono é o MESMO efeito: só o near/lados cortam o resto)
  [r047-cull] frustum de gameplay (far 8 u): 0/24 visíveis (o cull CORTA de verdade — o R-040 confirma-se vivo)
  ```
  - A hipótese (a) do prompt CONFIRMA-SE: o far é DINÂMICO
    (core/SceneBounds.h + `camerautil::editorClips` no main.cpp —
    `far = max(1.5×zoom, sceneFarthest + zoom + 10, 450)`), o piso é
    **450 u** e a cena do dono (os 6 modelos ao pé da origem,
    sceneFarthest ~dezenas de u) cabe TODA dentro: o plano far NUNCA
    corta; com a vista quase vertical os 6 modelos preenchem o frustum
    lateral — o 0.27% cortado (14/5178) é a geometria atrás do near ou
    nos cantos.
  - A hipótese (b) FALSIFICADA: o AABB do bloco É o volume real da
    geometria (a quantização 16-bit dequantiza para o MESMO volume — o
    pin prova o AABB = os dados).
  - A hipótese (c) FALSIFICADA: com um frustum de gameplay (far 8 u) o
    cull corta 24/24 — o culling corre e escolhe (a sentinela R-040
    `blockmesh_frustum_por_bloco_audit` vigia-o desde o 0.10-M).
  - CONCLUSÃO: NÃO é bug — é o comportamento declarado da casa («o
    espaço sem tetos», 0.8.9: o far cobre a cena para o editor nunca
    comer geometria ao orbitar). A alavanca do FPS do dono NÃO é o cull
    do frustum nesta cena — é o INSTANCING (esta sub-fase) e, mais à
    frente, o LOD/impostores (o lodDistance do dono fica a 0 por
    default; ligar o LOD nos 6 modelos corta os índices a ½ à distância).
- **O dc esperado no C33 é honesto e NÃO é < 500 com a cena intacta:**
  o instancing funde POR (mesh, bloco, material, textura) — com os 6 TICs
  a apontar o MESMO .gmesh de ~863 blocos, o dc desce de ~5164 para
  ~863 + os extras (6× de redução; o alvo do prompt era 40-50 FPS). O
  dc < 500 do §3.5b.9 exige OU o dono a importar com «expandir nós»
  (6 meshes de menos blocos), OU os blocos do modelo a fundirem-se em
  menos entradas (uma evolução do conversor — NÃO desta sub-fase). O
  30+ FPS é plausível (o GPU processa o MESMO nº de vértices em 6×
  menos draws — o gargalo do GE8320 é o draw call, não o vertex); a
  medição é do dono.
- **A ordem de blend do frame instanciado** — os blends de TICs
  instanciados desenham DEPOIS de TODOS os opacos (mais correto para o
  depth do que a ordem por-TIC de antes); entre um TIC não-instanciado e
  os opacos instanciados mantém-se a classe de aproximação inter-TIC de
  sempre (documentada na FASE 2 — a cena do dono é opaca, sem efeito).
- **O custo do batch por frame** — o stable_sort do batch é O(n log n)
  sobre os picks (5178 no pior caso do dono) — na sandbox o frame
  inteiro do c33 com 891 checks corre em segundos; no device o custo é
  de CPU barato vs 4300 draw calls poupados (a medição real é do dono).
- **O rasterizador do stub agora aplica o instancing** (o fb do slot 1) —
  os testes existentes que contam drawElements no caminho dos blocos
  contam instanciados quando o default ON está ativo; a única
  recalibração foi a FASE 20.2 (documentada no §2) — os restantes
  contadores do c33 continuaram verdes à primeira.
- **O sign-off no C33 físico é do dono** (o padrão de todas as fases) —
  o pin §3 é dele; o 30+ FPS e o PNG do HUD fecham a sub-fase.
- **O V.ONI não ganhou entradas** (a regra 1.6 — o VoniRegistry.cpp não
  foi tocado; VONI_referencia.md intacto).

## §5 Ficheiros tocados

| Área | Ficheiros |
|---|---|
| render | `render/Material.h/.cpp` (o UBO Instances + uInstOn no VS), `render/Mesh.h/.cpp` (drawInstanced), `render/Renderer.h/.cpp` (drawMeshInstanced + o setInstancing(false) do drawMesh), `render/BlockMesh.h/.cpp` (o classifyFrame extraído + collectVisible/drawInstancedGroup/drawBlendPicks/storageStats), `render/DrawStats.h` (instancesMerged) |
| components | `components/MeshRenderer.h` (o toggle instancing, default ON) |
| core | `core/SceneSerializer.cpp` ("instancing":false só quando false), `core/UndoStack.h/.cpp` (o TicSnap transporta) |
| ui | `ui/EditorLayout.h` + `ui/EditorUi.cpp` (a linha «instancing: sim/não») |
| platform | `platform/main.cpp` (o drawTics colheita→agrupamento→submissão + o BlockFrameStats.instancesMerged + o log «instanciados=N») |
| testes | `tests/test_fase35b_instancing.cpp` (NOVO, 9 pins + o pin da investigação §1.8), `tests/test_sentinels.cpp` (R-047), `tests/c33_virtual.cpp` (FASE 28 + a recalibração da 20.2), `tests/stub/GLES3/gl3.h` + `tests/stub/glstub_fb.h` (o instancing no stub/rasterizador), `tests/CMakeLists.txt` (o TU novo) |
| build | `app/build.gradle` (versionCode 73→74) |
| docs | `docs/RELATORIO-0.11-W-ABERTURA.md` (commit próprio cf6279a), `docs/RELATORIO-0.11-W-3.5B.md` (este), `docs/RELATORIO-0.9.6.md` (§14 vc 74) |
