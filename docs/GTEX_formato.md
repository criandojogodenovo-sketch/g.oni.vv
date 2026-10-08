# FORMATO DE TEXTURAS (.gtext / .gtc) — FONTE DE VERDADE (0.10-M · PASSO 1)

> Descreve o caminho da textura COMO ELE É HOJE no código. Fonte de
> verdade para qualquer pessoa ou IA; o gate `scripts/gmesh_docs_check.py`
> verifica que cada símbolo citado entre backticks existe no código.
> As evoluções do PASSO 5 (presets, mosaicos, sRGB) entram aqui quando
> existirem.

## 1. Os dois formatos em disco

### 1.1 `.gtext` — o comprimido PERSISTIDO (assets/)

Magic `GVTX`, header comum de 32 bytes (idêntico ao `.gmesh` — ver
GMESH_formato.md §2), escrito por `writeGText`, lido por `readGText`.
Payload (a seguir ao header):

| Secção | Campos | Notas |
|---|---|---|
| Formato | `u8` | um de `CompressedFormat` (§2); > 4 → «formato de textura desconhecido» |
| Dims | `width u32`, `height u32` | ≤ 16384 cada (teto do leitor) |
| Mips | `mipCount u32`, depois mips × (`width u32`, `height u32`, `offset u32`, `size u32`) | mipCount ≤ 32; cada mip ≤ dims do nível 0 |
| Blob | resto do payload | contíguo, mesma disposição de `CompressedImage::data`; offsets dos mips validados contra o blob |

### 1.2 `.gtc` — o CACHE em disco (textures/cache/)

Escrito/lido por `TextureCache` (magic `GTC`):

| Offset | Campo |
|---|---|
| 0..3 | magic `G`,`V`,`T`,`C` |
| 4 | `versão = 1` (u8) |
| 5 | `CompressedFormat` (u8) |
| 6..7 | reservado (0) |
| 8..11 | `width u32` |
| 12..15 | `height u32` |
| 16..19 | `mipCount u32` |
| 20..27 | hash de origem `u64` (eco — valida a chave) |
| 28.. | `mipCount` × (`width u32`, `height u32`, `offset u32`, `size u32`) |
| ... | blob contíguo dos mips |

A CHAVE do cache = FNV-1a 64 dos BYTES do PNG de origem
(`TextureCache::hashBytes`); nome do ficheiro
`cache_<hash16>_<fmt>.gtc` em `textures/cache/` (`TextureCache::kDir`).
PNG alterado → hash diferente → re-comprime (a entrada antiga fica órfã
até limpeza manual — barato e nunca falso-hit).

## 2. Os formatos de compressão (`CompressedFormat`)

| Valor | Formato | B/px | Notas |
|---|---|---|---|
| 0 | `RGBA8` | 4 | sem compressão; upload `glTexImage2D` + `glGenerateMipmap` na GPU |
| 1 | `ETC2_RGB` | 0.5 | `GL_COMPRESSED_RGB8_ETC2` (opaco) |
| 2 | `ETC2_RGBA` | 1.0 | `GL_COMPRESSED_RGBA8_ETC2_EAC` (com alpha) |
| 3 | `ASTC_4x4` | 1.0 | `GL_COMPRESSED_RGBA_ASTC_4x4_KHR` |
| 4 | `ASTC_6x6` | ~0.44 | `GL_COMPRESSED_RGBA_ASTC_6x6_KHR` |

## 3. Os compressores (`TextureCompressor` e filhos)

- `PassthroughCompressor` — identidade RGBA (1 nível; mips na GPU). O
  caminho de toda a textura quando não há compressão.
- `Etc2Compressor` — ETC2 via `vendor/etcpak`; escolhe RGB8/RGBA8_EAC
  sozinho; cadeia de mips COMPLETA em CPU. `heuristics=false` restringe
  os blocos (decodificáveis pelo decoder dos testes do CI).
- `AstcCompressor` — ASTC LDR via `vendor/astc-encoder`; bloco 4x4
  (default) ou 6x6, RGBA sempre.
- `HardwareCompressor` — a FACADE de seleção: ASTC se
  `setAstcSupported(true)` (o device injeta `glAstcSupported()`), senão
  ETC2; `lastFormat()` diz a última decisão.

**Gate de tamanho** (`TextureCompressor::canCompress` /
`Etc2Compressor::compressible`): ambas as dims ≥ 256 px (em sprites
pequenos o overhead de bloco supera o ganho) — abaixo disso → RGBA8.

## 4. A orquestração (`TexturePipeline::process`)

PNG bytes → hash → **cache hit** = lê o blob comprimido do disco
(`TextureLoadInfo::via == "cache"`) | **miss** → decode (`loadPng`) →
gate 4K (com compressão disponível a textura 4K entra INTEIRA — ETC2 4K
≈ 8 MB de VRAM; sem compressão, `downscaleTo2K` reduz por fator 2 até
2048 com AVISO em `TextureLoadInfo::warn`) → compressão → store no cache.

## 5. Mips e decode

- `MipGen.h` (`genMipChainRGBA`/`halveImageRGBA`): cadeia completa em CPU,
  média box 2×2 (a MESMA matemática do `downscaleTo2K`), desce até 1×1.
- `PngLoader.h` (`loadPng`): qualquer PNG → RGBA 8-bit via stb_image
  (`vendor/stb/stb_image.h`). `kMaxImageBytes = 64 MB` no conversor
  (`AssetConverter.h`) — foto de 64 MB+ recusa com erro legível.

## 6. As texturas do import glTF (`GltfTextures`)

- `extractGltfTextures`: os PNG embutidos do glTF → hash FNV-1a dos
  bytes → `textures/gltf_<hash16>.png` (mesmos bytes → MESMO ficheiro —
  dedup natural entre materiais/meshes/modelos; só escreve se ainda não
  existe). `outMeshTex` fica com 1 caminho por mesh ("" = sem textura).
- Textura EXTERNA (uri não data:) → o uri relativo direto. Mime não-PNG
  (ex.: jpeg) → caminho VAZIO sem falha (o mesh importa; a textura fica
  ausente; KHR extras fora do escopo).
- No conversor (`AssetConverter.cpp`, passe de TEXTURAS): cada imagem
  vira `assets/<stem>[_i].gtext` pelo `TexturePipeline`; FALHA PARCIAL
  NÃO ESCONDE O MODELO (uma textura má = aviso `texWarn` + o mesh entra
  com material por defeito).
- Upload GPU: `GpuAssets.cpp` lê `.gtext` (`readGText`) e submete com
  `glCompressedTexImage2D` por nível — zero re-compressão por arranque.

## 7. Tetos e suposições (hoje)

| Teto | Onde | Nota |
|---|---|---|
| 16384 px | `readGText` | dims acima recusadas com erro legível |
| 32 mips | `readGText` | — |
| 64 MB PNG | `kMaxImageBytes` (conversor) | o decode é inteiro em RAM |
| 4 B/px RGBA | `RawImage` | o decode intermédio é sempre RGBA8 |
| sRGB | — | NENHUMA flag hoje: o pipeline trata tudo como dados lineares que chegam ao shader como chegam (o PASSO 5 decide a política) |
| normais | — | sem caminho sem perda de canais hoje (ASTC/ETC2 comprimem RGBA igual) |
