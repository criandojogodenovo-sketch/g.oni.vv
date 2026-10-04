# RELATÓRIO 0.8.10 — MESH DETERMINÍSTICA (SÓ CUBO E ESFERA) + IMPORT STREAMING 500 MB + FORMATOS PRÓPRIOS (.gmesh/.gtext/.gm) + ARCHIVES + DIAGNÓSTICO COM IDENTIDADE

> Sub-fase 0.8.10 da campanha F8. HEAD anterior: 0.8.9-b (9084f38).
> versionCode 40 · suíte 568 → 581 testes · CLÁUSULA CALMA respeitada
> (só o prompt 0.8.10 + testes; zero física nova, zero V.ONI, zero layout
> novo além do overlay de progresso e dos 2 itens de Settings).

## 1. Objetivo

Quatro mudanças do dono, um release: (1) ficar só com cubo e esfera com
troca determinística SEM cache e deferred free — a intermitência do C33
("cubo quase sempre, esfera/cilindro só às vezes") vivia no caminho
partilhado do cache, não nos geradores; (2) import de 500 MB sem crash —
leitura em chunks, parse incremental, progresso com cancelamento, guarda
de orçamento de memória; (3) conversão para formatos próprios comprimidos
(.gmesh/.gtext/.gm) para ficar MENOR dentro da engine, com o runtime a
carregar só os formatos próprios; (4) EXTRAIR archives (zip) como passo
separado de importar. Mais o reforço transversal: diagnóstico com
identidade (crash dumps com build/versão/versionCode/sha256/epoch).

## 2. Diagnóstico (porque a intermitência sobreviveu a 0.7→0.8.9)

A 0.8.7 deu cap+evicção+negative-cache ao cache de primitivas; a 0.8.9
validou antes do upload. A intermitência SOBREVIVEU porque o problema era
ESTRUTURAL: todas as trocas de todos os TICs partilhavam o mesmo caminho
de cache — evicção/release/rebind em qualquer ponto podia mexer no mesh de
OUTRO TIC (use-after-free latente em 'mrs.at(r).mesh == m'), e o
`primMeshDestroy()` do lifecycle destruía meshes que ponteiros de
MeshRenderers ainda referenciavam entre TERM/INIT. A decisão do dono
elimina a CLASSE do bug: sem cache não há evicção, sem evicção não há
mesh de outro TIC a morrer debaixo dele — cada MeshRenderer tem o SEU
mesh, e a morte dele só acontece por deferred free no início do frame
seguinte à troca (nunca com draws em voo).

## 3. Arquitetura da troca determinística

- **Pedido** (PURO, no applyAssetPick/Inspector/serializer/preset):
  `mr->primOn=true + mr->prim=<assinatura> + mr->primPending=true` — o
  mesh ANTIGO continua ligado e a renderizar; NADA de GL no pedido.
- **Ponto seguro** (início do frame, ANTES de qualquer submissão GL —
  `primGraveDig()` + `primFlushPending()` no topo do `frame()`):
  1. COVA: liberta os meshes reformados no frame ANTERIOR (deferred
     free — os comandos do frame anterior já saíram);
  2. FLUSH: por pendente, o caminho ÚNICO `primUploadOne`:
     `passo=gerador` (makePrimMesh — pureza por hash) → `passo=validacao`
     (verts/idx > 0, coordenadas finitas, AABB não degenerado) →
     `passo=upload` (Mesh::create + **SELF-CHECK** das contagens
     pós-upload) → `passo=bind` (troca atómica do ponteiro; o antigo vai
     para a COVA; `mr->primPrev` encadeia o rótulo "de" da próxima troca).
- **Falha**: o mesh anterior fica (render continua), seleção intacta,
  `primNeg=true` (backoff — o rebind por frame NÃO insiste; pedido novo
  ou INIT_WINDOW limpam), log passo+razão e toast no ecrã.
- **Pós-TERM**: mesh null + primOn = re-rebind pelo mesmo caminho único
  (o detach do lifecycle não mexe em primPending — o load re-arma).

## 4. Arquitetura do import streaming (500 MB)

- **ImportJob em thread própria**: o frame NUNCA congela; o overlay
  "IMPORT…" (modal) desenha nome + MB/MB + barra + CANCELAR; o finalize
  (catálogo, diálogo "aplicar ao TIC?") corre no frame() quando o worker
  sinaliza done — só a thread principal toca cena/UI/GL.
