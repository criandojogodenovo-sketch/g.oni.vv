# RELATÓRIO 0.6.7 — Sub-fase: lifecycle GL + gestão de projetos

> Campanha 0.6.7 → 0.6.9 · Sub-fase 1 de 3 · Release 0.6.7 (versionCode 16)

## 1. Objetivo

Corrigir a classe de bugs de lifecycle GL que deixava letras/números como
quads brancos ("cubinhos") ao sair do editor e reentrar sem matar a app;
dar ao launcher a capacidade de APAGAR projetos (com confirmação explícita)
e ao editor a saída "Sair para projetos" (auto-save + volta ao gestor sem
matar a app). Fix adicional reportado pelo dono: o nome do projeto digitado
no diálogo do gestor não estava visível durante a digitação.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `7ab80ee` (f5.4-hotfix — release 0.6.5, versionCode 15)
- Suíte baseline: 279 testes verdes no Linux (CI)
- Problemas abertos confirmados na leitura do código:
  - `FontAtlas.cpp` L58: `if (tex_) return true;` — o guard impedia o
    re-upload após a recriação do contexto EGL (id stale);
  - `GpuAssets::releaseAll()` existia mas NUNCA era chamado do main;
  - `APP_CMD_TERM_WINDOW` só libertava cubo/grid/renderer;
  - sem "apagar projeto" no gestor; sem "sair" no editor;
  - `ProjectManagerActivity.askNewProject()`: EditText com
    `setTextColor(0xFFE6E6E6)` sobre AlertDialog do tema claro do manifest
    (texto branco sobre painel branco).

## 3. Causas raiz (com evidência)

**Cubinhos (a):** `EglContext::shutdown()` (platform/EglContext.cpp L71–85)
destrói surface E contexto — TODOS os ids GL morrem. O TERM_WINDOW antigo
não destruía `g_font` nem os mapas do `g_gpu`; no INIT_WINDOW seguinte o
guard `if (tex_)` saltava o re-bake e `GpuAssets::mesh/texture` devolviam
objetos com handles órfãos. O pass UI (`UiContext::endFrame`) submetia os
glifos com a textura stale → quads brancos; o pass 3D desenhava contra ids
inválidos (GL_INVALID_OPERATION silencioso) e os ponteiros `Mesh*` dos
MeshRenderers podiam ficar pendulares (use-after-free latente).

**Nome invisível (d):** o manifest aplica `Theme.NoTitleBar.Fullscreen`
(tema claro do framework) à app; o `AlertDialog.Builder(this)` herda-o →
painel branco. O EditText só definia `setTextColor` quase-branco.

## 4. Implementação (o que mudou, por commit)

- **0.6.7-a** (`5373238`) — lifecycle GL: `FontAtlas::destroy()` novo;
  TERM_WINDOW com ordem obrigatória (detach → `g_gpu.releaseAll()` →
  `g_font.destroy()` → cubo/grid → renderer → EGL por fim);
  `detachRenderersFromGpu()` no main (política: `&g_cubeMesh` é estático e
  re-criado no sítio — mantém-se; meshes/texturas do GpuAssets são
  apagados → nullptr; as refs relativas `meshPath/texPath` ficam como
  fonte da verdade para o re-bind); INIT_WINDOW loga "contexto
  RE-CRIADO" (contadores `g_windowInits/g_windowTerms`) e confirma o
  RE-UPLOAD da fonte. Stub GLES3 ganha contadores `glstub::stats`;
  `GpuAssets.cpp` entra na suíte (faltava).
- **0.6.7-b** (`76d7e73`) — gestor: `VvProjects.deleteProject` (File API —
  `DocumentsContract.deleteDocument` no doc URI da raiz + remoção da
  lista); long-press com DUAS ações ("Remover da lista" neutro /
  "Apagar projeto" destrutivo com confirmação explícita separada);
  `releasePermission` extraído; fix do nome invisível (fundo escuro
  0xFF1E222A + texto claro + hint + `requestFocus()` pós-show).
- **0.6.7-c** (`60eef55`) — "Sair para projetos": item 6 do Menu;
  `jniFinishToLauncher()` na ponte JNI; `VvActivity.bridgeFinish()`
  (`runOnUiThread(finish())`); auto-save no handler (cena + manifesto +
  `persistSceneAssets`); **reset de reentrada no android_main**:
  `TickGroups::clear()` novo (a física seria registada 2× e daria dois
  passos por frame), `InputState::resetAll()` novo, cena/editor/play/
  projeto/catalog/toast/orbit resetados, texCache/pipeline libertados antes
  do storage antigo.
