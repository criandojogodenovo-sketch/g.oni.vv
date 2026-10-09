# RELATÓRIO 0.10-M · PASSO 4 — O RENDER POR BLOCOS

Fase: **0.10-M PASSO 4** (commit próprio, destravado pela continuação do
dono). O que o PASSO 4 manda: o runtime percorre a TABELA de blocos do
.gmesh v3, faz frustum AABB por bloco (só o visível sobe ao GPU), mantém
uma cache LRU com orçamentos de RAM/VRAM DECLARADOS, carrega em lazy (a
1ª visibilidade), e o picking/corpos de colisão/export OBJ andam pela
tabela; o HUD e o log dizem as métricas REAIS (totais/desenhados/dc/
verts — coerentes com a frame); os pins: 920 blocos → render RAM ≤
orçamento, culling por audit, LRU ≤ orçamento sob orbit contínuo; as
mutações: culling morto → vermelho · sem evicção → vermelho · eager no
import → vermelho; P-05 PNG com o HUD; **PÁRA no fim**.

**O contexto do device (dono, C33, vc 57)**: o modelo de 203 MB importa
(verificado=1, .gmesh de ~972 MB) e o apply recusa com a mensagem do 3B —
«memória insuficiente ao carregar mesh (cura no PASSO 4: render por
blocos)» — a causa REGISTADA com números. Este passo É a cura prometida.
Nada de contornos: o mesh único continua a recusar no ResourceManager
(a rede de sempre); quem cura é o caminho NOVO.

---

## §1 · A ARQUITETURA (a tabela manda, o disco serve, o orçamento fecha)

```
   .gmesh v3 no storage
   ┌──────────────────────────────────────────────────────────┐
   │ [header 32][meta 160][DADOS DOS BLOCOS…][tabela][materiais]│
   └──────────────┬───────────────────────────▲──────▲─────────┘
        o PEEK    │                           │      │  a FAIXA dos materiais
     (192 B) ─────┘                    a FAIXA│da tabela (blockCount×80 B)
                                              │
   BlockMesh::open(st, ref) ── gmeshV3PeekMeta + gmeshV3ReadTableOnly
       │                              + gmeshV3ReadMaterialsOnly
       │  ZERO bytes dos DADOS (o storage CONTA: 3 leituras no pin)
       ▼
   por FRAME (drawTics → mr->blocks):
       frustumPlanes(vp)  ── os 6 planos (Gribb-Hart, colunas do clip)
       por entrada da tabela:
           blockVisible(model, blk, planes)?  ── 8 cantos × 6 planos
           │ NÃO → CULL (o bloco não paga dc, não sobe, não lê)
           │ SIM → ensureBlock(i): lazy
           │        ├ sem cpu → readBytesAt(dataOffset, dataSize)
           │        │            + gmeshV3MaterializeBlock (o MESMO
           │        │            decodificador do readGMeshV3Block)
           │        └ upload Mesh::create[Skinned] (fase=render por bloco)
           └ drawMesh(*gpu, model, vp, tex, tint, bones…)
       evictOverBudget() ── LRU estrito: VRAM (192 MB) primeiro,
                             RAM (64 MB) depois — a cache NUNCA passa
```

As decisões que o dono deve conhecer:

- **Os orçamentos da casa**: `kBlockCacheRamBudgetBytes` = 64 MB (os
  MeshData retidos para re-upload sem disco) + `kBlockCacheVramBudgetBytes`
  = 192 MB (os uploads estimados: verts×`sizeof(Vertex)` + índices×2 +
  pele 20 B/vert — o contrato aferível `blockMeshVramBytes`). A soma é
  **o MESMO 256 MB do `kMeshLoadBudgetBytes` do 3B** — o número da casa
  não cresceu; o que mudou é que agora é **limitado por construção**,
  seja o modelo de 38 MB ou de 972 MB.
- **O HULL de bounds**: o GpuAssets devolve, para um .gmesh de blocos, um
  `Mesh*` de 8 cantos com o AABB GLOBAL do meta (bounds EXATOS por
  construção — o `create` calcula-os dos cantos). É o «Mesh*» do contrato
  do picker/serializer: o fit/normalização do import, o AABB da cena
  (`camerautil::sceneAABB`) e o gizmo funcionam **sem carregar nada**.
  O render é do `BlockMesh` (o drawTics consulta `MeshRenderer::blocks`
  PRIMEIRO — o hull nunca desenha; se algo o desenhasse, mostraria a CAIXA
  do modelo, diagnóstico visível, nunca lixo). **ZERO mudanças em
  ui/EditorUi** — o `applyAssetPick` funciona palavra a palavra como
  estava (a FASE 20 aplica pelo picker REAL).
