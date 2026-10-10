# RELATÓRIO 0.10-M · PASSO 5A — TEXTURAS: compressão E redução SEPARADAS, três perfis, override por asset

> Sub-passo 5A do PASSO 5 (pipeline de assets final) — spec do dono.
> Scope respeitado: NENHUM toque na UI, seleção, física ou V.ONI.
> versionCode **62** · baselines: test_core **925/0** local + os gates todos
> verdes antes do push (o CI é quem decide o fecho — linha colada no fim
> pelo commit -docs).

## 1. O que o dono pediu

«Duas operações distintas, ambas logadas: compressão (ASTC 4x4 padrão,
ETC2 fallback) e redução de resolução (downscale). Nunca confundir no
código nem nos logs.» Política de redução por resolução (512/1024 mantém ·
2048 avalia→1K em perfis limitados · 4096→2K ou 1K conforme perfil ·
5120+ conforme perfil), três perfis (Qualidade/Equilibrado/Mobile) com
override por asset a vencer, avisos distintos no log, original guardado,
nunca recomprimir de comprimido, mips com filtro em espaço linear + flag
sRGB, normais sem perda de canais, GL_MAX_TEXTURE_SIZE → mip mais baixo
com aviso (nunca erro), codificação paralela fora da UI com progresso.

## 2. As causas e as decisões (ficheiro + função)

**A política vive num módulo NOVO e SÓ nele** — `assets/TexturePolicy.h`
/ `.cpp`:
- `texProfileCapMaxDim(w,h,perfil)` — a tabela (§4 do GTEX_formato.md):
  Qualidade nunca reduz; Equilibrado 2048→1K e 4096+/5120+→2K; Mobile
  tudo o que passa de 1024→1K. Leitura da spec do dono: «2048 avalia→1K
  em perfis limitados» aplica-se aos DOIS perfis limitados (Equilibrado e
  Mobile); «4096→2K ou 1K conforme perfil» dá o 2K ao Equilibrado e o 1K
  ao Mobile; «5120+ conforme perfil» segue o mesmo par. A seletividade do
  Equilibrado = só acima de 1024; a agressividade do Mobile = dois
  passos de halving a partir de 4096. **Crédito à revisão externa
  (10-09)**: a leitura «classe pela MAIOR dimensão + halving do original
  em passos de 2» (em vez de reescala arbitrária) e a regra «o teto
  físico do device vence até ao override "nunca reduzir"» vêm das notas
  da revisão que acompanhou a política de resolução.
- `texPerfilName`/`texPerfilFromInt` — os nomes EXATOS do log
  («Qualidade»/«Equilibrado»/«Mobile») e o parse do `texPerfil=0/1/2`
  (valor podre → Qualidade).
- `TexJob{assetKey, normalMap}` — o que varia POR ASSET.
- `kTexFlagSrgb`/`kTexFlagNormal` — os bits que viajam no formato.

**A orquestração** — `assets/TexturePipeline.h`/`.cpp`, `process()`:
- A decisão ANTES do trabalho: `pngDims` (novo, `assets/PngLoader.cpp`,
  via `stbi_info_from_memory`) lê as dims do header SEM decodificar — o
  cap do perfil/override/device fica conhecido antes do decode, e a
  CHAVE do cache pode incluir o perfil (`cacheSuffixFor`:
  `_p<perfil>_o<override>_n<normal>_a<astc>`).
- REDUÇÃO (`halveUntilCap`): halva do ORIGINAL decodificado com
  `MipGen::halveImageRGBA` no espaço certo; NUNCA de blob comprimido.
- COMPRESSÃO: normal map → `PassthroughCompressor` (RGBA8); resto →
  `HardwareCompressor` (ASTC 4x4 → ETC2; <256px → RGBA8 como sempre).
