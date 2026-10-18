# RELATÓRIO 0.7.1 — Cenas múltiplas + transições (fade / slide)

## 1. Objetivo

Segunda sub-fase do F6: (a) criar cenas novas com nome, listar as cenas do
projeto e trocar de cena no editor, com cada cena num `.goni` próprio;
(b) `Scene.Load` (troca instantânea) e `Scene.Transition` (fade/slide) como
ações declarativas dos botões da UI criável e funcionando em Play — a troca
acontece com o ecrã tapado, nunca a seco. Critério transversal mantido:
nada sobreposto nem desorganizado.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `23c1df5` (fecho da 0.7.0 — relatório; código no
  `5740e89`; suíte 367 OK; release 0.7.0 com versionCode 20).
- A base de cenas JÁ existia desde a F5 (`Project::scenes` + `addScene` +
  `activeScene` no manifesto, `saveActiveScene`/`loadActiveScene` por
  caminho) — mas sem UI: só havia UMA cena utilizável e a ação `LoadScene`
  declarativa da 0.7.0 chegava a um toast honesto ("sem carregador").

## 3. Arquitetura escolhida

- **Overlay CENAS** (Menu → "Cenas…"): a lista vive no MANIFESTO (fonte da
  verdade); a ativa é marcada; "+ Nova cena" abre o TECLADO in-app
  (propósito 1 — zero IME); a lista faz scroll (id 44) quando excede 6
  linhas. `sceneDisplayName` (basename sem extensão) é a FONTE ÚNICA do
  rótulo, partilhada com os testes.
- **Criar/trocar no main com a SEQUÊNCIA segura**: guardar a cena ATUAL no
  ficheiro dela ANTES de mexer na ativa; GUARD de duplicados antes do
  `addScene` (o `addScene` ATIVA a existente em vez de falhar — sem o
  guard, o `.goni` da cena existente seria APAGADO pela cena vazia; bug de
  perda de dados apanhado na revisão do contrato e travado com teste);
  troca falha de save ABORTA sem trocar (a cena atual fica intacta).
- **Transições PURAS (ui/SceneFx.h)**: `transitionStep` devolve true UMA
  vez no PONTO MÉDIO (o swap — o chamador troca de cena com o ecrã
  tapado); `transitionCover` em rampa 0→1→0 (partilhada com os testes);
  `transitionDraw` emite um QUAD PRETO (tema mono): fade = fullscreen com
  alpha; slide = cobre vindo da esquerda e sai pela direita. Inativa =
  custo zero (não emite nada).
- **Ações declarativas com ESTILO**: `UiActionCtx.loadScene` agora recebe
  `SceneSwap {Instant, Fade, Slide}` — `Scene.Load` (ação `scene`) =
  instantâneo; `Scene.Transition` (ação `trans`, nova) = fade (default) ou
  slide (campo `param` do elemento, serializado). O Inspector de UI ganhou
  a linha "estilo" (cicla) e o ciclo de ações inclui `trans`.

## 4. Implementação (por ficheiro, commit `61471cd`)

- **`ui/SceneFx.h/.cpp` (NOVOS)** — `SceneTransition` + `transitionStep` +
  `transitionCover` + `transitionDraw` (GL-free; batches aferíveis).
- **`ui/EditorUi.h/.cpp`** — `EditorState.scenesMenu`; Menu de ficheiro
  com 7 itens ("Cenas…" no 3; Export OBJ/Importar…/Export Downloads/Sair
  deslocados para 4..7); `drawScenesMenu` + `sceneDisplayName`;
  `closeAllOverlays` fecha o overlay de cenas.
- **`ui/UiEditor.h/.cpp`** — Inspector de UI: linha "estilo: fade|slide"
  (id 8017; cicla) quando a ação é `trans`; ciclo de ações com 7 estados.
- **`components/UiCanvas.h`** — `Action::TransitionScene` + enum
  `Transition` + campo `param` + `uiElementTransition`.
- **`ui/UiRuntime.h/.cpp`** — `SceneSwap`; `UiActionCtx.loadScene` com
  estilo; `applyUiAction` trata `scene`/`trans` (toast com o estilo;
  honestidade intacta).
- **`core/SceneSerializer.cpp`** — `act: "trans"` + `param` (ausente =
  fade).
- **`platform/main.cpp`** — `doSwitchScene` (save→ativa→manifesto→load;
  seleção limpa; catálogo refrescado), `startSceneTransition`,
  `loadSceneByName` (o callback da ação; em Play anda na transição),
  `createSceneNamed` (com o guard de duplicados), overlay CENAS + teclado
  propósito 1 no frame, `transitionStep` no início do frame (swap no
  ponto médio) + `transitionDraw` por cima de TUDO no fim (editor e
  play), `g_frameDt` do loop real; reentrada reseta a transição.
- **`README.md`** — escopo 0.7.1 + roteiro de verificação C33.

## 5. Decisões técnicas relevantes

- **O swap no PONTO MÉDIO** (não no início): a troca de cena é a operação
  mais pesada do fluxo (save + load + rebind de refs) — fazê-la com o
  ecrã tapado esconde o custo e o "salto"; testado que o swap acontece
  EXATAMENTE uma vez e com cover = 1.
- **A troca pelo overlay é DIRETA (sem transição)**: o overlay CENAS é
  editor-only (em Play a troca vem pela ação declarativa com transição) —
  dois caminhos com dois contratos claros.