- **A fronteira dos dois caminhos**: o caminho dos blocos dispara nas
  MESMAS condições em que o mesh único recusaria (verts > 65 535 ou
  estimativa > 256 MB — o MESMO peek de 192 B do 3B, uma só verdade). Um
  .gmesh pequeno segue o caminho de sempre (1 dc, sem taxo de gestão);
  o ResourceManager fica INTOCADO (a recusa do 3B é a rede de segurança).
- **O skin**: cada bloco traz os seus aJoints/aWeights (o v3 grava-os por
  vértice); o draw por blocos usa o MESMO contrato de matrizes do mesh
  único (`computeSkinMatrices` do TIC dono) — o teste
  `blockmesh_skinned_por_blocos` sobe 2 blocos com pele.
- **TERM_WINDOW**: o `releaseAll` de sempre apaga TUDO (o re-open custa
  192 B + tabela — nunca dados; o mesmo contrato do re-upload dos Mesh
  comuns). O `detachRenderersFromGpu` desliga `mr.blocks` ANTES do
  releaseAll (use-after-free nunca).

## §2 · AS FAIXAS NO GOwnFormats (os leitores PARTILHADOS)

O `readGMeshV3Meta` (o caminho do ficheiro inteiro) foi REFACTORADO para
chamar os validadores novos — **zero drift entre os dois leitores**:

| função (assets/GOwnFormats.cpp) | o que lê | quem a usa |
|---|---|---|
| `gmeshV3ReadTableOnly(tableBytes, tableLen, meta, blocks, err)` | as `blockCount`×80 B da faixa: cada entrada validada (indexType/cap/contagens/dataSize esperado) + o **CRC32 da própria tabela** | `readGMeshV3Meta` (inteiro) · `BlockMesh::open` (faixa) |
| `gmeshV3ReadMaterialsOnly(matBytes, matLen, meta, materials, err)` | `materialCount` nomes 4-alinhados | idem |
| `gmeshV3MaterializeBlock(dataBytes, dataLen, meta, blk, out, err)` | a faixa EXATA de UM bloco: CRC32 ANTES de qualquer parse + o decode de atributos (o MESMO do `readGMeshV3Block`, que agora é um wrapper) | `BlockMesh::ensureBlock`/`materializeBlock` · o verify do conversor (pelo wrapper) |

O materialize por faixa rejeita `dataLen != blk.dataSize` à entrada (o
wrapper do ficheiro inteiro valida os offsets absolutos e delega). O
**anti-retry-storm** de upload GL (3 falhas seguidas → os carregamentos
param até um open novo) cobre o driver morto sem TERM — o mesmo espírito
do `primNeg`.

## §3 · O CULLING (o audit puro — a prova do pin)

`BlockMesh::frustumPlanes` extrai os 6 planos do VP (linhas 3±i da matriz
clip, normalizadas; identidade ⇒ o cubo [-1,1]³ — o pin da fórmula em
R-040). `BlockMesh::blockVisible` transforma os **8 cantos do AABB do
bloco** pelo `model` do TIC dono e testa contra cada plano: fora só
quando TODOS os cantos estão fora do MESMO plano — conservativo-exato
para OBB (um modelo rodado nunca perde geometria visível; pode ficar por
excesso — documentado, honesto). O audit (`visibleBlocks`) devolve a
LISTA dos visíveis — é o que o teste da câmara-meia afera:

- a câmara centrada nos blocos 0..3 com meia-largura 8: vê **exatamente
  os 4 da esquerda** (os 4..7 culled);
- a câmara com o alvo deslocado 200 unidades: **0 visíveis** (tudo fora
  dos planos laterais);
- a câmara larga: **os 8**;
- o `model` transladado +100 com a câmara larga centrada nele: **os 8**
  (a transformação entra nos cantos).

## §4 · O LAZY, O LRU E OS ORÇAMENTOS (os pins)

- **A abertura é a tabela** (o anti-eager, MEDIDO): o
  `CountingStorage` do teste conta **3 leituras** (peek 192 B + tabela +
  materiais) e **ZERO na região dos dados**; nada residente, `ramBytes`
  0 — o apply de um modelo de 972 MB não queima um byte de geometria.
