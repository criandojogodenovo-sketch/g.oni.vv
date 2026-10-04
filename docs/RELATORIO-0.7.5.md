# RELATÓRIO 0.7.5 — Overlays modais com backdrop + TIC de UI próprio + teclado com minúsculas

Sub-fase de **correção das três falhas de UX da campanha 0.7 encontradas
no C33 com a 0.7.3/0.7.4** (a segunda das duas tarefas pós-campanha 0.7).

## 1. Objetivo

Corrigir: (a) **z-order de overlays modais** — com o MENU de ficheiros ou
o menu contextual abertos no modo UI, os elementos do canvas ("TESTE",
"Botao") desenhavam-se por cima/através do overlay, misturando texto;
(b) **TIC próprio de UI** — criar UI obrigava a anexar o UiCanvas a um
TIC 3D existente (não havia criação direta nem nó de UI); (c) **teclado
in-app sem minúsculas** — só maiúsculas, sem toggle de caso.

## 2. Estado inicial (HEAD de entrada)

`70a1b2a` (0.7.4-a) + relatório `58ec209` (0.7.4-b). Suíte: 399 OK / 0
falhas; CI 100% verde (runs `36809429284` e `36810967850`); APK 0.7.4
assinado cumulativo (versionCode 24).

## 3. Arquitetura escolhida

**(a) Z-order**: em vez de tentar ordenar cada overlay contra o canvas,
inverteu-se a responsabilidade — **com um modal aberto, o chrome do
editor NÃO se desenha** (toolbar, separador 3D|UI, viewport 2D — logo o
canvas UI —, gizmos, Hierarchy, Inspector) e no lugar desenha-se um
**backdrop opaco** (`drawModalBackdrop`: um painel BG #141414 no ecrã de
pendências) por baixo do overlay. Como os widgets são **immediate-mode**, não
desenhados = não interativos: os toques pertencem SÓ ao modal (que fecha
com toque fora como sempre) — sem gestos "às escuras" na toolbar/painéis
tapados. Ao fechar, o editor volta INTEIRO. Em Play nada muda (a entrada
em play já fechava todos os overlays). `anyOverlayOpen` foi publicado no
`UiEditor.h` e **completado**: CENAS (`scenesMenu`), navegador
(`fileBrowser`) e aplicar (`applyAsk`) não estavam na lista — nem o gesto
WYSIWYG bloqueavam por baixo deles.

**(b) TIC de UI**: `ensureUiTic(scene, st)` — procura o TIC **"UI"** pelo
nome (cria se não existir: só `UiCanvas`, **sem mesh/body**), garante o
canvas e seleciona-o. `uiAddElement` SEM TIC selecionado (em modo UI)
passa por ele. O TIC "UI" aparece na Hierarchy como qualquer outro
(renomeável/duplicável/removível; um TIC "UI" renomeado deixa de ser
reconhecível pelo nome — o próximo ensure cria um novo: comportamento
previsível e documentado). Com um TIC selecionado, o comportamento 0.7.0
mantém-se (o canvas anexa ao TIC selecionado — o caso Player).

**(c) Teclado**: toggle **abc/ABC** (`kKbCaseId`) na linha de baixo — o
espaço encolhe de 3u para 2u e a tecla nova ocupa 1u (geometria da FONTE
ÚNICA `keyboardLayout`, aferida sem sobreposição). O estado vive em
`EditorState::kbLower` (reset no `android_main` de reentrada); o rótulo
mostra o **estado SEGUINTE** (maiúsculas ativas → "abc"); as letras
segem o caso, dígitos/'_' não mudam; `_`, `-`, espaço, APAGA, OK, X
mantêm-se. `keyLabel(row, col, lower)` ganhou o parâmetro (default
false — 0.7.3-compat).

## 4. Implementação (por ficheiro, commit `38da789`)

`ui/UiEditor.h/.cpp` — `anyOverlayOpen` público e completo (+CENAS/
navegador/aplicar); `drawModalBackdrop`; `ensureUiTic`; `uiAddElement`
com o fallback do TIC de UI (só em `st.uiMode`); `KeyboardLayout` com
`caseKey` + `keyLabel(row, col, lower)`; `keyboardLayout` com a linha de
baixo de 6 teclas; `drawTextInput` com o botão abc/ABC e as letras no
caso ativo; **fix do bug latente 0.7.0**: a linha S..Z tinha o **'Z' em
falta** (8 inicializadores para 9 teclas — a 9ª era um ponteiro NULL,
tecla fantasma que desenhava vazia e CRASHAVA o `typeChar` ao tocar).

