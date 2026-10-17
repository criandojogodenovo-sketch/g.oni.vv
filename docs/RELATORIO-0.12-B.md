# RELATÓRIO 0.12-B — Arquitetura da UI (UI como TICs) · B-1

## 1. Objetivo e escopo

Transformar a UI criável de «um componente com elementos por valor»
(UiCanvas 0.7.0) numa ÁRVORE DE TICs à la Godot: um TIC raiz com o marcador
UIRoot e um TIC FILHO por elemento de UI, cada filho com o componente
Control (a base 2D partilhada — pos2D, tamanho, âncoras 9 cantos + esticar,
margens, pivô, rotação, visível, opacidade). A Hierarchy passa a mostrar a
árvore de UI como mostra qualquer árvore de TICs (nome, ícone por tipo,
seta, drag-reparent, rename, duplicate, delete — tudo o que a 0.7.0/0.11-H
já dão aos TICs). Cláusula anti-overengineering do prompt: «sem classes
novas para além do Control base» — Label/Button/Panel/Image/Menu/Card/
Article/VBox/HBox/Joystick NÃO são classes C++, são KINDS do Control (o
padrão UiElement::Kind da 0.7.0). O UiCanvas fica DEPRECATED: mantém-se por
compat (o leitor), os elementos novos nunca o usam. Pré-condição verificada:
a Campanha A está FECHADA (commit 3bdd9f9, vc 82, CI verde) — o prompt do
dono é o CONTINUAR.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: 3bdd9f9 (0.12-A: os bugs P0 — A-1..A-4). A suíte local
  no HEAD: test_core 1077 checks / 0 falhas, c33_virtual 1006/0 (os 2
  flakes conhecidos saltados via GONI_SKIP=r039,streaming_paralelo — o A/B
  da Y-1 provou que existem no HEAD sem mudanças; r039 exige ~4 GB de
  disco temporário e o hospedeiro desta sessão tem 9,9 GB partilhados).
- O modelo de UI vigente: UiCanvas componente por TIC com elementos por
  valor (UiElement), âncoras 3×3, containers por NOME (0.7.4), editor
  WYSIWYG por índice (st.selElement), joystick = TouchControls no TIC
  hospedeiro. Nenhuma representação da UI na Hierarchy além do TIC
  hospedeiro «UI».

## 3. Arquitetura escolhida

- **Control : Component** (components/Control.h, NOVO) — a base que todos
  os elementos partilham. O payload por tipo é O MESMO UiElement da 0.7.0
  (campo `elem` por valor — zero duplicação de estado; o draw/hit-test/
  layout existentes continuam a consumir o MESMO struct). Os campos da
  base: pos2D = elem.ox/oy, tamanho = elem.w/h, âncoras 9 = elem.anchorH×
  anchorV (o MESMO modelo 3×3), esticar = `stretch` bool, margens =
  marginL/T/R/B, pivô = pivotX/Y (0..1, default 0.5/0.5 — o pivô default
  reproduz a rotação «à volta do centro» da 0.8.6), rotação = elem.rot,
  visível = elem.visible, opacidade = `opacity` (0..1). O kind tem a FONTE
  no Control (Kind::Joystick = 9 não cabe no UiElement::Kind); nos kinds
  0..8 o elem.kind fica ESPELHADO para o draw; no Joystick o espelho fica
  Panel e o draw dele é o drawTouchControlsAt de sempre.
- **UIRoot : Component** (components/UIRoot.h, NOVO) — marcador vazio. A
  raiz é o primeiro TIC ativo com UIRoot; o editor cria/reusa o TIC "UI"
  (o nome de sempre da casa).
- **A vista espelho (ui/UiTreeView)** — o OLD código de draw/hit/layout
  (resolveCanvasLayout/drawCanvas/hitTestCanvas) continua a ser A FONTE:
  collectUiTree() materializa a árvore num UiCanvas transitório (elems por
  valor; joystick com rect DERIVADO do TouchControls; filho de container
  recebe o `parent` = o NOME do TIC container — o resolver da 0.7.4 dispõe
  como sempre). O EDITOR desenha/edita o espelho e SINCRONIZA de volta no
  fim do drawUiViewport (o rect derivado — joystick/esticar — nunca
  sincroniza; o parent editado segue para a árvore de TICs). O PLAY usa
  drawUiTree/hitTestUiTree sobre o MESMO espelho.
