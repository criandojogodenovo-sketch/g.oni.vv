# RELATORIO-0.11-H — FASE 1 · HIERARQUIA (a árvore edita-se)

> versionCode 69 · 0.9.6 · a FASE 1 do plano do dono (drag-reparent,
> ícone por tipo, apagar com filhos, duplicar com filhos, expandir/
> colapsar persistente) · 12 testes novos no test_core · suíte inteira
> verde no harness local (0 falhas, r039 incluída) · gates locais verdes

## 1. O ÂMBITO (o que a fase pede e o que a auditoria achou)

A spec da FASE 1 lista 5 itens e a cláusula anti-overengineering manda
só o que a fase pede. A auditoria ao código em pé (o commit de partida
e862479, «a FASE 1 começa no CONTINUAR») achou o que se segue — e o
plano honrou cada achado:

| Item da spec | O que JÁ existia | O que faltava (feito nesta fase) |
|---|---|---|
| Drag-reparent | Os campos `hierDrag/hierDragSrc/hierDragDst` em `EditorState` eram PLACEHOLDER declarado desde 0.9.0 — nenhum uso no fonte (o gesto não existia) | O gesto completo (claim do slider + alvo pelo dedo + drop no vazio = raiz + recusa honesta de ciclo) + o consumo no main com undo |
| Ícone por tipo | `hierIconFor` (FASE 9 · G2-7) já distinguia Camera/Body×3/Audio/UI/Mesh/Input — o «hoje só cubos genéricos» da spec estava desatualizado para esses tipos | ScriptComp → Terminal e AnimationPlayer → Clapper (saíram do cubo genérico). A LUZ é da FASE 5: não há componente de luz na engine hoje — nada para consultar (limitação DECLARADA no `hierIconFor` e aqui) |
| Apagar com filhos | O diálogo de confirmação existia (`drawRemoveDialog`); o delete era SÓ o TIC (`Scene::destroy`) — os filhos ficavam com `parent` para um slot REUTILIZÁVEL (corrupção latente: o próximo `create` nascia «pai» dos órfãos) | `destroyWithChildren` (a sub-árvore INTEIRA, filhos primeiro) + o diálogo DIZ o alcance («e a sub-árvore (N TICs)?») |
| Duplicar com filhos | `duplicateTic` clonava UM TIC (sem filhos, sem undo — política de sempre) | `duplicateWithChildren` (deep clone: sub-árvore inteira, novos handles, nomes únicos, estrutura espelhada, componentes por valor) |
| Expandir/colapsar persistente | NADA (o walk do `drawHierarchy` visitava sempre tudo) | O chevron por TIC com filhos + o estado por NOME (`EditorState::hierCollapsed`) persistido no layout.json (o formato da casa, retrocompatível) |

O parêntelo/children em si (o `Tic.parent`, a composição do
`TransformSystem`, o serializer e o undo do snapshot) já estava feito —
a spec do dono diz isso e a auditoria confirmou. O que faltava era a
EDIÇÃO da árvore.

## 2. AS 5 PERNAS (o que foi feito, ficheiro a ficheiro)

### Perna 1 — As operações puras de árvore (`ui/UiEditor.h/.cpp`)

A fonte única da edição da árvore, tudo PURO sobre a Scene (sem GL, sem
storage — afervel no CI): `isDescendant` (caminho pelos pais com guard
de ciclo — o mesmo teto do `composeAt` do TransformSystem),
`canReparentTo` (raiz = sempre legal com src vivo; recusa src morto,
auto-parent e alvo DENTRO da própria sub-árvore), `reparentTic` (muda
SÓ o pai — o TRS local fica: a matriz da casa não tem inverse/
decompose, o «manter transform mundial» fica para uma fase futura se o
dono quiser; o `TransformSystem` recomputa os mundos no próximo tick),
`collectSubtree`/`countSubtree` (BFS por slots, root primeiro, guard de
ciclo com visitados — o padrão dos walks do hotfix P0),
`destroyWithChildren` (filhos PRIMEIRO — nunca fica parent pendurado
para slot reutilizável) e `duplicateWithChildren` (o mapa slot de
origem → handle do clone espelha a estrutura; o root clona com o pai do
original, os filhos com o clone do SEU pai). O corpo do clone raso saiu
do `duplicateTic` para o `cloneTicInto` partilhado (UMA fonte de
verdade — o duplicar de 1 TIC fica byte a byte) e o nome único
Godot-style virou `uniqueTicName` (contra TODOS os vivos — o `find` da
cena continua determinístico).

