# RELATORIO-0.11-M — FASE 2 · GLTF MATERIAIS (o glTF completo)

> A spec do dono (os 7 itens da FASE 2): KHR_texture_transform ·
> alphaMode MASK/BLEND + alphaCutoff · doubleSided · samplers (wrap/
> filter) · thumbs de slot · u32 índices (a verificação da spec) · os
> QUATRO factors.
> ACEITAÇÃO: «15+ texturas + alpha blend + texture_transform mostra tudo
> corretamente». Fase fechada com CI verde + relatório; a campanha PARA
> (a FASE 3 só no CONTINUAR).

## 1. O ÂMBITO (o que a fase pede e o que a auditoria achou)

A auditoria do pipeline de materiais (o estado d8c99ba, FASE 1 selada):

- **parse** (`GltfImporter`): o material só trazia baseColorFactor + os
  5 índices de IMAGEM dos slots (baseColor/normal/metalRough/emissive/
  occlusion — o HOTFIX-2). NADA de alphaMode/alphaCutoff/doubleSided,
  NADA de metallic/roughness/emissive factors, NADA de samplers (o array
  `samplers` do glTF nem se lia), NADA de `KHR_texture_transform`.
- **binds** (o GTB1): os caminhos dos 5 slots por material no .gmesh —
  o leitor de nomes para em materialCount e o leitor de binds tolera
  bytes a mais depois do CRC (o contrato «a cauda pode trazer mais»
  documentado no formato) — é a porta de entrada do GTB2.
- **render** (o lit): SÓ o baseColor (unit 0), alpha forçado a 1.0, sem
  transform UV, sem discard, sem blend, culling fixo. O material não
  tinha NENHUMA propriedade além da textura + uTint.
- **sampler** (`Texture`): par FIXO REPEAT/LINEAR_MIPMAP_LINEAR/LINEAR —
  o wrap/filter do glTF morria no parse.
- **inspector**: as 5 linhas de slot como `ui.button` com o sufixo
  honesto «(vinculado · render na 0.11-L)» — sem thumbnails.
- **u32**: o parse aceita u8/u16/u32 (componentType 5125) e o v3 tem o
  indexType u32 por bloco — a verificação da spec é PROVAR com pins (nada a
  migrar: o formato já transporta).
- **o gap do mesh único**: um .gmesh pequeno FUNDIDO com vários materiais
  desenhava TUDO com a textura do material 0 (o render por material só
  existia no caminho por blocos). A FASE 2 torna-o critério de abertura.

## 2. AS PERNAS (o que foi feito, ficheiro a ficheiro)

### Perna 1 — O parse completo (`assets/GltfImporter.h/.cpp`)

- `GltfMaterial` ganha: `alphaMode` (0=OPAQUE/1=MASK/2=BLEND — os valores
  glTF; valor desconhecido = OPAQUE + warn honesto), `alphaCutoff`
  (default 0.5, só lido com MASK), `doubleSided`, `metallic`/
  `roughness` (os factors do pbr, default 1), `emissive[3]` (default 0)
  e os 5 arrays POR SLOT (`texOffset[5][2]`/`texRotation[5]`/
  `texScale[5][2]` com scale=1 por omissão/`texSampler[5]`) alinhados ao
  contrato `kBind*` — os campos i32 antigos ficam byte a byte.
- `GltfSampler` (os enums RAW do glTF — o transporte honesto; o
  mapeamento para GL é do device) + `GltfModel::samplers`.
- O parse lê: o array `samplers` · o `sampler` de cada textura (índice
  range-checked) · o `KHR_texture_transform` de CADA textureInfo (a
  extensão vive NO textureInfo — offset 2f/rotation f/scale 2f) · os
  factors e as props do material.

### Perna 2 — O bloco GTB2 (`assets/GOwnFormats.h/.cpp` + `docs/GMESH_formato.md`)

- `GMeshMatProps` (por material: as props + o transform/sampler por
  slot; `isDefault()`/`slotDefault(s)` puras) + os códigos `kSampler*`
  da casa (os enums RAW não cabem num u8) + a função pura
  `gltfSamplerToCodes` (o enum desconhecido cai no default e devolve
  false — o chamador loga UMA vez).
