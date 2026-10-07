# RELATORIO-0.9.6.19 — HOTFIX: OS ABERTOS DO RE-SIGN-OFF + A CÂMARA GIGANTE + OS RESTOS

> O scope do dono, palavra a palavra: «R1 · VALORES DO TRANSFORM · D17 ·
> CÂMARA COMO OBJETO PEQUENO · D14 · MENU CONTIDO · D15 · STRINGS DO MENU
> NO GATE · D16 (condicional) · CAPTURA · D18 · INICIAIS · D19 · ESTADO
> VISÍVEL NO TOGGLE. Scope só isto. PASSO 4 bloqueado até novo sign-off.
> P-01/P-04 antes de editar; commit próprio; CI verde.»
> Este relatório é a seção 0.9.6.19 da campanha; o contrato P-08
> (docs/LAYOUT_HIERARCHY.md) recebeu as regras §2.16–§2.19 NO MESMO
> commit (as rects dos menus mudaram — px cru → dp).

## 0. O QUE ENTROU (resumo)

| Item | O que o dono pediu | O que ficou |
|---|---|---|
| R1 | o valor é intocável — dropa a letra do eixo, depois o padding, nunca o valor | `transformRowBudget` degrada por dentro (letra→padding→nunca o valor); piso do valor 26dp; pin provado em 180/220/260dp |
| D17 | a câmara é um objeto pequeno que vê | glifo 24dp constante + frustum fino 1-2px (mudo 35% sem seleção / âmbar com seleção) + handles de canto 12dp só com seleção + hit-test glifo > handles > frustum intocável |
| D14 | menu contido | abre com offset 0 (mesmo reabrindo com o slot sujo), nunca cruza a tab bar, última linha alcançável inteira + CAUSA-RAIZ: os sheets eram px cru (violão R-018) — o menu a meia medida com o texto a 2× |
| D15 | «Exportar OBJ» + o menu inteiro no gate | tabela do menu toda em PT; o gate caça os fragmentos EN na tabela; «Snapping» fica POR DECISÃO EXPLÍCITA na allowlist `TECNICOS_MENU` |
| D16 | captura: diagnóstico + cura | o pipeline JÁ corria (o thumb.png escreve-se — FASE 15.6); as CAUSAS do card com iniciais eram DUAS: (1) o frame capturado tinha o menu/backdrop aberto (o thumb saía com o MENU desenhado), (2) a leitura Java corria UMA vez antes da escrita e nunca repetia |
| D18 | iniciais 1ª+última letra | regra implementada (projetoyygf→PF, prooksnsn→PN); o matiz de hash mantém-se; o gate apanha o regresso das «2 primeiras» |
| D19 | toggle com estado | o switch da casa (pílula 32×16 + knob 12) desenha o estado nos DOIS valores, no Inspector 3D e no editor de UI |

**NADA mais tocado**: V.ONI, seleção (o resto), física, TICs, render do
jogo, storage, import. PASSO 4 SEGUE BLOQUEADO (re-sign-off do dono,
§7 aqui).

## 1. R1 — OS VALORES DO TRANSFORM (causa ficheiro+função)

- **Sintoma do dono**: «o clamp do D2 deixou os campos X/Y/Z sem o texto
  do valor (só a letra do eixo)».
- **Causa** (`ui/EditorUi.cpp`, o draw de `InspRow::Kind::TransformRow`):
  o D2 orçamentou as CAIXAS (floor 40dp no painel 180) mas o espaço do
  valor por dentro da caixa era `maxVw = boxW − 6 − 14 − 8` — a 180dp,
  boxW 37.3 → **maxVw 9.3dp: nem a reticência cabia** e o
  `textfit::ellipsize` devolvia STRING VAZIA (o contrato do ellipsize:
  «nem o "..." cabendo, devolve string vazia — não desenha»). O valor
  simplesmente não desenhava; a letra do eixo sobrava sozinha.
- **A cura (a regra do dono, por dentro da caixa)**: a fonte única
  `transformRowBudget` ganhou dois passos DEPOIS do orçamento D2:
  1. `transformValueSpace(b) < kTfValueMinDp (26dp)` → **a LETRA do eixo
     sai** (`axisLabels=false`) — o 1.º a ceder;
  2. ainda abaixo do piso → **o PADDING DA LINHA cede**
     (`rowPadDropped` — as margens 16→4dp de cada lado e a caixa
     re-computa no MESMO regime do reset que a tentativa escolheu).
  O draw consome `tb.axisLabels`/`tb.rowPadDropped` (a letra só desenha
  se o orçamento a manteve; o padding da linha é o DO orçamento) — o
  draw e o re-despacho do tap partilham a MESMA matemática.
