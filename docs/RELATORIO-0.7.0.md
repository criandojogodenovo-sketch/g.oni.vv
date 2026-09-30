# RELATÓRIO 0.7.0 — UI criável core + editor de UI dedicado + gestão completa de TICs

## 1. Objetivo

Construir a primeira sub-fase do sistema de UI criável da engine (F6): (a) o
componente `UiCanvas` com os elementos base (Panel, Label, Button, Image) e
ancoragens 3×3 resolução-independentes; (b) o separador "3D | UI" na toolbar
que abre um viewport 2D dedicado com edição WYSIWYG (tap seleciona, drag
move) e um Inspector de UI (pos/size/cor/texto/visível/âncoras/ação);
(c) ações declarativas on-click (mostrar/esconder/alternar panel, carregar
cena, spawn); (d) a gestão completa de TICs que faltava ao editor:
desselecionar, menu contextual com renomear/remover (com confirmação)/
duplicar/visibilidade, teclado in-app minimalista (zero IME de sistema) e
cor por TIC; (e) serialização `.goni` da UI + nome/visibilidade dos TICs;
(f) hit-test em Play com a UI por cima da cena. Critério transversal: nada
sobreposto nem desorganizado — aferido por testes de layout em cada ponto.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `2d63edd` (fecho da F6 — fix do seletor de textura; suíte
  337 OK; release 0.6.10 com versionCode 19).
- Gaps de editor confirmados por leitura de código: não havia UI de jogo
  criável (nenhum componente de canvas); não havia desselecionar (a seleção
  só mudava para outro TIC), renomear, menu contextual, nem visibilidade por
  TIC (o campo não existia); a cor dos materiais era fixa (shader lit sem
  tint); o "+" só criava TICs de preset; um único viewport 3D.

## 3. Arquitetura escolhida

- **`UiCanvas` como COMPONENTE (id 5 no registry, no FIM — ids 0..4
  intactos)**: qualquer TIC pode ter um canvas; os elementos vivem por valor
  no componente e serializam no `.goni` (builds antigas IGNORAM-no — a
  política forward-compat do serializer, testada).
- **Ancoragem 3×3 por OFFSET DO PONTO DE ÂNCORA** (esq/centro/dir ×
  topo/meio/fundo): `ui::elementRect` é a FONTE ÚNICA partilhada pelo editor
  e pelo Play — zero divergência de layout; `UiCanvas::setAnchor`
  re-normaliza o offset sem mover o elemento (aferido).
- **Runtime puro (ui/UiRuntime)**: draw (via UiContext — os testes leem os
  batches de CPU), hit-test (o TOPO ganha; só Button/Menu; invisíveis —
  elemento E TIC — não hit-testam) e `applyUiAction` com dependências
  INJETADAS (`UiActionCtx`: sceneExists/loadScene/spawnPreset — o padrão de
  `AssetResolvers` da F6). Toasts honestos: alvo inexistente/preset
  inválido/sem carregador NUNCA fingem sucesso.
- **Editor 2D em escala-caber (ui/UiEditor)**: o viewport central mostra o
  espaço de design (o ECRÃ INTEIRO) escalado — nunca corta; o transform é
  partilhado por draw/hit-test/drag (WYSIWYG exato); gizmos e orbit
  DESLIGADOS no modo UI; o gesto não corre por baixo de overlays.
- **Teclado in-app**: overlay mono A-Z/0-9/_/-/espaço/APAGA/OK/Cancelar com
  geometria em `keyboardLayout()` (FONTE ÚNICA partilhada com os testes) —
  zero IME de sistema (frágil em NativeActivity). O buffer/propósito vivem
  no `EditorState` (o commit é uma função pura `commitTextInput`).
- **Cor por TIC = uniform `uTint` no shader lit** (multiplicativo; branco
  default definido a CADA draw — uniforms GL nascem a 0): sliders R/G/B no
  Inspector escrevem `MeshRenderer.tint`; o comportamento 0.6.x é mantido
  byte a byte com tint ausente (teste com stub GL).
- **Visibilidade**: campo `Tic::visible` (serializado; ausente = visível) —
  o pass 3D salta o TIC em editor E Play; a FÍSICA e a LÓGICA continuam
  (invisível ≠ desligado). Toggle "O/X" na Hierarchy + checkbox no Inspector.