- **0.6.7-d** (este commit) — release: bump versionCode 16 / versionName
  0.6.7, artifact CI `goni-vv-0.6.7-release-signed`, banners coerentes
  (android_main, JNI_OnLoad, runner de testes — que ainda dizia 0.4.2),
  README com escopo 0.6.7 + roteiro C33, este relatório.

## 5. Decisões técnicas relevantes

1. **Re-bake total vs. re-bind:** o `ResourceManager` (cache CPU de
   MeshData) NÃO é libertado no TERM_WINDOW — pouso re-parse dos assets
   (o cache é CPU, não GL); só os objetos GL (`GpuAssets`) são
   re-uploadados. As refs relativas do serializer re-bindam tudo.
2. **Política do cubo procedural:** `&g_cubeMesh` é um objeto com duração
   estática re-criado no sítio — os MeshRenderers que o apontam ficam
   válidos; os objetos do `GpuAssets` são apagados → detach obrigatório.
3. **A cena é recarregada do disco em cada INIT_WINDOW** (comportamento
   pré-existente mantido — os resolvers re-upam os assets). Alterações
   não salvas perdem-se num ciclo term/init, como antes; o novo
   "Sair para projetos" FAZ auto-save antes de sair.
4. **bridgeFinish é diagnóstico (não crítico)** no handshake — falhar não
   bloqueia o editor (toast honesto), ao contrário dos 6 mids SAF
   críticos cuja ausência mata a ponte.
5. **Contadores de janela são do PROCESSO** — não são resetados na
   reentrada do android_main: o INIT da reentrada loga "contexto
   RE-CRIADO", que é a verdade.

## 6. Testes novos (CI Linux)

`tests/test_lifecycle.cpp` (8 casos, 279 → 287):

1. `lifecycle_atlas_re_uploadado_apos_term_e_init` — texImage2D 2×,
   genTextures 2×, deleteTextures 1, métricas re-medidas;
2. `lifecycle_atlas_guard_impede_upload_duplicado_no_mesmo_contexto` —
   2ª carga SEM term = 1 upload só (guard intacto);
3. `lifecycle_atlas_glifos_uvs_validos_apos_re_upload` — todo o range
   ASCII com UV coerentes (o espaço sem tinta é `>=`);
4. `lifecycle_mesh_destroy_recreate_no_novo_contexto`;
5. `lifecycle_gpu_release_all_e_re_upload_dos_assets` — meshCount
   1→0→1, objeto GL novo (o ponteiro antigo pode ser reutilizado pelo
   allocator — a aferição é o ciclo, não o endereço);
6. `lifecycle_renderer_shutdown_e_reinit` — 2 programas criados,
   endFrame com batches vazios sem crash;
7. `lifecycle_sair_reentrar_cena_preservada_e_meshes_religados` — fluxo
   completo: 2 TICs com poses → save → destroy/re-create do cubo → reload
   → nomes/poses preservados + MeshRenderers re-ligados (tag "cube" →
   LoadCtx);
8. `lifecycle_sair_para_projetos_ponte_java_chegada` — registo fake da
   activity → `jniFinishToLauncher()` → a chamada chega à Java como
   `bridgeFinish` (void_calls do fake JNI).

## 7. Commits da sub-fase (branch main)

| Commit  | Assunto |
|---------|---------|
| 5373238 | 0.6.7-a: LIFECYCLE GL — fix dos "cubinhos" do C33 |
| 76d7e73 | 0.6.7-b: GESTÃO DE PROJETOS no launcher — APAGAR + nome visível |
| 60eef55 | 0.6.7-c: 'SAIR PARA PROJETOS' no editor + reset de reentrada |
| (este)  | 0.6.7-d: release 0.6.7 (bump, banners, README, relatório) |

## 8. HEAD da sub-fase

- HEAD de fecho: ver `git log -1` no push desta sub-fase (release 0.6.7,
  versionCode 16).

## 9. Suíte de testes

- **287 testes, 287 OK, 0 falhas** (Linux, suíte `test_core`; baseline
  era 279 — +8 novos do lifecycle).