- **O lazy**: a 1ª frame com o gigante de 2 blocos carrega **SÓ os
  visíveis** (`loadedThisFrame == visíveis`); a 2ª frame **relê zero**
  (o caminho quente da cache). A FASE 20 mostra o mesmo no device: o
  apply loga UMA linha `fase=load` (a tabela); os `fase=load`/`fase=render`
  **por bloco** aparecem na 1ª frame de visibilidade.
- **O LRU**: `evictOverBudget` corre DEPOIS do draw de cada frame —
  contabilidade viva (RAM conta TODAS as cópias CPU, com ou sem GPU
  vivo; VRAM conta os uploads), ordem LRU estrita (menos-vistos primeiro,
  desempate estável), VRAM primeiro (o pool caro), RAM depois. Um
  conjunto visível MAIOR que o orçamento gera churn com UM aviso no log
  («o conjunto visivel (X MB) excede o orcamento VRAM (Y MB)») — honesto,
  1× por open.
- **O PIN DOS 920 BLOCOS** (fork + VmHWM reiniciado; 920 blocos × 600
  verts = 552 000 verts ≈ 19 MB de dados; orçamento APERTADO de
  laboratório 2+4 MB — a casa declara 64+192 MB):

| fase | base | pico | delta | o teto | veredicto |
|---|---|---|---|---|---|
| **A** — orbit realista (dist 40, ~15% visível, 30 frames) | 31 924 KB | 31 924 KB | **0 KB** | ≤ 5 MB | **VERDE com folga total** — a cache cabe no que já estava mapeado |
| **B** — pior caso honesto (dist 120, o frustum engole os 920 TODOS, 20 frames de churn) | 31 924 KB | 42 540 KB | **10 616 KB** | ≤ 19 MB (o modelo inteiro) | **VERDE** — o churn fica ABAIXO do caminho do mesh único |

  Em AMBAS as fases a contabilidade da cache ficou ≤ orçamento em TODOS
  os frames (verificação direta no filho — sai com `_exit` se passar).
  O pino do orbit contínuo (60 frames, orçamento 192+64 KB) mantém a
  cache ≤ orçamento EM TODOS os frames com evicção real (o
  `evictedTotal > 0` aferido).

## §5 · O PICKING, OS CORPOS DE COLISÃO E O EXPORT (a tabela, nunca os dados)

- **Picking/apply**: o `res.mesh(rel)` do `makeAssetResolvers` cai no
  `GpuAssets::mesh` → peek 192 B → `blockHull(ref)` → o BlockMesh aberto
  + o hull no slot. O `meshExtent` do resolver lê os bounds do HULL (=
  o AABB do meta) — o fit de sempre funciona. A FASE 20 prova pelo
  picker REAL (o «Sim» do diálogo de import).
- **Corpos de colisão/AABB da cena**: os shapes da física NÃO leem o mesh
  (componentes de sempre); o `camerautil::sceneAABB` lê os bounds do
  `mr.mesh` — que num TIC de blocos é o HULL com o AABB global do meta: o
  far dinâmico contém o modelo SEM carregar nada. Zero mudanças em
  core/SceneBounds.h.
- **OBJ export**: `beginExportToDownloads` consulta o BlockMesh aberto e
  corre o STREAM: um bloco materializado de cada vez
  (`objStreamBegin`/`objStreamAppend` com numeração GLOBAL de v/vt/vn),
  o MeshData do bloco morre AGORA (pico = 1 bloco). O parity da geometria
  contra o caminho inteiro (parse dos dois OBJs: mesmas contagens, mesmo
  multiconjunto de posições) é do CI (`blockmesh_export_por_tabela…`).

## §6 · O HUD E O LOG (a métrica REAL, coerente com a frame)

- **O HUD do editor**: o chip «FPS · TICs» do canto direito da tab bar
  ganha «**· bl n/m**» (desenhados/totais do frame) — a FONTE é o
  `g_blockFrame` do drawTics, alimentado ao `BottomState` (`blDrawn`/
  `blTotal`) ANTES do `bottom::draw`; **sem blocos em cena o chip e o
  layout são O DE SEMPRE** (a reserva `kFpsW`+`kFpsWBlocks` só cresce
  com `blTotal > 0` — os testes de layout sem blocos ficam intactos).
  A FASE 20 afera o HUD a mudar com o orbit: «bl 2/2» → «bl 1/2» →
  «bl 2/2», e o label do chip está no REGISTO do draw (audit).
- **O HUD do play**: a status line de baixo ganha «bl %u/%u» (o campo só
  aparece com blocos — a linha de sempre intacta).