- **Números resultantes**: 180dp (164 úteis) → caixa 40, letra sai,
  valor 28dp; 220dp (204) → caixa 53.3, letra FICA, valor 27.3dp; 260dp
  (244) → caixa 60 + chip 40, valor 34dp. Os valores da casa são `%.2g`
  (curtos: «-12», «45») — cabem INTEIROS nos três regimes (provado com a
  fonte real na R-035).
- **Pin provado**: R-035(10) `regress_hotfix_defeitos` (os 3 regimes +
  a invariante da linha dentro do útil + o valor não-vazio com a fonte
  real) + FASE 16.4 do device virtual (os rótulos não-vazios no registo
  a 180/220/260 @2.0).

## 2. D17 — A CÂMARA COMO OBJETO PEQUENO (causa ficheiro+função)

- **Sintoma do dono**: a câmara era um CONE GIGANTE com quadrados
  filled de 26dp; arrastar na cena agarra o cone.
- **Causa** (`ui/CamGizmo.cpp` da 0.7.7): o desenho era o wireframe
  COMPLETO (corpo+caixa+lente com traço dp(3..4)) + 5 handles 26dp
  (4 cantos + o centro do far) e o hit-test do handle tinha raio dp(30)
  com PRIORIDADE sobre o gizmo — qualquer toque perto do far era «do
  handle», e o corpo/lente hit-testavam (pickCameraTic por distToBox).
- **A cura (o contrato do dono, letra a letra)**:
  - **(a) GLIFO ~24dp constante em ecrã** na posição da câmara
    (`kGlyphDp=24`, o ícone `icons::Icon::Camera` da casa) — o MESMO
    padrão de tamanho constante dos gizmos;
  - **(b) FRUSTUM FINO (1-2px)** que reflete a câmara real (near/far/
    cone; o far VISUAL continua ao cap de ecrã da 0.7.10 — o anti-
    gigante; o far REAL vive no CameraComp/gameProj): **sem seleção =
    cinza mudo ~35% alfa** (o token text2 com `kMutedAlpha=0.35`),
    **com seleção = âmbar fino**;
  - **(c) HANDLES só com seleção, 12dp, SÓ nos 4 CANTOS** (editam o fov
    — o MESMO dragFov): o quadrado filled gigante morreu (o handle de
    canto é contorno âmbar + preenchimento a 25% — a linguagem D12) e o
    handle do CENTRO/far MORREU (o far edita-se no Inspector — o
    `dragFar` saiu do código);
  - **(d) o gizmo de mover ancora ao GLIFO** (a pos do Transform3D —
    onde o glifo desenha; nunca a um vértice do frustum);
  - **(e) HIT-TEST: gizmo de mover > handles de canto > frustum
    (intocável)** — no `feedGizmo` (platform/main.cpp) o `beginGrab`
    corre PRIMEIRO no press edge e o `pickHandle` DEPOIS (a ordem antiga
    era o contrário); o hit do handle caiu para dp(16);
  - **(f) as linhas do frustum NUNCA interceptam toque** — o
    `pickCameraTic` é agora POR GLIFO (raio `kGlyphHitDp`=18dp do olho
    projetado; o distToBox corpo/lente morreu).
- **Pin provado**: `cameratic_d17_objeto_pequeno_estados_e_medidas`
  (mudo 35% presente + zero âmbar sem seleção; âmbar + handle ≤12dp com
  seleção) + **FASE 16.1 do device virtual E2E**: o tap NO GLIFO
  seleciona; o tap NO CONE não seleciona; os handles ≤12dp no registo; o
  gizmo no glifo; e o **GESTO INJETADO** — um drag começado FORA dos
  handles MOVE a câmara e NÃO mexe o fov.

## 3. D14 — O MENU CONTIDO (causa ficheiro+função)

- **Sintoma do dono**: a primeira linha cortada no topo +
  «Documentação V. …» cortada pela tab bar.
