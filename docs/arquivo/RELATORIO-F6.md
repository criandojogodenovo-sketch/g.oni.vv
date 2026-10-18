# RELATÓRIO F6 — Fix: seletor de textura do Inspector de verdade (C33)

## 1. Objetivo

Corrigir o wiring entre o seletor de textura do Inspector e o material do
TIC, para que escolher uma textura a aplique realmente (estado + render +
persistência). No C33 (0.6.9), após importar um PNG (import OK,
`textures/screenshot-….png` no projeto) e tocar em `tex:` no Inspector
escolhendo a imagem, nada acontecia: o estado permanecia `tex: none`, o cubo
continuava sem textura e o engine.log não registava nenhuma linha de
aplicação nem de erro. O seletor abria e aceitava o toque, mas a escolha não
chegava ao Material/MeshRenderer (handshake, permissão e import confirmados
OK pelos logs). Objetivos da sub-fase: (a) diagnosticar a desconexão;
(b) corrigir o wiring (escolher → referência no material → render binda →
Inspector reflete `tex: <nome>`); (c) garantir a persistência no `.goni`;
(d) opção de remover (`tex: none` liberta a referência); (e) logging
`material: textura aplicada <ref>` / `material: textura removida`; (f)
testes CI com stubs.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `01b8dba` (fecho da F5.5 — All Files Access de verdade;
  suíte 326 OK; release 0.6.9 com versionCode 18).
- O import de PNG funcionava no C33 (fix F5.5 confirmado VERIFIED): o
  ficheiro chegava a `textures/` do projeto e o catálogo do seletor listava
  o screenshot.
- O seletor abria (`st.assetMenu = 2`), desenhava a lista e o toque nos
  itens era aceite — mas nada se aplicava.

## 3. Arquitetura escolhida (diagnóstico primeiro)

**CAUSA RAIZ — bloco morto desde a F5-E (0.5.0), commit `a42641f`:**

- `drawAssetMenu` (ui/EditorUi.cpp) devolve a escolha `chosen` (1-based)
  E fecha o seletor NO CLIQUE — `st.assetMenu = 0` antes do `return`
  (mutação do próprio `g_editor` passado por referência).
- O dispatch no `main.cpp` `frame()` guardava `if (g_editor.assetMenu != 0)`
  (correto), chamava `drawAssetMenu` e SÓ DEPOIS verificava
  `g_editor.assetMenu == 1` / `== 2` para decidir mesh vs textura —
  mas nessa altura o valor JÁ ERA 0 → as duas condições eram sempre falsas
  → o bloco `if (pick > 0)` inteiro era **código morto**: a escolha era
  silenciosamente descartada.
- Sintoma exato do C33 explicado por esta única desconexão: o toque é
  aceite (botão volta `chosen`), o menu fecha, mas nenhum write chega ao
  `MeshRenderer` (texPath/texture intactos → `tex: none` eterno), o render
  nunca binda (cubo cinzento) e o engine.log não tem linha de aplicação
  NEM de erro (nenhum caminho do bloco corre — nem o de sucesso, nem o de
  falha).
- O seletor de **mesh** tinha o mesmo bug (mesmo bloco morto).
- Porquê o CI nunca apanhou: os testes de UI (`test_ui.cpp`) aferem que o
  botão do Inspector ABRE o seletor (`assetMenu == 1/2`) — o DISPATCH vivia
  no `main.cpp`, que compila nos gates de paridade mas não corre na suíte.

**Arquitetura do fix (padrão da casa — política pura e afervel):**

- `editor::applyAssetPick` NOVO (declaração em `ui/EditorUi.h`, corpo em
  `ui/EditorUi.cpp`, que compila NA APP E NA SUÍTE): função PURA que aplica
  a escolha no `MeshRenderer` do TIC selecionado, com resolvers INJETADOS
  (`AssetResolvers`: `mesh` / `texture` / `meshTextureFor` / `cubeMesh` /
  `material`) — GL-free, testável com stubs (o padrão de `StoragePerm.h` e
  `JniAttach.h`).
- O `main.cpp` captura o `menuKind` ANTES de chamar `drawAssetMenu` (o fix
  da ordem) e aplica o pick devolvido; `makeAssetResolvers()` liga os
  objetos reais de runtime (GpuAssets/ResourceManager/cubo procedural/lit
  do renderer).
- `AssetPickOutcome` devolve `applied` + `toast` + `log` — os efeitos
  colaterais de UI (toast) e de log (engine.log) ficam no chamador, a
  função mantém-se determinística e afervel.