- ESCRITOR `gmeshV3AppendProps` (magic `GTB2`, u32 propCount — só
  materiais não-default —, entradas FIXAS de 164 B, crc32) appendado
  DEPOIS do GTB1 pela `gmeshV3Skeleton` (a fonte partilhada dos DOIS
  escritores: `writeGMesh` e o conversor streaming).
- LEITOR: o `gmeshV3ReadBinds` ganha o out-param OPCIONAL `consumed`
  (o fim do GTB1); o `gmeshV3ReadProps` arranca daí; o
  `gmeshV3ReadMaterialsOnly`/`readGMeshV3Meta` encadeiam com out-params
  opcionais (os chamadores antigos byte a byte). CRC mau = erro legível.
- O formato documentado no `GMESH_formato.md` (§As propriedades por
  material — o gate gmesh-docs valida os símbolos).

### Perna 3 — O conversor alimenta o GTB2 nos DOIS escritores (`assets/AssetConverter.cpp/.h` + `assets/GmeshV3Stream.cpp/.h`)

- `convert::MatPropsMap` (material NOME → props; o MESMO contrato do
  MatTexMap: 1.º com o nome vence; os 100% default NEM ENTRAM).
- `convertGltfCommon` constrói o mapa do parse (com o warn do sampler
  desconhecido); os DOIS caminhos escrevem: o FUNDIDO (o `writeGMesh`
  com as props na MESMA ordem dedup dos binds) e o STREAMING
  (`convertGltfToV3`/`convertGltfToV3Nodes` com o novo parâmetro
  `matProps` — as peças do import expandido levam as props do SEU
  material por NOME na tabela de cada peça).

### Perna 4 — O render com props (`render/Material.h/.cpp` + `render/Renderer.cpp/.h` + `render/BlockMesh.h/.cpp`)

