# RELATORIO-0.12-V-VIEWPORT.md — os 3 bugs do viewport 3D do editor

Campanha 0.12-V (cláusula calma: SÓ viewport/gizmo/render do editor +
testes — zero features; física, V.ONI, importador glTF e tema da UI
INTACTOS). O pedido do dono: diagnóstico com ficheiro:linha ANTES de
qualquer correção, nada por palpite; só avançar com CI verde.

---

## §1 O diagnóstico (lido antes de mexer — o ficheiro:linha de cada causa)

### BUG 1 — «o modelo só aparece enquanto toco no gizmo»

**NÃO era um problema de estado GL nem de gating de draw.** O mesh é
desenhado em TODOS os frames: o loop de `drawTics`
(app/src/main/cpp/platform/main.cpp:5863..6038) não tem nenhuma condição
de seleção/gizmo, o `Renderer::drawMesh`
(app/src/main/cpp/render/Renderer.cpp:425..484) repõe o estado GL no fim
de cada draw (depth/cull/blend/mask) e o `beginFrame` limpa depth+color
(Renderer.cpp:412..423). O culling por blocos e o skinned cull consomem o
`tr->world` do frame (main.cpp:5800..5826) — corretos.

A causa real são DUAS metades do MESMO defeito:

1. **A posição persistente é invisível (a herança do pai).** O TIC
   «jogador» é filho da TIC «camera» (a cena do dono:
   StaticBody3D > camera > jogador). Com TRS local (0,0,0), o mundo
   persistente = o OLHO da câmara de jogo — e a câmara de jogo é
   frequentemente alinhada à vista do editor
   (`camgizmo::alignToView`, main.cpp:8603) — o modelo fica NO OLHO
   do editor (dentro do near/backfaces) ou fora do enquadramento.
   O importador NÃO parenta errado (verificado): o expand do glTF só
   parenta DENTRO da sub-árvore criada
   (app/src/main/cpp/assets/GltfInstantiate.cpp:82..86), os presets
   criam na raiz (app/src/main/cpp/core/Presets.cpp:54 —
   `Scene::create` default parent −1) e o .goni restaura os pais
   fielmente (app/src/main/cpp/core/SceneSerializer.cpp:1789..1794).
   A composição parent∘local do tick está CERTA
   (app/src/main/cpp/core/TransformSystem.cpp:40..41).

2. **O teleporte durante o arrasto (o que o dono VIA).** O feedback
   imediato do drag refrescava o cache com
   `tr->updateWorld()` (main.cpp:853 na árvore antiga) — e
   `updateWorld` escreve `world = TRS LOCAL`
   (app/src/main/cpp/components/Transform3D.h:33..36), IGNORANDO o pai.
   Como os ticks correm NO TOPO do loop (main.cpp:9382) e o `frame()`
   depois (9386), o mesh desenhava NO MESMO FRAME com world = local:
   saltava PARA O GIZMO («aparece um boneco minúsculo na base») e, ao
   largar, o TransformSystem recomponha parent∘local e o mesh voltava
   ao sítio «invisível». O MESMO defeito vivia nos 4 sítios de edição
   do Inspector (EditorUi.cpp:3029/3159/3388/5048 da árvore antiga).

### BUG 2 — «posição errada»

- (a) o importador/preset NÃO parentam errado — provado acima (§1-Bug1).
- (b) a matriz world e a ordem TRS estão CERTAS no motor:
  `world = parentWorld ∘ T·R·S` (TransformSystem.cpp:40..41;
  Transform3D::computeMatrix Transform3D.h:26..30).
- (c/d) o pivô/up-axis/escala do .gmesh não foram tocados (o 0.096 é o
  fit do import — `kImportTargetSize/maiorEixo`,
  EditorUi.cpp:5055 na árvore antiga — e continua igual).
- (e) **AQUI estava o bug do código**: o gizmo desenhava E hit-testava
  em `gizmoTr->pos` (o TRS LOCAL) projetado como se fosse MUNDO —
  main.cpp:6708 (`feedGizmo(gvp, gizmoTr->pos, …)`) e main.cpp:7347
  (`drawGizmo(g_ui, vp, gizmoTr->pos, …)` da árvore antiga) — enquanto o
  mesh desenha na TRANSLAÇÃO do `tr->world` (main.cpp:5874..5876). Para
  TICs com pai os dois sítios divergem: o gizmo não estava no mesh.
- E o drag escrevia o alvo do MUNDO diretamente no TRS LOCAL
  (main.cpp:819..848 da árvore antiga): num TIC com pai rodado/escalado
  o movimento saía rodado/escalado pelo pai.

### BUG 3 — «gizmos gigantes»

