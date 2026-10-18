# RELATÓRIO 0.8.6 — UX/layout/Inspector (hex, tipografia, gizmos de UI, Theme)

Terceira e última sub-fase da campanha F8. ZERO features novas fora do
polimento pedido: a UI ganhou edição de cor por código, tipografia por
elemento, gizmos de escalar/rodar no modo UI, Theme uniforme no launcher
e um teste de sobreposição que cobre a paisagem COMPLETA.

## 1. Objetivo

(1) Cor com sliders RGB + campo de código hex (editar por código) nos dois
Inspectors. (2) Tamanho de letra e estilo (normal/negrito/itálico) no
Label. (3) Gizmos de UI scale/rotate coerentes com os 3D. (4) Zero
sobreposição aferida por teste de rects por ecrã. (5) Página inicial limpa
+ Theme uniforme (toolbar/painéis/timeline/launcher/overlays). (6) Testes:
hex edita cor; font size/style aplica; gizmos alteram o elemento; launcher
limpo.

## 2. Estado inicial (HEAD de entrada)

`c8c8cff` (0.8.5-a, suíte 525 OK; CI da 0.8.5 a correr no push). Sintomas:
sobreposição e desorganização de UI; página inicial má; Theme
inconsistente (11 hex inline no launcher Java + diálogos claros do
sistema); Inspector sem cor RGB/hex, sem gizmos de UI scale/rotate, sem
tamanho/estilo de letra.

## 3. Arquitetura escolhida

**Hex = dados puros + teclado reutilizado.** `uiHexFormat`/`uiHexParse`
(funções PURAS em ui/UiEditor) fazem "#RRGGBB" ↔ RGB 0..1 — recusas
honestas (tamanho/dígitos) deixam o estado intacto. O teclado in-app
EXISTENTE recebe um MODO HEX (propósitos 4=elemento/5=TIC): a tecla de
caso vira "#", a geometria é a mesma (zero layout novo); o commitTextInput
aplica ao `color[]` do elemento ou ao `tint` do MeshRenderer.

**Tipografia por elemento = um caminho de texto com estilo.** `UiElement`
ganha `fontScale` (0.5..3.0 × base 28 px) e `textStyle` (Normal/Bold/
Italic). O `label()` de sempre passa por `labelStyled()`: escala multiplica
os glifos; negrito = duplo-draw embutido (+1 px — sem segundo atlas); 
itálico = cisalhamento dos VÉRTICES TOP dos quads (≈12°) — zero memória de
GPU nova, tudo afervel nos batches. `drawElement` usa o caminho estilizado
QUANDO o elemento diverge do default (caminho antigo byte-a-byte nos
restantes) e as métricas verticalmente escaladas mantêm a centragem.

**Gizmos de UI = matemática pura + wiring mínimo.** Handles: 4 cantos
(12 px de toque constante em ESCRÃ) + pega de rotação a 26 px acima do
topo-centro com haste. ESCALAR: canto oposto fixo (âncora capturada no
press em design px) e reancoragem por `uiGizmoScaleToRect` — o INVERSO
exato de `elementRect` nas 6 combinações de âncoras (testado: ida e volta
exata). RODAR: delta do ângulo do dedo à volta do centro com snap 15° (o
MESMO passo do snap de rotação 3D). Hit-test (editor E Play) respeita a
rotação por `uiRotatedRectHit` (inversa exata). Containers: só escala
(rotação ignorada — dívida documentada).

**Theme = uma fonte de verdade por plataforma.** No C++, os espelhos
kLine/kText do UiRuntime MORRERAM (UiContext.h é a fonte). No Java, os 11
hex inline viram 6 TOKENS constantes (BRAND/BG/TEXT/TEXT_DIM/SURFACE/LINE
— espelho do Theme.h) e o check estrutural REJEITA hex inline fora deles;
os 3 AlertDialogs correm num ContextThemeWrapper ESCURO (o manifest é
Fullscreen claro — a identidade deixou de quebrar nos diálogos).

## 4. Implementação (por ficheiro)

- **`components/UiCanvas.h`** — `fontScale`/`textStyle`/`rot` + nomes
  canónicos do estilo (serializer/Inspector).
- **`render/QuadBatch.h`** — `quadCorners` (cantos explícitos p/ rotação).
- **`ui/UiContext.h/.cpp`** — `labelStyled` (escala/negrito/itálico);
  `setQuadXform/clearQuadXform` + emissão rodada no `emitTo` e nos glifos
  com cantos; reset da xform no beginFrame (nunca arrastar para o frame).
- **`ui/UiRuntime.h/.cpp`** — `drawElement` com o caminho estilizado +
  rotação RAII; hit-tests com rotação; espelhos kLine/kText REMOVIDOS
  (tokens do tema); `uiRotatedRectHit`/`uiGizmoCornerRect`/
  `uiGizmoRotateHandleRect`/`uiGizmoSnapRot` (puras).
- **`ui/UiEditor.h/.cpp`** — `uiHexFormat/uiHexParse`; teclado em modo hex
  (propósitos 4/5); `commitTextInput` casos 4/5; `uiGizmoScaleToRect` +
  `uiGizmoAngleAt`; Inspector de UI: linhas HexBtn/FontScl/TStyleBtn
  (plano + draw + dispatch); drawUiViewport desenha os handles e processa
  os gestos de escala/rotação com prioridade sobre a seleção.
- **`ui/EditorLayout.h`** — ids 5303 (hex TIC)/8032-8034 (UI); row
  `ColorHex` no plano do Inspector de TICs (contagens atualizadas).