- **Criação**: uiAddTreeElement/uiAddTreeJoystick (ui/UiEditor.cpp) criam
  o TIC filho + Control (+ TouchControls no joystick — o runtime de input
  da física NUNCA muda). O dispatch do "+" no main foi religado para a
  árvore; o uiAddElement legado FICA INTACTO para a compat 0.7.x (testes
  em memória). Nomes únicos: variante CRIAÇÃO (base primeiro — "joystick",
  "joystick.001") NOVA (uniqueNewTicName), distinta da variante DUPLICAR
  do duplicateTic (que sempre sufixa).
- **Serialização**: o gerador de JSON de elemento foi EXTRAÍDO para
  appendUiElementJson (byte a byte o MESMO vocabulário do UiCanvas) e o
  leitor para fillUiElementJson — o Control partilha as DUAS fontes. O
  Control escreve kind + os campos do elem + os da base SÓ quando
  não-default (stretch/ml/mt/mr/mb/px/py/op). O UIRoot grava a presença.
- **Compat (a migração)**: migrateUiCanvasToTree() corre no FIM do
  loadText: cada TIC com UiCanvas não-vazio ganha UIRoot, cada elemento
  vira TIC filho (o elem por valor; o parent por NOME da 0.7.4 vira pai
  TIC e o elem.parent conserva o vocabulário), o canvas é REMOVIDO. A
  migração é DE MEMÓRIA — o .goni no disco não é reescrito (o próximo
  save grava a árvore). A animação de UI (0.8.0, UiPos/UiColor/UiAlpha)
  ganhou o fallback da árvore no AnimationPlayer::apply (alvo por NOME →
  TIC com Control) — sem ela o load convertia cenas e MATAVA as tracks.

## 4. Implementação (por ficheiro)

- components/Control.h (NOVO) — a base + controlKindName/FromName.
- components/UIRoot.h (NOVO) — o marcador.
- components/UiCanvas.h/.cpp — elementDefaults/elementDefaultText/reanchor
  ficam PÚBLICOS (a árvore reusa OS MESMOS defaults e A MESMA re-ancoragem
  — uma só fonte); setAnchor delega no reanchor.
- core/ComponentStore.h/.cpp — storages Control/UIRoot + accessors +
  especializações storageOf + registo "Control"(11)/"UIRoot"(12) no FIM
  (ids antigos intactos) + removeAll cobre os dois.
- core/SceneSerializer.cpp — appendUiElementJson/fillUiElementJson
  (extrações byte a byte); appendComponentJson(Control/UIRoot — com o null
  check OBRIGATÓRIO: o UIRoot sem ele gravava um marcador FANTASA em todos
  os TICs e matava a migração — apanhado no debug da sessão);
  fillControl; migrateUiCanvasToTree; dispatch + chamada no loadText.
- ui/UiRuntime.h/.cpp — controlRect (os 9 cantos delegam no elementRect;
  esticar = pai − margens); UiTreeView + collectUiTree (a cadeia de pais
  sobe SÓ por TICs com Control; o mapa slot→Tic* evita o handle com
  generation errada); drawUiTree/hitTestUiTree (o MESMO contrato do
  canvas); uiElementFromHit (o caminho ÚNICO do main para applyUiAction);
  findElementAnywhere procura também os TICs de UI por NOME (os alvos
  Show/Hide/Toggle na árvore).
- ui/UiEditor.h/.cpp — ensureUiRoot/uiAddTreeElement/uiAddTreeJoystick;
  drawUiViewport com MODO ÁRVORE (espelho + todos os joysticks desenhados +
  tap seleciona o TIC + sincronização no fim); drawUiInspector edita o
  elem AO VIVO quando o selecionado tem Control (âncoras pelo reanchor; o
  "colocar em" faz REPARENT de TIC; o "remover elemento" APAGA O TIC);
  commitTextInput renomeia TIC + elem.name e escreve texto/alvo/hex no
  Control; uniqueNewTicName.
- ui/EditorUi.cpp — hierIconFor: UIRoot → Monitor; Control → ícone por
  tipo (Label→Sort, Button→Cursor, Image→Monitor, Menu/Article→List,
  VBox/HBox→Grid, Joystick→Person, Panel/Card→Box — o conjunto existente,
  zero glifos novos).
