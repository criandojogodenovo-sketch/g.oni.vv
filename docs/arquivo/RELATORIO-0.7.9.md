# RELATÓRIO 0.7.9 — Grab-lock dos gizmos (fim da oscilação/"fuga" do drag)

Segunda sub-fase do trio pós-0.7.7 do C33. O dono reportou: ao arrastar
um eixo do gizmo, **o gizmo oscila e foge do dedo** (o drag re-faz
hit-test contra a posição atual do gizmo). A 0.7.8 fechou a fronteira
GL 3D↔UI; esta fecha a mecânica do drag dos gizmos.

## 1. Objetivo

(1) **Grab-lock**: no touch down num eixo/anel/handle, capturar
`{eixo/anel/handle, grab offset, plano/raio FIXO (perpendicular à câmara
no grab)}`. (2) Durante o touch move: **NÃO re-fazer hit-test**; aplicar
o delta do dedo projetado no eixo/plano FIXO — o gizmo move-se com o
objeto mas o drag não depende do dedo estar sobre ele. (3) Touch up
liberta o lock. (4) **Hit radius generoso no grab** (touch target
≥ ~44 px). (5) **Snapping aplicado ao valor final**, não ao delta cru.
(6) Testes de tudo (drag do eixo Y com desvio a meio, snap estável, grab
ligeiramente fora, rodar/escalar idem).

## 2. Estado inicial (HEAD de entrada)

`12d4d28` (0.7.8-b, relatório — sub-fase 0.7.8 fechada). Suíte: 426 OK /
0 falhas; CI 100% verde (run 36844402637); APK 0.7.8 assinado
(versionCode 28, sha256 `838312c7…`).

## 3. Arquitetura escolhida

**A causa raiz era TRIPLA (e estava toda no wiring do main):**

1. O press edge do `feedGizmo` capturava as âncoras de POSE
   (`anchorPos/anchorRot/anchorScale`) mas **NUNCA as âncoras
   GEOMÉTRICAS** — `anchorHit` ficava `(0,0,0)` (o primeiro frame media
   o delta contra a ORIGEM DO MUNDO → o objeto SALTAVA para longe),
   `anchorAngle` ficava `0` (o rodar aplicava o ângulo ABSOLUTO do dedo
   logo no arranque) e `anchorDist` ficava `0` (o escalar-uniforme caía
   no guard degenerado `dist0 < 1` — **drag morto**, o mesmo no
   escalar-fov da câmara).
2. O `applyGizmoDrag` media o hit de CADA frame contra um plano
   **RE-ANCORADO na posição ATUAL do gizmo** (`planeHit(basis, n,
   origin, …)` com `origin = gizmoTr->pos`): o plano MEXE com o drag →
   o delta depende de onde o objeto já está → **realimentação** →
   oscilação/fuga (o dedo afasta-se, o gizmo foge, o hit re-ancora mais
   longe, o gizmo foge mais…).
3. O raio do grab era o mesmo do hover (22 px) — dedos reais em
   720 px de altura perdiam o eixo.

**O grab-lock inverte o contrato**: o estado do drag deixa de ser "a
pose atual + o dedo" e passa a ser **um lock capturado no arranque** —
`gizmo::Grab { target, slot, basis (raio da câmara NO GRAB),
planeOrigin (pos do TIC NO ARRANQUE), planeNormal, anchorHit (hit
raio×plano FIXO), anchorAngle, anchorDist, anchorOx/Oy (centro projetado
do arranque) }`. `beginGrab()` faz o hit-test com o raio generoso
(`kGrabPx` = 44 px) e mede TODAS as âncoras no arranque; `grabHit()`
devolve o hit do dedo AGORA no plano FIXO (raio da base do GRAB — a
orbit pode mexer noutro dedo que o delta não salta). O `applyGizmoDrag`
do main consome o lock: move/escalar-eixo usam `grabHit` (plano fixo),
rodar/escalar-uniforme/câmara medem contra o CENTRO do grab. Touch up →
`g_grab = Grab{}` — o lock é libertado e o próximo press volta ao
hit-test. Os HANDLES do frustum (far/fov) ganham o mesmo lock
(`CamHandleDrag` guarda basis/plano/centro do grab).

