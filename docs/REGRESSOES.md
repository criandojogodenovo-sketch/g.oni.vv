# docs/REGRESSOES.md — REGISTO DE REGRESSÕES DE DEVICE (0.8.12)

O contrato desta casa: **CI verde não é prova** — entregas verdes que
falham no telefone repetidamente são inaceitáveis (0.8.5/0.8.7/0.8.9/
0.8.10). Este ficheiro é o REGISTO PERMANENTE dos bugs de device fixados:
cada entrada liga o SINTOMA exato (a linha de log que o dono viu) à causa
raiz (ficheiro/função), ao fix, ao teste sentinela que o vigia para sempre
e à linha do REPLAY do dispositivo virtual (executável `c33_virtual` em
CI) que o confirma limpo.

## POLÍTICA (a regra da casa)

1. **Todo fix de bug de device acrescenta uma entrada neste ficheiro + um
   teste sentinela** (caso `regress_*` no `tests/test_sentinels.cpp`, e
   quando o caminho é do main.cpp, um passo no replay do `c33_virtual`) +
   o padrão proibido correspondente em `ci/forbidden_log_patterns.txt`.
2. **Remover ou saltar uma sentinela exige aprovação explícita do autor**
   (o dono) e nota no relatório da release que apropria a mudança.
3. **Sentinela que nunca falha não é sentinela — é decoração**: cada
   sentinela nova vem com PROVA DE MUTAÇÃO colada no relatório (o fix
   revertido temporariamente → sentinela VERMELHA; reposto → VERDE).
4. O job de release (APK assinado) **depende** (needs:) das sentinelas, do
   gate de padrões proibidos e do harness do dispositivo virtual —
   sentinela vermelha = não há APK.
5. Campanhas futuras não podem desativar, skipar ou enfraquecer sentinelas;
   se uma sentinela precisar de mudar por mudança legítima de comportamento,
   o relatório explica e o autor aprova ANTES do merge.

---

## R-001 · perda de seleção entre o TIC e o picker

| campo | valor |
|---|---|
| ID | R-001 |
| Reportado | device 0.8.5, 0.8.7, 0.8.9, 0.8.10 |
| Sintoma exato | `mesh: troca - → prim esfera ERRO(sem TIC com mesh selecionado)` (e variantes `mesh pick 2/3`, `tex pick 2`); trocas com origem explícita davam `fim ok` — a seleção perdia-se entre selecionar o TIC e tocar no picker |
| Causa raiz | (a) `platform/main.cpp` chamava `editor::viewportTapClearsSelection` SEM guard de overlay — o tap na linha/backdrop do picker caía DENTRO do viewRect, armava o deselect no press e LIMPAVA a seleção no release do MESMO frame, ANTES do dispatch do pick (os botões da UI não reclamam o slot de input externo); (b) `APP_CMD_INIT_WINDOW` recarregava a cena (`loadActiveScene`) — os handles dos TICs morriam e `g_editor.selected` ficava stale (o Inspector limpava-o) |
| Fix | (a) guard `!editor::anyOverlayOpen(g_editor)` + `!audioMode` no chamador do deselect (main.cpp); (b) `editor::revalidateSelection()` — re-valida/re-mapeia por NOME após o load do INIT_WINDOW; (c) `editor::pickerGuardBlocked()` — guard no OPEN (Inspector) e no DISPATCH (main): sem alvo válido → hint `seleciona um TIC com mesh` + log `ui: pick bloqueado (sem seleção)`, nunca o caminho `ERRO(sem TIC…)` |
| Teste sentinela | `regress_selection_loss` (tests/test_sentinels.cpp) |
| Linha do replay | FASE 2 do c33_virtual (passos 2.2/2.4/2.8) + FASE 3 (lifecycle) |
| Padrão proibido | `sem TIC com mesh selecionado` (ci/forbidden_log_patterns.txt) |

Nota técnica: parte da intermitência da evidência vinha do
`camgizmo::pickSceneTic` — quando o TIC projetava sob o overlay, o MESMO
tap que matava a seleção re-selecionava o TIC e o pick "funcionava". O
replay usa o TIC AFASTADO do centro (o cenário em que o sintoma aparecia).

## R-002 · migração morta: staging em /tmp (read-only no Android)

