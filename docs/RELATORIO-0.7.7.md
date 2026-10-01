# RELATÓRIO 0.7.7 — TIC de câmara: frustum wireframe no editor + gizmos + handles + câmara de jogo em Play

Sub-fase final do par 0.7.6/0.7.7: a cena não tinha câmara própria (sem
perspetiva configurável; o Play renderizava pela orbit de edição) e o
dono quer o clássico **gizmo de câmara** — o frustum wireframe visível só
no editor, configurável, arrastável, e a **câmara de jogo** em Play.

## 1. Objetivo

(1) **Componente `Camera`** registado/serializado (fovY, near, far,
projeção persp|orto, orthoSize, active — uma ativa por cena). (2) **Visual
no editor** conforme a imagem de referência: corpo (caixa + lente), cone
de 4 arestas até ao retângulo do plano far, retângulo do far, linha de
visão central, handles nos 4 cantos + centro do far; cor de gizmo/marca;
**nunca em Play**. (3) **Seleção por toque** no corpo/frustum (além da
Hierarchy). (4) **Gizmos de transformação** na câmara: mover/rodar no
Transform3D; **escalar ajusta fovY/orthoSize** (nunca a escala do
transform). (5) **Handles**: centro do far → `far`; canto → `fovY`;
prioritários sobre os eixos do gizmo. (6) **Câmara de jogo**: em Play
renderiza pela ativa (fallback orbit). (7) **Alinhar à vista** no menu
contextual. (8) **Inspector** da câmara.

## 2. Estado inicial (HEAD de entrada)

`3c1d5b6` (0.7.6-c, relatório — sub-fase 0.7.6 fechada). Suíte: 407 OK /
0 falhas; CI 100% verde (run 36824812418); APK 0.7.6 assinado
(versionCode 26, sha256 `da1f5ed0…`).

## 3. Arquitetura escolhida

**O componente é DADOS; a semântica vive em dois módulos.**
`components/CameraComp.h` (POD com defaults e clamps) segue o padrão
MeshRenderer. O invariante **"uma ativa por cena"** é pequeno demais para
um sistema: `core/CameraUtil.{h,cpp}` (findActiveCameraTic /
setOnlyActiveCamera / clearActiveCamera / enforceSingleActiveCamera) —
chamado pelo LOAD (a primeira do manifesto fica) e pelos toggles do
Inspector. O VISUAL e a MATEMÁTICA de interação vivem em
`ui/CamGizmo.{h,cpp}` (namespace `camgizmo`), **puros e GL-free até ao
desenho**: `computeFrustum(Transform3D, CameraComp, aspect)` devolve o
wireframe COMPLETO em mundo; `drawFrustum/drawAll` só projetam e emitem
(padrão dos gizmos 0.6.9); `pickCameraTic/pickHandle` usam a MESMA
geometria projetada (distâncias em px — o critério do desenho); os drags
são funções de ÂNCORA (nunca acumulam).

**Convenções**: pose = Transform3D do mesmo TIC (−Z local = direção de
visão, +Y local = up — lookAt); a classe chama-se `CameraComp` (o nome
`Camera` já é a orbit de `render/Camera.h`) mas o REGISTRO e o .goni
dizem **"Camera"** (id 6, adicionado NO FIM — ids 0..5 intactos);
`nearZ/farZ` (evitar as macros `near/far`).

**Handles vs gizmos — prioridade num só sítio**: o `feedGizmo` do main
consulta `pickHandle` ANTES de `pickAxis` no press edge (raio 30 px >
22 px dos eixos): perto de um handle o drag é do handle, sempre — o
estado do handle-drag é um irmão pequeno do `GizmoState`
(`CamHandleDrag`, âncoras far/fov/hit/dist) e reclama o MESMO slot (o
orbit ignora o dedo). O **escalar numa câmara** troca o alvo no
`applyGizmoDrag`: o fator do `dragScaleUniform` de sempre, aplicado a
fov/orthoSize (`dragScaleToFov`) — o transform fica intacto.

**Câmara de jogo = view/proj derivados**: `gameView` = lookAt(pos, pos+−Z,
+Y); `gameProj` = perspective(fov°) ou ortho(±orthoSize·aspect). O render
de Play escolhe a ativa (fallback orbit); o **editor mantém a orbit
SEMPRE** (o frustum é um gizmo, nunca o render — como a spec manda). A
base de movimento dos TouchControls segue a câmara de jogo em Play (o
stick-cima afasta o que o jogador VÊ).

**Alinhar à vista**: `alignToView` deriva a rotação da PRÓPRIA orbit —
`fromEuler(−pitch, yaw, 0)` mapeia o −Z local exatamente na direção
eye→target (a convenção `dir(target→eye) = (cp·sy, sp, cp·cy)` da orbit
invertida) — sem mat→quat genérico, sem inversas.

## 4. Implementação (por ficheiro, commit `a23a6fe`)