- **`ui/EditorUi.cpp`** — draw + dispatch da linha hex (teclado propósito 5).
- **`core/SceneSerializer.cpp`** — `fscale`/`tstyle`/`rot` gravados quando
  ≠ default; ausentes = defaults (ficheiros 0.8.x abrem limpos).
- **`app/src/main/java/vv/goni/ProjectManagerActivity.java`** — tokens
  centralizados; ContextThemeWrapper escuro nos 3 diálogos; título +
  subtítulo + empty-state com CTA.
- **`scripts/projects_ui_check.py`** — 3 checks novos (tokens sem hex
  inline fora deles; todos os builders no wrapper escuro; hierarquia da
  página inicial).
- **`tests/test_wiring086.cpp`** (NOVO, 7) — ver §6; contratos de alturas
  atualizados em test_safearea/test_scroll/test_uieditor (828→864, 820→856,
  18→21 — o padrão da casa para o plano que cresce).
- **`app/build.gradle`** — bump 0.8.6 / versionCode 37; release.yml —
  artifact `goni-vv-0.8.6-release…`; README — escopo + checklist C33.

## 5. Decisões técnicas relevantes

1. **Teclado reutilizado em modo hex** (em vez de um teclado numérico
   novo): zero geometria nova, zero testes de sobreposição a mais — a
   tecla de caso torna-se "#" e o modo é do PROPÓSITO, não do ecrã.
2. **Negrito/itálico SEM segundo atlas**: duplo-draw + cisalhamento de
   vértices são apenas EMISSÃO — o atlas 512×512 e o pipeline ficam
   intactos (CLAUSULA CALMA; o negrito real exigiria variante da fonte).
3. **Rotação por transformação de quads na CPU**: o batch continua
   axis-aligned na submissão; os cantos rodam na emissão à volta do
   centro do elemento (o clip permanece axis-aligned — recorte
   aproximado em elementos rodados dentro de scroll, documentado).
4. **O inverso exato nas 6 âncoras** (testado ida-e-volta) é o que torna
   o gizmo de escala "correto por construção" — sem isso, cada âncora
   seria um caso manual.
5. **O check Python passa a REJEITAR regressões de Theme**: hex inline
   fora dos tokens = falha de CI (o mesmo espírito do jni_parity).

## 6. Testes novos (CI Linux)

`tests/test_wiring086.cpp` (+7 — 525→532):

1. **hex_parse_format_roundtrip** — format exato ("#1E82E6"), parse sem
   cardinal/minúsculas, recusas (vazio/tamanho/dígitos/nullptr), alpha
   intocado;
2. **hex_commit_aplica_aos_dados_do_elemento** — o fluxo do teclado:
   commit propósito 4 aplica "#FF0000" ao elemento; hex inválido devolve
   false e a cor fica INTACTA;
3. **tipografia_escala_negrito_italico_no_batch** — negrito = 2× quads por
   glifo; fontScale 2× duplica a largura emitida; itálico desloca os
   vértices TOP à direita do fundo (no batch REAL);
4. **gizmo_hit_rodado_e_handles** — hit rodado 90° (inversa exata), 4
   cantos disjuntos centrados, pega acima do topo-centro, snap 15°;
5. **gizmo_escala_reancora_nas_6_ancoras** — `uiGizmoScaleToRect` +
   `elementRect` = identidade para TODAS as combinações de âncora;
6. **rot_fontscale_style_roundtrip_no_goni** — "fscale"/"tstyle"/"rot"
   gravam e voltam exatos;
7. **zero_sobreposicao_do_chrome_em_paisagem_c33_e_1280** — composição
   COMPLETA (toolbar/painéis/centro/timeline/status/header) par-a-par e
   dentro do contentRect a 1600×720 e 1280×720.

## 7. Checklist device (resumo — versão completa no README)

Hex no tint e no elemento · letra Nx + estilo · gizmos de canto/rotação
com snap 15° · Play acerta elementos rodados · launcher limpo · diálogos
escuros · Theme uniforme · varredura de ecrãs sem sobreposição ·
regressões 0.8.x. VERIFIED no C33 = dono confirma cada item.

## 8. Riscos

- **Itálico por cisalhamento** é um oblique (≈12°) — a intenção visual é
  fiel, o traço não é caligráfico. Um atlas itálico real seria feature
  nova de assets (fora do CALMA).
- **Elementos rodados dentro de scroll**: o clip axis-aligned pode cortar
  meio-pixel nos cantos rodados — documentado; elementos rodados são
  tipicamente HUDs de topo (fora de scroll).
- **Contratos de altura dos Inspectores cresceram** (+36 px no de TICs,
  +3 linhas no de UI): o scroll existente cobre (testes atualizados e a
  passar); ecrãs muito baixos escrolam mais — comportamento correto.

## 9. Dívida (documentada)

- Rotação de CONTAINERS ignorada (filhos continuam axis-aligned) —
  implementar = transformação hierárquica no resolver (fora do CALMA).
- O modo UI continua slot-0-only nos widgets (dívida da 0.8.5,
  documentada).
- Negrito real (variante da fonte) e itálico caligráfico = dívida de
  assets/typography.

## 10. CLÁUSULA CALMA (cumprida)

Só o polish listado pelo dono. Zero features novas fora dele, zero física
nova, zero scripting. Formato `.goni` estendido de forma
backward-compatible (campos ausentes = defaults — ficheiros antigos abrem
exatos; round-trips verdes).