`gizmo::gizmoLength(camDist) = camDist · 0.16`
(app/src/main/cpp/ui/Gizmo.h:242 da árvore antiga) com os call sites a
passar `g_camera.dist` (main.cpp:6704/7348 da árvore antiga) — a
distância da ÓRBITA AO FOCO, não a distância DO OLHO AO GIZMO, e sem o
fator `tan(fov/2)`. O tamanho em píxeis
`len/(2·dist_real·tan(fov/2))·altura` só era constante quando o gizmo
estava no foco da órbita; perto do olho EXPLODIA (o gizmo da câmara
alinhada à vista, por exemplo) e longe encolhia. O hit-test já usava o
mesmo `len` (correcto), mas o raio do grab era 44 px (Gizmo.h:90 da
árvore antiga) — abaixo do mínimo de ~48 px do dono.

---

## §2 O fix (o diff resumido)

| Ficheiro | O que mudou |
|---|---|
| `core/TransformSystem.h/.cpp` | **`composeWorld(tr, parentWorld)`** — a FONTE ÚNICA da composição (world = parentWorld∘TRS; TRS na raiz, byte a byte); o `composeAt` do tick passa a usá-la (mesma matemática). **`worldToLocalPos(parent, worldPos)`** — a inversa TRS do pai (S⁻¹·R⁻¹·T⁻¹, guard ε na escala). **`parentTransformOf(scene, slot)`** — o resolve do pai por evento. |
| `platform/main.cpp` | **`gizmoAnchor(tr, tanHalfFov, &len)`** — a GEOMETRIA ÚNICA do gizmo: origem = a TRANSLAÇÃO do `tr->world` (o sítio do mesh), comprimento = dist ATÉ ao gizmo · tan(fov/2) · kScreenLen; consumida PELO `feedGizmo` (~6740) E PELO `drawGizmo` (~7390) — o hit e o draw partilham. A âncora do MOVER capturada no grab é a TRANSLAÇÃO do world; o alvo do drag (mundo) é convertido a LOCAL (`worldToLocalPos`) antes de escrever `tr->pos` (sem pai: byte a byte); o feedback imediato é `composeWorld` com o pai (o teleporte morre). |
| `ui/EditorUi.cpp` | Os 4 sítios de feedback do Inspector (campos do transform ~3042, reset ~3172, escala original ~3401, fit do applyAssetPick ~5061) usam **`refreshWorldWithParent`** (composeWorld com o pai). |
| `ui/Gizmo.h` | `gizmoLength(distToGizmo, tanHalfFov) = distToGizmo · tanHalfFov · kScreenLen` — a fórmula do dono, k = 0.16 como ÚNICA constante (na faixa 0.10..0.18; em píxeis: k/2 · altura — constante em zoom/distância/FOV/tamanho do objeto). `kGrabPx` 44→48 (o mínimo de toque). |

O QUE NÃO MUDOU no motor: o render dos meshes/blocos/partículas/céu, o
TransformSystem::tick (só passa pela fonte única — mesma matemática),
os formatos .goni/.gmesh, física, V.ONI, importadores, tema.

### O que ficou documentado FORA do âmbito

- **Rodar/Escalar em TICs com pai**: continuam a escrever o TRS LOCAL
  com eixos de mundo (o comportamento de sempre — o dono reportou a
  POSIÇÃO; rodar/escalar global de TICs com pai é feature nova e fica
  fora da cláusula calma). O MOVER é o caminho corrigido.
- **A cena do dono**: o «jogador» filho da «camera» é a hierarquia que
  o .goni traz; o motor não re-parenta nada por trás do dono. Com o
  fix, o gizmo aparece AGORA no sítio do mesh (e o mesh deixa de
  teleportar) — mover o jogador com o gizmo move-o no mundo real.
- R-062 (a entrada da F1 no REGRESSOES.md) continua como dívida
  declarada — não é desta campanha.

---

## §3 Os testes (todos novos, a suíte 0 falhas)

`tests/test_012v_viewport.cpp` (NOVO — o caminho REAL do device no
hospedeiro, o padrão do wiring087: inclui platform/main.cpp, o boot
INIT_WINDOW e o rasterizador honesto do stub):

1. **`viewport_modelo_sem_gizmo`** — a cena do dono
   (StaticBody3D > camera(CameraComp) > jogador(mesh, escala 0.096)),
   3 frames SEM toque nenhum e SEM seleção: o modelo projeta dentro da
   viewport e tem píxeis lit no sítio do world composto (o pedido do
   dono: «o teste que prove que o modelo é desenhado sem nenhum gizmo
   ativo»). Aferição extra: a translação do world = (0, 1.6, 0).
2. **`viewport_gizmo_no_mundo`** — com local (0,0,0) e mundo (0,1.6,0)
   distintos: píxeis de eixo vermelho NO mundo do mesh e ZERO no
   local-as-world (o Bug 2e morto ao nível do píxel).