- `DrawProps` (defaults = o comportamento de SEMPRE) +
  `DrawProps::fromGMesh` (o SLOT 0/baseColor → as uniforms; os factores
  metallic/roughness e os transforms dos slots 1-4 esperam o lit PBR da
  FASE 5 — o GTB2 já os transporta) + as funções PURAS
  `khrTextureTransformApply`/`khrTextureTransformMat2` (a fórmula
  OFICIAL da extensão: uv' = rotate(uv·scale, rotation) + offset).
- O lit ganha uBaseColor (vec4 multiplicativo), uAlphaMode/uAlphaCutoff
  (MASK = discard abaixo do cutoff; BLEND = alpha no outColor),
  uTwoSided (o verso com a normal invertida via gl_FrontFacing),
  uEmissive (self-glow — a luz PBR GGX completa é a FASE 5) e
  uUvRot mat2/uUvOff vec2 (o transform UV no VERTEX shader).
- `Renderer::drawMesh` ganha o parâmetro OPCIONAL `props` (nullptr =
  byte a byte): o estado GL segue as props e é RESTAURADO no draw —
  BLEND liga GL_BLEND + SRC_ALPHA/ONE_MINUS_SRC_ALPHA + depthMask FALSE;
  two-sided desliga GL_CULL_FACE. As props setam ANTES do tex/tint/skin
  (o ÚLTIMO 1f/3f/1i do draw continua a ser uHasTex/uTint/uSkin — o
  contrato dos pins do glstub fica byte a byte).
- `BlockMesh::draw` em DOIS passes: a 1.ª passada faz o cull + o lazy
  load + a partição (o alphaMode do GTB2 por bloco — nullptr = OPAQUE);
  os OPACOS desenham na ordem da TABELA (o caminho de sempre); os BLEND
  desenham DEPOIS, longe→perto pelo z de NDC do centro do bloco
  (`BlockMesh::blendDrawOrder` puro — insertion sort ESTÁVEL; o bloco
  atrás da câmara (w≤0) desenha primeiro, o «mais longe»). As props de
  cada draw saem do `propsOf(materialIndex)` (nullptr = defaults).

### Perna 5 — Os samplers no upload (`render/Texture.h/.cpp` + `render/GpuAssets.cpp/.h`)

- `Texture::setSampler` (os códigos da casa → glTexParameteri; o
  PRIMEIRO vence por textura — a disciplina documentada).
- `GpuAssets::textureWithSampler` (resolve + aplica; first-wins POR
  CAMINHO com o mapa `samplerApplied_` — um material seguinte com
  sampler DIFERENTE para a MESMA textura é logado e ignorado: a
  duplicação por sampler é memória a dobrar no device). O blockHull
  aplica o sampler do slot baseColor do GTB2 a cada bind (1 linha de
  log por sampler — «textura: sampler 'x' wrap=a/b min=c mag=d (glTF)»).
- `releaseAll` limpa o mapa (os samplers morrem com as texturas).

### Perna 6 — O critério do runtime (o gap do mesh único) (`render/GpuAssets.cpp`)

- `GpuAssets::mesh` abre POR BLOCOS também quando: `materialCount > 1`
  (o render POR MATERIAL — o modelo pequeno fundido desenhava tudo com
  a textura do material 0) OU o GTB2 tem props não-default (a sonda
  `gmeshPropsPresent` lê a cauda de materiais POR FAIXA — nunca os
  dados). O modelo pequeno de 1 material SEM props fica no mesh único
  byte a byte (o cubo/prim/legado intocados).

### Perna 7 — As thumbs de slot (`ui/UiEditor.cpp` + `ui/UiContext.h`)

- A linha de slot com ref desenha o quad 24dp da textura VIVA no lado
  direito (o padrão MatSelRow: desenho por cima do botão) via o MESMO
  resolver do elemento Image (`imageQuad`); o orçamento do texto cede
  os 30dp no pré-pass (o MESMO padrão do sufixo honesto — o nome
  trunca, o thumb nunca fica sob o texto); falha do resolver = moldura
  honesta (nunca o vazio silencioso). O tap continua a ser a linha
  INTEIRA (o browser abre como sempre).
- `kMaxImageBatches` 4 → 8 (o pior caso do inspector: 5 slots de UM
  material com texturas distintas + o resto da UI).

### Perna 8 — O u32 (a verificação da spec)

- O parse u32 (componentType 5125) e o bloco use32 do v3 JÁ existiam —
  a fase PROVA com pins: o accessor u32 parse+converte com a geometria
  exata e o bloco indexType=1 materializa POR FAIXA (o caminho do
  runtime por blocos). Nada a migrar: o formato já transporta (a
  decisão fica documentada).

## 3. AS PROVAS (10 pins novos — tests/test_fase2_materiais.cpp)

1. `fase2_parse_props_alpha_e_factors` — MASK 0.25/BLEND/doubleSided +
   os QUATRO factors + os defaults glTF do material sem props.
2. `fase2_parse_transform_e_sampler_por_slot` — o transform no
   baseColor E no normal (offset/rot/scale independentes) + os samplers
   RAW + o índice por slot (os slots sem textura ficam -1).
3. `fase2_sampler_codes_puro` — REPEAT/CLAMP/MIRRORED + os 7 filtros +
   o sampler sem filtros + o enum desconhecido (false + default) + o
   MIP filter no MAG (desconhecido — o glTF não o define aí).
4. `fase2_gtb2_roundtrip_no_import` — o import REAL (2 materiais):
   binds 2×5 slots INTACTOS + o GTB2 com o material BLEND (alpha/
   factor/transform/sampler por slot) e o default FORA.
5. `fase2_gtb2_retrocompat_e_crc` — (a) o pré-FASE 2 lê binds cheios +
   props VAZIAS; (b) o GTB2 sem GTB1 funciona; (c) o CRC podre = erro
   legível; (d) o material 100% default NEM ENTRA (o ficheiro fica byte
   a byte — tamanho IGUAL ao do writer antigo); (e) o append/read puro
   do bloco.
6. `fase2_uv_transform_puro` — identidade/offset+scale/rot 90° + O PIN
   DO SHADER: as colunas do mat2 produzem O MESMO valor da fórmula.
7. `fase2_drawprops_from_gmesh` — o GTB2 → DrawProps (o slot baseColor)
   + os defaults do nullptr.
8. `fase2_blend_order_puro` — z descendente (longe→perto), ESTÁVEL nos
   empates, 1/0 blocos, o atrás-da-câmara desenha primeiro.
9. `fase2_u32_indices_import` — o accessor u32 parse+converte (a
   geometria exata) + o bloco use32 materializa POR FAIXA.
10. `fase2_aceitacao_20_texturas` — A ACEITAÇÃO: 4 materiais × 5 slots
    = 20 texturas DISTINTAS importam (stats.textures=20), os 20 slots
    vinculam, os 20 ficheiros existem no projeto, as props de 3
    materiais chegam ao GTB2 (BLEND+transform / MASK 0.25+emissive /
    factors PBR) e o 4.º (100% default) fica FORA.

### A FASE 25 do c33 virtual (+27 checks, 797→824)

O GLB dos materiais (vidro BLEND com factor+transform+sampler CLAMP/
NEAREST; folha MASK 0.25 two-sided com sampler MIRRORED) importado,
aplicado e DESENHADO no caminho REAL do device (o glstub):

- 25.1 o import (2 texturas, o dedup honesto) + O PIN: o GTB2 no
  runtime (`bm->matProps().size()==2`; o vidro BLEND com o alpha 0.8; a
  folha MASK 0.25 two-sided) + os samplers no LOG (wrap=1/1 min=1
  mag=1 e wrap=2/0 min=3 mag=2).
- 25.2 o draw com props: 1 glBlendFunc (SÓ o BLEND mistura) com o par
  SRC_ALPHA/ONE_MINUS_SRC_ALPHA + o depthMask FALSE→TRUE + uBaseColor
  definido em AMBOS os draws com o valor (0.5,0.4,0.3,0.8) + uUvRot =
  rot90·scale(2,3) column-major (0,2,−3,0 — a MESMA matriz do pin puro)
  + uUvOff (0.25,0.5) + O PIN: 1 glDisable(GL_CULL_FACE) (só o
  two-sided) com o cull RESTAURADO.
- 25.3 os samplers no GL: 4 params × 2 texturas no glTexParameteri (o
  releaseAll + re-resolução reaplica) e o ÚLTIMO é o mag LINEAR da
  folha (o sampler 1 do glTF).

## 4. AS DECISÕES E OS LIMITES (nenhum escondido)

- **O lit PBR GGX completo é a FASE 5** (luzes). A FASE 2 aplica o que
  NÃO precisa de luz: baseColorFactor (multiplicativo), alphaMode
  (MASK discard / BLEND mix), doubleSided (verso), emissiveFactor
  (self-glow) e o transform UV. Os factors metallic/roughness e os
  transforms/samplers dos slots 1-4 VIAJAM no GTB2 (o round-trip é
  provado) e esperam o lit da FASE 5 — a fronteira está no sufixo
  honesto das linhas de slot («render na 0.11-L»).
- **O transform UV aplica-se ao slot baseColor** (o único que o lit de
  hoje amostra) — os outros 4 vão no GTB2 e aplicam-se quando os slots
  renderizarem (FASE 5).
- **Sampler first-wins POR FICHEIRO**: texturas partilhadas por
  materiais com samplers DIFERENTES ficam com o PRIMEIRO (a alternativa
  — duplicar a textura por sampler — é VRAM a dobrar no C33; o conflito
  é logado honesto, warn UMA vez por caminho).
- **O GTB2 perde-se em builds antigas** (o MESMO contrato do GTB1): um
  .gmesh novo aberto numa APK pré-FASE 2 perde SÓ as props — os
  defaults honestos entram, o mesh/texturas ficam. Reimportar restaura.
- **O mesh pequeno multi-material/props abre POR BLOCOS** — o caminho
  onde o render por material + props existe. Custo: a tabela por blocos
  (192 B + 80 B/entrada) e o LRU — para os modelos pequenos é 1-2
  blocos, o overhead é mínimo; o ganho é o render CORRETO por material
  (o comportamento anterior — 1 textura para tudo — estava errado).
- **O blend é o clássico SRC_ALPHA/ONE_MINUS com depthMask FALSE e a
  ordenação POR BLOCO** (o centro do AABB em z de NDC) — a ordenação
  por TRIÂNGULO dentro do bloco não existe (o custo por índice no
  device é proibitivo; a fronteira documentada — casos patológicos
  dentro de UM bloco podem misturar na ordem interna).
- **A sub-árvore SEM undo**: as props vêm do ASSET (o GTB2), não da
  cena — o serializer/undo do TIC não as toca (o reparent/clone mantêm
  o meshPath e as props seguem o asset; editar props por TIC é fase
  futura se o dono quiser — hoje a edição é reimportar).

## 5. OS GATES (a evidência do harness local)

- **test_core 0 falhas** (a suíte INTEIRA — 196 TUs, 960+ TESTs, os 10
  pins novos VERDES; a sentinela r039 = o flake de DISCO da sandbox
  documentado desde o hotfix P0 — a sandbox não cabe nos ~9 GB do
  perfil de 50M verts, o CI é o árbitro; o diff não toca na escrita do
  convert).
- **c33_virtual 824/824 checks, HARNESS VERDE** (a FASE 25 nova: +27).
- scope VERDE (todo o diff dentro de ci/scope.txt) · check_main OK ·
  link_parity OK (122 TUs) · docs-lint 83 · gmesh-docs 25 (o §GTB2
  entra no contrato) · ui-vocab · theme-hex · glyph · hierarchy-check ·
  projects-ui · release-identity versionCode 70 (o par declarado no
  §14 do RELATORIO-0.9.6).

## 6. NÃO VERIFICADO (honesto)

- **O sign-off no C33 físico é do dono**: os 3 itens — (1) um modelo
  com 15+ texturas + alpha blend + texture_transform a mostrar TUDO
  correto no device; (2) o BLEND a misturar com o fundo (o vidro à
  frente da cena) e o MASK a cortar nas folhas; (3) os samplers CLAMP/
  NEAREST a matar o sangramento nas bordas. O c33 virtual prova o
  caminho com o glstub; o pixel real é do Mali.
- **A ordenação do BLEND com o TIC transformado** — o pin puro cobre a
  ordem pelo z; a câmara do device a orbitar um modelo com MÚLTIPLOS
  blocos BLEND é a prova visual do dono.
- **Os números de frame-time** — a FASE 2 não media performance (o
  blend/two-sided por draw é estado GL barato; o ordena-por-bloco é
  O(n log n) sobre dezenas); se o dono quiser a tabela, é um passo de
  medições próprio.

## 7. FICHEIROS ALTERADOS

- `app/src/main/cpp/assets/GltfImporter.h/.cpp` — o parse completo
- `app/src/main/cpp/assets/GOwnFormats.h/.cpp` — o GTB2 + os códigos
- `app/src/main/cpp/assets/AssetConverter.h/.cpp` — o MatPropsMap + os dois escritores
- `app/src/main/cpp/assets/GmeshV3Stream.h/.cpp` — o matProps do streaming/peças
- `app/src/main/cpp/render/Material.h/.cpp` — DrawProps + as uniforms + o shader
- `app/src/main/cpp/render/Renderer.h/.cpp` — o drawMesh com props + o estado GL
- `app/src/main/cpp/render/BlockMesh.h/.cpp` — os dois passes + o propsOf
- `app/src/main/cpp/render/Texture.h/.cpp` — o setSampler
- `app/src/main/cpp/render/GpuAssets.h/.cpp` — o textureWithSampler + o critério
- `app/src/main/cpp/ui/UiEditor.cpp` + `ui/UiContext.h` — as thumbs + o cap 8
- `tests/test_fase2_materiais.cpp` NOVO · `tests/c33_virtual.cpp` (FASE 25) ·
  `tests/stub/GLES3/gl3.h` (o tracking das uniforms/estado) · `tests/CMakeLists.txt`
- `docs/GMESH_formato.md` (o §GTB2) · `docs/RELATORIO-0.11-M-MATERIAIS.md` (este) ·
  `docs/RELATORIO-0.9.6.md` (§14, vc 70) · `BACKLOG.md` · `README.md` ·
  `app/build.gradle` (69→70)