- As DUAS linhas de log (uma fonte, `logReduced`/`logFormat`):
  «textura: reduzida A→B (perfil X | override do asset | teto do
  device)» e «textura: comprimida ASTC 4x4 (N KB)» / «textura: RGBA8 N
  KB (normal map — sem perda de canais)» / «textura: reutilizada do
  cache (perfil X)». KB = blob/1024.
- O teto do device: `setMaxTextureSize` (o main injeta o
  GL_MAX_TEXTURE_SIZE no boot 4/6, `platform/main.cpp`) — excedeu =
  halva com a linha «(teto do device)», NUNCA erro (o pin «gigante não
  crasha»). O mosaico (alternativa da spec) NÃO foi escolhido: o mip
  mais baixo dá o mesmo resultado sem partir o UV/dados — decisão
  registrada (o dono pode reabrir).
- O hit do cache devolve o produto FINAL e os bits (`TextureCache` v2):
  `info.reduced` no hit deriva das dims do header vs as dims do blob —
  o dono continua a saber o que aconteceu mesmo reutilizando.

**O espaço de cor dos mips** — `assets/MipGen.h`/`.cpp`: `MipSpace::
SrgbLinear` (decode `srgbToLinear` → média → encode `linearToSrgbByte`
— LUT de 256 no decode, fórmula oficial no encode). O `compress` dos
compressores ganhou o parâmetro `mipLinear` (default **false** = o
byte a byte do pré-5A — o bench e os testes antigos não mudam um
byte); o pipeline passa `true` para as texturas de COR e `false` para
normal maps (dados lineares).

**O caminho normal map** — `assets/GltfImporter.h`/`.cpp`:
`GltfMaterial::normalTex` (o índice da IMAGEM do `normalTexture`,
mesmo mapa `textureSources` do baseColor) → o passe de texturas do
conversor marca a imagem → `TexJob::normalMap` → RGBA8 SEM PERDA (a
prova é o byte a byte contra o decode, teste `normal_map_rgba8_sem_
perda_de_canais`). Comprimidos lossy destruem normais (os canais
cruzam-se nos blocos) — por isso a resolução segue o perfil e o
FORMATO é que é sem perda.

**O formato v2** — `assets/GOwnFormats.h`/`.cpp`: `writeGText` grava v2
(payload: format u8 + **flags u8** + dims + mips + blob);
`readGText` abre v1 E v2 (v1 → flags 0, byte a byte como hoje). O
cache `.gtc` também é v2 (`assets/TextureCache.cpp`: o byte 6, que era
reservado, agora carrega as flags — o hit devolve os MESMOS bits) e a
chave ganhou o suffix de perfil (`TextureCache::fileNameForKey` +
`loadKeyed`/`storeKeyed`; a API antiga `load`/`store` fica para quem
não usa perfis). Entradas antigas do cache ficam órfãs (a política da
casa — nunca falso-hit).

**O passe PARALELO** — `assets/AssetConverter.cpp` (convertGltfCommon,
passe de TEXTURAS): valida/coleta os jobs EM SEQUÊNCIA (os avisos
R-020/R-022 mantêm a forma e a ordem), comprime num pool de até 4
workers (`std::thread`; `hardware_concurrency` capado a 4 — o pico de
RAM é ~1 textura decodificada por worker), com progresso no log
(«asset: texturas N em K worker(s)» + «texturas: N/M codificadas»), e
escreve os .gtext SEQUENCIALMENTE em ordem. O `stats.texWarn` de cada
falha continua a contar (o toast «SEM N textura(s)» intacto) e
`Stats::texReduced`/`Stats::texNormal` contam AS DUAS OPERAÇÕES
SEPARADAS (`assets/AssetConverter.h`). O cache é serializado por mutex
dentro do pipeline (o disco é rápido; a compressão é quem paraleliza).