- **Menu contextual** via botão "..." na linha da Hierarchy (o long-press
  da spec é o atalho alternativo; o botão é determinístico e aferível no
  CI). Remover pede CONFIRMAÇÃO (substitui o apagar sem aviso); duplicar
  copia os componentes por valor com nome único Godot-style.

## 4. Implementação (por ficheiro, commit `5740e89`)

- **`components/UiCanvas.h/.cpp` (NOVOS)** — elemento (kind/âncoras/ação/
  alvo/param/cor/visível/rect), `addElement` (defaults por tipo, nome único,
  nasce centrado), `findElement`, `setAnchor` (preserva a posição absoluta).
- **`ui/UiRuntime.h/.cpp` (NOVOS)** — `elementRect`, `drawElement` (tema
  mono; Image com textura via `UiContext::imageQuad` e placeholder
  moldura+diagonal), `drawCanvas`, `hitTestCanvas` (+ linha de Menu),
  `applyUiAction` (+ helpers de menu "label>alvo").
- **`ui/UiEditor.h/.cpp` (NOVOS)** — `uiViewportTransform` (escala-caber +
  inversos), `drawUiViewport` (WYSIWYG), plano do Inspector de UI
  (`uiInspectorPlan` — y cumulativo, o contrato F5.0-fix), `drawUiInspector`,
  `uiAddElement`, `drawContextMenu`, `drawRemoveDialog`, `duplicateTic`,
  teclado in-app (`keyboardLayout`/`drawTextInput`/`openTextInput`/
  `commitTextInput`/`uiTextCharAllowed`).
- **`ui/EditorUi.h/.cpp`** — EditorState estendido (uiMode, seleção/drag de
  elemento, menu contextual, remoção, teclado, desselecionar arm);
  `drawModeToggle` (separador 3D|UI); Hierarchy com olho/"..."/vazio;
  Inspector de TICs com linhas "visivel" + "cor R/G/B" (plano 32 rows);
  `viewportTapClearsSelection` (puro); `drawGizmoToolbar` encolhe em ecrãs
  estreitos (nunca sobrepõe o separador); `closeAllOverlays` estendido.
- **`ui/EditorLayout.h`** — ids/geometria novos (3D|UI 11/12, olho/⋮ 100000/
  200000+, menu contextual 6500+, remoção 6510+, teclado 6600+, Inspector de
  UI 8000+, scroll 46) + `toolbarModeRect`/`toolbarModeEndX`.
- **`ui/UiContext.h/.cpp`** — batch de IMAGENS (até 4 texturas por frame;
  submissão solids → imagens → glifos), `setImageResolver`, `imageQuad`.
- **`render/Material.h/.cpp`, `render/Renderer.h/.cpp`** — `uTint`
  (`setTint` a cada draw), `drawMesh(+tint)`, submissões 2→6.
- **`core/Tic.h`** — campo `visible` (física/lógica intactas).
- **`core/ComponentStore.h/.cpp`** — storage + registry do UiCanvas (id 5).
- **`core/SceneSerializer.cpp`** — UiCanvas completo (kinds desconhecidos
  ignorados), `visible` do TIC, `tint` do MeshRenderer.
- **`platform/main.cpp`** — separador e viewport 2D no frame; canvas em
  Play (hit-test press/release com slot reclamado, UI sobre a cena e sob os
  TouchControls); `drawTics` salta invisíveis + passa o tint; desselecionar
  no viewport 3D; dispatch dos diálogos (contextual/remoção/teclado/plus por
  modo); `makeUiActionCtx`; reentrada reseta o estado novo.
- **`tests/stub/GLES3/gl3.h`** — `glstub::stats.lastUniform3f` (uTint).

## 5. Decisões técnicas relevantes

- **O menu contextual usa o botão "..." (não long-press)**: a spec diz
  "long-press (ou ⋮)"; o botão é determinístico no immediate-mode (o
  long-press exigiria medição de tempo de press em widgets que só têm
  edges de 1 frame) e é aferível no CI com taps injetados.
- **Ancoragem por offset do ponto de âncora** (não por frações do pai):
  mais simples de editar WYSIWYG e de serializar; o contrato testado é a
  DISTÂNCIA à borda da âncora preservada entre resoluções.
