# RELATÓRIO 0.6.9 — Sub-fase: gizmos de transformação

> Campanha 0.6.7 → 0.6.9 · Sub-fase 3 de 3 · Release 0.6.9 (versionCode 18)

## 1. Objetivo

Gizmos de transformação no editor: **Mover** (3 setas por eixo + 3 quads de
plano), **Rodar** (3 anéis por eixo) e **Escalar** (3 handles por eixo +
handle central uniforme), com seletor de modo na toolbar, hit-test 3D
(raio do toque contra eixos/anéis/handles), snapping opcional (grid/15°/0.25)
e máscara de gestos (drag em gizmo NÃO orbita). Escrita no Transform3D
(pos/rot/scale) com âncoras. Só no TIC selecionado, SÓ em EDITOR.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `2ee8c65` (0.6.8-c complemento — CI 0.6.8 verde, APK
  assinado `dd7647cc…`).
- Suíte: 295 testes verdes.
- Situação: transformação apenas pelos sliders do Inspector (±20 pos /
  ±180° rot / 0.1–5 scale) — sem manipulação direta no viewport.

## 3. Arquitetura escolhida

- **Núcleo GL-free** (`ui/Gizmo.h/.cpp`): projeção (`projectPoint`), raio
  analítico (`screenRayDir` — NDC + tanHalfFov + aspect da base de câmara
  orbit; SEM inversas de matriz), interseção raio×plano (`planeHit` com
  guardas de paralelismo/atrás), hit-test por distância em PX à geometria
  PROJETADA (`distToSegmentPx`) e drag com ÂNCORAS.
- **Desenho por projeção:** a geometria 3D é projetada para segmentos de
  ecrã emitidos no batch de sólidos da UI (`QuadBatch::line` novo —
  retângulo rotacionado por 6 vértices). Sem depth (gizmo sempre visível),
  por baixo dos painéis (z-order de editor), tamanho de ecrã constante
  (`len = dist × 0.16`).
- **Cores de eixo:** X vermelho / Y verde / Z azul — **exceção documentada
  ao tema mono, SÓ nos gizmos 3D** (convenção universal de editores); a UI
  2D mantém o tema mono intacto. Hover/press → branco ACCENT + traço mais
  grosso.

## 4. Implementação (commits por ordem: mover → rodar → escalar)

- **0.6.9-a** (`707f9a0`) — **MOVER**: infra (QuadBatch::line +
  UiContext::drawLine + ui/Gizmo.h/.cpp + CMake app/suíte);
  `dragMoveAxis` (delta projetado no eixo — ruído fora do eixo ignorado);
  `dragMovePlane` (decomposição nos 2 eixos do plano); snapping grid 0.5;
  `drawGizmoToolbar` (Mover/Rodar/Escalar + Snap à direita da toolbar — os
  3 botões Menu/Play/Settings intactos); integração no main
  (`feedGizmo`/`applyGizmoDrag` + claimed mask ao orbit). Testes: 12.
- **0.6.9-b** (`7263caf`) — **RODAR**: `drawRing` (48 segmentos; anel ⟂ X
  no plano YZ etc.); `pickAxis` do Rotate (distância ao polilinha
  projetado); `dragRotate` (ângulo do dedo em torno do centro projetado;
  sinal corrigido por `dot(axis, fwd)`; rotação GLOBAL — pré-multiplicação
  `axisAngle(axis, delta) * âncora`; snap 15°). Testes: 6.
- **0.6.9-c** (`df58207`) — **ESCALAR**: `drawScaleHandle` + handle central;
  `pickAxis` do Scale (centro com PRIORIDADE); `dragScaleAxis` (fator =
  1 + delta/kScaleRef; clamp 0.05) e `dragScaleUniform` (fator radial por
  distâncias em px; âncora degenerada = sem mudança); snap 0.25.
  Testes: 8.
- **0.6.9-d** (este) — release: bump 18/0.6.9, artifact
  `goni-vv-0.6.9-release-signed`, banners, README (escopo + roteiro
  cumulativo), este relatório.

## 5. Decisões técnicas relevantes