| campo | valor |
|---|---|
| ID | R-002 |
| Reportado | device (log da migração; reportado nas séries 0.8.5–0.8.10) |
| Sintoma exato | `fileapi: mkdir falhou em '/tmp' errno=30 (Read-only file system)` → `asset: migracao … FALHOU` → `staging falhou: /tmp/goni_reconvert…` — a migração de projeto antigo morria no device porque o HOST de testes tem /tmp escrevível e o Android NÃO |
| Causa raiz | `assets/AssetConverter.cpp` `convert::reconvertFile` usava o LITERAL `"/tmp/goni_reconvert_<pid>.tmp"` no caminho SAF (raiz `content://` sem fopen) |
| Fix | `convert::stagingWrite` com FALLBACK em cascata: 1) `.staging/` DENTRO do projeto (raiz de ficheiros real — FsStorage do device); 2) CACHE DIR da app via JNI (`VvActivity.cacheDirPath()` → `getCacheDir()` — o único sítio garantido escrevível sem permissões); 3) erro LEGÍVEL com os caminhos reais tentados — JAMAIS /tmp. Gate de CI adicional: grep garante que nenhum literal `"/tmp`/`'/tmp` resta em FileApi/código de assets |
| Teste sentinela | `regress_tmp_staging` (tests/test_sentinels.cpp) |
| Linha do replay | FASE 1 (migração no boot com /tmp READ-ONLY) + FASE 4 (caminho content:// do SAF) do c33_virtual |
| Padrão proibido | `mkdir falhou em '/tmp'` e `staging falhou` (ci/forbidden_log_patterns.txt) |

## R-003 · `none` sem caminho seguro/serializável nos pickers

| campo | valor |
|---|---|
| ID | R-003 |
| Reportado | device (as séries do picker que não limpa / cai no erro) |
| Sintoma exato | a opção `none` nos TICs falhava (não limpava o slot ou caía no caminho de erro); o picker de MESH nem tinha entrada `none` (1º item era "cube (procedural)") |
| Causa raiz | `ui/EditorUi.cpp` `drawAssetMenu`/`applyAssetPick` (menuKind 1) sem entrada none; a limpeza não era de primeira classe (sem posse p/ deferred free, sem round-trip afervado) |
| Fix | `none` em 1º LUGAR nos pickers de mesh (1) e tex (2) (prim já tinha); `mesh: none` limpa o slot (mesh/material/paths/primOn) com posse `primRetire` para deferred free no início do frame seguinte (a cova — o MESMO caminho seguro das trocas; `primRetire = mesh ? mesh : primRetire` nunca perde pendente); serialização já gravava `"mesh":"none"` — agora afervado por round-trip |
| Teste sentinela | `regress_none_slot` (tests/test_sentinels.cpp) |
| Linha do replay | FASE 2 do c33_virtual (passos 2.5/2.6/2.7) |
| Padrão proibido | (coberto pelo R-001 — o `none` sem alvo dá hint, não ERRO) |

## R-004 · dump velho no viewer sem rótulo ANTIGO

| campo | valor |
|---|---|
| ID | R-004 |
| Reportado | device (o dono com o crash-1790830406.dump de outra build no ecrã) |
| Sintoma exato | o log viewer mostrava o dump VELHO (offsets idênticos, build antiga) SEM o rotular como antigo — impossível saber QUE build o produziu |
| Causa raiz | `platform/BuildInfo.cpp` `buildinfo::dumpIsFromOtherBuild` existia desde a 0.8.10 mas NUNCA era chamado pelo `drawLogViewer` (ui/EditorUi.cpp) — wiring morto |
| Fix | `buildinfo::dumpBadge()` (badge textual `[ANTIGO (build N)]` / `[ANTIGO (pre-0.8.10)]`) + chamada no `drawLogViewer` com a cor theme::WARN (exceção documentada ao tema mono, como a brand da toolbar); banner de boot no formato exigido `boot: G.One VV <versão> versionCode <N> sha256 <…> git <…>` |
| Teste sentinela | `regress_dump_identity` (tests/test_sentinels.cpp) |
| Linha do replay | FASE 5 do c33_virtual (dump da build 39 com badge; novo sem badge) |
| Padrão proibido | `ERRO(gerador/upload falhou)` (o erro desonesto da 0.8.7 — relacionado: com os guards, o caminho nunca é atingido pelos pickers) |
