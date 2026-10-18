# RELATÓRIO 0.10-M · PASSO 3B — LOG + CAUSA DA IMPORTAÇÃO

Fase: **0.10-M PASSO 3B** (hotfix de commit próprio, destravado pelo dono).
O que o PASSO 3B manda: (1) diagnosticar o log viewer stale (writer vs
viewer) e curar a causa REAL; (2) timings por fase no conversor e no load
(`gmesh: fase=<parse|cut|assembly|verify|load|render> ms=<n>`); (3)
confirmar a causa do fail do modelo de 203 MB — se é o load inteiro
pós-conversão, REGISTAR e não contornar (o PASSO 4 é a cura; até lá a
mensagem diz «memória insuficiente ao carregar mesh (cura no PASSO 4:
render por blocos)»); (4) o export de logs é a mesma fonte de verdade do
writer. Provas: mutações (viewer com ordem stale → vermelho · writer sem
banner de boot → vermelho) · suítes+gates verdes · **PÁRA no fim**.

**O contexto do device (dono, C33, vc 56)**: o dragão de 32 MB importa em
~2 min; o modelo de 203 MB falha após ~3,5 min com «ver causa no log»; o
log viewer mostra conteúdo de ONTEM. Nenhum contorno sem medir primeiro —
tudo abaixo foi medido antes de curado.

---

## §1 · O DIAGNÓSTICO DO VIEWER STALE (item 1 — a ordem certa)

O dono listou 5 hipóteses (banner no boot? rotação a descartar o dia?
offset persistido? errno no logcat? caminho trocado?). O diagnóstico
MEDIDO eliminou quatro e criminosou a quinta em versão diferente:

| hipótese | veredicto (código + prova) |
|---|---|
| banner de boot escrito a cada arranque? | **SIM, correto** — main.cpp:6690 (changelog) + main.cpp:6765 (`buildinfo::banner()`) + `[boot N/6]` em 6 passos; o harness FASE 19.1 lê-os no log de HOJE |
| rotação de 1 MB a descartar o dia? | **A rotação funciona** (3 ficheiros, o mais velho sai) — o dia NÃO é descartado pela rotação em si |
| tail com offset persistido errado? | **NÃO existe offset persistido** — o viewer relê a cada abertura (main.cpp:5483/6033) |
| elog a falhar a abrir (errno no logcat)? | **NÃO** — o `openLocked` loga o errno no logcat E no stderr; os caminhos JNI_OnLoad (fallback) e android_main (canónico) são O MESMO diretório (`Android/data/vv.goni/files/logs`) |
| caminho trocado após storage recheck? | **NÃO** — o `init` é chamado 2× com o mesmo caminho (o fd reabre) |

**A CAUSA REAL estava no LEITOR**: `elog::readTail` (EngineLog.cpp) lia a
rotação na ordem **`.2 → .1 → ativo`** com `break` ao encher a janela de
300 linhas. Com 1+ rotação (1 MB por ficheiro — as linhas de 2 KB do
import enchem-no depressa), o backup MAIS ANTIGO enchia a janela sozinho
e **o ficheiro ATIVO — o de HOJE, com o banner do boot e os erros da
sessão — nunca era lido**. O dono abria «Ver logs» e via ONTEM. O
comentário do código dizia «o histórico recente é o que importa» — a
implementação entregava o MAIS ANTIGO. Os testes existentes
(`elog_readtail_*` do test_storageperm) nunca criaram a condição do device
(backs pequenos com ~4 linhas); o harness usa `readTail(3000)` — por isso
o CI nunca viu o bug.

**A CURA** (EngineLog.cpp · `readTail`): o ATIVO lê-se PRIMEIRO; só
quando FALTAM linhas se completa com o `.1` e depois o `.2`, sempre pela
CAUDA (as linhas mais recentes de cada backup), prependando — o resultado
é sempre as ÚLTIMAS `maxLines` linhas em ordem cronológica, e as linhas
de HOJE ganham sempre.

---

## §2 · A CAUSA DO «FAIL DE 203 MB» (item 3 — registada, NÃO contornada)

O caminho do load pós-conversão: aplicar ao TIC → `GpuAssets::mesh` →
`ResourceManager::mesh` (branch `.gmesh`) → `storage_->readBytes` **lê o
ficheiro INTEIRO** → `readGMesh` **materializa TODOS os blocos num único
`MeshData`** (índices u16) → upload GPU do mesh inteiro. É o «load do
mesh inteiro» — confirmado em código E medido:

**A MEDIÇÃO** (nova: `medicao_010m_load_pos_conversao_a_parede_do_mesh_
unico`, filho fork com VmHWM reiniciado): a scene-213MB importa
verificado=1 (o .gmesh sai a **972 MB**) e o LOAD recusa o mesh único
(30 M verts > 65 535). A 1ª versão da medição (guard só dentro do
`readGMesh`) mostrou o tamanho do muro:

```
[1ª versão — guard DEPOIS do readBytes]
  LOAD: ms=898 | RAM pico do load=2007 MB (base 38) | mesh=RECUSADO
  → ler 972 MB custou ~2 GB de RAM (o readBytes inteiro + a cópia
    string→vector do FsStorage::readBytes). NO C33 A APP MORRIA ANTES
    DE MOSTRAR A MENSAGEM — «ver causa no log» que o dono nunca via.
```

**O REGISTO + O GUARDO CEDO** (a mensagem chega viva):
- `readGMesh` (GOwnFormats.cpp) recusa com a mensagem que o dono pediu —
  **«memória insuficiente ao carregar mesh (cura no PASSO 4: render por
  blocos)»** — com os números (vértices, blocos, índices, ~MB da
  estimativa) e o aviso de que O FICHEIRO ESTÁ CORRETO (a conversão
  verificou-o bit a bit). O orçamento declarado
  `kMeshLoadBudgetBytes` = **256 MB** (o número da casa do teto de range
  R-032) + a estimativa pura `gmeshV3LoadEstimateBytes` (verts×32 +
  índices×2 + pele 20 B/vértice) cobrem o caso «poucos vértices, índices
  gigantes» com a MESMA mensagem.
- O **espião de 192 B**: `ResourceManager::mesh` lê só header+meta
  (32+160 B) pela leitura por range NOVA (`ProjectStorage::readBytesAt` —
  FsStorage fopen+fseek+fread; SafStorage fd+lseek+read; FakeStorage
  fatia; default correto readBytes+fatia) e valida com
  `gmeshV3PeekMeta` (o MESMO `v3ReadMeta` do caminho inteiro — zero
  drift de layout; o checksum do header cobre os 160 B de meta, logo o
  peek é a validação COMPLETA do header+meta). Recusa ANTES do readBytes:

```
[versão final — o guard CEDO]
  LOAD: ms=0.0 | RAM pico do load=39 MB (base 39) | mesh=RECUSADO
  → a recusa custa 0 MB EXTRA. A 1ª medição: 2007 MB.
```

- `FsStorage::readBytes` deixou de DUPLICAR (o caminho antigo lia para
  `std::string` e copiava para o vector: pico ~2× o ficheiro; agora:
  stat → resize UMA vez → fread direto) — vale para TODOS os reads.
- **NÃO é contorno**: o modelo de 203 MB CONTINUA sem abrir — só a
  mensagem honesta é entregue sem incendiar a RAM. A cura de verdade é o
  PASSO 4 (render por blocos — a travessia da tabela com AABBs por bloco
  já existe no formato; `readBytesAt` é a semente do carregamento por
  blocos).

**A SEGUNDA METADE da cadeia partida**: o `LOGE` do `GpuAssets` só falava
com o LOGCAT (que o dono não tem PC para ler) — o `err` do load (com a
mensagem do PASSO 4) morria SEM entrar no engine.log que o viewer/export
mostram. Agora o `GpuAssets::mesh` loga `gpu: '<ref>' FALHOU ao carregar —
<causa completa>` pelo elog (logcat + ficheiro), e o upload ganhou a sua
linha `gmesh: fase=render ms=`.

---

## §3 · OS TIMINGS POR FASE (item 2 — a linha contrato)

| fase | onde nasce | linha |
|---|---|---|
| cópia | AssetConverter.cpp · `importFile` | `import: copia ms=<n>` |
| parse | AssetConverter.cpp · `convertGltfCommon` | `gmesh: fase=parse ms=<n>` |
| cut | idem (do `V3StreamResult` do streaming) | `gmesh: fase=cut ms=<n>` |
| assembly | idem (streaming) + o caminho merge (novo cronómetro) | `gmesh: fase=assembly ms=<n>` |
| verify | idem | `gmesh: fase=verify ms=<n>` |
| load | ResourceManager.cpp · `mesh` (sucesso E recusa) | `gmesh: fase=load ms=<n>` |
| render | GpuAssets.cpp · `mesh` (o upload GPU) | `gmesh: fase=render ms=<n>` |

Os ~2 min do dragão e os ~3,5 min do modelo de 203 MB passam a ter dono
por fase no engine.log de HOJE (o viewer mostra-os — §1). A linha
agregada «asset: v3 streaming completo» fica (histórico dos relatórios).

---

## §4 · O EXPORT = A MESMA FONTE (item 4)

