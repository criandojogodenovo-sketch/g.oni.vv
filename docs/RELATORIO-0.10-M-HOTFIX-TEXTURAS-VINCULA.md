# RELATORIO-0.10-M-HOTFIX-TEXTURAS-VINCULA.md

**0.10-M HOTFIX** — a spec do dono (C33): «modelo importado com texturas
vem sem textura; as imagens viram vários .gtx mas nada os liga ao mesh;
a atribuição manual no inspector troca a textura inteira em vez de
preencher o slot certo». Cura em 4 pernas + (e)/(f) do vocabulário
TIC e da sub-árvore real. NADA mais tocado.

## 1. A causa raiz (ficheiro + função, as 4 probabilidades confirmadas)

| # | Causa | Onde vivia |
|---|---|---|
| a | a tabela de materiais do gmesh NÃO guarda refs aos .gtext por slot | `v3WriteMaterial` (GOwnFormats.cpp:444) — entrada = `u16 len + nome UTF-8 + pad`, SÓ O NOME |
| b | o caminho GPU não carrega .gtext nos samplers por slot | `LitMaterial` (render/Material.h) tem UM sampler (unit 0); `BlockMesh::draw` (BlockMesh.cpp:847) recebia UMA `const Texture*` para TODOS os blocos — o `materialIndex` de cada bloco nunca escolhia textura |
| c | o parse copia as imagens mas perde o índice material→textura→imagem | `parseGltf` (GltfImporter.cpp:630) — `textureSources` era LOCAL à função e morria; `GltfMaterial::baseColorTex/normalTex` viviam só em RAM durante a conversão |
| d | o picker do inspector escreve um slot único global | `applyAssetPick` (EditorUi.cpp:3650-3680) — escrevia `mr->texture + mr->texPath` (a textura INTEIRA do TIC), sem slots nem material |

## 2. A cura

1. **Vinculação automática no import** — o bloco **GTB1** APPENDADO ao
   FIM da secção de materiais do `.gmesh` (`gmeshV3AppendBinds`,
   GOwnFormats.cpp; chamado pelo `gmeshV3Skeleton` partilhado pelos DOIS
   escritores — zero drift). O índice que morria no parse é reconstruído
   pelo `convertGltfCommon` (AssetConverter.cpp, o mapa
   `convert::MatTexMap` material→5 slots→.gtext com os nomes DERIVADOS
   `assets/<stem>_<imgIdx>.gtext`). O runtime resolve no
   `GpuAssets::blockHull` (a baseColor de cada material → GL, uma linha
   de log por slot vinculado) e o `BlockMesh::draw` usa a textura DO SEU
   material POR BLOCO (`tex ? tex : materialTexture(materialIndex)`).
   O import/instantiate/apply auto-vincula o TIC (texPath+texture quando
   a peça tem 1 material; multi-material fica por bloco) — **zero
   passos manuais no caso comum**.
2. **Cada .gtx mantém-se** — o que se constrói é a VINCULAÇÃO (material
   = conjunto de slots), NUNCA um atlas. Retro-compat: os leitores de
   sempre PARAM em `materialCount` nomes e ignoram a cauda (o contrato
   «o fim pode trazer bytes a mais» já documentado); ficheiros antigos
   abrem byte a byte; CRC mau = erro legível.
3. **Inspector** — a secção Material ganha as **5 linhas de slot**
   (`TexSlotRow`, EditorLayout.h — kBindBaseColor..kBindOcclusion, o
   contrato `kBindSlotCount = 5`) com o nome do .gtext em cada; o tap
   arma `EditorState::inspTexSlot` e abre o browser; o pick escreve O
   SLOT (`MeshRenderer::texSlotRef(slot)` — o slot 0 É o texPath de
   sempre) **sem tocar nos irmãos**. Serializer (texNormal/texMetalRough/
   texEmissive/texOcclusion) + Undo (TicSnap) seguem os slots.
4. **Multi-material** — N materiais = N bindings no GTB1; o draw POR
   BLOCO usa a textura do material de cada bloco (nunca um material
   global). O GL carrega o sampler que o shader tem (baseColor→unit 0);
   os slots 1-4 ficam VINCULADOS (tabela+inspector+serializer+log) e
   sobem a GL quando o shader ganhar os samplers — a fronteira honesta
   deste hotfix (o shader lit de hoje não amostra normal/emissive).

## 3. (e) + (f) — o vocabulário TIC e a sub-árvore REAL

- **(e)** «expandir nós» → **«expandir em sub-TICs»** no toggle das
  Definições (SettingsPage.cpp), nos logs (`import: modo expandir em
  sub-TICs`, a linha contrato `gmesh: v3 expandir em sub-TICs=<N>`) e
  nos docs vivos (README, GMESH_formato §EXPANDIR-EM-SUB-TICs). A chave
  `expandNodes=1` em settings.goni MANTÉM-SE (retro-compat dos projetos).