- Nenhuma flag dirty necessária: o render é immediate-mode (`drawTics` lê
  `mr->texture` a CADA frame → o bind acontece no frame seguinte ao pick,
  automaticamente); a persistência já gravava `texPath` no `.goni`
  (`SceneSerializer`) e o `LoadCtx.resolveTex` já re-resolvia no load —
  só o wiring é que estava partido.

## 4. Implementação (por ficheiro, commit `f6-a`)

- **`app/src/main/cpp/ui/EditorUi.h`** — declara `AssetResolvers`,
  `AssetPickOutcome` e `applyAssetPick` com o contrato documentado (menuKind
  capturado antes do draw; pick 1 = cube/none; 2.. = catálogo; falha mantém
  estado anterior; TIC morto/sem MeshRenderer → outcome vazio). Comentario
  de cabeçalho com a causa raiz e o porquê de não haver flags dirty.
- **`app/src/main/cpp/ui/EditorUi.cpp`** — corpo do `applyAssetPick`
  (imediatamente após `drawAssetMenu`): mesh pick aplica
  mesh/material/meshPath + textura embutida do glTF/GLB quando existe
  (comportamento F5.1-B preservado); texture pick aplica `texture` +
  `texPath`; `none` liberta a referência; falhas preservam o estado
  anterior e reportam toast + linha `FALHOU`; logs do contrato:
  `material: textura aplicada <ref>` / `material: textura removida`
  (+ `editor: mesh … aplicado [com textura …]` para o seletor de mesh).
- **`app/src/main/cpp/platform/main.cpp`** — `makeAssetResolvers()` NOVO
  (ao lado do `makeLoadCtx`); o bloco morto de ~60 linhas substituído por:
  captura do `menuKind` ANTES do `drawAssetMenu` → `applyAssetPick` →
  `showToast(out.toast)` + `LOGI("%s", out.log)`; banner do android_main
  0.6.10.
- **`tests/stub/GLES3/gl3.h`** — `glstub::stats` ganha `boundTextures`,
  `lastBoundTexture`, `lastUniform1f` (glBindTexture e glUniform1f passam a
  gravar — inócuo para os outros testes, o mesmo padrão dos contadores
  introduzidos na 0.6.7-a).
- **`tests/test_assetpick.cpp`** NOVO — 11 casos (ver §6).
- **`tests/CMakeLists.txt`** — TU novo na suíte.
- **Release 0.6.10**: `app/build.gradle` versionCode 19 / versionName
  0.6.10; `StorageBridge.cpp` banner JNI_OnLoad 0.6.10; `test_main.cpp`
  banner do runner 0.6.10; `.github/workflows/release.yml` artifact
  `goni-vv-0.6.10-release`; `README.md` com escopo 0.6.10 + roteiro de
  verificação C33.

## 5. Decisões técnicas relevantes

1. **Dispatch extraído para função pura em vez de corrigir in-place no
   main.cpp** — um fix de 2 linhas no main resolveria o bug, mas ficaria
   outra vez invisível ao CI (o main.cpp não corre na suíte). Com o
   `applyAssetPick` puro + resolvers injetados, o CONTRATO do dispatch
   passou a ser afervel — incluindo a sequência exata do main (o teste
   `assetpick_sequencia_do_main_menukind_antes_do_draw` teria apanhado o
   bug original).
2. **`menuKind` capturado antes do draw** — `drawAssetMenu` fecha o seletor
   no clique (comportamento mantido — os testes existentes travam-no), por
   isso o tipo do menu tem de ser lido ANTES da chamada.
3. **Falha preserva o estado anterior** — uma textura que falha a carga não
   estraga a que já estava aplicada (o comportamento pretendido do bloco
   original, que nunca chegou a correr para ser comparado).
4. **Toast do warn do gate preservado** — o aviso `>2K` da textura continua
   a chegar ao toast (comportamento F5.1-A), agora com a linha
   `material: textura aplicada <ref>` no log MESMO com aviso.
5. **Sem flags dirty** — o pipeline de render é immediate-mode por design
   (F3/F5-E): o `drawTics` lê `mr->texture` todos os frames; o material
   binda a textura no sampler (`setTexture` → `bind(0)` + `uHasTex=1`) no
   frame seguinte ao pick. Nada de "marcar dirty" — seria código morto.