### Perna 2 — O drag-reparent na UI (`ui/UiContext.h/.cpp` + `ui/EditorUi.cpp`)

Duas primitivas novas no UiContext (as ÚNICAS da fase):
`dragClaim` (a pega de arrasto com prioridade de SLIDER — captura
DENTRO da região de scroll pelo `scroll::sliderCaptures`, o mesmo
protocolo do `sliderCore`; sem registo no audit, o mesmo do
`dragHandle`) e `pointerPos` (leitura pura da pos do dedo, o par do
`pointerDownAt`). Mais o `releaseClaim` inline (a limpeza EXPLÍCITA de
um claim órfão — a linha reclamante pode morrer a meio do gesto).

O GESTO (a máquina no `drawHierarchy`, o estado é todo do
`EditorState`):
- press na zona do ÍCONE DE TIPO → `hierDragArm` (o claim é DESTE
  widget — o scroll nasce no RESTO da linha: drag no ícone = drag,
  drag no nome/olho/⋮ = scroll como sempre);
- movimento ≥ `scroll::kTapPx` (o limiar do scroll) → `hierDrag` +
  o ALVO segue o dedo (`updateDragTarget` corre DEPOIS do loop de
  linhas — o frame da transição inclui-se: um flick de 2 frames não
  perde o alvo). Alvo LEGAL acende (barra âmbar no topo da linha);
  alvo ILEGAL (ciclo) não acende; vazio da lista = RAIZ; fora da
  lista = cancelar;
- release com drag + alvo legal → ARMA `hierReparentPending`
  (o padrão dos pedidos do estado); release sem movimento → o tap
  de sempre (o toggle da multi-seleção — o contrato antigo sobrevive
  ao claim novo, pin dedicado);
- o FANTASMA: o ícone de tipo da TIC arrastada segue o dedo (o dedo
  cobre a linha de origem no device);
- o GUARD do claim órfão: a linha reclamante saiu do array visível a
  meio do gesto → cancela + `releaseClaim` (sem isto o `active_`
  preso bloqueava TODOS os gestos seguintes).

O CONSUMO no main (`platform/main.cpp`, o ponto seguro do frame):
revalida com `canReparentTo` (a cena pode ter mudado), empurra o undo
do snapshot (o reparent é UMA edição de UM TIC — o `TicSnap` cobre, o
botão Desfazer/Refazer do menu ⋯ restaura o pai de origem), toast +
linha de log «hierarquia: 'X' reparentado para 'Y'/RAIZ» ou a recusa
honesta. FIX DE PASSAGEM: o despacho do tap calculava o iconX em px
CRUOS (`x + 16 + 24*depth`) enquanto o desenho usa `theme::dp` — no
device com densidade ≠ 1 as zonas divergiam (o tap da multi-seleção
caía fora do ícone); agora desenho e toque partilham a MESMA fórmula
em dp (o padrão da casa).

### Perna 3 — Expandir/colapsar persistente (`ui/EditorUi.h/.cpp` + `ui/BottomPanel.h/.cpp` + main)