- **(f)** a sub-árvore é REAL: o `convertGltfToV3Nodes` (GmeshV3Stream.cpp,
  o 2.º passe) encontra o ancestral REGISTADO mais próximo (os nós vazios
  não viram TICs — o pin do 5B mantém-se), compõe a cadeia LOCAL PARA A
  FRENTE (a MESMA ordem de multiplicações do `worldChainOf` — o composto
  é o bit do mundo, sem inversos) e o registo sai com `parentRec` + TRS
  LOCAL quando a composição é TRS exata; shear continua FLAT (TRS mundo/
  baked — o log DIZ). O `gltfExpandInstantiate` cria os TICs em 2 passadas
  e compõe os mundos (helper público `gltfExpandComposeWorlds` — o fit do
  import re-escala SÓ as raízes e recomps). O **`TransformSystem::tick`
  agora compõe pais** (`world = world(pai) ∘ TRS` — a dívida «editor
  polish» do F3 paga; TICs sem pai ficam byte a byte; ciclo resolve como
  raiz, nunca pendura). O botão «reconverter assets» passa a ser SÓ
  re-expandir/colapsar (o import já entrega a árvore).

## 4. As provas (os pins do dono)

| Pin | Estado | Prova |
|---|---|---|
| import 2+ texturas → render texturado sem toque + 1 log/slot | CUMPRIDO | `bind_import_2_slots_e_log_por_slot` (GTB1 com 2 slots + as 2 linhas) + o `blockHull` loga por material no load |
| 4 materiais = 4 bindings | CUMPRIDO | `bind_4_materiais_4_bindings` (4 entradas no GTB1, uma por material) |
| atribuir a um slot não limpa os irmãos | CUMPRIDO | `audit_pick_escreve_o_slot_e_irmaos_intactos` (o pick por slot; none limpa SÓ o slot armado) |
| 50 sintéticas → 50 vinculadas, zero manual | CUMPRIDO | `bind_50_sinteticas_roundtrip` (10 materiais × 5 slots) |
| mutação: vinculação saltada → vermelho | CUMPRIDO | M-TB1 (o append morto) — 13 TESTs vermelhos (o import falha na verificação do próprio ficheiro) |
| mutação: slot assign limpa irmãos → vermelho | CUMPRIDO | M-TB3 (o picker volta ao slot único) — `audit_pick…` + 20 vermelhos |
| sub-árvore real + composição | CUMPRIDO | `subarvore_real_pai_local_e_mundo_composto` + `transform_system_compoe_pais` |
| retro-compat | CUMPRIDO | `bind_retrocompativel_sem_gtb1` (sem bloco abre igual; CRC mau = «CHECKSUM CORROMPIDO») |

**Mutações vermelho→verde** (cp dos backups): M-TB1 (a vinculação
saltada em `gmeshV3Skeleton`) → 13 vermelhos · M-TB3 (o picker limpa os
irmãos em `applyAssetPick`) → 21 vermelhos. TODAS repostas →
**test_core 932/0** · **c33 797/797 HARNESS VERDE**.

## 5. Gates

scope (todo o diff dentro de ci/scope.txt) · docs-lint 76 ficheiros ·
gmesh-docs **25 âncoras** (2 NOVAS: bind slots kBindSlotCount=5, bind
append gmeshV3AppendBinds) · ui-vocab · theme · hierarchy · gizmo ·
projects-ui · reload · glyph · jni-parity · check_main · link-parity ·
release-identity versionCode **64** (declarado no RELATORIO-0.9.6 §14).
A linha do CI entra pelo commit -docs (a prática da casa).

## 6. NÃO VERIFICADO (honestidade da casa)

- **O RENDER DOS SLOTS 1-4** (normal/metalRough/emissive/occlusion no
  GL): os slots ficam VINCULADOS (GTB1+inspector+serializer+log) mas o
  shader lit de hoje tem SÓ o sampler baseColor (unit 0) — carregá-los a
  GL sem o shader os amostrar era VRAM morta. O upload por slot entra
  quando o shader ganhar os samplers.
- **O sign-off do C33 físico** (o dono instala o APK assinado do CI e
  vê: o import com texturas chega TEXTURADO sem toque; a secção
  Material com os 5 slots; a sub-árvore no painel Hierarchy).
- **A leitura do GTB1 sob SAF sem stat** (`baseColorBindOf` devolve ""
  quando o provider não dá stat — o auto-bind do apply fica para a
  abertura por blocos, que lê o GTB1 na abertura; o pin do device usa
  peças → BlockMesh → cobre).
- **Projeto JÁ importado antes do hotfix**: os .gmesh antigos não têm
  GTB1 — reimportar (ou reconverter + reaplicar o mesh) escreve as
  vinculações.