- **`LoadScene` declarativa fica válida mas SEM carregador na 0.7.0** (toast
  "sem carregador"): a 0.7.1 liga o `loadScene` real — nunca um sucesso
  falso na 0.7.0.
- **A linha 3 do teclado NÃO sobrepõe a linha de baixo**: a 1ª versão da
  `keyboardLayout` esquecia a altura da linha de baixo no total do diálogo —
  a tecla "8" cobria EXATAMENTE o CANCELAR e roubava-lhe o toque. O teste
  de geometria (agora com cruzamento tecla×linha-de-baixo) apanhou e travou
  a regressão.
- **Plano do Inspector de TICs cresceu 606→750 px** (linhas visível+R/G/B):
  os testes de scroll/safearea foram ATUALIZADOS para os números novos (o
  contrato de scroll continuar a ativar continua a ser aferido).
- **Image desenha com textura REAL do projeto** (batch próprio no
  UiContext, resolver ligado ao GpuAssets no main); sem textura →
  placeholder mono honesto (moldura + diagonais).

## 6. Testes novos (CI Linux)

`tests/test_uicanvas.cpp` (11 casos) — ancoragens em 2 resoluções
(1600×720 e 1280×720; distâncias às bordas preservadas; safe-area
respeitada), `setAnchor` preserva posição (e cola à direita na 2ª
resolução), defaults/nome único do `addElement`, hit-test (topo ganha,
só interativos, invisíveis — elemento E TIC — fora), menu por linha
("label>alvo"), ações show/hide/toggle por nome EM QUALQUER canvas, spawn
(preset válido/inválido), LoadScene honesta (sem carregador / cena
inexistente / com callback + estilos da 0.7.1), draw sem sobreposição
(rects reais; invisível não desenha nem conta), round-trip `.goni`
(elementos completos + visible + tint), cenas 0.6.x abrem com defaults.

`tests/test_uieditor.cpp` (19 casos) — toggle 3D|UI, viewport 2D
escala-caber em 2 larguras, bg exato no centerRect sem invadir painéis,
WYSIWYG (tap seleciona, drag move pelo delta em design px, vazio
desseleciona, topo ganha), plano do Inspector de UI (15 linhas com ação,
y cumulativo), sliders escrevem pos/size/cor, botões (visível, âncoras
PRESERVANDO posição, ação cicla, texto abre teclado, cancelar não aplica,
remove apaga e desseleciona), teclado (geometria sem sobreposição em TODAS
as linhas + linha de baixo, dentro da safe-area, filtro de caracteres,
digita/APAGA/OK renomeia, cancelar não aplica, vazio recusado, nome
duplicado ganha sufixo), Hierarchy (olho, "...", vazio desseleciona), menu
contextal (4 ações), diálogo de remoção (confirmar apaga e limpa seleção;
cancelar mantém), desselecionar no viewport 3D (tap≠drag, 1-frame,
claimed não arma), cor por TIC (sliders + `uTint` no stub GL; nullptr =
branco), plano do Inspector de TICs com as linhas novas, "+" do modo UI
cria elemento (e canvas à primeira), toolbar sem sobreposição em 2 larguras.

`tests/test_scroll.cpp`/`test_safearea.cpp`/`test_ui.cpp`/`test_components.cpp`
— atualizados para o plano novo (750 px; plan[32]) e o registry com 6 tipos.

## 7. Commits da sub-fase (branch main)

- `5740e89` — 0.7.0-a: UiCanvas + UiRuntime + UiEditor + gestão de TICs +
  cor por TIC + serialização + testes (337→367).
- 0.7.0-b — este RELATÓRIO 0.7.0 (docs/RELATORIO-0.7.0.md) com o sha256 do
  APK assinado do run da sub-fase.

## 8. HEAD da sub-fase

`5740e89` (0.7.0-a — o código; o commit do relatório vem imediatamente
por cima, o padrão das sub-fases anteriores).

## 9. Suíte de testes

**367 OK / 0 falhas** (baseline 337; +30: 11 uicanvas + 19 uieditor).
Corrida local (hospedeiro) e no CI (job `core-tests`, ctest).

## 10. CI