`ui/EditorUi.h` — `EditorState::kbLower`.

`platform/main.cpp` — o gate do chrome (`const bool modalOpen =
anyOverlayOpen(...)`: backdrop quando aberto; chrome quando fechado) nos
blocos gizmo/toolbar/separador+viewport/gizmo-toolbar e Hierarchy+
Inspector; dispatch do "+" com o toast "TIC 'UI' criado + elemento" +
linha no engine.log quando nasceu o TIC.

`tests/test_uieditor.cpp` — o `Env::frame` replica o gate do main
(backdrop com modal, chrome sem modal, MENU desenhado como overlay);
teste do contrato novo do `uiAddElement` sem seleção; geometria do
teclado com 6 teclas na linha de baixo + rótulos minúsculos; 2 testes
novos (ver §6).

## 5. Decisões técnicas relevantes

1. **Backdrop opaco (não dim)**: o critério do C33 é "nenhum elemento do
   canvas visível por cima do overlay" — um dim deixaria o texto do
   canvas a atravessar (com menor contraste). Opaco = zero mistura;
2. **Chrome por omissão em vez de backdrop + chrome**: esconder o chrome
   (a) tapa o canvas POR CONSTRUÇÃO, (b) desativa os gestos tapados
   (immediate-mode) e (c) pouda o desenho — três efeitos num só gate;
3. **Toque fora continua a fechar o modal**: o backdrop é SÓ um quad (sem
   id de widget) — não captura gestos; o `pressedOutside` de cada overlay
   mantém o comportamento de sempre;
4. **"UI" por nome, sem flag escondida**: o TIC de UI é um TIC normal
   (serializa/duplica/renomeia como qualquer outro); nenhuma metadata
   especial no `.goni`;
5. **O rótulo do toggle mostra o estado SEGUINTE** (convenção dos
   teclados móveis: "abc" quando se está em maiúsculas);
6. **O 'Z' em falta foi corrigido na própria tabela** (A..R 2×9 + S..Z 8
   + '_' = 9) — em vez de esconder a tecla fantasma, o alfabeto ficou
   completo como a 0.7.0 pretendia.

## 6. Testes novos (CI Linux)

`tests/test_uieditor.cpp` — 399→401 (+2, e 1 reescrito ao contrato novo):

- `uieditor_overlay_modal_tapa_o_canvas` — com o MENU DE FICHEIROS aberto
  (e de novo com o MENU CONTEXTUAL): existe um quad OPACO que cobre o
  ecrã de pendências, **nenhum quad do elemento do canvas** é emitido (o teste
  desenha um Label com fundo no centro e afera a ausência do rect
  transformado), e ao FECHAR o menu o chrome + canvas voltam inteiros
  (sanidade antes/depois incluída);
- `uieditor_teclado_minusculas_apos_toggle` — 'A' maiúsculo por default;
  toggle → 'k' minúsculo; dígito '5' sem caso; toggle de volta → 'B'
  maiúsculo; `uiTextCharAllowed` já aceitava ambos os casos;
- `uieditor_plus_modo_ui_cria_elementos_no_canvas` (atualizado) — SEM TIC
  selecionado: o `uiAddElement` cria o TIC "UI" (SÓ com UiCanvas, **sem
  MeshRenderer, sem BodyComp**), seleciona-o e cria o elemento; a segunda
  criação REUTILIZA o mesmo TIC (sem "UI.001"); geometria do teclado com
  a linha de baixo de **6 teclas** sem sobreposição (o espaço 2u + abc
  1u + '-' + APAGA + OK + X) e rótulos minúsculos via `keyLabel(_, _,
  true)`.

## 7. Commits da sub-fase (branch main)

- `38da789` — 0.7.5-a: z-order dos overlays modais (backdrop + gate do
  chrome) + TIC de UI próprio (`ensureUiTic`) + teclado com minúsculas
  (abc/ABC + fix do 'Z' em falta) + testes (399→401).