A Java (`VvActivity.exportLogsToDownloads`) copia TODOS os ficheiros de
`getExternalFilesDir("logs")` — o MESMO diretório do writer por contrato
Android (`externalDataPath` == `getExternalFilesDir(null)`) — direto do
disco, sem intermediários: nunca foi stale. O que faltava era PROVA no
CI: o stub JNI ganhou o hook `export_logs` (o padrão `bridge_*` da casa)
que espelha o loop REAL (listar o diretório do writer, copiar CADA
ficheiro, substituindo por nome). A FASE 19.5 prova o caminho INTEIRO:
toque REAL no botão «Export» (rect do registo de audit) → a ponte JNI
chamada com `kDownloadsRelPath` → o engine.log exportado traz a linha de
erro de HOJE, a causa do load e os tempos por fase.

---

## §5 · OS FIXES (ficheiro · função)

| # | ficheiro · função | o que mudou |
|---|---|---|
| F1 | platform/EngineLog.cpp · `readTail` | A ORDEM: ativo primeiro; backups só quando faltam linhas (pela cauda, prependados). O bug do «log de ontem» morreu |
| F2 | assets/GOwnFormats.h/.cpp · `readGMesh` + `gmeshV3LoadRefusalErr` + `gmeshV3LoadEstimateBytes` + `kMeshLoadBudgetBytes` | a recusa com a mensagem do dono + números; o orçamento declarado 256 MB; a estimativa pura exportada |
| F3 | assets/GOwnFormats.cpp · `gmeshV3PeekMeta` | o espião de 192 B (header decodificado à mão — o gReadHeader recusa payloadSize > len; o MESMO `v3ReadMeta` partilhado) |
| F4 | core/ProjectStorage.h/.cpp · `readBytesAt` (novo virtual) | a leitura por range com default correto (readBytes+fatia) |
| F5 | core/FsStorage.cpp · `readBytesAt` + `readBytes` | o range exato (fopen+fseek+fread) + o fim da duplicação (stat → resize único → fread direto) |
| F6 | core/SafStorage.cpp · `readBytesAt` | o range exato pelo fd do SAF (openFd+lseek+read) |
| F7 | tests/FakeStorage.h · `readBytesAt` | a fatia em memória (o harness exercita o guard cedo) |
| F8 | assets/ResourceManager.cpp · `mesh` | O GUARDO CEDO (peek → recusa por 192 B) + `gmesh: fase=load` no sucesso E na recusa |
| F9 | render/GpuAssets.cpp · `mesh` | o err do load CHEGA ao engine.log (elog, não LOGE-logcat-only) + `gmesh: fase=render ms=` |
| F10 | assets/AssetConverter.cpp | `gmesh: fase=parse/cut/assembly/verify ms=` + `import: copia ms=` (o cronómetro do merge também) |
| F11 | tests/stub/jni.h · `export_logs` (hook) | o espelho do loop Java do export (o padrão bridge_*) |
| F12 | tests/test_logs_crash.cpp | `log_readtail_o_ativo_de_hoje_ganha_as_rotacoes` (a reprodução do device: 20 KB/ficheiro, ~350 linhas/backup contra a janela de 300) + `log_readtail_sem_rotacao_le_o_ativo` |
| F13 | tests/test_gmeshv3stream.cpp | o 70k atualizado (mensagem nova + o espião + readBytesAt ranges + a recusa do ResourceManager) + `v3stream_estimativa_de_load_e_orcamento` (a fórmula pura) |
| F14 | tests/test_010m_medicoes.cpp | `medicao_010m_load_pos_conversao_a_parede_do_mesh_unico` (a parede medida: import ok + load RECUSADO barato + a causa no err) |
| F15 | tests/c33_virtual.cpp · FASE 19 | +39 checks: o banner do boot ATUAL no viewer (com audit do draw), a rotação no viewer REAL (dir próprio), o fail de import visível no viewer E no export, o 70k que importa e recusa no apply com a mensagem do PASSO 4, os tempos por fase por DELTA, o export pela ponte JNI REAL |
| F16 | docs/GMESH_formato.md | §6 (guard cedo + tempos por fase + readBytesAt) e §ponte-para-o-runtime (a mensagem, o orçamento, a estimativa) |

---

## §6 · AS MEDIÇÕES (medido, não estimado — Release, sandbox x86_64)

| medição | número |
|---|---|
| load da scene-213MB (972 MB .gmesh) — 1ª versão (guard tarde) | ms=898 · **RAM pico 2007 MB** (base 38) |
| load da scene-213MB — final (guard cedo, espião 192 B) | **ms=0.0 · RAM pico 39 MB (base 39) — 0 MB extra** |
| import da scene-213MB (o de sempre, R-039-family) | cópia≈0 · parse 2 ms · corte 4831 ms · assembly 2994 ms · total 13321 ms · pico 55 MB · verificado=1 |
| import do 70k-verts no harness (FASE 19.4) | ~106 ms (com verificação bit a bit) |
| R-039 (a sentinela, inalterada) | 1754 MB → 31,1 s · pico 107 MB · blocos 920 · verificado=1 |