- **`components/CameraComp.h` (NOVO)** — dados + `projectionName` +
  clamps kMin/kMax (fov 1..170, near ≥ 0.05, far 0.2..2000).
- **`core/CameraUtil.{h,cpp}` (NOVO)** — o invariante da ativa.
- **`core/ComponentStore.{h,cpp}`** — storage `cameras_` + accessor +
  especializações + registo "Camera" (id 6) + removeAll/hasAny.
- **`core/SceneSerializer.cpp`** — `appendComponentJson(CameraComp)`
  (fov/near/far/proj/orthoSize omitidos quando default; `active` sempre
  gravado) + `fillCameraComp` + dispatch por nome +
  `enforceSingleActiveCamera` no fim do load.
- **`ui/CamGizmo.{h,cpp}` (NOVO)** — Frustum/computeFrustum/
  planeHalfExtents; drawFrustum/drawAll (cor de marca; handles só na
  selecionada); pickCameraTic (TODOS os segmentos do desenho: corpo,
  lente, near, far, cone, linha de visão — 22 px, o mais próximo ganha;
  TIC invisível não conta); pickHandle (30 px, 1..4 cantos, 5 centro);
  dragFar/dragFov/dragScaleToFov (âncoras, snap 1 u / 5° / passos dos
  gizmos, clamps); gameView/gameProj/gameForward; alignToView;
  `visible(play, ui)`.
- **`ui/EditorLayout.h`** — `InspProfile.cam` + kinds
  CamSection/Fov/Near/Far/Proj/Ortho/Active + ids 5400..5411 + as linhas
  no plano (a seguir à secção Transform3D).
- **`ui/EditorUi.cpp/.h`** — payload do Inspector (sliders + botões
  projecao/ativa com re-despacho; ativa usa setOnly/clear) ;
  `drawContextMenu(..., hasCamera)` com o 5º item "Alinhar a vista";
  drawPlusMenu 3D com 5 itens ("Camera").
- **`ui/UiEditor.cpp/.h`** — implementação do menu contextual com o item
  condicional.
- **`platform/main.cpp`** — criação pelo "+" (choice 5: Transform3D +
  CameraComp, nasce A ativa); `CamHandleDrag` + `applyCamHandleDrag` +
  âncoras `g_camScaleFov/Ortho`; feedGizmo com handles prioritários;
  applyGizmoDrag com o ramo escalar→fov/ortho; tap no viewport seleciona
  câmara (pick após o deselect); `drawAll` dos frustums no pass de editor
  (depois do backdrop, antes do gizmo); render de Play pela ativa (+
  base de movimento + eye/focus do grid); "Alinhar a vista" (ch == 5).
- **`tests/test_cameratic.cpp` (NOVO, 11 testes)** + ajustes:
  test_components (7 tipos), test_safearea (InspProfile explícito),
  test_uieditor (dispatch do choice 5).
- Bump 0.7.7 / versionCode 27 / artifact `goni-vv-0.7.7-release-signed` /
  README (escopo + roteiro C33).

## 5. Decisões técnicas relevantes

1. **`pickCameraTic` testa TODOS os segmentos que o desenho emite** — a
   primeira versão só via corpo/lente/cone e um toque no centro do far
   (fim da linha de visão) falhava; o teste de seleção apanhou (o hit-test
   e o visual partilham a geometria ou nenhum dos dois presta).
2. **O handle prioritário é uma REGRA DE CONSULTA, não de desenho**: o
   raio do handle (30 px) é maior que o dos eixos (22 px) e o feedGizmo
   pergunta PRIMEIRO pelos handles — um toque perto de um handle nunca
   rouba o eixo; longe deles, o gizmo de sempre.
3. **Escalar numa câmara nunca escreve no transform** — o ramo devolve
   ANTES do caminho do Transform3D (a escala de uma câmara não tem
   significado; o Inspector continua a mostrar os sliders de escala do
   transform, que ficam 1).
4. **Sem câmara ativa = fallback honesto**: desativar a ativa (ou uma
   cena sem câmaras) deixa o Play na orbit — nunca um estado ambíguo de
   "duas ativas" (o load arruma: a primeira do manifesto fica).
5. **O TIC de câmara nasce A ativa** — criar uma câmara é quer usá-la;
   `setOnlyActiveCamera` desativa as outras (previsível, afervido).
6. **O grid em Play com câmara de jogo** recebe `eye` da câmara e
   `focus = |pos|` (o fade é estético; nunca NaN — pos finito).
7. **Snap**: far em passos de 1 u, fov em passos de 5° (handles), fator
   do escalar nos passos dos gizmos (0.25) — coerente com o toggle Snap
   da toolbar.

## 6. Testes novos (CI Linux)