- 0.7.5-b — este RELATÓRIO 0.7.5 + sha256 do APK assinado do run da
  sub-fase.

## 8. HEAD da sub-fase

`38da789` (0.7.5-a — o código; o commit do relatório vem imediatamente
por cima).

## 9. Suíte de testes

**401 OK / 0 falhas** (baseline 399; +2 novos; 1 reescrito ao contrato
novo do TIC de UI).

## 10. CI

Run `36810967850` (commit `38da789`) — **100% verde**: core-tests (401
OK + check_main + link_parity 75 TUs + jni_parity), build-release (APK
assinado), verify-entry-symbols (símbolos + manifest binário).

## 11. APK

`goni-vv-0.7.5-release-signed` (artifact do run `36810967850`),
versionCode 25 / versionName 0.7.5, arm64-v8a, **APK CUMULATIVO** (a
campanha 0.7 + os fixes 0.7.4 e 0.7.5 sobre a 0.6.7→0.6.10). sha256 do
`app-release.apk`:
```
508204f9dd31ab76824fabf76f904caab04d588a3a7e11696604e46ec306845f
```

## 12. Verificação no device (roteiro C33 — resumo)

1. Modo UI com elementos no canvas → abrir MENU: o ecrã fica todo tapado
   e SÓ o menu aparece (zero texto do canvas à mista); o mesmo com menu
   contextual/teclado/CENAS/navegador; toque fora fecha e o editor volta
   inteiro;
2. Sem nada selecionado → modo UI → "+" → Label: nasce o TIC "UI" na
   Hierarchy (só com UiCanvas) e o elemento dentro dele (toast "TIC 'UI'
   criado + elemento"); um segundo elemento reutiliza o mesmo TIC; com o
   Player selecionado o comportamento de sempre mantém-se;
3. Renomear → teclado → "abc" → minúsculas ("cena2"); "ABC" volta; a
   tecla Z escreve (o fix da tecla fantasma);
4. Regressões: paridade editor↔Play, texturas, containers, joystick,
   cenas, navegador, gestão de TICs — tudo como antes (roteiro completo
   no README).

## 13. Riscos e mitigações

- **O backdrop esconde o contexto do editor** (intencional — é um modal):
  o toast e a status line continuam visíveis (desenhados depois);
  transições de cena por cima de tudo (0.7.1) intactas;
- **TIC "UI" renomeado**: deixa de ser achado pelo `ensureUiTic` → um
  novo "UI" nasce na próxima criação sem seleção. Aceito e documentado
  (nome é identidade; flag escondida seria mágica);
- **`kbLower` não persiste** entre aberturas do teclado (estado de UI
  transitório, como `elDrag`) — decisão, não bug.

## 14. Dívida técnica conhecida

- O gate do chrome no main é por blocos (gizmo/toolbar/viewport/painéis)
  — um dia pode virar um `drawEditorChrome()` único se crescer;
- Os 3 overlays que faltavam no `anyOverlayOpen` foram acrescentados
  ad-hoc; a lista vive num sítio só (UiEditor.cpp) e é testada
  indiretamente pelos testes de backdrop.

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só z-order de overlays + TIC de UI + teclado + testes. **Zero física,
zero scripting, zero novas features** (o backdrop/gate/TIC/tecla abc são
os próprios fixes pedidos). Tema mono (nenhuma cor nova); landscape;
safe-area intactos; "nada sobreposto" — agora também entre modais e
canvas — aferido no CI.

## 16. Conclusão

As três falhas de UX do C33 fechadas: os overlays modais são modais de
verdade (backdrop opaco + zero interação por baixo), criar UI não exige
TIC 3D (nasce o TIC "UI" próprio, visível na Hierarchy) e o teclado
escreve minúsculas com um toggle visível — de caminho, o teste novo
apanhou e corrigiu um **crash latente da 0.7.0** (tecla 'Z' em falta na
linha S..Z). Suíte 401 OK; CI 100% verde; APK assinado cumulativo
publicado. As duas tarefas pós-campanha 0.7 (0.7.4 qualidade + 0.7.5 UX)
estão fechadas — prontas para VERIFIED no C33.
