# FORMATO DE TEXTURAS (.gtext / .gtc) — FONTE DE VERDADE (0.10-M · PASSO 5A)

> Descreve o caminho da textura COMO ELE É HOJE no código. Fonte de
> verdade para qualquer pessoa ou IA; o gate `scripts/gmesh_docs_check.py`
> verifica que cada símbolo citado entre backticks existe no código.
> O PASSO 5A entrou: perfis, redução por política, flags, normal maps.

## 1. Os dois formatos em disco

### 1.1 `.gtext` — o comprimido PERSISTIDO (assets/)

Magic `GVTX`, header comum de 32 bytes (idêntico ao `.gmesh` — ver
GMESH_formato.md §2), escrito por `writeGText` (v2) e lido por `readGText`
(v1 e v2). Payload (a seguir ao header):

| Secção | Campos | Notas |
|---|---|---|
| Formato | `u8` | um de `CompressedFormat` (§2); > 4 → «formato de textura desconhecido» |
| Flags | `u8` (SÓ na v2) | bits `kTexFlagSrgb`/`kTexFlagNormal` (§3); v1 não tem este byte e lê como 0 |
| Dims | `width u32`, `height u32` | ≤ 16384 cada (teto do leitor) |
| Mips | `mipCount u32`, depois mips × (`width u32`, `height u32`, `offset u32`, `size u32`) | mipCount ≤ 32; cada mip ≤ dims do nível 0 |
| Blob | resto do payload | contíguo, mesma disposição de `CompressedImage::data`; offsets dos mips validados contra o blob |

Versões: `kGtextVersionWrite = 2` (o escritor), `kGtextVersionMinRead = 1`
(o leitor abre as duas). Um ficheiro v1 antigo carrega byte a byte com
flags 0 — nunca muda o aspeto de quem não fez upgrade.

### 1.2 `.gtc` — o CACHE em disco (textures/cache/)

Escrito/lido por `TextureCache` (magic `GTC`, versão 2):

| Offset | Campo |
|---|---|
| 0..3 | magic `G`,`V`,`T`,`C` |
| 4 | `versão = 2` (u8; a v1 pré-5A continua a abrir) |
| 5 | `CompressedFormat` (u8) |
| 6 | flags (u8, v2 — os MESMOS bits do §3) |
| 7 | reservado (0) |
| 8..11 | `width u32` |
| 12..15 | `height u32` |
| 16..19 | `mipCount u32` |
| 20..27 | hash de origem `u64` (eco — valida a chave) |
| 28.. | `mipCount` × (`width u32`, `height u32`, `offset u32`, `size u32`) |
| ... | blob contíguo dos mips |

A CHAVE do cache = FNV-1a 64 dos BYTES do PNG de origem
(`TextureCache::hashBytes`) + um SUFFIX de perfil:
`cache_<hash16>_p<perfil>_o<override>_n<normal>_a<astc>.gtc` em
`textures/cache/` (`TextureCache::fileNameForKey` — o suffix vem de
`TexturePipeline::cacheSuffixFor`). O p é o perfil do projeto, o o é o
override do asset (255 = sem override, 0 = «nunca reduzir»), o n marca o
caminho normal-map e o a a disponibilidade de ASTC. A MESMA textura nos
3 perfis = TRÊS ficheiros (três tamanhos — o pin do dono). PNG alterado →
hash diferente → re-comprime (a entrada antiga fica órfã até limpeza
manual — barato e nunca falso-hit).

## 2. Os formatos de compressão (`CompressedFormat`)

| Valor | Formato | B/px | Notas |
|---|---|---|---|
| 0 | `RGBA8` | 4 | sem compressão; upload `glTexImage2D` + `glGenerateMipmap` na GPU |
| 1 | `ETC2_RGB` | 0.5 | `GL_COMPRESSED_RGB8_ETC2` (opaco) |
| 2 | `ETC2_RGBA` | 1.0 | `GL_COMPRESSED_RGBA8_ETC2_EAC` (com alpha) |
| 3 | `ASTC_4x4` | 1.0 | `GL_COMPRESSED_RGBA_ASTC_4x4_KHR` — o PADRÃO do 5A |
| 4 | `ASTC_6x6` | ~0.44 | `GL_COMPRESSED_RGBA_ASTC_6x6_KHR` (existe; os perfis usam 4x4) |

