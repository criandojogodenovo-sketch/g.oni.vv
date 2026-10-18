# FORMATO .gmesh — FONTE DE VERDADE (0.10-M · PASSO 1)

> Este documento descreve o formato `.gmesh` **como ele é hoje no código**
> (v1). É a FONTE DE VERDADE para qualquer pessoa ou IA: o que aqui está
> foi lido de `app/src/main/cpp/assets/GOwnFormats.{h,cpp}` e verificado
> pelo gate `scripts/gmesh_docs_check.py` (cada símbolo citado entre
> backticks tem de existir no código — gate verde = doc honesta).
> A evolução v3 (blocos, 64-bit) é especificada no relatório do PASSO 2
> e entrará AQUI quando existir.

## 1. Identidade e princípios

- Magic: `GMES` (4 bytes ASCII).
- Little-endian explícito em TODOS os campos multi-byte (memcpy de
  `u16`/`u32`/`u64`/`f32` — o host e o arm64 do device são LE).
- Toda a leitura valida magic → endian → versão → tamanho → checksum →
  contagens ANTES de tocar arrays. Corrupção = erro legível, nunca crash.
- Determinismo: mesmos dados → mesmos bytes (o round-trip do CI aperia).
- GL-free / Android-free: o leitor e o escritor correm no CI Linux com
  bytes em memória (sem GL, sem NDK).

## 2. O cabeçalho comum (32 bytes — partilhado pelos 3 formatos próprios)

O header é o MESMO para `.gmesh` (`GMES`), `.gtext` (`GVTX`) e `.gm`
(`GANM`). Escrito por `writeHeader`, lido/validado por `gReadHeader`.
A constante de tamanho é `kGHeaderBytes = 32` — leitores e escritores
partilham-na (a regra: o tamanho é CONTRATO).

| Offset | Tam | Campo | Valor / significado |
|---|---|---|---|
| 0 | 4 | `magic` | `GMES` / `GVTX` / `GANM` |
| 4 | 2 | `version` | `1` (o escritor grava 1; o leitor rejeita != 1) |
| 6 | 2 | `endianMark` | `0x1A2B` lido de volta — trocado = ficheiro de outra endianess |
| 8 | 4 | `align` | `8` (eco informativo do alinhamento das secções) |
| 12 | 8 | `payloadSize` | bytes de payload a seguir ao header |
| 20 | 8 | `checksum` | FNV-1a 64 do PAYLOAD (`gfnv1a`) |
| 28 | 4 | reserved | `0` (padding até 32) |

Ordem de validação em `gReadHeader` (primeiro erro devolvido com mensagem
legível):

1. `len < 32` → «ficheiro próprio curto demais (N B)»;
2. `magic` != esperado → «magic errado (esperado GMES)…»;
3. `endianMark` != `0x1A2B` → «endianess trocada…»;
4. `version` != 1 → «versão N desconhecida (a engine lê a 1)»;
5. `payloadSize + 32 > len` → «payload truncado: header diz N B mas só há M B»;
6. `gfnv1a(payload)` != `checksum` → «CHECKSUM CORROMPIDO: … o ficheiro
   foi danificado».

FNV-1a 64: offset `1469598103934665603` (0xcbf29ce484222325), primo
`1099511628211` (0x100000001b3) — a MESMA convenção do `primMeshHash`.

## 3. Payload do .gmesh v1 (a seguir ao header de 32 bytes)

Escrito por `writeGMesh(const MeshData&, std::vector<u8>&, std::string&)`,
lido por `readGMesh(const u8*, size_t, MeshData&, std::string&)`.

| Secção | Campos | Notas |
|---|---|---|
| Contagens | `verts u32`, `indices u32` | verts ≤ 65535 (teto do formato, ver §5) |
| Bounds | 6 × `f32` | AABB min xyz + max xyz do MESHO espaço das posições |
| Grupos | `groups u32` | ≤ 4096 |
| Skin flag | `u8` | 0 = estático, 1 = skinned |
| Vértices × verts | pos `u16`×3 + normal `u16`×3 + uv `u16`×2 = **8 bytes/ vértice** | quantização 16-bit (§4); skinned acrescenta joints `u8`×4 + weights `u16`×4 (+12 B) |
| Índices × indices | `u16` | referem o array de vértices; count múltiplo de 3 (triângulos) |
| Grupos × groups | nome `str_`, material `str_`, `firstIndex u32`, `indexCount u32` | `str_` = `u16` len + bytes UTF-8 sem nulo (len truncado a 65535) |
| Skin × verts | joints `u8`×4, weights `u16`×4 | só se flag=1; soma dos pesos renormalizada na leitura |

## 4. A quantização 16-bit (a perda DE QUALIDADE do v1)

O v1 é um formato PERDIDO (lossy) — o v3 do PASSO 2 mata isto:

- **pos**: `u16` no AABB do mesh — `quantF(v, mn, ext)` mapeia
  `(v-mn)/ext` para `[0,65535]`; dequant: `mn + q/65535·ext`. Resolução =
  ext/65535 (um modelo de 100 unidades → passo de 1.5 mm).