`tests/test_cameratic.cpp` (11): geometria do frustum (far/near de
tan(fov/2)·dist com aspect, orto fixo, câmara rodada 90°, e
planeHalfExtents à parte); seleção por toque (centro do far, meio do
cone, vazio, TIC invisível); gizmo mover/rodar no transform (âncoras +
snap); escalar=fov/ortho (fator 1.5, snap 1.75→105, clamps, âncora
degenerada, transform intacto); handles (pick dos ids, raio prioritário,
dragFar com sinal/snap/clamps, dragFov fator/snap/clamps); serialização
round-trip (2 câmaras, campos exatos, UMA ativa no load, defaults
omitidos); CameraUtil (setOnly/clear/enforce/sem-pose-ignorada/honesto);
play usa a ativa (gameView = lookAt exato, gameProj persp+orto exatos) +
frustum nunca em play/modo UI (gate) + batch com/sem handles; alinhar-à-
vista (pos = eye, fwd = eye→target, up sem rolagem, idempotente); plano
do Inspector da câmara (7 linhas, y cumulativo, TIC comum sem elas);
criação/menu contextual (5º item só com câmara).

Ajustes de suíte existente: `registry count 6→7 (+Camera id 6)`;
`InspProfile` do test_safearea passou a explícito (o agregado posicional
deslizaria com o campo novo `cam`); Env do test_uieditor despacha o
choice 5 como o main.

## 7. Commits da sub-fase (branch main)

- `a23a6fe` — 0.7.7-a: implementação completa (componente/invariante/
  serializer, CamGizmo, main, inspector/menus, testes, bump).

## 8. HEAD da sub-fase

`a23a6fe` + este relatório (commit do relatório fecha a sub-fase).

## 9. Suíte de testes

**418 OK / 0 falhas** (407 → 418: +11 do test_cameratic). Confirmado no
hospedeiro no MESMO commit do push; o run 36825404231 do CI passou a
suíte (ctest 100%) + JVM host + check estrutural.

## 10. CI

Run **36825404231** 100% verde: core-tests (418 + check_main + link_parity
79 TUs + jni_parity + JVM host + projects check), build-release (APK
assinado), verify-entry-symbols.

## 11. APK

`goni-vv-0.7.7-release-signed` (artifact 11145106853 do run 36825404231),
versionCode **27**, versionName **0.7.7**.
sha256 do APK assinado:
`d6d9b9fa3a5fdf352afe122f5075c133caa75c6ddf0a92c26aafef92e9e4e83f`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.7.7, 8 passos): criar a câmara pelo
"+" (frustum azul aparece), seleção por toque no frustum, gizmos
(mover/rodar na pose; escalar abre/fecha o cone = fov), handles do far
(centro=far, canto=fov, prioritários), Inspector (secção Camera,
projeção cicla, ativa), alinhar-à-vista (frustum = pose da orbit), Play
renderiza pela câmara ativa SEM frustum + regressões 0.7.6/0.7.5.

## 13. Riscos e mitigações

- **Frustum gigante** (far 500 a fov 60°): os cantos do far saem do
  ecrã — o desenho/hit-test já lidam (projectPoint aceita fora do NDC;
  os testes usaram far 8 para o caso visível). UX: arrastar o handle do
  centro aproxima o far.
- **Câmara de jogo dentro de geometria / a olhar para a vertical**: o
  render é honesto (é o que a câmara vê); a base de movimento tem guard
  (fallback yaw da orbit quando o fwd projetado degenera).
- **Handles vs seleção de OUTRA câmara**: os handles só existem na
  câmara SELECIONADA (desenho e pick) — tocar no frustum de outra
  câmara seleciona-a primeiro (duas etapas intencionais, como os
  editores convencionais).

## 14. Dívida técnica conhecida

- O Inspector da câmara não mostra o ASPECTO (usa o do ecrã — o jogo é
  landscape travado; um campo viria com render targets).
- `enforceSingleActiveCamera` é ordem-de-manifesto — cenas editadas à
  mão com duas ativas ficam com a primeira (documentado; sem ambiguidade).
- Handles só do far (spec); handles do near/fov por eixo ficam para um
  ciclo futuro se o dono pedir.

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só toolbar(herdada da 0.7.6)/câmara + testes: componente, gizmo, handles,
play-view, menus e Inspector da CÂMARA. Zero física nova, zero scripting,
zero UI de jogo. Tema mono intacto (a cor de marca só nos ícones/ativos e
no frustum — gizmo, como as cores de eixo). Nada sobreposto. Nenhum
emoji. Bump 0.7.7 / versionCode 27.

## 16. Conclusão

A cena ganha CÂMARA PRÓPRIA: um TIC com pose + perspetiva, o frustum
wireframe de referência visível só no editor (cor de marca, line batch),
seleção por toque, gizmos coerentes (escalar = fov), handles do far sem
conflitos, Inspector completo, alinhar-à-vista e o Play a renderizar
pela câmara ativa com fallback honesto. Suíte 407→418; CI 100% verde
com APK assinado. As duas sub-fases 0.7.6+0.7.7 fecham a reestruturação
UI/UX e a câmara de cena — prontas para o C33.
