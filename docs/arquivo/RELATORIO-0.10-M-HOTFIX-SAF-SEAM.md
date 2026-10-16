# RELATÓRIO 0.10.6 · HOTFIX SAF-SEAM — as duas últimas costuras do SAF

> Escopo travado: SÓ a abertura por blocos do runtime (a cascata do fd),
> o overlay de progresso, a quarentena do asset corrompido e as provas.
> O PASSO 5 (texturas ASTC) continua À ESPERA do OK do dono — NADA mais
> foi tocado. versionCode **61**.

## 1. O pedido do dono (os 5 itens)

1. **BlockMesh open por fd** — a MESMA cascata do conversor:
   `openReadFd` / `mapFd64`; se o provider recusar o mmap, pread de
   ranges. Nunca abrir por caminho POSIX sob `content://`. Nas falhas,
   o log diz QUE leitura falhou (das 3), o errno e o tamanho do
   ficheiro visto.
2. **O total do overlay vem do length do bridge** — quando o length é 0
   e a fonte não é vazia, mostrar «copiando… B» e mudar para o valor
   real quando este chega; NUNCA mostrar «0/0».
3. **A tabela que não valida entra em quarentena** — rename `.corrupt`
   + mensagem «asset corrompido, reimporta»; nunca a troca silenciosa
   pela primitiva bola; o mesh picker mantém o estado anterior.
4. Regressões + mutações (as mutações têm de ficar VERMELHAS).
5. Relatório curto + gates + CI para o C33 verificar.

## 2. A causa (o que estava partido, com ficheiro+função)

- **A costura do runtime** (render/BlockMesh.cpp `open`): a abertura
  por blocos lia o peek/tabela/materiais TODOS pelo `readBytesAt` do
  storage — o conversor já tinha a cascata do FD DO BRIDGE (o hotfix
  SAF-STREAM), o runtime NÃO: sob `content://` o SAF reabre um fd POR
  RANGE (um round-trip ao provider por leitura) quando o provider dá
  um fd mapável uma única vez.
- **A costura dos materiais** (o mesmo `open`, o ramo sem tamanho): as
  sondas (1 MB→64 KB→4 KB→256 B) exigem o comprimento EXATO — a cauda
  de nomes é quase sempre mais curta que 256 B («cidade» são 8 B) e um
  provider sem stat nem fstat (fd=pipe/stream) MATAVA a abertura com
  «falhou pelas sondas». Achado NO CI da FASE 23: é a segunda costura
  que o nome do hotfix nomeia.
- **O overlay 0/0** (platform/main.cpp, o bloco do `IMPORT...`): a
  linha dividia incondicionalmente por MB; as fases reportam UNIDADES
  MISTAS (a cópia bytes, o corte TRIÂNGULOS, o assembly bytes de
  saída) — o total sub-MB arredondava «0 / 0 MB» (o corte do city de
  72k durante MINUTOS) e o arranque do job (length ainda a 0) idem.
- **A corrupção silenciosa**: a tabela que não validava devolvia um
  err genérico («faixa da tabela de blocos ilegível»), o asset FICAVA
  no catálogo (o dono volta a escolhê-lo) e o toast dizia só «falha ao
  carregar mesh» — sem CORROMPIDO, sem REIMPORTA, sem quarentena.
- **Bónus achado pelas provas**: o `SafStorage` tem CACHE de URIs
  (`fileUris_`) — um rename sem a invalidar deixava o `exists()` a
  MENTIR depois da quarentena (o original «existia» ainda).

## 3. A cura (item a item)