- **Causas** (`ui/EditorUi.cpp`, `drawFileMenu` — DUAS):
  1. o **slot do scroll persistia entre aberturas** (e o reciclo do
     beginScroll podia devolver o offset de OUTRA região) — reabrir o
     menu nascia com o offset antigo: a 1.ª linha cortada no topo;
  2. o fundo reservava **28px crus** (`maxY = … − 28.0f`) — MENOS que a
     tab bar de 32dp: o sheet entrava DEBAIXO dela e o «Documentação
     V.ONI» (a última linha) ficava tapado/cortado.
- **A CAUSA-RAIZ ACRESCENTADA (o achado do P-04)**: as medidas do sheet
  eram **PX CRU** (`kSheetW 280`, `kRowH 48`, `kHdrH 28` sem `theme::dp`
  — a violação R-018/§2.9): no device @2.0 o menu desenhava a MEIA
  medida (140dp de largura, linhas 24dp) com o TEXTO a 2× — o texto
  sangrava as linhas e era metade do «menu cortado» que o dono viu. Os
  TRÊS sheets (ficheiro, hier, cenas) estão em dp real agora.
- **A cura**: (1) os ABRIDORES (top bar [Menu] e o ⋯ do viewport) repõem
  `scrollSetOffset(kMenuScrollId, 0)` no abrir — provado na REABERTURA
  com o slot sujo a 333 (FASE 16.2); (2) `maxH`/`maxY` reservam
  `theme::dp(safe::kBottomTabH)`; (3) o scroll próprio (contentH 856dp >
  o sheet) faz a última linha chegar INTEIRA — provado com o salto ao
  fundo («Documentação V.ONI» inteira dentro do sheet no registo).
- **Pin provado**: FASE 16.2 (offset 0 na 1.ª abertura E na reabertura;
  o fundo ≤ topo da tab bar; a última linha inteira; o toque fora fecha).

## 4. D15 — AS STRINGS DO MENU NO GATE (causa ficheiro+função)

- **Causa** (`ui/EditorUi.cpp`, a tabela `kMenu`): «Export OBJ» e
  «Export Downloads» eram EN no meio da tabela PT — o gate
  ui_vocab_check (da 0.9.6.18) não olhava à tabela do menu.
- **A cura**: «Exportar OBJ» + «Exportar Downloads»; o gate
  `scripts/ui_vocab_check.py` estende-se à tabela `kMenu`
  (`TECNICOS_MENU` é a allowlist explícita — **«Snapping» fica POR
  DECISÃO EXPLÍCITA** com a justificação: termo da ferramenta 3D sem
  tradução curta consensual; o par ligado/desligado é PT; o gate mantém-
  se honesto nos DOIS sentidos — o termo na allowlist sem estar no menu
  também é vermelho).

## 5. D16 — A CAPTURA (diagnóstico + cura)

**O veredito do diagnóstico (o dono: «se já corre, documenta e fecha
como verificado»)**: o pipeline JÁ CORRE — o FASE 15.6 do 0.9.6.18
prova o save→thumb.png 256×144 ≤60KB OFF-thread. MAS o card do gestor
continuava com iniciais por DUAS causas reais, curadas:

1. **O frame capturado tinha o MENU desenhado por cima**
   (`platform/main.cpp`, `captureThumbIfPending`): o arm corria NO MESMO
   frame do gesto (Guardar cena/Sair pelo menu) — o backbuffer desse
   frame tem o SCRIM + o SHEET (e no Sair, o BACKDROP OPACO dos modais)
   — o thumb.png saía com o MENU, não com a cena. **Cura**: o ponto
   único `armThumbCapture()` põe 1 frame de calmo E o
   `captureThumbIfPending` só corre num frame **SEM overlay modal**
   (`editor::anyOverlayOpen` — o guard re-arma); se o teardown chegar
   antes do frame limpo, o `thumbJobWait` loga a captura abandonada.
2. **A leitura Java corria UMA vez e nunca repetia**
   (`ProjectManagerActivity.java`, `requestThumb`): o engine escreve o
   thumb.png no thread de jobs DEPOIS do `finish()` — o primeiro
   `openInputStream` do gestor podia chegar ANTES da escrita terminar →
   card em iniciais ATÉ ao próximo onResume. **Cura**: a leitura REPETE
   (250/500/1000ms, por card, com log «lê outra vez em Nms») — o thumb
   que chega tarde é apanhado na mesma sessão.
3. **O log do dono existe**: `thumb: captura ok (256×144 PNG N B, Nms
   off-thread; orçamento 50ms)` / `thumb: captura falhou (<motivo>)` —
   nos 5 motivos (sem projeto, viewport degenerado, crop degenerado,
   encode, escrita) + o teardown.