**O wiring do projeto** — `platform/main.cpp`: `texPerfil=0/1/2` em
`settings.goni` (`loadProjectSettings`/`saveProjectSettings`), o
override por asset em `textures/overrides.goni` (uma linha `<stem>
<maxDim>`; 0 = nunca reduzir; lido no arranque com um log do total) e o
boot GL injeta `GL_MAX_TEXTURE_SIZE` + re-aplica o perfil (a ordem
lifecycle é idempotente). `convertPng` (PNG solto) usa o stem como
chave do override e conta no `texReduced`.

## 3. Números medidos (host x86_64, ASTC 4x4, fixture green4096.png —
o teste `tabela_tempo_tamanho_por_perfil` corre no CI e imprime esta tabela)

| Perfil | antes→depois | blob final | tempo (decode+redução+compressão+store) |
|---|---|---|---|
| Qualidade | 4096→4096 px | 21 845 KB | 485 ms |
| Equilibrado | 4096→2048 px | 5 461 KB | 410 ms |
| Mobile | 4096→1024 px | 1 365 KB | 390 ms |

- Os TRÊS tamanhos distintos (o pin): 21 845 > 5 461 > 1 365 KB — a
  MESMA textura, a MESMA compressão ASTC 4x4, só a política muda.
- O cache guarda TRÊS ficheiros distintos (teste
  `perfis_tres_tamanhos_distintos`): `cache_<hash>_p0_o255_n0_a1.gtc`,
  `_p1…`, `_p2…`.
- O Mobile poupa 16,0× de VRAM na textura 4K vs Qualidade.

## 4. Provas de mutação (vermelho→verde; todas no
tests/test_texture_profiles.cpp, baseline 925/0)

- **M-T1 · a política morre** (`TexturePolicy.cpp`,
  `texProfileCapMaxDim` → sempre 0): `perfil_tabela_tetos FALHOU …
  texProfileCapMaxDim(2048, 2048, P::Equilibrado) == 1024u` (+7 linhas)
  e `perfis_tres_tamanhos_distintos`/`tabela_tempo_tamanho` — os três
  tamanhos viram um. Reposto → verde.
- **M-T3 · o normal map é comprimido** (`TexturePipeline.cpp`,
  `compressedPath` ignora `job.normalMap`):
  `normal_map_rgba8_sem_perda_de_canais FALHOU … r.comp.format ==
  CompressedFormat::RGBA8` (+ `r.comp.data.size() == ref.rgba.size()`)
  e `conversor_passe_texturas_paralelo_4_texturas FALHOU … img.format
  == CompressedFormat::RGBA8`. Reposto → verde.
- **M-T5 · o perfil sai da chave do cache** (`TexturePipeline.cpp`,
  `cacheSuffixFor` fixa p=0): `perfis_tres_tamanhos_distintos FALHOU …
  e.comp.width == 2048u` (o Equilibrado recebeu o blob do Qualidade) +
  `cache_por_perfil_entradas_distintas`. Reposto → verde.
- **M-T6 · o override morre** (`TexturePipeline.cpp`, o lookup de
  `overrides_` removido): `override_por_asset_vence_o_perfil FALHOU …
  keep.comp.width == 4096u && keep.comp.height == 4096u` (+3 linhas) —
  o «nunca reduzir» deixou de vencer o Mobile. Reposto → verde.
- **M-T7 · a luz dos mips morre** (`MipGen.cpp`, o ramo SrgbLinear →
  false): `mips_em_espaco_linear FALHOU … ml.rgba[0] >= 186u &&
  ml.rgba[0] <= 190u` (+ `ml.rgba[0] > mb.rgba[0]`) — o mip volta ao
  127 escurecido. Reposto → verde.

## 5. Os pins do dono, um a um