## 3. As flags do payload v2 (TexturePolicy.h)

| Bit | Nome | Significado |
|---|---|---|
| 0 | `kTexFlagSrgb` | conteúdo de COR: os mips foram filtrados em ESPAÇO LINEAR (§5); a decisão do sampler sRGB fica para a fase de render do dono (mudaria o aspeto de TODA a cena — fora do scope do 5A) |
| 1 | `kTexFlagNormal` | normal map: RGBA8 SEM perda de canais (§6), filtrado em bytes (dados lineares) |

Viajam no `.gtext` v2 E no `.gtc` v2 (o hit devolve os MESMOS bits).

## 4. Os perfis e a política de REDUÇÃO (`TexturePolicy`)

A redução e a compressão são DUAS OPERAÇÕES DISTINTAS, logadas distintas
(§7). A política de redução vive em `texProfileCapMaxDim`
(`TexturePolicy.cpp`) — pura e determinística, classe = a MAIOR dimensão:

| Classe | Qualidade (0) | Equilibrado (1) | Mobile (2) |
|---|---|---|---|
| ≤ 1024 | mantém | mantém | mantém |
| 2048 | mantém | → 1024 | → 1024 |
| 4096 | mantém | → 2048 | → 1024 |
| ≥ 5120 | mantém | → 2048 | → 1024 |

- O default do projeto é `Qualidade` (= o comportamento pré-5A: NADA
  muda sem decisão do dono). Escolha por projeto: `texPerfil=0/1/2` em
  `settings.goni` (lido por `loadProjectSettings` no main.cpp).
- **O override POR ASSET VENCE o perfil**: `textures/overrides.goni`, uma
  linha `<stem> <maxDim>` (o stem é o nome do .gtext sem pasta/extensão,
  ex. `cidade_3`); maxDim 0 = NUNCA reduzir este asset. Lido no arranque
  por `loadProjectSettings` → `TexturePipeline::setOverride`. Única
  exceção: o teto FÍSICO do device (§7) — física não é gosto.
- A redução halva do ORIGINAL decodificado (fator 2, média box 2×2 — em
  espaço linear para cor, `MipGen::halveImageRGBA`); NUNCA de um blob
  comprimido e NUNCA do cache.

## 5. Os compressores (`TextureCompressor` e filhos)

- `PassthroughCompressor` — identidade RGBA (1 nível; mips na GPU). O
  caminho dos normal maps do 5A (a prova «sem perda» é o byte a byte) e
  das texturas < 256px.
- `Etc2Compressor` — ETC2 via `vendor/etcpak`; escolhe RGB8/RGBA8_EAC
  sozinho; cadeia de mips COMPLETA em CPU. `heuristics=false` restringe
  os blocos (decodificáveis pelo decoder dos testes do CI).
- `AstcCompressor` — ASTC LDR via `vendor/astc-encoder`; bloco 4x4
  (default) ou 6x6, RGBA sempre.
- `HardwareCompressor` — a FACADE de seleção: ASTC se
  `setAstcSupported(true)` (o device injeta `glAstcSupported()`), senão
  ETC2; `lastFormat()` diz a última decisão; `astcAvailable()` alimenta a
  chave do cache.

**Gate de tamanho** (`TextureCompressor::canCompress` /
`Etc2Compressor::compressible`): ambas as dims ≥ 256 px (em sprites
pequenos o overhead de bloco supera o ganho) — abaixo disso → RGBA8.

O parâmetro `mipLinear` do `compress` escolhe o ESPAÇO dos mips: false =
bytes (o pré-5A byte a byte — quem chama sem saber não muda), true =
luz (`MipGen::srgbToLinear` → média → `MipGen::linearToSrgbByte`; a prova
do 50%: o mip de um tabuleiro preto/branco fica ~188, não 127).

## 6. A orquestração (`TexturePipeline::process`)

PNG bytes → hash → decisão ANTES do trabalho (`pngDims` do PngLoader lê o
header sem decodificar; o override vence o perfil; o teto do device entra
por último) → **cache** (chave §1.2; hit devolve o produto FINAL com as
flags e o `reduced` derivado das dims) | miss → decode (`loadPng`) →
REDUÇÃO (§4; loga a linha da redução) → COMPRESSÃO (normal map →
`PassthroughCompressor` RGBA8 sem perda; o resto →
`HardwareCompressor`: ASTC 4x4 se a extensão existe, senão ETC2; loga a
linha do formato) → flags (§3) → store no cache.

