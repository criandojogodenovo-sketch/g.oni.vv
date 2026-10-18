# RELATÓRIO 0.7.3 — Joystick editável + compostos (Menu / Card / Article)

## 1. Objetivo

Quarta e última sub-fase da campanha 0.7 (F6): (a) o joystick/TouchControls
como widget de UI ADICIONÁVEL e EDITÁVEL (pos/size/cor/sensibilidade) —
"a UI do Player passa a ser esta instância"; (b) os compostos Menu (lista
vertical de botões), Card (panel+borda+label) e Article (texto multilinha
com wrap); (c) testes: o joystick editável afeta o input em Play; os
compostos desenham e serializam. Com isto fecha a campanha: UI criável
completa com releases 0.7.0→0.7.3.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `27d24b3` (fecho da 0.7.2 — relatório; código no
  `e3a601e`; suíte 380 OK; release 0.7.2 com versionCode 22).
- O joystick era FIXO (layout hardcoded no TouchControls.cpp desde a
  F4-D: 150px/75px, sem persistência — "NÃO-CRIÁVEL" estava escrito no
  comentário original); os compostos existiam no RUNTIME (desenho/hit-test
  desde a 0.7.0) mas não eram criáveis pelo "+".

## 3. Arquitetura escolhida

- **Layout DERIVADO dos campos** (`layoutFor`): pos em FRAÇÕES da área
  útil (resolução-independente — o defeito do layout fixo em px), tamanho
  como escala do raio (base 75px), sensibilidade como multiplicador do
  eixo com CLAMP no círculo unitário. O default (0.09375/0.7361)
  reproduz o layout fixo 0.6.x no ecrã de referência 1600×720; o
  `layout()` estático fica para compat.
- **Um só caminho de layout**: `touchBegin` (o input) e `drawTouchControls`
  (o desenho) usam o MESMO `layoutFor` — o que se edita é o que corre em
  Play (aferido: o claim acontece na posição NOVA e o eixo escala com a
  sensibilidade).
- **Proxy no viewport 2D**: o joystick do TIC selecionado desenha-se com
  a geometria do Play escalada pelo transform do editor (o mesmo
  partilhado por draw/hit-test/drag dos elementos); arrastável com clamp
  0..1 nas frações; selecionável MESMO SEM canvas (o early-return da
  dica só corre quando não há canvas NEM joystick).
- **O Inspector do joystick** segue o contrato do plano (y cumulativo,
  `uiJoystickPlan` como fonte única partilhada com os testes): pos X/Y,
  tamanho, sensib., cor R/G/B, remover (tira o COMPONENTE; o InputMap
  fica — sem fonte até nova adição).
- **Compostos como ELEMENTOS** (kinds 4..6 do UiCanvas): o Menu é
  texto-orientado — uma linha por item no formato "label>alvo" (o alvo da
  linha sobrepõe-se ao alvo do elemento na ação); o Article faz wrap
  greedy por palavras medindo com a fonte real; o Card é composição
  pura de desenho. A criação entra pelo "+" (8 itens no modo UI).

## 4. Implementação (por ficheiro, commit `0ae9149`)

- **`components/TouchControls.h/.cpp`** — campos `relX/relY/size/sens/
  colR/colG/colB`; `layoutFor` (frações da área útil; jump ancorado à
  direita e alinhado ao joystick); `touchBegin` usa o layout editável;
  `axis()` multiplica pela sensibilidade com clamp; `layout()` estático
  mantido (compat).
- **`core/SceneSerializer.cpp`** — TouchControls grava `pos`/`size`/
  `sens`/`color` SÓ quando não-default (os `.goni` 0.6.x de presença-só
  abrem com os defaults); `fillTouchControls` novo.
- **`ui/UiEditor.h/.cpp`** — proxy do joystick no `drawUiViewport` (com
  rótulo; sem canvas continua editável); drag do joystick (relX/relY com
  clamp); `uiJoystickPlan`/`uiJoystickContentHeight`;
  `drawJoystickInspector` (sliders + remover re-despachado);
  `uiAddElement` aceita kinds 4..6; `UiInspRow::Kind::Sens`.
- **`ui/EditorUi.h/.cpp`** — `EditorState.selJoystick`; o "+" do modo UI
  com 8 itens (Panel/Label/Button/Image/Menu/Card/Article/Joystick).
- **`platform/main.cpp`** — o Inspector de UI abre com elemento OU
  joystick selecionado; o dispatch do "+" trata o item 8 (adiciona o
  TouchControls ao TIC e seleciona-o); banner 0.7.3 de fecho.
- **`ui/EditorLayout.h`** — ids do inspector do joystick (8020..8030).
- **`README.md`** — escopo 0.7.3 + roteiro de verificação C33 cumulativo
  de fecho da campanha.

## 5. Decisões técnicas relevantes

- **Pos como FRAÇÕES da área útil** (não px): o defeito do layout fixo
  era só funcionar bem num ecrã; com frações o joystick editado num
  dispositivo aparece proporcionalmente correto noutro (a mesma lógica
  das âncoras da UI criável);
- **Sensibilidade com CLAMP no círculo unitário**: sens 2 não deixa o
  eixo sair de [-1,1] — só chega lá com metade do curso (o contrato
  testado: 75px com sens 1 = 1.0; 37.5px com sens 2 = 1.0);
- **Serialização condicional**: campos só quando não-default — um
  `.goni` da 0.6.x com TouchControls de presença-só abre com o layout
  fixo de sempre (zero migração necessária);
- **O botão jump não é arrastável nesta fase**: o ancorário à direita
  mantém-se (o par joystick/jump desloca-se junto em Y, alinhado);
  arrastar o jump isolado fica como dívida explícita.