- **normal**: `u16` em [-1,1] (`quantS`/`dequantS`); a leitura
  RENORMALIZA (a quantização encolhe o módulo ~0.1%).
- **uv**: `u16` em [0,1] (`quantU`/`dequantU`) — uv fora de [0,1] é
  CLAMPADO (repeating wrap > 1 perde-se).
- **skin weights**: `u16` em [0,1]; a leitura renormaliza a soma dos 4
  (a quantização rouba ~0.002).

## 5. Tetos do formato (o que o v3 remove)

| Teto | Onde | Consequência |
|---|---|---|
| 65535 vértices | `writeGMesh` recusa (`m.vertices.size() > 65535`); `readGMesh` recusa (`nVerts > 65535`) | «mesh com N vértices — o limite do engine é 65535 (índices u16)» — o erro que o dono quer morto |
| índices `u16` | payload + `MeshData::indices` (`std::vector<u16>`) | causa raiz do teto acima |
| 4096 grupos | `readGMesh` | — |
| 16384 px | `.gtext` (não .gmesh) | ver GTEX_formato.md |
| payload `u64` | header comum | 2^64 B — sem teto prático no v1 |

## 6. Onde vive no pipeline

- **Escrita**: `AssetConverter.cpp` — o passe de SAÍDA do glTF/GLB escreve
  UM `.gmesh` por fonte (o mesh MERGED com as transforms dos nós BAKEADAS
  — `M·pos` + `worldRot·normal` — e as primitivas concatenadas) ou, no
  caminho sem nós, um por mesh. O OBJ passa pelo `ObjStreamParser` →
  mesmo `writeGMesh`.
- **Leitura runtime**: `ResourceManager.cpp` (branch `ext == "gmesh"`) — o
  **GUARDO CEDO** (0.10-M PASSO 3B) espia os 192 B do header+meta com
  `ProjectStorage::readBytesAt` + `gmeshV3PeekMeta` ANTES de ler o ficheiro
  inteiro: um v3 acima da parede recusa por 192 B (medido: a scene de
213 MB recusa com pico de RAM = base, 0 MB extra; a 1ª versão sem o
  espião media **2007 MB** para ler um .gmesh de 972 MB e recusar — no C33
  a app morria ANTES da mensagem). Passando o guard, o caminho de sempre:
  `storage_->readBytes(path, bytes)` lê o ficheiro inteiro (o
  `FsStorage::readBytes` agora dimensiona UMA vez — o pico era ~2× o
  ficheiro pela cópia string→vector) e chama `readGMesh`; o log é a linha
  da casa `asset: load <nome>.gmesh verts=N em Xms`.
- **Upload GPU**: `GpuAssets` monta o interleaved stride-32
  (`render/Vertex.h`: pos 3f + normal 3f + uv 2f) a partir do MeshData
  dequantizado.
- **Picking/colisores/OBJ**: consomem o MESMO MeshData (nada sabe de
  bytes .gmesh — a camada de bytes morre em `readGMesh`).
- **TEMPOS POR FASE (0.10-M PASSO 3B)**: cada fase do caminho loga a
  linha contrato `gmesh: fase=<parse|cut|assembly|verify|load|render>`
  ms=<n> (parse/corte/assembly/verificação no conversor; load na leitura
  runtime — logado no sucesso E na recusa; render no upload GPU do
  `GpuAssets`) + `import: copia ms=<n>` para a fase da cópia da fonte —
  os minutos de um import do dono passam a ter dono por fase.

## 7. O .gm (clips de animação) — resumo (a spec completa vive no código)

Mesmo header comum, magic `GANM`, escrito/lido por `writeGAnim`/
`readGAnim`: clips × (nome, tracks × (target `u8`, curve `u8`, element
`str_`, keys × (t f32, v f32×4, tanIn f32×4, tanOut f32×4))) + secção
opcional de esqueleto no FIM (hasSkeleton `u8` + joints com TRS + bind
TRS + inverseBind 4×4) — ficheiros v1 sem a secção continuam a abrir
(«ficheiros truncados sem a secção ficam VÁLIDOS»). Tetos: 256 clips,
4096 tracks/clip, 65536 keys/track, joints ≤ `SkeletonComp::kMaxBones`.

## §v3 — O FORMATO v3 (0.10-M PASSO 2 — O QUE O ESCRITOR ESCREVE HOJE)

Desde o 0.10-M o `writeGMesh` escreve SÓ v3 (`kGmeshVersionWrite = 3`).
O leitor abre v1, v2 e v3 (`kGmeshVersionMinRead = 1`). A v2 NUNCA
existiu no histórico do repo (a v1 nasceu na 0.8.10-a e nunca mudou o
campo) — a interpretação tolerante: v2 = payload v1 (decisão registrada
no relatório do PASSO 2; as fixtures `tests/fixtures/gmesh_v2_esfera.
gmesh` documentam os bytes exatos que abrem).

