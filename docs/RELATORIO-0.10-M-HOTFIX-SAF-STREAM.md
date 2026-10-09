# RELATÓRIO 0.10.4c — 0.10-M HOTFIX SAF-STREAM (o city importa sob SAF)

> versionCode 59 · commit próprio entre o PASSO 4 e o PASSO 5 · test_core
> 0 falhas · c33_virtual 724/724 · todos os gates verdes · scope dentro de
> ci/scope.txt · NADA mais tocado (V.ONI, seleção, física, TICs, janelas,
> EditorUi, ResourceManager — o contrato do picker/hull do PASSO 4 intacto).

## 1. O pedido do dono (verbatim, os 5 itens)

1. mmap por FD, não por caminho: o streaming recebe o fd do bridge
   (`bridgeOpenFd`) e mapeia por fd — mmap não precisa de path; se o
   provider recusar mmap, degrada para pread de ranges (já existe). Nunca
   descai para o legado por causa de storage.
2. O mmap/pread do scene.bin usa o fd do irmão já copiado em source/.
3. O legado de mesh única fica só para modelos ≤65535 vértices E sem
   skins; acima disso, streaming obrigatório sob qualquer storage; skinned
   mantém mensagem clara «pele ainda não suportada no streaming (BACKLOG)».
4. A mensagem «excede 65535» nunca mais aparece para modelo não-skinned,
   em storage nenhum — esse teto deixa de ser visível ao utilizador.
5. Atribuição de tempos: fase=parse cobre só o JSON; scan de ranges do
   bin vira fase=ranges ms=... separado (explicar os 5395 ms).

## 2. A causa (o diagnóstico, com ficheiro+função)

`app/src/main/cpp/assets/AssetConverter.cpp · convertGltfCommon` — o gate
da linha do PASSO 3:

```cpp
bool canStream = mmapBinPath != nullptr && binFile != nullptr &&
                 !st.root().empty() &&
                 st.root().rfind("content://", 0) != 0;   // ← a recusa SAF
```

Sob um projeto SAF (raiz `content://`) o streaming NUNCA ativava: o import
re-parseava no modo integral e caía no merge do legado, cujo teto de
65535 vértices matava o city (130 MB bin, 72 primitivas, 0 skins) com
«glTF: o modelo fundido excede 65535 vértices (… SAF sem ficheiro real
…)». Nota de contexto: o TEMP (cascata do stagingWrite → cache dir da
app) e o VERIFY (readback pelo storage) JÁ funcionavam sob content://
desde os fixes F2/F4 do PASSO 3 — a ÚNICA coisa que faltava era a FONTE
do bin exigir caminho POSIX.

## 3. A cura (item a item)

**(1) mmap por FD** — `platform/FileApi.{h,cpp} · fileapi::mapFd64` NOVO:
o MESMO contrato do `mapFile64` (validação de range contra o fstat,
alinhamento à página para baixo, errno no log) sobre um descritor JÁ
ABERTO; o fd não se fecha lá (quem abriu fecha — o mapping segura a
referência). O fd chega pela porta NOVA
`core/ProjectStorage.h · ProjectStorage::openReadFd`:
- `core/FsStorage.cpp` — `::open(O_RDONLY|O_CLOEXEC)` do caminho real
  (o 2.º degrau da cascata quando o mmap por caminho falha);
- `core/SafStorage.cpp` — `resolveFile` → `io_->openFd(uri, "r")`: no
  device é o **bridgeOpenFd** do `VvActivity`
  (`ContentResolver.openFileDescriptor` + `detachFd` — o JniSafIo já
  existia, ficheiro `platform/StorageBridge.cpp:885-931`);
- default honesto no `core/ProjectStorage.cpp` (false — o storage sem
  fds).

A CASCATA em `convertGltfCommon` (nunca o legado por causa do storage):
mmap-caminho (o de sempre — R-039 intocado) → sob `content://` a fonte
OFICIAL é o FD DO IRMÃO em source/ mapeado por fd → o provider que
recusa o mapa degrada para **pread de RANGES**: `GmeshV3Stream.cpp` ganha
o `V3RangeSource` — cada TAREFA carrega o seu SPAN por
`st.readBytesAt` (mutex entre workers; a aritmética do corte fica INTACTA
com `base = spanBuf.data() − spanOff`); a verificação re-carrega o que
precisa. A higiene MADV_DONTNEED fica DESLIGADA em heap (`TaskCtx.canDrop`
— a lição F3 do PASSO 3: madvise em memória anónima zera páginas de
vizinhos). O log nomeia a escolha:
`asset: v3 fonte=<mmap-caminho|mmap-fd|ranges|data-uri>`.