- components/AnimationPlayer.cpp — os alvos Ui* resolvem o elemento na
  ÁRVORE por nome (o canvas legado fica como fallback).
- platform/main.cpp — o "+" em modo UI usa a árvore; feedCanvasPlay arma/
  aplica com hitTestUiTree/uiElementFromHit; drawCanvasPlay desenha a
  árvore; TODOS os TouchControls desenham no Play (a árvore pode ter N).
- tests/test_012b_uitree.cpp (NOVO) — os 9 pins. tests/CMakeLists.txt —
  o TU registado (o app CMake não muda: NENHUM .cpp novo em app/ — só
  headers novos, a receita dos DOIS CMakeLists não se aplica).

## 5. Os pins (9/9 verdes — test_012b_uitree.cpp)

1. uitree_control_base_campos — os campos da base (pos2D/tamanho/âncoras/
   stretch/margens/pivô/opacidade), controlRect nos 9 cantos == elementRect
   e o esticar = pai − margens; o vocabulário dos kinds.
2. uitree_uirrot_tem_filhos — a raiz + 41 filhos (o cap 32 é só da VISTA
   do editor) + round-trip dos 41 (o «sem limite» é do FORMATO).
3. uitree_ui_add_joystick_cria_tic — o "+Joystick" cria TIC FILHO com
   Control(Kind::Joystick) + TouchControls, selecionado em edição.