## 6. Testes novos (CI Linux)

`tests/test_joystick.cpp` (6 casos) — layout derivado dos campos (o
default reproduz o fixo no ecrã de referência; pos/tamanho mandam; em
outra resolução o default ESCALA; o layout estático continua), o edit
afeta o INPUT em Play (`touchBegin` claima na posição NOVA; o eixo vem
×sens com clamp — 30px×sens2=0.8, raio cheio=1.0 sempre; sens 1 = o
0.6.x exato; fora do joystick novo NÃO claima), serialização (round-trip
de pos/size/sens/color; `.goni` 0.6.x abre com defaults), proxy no
viewport 2D (desenha SEM canvas; tap seleciona; drag move relX/relY pelo
delta em design px com clamp; o plano do inspector tem 9 linhas com y
cumulativo; "remover joystick" tira o componente e limpa a seleção),
compostos (Menu com 3 itens desenha as 3 linhas; Article comprido faz
wrap — dezenas de glifos DENTRO da largura do elemento; round-trip
completo dos três; `uiAddElement` aceita 4..6 e recusa 7), o "+" do modo
UI com 8 itens (o oitavo adiciona o TouchControls e SELECIONA-o).

`tests/test_physics.cpp` — o teste do eixo passou a usar `layoutFor` (o
contrato novo: o input segue o layout editável).

## 7. Commits da sub-fase (branch main)

- `0ae9149` — 0.7.3-a: TouchControls editável + proxy/inspector do
  joystick + compostos criáveis + testes (380→386).
- 0.7.3-b — este RELATÓRIO 0.7.3 + sha256 do APK assinado do run da
  sub-fase (o FECHO da campanha 0.7).

## 8. HEAD da sub-fase

`0ae9149` (0.7.3-a — o código; o commit do relatório vem imediatamente
por cima).

## 9. Suíte de testes

**386 OK / 0 falhas** (baseline da campanha: 337 na 0.6.10; +49 no
total: 30 uicanvas/uieditor + 5 scenes + 8 browser + 6 joystick).

## 10. CI

Run `36774135222` (commit `0ae9149`) — **100% verde**: core-tests (386
OK + check_main + link_parity 75 TUs + jni_parity), build-release (APK
assinado), verify-entry-symbols (símbolos + manifest binário).

## 11. APK

`goni-vv-0.7.3-release-signed` (artifact do run `36774135222`),
versionCode 23 / versionName 0.7.3, arm64-v8a, **APK CUMULATIVO** (a
campanha 0.7 inteira: UI criável + editor de UI + gestão de TICs + cenas
com transições + navegador com galeria + joystick editável + compostos,
sobre a 0.6.7→0.6.10). sha256 do `app-release.apk`:
```
0a554e29b3b4ee5b8178437debd07505bd2dfb046c2ca40419dca182d0de0462
```

## 12. Verificação no device (roteiro C33 — resumo)

1. Player selecionado → modo UI → o joystick aparece como quadro com
   knob (mesmo sem elementos);
2. arrastar move; selecionar abre o INSPECTOR DO JOYSTICK (pos/tamanho/
   sensib./cor/remover);
3. Play → o joystick está ONDE se pôs, do TAMANHO escolhido; sensib. 2
   dobra a resposta (o eixo escala); a física segue;
4. "+" → Joystick adiciona o componente e seleciona-o; "remover
   joystick" tira-o;
5. Save → Load preserva pos/tamanho/sens/cor (projeto 0.6.x abre no
   sítio de sempre);
6. "+" → Menu/Card/Article: Menu com "Jogar>cena2" por linha dispara a
   transição da cena alvo; Article comprido faz wrap; Card com título;
7. regressões de toda a campanha (navegador/cenas/UI/gizmos/seletores).

## 13. Riscos e mitigações

- **Joystick fora do ecrã** (frações extremas): clamp 0..1 no drag e nos
  sliders (o range do Inspector é 0..1);
- **Sensibilidade alta + clamp**: o eixo nunca sai do círculo unitário —
  a física vê valores no contrato de sempre;
- **O botão jump sobrepõe-se à UI criável?** o jump fica à direita e é
  desenhado DEPOIS do canvas (por cima) — input prioritário, o mesmo
  z-order da 0.7.0 (documentado no drawCanvasPlay).

## 14. Dívida técnica conhecida

- O botão jump não é arrastável/editável isoladamente ( ancorário à
  direita);
- O proxy do joystick não mostra o botão jump no editor (só o joystick —
  o jump aparece em Play);
- Os compostos não têm pré-visualização no "+" (só o rótulo).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só joystick editável/compostos + testes: zero física nova (o PhysicsSystem
continua a ler o MESMO InputSource — só os valores mudam), zero scripting
(V.ONI=F9.1), zero componentes de gameplay novos (o TouchControls é o
componente da F4 com campos). Tema mono (a cor do joystick é DADOS do
utilizador, default = LINE); landscape; safe-area.

## 16. Conclusão

A 0.7.3 fecha a campanha 0.7 com o último "NÃO-CRIÁVEL" do editor a cair:
o joystick é agora uma instância editável cujos campos mandam no input e
no desenho pelo mesmo caminho (`layoutFor`), e a caixa de ferramentas da
UI ganha os compostos que fazem do canvas uma interface de jogo de
verdade (menus, cartões, artigos). A campanha inteira manteve a
disciplina da casa: política pura e afervel, geometria partilhada com os
testes, honestidade nos caminhos de falha e releases com CI verde e APK
assinado em CADA sub-fase (0.7.0→0.7.3, versionCode 20→23, 337→386
testes). O sistema de UI criável da engine (F6) está completo.