1. **Hit-test em px de ecrã (não em unidades do mundo):** o toque é
   aferido contra a MESMA geometria projetada que é desenhada — o que se
   vê é o que se apanha; os limiares são independentes do zoom.
2. **Raio analítico sem inversas:** `screenRayDir` constrói a direção com
   a base (fwd/right/up + tanHalfFov + aspect) — estável e barato;
   `planeHit` recusa raios paralelos/planos atrás (honestidade).
3. **Âncoras no drag:** a pose final = âncora + delta recalculado a cada
   frame — o jitter do dedo nunca se acumula e o undo futuro (F8+) tem um
   ponto de partida limpo.
4. **Plano de vista para eixos:** o drag de eixo usa o plano ⟂ ao olhar
   que passa pelo gizmo — nunca degenera (o plano de vista nunca é
   paralelo ao raio); o drag de PLANO usa o plano do modo (XY/XZ/YZ) e
   recusa se estiver de perfil (`planeHit` devolve `anyHit=false`).
5. **Rotação global:** quat final = giro × âncora (pré-multiplicação) —
   coerente com `Transform3D::computeMatrix` (T·R·S com R global) e com o
   que o Inspector mostra (`Quat::toEuler`).
6. **Centro do Scale com prioridade:** a origem pertence aos 3 eixos
   projetados; o handle uniforme ganha — o hit-test do Scale testa o
   centro primeiro.
7. **QuadBatch::line sem clip:** emitido fora das regiões de scroll (o
   gizmo vive no viewport central); o clip por interseção de rects axis-
   aligned não se aplica a retângulos rotacionados.

## 6. Testes novos (CI Linux)

`tests/test_gizmo.cpp` — 26 casos (295 → 321):

- Fundação: projeção (alvo no centro; atrás rejeitado), raio do pixel
  (centro ≈ forward; cantos no cone; right/up coerentes com o y de ecrã).
- MOVER (12): hit-test X/Y/Z/XY/None; drag de eixo altera só o eixo; Y/Z;
  snap ao grid (0.3→0.5, 0.74→0.5, 0.8→1.0, −0.26→−0.5, sem snap exato);
  planos XY/XZ/YZ (eixo excluído intocado); drag em gizmo NÃO orbita
  (claimed) com contraste; NUNCA em play + exige seleção; desenho ≥21
  quads.
- RODAR (6): hit-test anel Z (plano XY) e anel X (plano YZ) a 45°; drag
  aplica o delta angular (quat == axisAngle com o sinal de facing; |w|
  coerente); snap 15° (11.4°→15°, 40°→45°); eixos independentes (o vetor
  do eixo fica imóvel); desenho dos 3 anéis (≥144 quads).
- ESCALAR (8): hit-test handle Y e CENTRO (prioridade); drag de eixo
  altera só o eixo (escala MULTIPLICA); nunca zero/negativo (clamp 0.05);
  uniforme pelo centro (2× em todos; âncora degenerada); snap 0.25;
  desenho handles+centro; integração final (Transform3D escrito com
  world == computeMatrix e worldDirty limpo — o contrato do applyGizmoDrag).

## 7. Commits da sub-fase (branch main)

| Commit  | Assunto |
|---------|---------|
| 707f9a0 | 0.6.9-a: GIZMOS — MOVER (setas + planos + infra + toolbar + integração) |
| 7263caf | 0.6.9-b: GIZMOS — RODAR (anéis + drag angular + snap 15°) |
| df58207 | 0.6.9-c: GIZMOS — ESCALAR (handles + central uniforme + snap 0.25) |
| (este)  | 0.6.9-d: RELEASE 0.6.9 (bump, banners, README, relatório) |

## 8. HEAD da sub-fase

- HEAD de fecho: 241117a (0.6.9-d) — release 0.6.9, versionCode 18.

## 9. Suíte de testes

- **321 testes, 321 OK, 0 falhas** (baseline da sub-fase: 295 — +26 novos).
- Gates locais verdes antes do push: `check_main.sh` OK; `link_parity.sh`
  OK (71 TUs — Gizmo.cpp incluído); `jni_parity.py` OK (3 natives).

## 10. CI