| Pin | Prova |
|---|---|
| mesma textura nos 3 perfis = 3 tamanhos distintos | `perfis_tres_tamanhos_distintos` + a tabela §3 (21 845 / 5 461 / 1 365 KB) |
| override por asset respeitado | `override_por_asset_vence_o_perfil` («nunca reduzir» vence o Mobile; 512 vence o Qualidade) + M-T6 |
| textura gigante não crasha | `gigante_nao_crasha_teto_do_device` (4096 com teto 1024 → 1024, ok, blob válido; o override «nunca» NÃO vence a física) |
| ETC2 verde em device sem ASTC | `etc2_verde_sem_astc_com_perfil` (astcSupported=false → ETC2_RGB 1024, mips 11) |
| mips em espaço linear + flag sRGB | `mips_em_espaco_linear` (188 vs 127 — a prova do 50%) + `gtext_v2_flags_e_v1_retrocompativel` |
| normais sem perda de canais | `normal_map_rgba8_sem_perda_de_canais` (byte a byte vs o decode) + o caminho no conversor |
| never recomprimir de comprimido | a entrada do pipeline é SEMPRE o PNG de origem (§2); o cache devolve o produto final — `cache_por_perfil_entradas_distintas` prova que PNG alterado = entrada nova |
| GL_MAX_TEXTURE_SIZE → mip mais baixo com aviso | `gigante_nao_crasha_teto_do_device` + a linha «(teto do device)» |
| codificação paralela fora da UI com progresso | o pool no passe de texturas (§2) + `conversor_passe_texturas_paralelo_4_texturas` (4 texturas importadas, em ordem, normal incluída) |
| avisos distintos no log | §7 do GTEX_formato.md — uma linha para a RESOLUÇÃO (com a causa), outra para o FORMATO |

## 6. Gates e CI

Locais (antes do push): scope-check VERDE (o diff inteiro dentro de
ci/scope.txt) · docs-lint VERDE · gmesh-docs-check VERDE (GTEX v2 com
21 âncoras) · ui-vocab/theme-hex/hierarchy/jni-parity/gizmo/rel⑦/
glyph/projects VERDES · check_main VERDE · link-parity VERDE (119 TUs)
· test_core 925/0 (suíte local, inclui os 9 TESTs novos do 5A).
O release-identity (R-15) passa com ESTE relatório a declarar
versionCode 62. A linha do CI (run verde) entra no commit -docs.

## 7. NÃO VERIFICADO (honestidade da casa)

- **Device real (C33)**: NENHUMA verificação em hardware nesta sessão —
  o CI corre a suíte do core e o c33 virtual; o APK assinado (vc 62)
  precisa de instalar e converter um modelo COM texturas para ver as
  linhas «textura: reduzida/comprimida» no engine.log do device.
- **O aspeto visual sRGB**: a flag viaja no formato, mas o SAMPLER sRGB
  (GL_SRGB8_* / GL_COMPRESSED_SRGB8_ASTC) NÃO foi ligado — mudaria o
  aspeto de TODA a cena (a decisão é do dono; §9 do GTEX). Os mips em
  luz estão medidos no CPU; o efeito visual no device não foi visto.
- **Renormalização de normais nos mips**: a média em bytes não
  renormaliza (§9 do GTEX) — sem verificação visual.
- **RAM pico do pool no device**: o cap de 4 workers é por construção
  (~1 textura decodificada por worker); o pico REAL não foi medido em
  hardware (o c33 do CI não mede o import com texturas grandes).
- **O mosaico**: alternativa da spec NÃO implementada (decisão §2 —
  mip mais baixo cobre o caso sem partir UVs).

## 8. Decisões novas candidatas (o dono decide)

- (5A-m1) O sampler sRGB na GPU (usar a flag que agora viaja): muda o
  aspeto de toda a cena para o correto-luz — é uma decisão de RENDER
  (fase própria), não do pipeline.
- (5A-m2) O seletor do perfil nas Definições (UI): hoje escolhe-se por
  `settings.goni` (`texPerfil=0/1/2`) — o toque na UI ficou FORA do
  scope desta passagem por ordem do dono.
- (5A-m3) ASTC 6x6 para o perfil Mobile (mais 2× de poupança, menos
  qualidade): a spec disse «ASTC 4x4 padrão» — os três perfis usam 4x4;
  abrir o 6x6 ao Mobile é decisão do dono.
