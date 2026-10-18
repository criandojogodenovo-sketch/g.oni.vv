# RELATÓRIO 0.10-M · PASSO 2 — FORMATO v3 (blocos, sem teto, mmap)

> A spec do dono (PASSO 2): cabeçalho com contagens em 64 bits, AABB e
> offset de 64 bits da tabela de blocos; blocos de até 65 535 vértices
> (configurável) com índices u16 locais (u32 quando preciso); cada
> entrada com offset/tamanho 64-bit, AABB, material e CRC32; layout de
> atributos DESCRITO no cabeçalho; alinhamento a 16 bytes, little-endian,
> sem ponteiros (mmap-ável); offsets 64-bit no código; leitor
> retrocompatível v1 e v2; o escritor só escreve v3. **RETROCOMPAT: os
> .gmesh v1 e v2 existentes continuam a abrir — quebrar isto é vermelho
> por definição.**

## 1. O que entrou (ficheiro+função)

- `assets/GOwnFormats.h` — as constantes do v3 (`kGmeshVersionWrite = 3`,
  `kGmeshVersionMinRead = 1`, `kGmeshV3MetaBytes = 160`,
  `kGmeshV3BlockEntryBytes = 80`, `kGmeshV3BlockVertexCap = 65535`,
  `kGmeshV3MaxAttrs = 8`), as 8 semânticas de atributo
  (`kAttrPosition..kAttrWeights`) e os 3 armazenamentos
  (`kAttrF32/U16/U8`), os tipos `GMeshV3Meta`/`GMeshV3Attr`/
  `GMeshV3Block`/`GMeshV3BlockIn`, `gcrc32`, `readGMeshV3Meta`
  (SÓ header+metadados+tabela — nunca aloca os dados) e
  `readGMeshV3Block` (materializa UM bloco).
- `assets/GOwnFormats.cpp` — o escritor v3 (`writeGMesh` reescrito: o
  corte por grupos em blocos de ≤ cap com a rotina `v3CutGroup` — a
  MESMA que o conversor streaming do PASSO 3 usa por primitiva), o
  leitor v3 (meta + bloco) e o dispatcher `readGMesh` (v1/v2 → payload
  cru dequantizado como sempre; v3 → os blocos montados por ordem da
  tabela).
- `platform/FileApi.{h,cpp}` — `mapFile64`/`unmapFile64` (mmap com
  offsets de 64 bits, a validação honesta da range contra o fstat, o
  errno no log da casa).
- `assets/GltfImporter.cpp` + `assets/AssetConverter.cpp` — as
  MENSAGENS dos 2 guards do merge recalibradas («o teto do caminho de
  mesh única; o formato v3 já não tem teto — o corte em blocos entra no
  PASSO 3») — os guards EM SI ficam (o runtime de hoje ainda desenha
  por mesh única; o render por blocos é o PASSO 4, bloqueado).

## 2. O formato (a spec viva é docs/GMESH_formato.md §v3)

- **Header comum 32 B** inalterado na forma; num v3 o `align` ecoa 16 e
  o **checksum cobre SÓ os 160 B de metadados** (o FNV-1a do payload
  inteiro obrigava a tocar TODOS os bytes antes de usar qualquer um —
  inimigo do mmap/streaming; a integridade do resto vive nos CRC32).
- **Metadados fixos 160 B**: attrCount/flags/blockVertexCap/tableCrc32
  + vertexCount/indexCount/blockCount/materialCount em **u64** + AABB
  global + **offsets ABSOLUTOS 64-bit** da tabela de blocos e de
  materiais + os 8 descritores de atributo.
- **Tabela de blocos**: entradas de 80 B com dataOffset/dataSize
  (ABSOLUTOS, 64-bit), vertexCount/indexCount, AABB do bloco,
  materialIndex, indexType (u16/u32) e **CRC32 do bloco**.
- **Atributos descritos no header** — acrescentar tangente/UV1/cor/bones
  NÃO muda a versão: o leitor anda pelos descritores e salta o que não
  conhece. O stride do vértice é calculado (`vertexStride()`).