- **Escrita streaming**: `ProjectStorage::openWriteStream/writeStreamChunk/
  closeWriteStream` (novo contrato) — FsStorage FILE* real, SafStorage fd
  SAF real, default acumulado com teto 256 MB (FakeStorage/testes).
- **Conversão**: OBJ linha-a-linha (ObjStreamParser — o parseObj em
  memória virou WRAPPER do mesmo caminho); GLB: header+JSON inteiros
  (JSON ≤ 16 MB) + BIN chunk DEFERIDO — o `GltfBufferStore` materializa
  SÓ os ranges de accessors/imagens (≤ 64 MB por range) por um
  `GltfRangeLoader` sobre a FILE*; glTF: JSON + .bin irmão; PNG ≤ 64 MB.
- **Orçamento de RAM**: chunk (6 MB) + 1 range + 1 CompressedImage —
  independente do tamanho da fonte. A PROVA (host, output real):

```
  [500MB-green] ok=1 err= | fonte 523837479 B → saida 139 B (ratio 3768615.0x) | 1700 ms | RSS pico 11 MB
  [500MB-red] readAll ok=1 — RSS pico 510 MB (o caminho antigo: o crash do dono)
  [500MB] fixture gerada em 220 ms; import verde + prova red acima
```

(a fixture: ~500 MB de ruído de comentário + geometria pequena válida —
o caso real em que o CONTEÚDO útil é limitado pelo u16 do engine e o que
crashava era o READALL; o filho RED é a prova de que o caminho antigo
estoura o orçamento no mesmo ficheiro.)

## 5. Arquitetura dos formatos próprios

Header comum de 32 B (magic[4] + version u16 + endianMark u16 + align u32
+ payloadSize u64 + checksum FNV-1a u64 + reserved u32) — toda a leitura
valida magic/endian/versão/tamanho/checksum/contagens ANTES de tocar em
arrays; corrupção = erro legível (output real):

```
  [corrupcao] err='CHECKSUM CORROMPIDO: payload diz 0x15250650597522458461, recalculado 0x16914277940048616040 — o ficheiro foi danificado'
```

- **.gmesh** (magic GMES): indexado+dedup (MeshData já é), QUANTIZAÇÃO
  16-bit (pos no AABB, normal [-1,1] + renormaliza, uv [0,1], skin
  joints u8/weights u16 + renormaliza), grupos com nome/material/range.
  Round-trip e rácio (output real):
```
  [gmesh] esfera 221 verts 1152 idx — OBJ-texto 6766 B → .gmesh 5926 B (1.14x menor)
```
- **.gtext** (magic GVTX): CompressedImage (formato + dims + tabela de
  mips + blob) — o produto do TexturePipeline persistido; upload direto
  `glCompressedTexImage2D`, ZERO re-compressão por arranque.
- **.gm** (magic GANM): clips (tracks/keys/tangentes) + esqueleto
  opcional (joints com TRS + bind TRS + inverseBind) — o loader cria o
  AnimationPlayer e o SkeletonComp do TIC (`attachGAnim` — as MESMAS
  regras do glTF: clips acrescentam sem duplicar nome).
- **Storage**: fonte em `source/`, convertidos em `assets/`; catálogo
  lista CAMINHOS COMPLETOS (assets/ primeiro + legado não-convertido);
  setting "fonte: manter/largar" (persistido em `settings.goni`) + botão
  "reconverter assets"; migração de projetos antigos EM SILÊNCIO no
  primeiro load (`migrateLegacyAssets` + fixup de refs + attach do .gm
  irmão + re-save das refs).

## 6. Archives — EXTRAIR ≠ IMPORTAR (2 passos do dono)

- **Passo 1 (extrair)**: tocar num .zip no navegador → job de extração
  streaming (thread + progresso + cancelar) para `extracted/<nome>/` —
  ficheiros CRUS, ZERO conversão; o browser ABRE a pasta extraída.
  Implementação: parser PRÓPRIO de central-directory (~300 linhas, sem
  vendoring) + inflate do zlib (NDK/host — `target_link_libraries(… z)`).