- **O LOG**: `blocos: total=%u visiveis=%u desenhados=%u dc=%u verts=%u
  carregados-agora=%u ram=%.1fMB vram=%.1fMB (residente, orçamento
  64+192 MB)` — throttled (1×/s e só quando o nº de desenhados MUDA; o
  `g_blockLogIntervalSecs` é 0 no harness para determinismo) e corre em
  editor E play (depois do drawTics — a métrica é da frame que ACABOU de
  desenhar). Os dc/verts somam-se ao `DrawStats` da frame — o HUD, o log
  e a status line dizem o MESMO número.

## §7 · OS FIXES (ficheiro · função)

| ficheiro | o que entrou |
|---|---|
| `assets/GOwnFormats.h/.cpp` | `gmeshV3ReadTableOnly`/`gmeshV3ReadMaterialsOnly`/`gmeshV3MaterializeBlock` (os leitores por faixa); `readGMeshV3Meta` refactorado para OS CHAMAR (zero drift); `readGMeshV3Block` vira wrapper do materialize partilhado |
| `render/BlockMesh.h/.cpp` (NOVOS) | a classe inteira: `open` por faixas (com a cascata de materiais: statBytes exato → sondagens decrescentes no SAF), `frustumPlanes`/`blockVisible`/`visibleBlocks` (o audit), `ensureBlock` (lazy + anti-retry-storm), `evictOverBudget` (LRU estrito com as duas pool), `draw` (cull→lazy→draw→evict + Stats), `createHull`, `materializeBlock` (o export/pins), `setBudgets` |
| `render/GpuAssets.h/.cpp` | o registry `blockMeshes_` (1 ref = 1 BlockMesh), `blockMeshIfOpen`, `blockHull` (abre + hull + a linha do apply), a branch dos blocos no `mesh()` (o MESMO peek/condições do 3B), `releaseAll` apaga tudo |
| `components/MeshRenderer.h` | o campo runtime `BlockMesh* blocks` (nunca serializado; o blockRebind sincroniza pelo meshPath) |
| `platform/main.cpp` | `blockRebind()` no PONTO SEGURO; o caminho dos blocos no `drawTics` (com skin pelo MESMO contrato); `g_blockFrame`/`g_blockFramePrev`/`g_blockLogIntervalSecs`/`g_statusLine`; o log throttled depois do drawTics; o HUD no `BottomState` antes do `bottom::draw`; o «bl %u/%u» na status line do play; `detachRenderersFromGpu` desliga `mr.blocks`; o export por stream no `beginExportToDownloads` |
| `ui/BottomPanel.h/.cpp` | o chip ganha «· bl n/m» + a reserva `kFpsWBlocks` SÓ com `blTotal > 0` (a métrica que o dono pediu no HUD — a ÚNICA mudança de UI do passo, cirúrgica e reportada) |
| `assets/ObjExporter.h/.cpp` | `ObjStreamBlock` + `objStreamBegin`/`objStreamAppend` (o stream por blocos com numeração global) |
| `tests/test_blockmesh.cpp` (NOVO) | os 9 casos (abertura/paridade, frustum audit, lazy, LRU orbit, pino 920 fork+VmHWM, corrupção isolada, export parity, v1 recusado, skinned) |
| `tests/test_sentinels.cpp` | R-040 `regress_render_por_blocos_r040` (orçamentos literais + estimativa + frustum) |
| `tests/c33_virtual.cpp` | FASE 20 (+36 checks) + o gerador `buildGlbFase20` (2 meshes separadas de 40k); RECALIBRAÇÃO honesta da 19.4/19.5 (a recusa do 3B virou a CURA — o mesmo 70k agora aplica e renderiza) |

**NADA mais tocado**: V.ONI, seleção, física, TICs, storage de projetos,
janelas (a única exceção UI é o chip do HUD — a métrica que o próprio
PASSO 4 manda no ecrã), EditorUi/EditorLayout/serializer (o hull preserva
os contratos), SceneBounds (lê o hull), ResourceManager (a recusa do 3B
fica como rede).

## §8 · AS SUÍTES E OS GATES (o estado no fim)

- `test_core`: **0 teste(s) com falha** (72 TUs — os 9 novos do
  test_blockmesh + a R-040).
- `c33_virtual`: **706/706 checks, 0 falha(s) — HARNESS VERDE**
  (670 → 706: a FASE 20 inteira + as recalibrações da 19.4/19.5).