- `core-tests` (Linux): ctest + check_main + link_parity + jni_parity.
- `build-release` (NDK r26): `assembleRelease` + `apksigner verify`.
- `verify-entry-symbols`: `nm -D` + `jni_parity.py dynsyms.txt` + gate do
  manifest binário.
- CI da sub-fase (run 36722491747, HEAD 241117a): core-tests **verde**,
  build-release **verde** (APK assinado), verify-entry-symbols **verde**.

## 11. APK

- `app-release.apk` (arm64-v8a), versionCode 18, versionName "0.6.9",
  assinado com a keystore dos secrets — artifact
  `goni-vv-0.6.9-release-signed` do workflow `release`.
- **APK CUMULATIVO**: cobre 0.6.7 (lifecycle GL + gestão de projetos) +
  0.6.8 (play mode) + 0.6.9 (gizmos).
- sha256 do APK assinado (artifact `goni-vv-0.6.9-release-signed`, run
  36722491747):
  `77f611027565220197700e0814fae3e909fb3926c34559ccf117505895746753`

## 12. Verificação no device (roteiro C33 — resumo)

1. Selecionar TIC → gizmo aparece (Mover por omissão; cores de eixo).
2. Drag na seta X → move só em X; a câmara não orbita durante o drag;
   drag fora do gizmo orbita normalmente.
3. Rodar/Escalar pelos botões da toolbar; snapping pelo toggle Snap
   (grid 0.5 / 15° / 0.25).
4. Inspector coerente com a pose pós-drag (sliders).
5. Play → gizmo some; Stop → volta na pose restaurada.
6. Regressões 0.6.8/0.6.7 (roteiros das sub-fases anteriores).

## 13. Riscos e mitigações

- **Pontos de interseção dos anéis** (anel Y e Z partilham o ponto +X do
  círculo): o desempate é a menor distância — o primeiro testado ganha;
  visualmente ambos destacam ao mesmo ponto, funcionalmente o giro aplica-
  se a UM eixo (comportamento aceitável e documentado).
- **Eixo projetado curto** (quase colinear com o olhar): o hit-test do
  segmento projetado encurta na mesma proporção — dedos tendem a usar os
  eixos "de lado"; o plano de drag do modo plano cobre o caso.
- **Touch-and-drag rápido fora do gizmo:** o slot só é reclamado no press
  (hit-test) — um drag que COMEÇA fora do gizmo orbita (correto); o drag
  que começa NO gizmo segue o dedo mesmo que saia (âncoras).

## 14. Dívida técnica conhecida

- Sem undo/redo dos drags (F8+); as âncoras já prepararam o terreno.
- Gizmos no espaço do MUNDO (o pai da F9 pode exigir conversão local).
- O snap de rotação arredonda o delta RELATIVO à âncora de rotação (0°) —
  valores absolutos "bonitos" exigiriam a âncora do euler original.

## 15. Restrições respeitadas (CLÁUSULA CALMA)

- Zero física nova, zero componentes de jogo novos.
- Render 3D novo: SÓ os gizmos (linhas 2D projetadas no batch da UI —
  nenhum shader/programa novo).
- **Cores de eixo como EXCEÇÃO DOCUMENTADA ao tema mono, SÓ nos gizmos
  3D; tema mono intacto em TODA a UI 2D.**
- 3 botões da toolbar intactos (grupo do gizmo à direita); landscape;
  safe-area intactos.
- PlaySnapshot intacto; orbit desativado em play; gizmo ausente em play.
- Gizmo só no TIC selecionado.

## 16. Conclusão

A sub-fase fecha os critérios de aceitação 0.6.9: selecionar TIC → gizmo
aparece; drag na seta move, no anel roda, no handle escala; snapping
funciona (grid 0.5 / 15° / 0.25); orbit intacto fora do gizmo (e morto
dentro do drag); nada em PLAY. CI verde nos três jobs; APK assinado
cumulativo no artifact. **A campanha 0.6.7 → 0.6.9 fecha aqui**: lifecycle
GL curado (sem "cubinhos"), gestão de projetos completa (apagar/sair com
confirmação e auto-save), play mode com janela própria e gizmos de
transformação com hit-test 3D.