### O layout

    [header comum 32 B]  — magic GMES, version=3, endianMark, align=16,
                           payloadSize, checksum = FNV-1a dos 160 B de
                           metadados (NÃO do payload inteiro — ver abaixo)
    [metadados 160 B]    — fixos (kGmeshV3MetaBytes), 16-alinhados:
        u32 attrCount            (≤ kGmeshV3MaxAttrs = 8)
        u32 flags                (bit0 = skinned · bit1 = kGmeshV3FlagPiece
                                 — PEÇA do import expandido, §EXPANDIR-EM-SUB-TICs)
        u32 blockVertexCap       (o teto configurável; default
                                  kGmeshV3BlockVertexCap = 65535)
        u32 tableCrc32           (CRC32 das entradas da tabela)
        u64 vertexCount          (TOTAL — passa 2^32; é o ponto do v3)
        u64 indexCount           (TOTAL, múltiplo de 3)
        u64 blockCount
        u64 materialCount
        f32 aabbMin[3] aabbMax[3] (AABB global)
        u64 blockTableOffset      (offset ABSOLUTO no ficheiro)
        u64 materialTableOffset   (offset ABSOLUTO)
        8 × descritor de atributo (8 B cada: semantic u8, storage u8,
                                   normalized u8, reserved u8, u32)
        u32 reserved × 2
    [dados dos blocos]   — cada bloco alinhado a 16:
        [pool de vértices interleaveada SEGUNDO OS DESCRITORES]
        [índices locais u16 (indexType=0) ou u32 (indexType=1)]
    [tabela de blocos]   — blockCount × 80 B (kGmeshV3BlockEntryBytes):
        u64 dataOffset (ABSOLUTO — o mmap salta direto), u64 dataSize,
        u64 vertexCount, u64 indexCount,
        f32 aabbMin[3] aabbMax[3] (do bloco),
        u32 materialIndex, u32 indexType (0=u16/1=u32), u32 crc32 (dos
        dataSize bytes do bloco), u32 reserved, u64 reserved
    [materiais]          — materialCount × (u16 len + bytes UTF-8, pad a 4)
    [vinculação]       — OPCIONAL (o bloco GTB1 do hotfix TEXTURAS-VINCULA;
                         sem texturas o ficheiro acaba nos nomes — byte a
                         byte com o pré-hotfix; os leitores de sempre PARAM
                         em materialCount nomes e ignoram a cauda)
    [propriedades]     — OPCIONAL (o bloco GTB2 da FASE 2 0.11-M, DEPOIS do
                         GTB1; sem props não-default o ficheiro acaba no
                         GTB1 ou nos nomes — byte a byte com o pré-FASE 2)

### A vinculação de texturas por material (o bloco GTB1 — hotfix 0.10-M)

O import glTF escreve, DEPOIS dos nomes de material, o bloco de
vinculação material→slot→.gtext (a cura do bug do dono: os .gtext
existiam mas NADA os ligava ao mesh — o índice material→textura→imagem
morria no `textureSources` local do parse). Layout: magic u32 `GTB1`,
u32 bindCount (só materiais COM textura), bindCount × (u16
materialIndex · u8 slotCount · slotCount × (u8 slot · u16 pathLen ·
path) · pad a 4), u32 crc32 das entradas. Os 5 slots (o contrato
`kBindSlotCount = 5`): 0 baseColor · 1 normal · 2 metalRough ·
3 emissive · 4 occlusion. O LEITOR de sempre para em materialCount —
ficheiros novos abrem em builds antigas SEM saber; o CRC mau = erro
legível («CHECKSUM CORROMPIDO: bloco de vinculação (GTB1)»). O runtime
resolve a baseColor de cada material no `GpuAssets::blockHull` (uma
linha de log por slot vinculado) e o draw POR BLOCO usa a textura DO
SEU material — nunca um material global a substituir todos.

### As propriedades por material (o bloco GTB2 — FASE 2 0.11-M)