**Snap no valor final**: `dragMoveAxis`/`dragMovePlane` aterram nas
COORDENADAS FINAIS (âncora+delta arredondado — degraus ABSOLUTOS do
grid, âncoras fora do grid incluídas); `dragScaleAxis`/`dragScaleUniform`
nas COMPONENTES FINAIS (passos absolutos de 0.25); `dragScaleToFov` no
VALOR (fov em passos de 5° como o handle do fov, ortho em 0.25 —
60×1.8=108 → **110**, não 105). O rodar mantém o snap do ângulo TOTAL
(o "valor final" da rotação é o delta absoluto desde a âncora — nunca o
incremento por frame).

## 4. Implementação (por ficheiro, commit `394a816`)

- **`ui/Gizmo.h/.cpp`** — `kGrabPx` (44); `struct Grab` + `beginGrab()`
  (hit generoso + âncoras todas no arranque; normal do plano conforme o
  alvo: eixo → plano de vista, plano → normal de mundo, Center/rotate →
  âncoras de ecrã) + `grabHit()` (plano FIXO, raio do grab);
  `pickAxis` ganha `grabRadius` (default fino — o hover não muda);
  snap no valor final em `dragMoveAxis`/`dragMovePlane`/
  `dragScaleAxis`/`dragScaleUniform`.
- **`ui/CamGizmo.cpp`** — `dragScaleToFov` com snap no VALOR final.
- **`platform/main.cpp`** — `gizmo::Grab g_grab`; `feedGizmo` com o
  ciclo grab-lock (press → `beginGrab`; move → `applyGizmoDrag` sem
  hit-test; up → lock libertado); `applyGizmoDrag` consome o lock
  (`grabHit`/centro do grab; nada re-ancora na pos atual);
  `CamHandleDrag` estendido com o lock (basis/planeN/planeOrigin/
  anchorOx/Oy) e `applyCamHandleDrag` sempre no plano/centro FIXOS.
- **`tests/test_gizmo.cpp`** — secção 0.7.9 com 7 testes novos (ver §6).
- **`tests/test_cameratic.cpp`** — expetativa do snap do
  `dragScaleToFov` atualizada (105 → 110; comentário com a semântica).
- **`app/build.gradle`** — bump 0.7.9 / versionCode 29.
- **`.github/workflows/release.yml`** — artifact
  `goni-vv-0.7.9-release-signed`.
- **`README.md`** — escopo 0.7.9 + roteiro C33 (8 passos).

## 5. Decisões técnicas relevantes

1. **O lock é um struct puro em `ui/Gizmo`** (não no main): as âncoras e
   o plano são AFERVÁVEIS no CI — o bug estava exatamente no wiring
   não-testável; agora a captura e o consumo do lock são funções puras
   e o main só encadeia.
2. **Raio do GRAB vs raio do HOVER separados**: 44 px só no press edge
   ("grab ligeiramente fora ainda agarra"); o destaque visual continua
   fino (22 px) para o gizmo não "acender" meio viewport.
3. **O raio também é do grab** (não só o plano): com multi-touch, um
   segundo dedo pode orbitar durante o drag — o hit passa a ser medido
   com a base da câmara NO GRAB, logo o delta é imune à orbit.
4. **Rotate mede contra o CENTRO do grab**: o centro de um drag de
   rotação nunca se move (rodar não translada), mas a sua PROJEÇÃO muda
   se a orbit mexer — medir o ângulo contra o centro do arranque elimina
   o último grau de liberdade do salto.
5. **Snap final ≠ snap do delta**: com âncora fora do grid (pos 1.3),
   o snap do delta dava passos RELATIVOS (1.8, 2.3, …); o snap do valor
   final aterra em degraus ABSOLUTOS (1.5, 2.0, …) — o comportamento
   clássico dos editores e o que o C33 descreve como "snap estável".

## 6. Testes novos (CI Linux)

`tests/test_gizmo.cpp`, secção 0.7.9 (7): **âncoras capturadas no
arranque** (Grab válido com target/slot, plano pela pos do TIC com
normal ⟂ câmara, anchorHit NO plano fixo [não (0,0,0)!], centro/
ângulo/distância de ecrã; `Grab{}` inválido = lock libertado);
**raio generoso** (toque 40 px além da ponta do eixo: hover Não
destaca, grab AGARRA); **primeiro frame não salta** (dedo parado → pos
== âncora EXATA — a regressão do salto para a origem do mundo);
**drag do eixo Y segue o dedo sem oscilação** (dedo sobe 0.8 u e
DESVIA 120 px lateralmente a meio: lock não larga, eixo travado em x/z,
sem fuga [< 4 u], IDEMPOTENTE no frame seguinte); **snap no valor
final** (âncora {1.3, 0.7, 0} + 0.3 → 1.5; plano 1.65→1.5/0.8→1.0;
escala eixo 2.36→2.25; uniforme componentes 3.6→3.5/1.8→1.75);
**âncora do ângulo** (grab no anel Z, dedo parado → rotação identidade);
**âncora da distância** (grab no handle central a 40 px: distância
capturada > 30, dedo parado → escala identidade, +50% → 1.5× — o drag
que estava MORTO).