**Teto do device**: `TexturePipeline::setMaxTextureSize` recebe o
GL_MAX_TEXTURE_SIZE do boot (main.cpp, boot 4/6). Textura maior = halva
do original até caber, com a linha «(teto do device N)» — NUNCA erro,
NUNCA crash (o mosaico ficou como alternativa não escolhida, decisão do
relatório 5A).

**Codificação PARALELA fora da UI**: o passe de texturas do conversor
(`AssetConverter.cpp`) valida/coleta os jobs em sequência (os avisos de
sempre, R-020/R-022 intocados), comprime num pool de até 4 workers
(`std::thread`; o pico de RAM é ~1 decodificado por worker) com progresso
no log («texturas: N/M codificadas») e escreve os .gtext SEQUENCIALMENTE
em ordem. O cache é serializado por mutex dentro do pipeline.

## 7. As linhas no engine.log (o dono sabe sempre o que aconteceu)

| Linha | Quando |
|---|---|
| `textura: perfil <Nome> (texPerfil=N) — K override(s) por asset (textures/overrides.goni)` | arranque do projeto (loadProjectSettings) |
| `textura: reduzida <A>→<B> (perfil <Nome>) — <ficheiro>` | a resolução desceu pela TABELA |
| `textura: reduzida <A>→<B> (override do asset) — <ficheiro>` | a resolução desceu pelo OVERRIDE |
| `textura: reduzida <A>→<B> (teto do device) — <ficheiro>` | a resolução desceu pelo GL_MAX_TEXTURE_SIZE |
| `textura: comprimida <formato> (<N> KB) — <ficheiro>` | ASTC 4x4 / ETC2 EAC / ETC2 RGB |
| `textura: RGBA8 <N> KB (normal map — sem perda de canais) — <ficheiro>` | o caminho normal map |
| `textura: RGBA8 <N> KB (sem compressão — pequena) — <ficheiro>` | < 256px ou fallback sem hardware |
| `textura: reutilizada do cache (perfil <Nome>) — <ficheiro>` | hit (o produto é o MESMO da 1ª vez) |

## 8. As texturas do import glTF (`GltfTextures` + o passe do conversor)

- `extractGltfTextures`: os PNG embutidos do glTF → hash FNV-1a dos
  bytes → `textures/gltf_<hash16>.png` (mesmos bytes → MESMO ficheiro —
  dedup natural entre materiais/meshes/modelos; só escreve se ainda não
  existe). O ORIGINAL FICA no projeto — o pipeline nunca o substitui e
  NUNCA recomprime de comprimido (a entrada é sempre o PNG de origem).
- `GltfMaterial::normalTex` (novo no 5A): o índice da IMAGEM do
  `normalTexture` do glTF — o passe de texturas do conversor marca essas
  imagens e elas seguem o caminho RGBA8 sem perda de canais.
- No conversor (`AssetConverter.cpp`, passe de TEXTURAS): cada imagem
  vira `assets/<stem>[_i].gtext` pelo `TexturePipeline`; FALHA PARCIAL
  NÃO ESCONDE O MODELO (uma textura má = aviso `texWarn` + o mesh entra
  com material por defeito). `convert::Stats` conta `texReduced` e
  `texNormal` SEPARADAMENTE (as duas operações).
- Upload GPU: `GpuAssets.cpp` lê `.gtext` (`readGText`) e submete com
  `glCompressedTexImage2D` por nível — zero re-compressão por arranque.

## 9. Tetos e suposições (hoje)

| Teto | Onde | Nota |
|---|---|---|
| 16384 px | `readGText` | dims acima recusadas com erro legível |
| 32 mips | `readGText` | — |
| 64 MB PNG | `kMaxImageBytes` (conversor) | o decode é inteiro em RAM |
| 4 B/px RGBA | `RawImage` | o decode intermédio é sempre RGBA8 |
| 4 workers | passe de texturas (`AssetConverter.cpp`) | o pico de RAM é ~1 textura decodificada por worker |
| sRGB na GPU | — | a flag VIAJA no formato (§3); o sampler sRGB (GL_SRGB8_*) é decisão do dono — mudaria o aspeto de toda a cena |
| renormalização de normais | — | os mips de normais são média em bytes SEM renormalizar (decisão de render do dono) |