O import glTF escreve, DEPOIS do GTB1, o bloco de PROPRIEDADES
material→props (a FASE 2 do dono: alphaMode/alphaCutoff/doubleSided,
os QUATRO factors — baseColorFactor/metallicFactor/roughnessFactor/
emissiveFactor —, o transform UV por slot do `KHR_texture_transform` e
os samplers normalizados). Layout: magic u32 `GTB2`, u32 propCount (só
materiais com PELO MENOS UMA propriedade fora do default glTF —
`GMeshMatProps::isDefault`), propCount × ENTRADA FIXA de 164 B
(`kGmeshMatPropsEntryBytes`: u16 materialIndex · u8 alphaMode · u8
doubleSided · f32 alphaCutoff · f32 baseColor[4] · f32 metallic · f32
roughness · f32 emissive[3] · 5 × { f32 offset[2] · f32 rotation ·
f32 scale[2] · u8 wrapS · u8 wrapT · u8 minFilter · u8 magFilter }),
u32 crc32 das entradas. Os samplers viajam nos CÓDIGOS DA CASA
(`kSampler*`: 0 = o default REPEAT/LINEAR_MIPMAP_LINEAR/LINEAR — os
enums RAW do glTF não cabem num u8; a fonte do mapeamento é a função
pura `gltfSamplerToCodes`, o GL vive no `Texture::setSampler`). O MESMO
contrato de cauda do GTB1: o leitor de nomes para em materialCount, o
leitor de binds diz quantos bytes consumiu (o `consumed` do
`gmeshV3ReadBinds`) e o `gmeshV3ReadProps` arranca DAÍ; ficheiros novos
abrem em builds antigas SEM saber (perdem SÓ as props — os defaults
honestos entram, nunca o mesh); CRC mau = erro legível («CHECKSUM
CORROMPIDO: bloco de propriedades (GTB2)»). O runtime aplica as props
POR MATERIAL no draw por blocos (o `BlockMesh::propsOf` → as uniforms
do lit: uBaseColor/uAlphaMode/uAlphaCutoff/uTwoSided/uEmissive/uUvRot/
uUvOff — o lit PBR GGX completo que consome metallic/roughness é a
FASE 5; o GTB2 já os transporta). O critério do RUNTIME: um .gmesh
pequeno fundido com materialCount > 1 OU props não-default abre POR
BLOCOS (`GpuAssets::mesh`) — é o caminho onde o render POR MATERIAL
existe; o modelo pequeno de 1 material sem props fica no mesh único
byte a byte.

### O LOD por distância (a cauda GLOD — 0.11-T FASE 3.5a)

O import gera, DEPOIS do GTB2, a cauda do **LOD por distância**: o
`kGmeshV3FlagLOD` (0x4) acende-se no `meta.flags` e os bytes vivem ALÉM
do fim do payload declarado no header (o MESMO padrão de cauda do
GTB1/GTB2 — os leitores v3 de sempre nunca chegam lá; o `payloadSize`
NÃO conta o GLOD, ficheiros novos abrem em builds antigas SEM saber e
perdem SÓ o LOD, nunca o mesh; ficheiros antigos em apps novas abrem
sem LOD — a cauda não existe). Layout: magic u32 `GLOD`, u32 version=1,
u32 lodCount (== blockCount — o leitor recusa outro valor), lodCount ×
ENTRADAS de 80 B (o MESMO layout da tabela de blocos, 1:1 POR ORDEM — a
entrada k é o LOD do bloco k; `indexCount==0` = o bloco k não tem LOD),
u32 crc32 (de magic ao fim das entradas) e DEPOIS os BLOBS LOD nos
offsets ABSOLUTOS que as entradas declaram (16-alinhados, o MESMO
layout de dados dos blocos normais — pos/normal/uv f32 + índices, pele
quando o meta diz skinned). A DECIMAÇÃO é o parity-weld determinístico
`gmeshDecimateHalf`: o vértice ÍMPAR solda ao vizinho PAR mais próximo
(i-1/i+1) dentro de 25% da diagonal do AABB do bloco (fora disso o
ímpar é MANTIDO — nunca um sliver alongado), triângulos degenerados
CAEM, os atributos do REPRESENTANTE vencem (a pele também — a emenda de
pesos é a fronteira documentada; uma grelha row-major decima EXATAMENTE
para metade — o pin aferre o número). Um bloco pequenino (< 8 verts,
AABB degenerado, nada soldável) fica com a entrada VAZIA e o draw usa o
normal. O RUNTIME: o `BlockMesh` carrega o índice na abertura (a faixa
dos materiais cresce pelo índice — `gmeshV3LodIndexBytes` — os blobs
ficam no disco), as entradas LOD partilham o MESMO LRU/orçamentos das
normais e o draw troca bloco normal→LOD quando o TIC está além da
`lodDistance` do dono (o `Stats::lodDrawn` conta; o `.goni` transporta
"lodDist" por TIC SÓ quando != 0 — o default é OFF, o default jogável
manda).

### As semânticas de atributo (o layout é DESCRITO — acrescentar não muda a versão)

`kAttrPosition`(0) 3×, `kAttrNormal`(1) 3×, `kAttrUv0`(2) 2×,
`kAttrTangent`(3) 4×, `kAttrUv1`(4) 2×, `kAttrColor`(5) 4×,
`kAttrBones`(6) 4×, `kAttrWeights`(7) 4×. O armazenamento por atributo:
`kAttrF32`(0)/`kAttrU16`(1)/`kAttrU8`(2). O conversor escreve f32 (SEM
PERDA); u16/u8 existem para o modo compacto opt-in. O stride do vértice
= a soma dos componentes dos descritores (`GMeshV3Meta::vertexStride`).
Um leitor que não conheça uma semântica futura salta-a pelo descritor.

### A integridade sem ler o ficheiro inteiro

O FNV-1a do payload INTEIRO (a regra v1) obrigava a tocar todos os
bytes antes de usar qualquer um — inimigo do mmap/streaming. No v3:
o checksum do header cobre SÓ os 160 B de metadados; a tabela tem o seu
próprio CRC32; cada bloco tem o seu CRC32 (`gcrc32`) verificado quando
o bloco é materializado (`readGMeshV3Block`). Um bloco pode estar
CORROMPIDO e os outros continuam a abrir.