- **`Project::addScene` duplicado = ATIVA** (contrato pré-existente): o
  guard do `createSceneNamed` protege o `.goni` existente ANTES de o
  `addScene` ativar a cena e a vazia sobrescrever o ficheiro.
- **`param` como string** (não enum): serializa/deserializa com
  forward-compat (ausente = fade) e o editor cicla sem parser extra.

## 6. Testes novos (CI Linux)

`tests/test_scenes.cpp` (5 casos) — criar/trocar/persistir cenas e2e no
FakeStorage (A→B→A com round-trip dos TICs de cada cena; duplicado SÓ
ATIVA sem duplicar; reopen mantém a ativa; a cena A fica intocada no
ficheiro dela), transição pura (swap UMA vez no ponto médio com cover=1;
cover em rampa 0/0.5/1/0.5/0; inativa = custo zero), draw fade/slide nos
batches (fade: fullscreen com alpha 0.5 a meio da subida; slide: ida
x=0 com meia largura, volta x0>0 colado à direita; inativa não emite),
overlay CENAS (lista 3 cenas com nome de exibição, tap troca pick=4,
"+ Nova cena" pick=1, fora fecha, teclado propósito 1), ações declarativas
(Load=instant; trans fade default; slide por param; cena inexistente →
toast honesto SEM callback; round-trip `.goni` com act trans + param; um
`.goni` 0.7.0 com `trans` SEM param abre com fade default).

`tests/test_uicanvas.cpp` — o teste de ações estendido ao contrato dos
estilos (instant/slide/fade pelo callback).

## 7. Commits da sub-fase (branch main)

- `61471cd` — 0.7.1-a: SceneFx + overlay CENAS + ações scene/trans +
  wiring do main + testes (367→372).
- 0.7.1-b — este RELATÓRIO 0.7.1 + sha256 do APK assinado do run da
  sub-fase.

## 8. HEAD da sub-fase

`61471cd` (0.7.1-a — o código; o commit do relatório vem imediatamente
por cima).

## 9. Suíte de testes

**372 OK / 0 falhas** (baseline 367; +5 em test_scenes.cpp). Corrida
local e no CI (`core-tests`, ctest).

## 10. CI

Run `36770839644` (commit `61471cd`) — **100% verde**: core-tests (372 OK
+ check_main + link_parity com 75 TUs + jni_parity), build-release (APK
assinado), verify-entry-symbols (símbolos + manifest binário).

## 11. APK

`goni-vv-0.7.1-release-signed` (artifact do run `36770839644`),
versionCode 21 / versionName 0.7.1, arm64-v8a, **APK CUMULATIVO** (cobre
0.7.0 + 0.6.7→0.6.10). sha256 do `app-release.apk`:
```
21f202ea3e746aee63fe1b58cd628f494f6b59a6f00e58bfb86e0ef8843daa6a
```

## 12. Verificação no device (roteiro C33 — resumo)

1. Menu → "Cenas…" → lista com a ativa marcada;
2. "+ Nova cena" (teclado in-app) → cena vazia ativa; trocar de volta
   devolve os TICs da outra (nada se perdeu); nome duplicado → toast;
3. Sair/reabrir → a cena ativa persiste (manifesto);
4. Button com acao "trans" + alvo + estilo "fade" → Play → tocar → o
   ecrã escurece, a cena troca AO ESCURO e volta a clarear;
5. estilo "slide" → cobre pela esquerda, sai pela direita;
6. acao "scene" (Scene.Load) → troca instantânea (os dois convivem);
7. alvo inexistente → toast "cena 'xyz' nao existe";
8. regressões 0.7.0/0.6.x.

## 13. Riscos e mitigações

- **Trocar com transição em Play** → o PlaySnapshot fica com handles da
  cena ANTIGA; o restore ao Stop salta os mortos com segurança (o
  contrato do PlaySnapshot — testado desde a F4.2);
- **Save falha na troca** → aborta SEM trocar (a cena atual fica intacta)
  + toast + engine.log com a causa;
- **Nome de cena com caracteres estranhos** → o teclado in-app só aceita
  A-Z/0-9/_/-/espaço; o `addScene` recusa pontos/barras.

## 14. Dívida técnica conhecida

- O overlay CENAS não copia/duplica cenas (só criar/trocar — o que a spec
  pede); gerir a ordem das cenas fica para fase futura;
- A duração da transição é fixa (0.6 s) — configurável fica para a F8.

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só cenas/transições + testes: zero física nova, zero scripting
(V.ONI=F9.1), zero componentes de gameplay. O efeito visual da transição
é UM QUAD PRETO (tema mono — sem cores novas, sem assets). Landscape;
safe-area; o overlay vive dentro do contentRect.

## 16. Conclusão

A 0.7.1 liga o que a 0.7.0 deixou honestamente pendente: a ação
`LoadScene` agora tem carregador real, e a troca de cena em Play ganhou
transições puras e aferíveis (o swap único no ponto médio é o contrato
central, testado ao pixel). O fluxo de cenas usa o manifesto como fonte
da verdade com a sequência segura (guardar antes de trocar; duplicados
travados antes de destruir). Com cenas e UI criável a funcionar, a 0.7.2
traz o import robusto — o navegador com a galeria e o aplicar-após-import
que fecha o ciclo do asset no device.
