# RELATÓRIO 0.7.8 — Separação render 3D ↔ UI no Play (fronteira GL explícita)

Primeira sub-fase do trio pós-0.7.7 do C33. O dono reportou: no Play com
câmara ativa e UiCanvas, **a UI de jogo desenha-se gigante/cortada** — o
pass de UI herda o estado GL do pass 3D da câmara de jogo. As outras duas
queixas (gizmo que oscila/foge; frustum gigante que rouba toques) têm
sub-fases próprias (0.7.9/0.7.10).

## 1. Objetivo

(1) **Diagnóstico** do estado GL que o pass de UI herda do pass 3D no Play
com câmara ativa (viewport/scissor/matriz/depth). (2) **Fix**: após o
render 3D, repor para o pass de UI o viewport CHEIO, o scissor reposto, o
depth off para UI, a matriz ortográfica de ecrã (a do editor/modo UI) e o
textScale correto. (3) O **resolver de layout** usa coordenadas de ecrã
(safe-area), nunca afetado pela câmara; a UI desenha depois do 3D, por
cima. (4) **Regressões**: TouchControls corretos no Play; editor/modo UI
inalterados. (5) **Testes** de tudo isto no CI Linux.

## 2. Estado inicial (HEAD de entrada)

`e17694a` (0.7.7-b, relatório — sub-fase 0.7.7 fechada). Suíte: 418 OK /
0 falhas; CI 100% verde (run 36825404231); APK 0.7.7 assinado
(versionCode 27, sha256 `d6d9b9fa…`).

## 3. Arquitetura escolhida

**A fronteira 3D→UI passa a ser EXPLÍCITA e IDEMPOTENTE.** O inventário do
que cada pass deixava cair (o diagnóstico pedido):

| Estado GL | Pass 3D deixava | Pass de UI afirmava |
|---|---|---|
| `glViewport` | nada (só o `resize` do INIT/RESIZED o definia) | **nada** — herança implícita |
| `GL_SCISSOR_TEST` | nada | **nada** (nunca gerado em lado nenhum) |
| `GL_DEPTH_TEST` | `glEnable` por `drawMesh` (LitMaterial) | só DENTRO do `endFrame`, e **só com submissões** (early-return) |
| `GL_CULL_FACE` | `glEnable` por `drawMesh` | idem |
| matriz de projeção | `uVP` = proj da câmara de jogo (fov/near/far do CameraComp) | ortográfica de ecrã — correta, mas de `w_/h_` em **cache** |
| blend/depthmask | grid alterna (`enable/disable`, `mask FALSE/TRUE`) | `endFrame` ligava/desligava blend |

Em Play com câmara ativa o pass 3D corre com a proj da CÂMARA DE JOGO
(fov/near/far do CameraComp) — qualquer divergência entre o viewport real
e o `w_/h_` em cache (re-criação de superfície, barras a esconder/mostrar
sem `WINDOW_RESIZED`, estado de driver) fazia a UI desenhar escalada
(gigante) e/ou fora do ecrã (cortada). O `endFrame` com 0 submissões
nem sequer desligava o depth — o frame seguinte herdava.

**Fix em duas camadas**: (a) `Renderer::beginUiPass(w, h)` — chamada pelo
main DEPOIS do render 3D, ANTES de qualquer widget — impõe viewport CHEIO
com o tamanho ATUAL do frame (o mesmo `w/h` que `frame()` leu do EGL e que
o resolver usa), `glDisable(SCISSOR_TEST)`, `glDisable(DEPTH_TEST)`,
`glDisable(CULL_FACE)`, e refresca `w_/h_` (a ortográfica de ecrã do
`endFrame` fica COERENTE com o viewport e com o layout do MESMO frame);
(b) `Renderer::endFrame()` afirma o MESMO estado SEMPRE — o early-return
de 0 submissões passa a correr DEPOIS da afirmação (o frame seguinte nunca
herda o depth/cull do 3D). Defesa em profundidade: dois pontos afirmam,
nenhum assume.

**Re-sync defensivo do contentRect**: `applyContentRect` (CONTENT_RECT_
CHANGED) agora reconsulta o tamanho do EGL (`refreshSize`) e chama
`resize` quando a superfície MUDOU — cobre os OEMs em que barras a
esconder/mostrar mudam a superfície SEM `WINDOW_RESIZED` (o contentRect
chega, o resize não).