### A ponte para o runtime de hoje

`readGMesh` num v3 com ≤65,535 vértices TOTAIS monta o MeshData (1
grupo por bloco — os nomes passam a «bloco N»; os materiais vêm da
tabela). Acima disso — ou quando a estimativa do mesh único
(`gmeshV3LoadEstimateBytes`: verts×`sizeof(Vertex)` + índices×2 + pele
20 B/vértice) passa o orçamento declarado `kMeshLoadBudgetBytes`
(256 MB, o número da casa do teto de range R-032) — devolve o erro
**«memória insuficiente ao carregar mesh (cura no PASSO 4: render por
blocos)»** com os números (vértices, blocos, índices, ~MB): o runtime de
hoje monta o mesh ÚNICO em RAM e é ESSA a parede do modelo grande; a
causa fica REGISTADA com números, NÃO contornada — o FICHEIRO está
correto (a conversão verificou-o bit a bit) e abre por blocos (tabela
v3) quando o render por blocos existir. As pools são POR BLOCO (por
grupo/primitiva): vértices partilhados entre grupos viajam duplicados
por bloco — NUNCA soldados; o conjunto de triângulos é idêntico ao da
entrada. Vértices órfãos (referenciados por nenhum triângulo) não
sobrevivem ao corte. A pool de cada bloco sai em ordem ASCENDENTE do
índice original (o round-trip de um mesh fica IDÊNTICO ao de entrada).

### O corte em blocos

`v3CutGroup` corta por grupo/primitiva em blocos de ≤
`blockVertexCap` vértices, só em FRONTEIRA DE TRIÂNGULO (um triângulo
nunca fica partido por dois blocos). A pool do bloco = os vértices
REFERENCIADOS. indexType = u16 quando a pool ≤ 65,535; u32 quando
acima (o cap é configurável — a spec manda).

### O runtime por blocos (0.10-M PASSO 4 — A CURA CHEGOU)

O render por blocos EXISTE: um .gmesh v3 que o mesh único recusaria
(> 65.535 verts ou estimativa acima do orçamento) abre pela TABELA —
`gmeshV3PeekMeta` espia os 192 B, `gmeshV3ReadTableOnly` lê a faixa da
tabela, `gmeshV3ReadMaterialsOnly` a faixa dos materiais (NUNCA um byte
dos dados: o `BlockMesh` de `render/BlockMesh.h` é o dono do caminho).
O bloco materializa-se pela faixa exata (`readBytesAt` +
`gmeshV3MaterializeBlock` — o MESMO decodificador do `readGMeshV3Block`,
uma só fonte de verdade) na PRIMEIRA frame em que fica visível (lazy),
desenha-se bloco a bloco com frustum AABB por entrada da tabela, e a
cache é LRU com os ORÇAMENTOS DECLARADOS da casa:
`kBlockCacheRamBudgetBytes` (64 MB de MeshData retidos) +
`kBlockCacheVramBudgetBytes` (192 MB de uploads estimados) — o MESMO
total de 256 MB do mesh único, agora LIMITADO POR CONSTRUÇÃO, seja o
modelo de 38 MB ou de 972 MB. O picker/serializer recebem o HULL de
bounds (o AABB global do meta como mesh de 8 cantos — o contrato
MeshData→Mesh intacto, zero mudanças na UI do seletor); o picking, os
corpos de colisão e o AABB da cena leem os bounds do meta (a tabela,
nunca os dados); o export OBJ anda pela tabela com o stream por blocos
(`objStreamBegin`/`objStreamAppend` — pico de RAM = 1 bloco). As
métricas REAIS por frame (blocos totais/visíveis/desenhados, dc, verts)
vivem no HUD (o chip «FPS · TICs · bl n/m») e no `engine.log`
(throttled); a sentinela R-040 vigia os orçamentos para sempre.

### A fonte do streaming sob QUALQUER storage (0.10-M HOTFIX SAF-STREAM)

O conversor streaming lê o buffer 0 por uma CASCATA que NUNCA desce ao
legado por causa do armazenamento (a causa do «city»: o gate
`content://` desligava o streaming nos projetos SAF e o import caía no
merge do teto de 65.535):

1. **mmap por CAMINHO** do ficheiro real (`mapFile64`) — o de sempre
   (browser/Fs; raiz POSIX ou virtual com fonte real);
2. **mmap POR FD** — o mmap não precisa de caminho: sob SAF (raiz
   `content://`) a fonte oficial é o FD DO IRMÃO JÁ COPIADO em `source/`
   (`ProjectStorage::openReadFd` → `SafStorage` resolve o URI e abre pelo
   bridge `bridgeOpenFd`; `fileapi::mapFd64` mapeia o descritor direto —
   no device é o `ContentResolver.openFileDescriptor` + `detachFd`);
