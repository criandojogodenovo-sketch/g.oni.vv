# RELATÓRIO 0.8.5 — Funcionalidade desbloqueada (animação, primitivas, import)

Segunda sub-fase da campanha F8. ZERO features novas: desbloquear as
funcionalidades que a F7 implementou mas que morriam no WIRING do device —
com um teste de integração POR FIX que falhava antes e passa depois.

## 1. Objetivo

(1) Import obj/gltf/glb FUNCIONAL do toque ao mesh/clips aplicados, com
erro claro para formatos não suportados. (2) Animação e2e: criar
AnimationPlayer, add track, add keys, scrub, play; opções
loop/once/pingpong/speed respondem; serializa e recarrega. (3) Trocar
primitiva reflete no render e no .goni. (4) Fluxos criar-anim→play→
save→load e importar→aplicar→reproduzir afervéis no CI.

## 2. Estado inicial (HEAD de entrada)

`38b659d` (0.8.4-a, suíte 519 OK; CI da 0.8.4 a correr no push). Os três
sintomas: animação não cria/reproduz e opções não respondem; trocar
primitiva não troca; import não aceita obj/gltf/glb.

## 3. Diagnóstico (a causa-mãe + as secundárias)

**O IMPORT ESTAVA MORTO DESDE A 0.7.2.** O diálogo "aplicar ao TIC?" é
despachado por `g_editor.applyAsk && g_applyAsk.open` (main.cpp). O
`browserImportFile` setava SÓ `g_applyAsk.open` — o único lugar onde
`st.applyAsk` ficava `true` eram os TESTES (test_browser.cpp:317/329, que
setavam a flag À MÃO e por isso nunca apanharam o gap). Consequências em
cadeia no device: (a) o diálogo nunca abria; (b) nesse ramo também não há
toast (o "importado:" vive no `else`) — o dono via SILÊNCIO TOTAL;
(c) `gltfAttachSkin/gltfAttachClips`, que só correm DENTRO do "Sim",
ficavam INATINGÍVEIS — skins e clips glTF nunca chegavam ao player, mesmo
aplicando o mesh pelo seletor. O `importCandidate` (menu IMPORT, o 2.º
caminho de entrada) nem sequer setava o diálogo.

**ANIMAÇÃO "NÃO CRIA".** `AnimationPlayer::addTrack` escrevia SEMPRE em
`editClip()` = clips[0]. A timeline edita/mostra `activeClipPtr()` — e o
attach de clips glTF põe o importado como ATIVO. Com clip importado ativo,
cada "+track" caía num clip INVISÍVEL: a UI parecia morta (nenhum row
aparecia) e o play "não reproduzia" (o clip visível continuava vazio).
As OPÇÕES (loop/once/pingpong/speed) estavam SAUDÁVEIS no código — as
queixas vinham do header sobreposto da 0.8.4 (play↔clip a lutar pelo
toque) e deste clip invisível.

**PRIMITIVAS.** O wiring seletor→applyAssetPick→cache→render está íntegro
(e afervel desde a 0.8.0). Os sintomas no device eram sintomas da
CORRUPÇÃO DE MEMÓRIA fixada na 0.8.4 (comportamento aleatório após passar
32 elementos) — o que faltava era a PROVA de integração com upload real e
round-trip .goni, que agora existe.

**MULTI-MESH.** `ResourceManager::mesh` exige `#<i>` quando o glTF tem
vários meshes (contrato testado, correto para refs explícitas) — mas o
browser/catálogo/apply NUNCA geram sub-refs: todo .glb multi-mesh era
impossível de aplicar no device ("falha ao carregar mesh").

**CATÁLOGO.** O filtro comparava literais ("glTF", "GLB", "OBJ") — um
.Glb/OBJ importado pelo browser (case-insensitive) DESAPARECIA do seletor.

## 4. Implementação (por ficheiro)

- **`platform/main.cpp`** — `browserImportFile`: seta as DUAS flags
  (`g_applyAsk` + `g_editor.applyAsk`) quando há TIC com MeshRenderer;
  guarda de formato (kind 0) ANTES de ler o ficheiro, com toast
  "formato nao suportado ainda: <ext>" + elog::warn. `importCandidate`
  (menu Importar): MESMO wiring pós-import (diálogo nas duas flags).
  `refreshCatalog`: classificação centralizada em `fileapi::kindOfExtension`
  (lowercase) — o MESMO predicado que o browser e o e2e já usavam.
- **`platform/FileApi.cpp`** — `listDirEntries`: TODOS os ficheiros entram
  (kind 0 = não suportado); diretorias continuam primeiro.
- **`ui/UiEditor.cpp`** — `drawFileBrowser`: rótulo "? " para kind 0
  (mesh:/tex: para os suportados).
