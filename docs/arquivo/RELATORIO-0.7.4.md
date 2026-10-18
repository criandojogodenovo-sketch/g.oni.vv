# RELATÓRIO 0.7.4 — Paridade editor↔Play + Label só texto + texturas de fundo + Menu configurável + containers VBox/HBox

Sub-fase de **correção dos gaps de qualidade da UI criável encontrados no
C33 com a 0.7.3** (a primeira das duas tarefas pós-campanha 0.7).

## 1. Objetivo

Corrigir os quatro gaps reportados no C33: (a) aparência inconsistente —
Label desenhava SEMPRE com fundo claro enquanto o Button é escuro;
(b) divergência editor↔Play em vários elementos (o Menu era o caso visível:
caixas no editor, texto solto no Play); (c) texturas em falta — Button,
Panel e Image sem forma de escolher imagem de fundo; (d) ausência de
containers de layout — organizar era posicionar cada elemento à mão.
Princípio transversal estabelecido: **editor e Play renderizam IDÊNTICOS**
em geometria, cor, textura, texto e espaçamento; só a interação difere
(seleção/handles/drag no editor; hit-test/ações no Play).

## 2. Estado inicial (HEAD de entrada)

`040e8f4` (0.7.3-b — fecho da campanha 0.7). Suíte: 386 OK / 0 falhas;
CI 100% verde (run `36774826070`); APK 0.7.3 assinado cumulativo
(versionCode 23).

## 3. Arquitetura escolhida

**A divergência editor↔Play tinha causa raiz TRIPLA**, todas no viewport
2D do editor:

1. **Texto não escalado**: o viewport desenha o canvas em scale-to-fit
   (~0.4×) mas os glifos eram emitidos a 28 px CRUS — o texto ficava
   proporcionalmente MAIOR no editor, com truncagens/centralizações
   diferentes (exatamente o "Menu com caixas no editor e texto solto no
   Play");
2. **Insetes/espessuras constantes**: os 10/20/8 px de resguardo e as
   molduras de 1-2 px do `drawElement` não escalavam com o rect;
3. **Layout com insets ZERO**: o editor resolvia as âncoras com
   `Insets{}` enquanto o Play usa a safe-area REAL.

A correção segue três camadas complementares:

- **`textScale` no `UiContext`**: uma escala de contexto que multiplica
  métricas (`fontWidth`/`fontHeight`/`textMetrics`), a EMISSÃO de glifos
  (avanço, offset, tamanho) e — via `k = textScale()` dentro do
  `drawElement` — TODOS os insetes/molduras do desenho de elementos. No
  Play `k = 1` (comportamento 0.7.3 exato); no viewport 2D `k = escala`
  do transform. O mini-canvas passa a ser o Play REDUZIDO ao pixel;
- **Resolver de layout único** (`resolveCanvasLayout` em ui/UiRuntime):
  âncoras + safe-area + **containers** num único sítio, consumido por
  `drawCanvas` (Play), `drawUiViewport` (editor: draw + hit-test + drag)
  e `hitTestCanvas` — a paridade deixa de ser uma semelhança desejada e
  passa a ser **estrutural** (o mesmo código nos dois modos);
- **Clip ao mini-ecrã** (`UiContext::ScopedClip` + `scroll::intersectRects`):
  nada do canvas/joystick sangra para os painéis ao lado (o Play recorta
  na borda física do ecrã; o editor recorta igual).

**Texturas de fundo**: a ref vive no elemento (`e.image` — o campo do
Image 0.7.0, agora partilhado por Panel/Button), a renderização resolve
POR FRAME pelo `imgResolve` do `UiContext` (zero estado extra, zero
flags dirty), e o **tint passou a branco×alpha** (o Image 0.7.0 tingia
com o LINE escuro — a imagem escolhida ficava quase preta). O
**z-order sólidos↔texturas** passou a ser a ordem real de emissão: o
`UiContext` regista **runs** (batch + textura + range de vértices) e o
`endFrame` submete-os sequencialmente (`Renderer::submit` com range;
cap 6→32) — antes os batches submetiam-se em grupos fixos e um botão
sólido desenhado DEPOIS de um painel texturizado ficava por baixo dele.

**Containers**: campo `parent` (nome do container) + `spacing`/`pad`/
`align` por elemento; VBox/HBox dispõem os filhos em coluna/linha com
**auto-fit no eixo do conteúdo** (os filhos nunca transbordam);
aninháveis com **guard de ciclos**; filhos invisíveis colapsam;
container invisível esconde os descendentes; filhos desenham com **clip
ao rect do pai** e hit-testam no rect DISPOSTO (não no ox/oy).

## 4. Implementação (por ficheiro, commit `70a1b2a`)

`ui/UiContext.h/.cpp` — `textScale_` (RAII `ScopedTextScale`),
`ScopedClip`, runs de submissão (`recordRun`, `runCountForTest`…),
`label()` com glifos escalados, métricas escaladas, `imageQuad` com run
da textura, `endFrame` submete runs em ordem + glifos no fim.

`render/Renderer.h/.cpp` — `submit(batch, tex, firstVertex, vertexCount)`
(range de vértices), cap de submissões 6→32, `endFrame` desenha por range.

`ui/ScrollMath.h` — `intersectRects` (clip composto GL-free).

`components/UiCanvas.h/.cpp` — kinds VBox/HBox (7/8) + `Align`
(start/center/end); campos `spacing`/`pad`/`parent`; `defaultColor` do
Label com **alpha 0** (só texto; RGB=PANEL para o fundo opcional nascer
escuro mono); `defaultSize` dos containers; `uiElementIsContainer`/
`uiAlignName`; `setAnchor` com **overload de insets** (a fórmula do
`elementRect` — o elemento não salta ao trocar de âncora com safe-area).

`ui/UiRuntime.h/.cpp` — `resolveCanvasLayout` + `CanvasLayout` (rect
efetivo, `parentIdx`, `laid`, `shown`); `drawElement` com escala `k`
integral (insetes/molduras), Label com fundo opcional (alpha>0), Panel/
Button com textura de fundo (fallback sólido honesto), Image com
placeholder **"(sem imagem)"** (ou o nome da ref quando a carga falha),
Menu com espaçamento/fundo ON-OFF/alinhamento (`menuItemRect` com vãos),
VBox/HBox (fundo opcional + moldura; filhos desenhados pelo chamador com
clip do pai); `drawCanvas`/`hitTestCanvas` consomem o resolver (hit do
Menu por `floor((y-topo)/(rowH+spacing))`).

`ui/UiEditor.h/.cpp` — `drawUiViewport` reescrito sobre o resolver
(insets reais, clip ao mini-ecrã, `ScopedTextScale`, joystick com o
núcleo partilhado `drawTouchControlsAt` — base + knob + JUMP como no
Play, knob em repouso NO CENTRO; overlay de edição apenas moldura +
rótulo); detach-on-drag de filhos (`uiDetachElement` conserva a posição
visual via rect resolvido → âncoras Left/Top); Inspector de UI com as
linhas novas (**fundo A**, **tex:**, **espaço**, **pad**, **alinhamento**,
**colocar em:**) e filhos sem linhas x/y/âncoras (posição é do container);
`uiAddElement` aceita kinds 7/8 e nasce FILHO quando há container
selecionado (irmão do selecionado); `uiPlusChoiceKind` (mapa do "+").

`ui/EditorUi.h/.cpp` — `drawPlusMenu` com **10 itens** (+ VBox/HBox);
`drawAssetMenu` com `withImport` (linha **"importar…"** → navegador
0.7.2, código `kAssetPickImport`); **`applyUiTexPick`** (menuKind 3:
escreve/limpa a ref `e.image` do elemento; alvos mortos sem crash);
`drawTouchControlsAt` (núcleo parametrizável do joystick).

`ui/EditorLayout.h` — ids novos (`kUiInspA/Spacing/Pad/Tex/Parent/Align`,
`kAssetPickImport`).

`core/SceneSerializer.cpp` — `spacing`/`pad`/`align`/`parent` por
elemento (condicionais; ausentes = defaults 0.7.3-compat; kinds vbox/
hbox aceites no load).

`platform/main.cpp` — dispatch do "+" via `uiPlusChoiceKind`; seletor
menuKind 3 → `applyUiTexPick` + "importar…" abre o navegador.

`tests/test_uilayout.cpp` (NOVO, 13 testes) + `tests/test_joystick.cpp`/
`tests/test_uieditor.cpp` atualizados ao contrato novo (kind 7/8 válidos,
"+" com 10 itens, plano do Inspector com as 3 linhas novas — o `tapRow`
agora faz scroll até à linha).

## 5. Decisões técnicas relevantes

1. **Paridade por construção, não por comparação**: o mesmo resolver e o
   mesmo `drawElement` alimentam os dois modos — o teste de paridade é a
   prova (editor = Play × escala + offset, rect-a-rect, sólidos E glifos);
2. **Texto sempre por cima** (glifos submetem-se por último): decisão do
   tema mantida da 0.7.0 — legibilidade do texto sobre texturas;
3. **Tint branco×alpha** para texturas de fundo (a textura mostra as
   cores próprias; o alpha do elemento dá translucidez) — em vez do tint
   escuro do elemento;
4. **Auto-fit no eixo do conteúdo** dos containers (VBox: h; HBox: w):
   os filhos NUNCA transbordam; o eixo transversal fica manual; container
   vazio mantém o tamanho manual (placeholder);
5. **Filhos invisíveis colapsam** (não ocupam lugar — útil para alternar
   conteúdo) e **container invisível esconde descendentes** (cascata,
   semântica Godot-like);
6. **Detach conserva a posição visual** ("colocar em:" ao sair, ou drag
   para fora do container): o rect resolvido vira âncoras Left/Top — o
   elemento fica exatamente onde estava;
7. **Z-order por runs** em vez de reordenar batches: os batches acumulam
   como sempre (a API de testes lê `solidsForTest` íntegro); só a
   SUBMISSÃO é segmentada por ranges.

## 6. Testes novos (CI Linux)

`tests/test_uilayout.cpp` — 13 testes, 386→399:

- `uilayout_label_sem_fundo_por_default` — default alpha 0 (só glifos, 0
  quads sólidos no rect); alpha>0 desenha o painel; Button mantém fundo
  escuro;
- `uilayout_button_panel_image_com_textura` — quads texturizados nos
  rects certos (resolver de textura com `Texture` real no glstub); runs
  intercalados (textura primeiro, sólidos do botão DEPOIS — o z real);
  fallback sólido sem resolver;
- `uilayout_image_placeholder_sem_imagem` — placeholder centrado sem ref;
  com o nome da ref quando a carga falha;
- `uilayout_paridade_editor_play_por_tipo` — para Panel/Label/Button/
  Image/Menu/Card/Article: cada quad do Play existe no editor × escala +
  offset (sólidos e GLIFOS — a causa raiz do Menu);
- `uilayout_paridade_containers_e_insets` — paridade com VBox + filhos;
- `uilayout_vbox_hbox_dispoem_filhos_sem_sobreposicao` — auto-fit
  (134/188 px exatos), pad/spacing, align start/center/end, HBox
  aninhado (larguras auto), sem sobreposição;
- `uilayout_container_visibilidade_e_ciclos` — filho invisível colapsa;
  container invisível esconde descendentes; ciclo A↔B → órfãos de topo
  sem crash; pai inexistente → topo;
- `uilayout_hit_test_e_drag_usam_o_resolver` — Button filho hit-testa no
  rect disposto (não no ox/oy); `uiDetachElement` conserva a posição;
- `uilayout_add_element_nasce_filho_do_container_selecionado` — filho do
  selecionado; irmão quando o selecionado é filho; topo sem container;
- `uilayout_menu_espacamento_fundo_alinhamento` — rowH com vãos (spacing
  0 = 0.7.3 exato); fundo ON/OFF; texto centrado no align center;
- `uilayout_serializacao_roundtrip_containers` — spacing/pad/align/
  parent/image/alpha no round-trip; `.goni` 0.7.3 antigo abre com os
  defaults;
- `uilayout_runs_intercalam_solidos_e_texturas` — painel texturizado +
  botão sólido: run 0 = textura, último run = sólidos;
- `uilayout_applyuitexpick_escreve_a_ref` — ref/none/importar/alvos
  mortos.

Atualizados ao contrato novo: `compostos_menu_card_article…` (kinds 4..8
criáveis, 9 recusado), `joystick_plus_menu_dez_itens…` (10 itens +
`uiPlusChoiceKind`), `uieditor_inspector_ui…` (plano com 18 linhas,
y cumulativo), `uieditor_inspector_ui_botoes…` (tapRow com scroll até à
linha), `uieditor_plus_modo_ui…` (menu de 10 itens).

## 7. Commits da sub-fase (branch main)

- `70a1b2a` — 0.7.4-a: paridade estrutural + Label só texto + texturas
  de fundo (tex:) + z-order runs + Menu configurável + containers
  VBox/HBox + serialização + testes (386→399).
- 0.7.4-b — este RELATÓRIO 0.7.4 + sha256 do APK assinado do run da
  sub-fase.

## 8. HEAD da sub-fase

`70a1b2a` (0.7.4-a — o código; o commit do relatório vem imediatamente
por cima).

## 9. Suíte de testes

**399 OK / 0 falhas** (baseline 386; +13 em `test_uilayout.cpp`; 5
atualizados ao contrato novo).

## 10. CI

Run `36809429284` (commit `70a1b2a`) — **100% verde**: core-tests (399
OK + check_main + link_parity 75 TUs + jni_parity), build-release (APK
assinado), verify-entry-symbols (símbolos + manifest binário).

## 11. APK

`goni-vv-0.7.4-release-signed` (artifact do run `36809429284`),
versionCode 24 / versionName 0.7.4, arm64-v8a, **APK CUMULATIVO** (a
campanha 0.7 + os fixes desta sub-fase sobre a 0.6.7→0.6.10). sha256 do
`app-release.apk`:
```
f8731f850ce0d64ef0e24e09c76668d35bc71c703aef828bddbf759e3468cbef
```

## 12. Verificação no device (roteiro C33 — resumo)

1. Menu criado no modo UI → Play → EXATAMENTE o que o viewport mostrava
   (caixas/texto/espaçamentos); alternar editor↔Play em todos os tipos;
2. Label nasce só texto; "fundo A" liga o fundo; Button continua escuro;
3. Panel/Button com "tex:" de `textures/` (ou "importar…" → navegador →
   galeria); Image escolhe a imagem; sem imagem → "(sem imagem)";
4. Botão sólido sobre painel texturizado continua VISÍVEL (z-order);
5. Menu: "espaco"/"fundo A"/"alinhamento" mexem nos itens;
6. VBox/HBox: filhos automáticos (sem posição manual), auto-fit,
   aninhamento, drag para fora tira do container, "colocar em:" move;
7. Save→Load preserva texturas/alpha/spacing/pad/align/filhos; `.goni`
   antigo abre como sempre (roteiro completo no README).

## 13. Riscos e mitigações

- **Cap de runs (32) / de batches de textura (4)**: um canvas com muitas
  alternâncias sólido↔textura poderia cair fora do cap → os quads
  acumulam nos batches (os testes continuam a vê-los) mas o run extra não
  é submetido. Mitigado: casos reais ficam muito abaixo; documentado;
- **`CanvasLayout lay[32]` no viewport 2D**: canvases com >32 elementos
  ignoram os extras no EDITOR (o Play usa vector sem cap). Mitigado:
  32 elementos é muito acima do uso real; documentado;
- **`.goni` antigos com Label de fundo claro**: o load PRESERVA a cor
  guardada (o default novo só aplica a elementos novos) — honesto com o
  que o dono tinha.

## 14. Dívida técnica conhecida

- O cap de 5 ficheiros no seletor de texturas mantém-se (sem scroll no
  overlay — F8);
- Reordenar filhos dentro de um container continua por fazer (a ordem é
  a de criação; mover é via "colocar em:"/drag para fora e reentrada);
- O editor mostra o canvas do TIC SELECIONADO; em Play desenham-se todos
  os canvases ativos — o fluxo recomendado (e o TIC "UI" da 0.7.5)
  concentra a UI num só canvas.

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só aparência/paridade/texturas/containers da UI criável + testes. **Zero
física, zero scripting** (V.ONI=F9.1 intacta). Tema mono (nenhuma cor
nova — os tokens existentes); landscape; safe-area; "nada sobreposto"
aferido (clip do mini-ecrã, clip pai→filho, planos cumulativos).

## 16. Conclusão

Os quatro gaps do C33 fechados com uma mudança de PRINCÍPIO: a paridade
editor↔Play deixou de ser aspiracional — é estrutural (o mesmo resolver,
o mesmo `drawElement`, texto e insetes escalados), provada por um teste
determinístico por tipo de elemento. O Label nasce só texto, Button/
Panel/Image aceitam textura (com seletor e importação), o Menu é
configurável e os containers VBox/HBox organizam os filhos
automaticamente. Suíte 399 OK; CI 100% verde; APK assinado cumulativo
publicado.