O chevron (16dp, `ChevronDown`/`ChevronRight` do conjunto existente —
zero ícones novos) só nasce em TIC com filhos (uma passada
`hasKidsTab`). O tap na zona do chevron (24dp antes do ícone — TODAS
as linhas deslocam +24dp, grelha consistente) alterna o estado, que é
POR NOME em `EditorState::hierCollapsed` (`hierCollapsedHas`/
`hierCollapsedToggle` inline — a chave é o nome porque os handles
morrem entre sessões; nomes repetidos partilham o estado, a limitação
documentada). Com PESQUISA ativa o colapso é IGNORADO (a pesquisa
mostra tudo o que casa — a decisão da spec B de sempre). A persistência
segue a casa das preferências de UI: `serializeLayout`/`parseLayout`
ganham as linhas `hierColN=<n>` + `hierCol=<nome>` (out-param opcional
— os chamadores antigos ficam byte a byte; o out-param é reposto
SEMPRE; um nome com '\n' nunca se grava — a defesa é na fonte). O save
entra no MESMO debounce G1-5 (o draw arma `hierCollapsedDirty`, o
`layoutSaveTick` consome com o motivo «árvore recolhida/expandida»). A
TROCA de cena limpa a lista (o estado é da cena aberta — a lista do
layout.json é a da cena ativa ao gravar; a recarga da MESMA cena no
arranque reabre com o estado gravado — o caso «entre sessões» da
aceitação).

### Perna 4 — Apagar com filhos (UiEditor + main)

O menu ⋮ → Remover conta a sub-árvore (`countSubtree`) e o diálogo
DIZ: «Remover 'X' e a sub-árvore (N TICs)?» (com UM TIC o texto é o de
sempre, byte a byte). Confirmar com sub-árvore → `destroyWithChildren`
(SEM undo — o op do `UndoStack` cobre UM TIC, sub-árvore não cabe; o
diálogo já diz «não há desfazer», a decisão documentada). Confirmar
com UM TIC → o caminho de 0.9.0 de sempre COM undo (byte a byte). A
corrupção latente morre: os filhos nunca sobrevivem com parent para
slot reutilizável (pin dedicado: criar sobre os slots libertados
produz TICs limpos).

### Perna 5 — Ícones por tipo (EditorUi.cpp)

`hierIconFor` ganha os dois tipos que faltavam: ScriptComp → Terminal
e AnimationPlayer → Clapper (ícones que JÁ existem no conjunto — zero
geometria nova). Camera/Body×3/Audio/UI/Mesh/Input já distinguiam
desde a FASE 9 G2-7. A LUZ: NÃO IMPLEMENTADA nesta fase porque não há
componente de luz na engine (a FASE 5 do dono é que as cria) — o
comentário no `hierIconFor` declara o placeholder; o ícone de luz
nasce com o componente, na 0.11-L.

## 3. AS PROVAS (12 pins novos — tests/test_fase1_hierarquia.cpp)

1. `fase1_reparent_puro_recusas_e_filhos_acompanham` — a ACEITAÇÃO da
   spec: o neto muda para a outra raiz e o BISNETO ACOMPANHA; as
   recusas (auto-parent, ciclo pelo descendente, mortos) não mexem em
   nada; o drop na raiz põe parent −1.
2. `fase1_reparent_undo_restaura_o_pai` — o MESMO caminho do botão
   Desfazer (UndoStack::undo/redo → restoreTic): o pai de origem volta
   e o redo re-aplica.
3. `fase1_apagar_com_filhos_e_a_contagem_do_dialogo` — countSubtree
   (folha 1, meio 2, raiz 4, morto 0), a ordem do collect (root
   primeiro), a sub-árvore INTEIRA morre, os slots reutilizados nascem
   limpos, a folha morre só.
4. `fase1_duplicar_com_filhos_deep_clone` — +4 TICs com IDs NOVOS,
   nomes únicos e distintos, estrutura ESPELHADA (os filhos penduram-se
   nos CLONES), componentes por valor (meshPath/texPath/pos), o
   original intacto, a folha = o duplicar de sempre, alvo morto →
   invalid.