- Gates locais verdes antes do push: `check_main.sh` OK;
  `link_parity.sh` OK (70 TUs da app); `jni_parity.py` OK (3 natives,
  paridade Java↔tabela).

## 10. CI

- Job `core-tests` (Linux): cmake + ctest + check_main + link_parity +
  jni_parity — **verde**.
- Job `build-release` (NDK r26): `assembleRelease` + `apksigner verify`
  — **verde**; artifact `goni-vv-0.6.7-release-signed`.
- Job `verify-entry-symbols`: `nm -D` exige `ANativeActivity_onCreate` +
  `android_main` + `JNI_OnLoad` + os 3 `Java_vv_goni_VvActivity_*`;
  `jni_parity.py dynsyms.txt`; gate do manifest binário (hasCode=true,
  ProjectManagerActivity launcher, VvActivity, lib_name=goni_vv) —
  **verde**.

## 11. APK

- `app-release.apk` (arm64-v8a), versionCode 16, versionName "0.6.7",
  assinado com a keystore dos secrets (`VV_KEYSTORE`/`VV_STORE_PW`/
  `VV_KEY_ALIAS`/`VV_KEY_PW`) — artifact
  `goni-vv-0.6.7-release-signed` do workflow `release`.
- sha256 do APK assinado (artifact `goni-vv-0.6.7-release-signed`, run
  36717845547):
  `bb973a81a309e3261efc5ef71a183f6de56c81731ce2f1ebc8b87ecff1bd81a5`

## 12. Verificação no device (roteiro C33 — resumo)

1. **Lifecycle:** entrar num projeto → home → voltar → texto NORMAL
   (sem cubinhos); logs com `INIT_WINDOW #2 … RE-CRIADO` e
   `fonts OK … RE-UPLOAD no contexto novo`.
2. **Apagar:** long-press → "Apagar projeto" → confirmação → pasta some
   do gestor de ficheiros + entrada sai da lista; "Remover da lista"
   mantém a pasta.
3. **Sair:** Menu → "Sair para projetos" → gestor retoma sem reiniciar a
   app; cena auto-salva; reentrar → pose preservada, texto normal.
4. **Nome:** "+ Novo projeto" → digitação visível (campo escuro).

## 13. Riscos e mitigações

- **Providers SAF que recusam apagar a raiz:** o Toast é honesto
  ("pasta não apagada — entrada removida da lista") e o erro fica no
  logcat; a entrada sai da lista à mesma (pasta inacessível não é
  projeto utilizável).
- **Race do finish() com o loop da engine:** o finish é postado na UI
  thread; o loop da engine continua até o glue processar
  `destroyRequested` — sem GL depois do TERM (g_ready=false).
- **Reentrada com storage morto:** o fallback app-private cobre; o
  ProjectSlot sobrepõe (1 slot) e o timeout de 3s mantém-se.

## 14. Dívida técnica conhecida

- O banner do runner de testes estava stale ("0.4.2") desde a F5 —
  corrigido nesta sub-fase; manter o hábito de bump por release.
- A cena continua a ser recarregada do disco em CADA INIT_WINDOW (perde
  edições não salvas num ciclo term/init) — comportamento pré-existente,
  documentado; o auto-save do "Sair" cobre o caso novo.

## 15. Restrições respeitadas (CLÁUSULA CALMA)

- Zero física nova, zero componentes de jogo novos, zero render 3D novo.
- Tema mono intacto na UI 2D (o fix do nome usa o cinza escuro do tema).
- 3 botões da toolbar intactos; landscape; safe-area intactos.
- PlaySnapshot intacto (a sub-fase 0.6.8 é que o usa como base).
- Apagar projeto exige confirmação explícita (dois diálogos: ação →
  confirmação destrutiva).

## 16. Conclusão

A sub-fase fecha os critérios de aceitação 0.6.7: sair do editor e
reentrar sem matar a app deixa o texto normal (re-upload de TODOS os
recursos GL no re-init do contexto); o launcher apaga projeto com
confirmação; o editor tem "Sair para projetos" com auto-save; o nome do
projeto é visível durante a digitação. CI verde nos três jobs; APK
assinado no artifact. Segue para a sub-fase 0.6.8 (Play Mode com janela
própria).