- **Segurança** (output real dos testes):
```
  [zip] 4 entradas → 4 ficheiros crus em extracted/pack/ (426 B de 586 B de archive; ZERO conversao)
  [bomb-guard] err='archive: bomb-guard entrada 'bomba.bin' declara 3072 MB (teto 1024 MB)'
```
  zip-slip (`../`, absoluto, `\`) → REJEITADO + log, nunca escreve fora;
  bomb-guard (1 GB/entrada, 2 GB total) → erro legível; CRC por entrada
  conferido; archives aninhados ignorados + log; cancelamento remove o
  parcial (sem estado parcial); ZIP64/encriptado → erro legível.
- **Passo 2 (importar)**: manual, pelo fluxo de sempre, a partir da pasta
  extraída (o pipeline de conversão normal).
- **Tabela de decoders**:

| Decoder | Decisão | Licença | Tamanho | Porquê |
|---|---|---|---|---|
| zlib inflate | **USADO** (ZIP deflate) | zlib (permissiva) | 0 (sistema/NDK) | o formato ZIP é estável; o parser próprio são ~300 linhas |
| miniz | REJEITADO | MIT | ~150 KB vendored | peso morto: o parser próprio + zlib cobrem o caso |
| unrar | REJEITADO | freeware c/ restrições (fonte visível obrigatória) | ~500 KB | licença restritiva + peso; o .rar dá erro LEGÍVEL "usa .zip" (a decisão documentada) |

## 7. Diagnóstico com identidade

- CI em 2 passes: assembleRelease → sha256 REAL da libgoni_vv.so arm64 →
  `app/src/main/assets/build_info.txt` (version/versionCode/git/epoch/
  soSha256) → 2º assembleRelease embute (a .so não muda — o sha vale).
- VvActivity.onCreate lê o asset + BuildConfig → `nativeSetBuildInfo`
  (JNI; gate jni_parity com a assinatura) → `vv::buildinfo`.
- (pendente na altura) crash dump: `crash-<unix>-vc<versionCode>.dump` + header
  `build:`/`git:`/`so:`/`epoch:` (output real do teste):
```
  [identidade] dump 'test-wiring010-logs/crash-1790000000-vc40.dump' com build/versionCode/git/epoch no header
  [banner] boot: goni-vv 0.8.10-teste (versionCode 40, git abcd1234, so ok)
```
- Banner de versão no boot log (1ª linha do viewer); log viewer marca
  dumps de outra build com **[ANTIGO]** (dumps pré-0.8.10 = ANTIGO).

## 8. Implementação por ficheiro

- `render/Primitives.{h,cpp}`: PrimKind {Sphere, Box, Count=2};
  primRemoved/primFromName com migração → Box; primMeshHash (pureza);
  geradores esfera+box apenas (os 6 outros APAGADOS).
- `components/MeshRenderer.h`: primPending/primRetire/primNeg/primPrev
  (runtime-only, nunca serializados).
- `platform/main.cpp`: cache APAGADO (PrimCacheEntry/primCacheEvict/
  primMesh/primMeshDestroy/rebindPrimMeshes mortos); g_primOwners+
  g_primGrave + primUploadOne/primFlushPending/primGraveDig/
  primMeshesDestroyAll/primMeshesToGrave; ImportJob (thread+atómicos+
  overlay+cancel); browserExtractArchive; postLoadMigrateAndFixup;
  refreshCatalog com caminhos completos; banner de boot; badge ANTIGO;
  settings fonte/reconverter; buildinfo na JNI.
- `ui/EditorUi.cpp`: seletor de primitivas com 2 botões; pick ARMA o
  pedido; Inspector sliders R/Seg/Anéis (H/Tube mortos); catálogo com
  caminhos completos; Settings com 5 itens (fonte + reconverter).
- `ui/EditorLayout.h`: primUsesSegments/primUsesRings; kInspectorPrimRings.
- `core/SceneSerializer.{h,cpp}`: prim carrega PENDENTE; onPrimMigrated
  (callback de migração com log+toast 1×); height/radius2 lidos e
  ignorados (compat .goni antigo).
- `core/ProjectStorage.{h,cpp}`: openWriteStream/writeStreamChunk/
  closeWriteStream (default acumulado com teto) + remove.
- `core/FsStorage.cpp` / `core/SafStorage.cpp`: write stream REAL (FILE*/
  fd SAF) + remove REAL.
- `assets/GOwnFormats.{h,cpp}` (NOVO): header comum + .gmesh/.gtext/.gm.
- `assets/AssetConverter.{h,cpp}` (NOVO): importFile/reconvertFile/
  migrateLegacyAssets + copyToStorage streaming.
- `assets/ObjImporter.{h,cpp}`: ObjStreamParser linha-a-linha (parseObj
  vira wrapper).
- `assets/GltfImporter.{h,cpp}`: GltfRangeLoader + GltfBufferStore
  (buffers DEFERIDOS com materialização por range).
- `assets/GltfAnim.{h,cpp}`: attachGAnim (clips + SkeletonComp do .gm).
- `assets/ZipExtract.{h,cpp}` (NOVO): parser ZIP + inflate + zip-slip +
  bomb-guard + CRC + cancelamento.
- `assets/ResourceManager.cpp`: branch .gmesh (log com tempo).
- `render/GpuAssets.cpp`: branch .gtext (upload direto).
- `platform/BuildInfo.{h,cpp}` (NOVO) + `platform/CrashHandler.cpp`
  (identidade no nome/header) + `platform/StorageBridge.cpp`
  (nativeSetBuildInfo) + `platform/FileApi.{h,cpp}` (fileSize/ChunkReader/
  copyFileChunked; kind 'a').
- `VvActivity.java`: nativeSetBuildInfo + leitura do build_info.txt.
- CI `release.yml`: 2 passes + build_info.txt + gates de símbolos dos
  formatos/streaming/zip (writeGMesh/readGMesh/readGText/readGAnim/
  primMeshHash/extractArchive/attachGAnim/importFile/migrateLegacyAssets).

## 9. Decisões

- **Sem cache**, sem hit: cada troca gera+valida+upload+bind — o custo é
  um mesh pequeno por troca no ponto seguro (stress: 600 trocas em 3 ms);
  a DETERMINISMO vale mais que o micro-reuso.
- **primPrev no MeshRenderer** (runtime-only): o rótulo "de" do log usa a
  FONTE REAL do mesh ligado (cube/asset/assinatura anterior) — o pedido
  já sobrescreveu mr->prim quando o flush corre.
- **Pedido NÃO anula o mesh**: a primeira versão anulava (mesh=null no
  pick) — uma falha de upload deixaria o TIC SEM mesh; a versão final
  mantém o antigo a renderizar até o bind (zero flicker, zero buraco).
- **Header de 32 B com reserved**: a 1ª versão escrevia 28 B e os
  leitores assumiam 32 (o roundtrip do repro apanhou: "header diz 106 B
  mas só há 102 B"); o reserved fecha o tamanho como CONTRATO.
- **migração por reconvertFile** (não importFile direto): um só caminho
  de reconversão que cobre FS real (streaming) E SAF (readBytes com
  guarda + staging /tmp) — dívida documentada: reconverter fontes
  GIGANTES em SAF faz readBytes com teto 256 MB (o caminho 100%
  streaming é o import do browser, onde os 500 MB entram).
- **unrar REJEITADO** (licença + peso) — .rar dá erro legível "usa .zip"
  (tabela no §6); ZIP = parser próprio + zlib (sem vendoring).
- **.gm com esqueleto embutido** (secção opcional no fim): scenes
  convertidas recuperam skinning sem re-parse do glTF; ficheiros .gm v1
  sem a secção continuam válidos.

## 10. Testes (568 → 584; todos com o caminho do device)

Novos: formatos round-trip com rácio impresso; checksum corrompido /
magic / versão / endian rejeitados com erro legível; 500 MB com RSS REAL
+ prova RED do readAll (forks); zip cru/estrutura/deflate + zip-slip +
bomb-guard + cancelamento sem estado parcial; migração e2e (ref do TIC
reescrita em silêncio); identidade no dump + badge ANTIGO + banner;
setting fonte (largar) + reconverter; stress 600 trocas SEM cache
(100% sucesso, contadores no output); deferred free (o antigo NÃO morre
no mesmo frame e AINDA desenha); backoff pós-falha (60 frames = zero
uploads); pureza por hash (2 tipos × variantes); migração de prim
removida + round-trip .goni; oito→duas prims ×3 ordens no device-path.

Output real do stress (o "testar mesmo"):
```
  stress: 600 trocas em 3 ms (media 0.01 ms) — trocas ok 406, ERRO 0, vivos=1 cova=0
```
(406 = as trocas que passaram pelo caminho de prim; as outras 194 foram
ciclos prim↔cube/prim↔importado — o contador global `g_primSwapOk` conta
APENAS uploads de prim, por isso 406 ≠ 600: as 600 iterações todas
terminaram com mesh válido — `ok == 600` aferido no local.)

## 11. Riscos

- O import job usa `std::thread` — o elog é mutex'd e o storage só é
  tocado pelo worker enquanto o main não escreve (o finalize corre no
  frame); se um futuro auto-save tocar o storage a meio do job, será
  preciso um mutex de storage (dívida anotada, não há auto-save hoje).
- Quantização 16-bit: erro posicional ~1/65535 do maior eixo (aferido:
  extents de 1000/500/250 reconstruídos dentro de 0.1) — suficiente para
  meshes de jogo; a geometria ORIGINAL fica intacta em source/.
- O default de write stream acumula com teto 256 MB (FakeStorage e
  eventuais storages futuros) — Fs/Saf têm streaming real.

## 12. Dívida

- Reconversão de fontes > 256 MB em projetos SAF (readBytes com guarda;
  o caminho streaming é o import do browser) — virar o SafStorage para
  leitura por fd/ranges se o dono trouver fontes gigantes em SAF.
- ZIP64 (> 4 GB) e entradas encriptadas: rejeitados com erro legível
  (documentado) — sem caso de uso no device.
- deflate do ZIP com dictionary/ríos exóticos: o inflate streaming cobre
  o subset canónico; testes com stored+deflate standard.
- Áudio (.gi): 0.8.11 (a sub-fase seguinte do prompt).

## 13. CLÁUSULA CALMA

Só o prompt 0.8.10 + testes: zero física nova, zero V.ONI, zero layout
novo além do overlay de progresso do import e dos 2 itens novos de
Settings (fonte/reconverter) exigidos pelo próprio prompt.

## 14. Como ler o log se algo falhar no C33

- Import: `import: job iniciado` → `import: fonte copiada … chunks` →
  `asset: convert …` (se faltar UMA delas, o passo exato está antes dela;
  `import: FALHOU — <razão>` diz a causa).
- Troca de primitiva: as 4 linhas `passo=` — a que faltar/ERRO diz o
  passo; `mesh: prim <x> removido -> cube` é a migração (normal).
- Archives: `archive: open … entries=N` → `archive: extract … ok` por
  entrada → `archive: extraido N ficheiro(s)`; `zip-slip rejeitado` e
  `bomb-guard` são as guardas a FUNCIONAR.
- Crash (se algum): o NOME do dump já traz o versionCode; o header traz
  build/git/so/epoch — o addr2line do workflow resolve contra o build
  exato sem adivinhar.

## 15. Gates do CI

core-tests 581 · check_main · link_parity (89 TUs com -lz) · jni_parity
(4 natives, +nativeSetBuildInfo) · build-release ASSINADO versionCode 40
(2 passes com build_info.txt embutido) · verify-entry-symbols com os
gates NOVOS (writeGMesh/readGMesh/readGText/readGAnim/primMeshHash/
extractArchive/attachGAnim/importFile/migrateLegacyAssets no .dynsym).

## 16. Checklist device

No README (§ Verificação no Realme C33 — 0.8.10): 7 blocos com pass/fail
por item — trocas 10× ordens variadas (0 falhas/0 desseleções/0 dumps),
cena antiga com prim removida (cube + aviso), import do maior ficheiro
com progresso+cancelamento sem crash, projeto antigo converte em
silêncio, extração .zip com ficheiros crus + import posterior, settings
fonte/reconverter, banner de identidade + badge ANTIGO. Zero dumps
novos continua a ser o critério global — e agora cada dump se identifica.

---

## FECHO CI (17)

- Commits: `6052595` (0.8.10-a: mesh determinística + streaming + formatos)
  → `9d4317d`/`22c62b5` (fixes CI: buildConfig true + assetInfo de
  instância — os 3 erros javac do 1º run) → `ad4ed3a` (artifact 0.8.10).
- Run **37046257129**: 100% VERDE — core-tests **581** OK + JVM host +
  check estrutural + build-release **ASSINADO versionCode 40** (2 passes
  com build_info.txt) + verify-entry-symbols com os GATES NOVOS.
- **Artifact `goni-vv-0.8.10-release-signed`** — `app-release.apk`
  sha256 `a0781e661c29853f53c2158e6080e060eceaf2418731850f0e6e261dacbae807`.
- **Identidade verificada ponta-a-ponta**: o build_info.txt DENTRO do APK
  diz `soSha256=f9ba6bc647766ac37d5594252eee2105116c9573723c097e991ec215bc60449a`
  e a .so arm64 EXTRAÍDA do mesmo APK tem EXATAMENTE esse sha256 (o
  2-passes do CI funciona — qualquer crash dump desta build identifica-se).
- Símbolos novos confirmados no .dynsym real do APK: writeGMesh/readGMesh/
  readGText/readGAnim/primMeshHash/extractArchive/attachGAnim/importFile/
  migrateLegacyAssets (9 famílias).
- Aguarda VERIFIED do dono no C33 (checklist 0.8.10 no README — 7 blocos;
  zero dumps novos continua a ser o critério global).