- **Pin provado**: FASE 15.6 (o save→thumb.png) + FASE 16.5 (o frame do
  menu NÃO é capturado; a captura fica armada; corre no PRIMEIRO frame
  limpo).

## 6. D18 + D19 — AS INICIAIS E O TOGGLE (causa ficheiro+função)

- **D18** (`ProjectManagerActivity.java`, `initialsOf`): a regra antiga
  («1 palavra = as 2 primeiras letras») fazia projetoyygf/prooksnsn/
  projetor → **todas «PR»**. A cura é a regra do dono: 2+ palavras → as
  duas primeiras; 1 palavra → **primeira + ÚLTIMA letra**
  (projetoyygf→«PF», prooksnsn→«PN», projetor→«PR»); 1 letra → ela
  própria; vazio → «·». O matiz por hash mantém-se (projetoyygf e
  projetor partilham «PF» e distinguem-se PELO MATIZ — o pin «iniciais
  OU matiz distintos» fica satisfeito). O gate
  `scripts/projects_ui_check.py` caça o regresso das «2 primeiras».
- **D19** (`ui/EditorUi.cpp` + `ui/UiEditor.cpp`, `Kind::VisToggle`): a
  linha mostrava o par Eye/EyeOff + o rótulo mas NÃO O ESTADO. A cura é
  o **switch da casa** — `editor::drawVisSwitch` (a FONTE ÚNICA, os dois
  inspetores partilham): trilho-pílula 32×16dp + knob 12dp — ON: trilho
  âmbar com o knob à DIREITA (tinta accentInk); OFF: trilho border com o
  knob text2 à ESQUERDA. A posição do knob É o estado. O alvo de toque
  continua a ser a LINHA inteira.
- **Pins provados**: R-035(11) (o knob ON à direita, OFF à esquerda, no
  audit) + FASE 16.6 (os dois estados no device @2.0) + o gate
  projects_ui (D18).

## 7. RE-SIGN-OFF NO C33 (7 itens — PARA O DONO PREENCHER)

| # | Item do dono | Onde provar | ✓ |
|---|---|---|---|
| 1 | valores visíveis nos 3 campos (180/220/260dp) | R-035(10) + FASE 16.4 + PNG `docs/p05/hotfix19-device-camara-selecionada.png` (o Transform com os valores) | ☐ |
| 2 | câmara = glifo pequeno + frustum fino (mudo sem seleção, âmbar com seleção, handles pequenos) | FASE 16.1 + PNGs `hotfix19-device-camara-sem-selecao.png` / `-selecionada.png` (e o ANTES da mutação com os quadrados gigantes) | ☐ |
| 3 | mover a câmara por drag na cena funciona sem agarrar o cone | FASE 16.1 o GESTO INJETADO (o drag fora dos handles move a câmara; o tap no cone não seleciona) | ☐ |
| 4 | menu com primeira/última linha inteiras | FASE 16.2 + PNG `hotfix19-device-menu-aberto.png` (a 1.ª linha no topo; a última alcançável pelo scroll) | ☐ |
| 5 | «Exportar OBJ» | o registo da FASE 16.3 + o gate ui_vocab (a tabela caçada) | ☐ |
| 6 | card com captura real após save novo | FASE 15.6 (o thumb.png no projeto) + FASE 16.5 (o frame do menu nunca é capturado) + o retry Java (a leitura repete) | ☐ |
| 7 | toggle «visível» com estado | FASE 16.6 (o knob nos dois valores) + o PNG (a linha «visível» com o switch âmbar) | ☐ |

## 8. A TABELA DAS MUTAÇÕES (vermelho→verde, colada do log)