- Gates locais: scope-check (24 ficheiros no scope) · docs-lint
  (69 ficheiros) · **gmesh-docs 21 âncoras** (as duas novas dos
  orçamentos) · ui-vocab · theme-hex · hierarchy · gizmo · glyph ·
  jni-parity · reload · link parity 118 TUs · check_main (main.cpp +
  StorageBridge + OboeBackend compilam) · release-identity
  (versionCode **58**).
- P-05: `docs/p05/passo4-hud-blocos.png` (o PNG real da frame — 1600×720
  com o chip «FPS · TICs · bl 2/2» renderizado; 207 cores na faixa do
  chip, o texto desenhado).

## §9 · AS MUTAÇÕES (vermelho → verde, contra o código FINAL)

| mutação | o que se mutou | VERMELHO (a prova) | reposta |
|---|---|---|---|
| **M1** culling morto | `BlockMesh::blockVisible` devolve `true` sempre | `blockmesh_frustum_por_bloco_audit` FALHOU: «n == 4», «onlyLeft», «== 0» (3 checks) | cp do backup → VERDE |
| **M2** evicção morta | `BlockMesh::evictOverBudget` retorna à entrada | `blockmesh_lru_orcamento_contido_sob_orbit` FALHOU: **65 checks** «frame N: ram/vram … > orçamento» (todos os frames passaram o teto) | cp do backup → VERDE |
| **M3** eager no open | loop de `ensureBlock` em TODOS os blocos dentro do `open` | `blockmesh_abre_por_faixas…` FALHOU «st.rangeReads == 3» (a abertura leu os dados) + `blockmesh_lazy…` FALHOU «loadedThisFrame == 8» (nada sobrou para o lazy) | cp do backup → VERDE |

Repostas → `test_core` 0 falhas · `c33_virtual` 706/706. A sentinelas
que apanham cada mutação: M1 → o audit do culling (+ a R-040 no pin do
frustum); M2 → o pin do orçamento por frame; M3 → o pin da abertura (o
storage contador) + o pin do lazy.

## §10 · AS RECALIBRAÇÕES HONESTAS (no MESMO commit)

- **c33 19.4**: a fixture de 70k do 3B (que se aplicava para PROVAR a
  recusa) — a recusa era a promessa da cura; com o PASSO 4 o MESMO 70k
  **aplica e renderiza** (5 blocos pelo corte do conversor): os checks
  da recusa («FALHOU ao carregar», a mensagem «memória insuficiente», o
  toast «falha ao carregar») viram os checks da cura («blocos: …
  aplicado», «blocos: total=5», o toast «mesh aplicado»). A mensagem do
  3B CONTINUA no código (a rede do ResourceManager) e nos testes de
  unidade do readGMesh — só a FASE do device é que passou para o lado
  verde da história.
- **c33 19.5**: o export de logs traz agora a linha DA CURA (o «blocos:
  … aplicado») em vez da linha da recusa.
- **statusLine**: a linha de baixo (que só existe em PLAY) ganhou o campo
  «bl» — sem blocos, byte a byte a de sempre; o throttle do log de
  métricas saiu da statusLine para depois do drawTics (corre em editor E
  play — antes só em play).

## §11 · NÃO VERIFICADO (honesto — o device real, na mão do dono)

Nada disto tocou o device C33 — tudo acima é CI (Linux x86_64 + stub GL).
Para o sign-off no aparelho (vc 58), o dono verifica:

1. **O 203 MB RENDERIZA**: importar o modelo de 203 MB → aplicar ao TIC →
   o modelo aparece (blocos a subir à medida que o orbit os vê — o 1º
   segundo pode ter micro-solavancas do lazy; depois a cache entrega).
2. **O HUD muda com o orbit**: o chip «FPS · TICs · bl n/m» — orbitar
   para longe/fora e ver o «n» cair; voltar e vê-lo subir SEM recargas (o
   log `blocos: total=… desenhados=…` na Consola diz o mesmo).
3. **A RAM estável**: Settings › Diagnóstico (ou as Opções de
   programador) — orbitar 30 s e conferir que o RSS não cresce sem fim
   (o teto da casa: ~64 MB de cache + o resto da app).
4. **O export OBJ do gigante**: menu ⋯ › Exportar OBJ — o ficheiro sai ao
   Download/GOneVV/export (o log diz «export por BLOCOS … pico de RAM =
   1 bloco»).

## §12 · PÁRA

O PASSO 4 fecha aqui: commit próprio, CI a correr, gates verdes, provas
coladas. **PÁRA** — o PASSO 5 (texturas ASTC) e o PASSO 6 (a prova de
mesa no C33) ficam à espera do OK explícito do dono.