- **Alinhamento a 16** nos dados e nas entradas; little-endian; SEM
  ponteiros — o ficheiro é legível por **mmap** (prova: o teste
  `gmeshv3_mmap_de_64_bits_le_o_ficheiro`).
- **A integridade sem ler tudo**: metadados (FNV-1a) → tabela (CRC32) →
  bloco (CRC32 no materializar). Um bloco corrompido não derruba os
  outros.

## 3. As decisões (com a razão)

| Decisão | Razão |
|---|---|
| O checksum do header v3 cobre só os metadados | o FNV do payload inteiro = ler o ficheiro todo por arranque — mata o streaming; os CRC32 por secção dão a integridade ON DEMAND |
| A v2 abre como payload v1 (a interpretação tolerante) | a v2 NUNCA existiu no histórico do repo (a v1 nasceu na 0.8.10-a e o campo version nunca saiu de 1 — verificado com git log -S). A cláusula do dono exige que «v2 existente abra»: define-se v2 = v1-layout e COMITAM-SE as fixtures exatas que abrem (a decisão volta ao dono no sign-off) |
| Pools POR BLOCO (por grupo/primitiva) — vértices partilhados viajam duplicados | a alternativa (pool partilhada global) recria o mesh único por outra via; a duplicação é local ao bloco (≤cap) e o conjunto de TRIÂNGULOS é idêntico — a semântica fica no doc do formato |
| A pool de cada bloco em ordem ASCENDENTE do índice original | o round-trip de um mesh fica IDÊNTICO ao de entrada (bit a bit nos floats, índices iguais) — o teste afixa |
| O corte só em fronteira de triângulo | um triângulo nunca fica partido por dois blocos (o render por blocos do PASSO 4 desenha cada bloco com a sua pool) |
| Vértices órfãos não sobrevivem | não afetam o conjunto de triângulos; documentado no formato e provado por teste |
| O meta lê SEM os dados (offsets >4 GB legais) | o contrato do dono: «contagens acima de 2^32 e offsets acima de 4 GB, lido e escrito sem alocar os dados (só cabeçalho+tabela)» — o fit dos dados valida-se no materializar (readGMeshV3Block) |

## 4. Testes, sentinelas e mutações

`tests/test_gmeshv3.cpp` (NOVO, 8 casos no test_core):

1. `gmeshv3_escrita_v3_e_roundtrip_exato` — o escritor grava version 3 e
   o round-trip é EXATO (pos/normal/uv bit a bit, índices iguais, o
   material sobrevive).
2. `gmeshv3_multi_grupo_blocos_por_material` — 3 grupos/2 materiais → 3
   blocos, os materiais dedup na tabela, a bridge devolve os materiais.
3. `gmeshv3_v1_e_v2_fixtures_reais_abrem` — as fixtures REAIS commitadas
   (`tests/fixtures/gmesh_v1_esfera.gmesh`, gerada COM O ESCRITOR v1 da
   era 0.9.6 antes deste commit; a variante v2) abrem com 221 verts /
   1152 idx / grupo «corpo»/«laca».
4. `gmeshv3_meta_sem_alocar_dados_contagens_grandes` — vertexCount
   5,000,000,000 (>2^32) e dataOffset 5,000,000,000 (>4 GB) lidos de um
   ficheiro de <1 KB (só header+meta+tabela) — SEM alocar os dados.
5. `gmeshv3_corrupcao_apanhada_pelos_crc` — a corrupção dos dados do
   bloco, da tabela e dos metadados é apanhada com «CHECKSUM CORROMPIDO»
   em cada camada.
6. `gmeshv3_versao_desconhecida_rejeitada` — version 9 → erro legível.
7. `gmeshv3_mmap_de_64_bits_le_o_ficheiro` — o ficheiro mapeado com
   `mapFile64` (offset 0 e offset 32) lê-se igual aos bytes em memória.
8. `gmeshv3_orfaos_nao_sobrevivem_ao_corte` — 3 órfãos fora do ficheiro.

**R-038** `regress_gmesh_v3_retrocompat` (`tests/test_sentinels.cpp`) —
a sentinela da cláusula: v1 abre + v2 abre + o escritor grava 3 (o
LITERAL 3 no check, de propósito) + round-trip exato.