Ajuste de suíte: `cameratic_gizmo_escalar_altera_fov_ortho` — o snap do
`dragScaleToFov` passa ao valor final (105 → 110; +comentário).

## 7. Commits da sub-fase (branch main)

- `394a816` — 0.7.9-a: grab-lock completo (Grab/beginGrab/grabHit +
  feedGizmo/applyGizmoDrag + lock dos handles + snap no valor final +
  7 testes + bump 0.7.9/versionCode 29 + README).

## 8. HEAD da sub-fase

`394a816` + este relatório (commit do relatório fecha a sub-fase).

## 9. Suíte de testes

**433 OK / 0 falhas** (426 → 433: +7 do grab-lock, 1 ajuste). Confirmado
no hospedeiro no MESMO commit do push; o run **36846228410** do CI
passou a suíte (ctest 100%) + check_main + link_parity (79 TUs) +
jni_parity + JVM host + projects check.

## 10. CI

Run **36846228410** 100% verde: Testes do core (433 + checks),
build-release (APK assinado), verify-entry-symbols.

## 11. APK

`goni-vv-0.7.9-release-signed` (artifact 11154025233 do run 36846228410),
versionCode **29**, versionName **0.7.9**.
sha256 do APK assinado:
`4ef443cd11a501ce7c80a64e2f92250683f511b69e035d07e442d4e95a230edd`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.7.9, 8 passos): arrastar o eixo Y
com desvio lateral a meio (suave, sem fugir), sem salto no arranque,
rodar sem salto, escalar-uniforme VIVE, snap estável de degrau em degrau
(fora do grid incluído), grab a 3-4 mm da ponta ainda agarra, handles da
câmara suaves + escalar=fov com snap nos valores, regressões 0.7.8/0.7.7
+ orbit normal fora dos gizmos.

## 13. Riscos e mitigações

- **Alvo de 44 px rouba toques?** Só no PRESS edge e só perto do gizmo
  (o handle do frustum continua prioritário a 30 px; o tap de
  desseleção só se arma em slots NÃO reclamados — agarrar o gizmo é
  intencional). O hover continua fino.
- **Plano fixo vs câmara orbitada a meio**: o raio é o do GRAB — o
  objeto segue o dedo no espaço do arranque (sem salto); ao LARGAR, o
  próximo grab re-ancora na pos atual (o ciclo é sempre coerente).
- **Snap do valor final muda valores antigos**: âncoras ON-grid
  comportam-se IGUAL (testes 0.6.9 todos verdes sem edição); só âncoras
  fora do grid passam a aterrarem em degraus absolutos.

## 14. Dívida técnica conhecida

- O grab do ROTATE não trava o eixo contra a orbit (o ângulo é medido no
  ecrã do grab; uma orbit forte a meio pode inclinar o plano percebido —
  aceitável; um lock ao plano do anel ficaria para um ciclo futuro).
- `pickHandle` (frustum) mantém o raio fixo de 30 px (o alvo generoso
  dos 44 px aplica-se aos eixos/planos/centro dos gizmos — os handles
  já eram prioritários).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só drag de gizmos + testes. Zero física nova, zero scripting, zero
features novas. Toolbar, layout da UI e projeção da câmara intactos. O
comportamento dos handles far/fov e do escalar=fov mantém a semântica
0.7.7 (só a estabilidade e o snap mudam). Tema mono intacto. Nenhum
emoji. Bump 0.7.9 / versionCode 29.

## 16. Conclusão

O drag dos gizmos passa a ser um LOCK capturado no arranque: alvo, raio,
plano fixo e âncoras — nada re-ancora na posição atual, o dedo pode
desviar-se (ou a câmara orbitar noutro dedo) que o objeto segue suave,
travado no eixo, sem saltos e sem fugir. O escalar-uniforme (morto desde
a 0.6.9 no wiring) vive. O snap aterra em valores finais absolutos.
Suíte 426→433; CI 100% verde com APK assinado. Pronta a sub-fase final:
domar o frustum da câmara (0.7.10).