6. **Persistência sem alterações** — `SceneSerializer` já gravava `texPath`
   e o `LoadCtx.resolveTex` já re-ligava; o teste de round-trip do
   `applyAssetPick` → dump → loadText agora trava o caminho COMPLETO.
7. **Contadores no stub GL** — a mesma técnica dos contadores do lifecycle
   (0.6.7-a): os no-ops gravam números; nenhum teste existente é afetado
   (o reset zera).

## 6. Testes novos (CI Linux)

`tests/test_assetpick.cpp` — 11 casos (suíte 326 → 337):

1. `assetpick_escolher_textura_aplica_estado_e_log` — pick 2 no menu
   textura → `texture` + `texPath` no MeshRenderer, log exato
   `material: textura aplicada textures/wood.png`, toast `textura aplicada`;
2. `assetpick_textura_screenshot_do_c33` — o nome REAL do caso
   (`screenshot-20260930-1010.png`) passa íntegro pela ref (sem truncagem);
3. `assetpick_textura_com_aviso_do_gate_mostra_o_aviso` — warn `>2K` chega
   ao toast e a textura APLICA na mesma (F5.1-A);
4. `assetpick_textura_que_falha_mantem_estado_anterior` — resolver devolve
   nullptr → estado anterior intacto, toast `falha ao carregar textura`,
   log com `FALHOU` + a ref;
5. `assetpick_remover_textura_volta_a_none` — pick 1 liberta a referência
   (`texture == nullptr`, `texPath` vazio, log
   `material: textura removida`) e re-aplicar volta a funcionar (ciclo
   completo);
6. `assetpick_sequencia_do_main_menukind_antes_do_draw` — **o teste do
   bug**: réplica EXATA do protocolo do main com clique REAL injetado
   (InputState, fonte a 28 px, geometria do seletor): menuKind capturado
   antes → `drawAssetMenu` devolve pick 2 e fecha-se → `applyAssetPick`
   chega ao componente;
7. `assetpick_escolher_mesh_aplica_e_textura_embutida` — mesh pick aplica
   mesh/material/meshPath + textura embutida do glTF (F5.1-B);
8. `assetpick_mesh_cube_procedural_limpa_a_ref` — pick 1 do menu mesh →
   cubo procedural + ref libertada;
9. `assetpick_alvos_invalidos_sao_ignorados_sem_crash` — TIC morto, TIC
   sem MeshRenderer, pick 0/negativo, pick fora do catálogo, menuKind
   desconhecido;
10. `assetpick_roundtrip_goni_preserva_a_referencia` — escolher →
    `SceneSerializer::dump` → `loadText` com resolver → TIC recarregado
    com `texPath` preservada e `texture` re-ligada;
11. `assetpick_render_binda_a_textura_aplicada` — stub de render:
    `Renderer::drawMesh(mesh, model, vp, &tex)` → `glBindTexture` com o id
    CERTO (`lastBoundTexture == tex.handle()`) + `uHasTex = 1.0`
    (`lastUniform1f`); `nullptr` → `uHasTex = 0.0` e ZERO binds (cubo
    cinzento).

## 7. Commits da sub-fase (branch main)

- `558eb79` — **f6-a**: o fix completo (dispatch puro + fix da ordem +
  stub GL + 11 testes + release 0.6.10).
- (segundo commit) — **f6-b**: este relatório + sha256 do APK assinado.

## 8. HEAD da sub-fase

`558eb79` (f6-a) + o commit do relatório (f6-b).

## 9. Suíte de testes

- **337 OK / 0 falhas** (326 da F5.5 + 11 novos de `test_assetpick.cpp`).
- Gates: `check_main.sh` (sintaxe dos TUs device-only) OK;
  `link_parity.sh` (71 TUs da app ligam no hospedeiro) OK;
  `jni_parity.py` (Java ↔ RegisterNatives ↔ .dynsym) OK — nenhum gate
  existente removido.

## 10. CI

Run **36748264165** (commit `558eb79`) — 100% verde:

- `Testes do core (Linux)` — sucesso (cmake/ctest com a suíte completa +
  check_main + link_parity + jni_parity);
- `APK release arm64 (assinado via secrets)` — sucesso (keystore dos
  secrets, apksigner verify OK);
- `verify-entry-symbols` — sucesso (nm -D: ANativeActivity_onCreate,
  android_main, JNI_OnLoad e TODOS os natives da VvActivity exportados;
  gate do manifest binário: hasCode=true, VvActivity, lib_name=goni_vv,
  MANAGE_EXTERNAL_STORAGE declarado).