- **`render/GpuAssets.cpp`** — fallback de APPLY: ref gltf/glb SEM '#'
  que falhe no ResourceManager → tentar `#0` (log honesto "a aplicar o
  #0"). O contrato do ResourceManager (erro para refs explícitas) fica
  INTACTO — a decisão de wiring vive na camada de GPU, host-testável.
- **`components/AnimationPlayer.cpp`** — `addTrack` no `activeClipPtr()`
  com fallback `editClip()` (o clip ATIVO é o que a timeline mostra).
- **`tests/test_browser.cpp`** — contrato atualizado: não suportados
  VISÍVEIS com kind 0 (a 0.8.5 dá-lhes voz; antes eram invisíveis).
- **`tests/test_wiring085.cpp`** (NOVO, 6 testes) — ver §6.
- **`tests/CMakeLists.txt`** — o novo TU na suíte.
- **`app/build.gradle`** — bump 0.8.5 / versionCode 36.
- **`.github/workflows/release.yml`** — artifact `goni-vv-0.8.5-release…`.
- **`README.md`** — escopo 0.8.5 + checklist C33 (9 passos).

## 5. Decisões técnicas relevantes

1. **As DUAS flags nos DOIS caminhos**: em vez de redefinir o dispatch
   (calma), o fix replica o contrato que os testes já assumiam — e o 2.º
   caminho (menu Importar) ganha o diálogo que nunca teve.
2. **O erro claro antes do I/O**: o formato é guardado ANTES do readAll —
   um .fbx de 500 MB não é lido para ser recusado.
3. **Fallback #0 no GpuAssets, não no ResourceManager**: o contrato do
   rm_ é protegado por testes e serve refs EXPLÍCITAS de .goni; o gap era
   o APPLY do device (que nunca gera sub-refs). Camada certa, log certo.
4. **kindOfExtension como fonte única**: browser, catálogo e testes
   passam a partilhar o MESMO predicado — casings mistos deixam de ser
   território de literais duplicados.
5. **Prova antes/depois (regra da campanha)**: sem o fix do addTrack, a
   suíte crasha (SEGV no fluxo que o fix habilita); com o fix, 525/525.
   O e2e de import→clip→play corre o caminho exato que o device nunca
   alcançava — o teste que teria apanhado o bug em 0.7.2.

## 6. Testes novos (CI Linux)

`tests/test_wiring085.cpp` (+6 — 519→525):

1. **addtrack_vai_para_o_clip_ativo** — clips[0]="edit" + importado
   ATIVO: addTrack entra NO ATIVO e o "edit" fica intacto; player vazio
   mantém o fallback editClip;
2. **anim_e2e_criar_keys_scrub_play_opcoes_save_load** — o critério
   completo: scrub aplica (t=1 → x+2); once termina NO FIM e para;
   loop volta do fim (2.1→0.1); pingpong reflete (1.9+0.2→1.9);
   speed 2× dobra o avanço; save/load reproduz;
3. **prim_switch_x3_reflete_no_render_e_no_goni** — esfera→cone→box→
   torus com upload REAL no stub: prim.kind/mesh trocam a cada pick, o
   drawMesh desenha o NOVO mesh, o .goni guarda "prim" e a ÚLTIMA troca
   persiste no load;
4. **gltf_multimesh_fallback_mesh0_no_gpu_assets** — rm_ sem '#' → erro
   "#<i>" (contrato intacto); GpuAssets → mesh 0; sub-refs explícitas
   continuam a funcionar;
5. **browser_lista_nao_suportados_e_catálogo_lowercase** — kindOfExtension
   .Glb/.GLTF/.OBJ/.PNG aceites, .fbx/.psd/sem-ext = 0 (o contrato do
   erro claro);
6. **import_animacao_do_parse_ao_play_com_track_no_clip_importado** — o
   caminho que o device nunca alcançava: parse → mesh aplicado pelo
   GpuAssets → gltfAttachClips (clip "girar" ativo) → "+track" entra no
   importado → play reproduz (t=1 → x+1) → save/load mantém e reproduz
   o FIM do clip (t=2 → x+2).

## 7. Checklist device (resumo — versão completa no README)

Import com diálogo · glb animado → clips · +track no importado · opções
respondem · primitivas ×3 · multi-mesh aplica · fbx tem voz · casings
mistos · regressões 0.8.x. VERIFIED no C33 = dono confirma cada item.

## 8. Riscos

- **Não suportados agora visíveis**: o browser lista .txt/.fbx/etc. —
  ecrãs com muito lixo ficam mais longos (scroll existe); o ganho
  (diagnóstico honesto) compensa.
- **Fallback #0 é implícito**: um .glb multi-mesh aplicado mostra o mesh
  0 SEM pergunta — o log documenta e a escolha explícita continua
  possível por .goni (#<i>). A alternativa (pergunta UI) seria feature
  nova — fora do CALMA.
- **addTrack em clip importado**: o dono pode acrescentar tracks manuais
  a um clip importado (antes impossível na prática) — a serialização já
  os suporta (round-trip testado).

## 9. Dívida (documentada)

- A UI device continua slot-0-only para widgets (o 2.º dedo não clica
  painéis/timeline) — vivo desde a F1, agora DOCUMENTADO; suportar
  multi-slot é mudança de arquitetura (fora do CALMA).
- Reimportar o mesmo glb duplica clips (já documentado na 0.8.1).
- O `importCandidate` aceita por kind do scanner (não passa pelo guard
  kind 0 — o scanner já só lista suportados).

## 10. CLÁUSULA CALMA (cumprida)

Só fixes e wiring listados. Zero features novas, zero física nova, zero
scripting. Formato `.goni` intocado (round-trips verdes). Nenhum widget
novo — o "?" no browser é o rótulo do ficheiro que a engine não lê.