Run `36768482718` (commit `5740e89`) — **100% verde**:
- `core-tests`: 367 OK + `check_main.sh` (paridade de sintaxe do main) +
  `link_parity.sh` (**75 TUs** — 71 + UiCanvas/UiRuntime/UiEditor) +
  `jni_parity.py`;
- `build-release`: APK assinado via secrets (`VV_KEYSTORE`);
- `verify-entry-symbols`: `ANativeActivity_onCreate`/`android_main`/
  `JNI_OnLoad`/natives da VvActivity no `.dynsym` + gates do manifest
  binário (hasCode, activities, lib_name, MANAGE_EXTERNAL_STORAGE).

## 11. APK

`goni-vv-0.7.0-release-signed` (artifact do run `36768482718`),
versionCode 20 / versionName 0.7.0, arm64-v8a, **APK CUMULATIVO** (cobre
0.6.7→0.6.10). sha256 do `app-release.apk`:
```
c77aa6ac0b2af8a72bdca1ccc4ae7fcd2f6e9430c44a7ca36046aef774c9d890
```

## 12. Verificação no device (roteiro C33 — resumo)

1. "UI" na toolbar → viewport 2D dedicado; "3D" devolve a cena;
2. "+" no modo UI cria Button centrado e SELECIONADO; Inspector UI completo;
3. drag move (WYSIWYG); vazio desseleciona; âncora não faz saltar;
4. ação toggle por nome (teclado in-app no texto/alvo) → Play → o botão
   alterna o Panel POR CIMA da cena;
5. "..." na Hierarchy → Renomear (teclado)/Remover (confirma)/Duplicar
   (.001)/Visibilidade; olho O/X imediato; invisível não desenha (a física
   continua — chão invisível segura);
6. vazio do viewport/Hierarchy desseleciona (drag continua a orbitar);
7. cor R/G/B tinge o cubo; Save→Load preserva; branco = o cinzento de sempre;
8. regressões 0.6.x (gizmos, play mode, seletor de textura, import, logs).

## 13. Riscos e mitigações

- **Plano do Inspector cresceu** (750 px) → o scroll continua a ativar
  (testes de safearea atualizados com os números novos);
- **Esconder um TIC por engano** → o olho é um clique (não guarda nada) e
  o estado serializa; o Inspector mostra "visivel: nao" no mesmo TIC;
- **Teclas do teclado muito pequenas em ecrãs estreitos** → o diálogo
  usa 92% da largura útil (cap 760 px); testado sem sobreposição;
- **Trocar de modo com drag em curso** → o `drawModeToggle` limpa a seleção
  de elemento e o `elDrag` ao voltar a 3D.

## 14. Dívida técnica conhecida

- O elemento Image não tem UI de ESCOLHA da ref da textura (o campo
  `image` serializa e desenha; escolher do catálogo fica para a 0.7.2 com
  o navegador de ficheiros);
- Long-press como ATALHO alternativo ao "..." (a spec permite qualquer um);
- O rótulo do olho é textual ("O"/"X") — o atlas é ASCII (sem ícones).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só UI/cenas-de-preparação/gestão de TICs + testes: **zero física nova** (a
visibilidade NÃO mexe no PhysicsSystem — o pass de física continua a
correr), **zero scripting** (V.ONI=F9.1), **zero componentes de gameplay**.
O render mudou APENAS o uniform `uTint` (cor por TIC, pedida pela spec) com
default branco = comportamento anterior byte a byte. Tema mono; landscape;
safe-area; teclado in-app (nada de IME de sistema); testes de layout
(rects/glifos) verdes.

## 16. Conclusão

A 0.7.0 entrega o núcleo da UI criável com o padrão da casa: política pura
e afervel (`elementRect`/`applyUiAction`/`viewportTapClearsSelection`/
`commitTextInput`/`keyboardLayout` — tudo GL-free com injeção de
dependências), geometria partilhada entre desenho e testes (fonte única) e
os bugs de layout apanhados pelo CI ANTES do device (o caso da tecla "8"
sobre o CANCELAR). A gestão de TICs fecha os gaps apontados (desselecionar,
renomear, menu contextual, visibilidade, cor) e o `.goni` cresce de forma
forward-compat. O terreno está posto para a 0.7.1 (cenas múltiplas +
transições), que já encontra `Project::scenes`/`addScene` prontos e a ação
`LoadScene` validada.