**Mutações coladas (vermelho→verde, na sessão):**

- **M-V3a** o escritor a gravar v2 (`kGmeshVersionWrite` 3→2) → a R-038
  FALHOU 2× («o escritor grava version=2 (a cláusula do dono: só v3)» +
  o readGMesh do round-trip) e 25 testes vermelhos no total; reposta →
  **0 falhas**.
- **M-V3b** o leitor a recusar v1 (o dispatch a recusar ver==1) → a
  R-038 FALHOU na fixture v1 REAL («versão 1 recusada (a mutação
  M-V3b)»); reposta → **0 falhas**.

**Recalibrações honestas no MESMO commit** (o contrato mudou por spec do
dono):

- `wiring010_gmesh_roundtrip_e_ratio`: o rácio «OBJ → .gmesh menor» era
  a propriedade da QUANTIZAÇÃO v1; o v3 é float32 SEM PERDA (em meshes
  pequenas é maior que o texto — 9652 B vs 6766 B na esfera de 221
  verts). O teste imprime os dois rácios e a fixture v1 real (5930 B)
  continua a provar o rácio da v1. A promessa do v3 é outra: NENHUM
  erro de limite e NENHUMA perda.
- `warning010_magic_versao_endian_rejeitados`: a versão 2 ABRE agora (a
  cláusula do dono) — a recusa de versões futuras testada com a 9.
- `wiring010` 500 MB: o filho passou a limpar o SEU root FsStorage
  (`rmrf(root)`) — a lição /tmp: 500 MB × N corridas encheram o disco
  desta sandbox a 99% e a falha fantasma do próprio teste era DISCO.
- As mensagens dos 2 guards do merge (GltfImporter/AssetConverter)
  deixam de mentir sobre o «limite u16 do .gmesh» (o formato já não
  tem limite; o teto é o caminho de mesh única, que o PASSO 3 cura).

## 5. Suítes e gates

- **test_core: 0 falhas** (com os 8 casos novos + a R-038 + as
  recalibrações).
- **c33_virtual: 631/631** (o harness do dispositivo virtual inteiro
  verde — a UI e o replay não foram tocados).
- Gates locais verdes: docs-lint (67 ficheiros), gmesh-docs-check (19
  âncoras agora, com as do v3), scope-check, hierarchy-check, ui-vocab,
  theme-hex.
- versionCode 53 → **54** (o código de app mudou) — declarado no §14 do
  RELATORIO-0.9.6.md.

## 6. NÃO VERIFICADO (honesto)

- **O device**: o v3 escrito pelo conversor no C33 não foi aberto por
  outra ferramenta externa (não existe uma no projeto) — a prova de
  interoperabilidade é o gate do formato (os âncoras byte a byte) + os
  testes de round-trip.
- **A v2 do dono**: se os ficheiros v2 REAIS do device tiverem um layout
  DIFERENTE do v1 (a nossa interpretação tolerante), o leitor vai errar
  com mensagem legível — o dono manda uma amostra e o leitor estende-se.
- **O desempenho do FNV/CRC nos ficheiros de 1 GB** (o checksum dos
  metadados é O(160 B); o CRC32 por bloco é O(bloco) — só no
  materializar): o custo de leitura completa dos 1 GB não existe no
  arranque (o requisito do streaming) mas o PASSO 4 medirá.

## 7. Retoma (o registo do worklog)

- Último commit: este (o 2.º do 0.10-M).
- PASSO 3 (conversor streaming): ler por primitiva/range (mmap), nunca o
  modelo inteiro na RAM; agrupar por material e cortar em blocos; pool
  de threads (núcleos-1) + o escritor que ordena; SEM PERDA (float32 da
  origem); a verificação bit a bit com o log `gmesh: v3 blocos=…
  verificado=1`; progresso + cancelar sem bloquear a UI; o teste
  sintético de 50M verts (~1.6 GB) com RAM ≤512 MB; R-039 + a mutação
  do readAll. **PÁRO ABSOLUTO antes do PASSO 4.**