5. `fase1_colapso_toggle_e_roundtrip_do_layout` — o toggle entra/sai;
   o round-trip do layout.json (hierColN/hierCol) devolve a MESMA
   lista, o out-param é REPOSTO (nunca acumula), o ficheiro ANTIGO
   lê vazio, o chamador sem out-param compila e corre.
6. `fase1_icones_por_tipo_script_e_animacao` — Terminal/Clapper nos
   tipos novos; Box/Cube/Camera mantêm-se (a luz = FASE 5, declarada).
7. `fase1_drag_reparent_ui_real_do_neto_para_a_outra_raiz` — a UI REAL
   com gestos injetados (o padrão do test_uieditor): press na zona do
   ícone do NETO (linha 2), move para a RAIZ2 (linha 4), release → o
   consumo aplica; os filhos acompanham; o gesto termina limpo.
8. `fase1_drag_para_o_vazio_da_lista_vai_para_a_raiz` — drop no vazio
   da lista (dentro da região) → RAIZ, com a sub-árvore a acompanhar.
9. `fase1_drag_ciclo_cancela_sem_aplicar` — arrastar a raiz para o
   PRÓPRIO filho: o alvo ilegal não acende e o release CANCELA (nada
   aplicado, nada mexido).
10. `fase1_tap_na_zona_do_icone_mantem_a_multi_selecao` — o tap sem
    movimento alterna no conjunto (entra e sai) — o contrato antigo
    sobrevive ao claim novo.
11. `fase1_chevron_colapsa_e_esconde_a_sub_arvore` — o tap no chevron
    recolhe (o estado fica sujo→consumido), a linha do filho DESAPARECE
    (o tap onde o filho era seleciona a OUTRA raiz — nunca o filho),
    o segundo tap expande e o filho volta, a folha não recolhe nada.
12. `fase1_colapso_persiste_no_layout_json` — o toggle pela UI chega ao
    serializeLayout e o parse devolve a MESMA lista (o caminho real do
    saveLayoutNow).

MUTAÇÃO (vermelho→verde, a prova de que os pins apanham): o pin 10
apanhou o conflito claim×tap durante o desenvolvimento — a 1.ª versão
do claim matava o tap da multi-seleção (o scrollTap já não via o gesto
da zona do ícone); a cura foi o toggle no finalize do release sem
movimento (pin verde). O guard do claim órfão e o `updateDragTarget`
pós-loop nasceram das MESMAS caçadas (o flick de 2 frames perdia o
alvo; o gesto ficava preso se a linha morresse).

## 4. AS DECISÕES E OS LIMITES (nenhum escondido)

1. **O reparent mantém o TRS LOCAL** (o Godot tem «keep global» — a
   matriz da casa não tem inverse/decompose; o mundo recomputa no
   próximo tick do TransformSystem). Se o dono quiser «manter
   transform mundial», é uma fase própria (Mat4::inverse + decompose
   TRS com os casos de escala negativa/não-uniforme).
2. **Apagar sub-árvore NÃO tem undo** (o op do UndoStack é
   por-UM-TIC; o diálogo diz «não há desfazer» — o apagar de UM TIC
   mantém o undo de sempre). A extensão multi-TIC do UndoStack é
   fase futura se o dono quiser.
3. **Duplicar não empurra undo** (o duplicar de sempre também não
   empurrava — política mantida).
4. **O colapso é por NOME** (os handles morrem entre sessões; nomes
   repetidos partilham o estado). **Por cena**: a troca de cena limpa a
   lista — a lista gravada é a da cena ativa ao gravar (cada cena
   reabre com o SEU estado quando for a ativa no arranque).
5. **O drag vive SÓ na zona do ÍCONE** (40dp da linha): o scroll da
   lista continua a nascer no resto da linha (o protocolo do slider);
   arrastar pelo chevron = scroll.