Leitura honesta: a conversão está CURA (a promessa do PASSO 3 mantém-se);
a parede do «fail de 203 MB» é o LOAD do mesh inteiro (agora recusado com
mensagem e números, por 192 B); no C33 os ~2 min/~3,5 min do dono passam
a ter dono por fase na PRÓXIMA sessão real (as linhas contrato estão no
caminho de produção).

---

## §7 · AS SUÍTES E OS GATES (o estado no fim)

| suíte / gate | resultado |
|---|---|
| test_core | **0 teste(s) com falha** (incl. os 2 novos do readTail + o 70k estendido + a estimativa + a medição do load) |
| c33_virtual | **670 check(s), 0 falha(s) — HARNESS VERDE** (era 631; a FASE 19 soma 39) |
| release-identity (R-015) | VERDE: versionName 0.9.6 · **versionCode 57** (declarado no RELATORIO-0.9.6.md) |
| scope-check (P-01) | VERDE: todo o diff (24 ficheiros) dentro de ci/scope.txt |
| o CI do GitHub (run 37849657818, commit f09792d) | **VERDE — todos os jobs + o APK assinado versionCode 57 publicado** (o 1º push (e646803) morreu num ICE do GCC 13 do runner em gimplify.cc:774 — o construto «meta = GMeshV3Meta{}» de sempre, empurrado ao bug pelo TU novo; cura sem mudança de semântica: v3Reset() explícito + a mensagem por appends — commit 0.10.3b) |
| docs-lint / gmesh-docs / ui-vocab / theme-hex / hierarchy / gizmo / glyph / jni-parity / reload / check_main | VERDES |

---

## §8 · AS MUTAÇÕES (vermelho → verde, coladas — contra o código FINAL)

### M-A — a ordem stale do readTail reposta (o «log de ontem»)

Mutação: o `readTail` inteiro reposto pelo do HEAD (`.2 → .1 → ativo`
com break).

```
[VERMELHO] log_readtail_o_ativo_de_hoje_ganha_as_rotacoes FALHOU
           (test_logs_crash.cpp:201 hasToday; :205 out.back();
            :212 out3[0]; :213 out3[2]) — 4 teste(s) com falha
           → as linhas de HOJE NUNCA entravam na janela (o bug reproduzido)
[VERDE após reposição] 0 teste(s) com falha
```

### M-B — o banner de boot calado (o writer sem boot)

Mutação: o `elog::info("[boot 1/6] contentRect OK …")` do INIT_WINDOW
comentado (main.cpp).

```
[VERMELHO] 19.1 o viewer mostra o [boot 1/6] do boot ATUAL — [FAIL]
           == C33 VIRTUAL: 670 check(s), 1 falha(s) ==
[VERDE após reposição] 670/670 HARNESS VERDE
```

---

## §9 · NÃO VERIFICADO (honesto)

- **O device real (C33)**: o PASSO 3B correu todo na sandbox (x86_64,
  Release) e no dispositivo virtual. O sign-off real do dono: após o
  arranque, o viewer mostra o banner do boot ATUAL; provocar um fail de
  import = linha de erro de HOJE visível no viewer E no export. O APK do
  versionCode 57 é o artefacto para o dono provar.
- **A composição real do modelo de 203 MB**: os ~3,5 min do dono não
  foram ainda atribuídos POR FASE no device — as linhas contrato estão no
  caminho de produção e a próxima sessão real do dono dá os donos (a
  medição aqui usa os perfis declarados; o FAIL exato do dono pode ser
  conversão, storage ou LMK — agora visível no log de HOJE).
- **O efeito do guard cedo com storage SAF real**: o `readBytesAt` do
  SafStorage segue o padrão fd+lseek+read mas só o FakeSafIo o exercitou
  (o content:// real fica por provar no device).
- **O export REAL da Java**: o hook do stub espelha o loop da
  VvActivity.exportLogsToDownloads; o MediaStore real (API 29+) fica por
  provar no device (o código Java não mudou — a paridade é por contrato
  Android: getExternalFilesDir("logs") == o diretório do writer).
- **O PASSO 4 NÃO começou**: o render por blocos fica BLOQUEADO à espera
  do OK explícito do dono (PÁRO). O `readBytesAt`/`gmeshV3PeekMeta` são
  infraestrutura de diagnóstico/load — não renderizam nada por blocos.

---

## §10 · PÁRO

O mandato manda parar no fim do PASSO 3B (commit próprio). O PASSO 4
(render por blocos), o PASSO 5 (texturas) e o PASSO 6 (a prova no C33
real) ficam **à espera do OK explícito do dono** — a cura do load inteiro
é o PASSO 4; o registo da causa (mensagem + números + espião de 192 B)
é o que ESTE passo entrega.