**Layout intacto por construção**: `resolveCanvasLayout`/`elementRect`
recebem `(sw, sh, insets)` — **não existe parâmetro de câmara** (o
contrato está no tipo; aferido por teste-guarda). A UI desenha depois do
3D, por cima. O `textScale` é reposto a 1.0 por `UiContext::beginFrame`
(o Play nunca herda a escala do viewport 2D do editor).

## 4. Implementação (por ficheiro, commit `9ad9956`)

- **`render/Renderer.h/.cpp`** — `beginUiPass(i32,i32)` NOVO (a fronteira:
  viewport cheio + scissor/depth/cull off + `w_/h_` frescos);
  `endFrame()` afirma o estado antes do early-return de 0 submissões.
- **`platform/main.cpp`** — `g_renderer.beginUiPass(w, h)` entre o pass
  3D (`drawTics`+grid) e o pass UI (`g_ui.beginFrame`), nos DOIS ramos
  (Play e editor — o mesmo frame, a mesma fronteira);
  `applyContentRect` re-sincroniza EGL+viewport quando a superfície mudou.
- **`tests/stub/GLES3/gl3.h`** — o stub deixa de ser no-op para
  viewport/enables: grava `viewport[4]`, `viewportCalls`,
  `scissorEnabled/depthEnabled/cullEnabled/blendEnabled`,
  `lastMatrix4fv[16]`/`matrix4fvCalls` e `drawArraysCalls` (o padrão dos
  contadores do lifecycle — inócuo para os testes existentes).
- **`tests/test_passgl.cpp` (NOVO, 8 testes)** + registo no CMakeLists.
- **`app/build.gradle`** — bump 0.7.8 / versionCode 28.
- **`.github/workflows/release.yml`** — artifact
  `goni-vv-0.7.8-release-signed`.
- **`README.md`** — secção de escopo 0.7.8 + roteiro de verificação C33
  (6 passos).

## 5. Decisões técnicas relevantes

1. **Fronteira no Renderer, não no main**: o main chama UMA linha; o
   contrato vive com o resto do estado GL (o Renderer é o único dono de
   GL do pass de UI — `check_main`/`link_parity` continuam a apanhar tudo).
2. **O tamanho vem do FRAME, não do resize**: `beginUiPass(w,h)` recebe o
   `w/h` que o `frame()` leu do EGL — viewport, ortográfica e resolver
   ficam coerentes ENTRE SI mesmo que o `resize` tenha ficado stale.
3. **Afirmação duplicada (fronteira + submissão)**: barata (4 chamadas de
   estado por frame) e fecha o buraco do `endFrame` vazio — o estado do
   pass de UI nunca depende de "havia widgets neste frame?".
4. **Sem tocar no layout nem na projeção**: o resolver já era sadio; o bug
   era 100% da camada GL. A prova é o teste-guarda
   `passgl_resolver_da_ui_nao_depende_da_camara` — duas cenas com câmaras
   radicalmente diferentes resolvem rects IDÊNTICOS.
5. **O glstub grava estado em vez de só contares** — os testes de pass
   sujam o estado a propósito (viewport 37,91,640,360 + scissor/depth/
   cull ON, o pior caso do C33) e aferem a reposição. É o mesmo padrão
   que apanhou o bug dos cubinhos (0.6.7), agora para a fronteira.

## 6. Testes novos (CI Linux)

`tests/test_passgl.cpp` (8): `beginUiPass` repõe viewport cheio e desliga
scissor/depth/cull do estado sujado do pass 3D; `beginUiPass` refresca o
tamanho ATUAL da superfície (1600x800 após resize 1600x720); `endFrame`
afirma o estado MESMO sem submissões (regressão do early-return —
drawCalls 0 e estado UI); `endFrame` envia a ortográfica de ecrã exata
(`Mat4::ortho(0,w,h,0,-1,1)`) com blend off no fim; o frame de Play
INTEIRO (beginFrame 3D → estado sujado → fronteira → widgets → endFrame)
termina com o estado todo do pass de UI; Play com câmara ativa + UiCanvas
→ os quads emitidos começam nos rects do resolver (âncora Bottom com
insets), dentro do ecrã, com o estado GL final correto; o resolver NÃO
depende da câmara (guarda de contrato: cenas com fov 100 vs fov 20/far
2000/pose distante resolvem rects iguais); `textScale` reposto a 1.0 por
frame (fontHeight 28 no Play depois de um frame de editor com
ScopedTextScale 0.4).