6. **A pesquisa ignora o colapso** (mostra tudo o que casa).
7. **A LUZ não tem ícone nesta fase** (não há componente de luz —
   FASE 5 do dono; declarado no hierIconFor).
8. **O c33_virtual NÃO foi estendido** (a fase não mexe no caminho do
   device que o replay aferva; os pins são do test_core). O c33 corre
   na suíte como sempre.
9. **As medições no C33 (device físico)**: a FASE 1 é edição de
   estrutura da UI — não há campanha de tempos (o orçamento do frame
   não muda: o walk da árvore continua O(n) com o teto de 64 linhas
   visíveis de sempre; as novas passadas — hasKids, o lookup do
   colapso, o target do drag — são O(64 × recolhidas) por frame e só
   com painel aberto). A prova de mesa do dono no device vale o §6.

## 5. OS GATES (a evidência do harness local)

- test_core: **0 falhas** (a suíte INTEIRA — 948 testes anteriores +
  os 12 novos; a sentinela r039 correu VERDE com o disco limpo — o
  flake de DISCO da sandbox documentado desde o hotfix P0, o CI é o
  árbitro).
- check_main (main.cpp/StorageBridge/OboeBackend com stubs): OK.
- link_parity (a lib inteira, 122+ TUs): OK.
- docs-lint 83 · hierarchy-check (o contrato ganhou a regra 29 — a
  árvore edita-se) · ui-vocab · theme-hex · glyph · gmesh-docs: VERDE.
- scope: os ficheiros desta fase estão TODOS dentro do ci/scope.txt
  (app/src/main/cpp/ + tests/ + docs/ + app/build.gradle + BACKLOG.md
  + README.md).
- O c33_virtual não compila neste sandbox sem cmake (o harness local
  cobre o test_core inteiro; o job do CI compila e corre o replay
  797/797 como sempre — nenhum caminho do replay foi tocado).

## 6. NÃO VERIFICADO (honesto)

- O SIGN-OFF no C33 físico (o dono): criar a árvore de 3 níveis, o
  drag-reparent do neto para outra raiz com o dedo, reabrir a cena (a
  árvore intacta), os ícones a distinguirem os tipos, o colapso a
  sobreviver ao fechar/abrir da app. Os pins cobrem a lógica no
  hospedeiro; o gesto REAL no ecrã do device é a prova de mesa.
- A suíte INTEIRA no CI (o árbitro): este relatório sai ANTES do push;
  o run do CI é que sela.
- A reordenação visual das linhas (+24dp do chevron) em painel no PISO
  140dp: o nome trunca por desenho como sempre (o tip do long-press é
  o acesso) — a conferência visual no device fica para o sign-off.

## 7. FICHEIROS ALTERADOS

- `app/src/main/cpp/ui/UiEditor.h/.cpp` — as operações de árvore + o
  diálogo com contagem + o clone raso partilhado.
- `app/src/main/cpp/ui/EditorUi.h/.cpp` — o estado novo (drag/pending/
  colapso), o walk com colapso, o chevron, o claim do drag, o
  despacho do tap em dp, os ícones novos.
- `app/src/main/cpp/ui/UiContext.h/.cpp` — dragClaim/pointerPos/
  releaseClaim.
- `app/src/main/cpp/ui/BottomPanel.h/.cpp` — hierColN/hierCol no
  layout.json.
- `app/src/main/cpp/ui/EditorLayout.h` — kHierDragBase (300000).
- `app/src/main/cpp/platform/main.cpp` — o consumo do reparent com
  undo, o apagar/duplicar com filhos, o save/load do colapso, a troca
  de cena limpa.
- `tests/test_fase1_hierarquia.cpp` (NOVO) + `tests/CMakeLists.txt`.
- `docs/LAYOUT_HIERARCHY.md` (a regra 29) · `docs/RELATORIO-0.9.6.md`
  (§14) · `BACKLOG.md` · `README.md` · `app/build.gradle` (vc 69).