3. **pread de RANGES** — o degradado honesto quando o provider recusa o
   mapa (FUSE sem mmap, streams): cada TAREFA do corte carrega o seu
   SPAN por `readBytesAt` (o pico de RAM é o maior span em voo — a mesma
   fórmula do mmap, sem a higiene MADV em heap); a verificação re-carrega
   o que precisa. O log nomeia a escolha: `asset: v3 fonte=<mmap-caminho
   |mmap-fd|ranges|data-uri>`.

O 4.º canto: um `.gltf` SEM ficheiro de buffer (`data:` URI) também
streama — o `parseGltf` exporta os bytes embutidos (`streamOwnedBin`) e o
conversor corre sobre eles. Com isto, o teto de 65.535 deixa de ser
visível a um modelo SEM pele em qualquer storage; a PELE acima do teto
falha com a mensagem clara «pele ainda não suportada no streaming
(BACKLOG 0.10-A)» (o merge é o que preserva joints/weights — ver o
BACKLOG.md, entrada 0.10-A SKELETAL).

A linha contrato de tempos ganhou a fase que faltava: `gmesh:
fase=parse` cobre SÓ o JSON; o SCAN DE RANGES do bin (as imagens que
materializam os seus bufferViews durante o parse) loga `gmesh:
fase=ranges ms=<n>` EM LINHA PRÓPRIA — os 5.395 ms que o dono viu no
parse do city passam a ter dono. A sentinela R-041 (mapFd64: bytes
exatos / recusa real do SO / fd do Fs) e a FASE 21 do c33_virtual (o
city de 72 primitivas importando sob `content://` pelo fd do bridge, e
o provider que recusa o mapa ficando VERDE pelos ranges) vigiam o
hotfix para sempre.

### §SAF-SEAM — o runtime por blocos pela MESMA cascata + a QUARENTENA (0.10.6)

O HOTFIX SAF-STREAM fechou a costura do CONVERSOR; sobravam DUAS no
RUNTIME — o `BlockMesh::open` (a abertura por blocos) abria por RANGES
do storage mesmo quando o provider dá FD, e os MATERIAIS morriam sob um
provider SEM tamanho (as sondas de 256 B+ exigem o comprimento EXATO e
a cauda de nomes é quase sempre mais curta — «cidade» são 8 B).

**A abertura usa a MESMA cascata do conversor** (nunca um caminho POSIX
sob `content://`): `openReadFd` (o FD DO BRIDGE) → `mapFd64` serve as 3
leituras (peek 192 B + tabela + materiais — ZERO ranges) → o provider
que recusa o mapa (FUSE) degrada para **pread das faixas NO MESMO fd**
→ o pipe/stream (fstat 0 B) cai nos RANGES do `readBytesAt`. O fd e o
mapping vivem SÓ durante a abertura — o lazy de cada bloco continua a
ser o `readBytesAt` de sempre (o contrato do PASSO 4 intacto). A linha
contrato do load NOMEIA a fonte: `gmesh: fase=load ms=<n> (por BLOCOS:
tabela de <k> blocos, <b> B lidos, fonte=<mmap-fd|pread-fd|ranges>;
dados=<d> B por carregar em lazy)` — o par exato do `asset: v3 fonte=`
do conversor (é assim que o dono confere no engine.log que runtime e
conversor leem pelo MESMO fd).

**Os materiais sem tamanho** leem-se NOME A NOME (a tabela é
auto-descritiva: `u16` comprimento + nome + pad a 4 do offset ABSOLUTO —
o MESMO alinhamento do escritor): a leitura incremental monta os bytes
exatos sem conhecer o tamanho do ficheiro; as sondas continuam a ser a
primeira tentativa (uma leitura quando a cauda é grande).

**Em falha, o `err` diz TUDO**: QUAL das 3 leituras (peek/tabela/
materiais), o errno do pread quando o fd recusa e o tamanho do ficheiro
VISTO (`ficheiro visto com <n> B`) — a linha de sempre confundia «não
encontrado» com «faixa ilegível» sem dizer qual nem porque.

**A QUARENTENA do asset corrompido** (o peek v3 VERDE + a tabela ou os
materiais que não validam ou acabam além do fim): o ficheiro é renomeado
`<rel>.corrupt` — os bytes FICAM para forense, o NOME sai do catálogo
(o picker lista `.gmesh`, nunca `.corrupt`) — e a mensagem é
«**asset corrompido, reimporta** — <causa>». NUNCA a troca silenciosa
por primitiva: o TIC MANTÉM o mesh anterior e o toast do dispatch diz a
causa (`GpuAssets::lastMeshError`). O rename é o primitivo novo
`ProjectStorage::rename` — `FsStorage` = `::rename` POSIX atómico;
`SafStorage` = **cópia streaming por fd** (uma abertura de leitura +
o write stream do destino em chunks de 1 MB + remove; a interface SafIo
não tem `renameDocument`) com a cache de URIS a seguir o nome (o
`exists()` não mente depois da quarentena); um peek que NÃO valida (v1/
garbage) NÃO entra em quarentena — o caminho do mesh único decide.