3. **`viewport_drag_sem_teleporte`** — grab no eixo Y do gizmo (a vp do
   frame), 40 px de arrasto: o cache compõe com o pai NO GRAB, DURANTE
   e AO LARGAR; o mundo sobe com o dedo; o local fica coerente com o
   alvo do mundo (a conversão).
4. **`viewport_gizmo_px_constante`** — o mesmo gizmo medido a 3
   distâncias (~1.7 / ~5.5 / ~20.7): a largura do eixo X em píxeis
   constante ±20% (a fórmula velha dava 1:6.2); `kGrabPx ≥ 48`; a
   fórmula `dist·tan·k` aferida diretamente.

Sentinela **R-063 `regress_viewport_gizmo_world`**
(tests/test_sentinels.cpp) + a entrada em docs/REGRESSOES.md: a
composição única (o filho rodado+escalado segue o pai; mundo→local
devolve o local; a raiz byte a byte), a fórmula no fonte SEM comentários
(a lição M-Yd1), o main com a MESMA geometria no hit e no draw, o drag
com a conversão e o Inspector com os ≥4 sítios.

Recalibrações com nota (a fórmula nova encurta o gizmo ×tan(30°) —
tamanho de ecrã constante):

- `tests/test_gizmo.cpp` — o Env calcula `gizmoLength(cam.dist,
  tan(fov/2))` (o gizmo dos testes vive NO foco — a proporção de todos
  os pins mantém-se); 2 comentários de limiar atualizados (48).
- `tests/c33_virtual.cpp` — o press do teste de gesto D17 passa do MEIO
  para os 85% do segmento do eixo X (o gizmo mais curto aproximava o
  meio do glifo e a folga de 12dp ao handle morria; 1008/0 de novo).

---

## §4 As mutações (vermelho → verde, ao vivo)

- **M-V1** (o teleporte — o feedback do drag volta ao
  `updateWorld` local-as-world): `viewport_drag_sem_teleporte` VERMELHO
  (5 EXPECTs — «o cache no GRAB deixou de compor com o pai», «o arrasto
  no eixo Y não moveu o MUNDO (1.600 → 0.327)», «DURANTE o arrasto…»,
  «ao LARGAR…») + `regress_viewport_gizmo_world` VERMELHA. Reposto —
  TUDO VERDE.
- **M-V2** (a âncora local — o feed E o draw em `gizmoTr->pos` com
  `dist_da_órbita·0.16`): `viewport_gizmo_no_mundo` VERMELHO («o gizmo
  NÃO está no mundo do mesh (red=0)» + «ficou no LOCAL-as-world
  (red=147)») e `viewport_gizmo_px_constante` VERMELHO (147/91/37 px).
  Reposto — TUDO VERDE.
- **M-V3** (a distância da ÓRBITA no `gizmoAnchor` — o defeito exato do
  Bug 3): `viewport_gizmo_px_constante` VERMELHO («perto: 181 px…
  longe: 29 px»). Reposto — VERDE.

---

## §5 Verificação

- test_core: **0 falhas** (1233 testes; GONI_SKIP=r039,streaming_paralelo
  localmente — os flakes documentados da SANDBOX; o CI é o árbitro e
  corre completo).
- c33_virtual: **1008/0** (o replay inteiro pelo frame() real).
- Gates: theme_hex · ui_vocab · hierarchy · gizmo_camera_medidas ·
  projects_ui · glyph_source · reload_concurrency · gmesh_docs ·
  docs-lint · release-identity · jni_parity · check_main · link_parity —
  VERDES.
- versionCode 122 → **123** (a regra da casa: bump por commit com
  código; o §14-m do RELATORIO-0.9.6.md declara o par).

## §6 O que NÃO foi mexido (o inventário do dono)

- **Física** (physics/, PhysicsSystem, corpos) — intocada.
- **V.ONI** (voni/, o registo, tykers, VM) — intocada.
- **Importador glTF** (assets/Gltf*.cpp, GmeshV3Stream, conversor) —
  intocado (o Bug 2 provou que não era ele).
- **Tema da UI** (ui/Theme.h, os gates de hex/vocabulário) — intocado.
- **O format .goni/.gmesh** e o serializer — intocados (nenhum campo
  novo; a compat byte a byte mantém-se).
- **O render 3D** (Renderer, Mesh, BlockMesh, SkyRenderer,
  ParticleRenderer, Material, Grid) — intocado; o estado GL entre
  passes já estava correto (verificado no §1 — a hipótese (a) do dono
  ficou provada NÃO-ser-a-causa).
- **Rodar/Escalar de TICs com pai** — semântica de sempre (§2).