Ajustes de suíte: nenhum (os 418 existentes passaram intocados — o stub
só acrescentou gravações).

## 7. Commits da sub-fase (branch main)

- `9ad9956` — 0.7.8-a: fronteira GL explícita (beginUiPass + endFrame
  sempre afirma + re-sync do contentRect) + stub com estado + test_passgl
  (8) + bump 0.7.8/versionCode 28 + README.

## 8. HEAD da sub-fase

`9ad9956` + este relatório (commit do relatório fecha a sub-fase).

## 9. Suíte de testes

**426 OK / 0 falhas** (418 → 426: +8 do test_passgl). Confirmado no
hospedeiro no MESMO commit do push (build g++ local com os stubs, a
replicar o CMake do CI); o run **36844100263** do CI passou a suíte
(ctest 100% — job "Testes do core" verde) + check_main + link_parity
(79 TUs) + jni_parity + JVM host + projects check.

## 10. CI

Run **36844100263** 100% verde: Testes do core (426 + checks), build-release
(APK assinado), verify-entry-symbols (manifest + .dynsym + apksigner).

## 11. APK

`goni-vv-0.7.8-release-signed` (artifact 11151993567 do run 36844100263),
versionCode **28**, versionName **0.7.8**.
sha256 do APK assinado:
`838312c72c48c34a8dba05313800141ca97b41fdd50d22a320c7999e55c59799`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.7.8, 6 passos): cenário do bug
(câmara ativa + UiCanvas → Play), UI de jogo a tamanho/posição corretos
(rects do editor, texto 28 px), TouchControls normais + botões clicáveis,
editor/modo UI inalterados, lifecycle home→voltar com Play ativo (a
fronteira afirma em cada frame), regressões 0.7.7/0.7.6.

## 13. Riscos e mitigações

- **Estado duplicado por frame** (beginUiPass + endFrame): 8 chamadas de
  estado idempotentes por frame — custo nulo (nenhuma submissão extra);
  o benefício é o estado nunca depender de "havia widgets?".
- **refreshSize no contentRect**: `eglQuerySurface` num evento de
  lifecycle (contexto corrente) — inócuo; só re-age quando o tamanho
  MUDOU (a chamada `resize` está guardada pelo if).
- **Bug com gatilho de driver**: a causa exata no C33 (Unisoc T612/Mali)
  não é reproduzível no CI — por isso o fix é CLASSE (afirmar o estado
  todo, sempre) e não ponto (um glViewport num sítio). Os testes aferem o
  contrato, não o driver.

## 14. Dívida técnica conhecida

- O `endFrame` continua a afirmar viewport com `w_/h_` internos — se um
  futuro render-target mudar o viewport a meio do pass de UI, precisará
  de recomposição (render targets não existem na engine; documentado).
- `drawArraysCalls` no stub conta submissões do pass de UI inteiro (não
  distingue runs) — suficiente para os testes atuais.

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só estado GL 3D↔UI + testes. Zero física nova, zero scripting, zero
features novas. Toolbar, layout da UI e projeção real da câmara no Play
INTACTOS (a `gameProj`/`gameView` não foram tocadas — a câmara de jogo
renderiza exatamente como na 0.7.7). Tema mono intacto. Nenhum emoji.
Bump 0.7.8 / versionCode 28.

## 16. Conclusão

A fronteira entre o render 3D e o render de UI deixou de ser herança
implícita: o pass de UI afirma o SEU estado (viewport cheio com o tamanho
do frame, scissor/depth/cull off, ortográfica de ecrã coerente com o
resolver) em cada frame, venha o estado de onde vier — câmara de jogo,
grid, re-criação de superfície ou driver. A UI de jogo no Play com câmara
ativa volta a desenhar-se nos rects do resolver (coords de ecrã +
safe-area, texto 28 px), por cima da cena. Suíte 418→426; CI 100% verde
com APK assinado. Pronta a sub-fase para o grab-lock dos gizmos (0.7.9).