1. **A cascata no `BlockMesh::open`** (render/BlockMesh.cpp):
   `st.openReadFd` (sob content:// é o FD DO BRIDGE — `bridgeOpenFd`)
   → `fstat` → `fileapi::mapFd64` serve as 3 leituras (**ZERO** ranges)
   → o provider que recusa o mapa: **pread das faixas NO MESMO fd**
   (fonte=`pread-fd`) → o pipe/stream (fstat 0 B) fecha o fd e cai nos
   RANGES do `readBytesAt` (fonte=`ranges`). O fd/mapping vivem SÓ
   durante a abertura (o lazy de cada bloco é o `readBytesAt` do PASSO
   4 — contrato intocado). Em falha, o `err` diz QUAL das 3 leituras
   («faixa do PEEK/TABELA/MATERIAIS»), o errno do pread e «ficheiro
   visto com N B». A linha contrato: `gmesh: fase=load ms=… (por
   BLOCOS: tabela de N blocos, B B lidos, fonte=<…>; dados=D B por
   carregar em lazy)` — o par do `asset: v3 fonte=` do conversor.
2. **Os materiais NOME A NOME** (o ramo sem tamanho): as sondas
   continuam primeiro (uma leitura quando a cauda é grande); quando
   todas falham, a leitura incremental monta os bytes EXATOS pela
   tabela AUTO-DESCRITIVA (u16 comprimento + nome + pad a 4 do offset
   ABSOLUTO — o MESMO alinhamento do escritor); uma leitura que falha
   a meio entrega o PARCIAL ao reader (truncado → quarentena com a
   causa certa, não «sondas»).
3. **A quarentena** (o peek v3 VERDE + tabela/materiais que não
   validam ou acabam além do fim): `ProjectStorage::rename` NOVO —
   `FsStorage` = `::rename` POSIX atómico; `SafStorage` = CÓPIA
   STREAMING por fd (uma abertura de leitura + o write stream do
   destino em chunks de 1 MB + remove; falha a meio LIMPA o destino —
   sem estado parcial) com a `fileUris_` a seguir o nome; `FakeStorage`
   = o mapa. Os bytes FICAM para forense; o NOME sai do catálogo (o
   picker lista `.gmesh`, nunca `.corrupt`). O `err`: «asset
   corrompido, reimporta — <causa>». O peek inválido (v1/garbage) NÃO
   entra em quarentena (o caminho do mesh único decide); a falha de
   I/O NÃO é quarentena (o ficheiro fica para RETENTAR).
4. **A mensagem ao dono**: `GpuAssets::lastMeshError()` (o último erro
   de mesh/blockHull) + `reportMeshPickFailure(menuKind)` nos DOIS
   dispatches (o «Sim» do diálogo pós-import e o seletor do
   Inspector): o toast diz «asset corrompido, reimporta (renomeado
   .corrupt; causa no engine.log)», o engine.log ganha a linha da
   quarentena e o `refreshCatalog()` re-lista (o `.corrupt` já não é
   oferecido). O applyAssetPick NÃO mudou de contrato: falha limpa, o
   TIC MANTÉM o mesh/path/primOn anteriores — nunca a bola.
5. **O overlay** (`importOverlayBytesText`, PURA e aferível):
   total=0 → «a copiar…» (arranque) / «copiando… N B» (a fonte corre,
   o provider mentiu no length); total<1 MB → «N / M B»; ≥1 MB →
   «N / M MB». O frame desenha por ela; nunca «0 / 0» em fase nenhuma.

## 4. As regressões (verde) e as mutações (vermelho→verde)

**Regressões** (todas verdes, Release):

- `test_blockmesh.cpp` +7 TESTs SAF-SEAM: **fonte mmap-fd com ZERO
  ranges** (o FdCountStorage serve memfds e conta os `readBytesAt` —
  a abertura por mmap-fd NÃO toca nos ranges); **o pipe** (fstat 0 B +
  pread ESPIPE → degradação LOGADA + fonte=ranges + a abertura VERDE);
  **a tabela truncada** → quarentena (err com «asset corrompido,
  reimporta», o `.corrupt` com os bytes exatos, o original fora);
  **os materiais além do fim** (patch do materialTableOffset +
  checksum FNV recalculado — o peek segue verde) → quarentena; **o
  peek inválido SEM quarentena** (o lixo fica no sítio); **a falha de
  I/O** (o readBytesAt engasga na tabela) → err com «faixa da TABELA»
  + «ficheiro visto com N B» e SEM quarentena.
- `test_wiring087.cpp` +2 TESTs (o TU que inclui o main.cpp — o
  caminho do device): **o overlay nunca 0/0** (a janela inicial, o
  «copiando… 5243003 B», o PIN do 29 MB «6 / 29 MB», o sub-MB em B, a
  SEQUÊNCIA copy→cut→assembly de um import de 29 MB com corte de 72k,
  e o JOB REAL do arranque ao fim); **o gmesh truncado pelo caminho
  REAL do apply** (import pequeno → mesh anterior no TIC; o «grande»
  construído por patch do vertexCount + truncatura; o pick → o TIC
  MANTÉM o mesh/path/primOn, o toast e o engine.log dizem «asset
  corrompido, reimporta», o `.corrupt` vive, o catálogo re-listou).
- Sentinela **R-043** (test_sentinels.cpp): o rename do primitivo nas
  TRÊS camadas (Fs atómico no disco; Fake o mapa; Saf a cópia streaming
  com a cache de URIS a seguir o nome) + o ausente → false honesto.
- **c33 FASE 23** (+38 checks, 743→781): o city importando sob
  `content://` E ABRINDO por blocos com `fonte=mmap-fd` (a linha do
  load — o SEAM fechado); o chip «bl 72/72» pela frame REAL sob SAF; o
  overlay (o pin do 29 MB); o provider que recusa o mmap (a abertura
  VERDE por ranges, o fd-0-B LOGADO); o truncado em QUARENTENA com o
  rename streaming do SAF (o `.corrupt` com os bytes exatos, o
  original fora, o lastMeshError com a mensagem).

**Mutações** (contra o código final, cp dos backups, todas VERMELHAS):

- **M-SS1** (a abertura por caminho apenas — o `openReadFd`
  desligado): `blockmesh_fonte_mmapfd…` VERMELHA (o pin «ZERO ranges»
  falha — 3 ranges; a linha `fonte=mmap-fd` desaparece) +
  `blockmesh_pipe…` VERMELHA (a degradação deixa de ser logada) + c33
  com 10 checks VERMELHOS (o 23.1 «A FONTE da ABERTURA» entre eles).
- **M-SS2** (o total SÓ do length — a linha de sempre, incondicional
  em MB): `wiring087_overlay_import_nunca_zero_sobre_zero` VERMELHA +
  c33 23.3 com 5 checks VERMELHOS (o «0 / 0» VOLTOU).
- **M-SS3** (a TROCA SILENCIOSA PELA BOLA — o ramo de falha do
  applyAssetPick arma a esfera e diz «mesh aplicado»):
  `wiring087_gmesh_truncado_picker_mantem_estado…` VERMELHA (o mesh
  anterior é retirado, o primOn liga, o toast mente).
- Todas repostas → **test_core 0 falhas · c33 781/781 HARNESS VERDE**.

## 5. As linhas contrato novas (o que o dono vê no engine.log)

```
gmesh: fase=load ms=12.3 (por BLOCOS: tabela de 72 blocos, 3424 B lidos, fonte=mmap-fd; dados=2736000 B por carregar em lazy)
blocos: 'assets/x.gmesh' — o fd de '…' veio com 0 B (pipe/stream do provider?) — o pread decide, e a seguir os ranges
blocos: '…' — a leitura da faixa da TABELA [2563392, 2569152) falhou no fd — errno=29 (Illegal seek); …; a degradar para RANGES pelo storage
blocos: materiais de '…' lidos NOME A NOME (1 nome(s) — sem tamanho do ficheiro sob este provider, a sonda mínima não caberia)
saf: quarentena assets/x.gmesh -> assets/x.gmesh.corrupt (copia streaming por fd — a interface SafIo nao tem rename)
blocos: 'assets/x.gmesh' ASSET CORROMPIDO — em quarentena 'assets/x.gmesh.corrupt' (os bytes ficam; a tabela de blocos acaba além do fim (…); ficheiro visto com 2566272 B) — REIMPORTA o ficheiro original
editor: pick FALHOU — asset em QUARENTENA (assets/x.gmesh: asset corrompido, reimporta — …) — o TIC mantém o mesh anterior; REIMPORTA o ficheiro original
```

O overlay no ecrã: «a copiar…» → «copiando… 5243003 B» (se o provider
mente no length) → «6 / 29 MB» … «29 / 29 MB» (o pin) → «0 / 72000 B»
… «72000 / 72000 B» (o corte) — NUNCA «0 / 0».

## 6. Gates + CI (no push deste commit)

- **test_core** (Release, hospedeiro): **0 falhas** (com os +9 TESTs
  novos e a sentinela R-043).
- **c33_virtual**: **781/781 checks, HARNESS VERDE** (a FASE 23 nova).
- Gates: docs-lint · gmesh-docs (a âncora §SAF-SEAM) · release-identity
  **versionCode 61** (declarado no RELATORIO-0.9.6 §14) · scope (os
  ficheiros do hotfix em `ci/scope.txt`) · check_main · ui-vocab ·
  theme · hierarchy · gizmo · glyph · projects-ui · reload · link
  parity · jni-parity.
- **CI do push e610d0e — run 38035189359: 100% VERDE À PRIMEIRA**
  (core-tests ✓ · c33 781/781 com a FASE 23 ✓ · APK arm64 assinado ✓ ·
  symbol gate ✓). Artefacto publicado:
  **goni-vv-0.9.6-release-signed (versionCode 61)** — o APK que o dono
  instala para o sign-off do C33 (os 3 itens do §8).

## 7. O que NÃO foi tocado

O conversor (a cascata do SAF-STREAM intocada — a R-041 verde sem
recalibração), o ResourceManager (o guard de 192 B e a recusa do 3B),
o caminho do mesh único, o lazy/LRU do PASSO 4 (o contrato de cada
bloco é o `readBytesAt` de sempre), o `applyAssetPick` (o contrato
puro — só o DISPATCH ganhou o relatório da falha), o serializer, os
TICs/V.ONI/física/seleção, o EXPANDIR-NÓS (a R-042 verde sem
recalibração). O OBJ gigante continua no BACKLOG (0.10-M PASSO 3-b).

## 8. NÃO VERIFICADO (honesto — só o device humano afere)

1. **O city REAL de 130 MB num projeto SAF**: importar e APLICAR — o
   engine.log com a linha `gmesh: fase=load … fonte=mmap-fd` (o par da
   `asset: v3 fonte=mmap-fd` do conversor) + o chip «bl n/m» no HUD a
   mudar com o orbit.
2. **O overlay do GLB de 29 MB do dono**: os MB REAIS a subirem (nunca
   «0 / 0»; «a copiar…» no arranque é o esperado).
3. **Um asset corrompido a sério** (copiar um .gmesh e cortá-lo): o
   toast «asset corrompido, reimporta», o `x.gmesh.corrupt` na pasta
   assets/ do projeto e o picker do Inspector a MANTER o mesh anterior.

## 9. PÁRA

O PASSO 5 (texturas ASTC) continua BLOQUEADO à espera do OK explícito
do dono (depois o PASSO 6 — a prova de mesa no C33; após o 6, STOP
absoluto do 0.10-M). Nada além deste hotfix foi tocado.