**O overlay de import nunca diz «0 / 0»**: o total vem do length do
bridge (os atómicos do job); `length=0` com a fonte a correr mostra
«copiando… <N> B» (os bytes que já correram) e «a copiar…» no arranque;
totais sub-megabyte mostram-se em B inteiros (o corte reporta
TRIÂNGULOS, o assembly bytes de SAÍDA — a divisão de MB incondicional
arredondava «0 / 0 MB» durante o import de 29 MB). A função é PURA e
aferível (`importOverlayBytesText`).

A sentinela R-043 (o rename nas 3 camadas de storage) e a FASE 23 do
c33_virtual (a abertura nomeando `fonte=mmap-fd` sob `content://`, o
chip «bl n/m», o overlay, o provider que recusa o mapa e a quarentena
com o rename streaming) vigiam tudo isto para sempre.

### §EXPANDIR-EM-SUB-TICs — o import de nós como sub-árvore e as PEÇAS (0.10-M EXT; o hotfix 0.10-M tornou a sub-árvore REAL)

O BACKLOG 0.10-M-ext do dono: «opção no import "expandir nós" que cria
um TIC por nó com mesh (nomes do glTF preservados, transformação do nó
como Transform do TIC), em vez de fundir num TIC só. Default fundido por
performance mobile; expandido para peças editáveis». O **default é o
FUNDIDO de sempre** — o caminho vigente byte a byte idêntico (o driver
só entra com o setting «expandir em sub-TICs» ligado nas Definições;
`settings.goni` guarda `expandNodes=0/1`, capturado no lançamento do job
de import e honrado pelo «reconverter assets»).

**O driver das peças** (`convertGltfToV3Nodes` em GmeshV3Stream.cpp):
agrupa as `primRefs` por nó (a ordem do array de nós) e faz de CADA
nó-com-mesh UMA chamada completa ao `convertGltfToV3` de sempre — corte,
assembly e VERIFICAÇÃO bit a bit por peça (a mesma rotina, zero código
novo de corte). O produto é uma **PEÇA por nó**:

- ficheiro `assets/<stem>_<nomeDoNó>.gmesh` (o nome do glTF sanitizado;
  anónimos viram `n<i>`; dois nós com o mesmo nome ganham o sufixo
  determinístico `_<i>`); a linha contrato do expand no log:
  `gmesh: v3 expandir em sub-TICs=<N> peça(s) blocos=… verts=… tris=… verificado=<0|1>`;
- o registro `convert::Output::NodeOut` (nome, TRS, índice da peça em
  `Output::meshes`) — é ISTO que o `gltfExpandInstantiate` (assets/
  GltfInstantiate.cpp, GL-free) lê para criar os TICs: um TIC por nó com
  mesh, o nome preservado, o TRS no `Transform3D`, o `MeshRenderer` com a
  ref da peça (no device o binder é o `GpuAssets::blockHull`);
- o **bit1 do meta** (`kGmeshV3FlagPiece`): «este .gmesh é UM NÓ» — o
  `GpuAssets::mesh` abre a peça **POR BLOCOS mesmo pequena** (o peek de
  192 B já lê o meta; sem a flag, um .gmesh de 1000 verts caía no mesh
  único SEM culling). É o bit1 que dá o **culling por TIC** do pin do
  dono: cada peça é um `BlockMesh` próprio e o frustum AABB por bloco é
  avaliado com o model do TIC dono — o MESMO caminho lazy+LRU do PASSO 4.

**A transformação e a geometria** (a parte fina): o registro traz o TRS
**MUNDO** do nó, decomposto por `mat4ToTRS` da matriz da `worldChainOf`
(a MESMA rotina do bake das tarefas — a igualdade é garantida por
construção), e a peça sai com a geometria **CRUA no espaço local** (as
`primRefs` da peça levam `node=-1`: o bake é a identidade). É assim
porque o engine compõe FLAT (`TransformSystem::tick` não resolve pais —
verificado no código, «trabalho da fase de editor polish»): com o TRS
mundo no TIC, a peça desenha no sítio EXATO sem hierarquia entre TICs, e
«transformação do nó como Transform do TIC» fica literal. Um nó cujo
mundo tem SHEAR (escala não-uniforme do pai composta com rotação do
filho — sem representação TRS exata; a reconstrução T·R·S diverge) cai
no bake do mundo na geometria (as `primRefs` mantêm o nó → o bake de
sempre pela `worldChainOf`) com TRS identidade no registro — o desenho é
exato na mesma e o LOG DIZ («tem SHEAR»; nunca silencioso).

