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
- **Leitura runtime**: `ResourceManager.cpp` (branch `ext == "gmesh"`) —
  `storage_->readBytes(path, bytes)` lê o FICHEIRO INTEIRO para a RAM e
  chama `readGMesh`; o log é a linha da casa
  `asset: load <nome>.gmesh verts=N em Xms`.
- **Upload GPU**: `GpuAssets` monta o interleaved stride-32
  (`render/Vertex.h`: pos 3f + normal 3f + uv 2f) a partir do MeshData
  dequantizado.
- **Picking/colisores/OBJ**: consomem o MESMO MeshData (nada sabe de
  bytes .gmesh — a camada de bytes morre em `readGMesh`).

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
        u32 flags                (bit0 = skinned)
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
tabela). Acima disso devolve erro legível que nomeia o PASSO 4 (render
por blocos) — o FICHEIRO está correto. As pools são POR BLOCO (por
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