| Mutação | O regresso do defeito | O teste que fica VERMELHO | Prova |
|---|---|---|---|
| M-R1 | a letra nunca cede (o clamp corta o valor) | R-035(10) `regress_hotfix_defeitos` FALHOU | ✓ restaurado → OK |
| M-D17 | o handle de canto volta a 26dp (o quadrado gigante) | `cameratic_d17_objeto_pequeno_estados_e_medidas` FALHOU (rebuild limpo dos TUs com o header) | ✓ restaurado → OK |
| M-D17b | o handle do CENTRO/far volta ao pickHandle | `cameratic_handles_somente_cantos_fov` FALHOU | ✓ restaurado → OK |
| M-D14 | o reset do offset morre nos abridores | FASE 16.2 «a REABERTURA repõe o offset 0» FALHOU | ✓ restaurado → OK |
| M-D15 | «Export OBJ» volta à tabela | `ui_vocab_check` FALHOU («"Export » na tabela) | ✓ restaurado → OK |
| M-D16 | o guard do frame limpo morre | FASE 16.5 «o frame com overlay modal NÃO é capturado» FALHOU | ✓ restaurado → OK |
| M-D18 | as «2 primeiras letras» voltam | `projects_ui_check` FALHOU (a fórmula antiga no fonte) | ✓ restaurado → OK |
| M-D19 | o knob ignora o estado (sempre à esquerda) | R-035(11) `regress_hotfix_defeitos` FALHOU | ✓ restaurado → OK |

O log bruto das provas vive na sandbox do agente
(`/home/z/my-project/build-local/mutacoes.log` — fora do repo, o mesmo
padrão da prova ECJ do 0.9.6.18b).

## 9. OS PNGS P-05 (docs/p05/)

| PNG | O que prova |
|---|---|
| `hotfix19-device-camara-sem-selecao.png` | D17 sem seleção: frustum fino mudo, ZERO handles |
| `hotfix19-device-camara-selecionada.png` | D17 com seleção: âmbar, handles 12dp nos cantos, o gizmo no glifo, o switch «visível» ON |
| `hotfix19-device-camara-selecionada-ANTES-mutacao.png` | O ANTES (a mutação M-D17 com os 26dp): os quadrados filled gigantes que o dono vetou |
| `hotfix19-device-menu-aberto.png` | D14/D15: o menu contido (offset 0, acima da tab bar, «Exportar Downloads» PT) |

A honestidade do «antes»: os PNGs de ANTES só existem para a câmara (a
mutação M-D17 reproduz o estado antigo no MESMO harness — o padrão da
casa: a mutação É a reprodução do defeito). Para o menu, o «antes» é
provado POR CAUSA (as medidas px cru no código antigo: o sheet a 140dp
efetivos com o texto a 2×) — o harness antigo não exportava o menu; a
mutação M-D14 prova o pin do offset, não a imagem.

## 10. AS SUÍTES E OS GATES (o estado local deste commit)

- `test_core`: **0 falhas** (856 testes; R-035 estendida com (10) R1 e
  (11) D19; test_cameratic recalibrado ao D17 + o teste novo
  `cameratic_d17_objeto_pequeno_estados_e_medidas`).
- `c33_virtual`: **572 checks, 0 falhas** (a FASE 15 recalibrada ao
  `armThumbCapture` + a FASE 16 NOVA: 16.1 D17 E2E com gesto injetado ·
  16.2/16.3 D14/D15 · 16.4 R1 · 16.5 D16 · 16.6 D19 · 16.7 o validador
  inteiro 0/0).
- Gates: scope ✓ (12 ficheiros, todos dentro de ci/scope.txt) ·
  docs-lint 60 ✓ · theme ✓ · hierarchy ✓ (o contrato §2.16–§2.19) ·
  jni ✓ · link_parity 117 TUs ✓ · glyph ✓ · reload ✓ · projects_ui ✓ ·
  ui_vocab (com a tabela do menu) ✓ · identity (versionCode 51) ✓.

## 11. NÃO VERIFICADO (honesto)

1. **O re-sign-off do dono no C33** (os 7 itens de §7) — SÓ o device
   humano fecha.
2. **O gesto no device físico** (o drag da câmara com dedo real, multi-
   touch com orbit noutro dedo) — o harness prova com gestos injetados
   no frame() real; o multi-touch simultâneo não é reproduzido aqui.
3. **O timing real do retry das thumbs** (o thumb que chega entre os
   250/500/1000ms) — o caminho é provado por código + logs; o timing
   real depende do device (a escrita do worker demora <50ms; o gestor
   demora >1s a retomar é improvável mas possível).
4. **O card do gestor com a captura real** — a Activity Java não
   instancia na JVM do hospedeiro (a cultura desde a 0.9.0: o check é
   estrutural); a imagem final do card SÓ o device mostra.
5. **O Java SÓ compila no CI** — nenhum gate local o apanha (a lição
   0.9.6.18b); o commit só fica fechado com o job do APK verde.

## 12. BACKLOG (nada adiado neste hotfix)

Nada deste scope foi adiado. Os adiados COM NOME continuam os do F17
(fade de distância da grelha · dithering anti-banding) — intactos no
BACKLOG.md.