4. uitree_ui_add_2_joysticks — DOIS TICs distintos ("joystick"/"joystick.
   001"), os dois filhos da raiz, instâncias TouchControls independentes.
5. uitree_hierarquia_arvore — ícones por tipo (Monitor/Person/Cursor/Sort)
   + o draw: a SETA (chevron) na raiz com filhos e AUSENTE sem eles.
6. uitree_hierarquia_rename — commitTextInput renomeia o TIC e o elem.name
   segue; round-trip transporta.
7. uitree_hierarquia_delete — destroyWithChildren apaga a sub-árvore (o
   vbox com o neto), os irmãos ficam, o round-trip não tem fantasmas.
8. uitree_serializa_arvore — a árvore completa (6 kinds + filho de
   container): ação/target/param, stretch/margens/pivô/opacidade/rot,
   TouchControls por valor, spacing/pad, invisível; o 2.º dump é estável.
9. uitree_compat_uicanvas — o .goni antigo abre como UIRoot + filhos (a
   estrutura do parent por NOME vira pai TIC), o canvas sai, o save da
   cena convertida grava a árvore e reabre igual.

## 6. As mutações (vermelho → reposto → verde)

- **M-B1a** (o UIRoot não cria filhos): uiAddTreeJoystick passa a devolver
  sem criar o TIC → uitree_ui_add_joystick_cria_tic E
  uitree_ui_add_2_joysticks VERMELHOS:
  ```
  uitree_ui_add_joystick_cria_tic ... FALHOU (uiAddTreeJoystick(s, st); elems.size() == 1)
  uitree_ui_add_2_joysticks       ... FALHOU (uiAddTreeJoystick(s, st) ×2)
  ```
  Reposta → 0 falhas (os 9 pins verdes).
- **M-B1b** (a serialização só guarda o pai): o appendComponentJson(Control)
  sai do dump → uitree_serializa_arvore VERMELHO (5 EXPECTs — o
  stretch/ml/op/kind joystick desaparecem do texto e e2.size()==7 morre):
  ```
  uitree_serializa_arvore ... FALHOU (text.find("\"stretch\":true"), "ml":8,
  "op":0.5, "kind":"joystick"; e2.size() == 7) — 5 teste(s) com falha no
  filtro uitree_ (os round-trips dos irmãos também dependem do Control)
  ```
  Reposta → 0 falhas.
- **M-B1c** (a compat quebrada): o migrateUiCanvasToTree(scene) sai do
  loadText → uitree_compat_uicanvas VERMELHO:
  ```
  uitree_compat_uicanvas ... FALHOU (UIRoot != nullptr; UiCanvas == nullptr;
  elems.size() == 2) — 3 teste(s) com falha no filtro
  ```
  Reposta → 0 falhas.

## 7. Sentinela R-050 (docs/REGRESSOES.md)

A serialização da UI é estrutural → R-050 («a UI é uma ÁRVORE de TICs: o
.goni grava UIRoot+Control e o load converte o UiCanvas») com a mutação
M-B1b como prova de vigília, colada no REGRESSOES.md.

## 8. Recalibrações legítimas (com nota no sítio)

O CONTRATO DE ABERTURA mudou por ordem do prompt («.goni antigo com
UiCanvas abre como UIRoot + filhos automáticos») — os pins que aferravam o
load antigo foram recalibrados para a árvore, com a nota 0.12-B no sítio:
- test_components.cpp — registry count 11→13 (+Control/UIRoot; ids 11/12).
- test_scenes.cpp (cenas_acoes…) — o round-trip do act/param aferra o FILHO
  (helper local treeElemsOf).
- test_uicanvas.cpp (serializacao_roundtrip) — os 2 elementos voltam nos
  filhos, mesma ordem/dados.
- test_joystick.cpp (compostos_menu_card_article…) — o round-trip dos
  compostos aferra os filhos.
- test_uilayout.cpp (serializacao_roundtrip_containers) — o botão é NETO da
  raiz (filho do TIC do container) e o elem.parent conserva o vocabulário.
- test_wiring084.cpp (save_load_com_cena_grande) — a contagem cresce com a
  árvore (+50 filhos; os 50 Controls contados na raiz).
- test_wiring086.cpp (rot_fontscale_style) — fscale/tstyle/rot voltam no
  elem do filho.
- test_wiring089.cpp (load_cura_selfparent) — a cura corre ANTES da
  conversão; o filho Control nasce sem parent; o erro legível mantém-se.
- test_wiring084.cpp (play_pos_restaura) — INTACTO (o PlaySnapshot clona a
  cena em memória — sem load, sem conversão).

## 9. Achados fora da lista (cláusula calma — NÃO fixado)

- O chevron fantasma do pin 5 revelou que a janela de deteção [14,42]
  apanhava a borda do ÍCONE DE TIPO (x≥40) — ajuste do PIN, não do código.
- O drawUiInspector em modo árvore mostra as rows de posição para filhos de
  container (o plano lê e.parent, que na árvore vive no TIC) — os sliders
  editam ox/oy que o container ignora. SEM sintoma de corrupção; o
  redesign do Inspector é EXATAMENTE a sub-fase B-2 (o plano novo lê a
  estrutura do TIC).
- O UiElement.rot roda à volta do pivô APENAS quando o pivô é o centro
  (o ScopedQuadXform usa o centro do rect) — o pivô não-centro é DADO
  serializado; a aplicação no draw entra na B-2 (documentado em §11).

## 10. O bug apanhado a meio (o UIRoot fantasma)

A 1.ª versão de appendComponentJson(const UIRoot*) não verificava o
ponteiro null — TODOS os TICs gravavam «{"type":"UIRoot"}» no .goni, o que
(1) violava o formato limpo de defaults e (2) MATAVA a migração de compat
(o hospedeiro já «tinha» UIRoot ao carregar, o converso saltava-o, o pin
compat_uicanvas e o round-trip dos irmãos crashavam com deref de null nos
testes antigos). Apanhado pelo crash do test_core (sinal 11 no
uicanvas_serializacao_roundtrip), isolado com um repro de 30 linhas e
curado com o null check + nota no sítio. O padão da casa («o gerador é
simétrico ao leitor») volta a valer.

## 11. Limites declarados (a B-2/B-3 tratas)

- O pivô/opacity são DADOS + serialização (pins 1/8); a APLICAÇÃO no draw
  (rotação pelo pivô não-centro, o alfa multiplicado) entra na B-2 com o
  Inspector que os expõe.
- O cap de 32 elementos na VISTA do editor é o de sempre (0.7.4) — o dado
  e o formato não têm limite (pin 2).
- Reordenar IRMÃOS por drag (o C33 diz «arrastar para reordenar») — o que
  existe é o drag-REPARENT da 0.11-H (mover para dentro de outro TIC ou
  para a raiz). A reordenação pura de irmãos exigiria ordem de filhos
  persistida (o slot é a ordem de criação) — FORA do escopo da B-1 (a
  cláusula calma manda); o C33 do dono executa com o reparent existente e
  a nota fica aqui para o dono decidir se quer a fase extra.
- Os overlays do editor (teclado/seletores) continuam a tapar o viewport —
  intocados (o contrato 0.7.5).

## 12. Pin C33 (dono) — o protocolo do device

1. Abrir/criar projeto → modo UI → «+» → Joystick ×2, Button ×3, Label
   («texto») ×2 → a Hierarchy mostra o TIC "UI" (ícone monitor) com SETA
   e os 7 TICs filhos (2 joysticks com ícone de pessoa, 3 botões com
   cursor, 2 textos com linhas).
2. Renomear um (⋮ → Renomear), apagar outro (⋮ → Remover → confirmação),
   arrastar um filho para outro (reparent) — a árvore reflete os três.
3. Guardar (≡ → Save), matar a app, reabrir — os 7 TICs (ou 6, se apagou
   um) voltam com os nomes/estrutura; PNG da HIERARQUIA com a árvore
   aberta. (O PNG é do dono — o caminho do C33 virtual fica coberto pelos
   pins 5/7 no replay da suíte.)

## 13. Fecho da sub-fase

test_core 0 falhas (1077+9 pins novos; os 2 flakes conhecidos saltados) ·
c33_virtual 1006/0 · versionCode 82 → 83 (o §14 do RELATORIO-0.9.6.md
declara o par novo para o gate R-015). STATE.md e CODEMAP.md viajam NESTE
commit (a regra §1.2). PARAR — a B-2 só arranca por ordem do fluxo da
campanha (o prompt da B manda «vc → próximo»).

---

# B-2 — O INSPECTOR DE UI (2D + âncoras)

## 14. Objetivo e o que mudou

O selecionado da árvore (um TIC com Control) tem agora um Inspector PRÓPRIO
com a estrutura pedida pelo prompt: as **3 secções colapsáveis**
(Transform / Aparência / Comportamento — o cabeçalho leva o chevron da casa
e o tap alterna; o colapso vive no EditorState::inspUiCollapsed, o bit i =
a secção i fechada), os **campos da base** que a B-1 introduziu (margens
L/T/R/B, pivô X/Y, rotação, o toggle ESTICAR — a 10.ª âncora, cuja SAÍDA
conserva a posição pelo rect atual com o MESMO padrão do uiDetachElement
da 0.7.4 — e a opacidade), os **campos específicos por tipo** (Label/
Button: texto, tamanho da letra, estilo, fundo A, hex, tex; **Joystick:
raio 40..150 px, ZONA MORTA, sensibilidade, cor** — os campos do joystick
escrevem o TouchControls do MESMO TIC: a fonte de input da física nunca
muda) e os **botões de reset por campo** — o quadrado 30px com o ícone
Reset à direita das linhas resetáveis; o valor do slider desloca-se para
a esquerda e o quadrado fica INTEIRO dentro da linha e do painel (o
«(não cortados)» do prompt). A **ZONA MORTA** é um campo NOVO do
TouchControls (`dead`, a fração do raio em que o eixo lê 0; o default 0
reproduz o comportamento de sempre byte a byte; o .goni grava `dead` só
quando != 0 — a política de compat da casa).

O plano é a FONTE ÚNICA (uiInspectorPlanControl — o MESMO contrato de y
cumulativo dos planos do editor) partilhada com os pins; o caminho legado
(canvas em memória dos testes 0.7.x) continua pelo plano antigo byte a
byte. O uiJoystickPlan/drawJoystickInspector do 0.7.x SAÍRAM do código
(mortos pelo «sem remover joystick») e o test_joystick foi recalibrado
para o plano de CONTROLO.

## 15. Os pins (5/5 verdes — test_012b_inspui.cpp)

1. **insp_control_2d** — o plano mostra X/Y/W/H para um Control e os
   sliders ESCREVEM o Control ao vivo (o drag do X a 90 % da faixa põe
   ox > 1000; o W a 50 % põe w > 1000) — o MESMO contrato de interação do
   Inspector 0.7.x.
2. **insp_ancoras** — as rows AnchorH/AnchorV existem no plano e os 3×3
   taps passam pelos 9 CANTOS; em cada um, controlRect == elementRect (a
   MESMA matemática — zero divergência).
3. **insp_sem_remover** — com TODAS as linhas do plano do joystick
   tocadas: NENHUM caminho tira SÓ o componente TouchControls (TIC vivo ⇔
   componente vivo); a linha Remove existe e leva o TIC INTEIRO (o padrão
   da árvore B-1).
4. **insp_campos_por_tipo** — os planos do Label e do Joystick diferem
   campo a campo (Label: texto/letra/estilo/fundo A/âncoras; Joystick:
   raio/zona morta/sens/opacidade; os 3 cabeçalhos de secção nos DOIS).
5. **insp_reset_visivel** — ≥10 linhas com reset; o quadrado é inteiro
   dentro da linha (h ≥ 30px) e do painel (x fixo); o reset da linha X
   repõe o valor (o drag a 90 % → o reset → 0).

## 16. As mutações (vermelho → reposto → verde)

- **M-B2a** (as âncoras fora do plano — o push de AnchorH/AnchorV
  removido): insp_ancoras VERMELHO:
  ```
  insp_ancoras ... FALHOU (anchors == 2u)
  ```
  Reposto → 0 falhas.
- **M-B2b** (o tipo não muda os campos — o plano força joystick=false):
  insp_campos_por_tipo VERMELHO (5 EXPECTs — o joystick perde
  dead/radius/sens e ganha âncoras/tamanho de elemento):
  ```
  insp_campos_por_tipo ... FALHOU (has(jp, jn, Dead); has(Radius);
  has(Sens); !has(AnchorH); !has(SizeW))
  ```
  Reposto → 0 falhas.

## 17. O achado do drawUiInspector (a skip-list comia o reset)

A 1.ª versão despachava o tap numa lista única: os SLIDERS eram saltados
(`continue`) ANTES do check da zona de reset — o quadrado de reset das
linhas de slider era INALCANÇÁVEL (o pin insp_reset_visivel apanhou: o tap
no quadrado não repunha o valor). O fix estrutural: a zona de reset tem um
LOOP PRÓPRIO com prioridade (só linhas reset; a zona [w−kPad−30, w−kPad]);
o corpo das linhas despacha DEPOIS com o skip de sempre. A lição fica no
relatório: «a zona nova de um widget de linha tem de entrar ANTES do skip
do gesto do corpo».

## 18. Recalibrações e limites

- test_joystick (joystick_proxy_editavel_no_viewport_2d) — RECALIBRADO
  com nota: a 2.ª metade usa o plano de CONTROLO (o de 0.7.x morreu com o
  botão); o apagar verifica o TIC INTEIRO (nunca só o componente); o
  colapso das 2 primeiras secções é o gesto honesto do dono (o plano e o
  frame partilham o MESMO colapso).
- O joystick não mostra W/H/âncoras/margens/pivô/rotação (o rect dele é
  DERIVADO do TouchControls — B-1) nem tex/texto; mostra raio/zona
  morta/sens/cor/opacidade — o plano é a fonte da diferença.
- O pivô não-centro e a opacidade dos ELEMENTOS continuam sem aplicação no
  draw (o B-1 §11 mantém-se; os campos são vivos e serializados — o draw
  aplica no B-3 quando o canvas muda).
- O RESET do raio (joystick) põe size=1 (raio 75px — a base do layout
  0.7.3); o reset da cor do joystick repõe o LINE mono (0.1804 — o default
  do TouchControls), o dos elementos repõe a cor POR TIPO (elementDefaults
  — o Label volta transparente).

## 19. Pin C33 (dono) — o protocolo do device

1. Modo UI → criar um joystick (+ → Joystick) → selecioná-lo na
   HIERARQUIA (o ícone de pessoa) → o painel direito mostra o INSPECTOR DE
   CONTROLO (não o de 3D): as 3 secções com chevrons + x/y/raio.
2. Mover o joystick (x/y ou drag no viewport), mudar o raio, tocar um
   reset (o valor volta), colapsar/abrir as secções.
3. PNG do Inspector com o joystick selecionado.


---

# B-3 — O CANVAS com a moldura do device

## 20. Objetivo e o que mudou

O modo UI passa a mostrar O QUE O DEVICE VÊ: as **3 chips** no topo do
viewport (a FONTE ÚNICA da geometria é uiCanvasChipsRect — o draw e os
pins partilham os rects; os ids 8060..8062) — o seletor de proporção
ANDROID (0=livre → 16:9 → 18:9 → 20:9, o tap cicla), o toggle das guias e
o toggle da grelha. A **MOLDURA** (uiDeviceFrameRect — puro) é o rect da
proporção escolhida CENTRADO no espaço de design, com o lado MAIOR a
casar (o letterbox do device preview); a **ÁREA FORA escurece** (os 4
bandos pretos α0.55) — o que o device não vê. O layout dos ELEMENTOS não
muda (o elementRect continua a ser a fonte — a moldura é a pré-visualização
honesta do corte, não uma re-ancoragem). As **GUIAS de margem segura**
(ativáveis) são as 4 hairlines accent ao INSET da moldura pela safe-area
do design; a **GRELHA de alinhamento** (ativável) são as hairlines a cada
40px de design DENTRO da moldura. Nada disto desenha em Play.

O **TOAST** é OPACO: a fonte única vpchrome::toastColors/toastBgAlpha/
toastContrastRatio (o padrão sceneRowIcon — o draw do main consome as
funções, os pins aferem-nas). O vidro surface2 α0.86 com o fade misturava
a cena e o «joystick adicionado» ficava ilegível sobre fundo claro; o
alfa 1 garante o contraste WCAG (L1+0.05)/(L2+0.05) ≥ 4.5 INDEPENDENTE do
que está por baixo. O fade-out de alfa saiu (o toast vive os 1.8 s e
desaparece — o corte seco é o preço honesto do contraste constante).

## 21. Os pins (3/3 verdes — test_012b_canvas.cpp)

1. **canvas_moldura** — a matemática pura dos 4 presets (os valores e os
   nomes); o rect 16:9 no design 1600×720 = {160,0,1280,720} (w/h =
   16/9), o 18:9 = {80,0,1440,720}, o 20:9 = o design inteiro; o DRAW: o
   bando esquerdo do véu [t.ox, frameX0) com alfa 0.55 existe no batch e
   DESAPARECE em «livre».
2. **canvas_guias** — o tap nas CHIPS alterna (o estado e o draw); com a
   safe-area NÃO-nula {10,12,14,16} a guia esquerda vive na coluna de
   design fr.x+10 (DISTINTA da borda da moldura em fr.x) e morre no
   segundo tap; a grelha tem as suas linhas a cada 40px dentro da moldura.
3. **canvas_toast_contraste** — toastBgAlpha()==1.0; o bg é o TOKEN
   surface2 com alfa 1 (nunca o vidro); o texto é o text1 opaco; o rácio
   WCAG do par ≥ 4.5 (e ≤ 21 — o teto da escala).

## 22. As mutações (vermelho → reposto → verde)

- **M-B3a** (a moldura removida — o frameOn forçado a false):
  canvas_moldura VERMELHO:
  ```
  canvas_moldura ... FALHOU (veilEsq)
  ```
  Reposto → 0 falhas.
- **M-B3b** (o toast volta ao cinza-vidro — o alfa 0.86):
  canvas_toast_contraste VERMELHO:
  ```
  canvas_toast_contraste ... FALHOU (toastBgAlpha()==1.0f; bg[3]==1.0f)
  ```
  Reposto → 0 falhas.

## 23. Recalibrações e limites

- test_uilayout (uilayout_paridade_editor_play_por_tipo) — RECALIBRADO com
  nota: as chips são CHROME DE TEXTO no topo do viewport que o Play não
  desenha — os glifos da banda delas saem da comparação de paridade (um
  loop manual explícito; a geometria dos ELEMENTOS continua par a par).
- As chips cobrem o canto superior-esquerdo do design (o chrome do editor
  por cima do conteúdo — o padrão dos gizmos); o dono que precisar da
  área livre desliga a moldura (livre) — as chips continuam (o seletor é
  sempre visível).
- O corte seco do toast (sem fade) é a decisão documentada: o contraste
  constante vale mais que o fade decorativo (o M-B3b vigia).
- A guia usa o INSET DA MOLURA pela safe-area do DESIGN (a margem das
  âncoras) — a margem de CADA device real é o passo seguinte se o dono
  quiser (o campo ficaria no seletor).

## 24. Pin C33 (dono) — o protocolo do device

1. Modo UI com um joystick → tocar a chip de proporção até «16:9» → a
   moldura desenha com os lados escuros.
2. Tocar «guias» → as 4 linhas âmbar dentro da moldura; tocar «grelha» →
   as linhas de alinhamento.
3. + → Joystick → o toast «joystick adicionado» LEGÍVEL (opaco) por cima
   de qualquer fundo.
4. PNG do viewport com a moldura + guias.