**O fecho no main** (`importJobFinish`): com registros no Output, a
sub-árvore ENTRA na cena — sem diálogo «aplicar ao TIC?» (os TICs novos
são o resultado), com o fit único (a caixa do modelo inteiro em mundo =
união dos AABBs das peças transformados pelos seus TICs; `pos*=s` e
`scale*=s` em cada TIC novo — a translação FORA do TRS encolhe o
conjunto NO SÍTIO sem partir o layout relativo; o mesmo alvo
`kImportTargetSize` do apply), o 1º TIC selecionado e o toast «expandido:
N TIC(s) na cena». Animações: os clips de um .gm ficam SEM attach no
modo expandido (os alvos de um clip são NÓS do glTF — o expand é para
peças; o BACKLOG 0.10-A traz os clips) — o log diz.

As sentinelas: 5 TESTs `ext_*` no `test_gmeshv3stream.cpp` (peças com
nomes/flag/verificação + a geometria TOTAL igual à do fundido; o AABB
LOCAL da peça provando a geometria crua; o shear no bake com TRS
identidade; o instantiate puro; a peça por blocos no GpuAssets com o
contraste fundido) e a FASE 22 do c33 (o pin: o city espalhado em 72
faixas vira 72 TICs com nomes e o frustum estreito culle 71, deixa 1) —
R-042 no `docs/REGRESSOES.md` com as mutações M-E1..M-E4.

### §THUMBS — as miniaturas pré-renderizadas (0.10-M PASSO 5C)

A spec do dono: «no import, renderizar uma miniatura 256×256 e guardá-la
em thumbs/; o browser mostra-a imediatamente». O CONTRATO tem três pernas
e vive em DOIS módulos novos e só neles:

- **`thumbs/<stem>.png`** — UM PNG 256×256 por mesh do import (o stem da
  peça: `assets/city_n3.gmesh` → `thumbs/city_n3.png`; o import FUNDIDO
  escreve a do mesh único). A escrita é o ÚLTIMO passo do `importFile`
  (depois da verificação da cópia, antes do rácio) — `writeImportThumbs`
  em AssetConverter.cpp chama `ThumbIndex::ensureThumb` com `force=true`
  (o asset ACABOU de mudar: o render é sempre novo) e a MESMA rotina é
  a que o browser usa sem o force (o hit devolve o PNG pronto). O
  `Stats::thumbs` conta as escritas (o log do rácio e o c33 leem-no).
  Falha de thumb = aviso no log + o import SEGUE (o browser fica com o
  placeholder — NUNCA mata o import).

- **`ThumbRender`** (assets/ThumbRender.cpp — PURO, sem GL, afervel no
  CI): o render SOFTWARE da miniatura — câmara ortográfica isométrica
  fixa (a vista 3/4) que ENCAIXA o AABB do mesh com 8% de folga de cada
  lado, z-buffer de floats, lambert flat com a luz da casa (a MESMA
  direção `kLightDir` do Material) e fundo grafite. DETERMINÍSTICO
  (mesmos vértices → mesmos bytes — TEST). O orçamento: um .gmesh
  gigante contribui com os PRIMEIROS `kMaxTrisPorThumb` (4096)
  triângulos — a pré-visualização é representativa e o custo fica
  limitado. O `placeholder` (asset sem render possível): fundo + quadro
  + as iniciais do stem (1.ª+última letra — a regra D18 das telas de
  projeto; «grelha1» → «G1»). O PNG sai pelo `encodePngRgb` do
  render/ThumbPng (o zlib do 0.9.0) em RGB.

- **`ThumbIndex`** (assets/ThumbIndex.cpp — o lado do browser, GL-free):
  `hasThumb` (existe o PNG?), `ensureThumb` (o RENDER SOB DEMANDA —
  falta? renderiza do .gmesh e escreve; o asset AUSENTE é erro limpo e
  NUNCA nasce thumb do nada), `loadRows` (o custo de LISTAR: carrega+
  decodifica as thumbs de N assets; o que falta entra como placeholder
  no sítio). O render sob demanda lê o .gmesh PELA TABELA DE BLOCOS do
  PASSO 4 — peek 192 B **por faixa** (`readBytesAt 0..192`), a tabela,
  o data do bloco 0 e `gmeshV3MaterializeBlock`: **o pico de RAM é UM
  bloco, nunca o modelo** (a lição R-044: um peek pelo ficheiro INTEIRO
  mediu 1965 MB no gigante de 50M verts; a leitura por faixa volta ao
  99 MB do orçamento).

Os pins do dono, medidos: o import escreve a thumb POR MESH (o glTF de
2 nós com mesh na EXPAND → 2 peças → 2 thumbs — TEST + c33 24.1 sob
content://); 100 assets no browser = 100 thumbs carregadas+decodificadas
em **15,6 ms** no hospedeiro (o piso: <500 ms — TEST pin_100_thumbs_em_menos_de_500ms) e
**0,5 ms** sob o provider SAF do c33 (FASE 24.4); thumb ausente =
placeholder + render sob demanda VERDE (TEST + c33 24.2); o fantasma
não nasce (TEST + c33 24.3). As mutações TH1..TH4 (o import sem thumbs,
o peek pelo ficheiro inteiro, o loadRows a mentir, o guard do fantasma
morto) — vermelho→verde em `docs/REGRESSOES.md` R-044.