## 11. APK

- Artifact **`goni-vv-0.6.10-release-signed`** (id 11113288061, run
  36748264165), `app-release.apk` (1.189.861 bytes).
- **sha256**:
  `cfdc8803a3b160e2804a5d4c511c07629a14652e493b945b1b535b172cf9db9e`
- APK cumulativo: cobre 0.6.10 (este fix) + 0.6.9 (gizmos) + 0.6.8 (play
  mode) + 0.6.7 (lifecycle GL + gestão de projetos) + F5.x (assets, storage,
  All Files Access).

## 12. Verificação no device (roteiro C33 — resumo)

1. Instalar o APK 0.6.10 (versionCode 19) sobre o 0.6.9 — upgrade normal;
2. Importar um PNG (Menu → Importar…): toast OK, `textures/screenshot-….png`
   no projeto;
3. Tocar `tex:` no Inspector com o TIC selecionado → escolher o screenshot →
   **o cubo mostra a imagem** e o Inspector passa a `tex: screenshot-…`;
4. Settings → Ver logs → linha `material: textura aplicada
   textures/screenshot-….png`;
5. Menu → Save → Load (ou sair/reentrar) → a textura persiste;
6. Tocar `tex:` → none → `tex: none` + cubo cinzento + log
   `material: textura removida`;
7. Regressões: gizmos (0.6.9), play mode (0.6.8), lifecycle/gestor de
   projetos (0.6.7), import/export All Files Access (F5.5).

## 13. Riscos e mitigações

- **Risco: o seletor de mesh mudar de comportamento** — o bloco morto
  continha a lógica de mesh também; a extração reproduz a MESMA lógica
  (cube procedural, asset + material lit, textura embutida glTF), agora
  alcançável. Mitigação: testes 7/8 + os de UI existentes.
- **Risco: ODR dos stubs inline no hospedeiro** — descoberto DURANTE o
  desenvolvimento (local): objetos .o compilados contra a versão antiga do
  stub silenciosamente mantinham os no-ops antigos (os contadores novos
  nunca gravavam). Mitigação no ambiente local de desenvolvimento: rebuild
  total quando os stubs mudam (hash stamp). O CI (cmake) compila sempre de
  raiz — não afetado.
- **Risco: textos longos no log/toast** — buffers `char[64]`/`char[160]` com
  `snprintf` (truncagem segura, nunca overflow).

## 14. Dívida técnica conhecida

- O seletor de assets continua com cap de 5 ficheiros sem scroll (F8
  documentada desde a F5-E) — inalterada por CLÁUSULA CALMA.
- O label do Inspector mostra o basename (sem preview da textura) —
  inalterado (zero UI nova).
- O `AssetPickOutcome` usa buffers fixos em vez de `std::string` —
  deliberado (cópia barata, sem alocação no hot path da UI; o mesmo padrão
  do resto do código).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

- **Só wiring material/textura + testes**: `applyAssetPick` + fix da ordem
  no main + stub GL para aferir bind + 11 testes. Zero UI nova (o seletor e
  o Inspector ficaram INTACTOS — mesmos ids, mesma geometria, mesmo tema
  mono), zero física, zero componentes, zero campanhas.
- Tema mono e landscape intactos (nenhum token de cor, nenhum layout
  tocado).
- Nenhum gate existente removido (check_main, link_parity, jni_parity,
  verify-entry-symbols com o gate do manifest — todos verdes).
- 1–2 commits (f6-a fix + f6-b relatório), testes verdes antes do push,
  CI release com APK assinado.

## 16. Conclusão

A desconexão era de UMA linha de ordem de avaliação num bloco de dispatch
que nunca correu — mas era invisível ao CI precisamente porque vivia no
main.cpp (fora da suíte) e os testes de UI paravam na abertura do seletor.
O fix corrige a ordem E extrai o contrato para uma função pura afervel, com
o teste da sequência do main a travar a regressão para sempre. O seletor de
mesh — igualmente morto desde 0.5.0 — ficou curado pelo mesmo fix. Com o
wiring fechado, o caminho completo escolher → material → render bind →
persistência → reload funciona ponta a ponta, com as linhas de log do
contrato visíveis no log viewer do C33. Suíte 337 OK / 0 falhas; CI run
36748264165 100% verde com APK assinado (sha256
`cfdc8803a3b160e2804a5d4c511c07629a14652e493b945b1b535b172cf9db9e`).
Aguarda VERIFIED do dono no C33 pelo roteiro do §12.