**(2) o irmão copiado em source/** — `importFile` passa o `srcRel`
(`source/<nome>`) a `convertGlbFile`/`convertGltfFile`;
`convertGltfFile` aponta o `binRel` ao IRMÃO (`source/<rel>` do
`siblingPathOf` — exatamente onde o `copyGltfSiblings` o copiou). Sob
`content://` o conversor lê-O pelo fd do bridge (GLB: o próprio ficheiro
copiado; .gltf: o scene.bin copiado).

**(3) o legado só ≤65535 sem pele** — o gate não olha mais o storage
(olha a EXISTÊNCIA de fonte: caminho, cópia em source/, ou bytes data:).
O merge continua a ser a rede de segurança, e a pele acima do teto falha
com a mensagem clara «pele ainda não suportada no streaming (BACKLOG)»
(`GltfImporter.cpp` no merge por-mesh; `AssetConverter.cpp` no merge do
modelo). A entrada vive no `BACKLOG.md` («0.10-M · PELE NO STREAMING»).

**(4) o teto invisível ao não-skinned** — com a cascata + o 4.º canto: um
`.gltf` SEM ficheiro de buffer (data: URI) também streama — o
`parseGltf` ganha o parâmetro `streamOwnedBin`: no modo streaming o
buffer 0 embutido sai INTEIRO do parse e o conversor corre sobre os bytes
(`binDroppable=false` — heap). Os restantes textos de teto nunca mais
culpam o storage (dizem pele ou multi-buffer, com causa no engine.log).
Asserção de guard: `logCount("excede 65535") == 0` na FASE 21.

**(5) fase=ranges** — `FileRangeCtx.rangeMs` acumula o tempo das leituras
de ranges DURANTE o parse (o `fileRangeLoad` cronometra-se); a linha
contrato passa a ser DUAS: `gmesh: fase=parse ms=<total−ranges>` (só o
JSON) + `gmesh: fase=ranges ms=<ranges>` (o scan do bin — as imagens que
materializam os seus bufferViews). Os 5395 ms que o dono viu no parse do
city passam a ter dono próprio.

## 4. As regressões pedidas (verde) e as mutações (vermelho→verde)

| Prova do dono | Onde corre | Resultado |
|---|---|---|
| FakeStorage-com-fd que recusa mmap → verde via pread | `safstream_fd_que_recusa_mmap_importa_por_ranges` (SafStorage+FakeSafIo, raiz content://, `refuseMmapFds`: a sonda do mmap leva um PIPE — o fstat do FIFO dá size 0, a recusa É do SO) + c33 21.2 | **VERDE** — `fonte=ranges`, «a degradar para pread de RANGES», `verificado=1`, «ranges do corte: 72» (UMA carga de span por tarefa) |
| mutação (streaming desativado sob SAF) → vermelho | **M-S1**: o gate `content://` reposto | **VERMELHO** — `safstream_saf_importa_city…` e `safstream_fd_que_recusa…` FALHAM (o import morre no teto do legado — a reprodução exata do bug do dono; os testes não-SAF ficam verdes: a mutação é cirúrgica) |
| cena sintética >65535 sob FakeStorage → verde | `safstream_cena_sintetica_65535_fakestorage_verde` | **VERDE** — `fonte=mmap-caminho` (o de sempre), `verificado=1`, ≥2 blocos |
| (a mais) o city sob content:// | `safstream_saf_importa_city_em_blocos_pelo_fd_do_bridge` + c33 21.1 | **VERDE** — `fonte=mmap-fd` (o fd do bridge), `verificado=1`, 72+ blocos, ZERO «excede 65535» |

Provas EXTRA (o contrato do hotfix completo): `safstream_ranges_no_
conversor_puro` (bin=nullptr + `V3RangeSource` com contador — UMA carga
por tarefa, verificado=1), `safstream_data_uri_sem_ficheiro_tambem_
streama` (70 000 verts embutidos no JSON — sem ficheiro, sem teto),
`safstream_fase_ranges_linha_propria` (as DUAS linhas existem),
`safstream_pele_grande_mensagem_backlog` (a pele de 70 000 verts → a
mensagem do BACKLOG, nunca a de storage), `regress_safstream_fd_mmap_e_
ranges_r041` (o mapFd64: bytes exatos com offset não alinhado; o fd
continua usável; o PIPE recusa; o openReadFd do Fs entrega fd real).

Mutações M-S2 (a degradação morta — o mmap recusado cai no legado em vez
dos ranges) → `safstream_fd_que_recusa…` VERMELHA; M-S3 (o `mapFd64`
nunca chamado — o fd abre e fecha sem mapa) → `safstream_saf_importa_
city…` VERMELHA (a linha `fonte=mmap-fd` desaparece). Todas repostas por
cp do backup → **test_core 0 falhas · c33 724/724 VERDE**.

A nota honesta do fake: não existe fd seekable+com-conteúdo+não-mapeável
construível no CI sem mount FUSE real — o pipe dá a RECUSA REAL do SO
(não um hook da engine) e as reaberturas do `readBytesAt` servem memfds
seekable, que é exatamente o caso FUSE-sem-mmap do device (mapa recusado,
leituras com lseek funcionam). O `FakeSafIo` ficou mais fiel ao provider
real: materializa as escritas PENDENTES de um doc ANTES de servir o seu
fd de leitura (o import lê source/ DENTRO do próprio importFile — a cópia
acabou de fechar o fd de escrita; no device é o provider que persiste ao
fechar).

## 5. As linhas contrato novas (o que o dono vê no engine.log)

```
asset: v3 fonte=mmap-fd 'source/city.glb' (o fd do bridge — mmap por fd, SEM caminho)
asset: v3 fonte=ranges 'source/city.glb' (pread de ranges pelo storage — o pico é o span da tarefa)
asset: v3 fonte=data-uri (3145728 B embutidos no JSON — o buffer 0 sem ficheiro, SEM teto)
asset: v3 ranges do corte: 72 carga(s) de span — o pico é o MAIOR span em voo (nunca o modelo)
gmesh: fase=parse ms=12        (só o JSON)
gmesh: fase=ranges ms=5395     (o scan de ranges do bin — as imagens)  ← o número do dono, com dono
```

## 6. Gates + CI (no push deste commit)

scope (dentro de ci/scope.txt — cpp/, tests/, docs/, README, BACKLOG,
build.gradle) · docs-lint 70 ficheiros · gmesh-docs 21 âncoras ·
check_main (paridade main.cpp/StorageBridge.cpp no hospedeiro) ·
test_core 0 falhas · c33_virtual 724/724 (FASE 21: +18 checks, 706→724) ·
release-identity versionCode 59 (declarado no RELATORIO-0.9.6 §14) · APK
arm64 assinado `goni-vv-0.9.6-release-signed`.

**A NOTA DO CI (colada)**: o push `74580bd` → run **37911218605** —
**VERDE À PRIMEIRA**: Testes do core (Linux) ✓ · C33 virtual
(dispositivo + sentinelas + gates) ✓ · APK release arm64 assinado ✓ ·
verify-entry-symbols ✓ · artefacto **goni-vv-0.9.6-release-signed**
(versionCode 59) — o APK que o dono instala para o re-sign-off do §8.

## 7. O que NÃO foi tocado

V.ONI, seleção, física, TICs, render do jogo, storage de projetos
(exceto o `openReadFd` ADITIVO — nenhuma implementação existente mudou de
semântica), janelas, EditorUi/serializer, ResourceManager (a recusa do 3B
e o guard de 192 B intactos), o caminho mmap-por-caminho do Fs (R-039
intocado — a sentinela dos 50 M verts corre no MESMO caminho de sempre).

## 8. NÃO VERIFICADO (honesto — só o device humano afere)

1. **O city REAL de 130 MB no C33** (vc 59): importar o ficheiro original
   do dono num projeto SAF (escolher a pasta pelo SAF) — o esperado no
   device: o import completa com `verificado=1`, o engine.log traz
   `fonte=mmap-fd` (ou `fonte=ranges` se o provider do C33 recusar o
   mapa — a linha diz qual) e NENHUMA linha «excede 65535»; o apply pelo
   picker segue o contrato do PASSO 4 (o hull + o render por blocos).
2. **Os 5395 ms do parse do city** redistribuídos: no device, a linha
   `fase=parse` deve encolher para o tempo do JSON e a `fase=ranges` deve
   absorver o scan do bin (o dono cola as duas linhas aqui):
   ```
   gmesh: fase=parse ms=<n> · gmesh: fase=ranges ms=<n>
   ```

## 9. PÁRA

**PÁRA** — o PASSO 5 (texturas ASTC) continua BLOQUEADO à espera do OK
explícito do dono (depois o PASSO 6 — a prova de mesa no C33; após o 6,
STOP absoluto do 0.10-M).
