# G.One VV 0.7.7 — TIC de câmara: frustum wireframe no editor + gizmos + handles + câmara de jogo em Play

<!-- (0.7.6 abaixo — histórico) -->

# G.One VV 0.7.6 — reestruturação UI/UX do editor: toolbar final de 5 grupos + ícones vetoriais + tela de projetos + Theme central

Engine com editor, projeto `.goni` e maturação de assets (compressão ETC2/ASTC
com cache, extração de texturas glTF/GLB, import OBJ/glTF/GLB/PNG, export
OBJ). Mobile-first: arm64-v8a, minSdk 24, landscape travado
(`sensorLandscape`). Devices de teste: Realme C33 (720x1600) e Realme
RMX3624 (Android 13).

Relatórios 1-16 das sub-fases: `docs/RELATORIO-0.7.{4,5,6,7}.md` (com os
sha256 dos APKs assinados).

## Escopo 0.7.7 (implementado — TIC de câmara + frustum + gizmos)

**COMPONENTE `Camera`** (`components/CameraComp.h`, registado no fim do
ComponentStore — id 6; serializado no .goni como "Camera"): fovY (graus),
near, far, projeção (perspetiva|ortográfica), orthoSize (meia-altura) e
`active` — **UMA câmara ativa por cena** (`core/CameraUtil` assegura o
invariante no load e nos toggles do Inspector). A POSE vem do Transform3D
do mesmo TIC (−Z local = direção de visão, +Y = up).

**FRUSTUM WIREFRAME no editor** (`ui/CamGizmo`): corpo (caixa + lente),
cone de 4 arestas, retângulo do plano far, linha de visão central e
handles nos 4 cantos + centro do far — na COR DE MARCA #8AB4F8, desenhado
pelo line batch dos gizmos. **Só no editor, nunca em Play** (como os
gizmos); a câmara selecionada ganha os handles.

**SELEÇÃO POR TOQUE**: tocar no corpo/frustum de qualquer câmara no
viewport seleciona o TIC dela (hit-test 3D por projeção — a mesma técnica
dos gizmos), além da Hierarchy.

**GIZMOS NA CÂMARA**: mover/rodar atuam no Transform3D (o caminho de
sempre); **escalar ajusta fovY/orthoSize** (o frustum escala — a escala
do transform fica intacta: não tem significado numa câmara). Snap ativo:
o fator salta nos passos dos gizmos; fov arredonda a 5° nos handles.

**HANDLES DO FAR**: arrastar o CENTRO muda `far` (delta projetado no eixo
de visão; snap 1 u); arrastar um CANTO muda `fovY` (fator radial). Com a
câmara selecionada, o hit-test dos handles tem PRIORIDADE sobre os eixos
do gizmo — sem conflitos de drag.

**CÂMARA DE JOGO**: em Play a cena renderiza pela câmara ATIVA (view da
pose + proj dos parâmetros, persp ou orto); sem câmara ativa o fallback é
a orbit de edição. O editor mantém a orbit SEMPRE. A base de movimento
dos controlos segue a câmara de jogo em Play.

**ALINHAR À VISTA**: item novo no menu contextual da câmara (⋮) copia a
pose da orbit de edição para o transform dela.

**INSPECTOR da câmara**: secção Camera com fov/near/far (sliders),
projeção (cicla persp/orto), orthoSize e ativa (uma ativa por cena).
Criação: o "+" do 3D ganha "Camera" (nasce A ativa).

Suíte 407→418 (+11: geometria do frustum [far/near/orto/rodada +
planeHalfExtents], seleção por toque no frustum [centro/cone/vazio/
invisível], gizmo mover/rodar no transform, escalar=fov/ortho com clamps
e snap, handles far/fov [ids, raio prioritário, âncoras, snap, clamps],
serialização round-trip + uma ativa, CameraUtil, play usa a ativa
[gameView/gameProj exatos] + frustum nunca em play + batch, alinhar-à-
vista [pose idempotente], plano do Inspector, criação/menu contextual).

## Escopo 0.7.6 (implementado — reestruturação UI/UX definitiva)

**BARRA SUPERIOR FINAL DE 5 GRUPOS** (a toolbar cresceu orgânica — tudo
texto, tudo ao mesmo nível — e já não escalava). Princípios: texto só
para identidade/ações raras; uso frequente = ÍCONES; grupos com
separador visual; estados exclusivos = segmented control; esconder o que
não se aplica:
- **G1 sistema** `[Menu ▾][Cena ▾]` — Menu abre o dropdown (Settings,
  Guardar, Carregar, Export OBJ, Importar…, Export Downloads, Sair — o
  Settings deixou de ser botão próprio); Cena abre a lista de cenas do
  projeto (o item "Cenas…" saiu do menu de ficheiros);
- **G2 playback** `[pause][play]` — ícones;
- **G3 modo** `[3D][UI]` — segmented, ativo com fundo de marca;
- **G4 transformação** `[mover][rodar][escalar][snap]` — segmented de
  ícones SÓ com seleção ativa em 3D (some da barra, não fica cinzento);
  mover/rodar/escalar exclusivos, snap é toggle;
- **G5 painéis** `[inspector]` — mostra/esconde o painel direito (a área
  junta-se ao viewport central).

**8 ÍCONES VETORIAIS PRÓPRIOS** (Mover/Rodar/Escalar/Snap/Inspector/
Cena/Play/Pause) como POLILINHAS (viewBox 0..24, stroke uniforme)
desenhadas pelo line batch dos gizmos — sem parser de SVG, sem raster.
Cor de marca `#8AB4F8` sobre o fundo da barra; ativo inverte (fundo de
marca + ícone escuro). Nenhum emoji.

**THEME CENTRAL** (`ui/Theme.h`): struct Theme lido pela toolbar/ícones —
cor de marca `#8AB4F8` (exceção DOCUMENTADA ao tema mono dos painéis,
que fica intacto), fundo da barra `#0B0E13`, stroke.

**TELA DE PROJETOS** (lado Java): UMA entrada de criação ([Novo projeto]
no topo — o botão de fundo morreu), + [Importar projeto] (pasta que já é
um projeto .goni) e o cabeçalho "Meus projetos"; a lista mostra SÓ nome
+ data da última edição (o URI — com o prefixo interno "primary:" —
nunca mais aparece; `ProjectsFormat.folderLabel` limpa o prefixo e é
testado na JVM do CI); botões de CONTORNO `#8AB4F8` sobre fundo escuro
(sem o gradiente cinza do tema do sistema).

Layout dinâmico (larguras proporcionais quando não cabe — nada
sobreposto em nenhuma largura). Suíte 401→407 (+7: 5 grupos sem
sobreposição em 2 larguras ±G4, transformação condicional, segmented
exclusivos + snap toggle, ícones dentro do rect/24/32px + espessura
uniforme, Theme central com cores exatas, ações G1/G2/G5, dropdown do
Menu 0.7.6) + testes Java host + check estrutural no CI.

## Escopo 0.7.5 (histórico — fix das falhas de UX do C33 0.7.4)

**Z-ORDER DOS OVERLAYS MODAIS** (o fix do "texto do canvas ATRAVÉS do
MENU"): com um modal aberto (+, MENU, Settings, seletores, diálogo de
armazenamento, import, logs, menu contextual, remoção, teclado, CENAS,
navegador, aplicar) o CHROME DO EDITOR NÃO SE DESENHA — no lugar, um
BACKDROP OPACO tapa o ecrã todo (nada do canvas UI/painéis/toolbar à
mista com o overlay). Os widgets são immediate-mode: não desenhados =
não interativos (os toques só pertencem ao modal, que fecha com toque
fora como sempre). `anyOverlayOpen` ficou público e ganhou os 3 que
faltavam (CENAS/navegador/aplicar não bloqueavam o gesto WYSIWYG).

**TIC DE UI PRÓPRIO + criação direta** (o fix do "criar UI obriga a um
TIC 3D"): no modo UI, criar um elemento SEM TIC selecionado assegura/cria
o TIC **"UI"** (só com UiCanvas — sem mesh/body) que hospeda o canvas e
aparece na Hierarchy como qualquer outro (renomeável/duplicável/
removível). Segundo elemento reutiliza o mesmo TIC. O toast diz
"TIC 'UI' criado + elemento".

**TECLADO COM MINÚSCULAS**: toggle **abc/ABC** visível na linha de baixo
(o espaço encolhe 3u→2u; o rótulo mostra o estado SEGUINTE), minúsculas
para renomear/texto/alvo/nome de cena; dígitos/'_' sem caso; `_`, `-`,
espaço, APAGA, OK, X mantêm-se. **Fix de um bug latente da 0.7.0 apanhado
pelo teste novo**: a linha S..Z tinha o **'Z' em falta** (a 9ª tecla era
um ponteiro NULL — tecla fantasma que crashava ao tocar).

Suíte: 399→401 testes (+ backdrop modal tapa o canvas [com MENU e menu
contextal; reabre o chrome ao fechar], TIC de UI sem seleção [só canvas,
sem mesh/body, reutiliza], teclado minúsculas após toggle [geometria da
linha de baixo com 6 teclas sem sobreposição]). Gates verdes.

## Escopo 0.7.4 (histórico — fix dos gaps de qualidade do C33 0.7.3)

**PARIDADE EDITOR↔PLAY (princípio transversal)** — o que o dono viu no C33
("o Menu mostra caixas no editor e texto solto no Play") tinha causa raiz
TRIPLA no viewport 2D: o texto era desenhado a 28 px CRUS sobre rects
escalados a ~0.4x, os insets/molduras constantes não escalavam e o layout
usava insets ZERO (o Play usa a safe-area real). Tudo corrigido:

- **`textScale` no UiContext**: métricas + EMISSÃO de glifos escalam com o
  viewport — o mini-canvas é o Play REDUZIDO ao pixel (texto, insets,
  molduras, joias de desenho — tudo × escala; no Play k=1);
- **Resolver de layout único** (`resolveCanvasLayout`): âncoras + safe-area
  REAL + containers — o MESMO código alimenta drawCanvas (Play),
  drawUiViewport (editor: draw/hit-test/drag) e hitTestCanvas. Paridade
  ESTRUTURAL, afervida por tipo de elemento no CI (rect-a-rect, editor =
  Play × escala + offset);
- **Joystick com a aparência do Play** no viewport 2D: o núcleo
  `drawTouchControlsAt` (parametrizável por origem/escala) desenha base +
  knob + botão JUMP nos dois modos (o proxy 0.7.3 era outra coisa);
- **Clip ao mini-ecrã**: nada do canvas sangra para os painéis ao lado (o
  Play recorta na borda física do ecrã; o editor recorta igual).

**LABEL = SÓ TEXTO por default** (o fix do "fundo branco fixo"): alpha 0;
fundo OPCIONAL configurável no Inspector (cor com alpha — slider "fundo A"
em todos os elementos). Button mantém o fundo escuro.

**TEXTURAS DE FUNDO em Button/Panel + Image escolhe a imagem**:

- Linha **`tex:`** no Inspector de UI (Panel/Button/Image) → seletor de
  `textures/` + **"importar…"** (abre o NAVEGADOR 0.7.2 — galeria
  incluída; o ficheiro cai em `textures/` e volta ao seletor);
- A ref vive no elemento (`image`); a renderização resolve POR FRAME pelo
  resolver do UiContext (o caminho do Image 0.7.0 — agora partilhado);
  **tint branco×alpha** (antes o Image tingia a textura com o LINE escuro
  e a imagem ficava quase preta);
- Sem imagem: **placeholder claro "(sem imagem)"** centrado (Image sem
  ref) ou com o NOME da ref quando a carga falha (honesto);
- **z-order sólidos↔texturas**: submissão em RUNS na ordem real de
  emissão (antes: grupos fixos — um botão sólido desenhado depois de um
  painel texturizado ficava POR BAIXO dele na tela).

**MENU configurável**: espaçamento entre itens (slider "espaco"), fundo
das caixas ON/OFF (alpha 0 = só texto) e alinhamento do texto
(start/center/end — default start = 0.7.3 exato).

**CONTAINERS VBox/HBox** (o fix do "organizar é posicionar tudo à mão"):

- Filhos (campo `parent` por nome) dispostos automaticamente em
  coluna/linha com **espaçamento + padding + alinhamento transversal**
  (start/center/end) — o filho não precisa de posição; aninháveis;
- **Auto-fit no eixo do conteúdo** (VBox: h; HBox: w) — os filhos nunca
  transbordam; filhos invisíveis COLAPSAM; container invisível esconde os
  descendentes; guard de ciclos (órfãos de topo, sem crash);
- Criação: "+" ganha **10 itens** (+ VBox/HBox); com um container
  selecionado, o elemento novo nasce FILHO; Inspector: "colocar em:"
  cicla os containers e ao SAIR o elemento fica NO SÍTIO onde estava
  (detach conserva a posição); arrastar um filho tira-o do container;
  filhos de container hit-testam no rect DISPOSTO (não no ox/oy);
- **Serialização**: spacing/pad/align/parent (ausentes = defaults
  0.7.3-compat; `.goni` antigos abrem); alpha já vinha no color[4].

Suíte: 386→399 testes (+13 em `tests/test_uilayout.cpp`: label default,
texturas + fallback + placeholder, paridade por tipo (rect-a-rect com
transform exato), containers (layout/auto-fit/visibilidade/ciclos/hit-test
/detach/add-filho), menu espaçamento/fundo/alinhamento, round-trip,
z-order dos runs, applyUiTexPick). Gates verdes.

## Escopo 0.7.3 (histórico — F6: joystick editável + compostos)

**JOYSTICK/TOUCHCONTROLS EDITÁVEL** — o widget de input passa a ser uma
instância EDITÁVEL ("a UI do Player passa a ser esta instância"):

- **Campos do componente** (pos/tamanho/sensibilidade/cor), editáveis no
  Inspector de UI e serializados no `.goni` (quando não-default; os
  `.goni` da 0.6.x abrem com o layout fixo de sempre);
- **Pos em frações da área útil** (resolução-independente; o default
  reproduz o layout fixo 0.6.x no ecrã de referência 1600×720),
  **tamanho** escala o raio (base 75px), **sensibilidade** multiplica o
  eixo (clamp no círculo unitário — sens 1 = o comportamento 0.6.x
  exato), **cor** tinge a base/o knob (default = LINE do tema mono);
- **PROXY no viewport 2D**: o joystick do TIC selecionado desenha-se com
  a MESMA geometria do Play (layoutFor), arrastável (pos, com clamp
  0..1) e selecionável — mesmo SEM canvas (o early-return da dica só
  corre quando não há canvas NEM joystick);
- **"+" do modo UI ganha 8 itens**: Panel/Label/Button/Image +
  Menu/Card/Article + **Joystick** (adiciona o componente ao TIC e
  seleciona-o para edição);
- **O INPUT em Play segue o EDIT**: `touchBegin` claima na posição NOVA
  e o eixo vem escalado pela sensibilidade (aferido no CI);
- **Inspector do joystick**: pos X/Y, tamanho, sensib., cor R/G/B,
  "remover joystick" (o InputMap fica — sem fonte até nova adição).

**COMPOSTOS** (elementos de UI compostos, criáveis pelo "+"):

- **Menu**: lista vertical de botões — o texto é uma linha por item no
  formato "label>alvo" (alvo opcional; sem '>' o alvo é o próprio
  label); cada item dispara a AÇÃO do elemento com o alvo da LINHA;
  hit-test por linha (já aferido na 0.7.0);
- **Card**: panel + moldura + título no topo com separador;
- **Article**: texto multilinha com WRAP pela largura (greedy por
  palavras; nunca sai do rect).

Testes 380→386 (+6 em `test_joystick.cpp`): layout derivado dos campos
(default reproduz o fixo; pos/tamanho mandam; o layout fixo continua),
o edit afeta o INPUT (claim na posição nova; eixo × sens com clamp; sens
1 = 0.6.x), serialização (round-trip dos campos; `.goni` 0.6.x abre com
defaults), proxy no viewport 2D (sem canvas; tap seleciona; drag move
relX/relY com clamp; plano do inspector com y cumulativo; remover tira o
componente), compostos (Menu 3 itens desenha; Article faz wrap — dezenas
de glifos dentro da largura; round-trip completo; uiAddElement aceita
4..6 e recusa 7), "+" com 8 itens (Joystick adiciona e seleciona).

## Escopo 0.7.2 (histórico — F6: import robusto — navegador + galeria + aplicar)

**NAVEGADOR DE FICHEIROS IN-APP** (Importar… abre o NAVEGADOR — substitui a
lista fixa Download/Documents da 0.6.x):

- **Navegar QUALQUER pasta** do armazenamento (com o All Files Access
  concedido): diretorias primeiro (ordenadas), ficheiros só os suportados
  (.obj/.gltf/.glb/.png — case-insensitive); subir com "^ Subir" (a raiz
  fica na raiz); a lista faz scroll (id 45) quando excede 8 linhas;
- **A GALERIA nas raízes navegáveis**: [Raiz][Download][Docs][Camera]
  [Pictures] — DCIM/Camera (fotos da câmara) e Pictures entram como raízes
  próprias (um toque chega);
- **O CAMINHO é VISÍVEL no topo** do overlay: o caminho inteiro quando
  cabe; quando não cabe corta o INÍCIO e guarda o FIM ("…DCIM/Camera" — é
  onde o dono está);
- **Pasta vazia/sem acesso → mensagem COM O CAMINHO** dentro do overlay
  (nunca um toast cego); o opendir falho também loga o errno no engine.log;
- **Aplicar-após-import**: ao importar com um TIC selecionado (com
  MeshRenderer), a engine PERGUNTA "Aplicar ao TIC?" — Sim aplica já a
  textura/mesh pelo MESMO caminho do seletor do Inspector
  (`applyAssetPick`: ref do projeto + resolvers de GPU + toast/log
  honestos); Nao deixa só importado (aplicável depois nos seletores).

Testes 372→380 (+8 em `test_browser.cpp`): `listDirEntries` (diretorias
primeiro ordenadas, ficheiros suportados com .PNG case-insensitive,
subpastas, pasta inexistente → false), `parentPath`, raízes com a galeria,
rótulo do caminho (inteiro quando cabe; corta o início guardando o fim;
nunca excede a largura), mensagem vazia COM O CAMINHO, overlay (raízes
1..5 / subir 6 / entradas 7+; pasta vazia desenha a mensagem; fora
fecha), diálogo aplicar (Nao não mexe; Sim liga texture+texPath pelo
caminho do seletor) e e2e do import (listar → ler → gravar no projeto →
catálogo → aplicar no TIC).

## Escopo 0.7.1 (histórico — F6: cenas múltiplas + transições)

**CENAS MÚLTIPLAS** — cada cena é um `.goni` próprio no manifesto do
projeto (`Project::scenes`; a estrutura já existia desde a F5 — a 0.7.1
traz a UI e o fluxo completo):

- **Overlay CENAS** (Menu → "Cenas…"): lista as cenas do projeto com a
  ATIVA marcada, botão "+ Nova cena" (teclado in-app — zero IME) e troca
  com um toque; a lista faz scroll quando excede 6 linhas;
- **Criar cena**: guarda a cena ATUAL no ficheiro dela, regista a nova no
  manifesto (ativa), escreve o `.goni` VAZIO dela e persiste o manifesto;
  duplicados são recusados com toast claro (o guard impede que a cena
  existente seja substituída pela vazia);
- **Trocar de cena**: guarda a atual → muda a ativa → persiste o manifesto
  → carrega a nova (refs relativos re-ligam pelo LoadCtx canônico); a
  seleção não sobrevive à troca; falha de save aborta SEM trocar (a cena
  atual fica intacta);
- **Persistência**: fechar/reabrir mantém a cena ativa (manifesto).

**TRANSIÇÕES (fade / slide)** — `ui/SceneFx.h` puro e afervel:

- **`Scene.Transition` como ação declarativa** (novo tipo de ação do
  Button/Menu, com alvo = nome da cena e estilo `fade`|`slide` no
  Inspector de UI — botão "estilo" cicla; serializa como `act: "trans"` +
  `param`);
- **`Scene.Load` = troca instantânea** (ação `scene`, como na 0.7.0 —
  agora ligada ao carregador real);
- **Em Play** a transição tapa o ecrã e faz o SWAP no PONTO MÉDIO (nunca
  se vê a troca a seco): fade = quad preto fullscreen com alpha em rampa
  0→1→0; slide = quad que cobre vindo da esquerda e sai pela direita;
  duração 0.6 s; o overlay desenha-se por cima de TUDO (também no editor,
  para transições futuras);
- **Honestidade**: cena inexistente → toast claro; sem carregador →
  "sem carregador" (nunca sucesso falso).

Testes 367→372 (+5 em `test_scenes.cpp`): criar/trocar/persistir cenas
e2e no FakeStorage (A→B→A com round-trip de TICs e reopen do manifesto),
transição pura (swap UMA vez no ponto médio, cover em rampa, inativa =
custo zero), draw fade/slide nos batches (fullscreen/alpha; ida pela
esquerda, volta pela direita), overlay CENAS (lista/marca ativa/troca/
nova/fora fecha) e ações Scene.Load (instant) / Scene.Transition
(fade default, slide por param) + round-trip `.goni` com `act: trans`.

## Escopo 0.7.0 (histórico — F6: UI criável core + editor de UI + gestão de TICs)

**UI CRIÁVEL (componente `UiCanvas`)** — qualquer TIC pode ter um canvas
de elementos 2D desenhados POR CIMA da cena em Play (pass UI sem depth —
a UI nunca é ocluída):

- **Elementos**: Panel, Label, Button, Image (textura do projeto via ref
  relativa — placeholder mono com moldura+diagonal quando a textura não
  existe/ainda não carregou);
- **Ancoragens 3×3** (esquerda/centro/direita × topo/meio/fundo): o rect de
cada elemento vive relativo ao PONTO DE ÂNCORA da safe-area — resolução-
independente (aferido em 2 resoluções no CI); mudar a âncora PRESERVA a
posição absoluta (`setAnchor`);
- **Ações declarativas on-click**: mostrar/esconder/alternar panel (por
  NOME, em qualquer canvas), carregar cena (alvo validado contra o
  manifesto; o carregamento em Play chega na 0.7.1 com as transições — o
  toast é HONESTO, nunca finge sucesso) e spawn de preset
  (PlayerBody3D/CharacterBody3D/StaticBody3D/RigidBody3D);
- **Hit-test em Play**: o TOPO ganha (z-order), só Button/Menu são
  interativos; o toque num botão da UI é RECLAMADO (não vai aos
  TouchControls nem à câmara); o press ARMA e o release DENTRO do mesmo
  elemento dispara (o gesto clássico de botão);
- **Serialização**: `UiCanvas` completo no `.goni` (elementos com kind/
  nome/texto/rect/cor/visível/âncoras/ação+alvo) — builds antigas IGNORAM
  o componente (forward-compat do serializer).

**EDITOR DE UI DEDICADO (separador "3D | UI" na toolbar)**:

- O botão "UI" troca o viewport central: deixa de mostrar a cena 3D e
  passa a mostrar o canvas do TIC selecionado em ESCALA-CABER (nunca
  corta; o espaço de design é o ecrã inteiro) com moldura do "ecrã";
- **WYSIWYG**: tap seleciona o elemento sob o dedo (o de CIMA ganha),
  drag MOVE (ox/oy pelo delta em design px), tap no vazio DESSELECCIONA;
  os gizmos e o orbit ficam DESLIGADOS no modo UI;
- **Inspector de UI** (painel direito, quando há elemento selecionado):
  pos x/y, largura/altura, cor R/G/B, visível, âncoras (ciclam), texto
  (teclado in-app), ação + alvo, remover elemento — plano com y cumulativo
  (o contrato F5.0-fix) e scroll;
- **"+" do modo UI cria elementos** (Panel/Label/Button/Image; o canvas é
  criado à primeira no TIC selecionado) e SELECIONA o novo — WYSIWYG
  imediato.

**GESTÃO COMPLETA DE TICs**:

- **Desselecionar**: tocar no VAZIO do viewport (tap parado — drag continua
  a orbitar) ou da Hierarchy limpa a seleção;
- **Menu contextual** (botão "..." na linha da Hierarchy — o long-press da
  spec é o atalho alternativo; o botão é determinístico e aferível):
  **Renomear** (teclado in-app) / **Remover** (diálogo de CONFIRMAÇÃO —
  substitui o apagar sem aviso) / **Duplicar** (todos os componentes por
  valor + nome único Godot-style ".001") / **Visibilidade**;
- **Visibilidade**: toggle "O/X" na Hierarchy + checkbox "visivel" no
  Inspector; TIC invisível NÃO desenha em editor nem em Play — a FÍSICA e
  a LÓGICA continuam (invisível ≠ desligado);
- **Cor por TIC**: sliders R/G/B no Inspector (MeshRenderer) — tint
  multiplicativo no shader lit (`uTint`; branco default = comportamento
  0.6.x byte a byte);
- **Teclado in-app**: A-Z, 0-9, '_', '-', espaço, APAGA, OK/Cancelar —
  ZERO IME de sistema (frágil em NativeActivity); geometria partilhada
  com os testes (sem sobreposição, dentro da safe-area).

**Serialização nova**: `visible` (TIC; ausente = visível), `tint` (R/G/B;
ausente = branco), componente `UiCanvas`. Testes 337→367 (+30):
âncoras em 2 resoluções, hit-test (topo/interativos/invisíveis), ações
declarativas (toggle por nome, spawn, cena honesta), menu por linha,
desenho sem sobreposição, round-trip `.goni` completo, cenas 0.6.x a
abrir com defaults, toggle 3D|UI, viewport 2D (scale-to-fit, WYSIWYG
seleciona/move/desseleciona), plano do Inspector de UI, teclado
(geometria + digitar/apagar/OK/cancelar/vazio/sufixo único), menu
contextual (4 ações), remoção com confirmação, desselecionar (viewport
tap≠drag, vazio da Hierarchy), cor por TIC (sliders + bind no stub GL),
toolbar sem sobreposição em 2 larguras.

## Escopo 0.6.10 (histórico — fix do seletor de textura do C33)

**WIRING DO SELETOR DE TEXTURA DO INSPECTOR**: no C33 (0.6.9), após
importar um PNG (import OK, `textures/screenshot-….png` no projeto) e tocar
`tex:` escolhendo a imagem, NADA acontecia — o estado ficava `tex: none`,
o cubo continuava cinzento e o engine.log não registava linha de aplicação
nem de erro. CAUSA RAIZ: `drawAssetMenu` fecha o seletor NO CLIQUE
(`st.assetMenu = 0` antes do return) e o dispatch do main lia
`g_editor.assetMenu` DEPOIS da chamada → sempre 0 → o bloco `if (pick >
0)` era **código morto desde a F5-E (0.5.0)** — a escolha nunca chegava ao
MeshRenderer (o mesmo no seletor de mesh):

- **Dispatch puro e afervel** — `editor::applyAssetPick` (ui/EditorUi):
  escolher textura → atualiza `texture` + `texPath` do MeshRenderer do TIC
  selecionado (o render é immediate-mode: `drawTics` lê `mr->texture` a
  cada frame → o bind acontece no frame seguinte, sem flags dirty);
  escolher mesh → `mesh`/`material`/`meshPath` + textura embutida do
glTF/GLB quando existe;
- **Fix da ordem** — o `menuKind` é capturado ANTES do `drawAssetMenu`
  (o seletor fecha-se a si próprio no clique; o protocolo da sequência está
  travado por teste com um clique REAL injetado);
- **Remover textura** — `none` no seletor liberta a referência (Inspector
  volta a `tex: none`, cubo cinzento);
- **Falha honesta** — carga que falha mantém o estado ANTERIOR intacto +
  toast + linha `FALHOU` no log;
- **Logging** — `material: textura aplicada <ref>` / `material: textura
  removida` no engine.log (visível no log viewer do C33);
- **Persistência intacta** — a ref já era gravada no `.goni`
  (`SceneSerializer`): Save → Load preserva a textura aplicada (o
  re-resolve via `LoadCtx` já existia);
- **Testes CI** — `test_assetpick.cpp` (11 casos): escolher muda o estado,
  sequência do main (o bug da ordem), bind confirmado no stub GL
  (`glBindTexture` + `uHasTex=1/0`), round-trip `.goni`, remover volta a
  none, aviso do gate no toast, alvos inválidos sem crash.

## Escopo F5.5 (histórico — fix do All Files Access no C33)

**MANAGE_EXTERNAL_STORAGE DECLARADO** (fix do C33 0.6.9): sem a declaração,
o sistema não tinha o que conceder — a app não aparecia na lista "Acesso a
todos os ficheiros" (o Godot aparecia "Permitida") e o toggle na página da
app não concedia nada (`isExternalStorageManager()` sempre `false`):

- **Manifest**: `<uses-permission android:name="android.permission.
  MANAGE_EXTERNAL_STORAGE" />` — sem física/render/componentes (CLÁUSULA
  CALMA; decisão e implicação Play Policy documentadas em
  `docs/SAF_EXCEPTION.md`);
- **Fluxo mantido**: diálogo in-app → "Permitir" →
  `ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` → voltar → re-verificar
  `isExternalStorageManager()` → prosseguir import/export;
- **Re-verificação no resume** (`APP_CMD_RESUME`): drena a fila do
  `onActivityResult` primeiro; se o retorno nunca chegou (ecrã OEM sem
  `setResult`), verifica fresco e decide — `storage::resumeRecheck`
  (política pura, afervel no CI). Recusa → toast claro SEM loop;
- **Log de transição**: `storage: all-files granted=1/0` no engine.log a
cada transição (boot / retorno / resume / variação por tentativa) — visível
no log viewer;
- **Gate CI**: o manifest BINÁRIO do APK passou a ser aferido (aapt2) — a
  permissão tem de estar presente em TODO build.

## Escopo 0.6.9 (histórico)

**GIZMOS DE TRANSFORMAÇÃO — Mover / Rodar / Escalar** (só no TIC
selecionado, SÓ em EDITOR — nunca em PLAY):

- **Mover:** 3 setas por eixo + 3 quads de plano (XY/XZ/YZ) — drag ao longo
  do eixo (projeção no eixo; ruído fora do eixo é ignorado) ou no plano
  (decomposição nos 2 eixos do plano);
- **Rodar:** 3 anéis por eixo — drag angular no plano do anel (ângulo do
  dedo em torno do centro projetado; sinal corrigido pela orientação do
  eixo face à câmara; rotação GLOBAL — pré-multiplicação);
- **Escalar:** 3 handles por eixo + handle central uniforme (arrasto
  radial por distâncias em px ao centro); clamp em 0.05 (nunca
  zero/negativo);
- **Hit-test 3D:** raio do toque contra eixos/anéis/handles — distâncias
  em PX de ecrã sobre a geometria PROJETADA (o MESMO critério do desenho);
  eixo sob press DESTACADO (branco ACCENT + traço mais grosso);
- **Seletor de modo na toolbar** (grupo à direita — os 3 botões
  Menu/Play/Settings ficam INTACTOS): [Mover][Rodar][Escalar] + [Snap]
  (toggle);
- **Snapping opcional:** mover ao grid de 0.5 u, rodar a 15°, escalar em
  passos de 0.25;
- **Gestos:** drag em gizmo NÃO orbita a câmara (o slot que apanha o
  gizmo é reclamado — a máscara vai ao `editor::updateCameraOrbit`); drag
  fora do gizmo = orbit normal;
- **Escrita no Transform3D** (pos/rot/scale) com âncoras (pose final =
  âncora + delta — o jitter nunca se acumula) + `updateWorld()` imediato;
- **CORES DE EIXO (X vermelho / Y verde / Z azul): EXCEÇÃO DOCUMENTADA ao
  tema mono, SÓ nos gizmos 3D** — a UI 2D mantém o tema mono intacto;
- **Desenho:** projeção da geometria 3D para segmentos de ecrã
  (`QuadBatch::line` novo — retângulo rotacionado), SEM depth (sempre
  visível) e por baixo dos painéis (z-order de editor); tamanho de ecrã
  CONSTANTE (~16% da distância da câmara).

O núcleo (projeção/raio/hit-test/drag/snapping) é GL-free e testado no CI
Linux (26 testes novos em `tests/test_gizmo.cpp`).

## Escopo 0.6.8 (histórico)

**PLAY MODE COM JANELA PRÓPRIA.** Dois modos de UI com transição Play/Stop:
o **EDITOR** (toolbar de 3 botões + Hierarchy + Inspector + menus — intactos)
e o **PLAY** (viewport fullscreen + TouchControls ancorados na safe-area +
barra superior mínima). Em PLAY: painéis de edição e toolbar ESCONDIDOS
(early return no frame — nunca desenhados); o botão Stop (id 5, faixa
exclusiva) devolve ao editor com a pose restaurada (PlaySnapshot intacto) e
os painéis repostos exatamente (scroll/seleção vivem fora das flags de
overlay). A barra PLAY mostra o estado "a correr · fps N", o botão Stop e o
aviso "simulação — alterações descartadas ao parar" (labelFitted — nunca
sai da barra). O ORBIT fica DESATIVADO em play (1 dedo = controlos): a
lógica de orbit foi extraída do main para `editor::updateCameraOrbit`
(`OrbitState` puro, regra F3 do dono-do-gesto e pinch intactos) com guard
`playMode` que reseta o gesto pendente — o orbit volta a funcionar ao sair.
O estado do modo vive em `EditorState.playMode` (não numa global do main) —
a transição completa editor→play→editor é testável na suíte.

## Escopo 0.6.7 (histórico)

**1 — LIFECYCLE GL (fix dos "cubinhos" do C33).** Sair do editor
(home/recents) e reentrar SEM matar a app deixava TODO o texto em quads
brancos: o `APP_CMD_TERM_WINDOW` destrói o contexto EGL (o
`EglContext::shutdown` mata surface E contexto — todos os ids GL ficam
órfãos), mas o `FontAtlas` guardava `tex_ != 0` (guard "já carregado") e o
2º `APP_CMD_INIT_WINDOW` NUNCA re-upava o atlas → o pass UI amostrava uma
textura órfã = quads brancos. O mesmo acontecia aos mapas do `GpuAssets`
(`releaseAll()` existia mas ninguém chamava). FIX: `FontAtlas::destroy()`
novo (`glDeleteTextures` + reset do id E das métricas — chamado no
TERM_WINDOW com o contexto ainda corrente; o guard passa a impedir só o
upload duplicado no MESMO contexto); TERM_WINDOW com ordem obrigatória
detach dos MeshRenderers → `g_gpu.releaseAll()` → `g_font.destroy()` →
cubo/grid → renderer → `g_egl.shutdown()` POR FIM; INIT_WINDOW loga a
RE-CRIAÇÃO do contexto e confirma o RE-BAKE + RE-UPLOAD. NENHUM recurso GL
é assumido vivo entre term/init (a cena é recarregada do disco e os
resolvers re-upam os assets).

**2 — APAGAR PROJETO no gestor.** Long-press numa entrada → diálogo com o
nome e a pasta e DUAS ações: "Remover da lista" (a pasta fica — como
antes) e "Apagar projeto" → confirmação EXPLÍCITA separada ("Apagar
projeto X? Não pode ser desfeito — a PASTA e TODOS os ficheiros são
apagados") → `VvProjects.deleteProject` novo remove a pasta via File API
(`DocumentsContract.deleteDocument` no URI de documento da raiz) + sai da
lista; a permissão persistente é libertada e a lista refresca.

**3 — "Sair para projetos" no editor.** Menu → 6º item: auto-save da cena
(cena + manifesto + assets de runtime materializados) → a activity
termina-se (`VvActivity.bridgeFinish` novo, chamado por
`jniFinishToLauncher` na ponte JNI) e o GESTOR retoma da back stack SEM
matar a app. Reentrar arranca um NOVO `android_main` — que agora faz RESET
de estado na entrada (TickGroups.clear() novo — sem isto a física seria
registada 2× e daria dois passos por frame; InputState.resetAll() novo;
cena/editor/play/projeto/catalog resetados) com um contexto EGL novo
(re-upload de tudo — o fix 1 cobre a reentrada).

**4 — fix do nome do projeto invisível.** O diálogo de novo projeto herda
o tema CLARO do manifest (`Theme.NoTitleBar.Fullscreen`) → painel branco;
o EditText tinha só texto quase-branco = texto branco sobre fundo branco
durante a digitação. Fix determinístico sem `res/`: fundo escuro
explícito (0xFF1E222A) + texto claro + hint + `requestFocus()` pós-show.

## Escopo F5.4 (histórico)

**PARTE 1 — fix do `UnsatisfiedLinkError` (RMX3624, Android 13).** O
logcat mostrava `No implementation found for void
vv.goni.VvActivity.nativeRegisterActivity(...)` no onCreate E no onResume —
o handshake nunca ficava OK e todo import/export falhava com "ponte Java
indisponível (handshake)". Causa raiz (docs/HANDSHAKE_AUDIT.md §6): o
`android.app.NativeActivity` carrega a lib com `dlopen(RTLD_LOCAL)` CRUO
(`loadNativeCode_native`) — um dlopen cru NÃO corre o `JNI_OnLoad` nem
registra a lib no mapa de resolução do JVM, logo o `RegisterNatives` da
0.6.3 nunca correu no device e a busca por nome não via a lib (a premissa
"super.onCreate faz System.loadLibrary" era FALSA — o hospedeiro testava o
C++ direto, a RESOLUÇÃO não era modelada). FIX em 2 camadas: (1)
`static { System.loadLibrary("goni_vv"); }` na VvActivity — o JNI_OnLoad
corre de verdade, o RegisterNatives executa e a lib entra no mapa do JVM;
(2) `ensureNativesRegistered()` idempotente no 1º nativeRegisterActivity
via GetObjectClass + `JNI_OnLoad` TOLERANTE (FindClass/RegisterNatives a
falhar NUNCA devolvem JNI_ERR — logam e adiam para a 2ª camada). GATE NOVO
pedido pelo dono: `scripts/jni_parity.py` no CI afere TODO native da
VvActivity.java contra a tabela RegisterNatives (nome+assinatura), o static
loadLibrary obrigatório e, no release, os símbolos `Java_vv_goni_VvActivity_*`
no `.dynsym` do .so real — na 1ª execução apanhou um bug latente real
(assinatura de nativeOnActivityResult com um 'I' a mais; corrigida).

**PARTE 2 — Gestor de Projetos (estilo Godot, múltiplas pastas).** A app
abre agora no ecrã **"Projetos"** (ProjectManagerActivity, launcher novo):
lista dos projetos já criados (vazia na 1ª instalação), toque abre direto
no editor, long-press remove da lista (a pasta NÃO é apagada). "+" → nome →
seletor de pastas do sistema (SAF, `ACTION_OPEN_DOCUMENT_TREE`) → a pasta
de CADA projeto é escolhida por ele: cada escolha gera um URI próprio,
guardado com `takePersistableUriPermission` — projetos diferentes em
pastas diferentes, NUNCA se misturam, MESMO SEM All Files Access. A
estrutura (project.goni, scenes/, meshes/, textures/) é criada na pasta
colhida e o editor abre esse projeto (SafStorage: ProjectStorage sobre a
árvore SAF — o editor não sabe a diferença). A lista vive em
`projects.json` no app-private (não depende do handshake nem de
permissões). O All Files Access mantém-se EXATAMENTE como estava, só para
import/export de assets soltos DENTRO de um projeto aberto
(Download/Documents) — as duas coisas COEXISTEM.

## Escopo F5.3 (histórico — handshake invertido)
Objetivo: a ponte Java↔native LIGAR DE VERDADE em runtime (handshake
INVERTIDO: a activity Java registra-se no native), para o fluxo All Files
Access funcionar no C33: diálogo → settings do app → permissão ativada →
File API direta. Mensagens de erro honestas (causa real).

No C33 (0.6.2), o engine.log mostrava `jni: initJava env/activity
indisponíveis (vm=0x…)` → `supported=0 manager=0 modo app-private` →
`export falhou (copied=0)`. Causa única: o `android_main` corre no thread do
glue (pthread) que NÃO está anexado à VM — `GetEnv` devolvia `JNI_EDETACHED`
e a ponte morria no arranque. As auditorias estáticas (manifest/dex/símbolos)
estavam corretas — o bug era o handshake runtime
(docs/HANDSHAKE_AUDIT.md tem o despejo do manifest do APK 0.6.2 REAL).

1. **APK real verificado (item 1)** — o 0.6.2 do CI despejado:
   `hasCode=true`, launcher=`vv.goni.VvActivity`, `lib_name=goni_vv` —
   o sistema instancia a VvActivity; o dex não tinha onCreate/onResume
   (nunca se registrava). Gate NOVO no CI (`aapt2 dump xmltree`) afere o
   manifest binário em TODO build.
2. **Handshake invertido (item 2)** — `VvActivity.onCreate()` chama
   `nativeRegisterActivity(this,"onCreate")` (após `super.onCreate`, quando
   o loadLibrary/JNI_OnLoad já correu); `onResume()` reforça (idempotente —
   GlobalRefs substituídos). O native guarda `JavaVM` + `GlobalRef` + métodos
   e loga: `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` → no boot `handshake=1`. A chamada
   `initJava` do android_main foi REMOVIDA (o native não descobre a
   activity sozinho — era a causa).
3. **Attach de threads (item 3)** — qualquer thread da engine que chame
   Java faz `AttachCurrentThread` NOMEADO (`goni-engine`, via
   `JavaVMAttachArgs`) e PERMANENTE (sem Detach); decisões na tabela pura
   `platform/JniAttach.h` (JNI_OK→usa; EDETACHED→attach; resto→falha com o
   CÓDIGO no log). Nenhum `JNIEnv*` assumido não-nulo.
4. **Fluxo All Files pós-handshake + mensagens honestas (item 4)** —
   diálogo → `ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` →
   `isExternalStorageManager()` no retorno → File API direta. Se o
   handshake falhar, toast/log diz **"ponte Java indisponível (handshake)"**
   — NUNCA "sistema sem All Files Access" (0.6.2 mentia sobre a causa;
   `blockReason`/`blockMessage` em `StoragePerm.h` distinguem ponte vs
   sistema, testados no CI).
5. **Testes CI (item 5)** — `tests/test_handshake.cpp` + a ponte JNI
   INTEIRA compilando na suíte com um fake JNI controlável
   (`tests/stub/jni.h`): handshake simulado (o teste chama
   `nativeRegisterActivity` — o papel do "stub Java"), re-registro
   onResume, registo parcial, attach com/sem falha (rc logado), fluxo de
   permissão completo (diálogo → intent 4301 → onActivityResult → fila →
   Granted + ação retomada), regressão das mensagens honestas e
   `JNI_OnLoad` com 2 nativos. 254→264 testes.

## Escopo F5.2 (implementado)
Objetivo: o fluxo correto de permissões de armazenamento (o mesmo do Godot) +
diagnóstico in-app — no C33, o 0.6.1 mostrava "SAF indisponível" sem nunca
pedir permissão; o SAF tree picker foi REMOVIDO.

1. **Diálogo in-app (item 1)** — ao tentar Importar/Export pela primeira vez:
   overlay mono "ARMAZENAMENTO — Precisa de acesso a todos os ficheiros para
   importar/exportar projetos" com **Permitir/Cancelar** (mensagem quebrada
   pela fonte real; toque fora fecha). Máquina de estado GL-free em
   `platform/StoragePerm.h` (testada no CI: diálogo só na 1ª tentativa,
   cancelar volta a Idle, sem nag em loop).
2. **Abrir settings (item 2)** — "Permitir" → `VvActivity.openAllFilesSettings(4301)`
   lança **`ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION`** com `package:vv.goni`
   (a janela de permissões DO app). Constante `kSettingsAction` afervel no CI.
3. **Verificar + File API direta (item 3)** — o retorno chega por
   `onActivityResult` → fila JNI → thread da engine → `Environment.isExternalStorageManager()`
   re-verificada; concedido → **File API POSIX direta**: import varre
   `Download/` + `Documents/` (obj/gltf/glb/png, ordenado, overlay de escolha)
   e copia para `meshes/`/`textures/`; export grava
   `Download/GOneVV/export/export_<nome>.obj` (visível no gestor).
4. **Log viewer in-app (item 4)** — Settings → "Ver logs": viewer com SCROLL
   (id 43, auto-scroll para o fim) desenha o tail do `engine.log` + a lista de
   `crash-*.dump` — **funcional sem export**.
5. **Boot self-check (item 5)** — `fileapi::logStorageSelfCheck` no arranque:
   `getExternalFilesDir` (null?) + errno de cada opendir/fopen falhado no
   `engine.log` — a CAUSA de qualquer falha de storage fica registrada.
6. **Fallbacks (item 6)** — recusa ou API < 30 → modo **app-private**
   (`getExternalFilesDir`, sem permissões); o Settings mostra sempre
   "armazenamento: all files / app-private" + botão "Acesso a ficheiros…".
7. **Testes CI (item 7)** — 26 novas verificações no hospedeiro: constantes
   do fluxo, resolveMode, diálogo (mostrado 1×/permitir/cancelar/negado),
   intent (consumeOpenSettings), permissão verificada (onSettingsReturn +
   ação retomada), fallback sem suporte, File API (lista/roundtrip/errno),
   self-check, readTail/listDumps, e 4 testes de UI (diálogo, import,
   viewer, settings) com fonte real e taps injetados.

SAF removido: `SafIo.java`, `SafIoJni.cpp`, `core/SafStorage` e o item
"Pasta (SAF)" saíram; `docs/SAF_EXCEPTION.md` documenta o All Files Access
como caminho primário (com nota de Play Policy). A ponte Java mínima
(VvActivity) continua — `NativeActivity` não lança a janela de permissões nem
reencaminha `onActivityResult`.

## Escopo F5.1-hotfix (implementado)
Objetivo: o 0.6.0 crasha no arranque no C33 e o dono não tem PC/logcat — a
engine passa a **autodiagnosticar-se em ficheiros legíveis** e a ponte JNI/SAF
foi auditada e corrigida (docs/AUDIT_SAF_JNI.md).

1. **Log writer (parte 1.1)** — `vv::elog::info/warn/error` faz DUAS coisas:
   `__android_log_write` (logcat) + append a `Android/data/vv.goni/files/logs/`
   (= `getExternalFilesDir("logs")`) com rotação por tamanho: 3 ficheiros de
   1MB (`engine.log` + `.1` + `.2`, o mais velho descartado). Thread-safe;
   ativo desde a 1ª linha (o `JNI_OnLoad` usa o caminho fallback antes do
   `android_main`).
2. **Boot progress (parte 1.2)** — cada passo crítico do arranque escreve um
   marcador: `[boot 2/6] storage`, `[boot 5/6] physics` (android_main),
   `[boot 1/6] contentRect`, `[boot 3/6] fonts`, `[boot 4/6] renderer`,
   `[boot 6/6] scene OK → editor` (INIT_WINDOW). O dono vê EXATAMENTE onde o
   boot parou.
3. **Crash dump (parte 1.3)** — handlers C++ para SIGSEGV/SIGABRT/SIGBUS/
   SIGFPE (SA_ONSTACK + sigaltstack): recolhem o stacktrace via
   `_Unwind_Backtrace` (bionic `backtrace()` só existe API 33+; C33 é 31/32)
   e resolvem módulo+função+offset com `dladdr()` → `crash-<timestamp>.dump`
   no MESMO diretório, formato legível sem ndk-stack
   (`#03 pc 0x1a2b3c  libgoni_vv.so (função+0x88)`) + pc/lr/sp do fault
   (arm64). Ativo em TODAS as builds (release incluída); re-raise mantém o
   tombstone do sistema.
4. **Export de logs (parte 1.4)** — botão **Settings → "Exportar logs"**:
   copia TODOS os ficheiros de `getExternalFilesDir("logs")` para
   `Downloads/GOneVV/logs/` via MediaStore (API 29+, SEM
   WRITE_EXTERNAL_STORAGE; export repetido substitui). Caminho definido pela
   constante única `vv::elog::kDownloadsRelPath` (afervel no CI).
5. **Auditoria JNI/SAF (parte 2)** — evidência no APK 0.6.0 real: manifest,
   dex e símbolos OK (crash é runtime). Fixes: `JNI_OnLoad` com
   `RegisterNatives` explícito (falha morre no arranque COM log), higiene de
   exceções JNI em toda a ponte, resultado SAF DIFERIDO para o thread da
   engine (o handler antigo corria no thread da UI e chamava GL sem contexto
   EGL), guardas nulos + logging por método. Detalhe em
   **docs/AUDIT_SAF_JNI.md**.

## Escopo F5 (implementado)
- **Projeto e storage (F5-A)**: `core/ProjectStorage` (interface única —
  F5.2/SAF entra como outra implementação) + `core/FsStorage` (POSIX; no
  device a raiz é `getExternalFilesDir()`, sem permissões) + `core/Project`
  (manifesto `project.goni`: nome/versão/cenas/settings verbatim; refs
  RELATIVOS; `scenes/` `meshes/` `textures/`). Boot abre o projeto existente
  ou cria "projeto"; a cena ativa volta do disco a cada arranque; Menu
  Save/Load opera no projeto. Guarda de caminhos: `../`, absoluto e `//`
  recusados em toda a interface.
- **Import OBJ (F5-B)**: `assets/ObjImporter` (v/vn/vt, todas as formas de
  canto, fan de polígonos, dedup de cantos, grupos g/o + usemtl com ranges,
  índices negativos, CRLF, limite u16 sobre vértices ÚNICOS) +
  `assets/ObjExporter` (round-trip testado) + `assets/ResourceManager`
  (cache CPU por ref: 1 carga, ponteiro estável).
- **Import glTF/GLB (F5-C)**: `assets/GltfImporter` — accessors
  bounds-checked (POSITION/NORMAL/TEXCOORD_0, índices u8/u16/u32),
  bufferViews com byteStride, data URIs base64, buffers externos relativos à
  pasta do .gltf, container GLB validado, materiais básicos (name +
  baseColorFactor), nós → hierarquia (TRS). `assets/GltfInstantiate` cria
  TICs com parent/Transform3D/MeshRenderer com ref `path#i`.
- **Texturas (F5-D)**: `assets/PngLoader` (stb_image vendor, RGBA 8-bit) com
  gate 1K/2K OK, 4K+ reduzido para 2K com aviso; `assets/TextureCompressor`
  (interface; passthrough na F5 — ASTC/ETC2 é F5.1); `render/Texture`
  (glTexImage2D + glGenerateMipmap, LINEAR_MIPMAP_LINEAR, REPEAT).
- **Export + editor (F5-E)**: Menu ganha "Export OBJ" (mesh do TIC →
  `meshes/export_<tic>.obj`); Inspector mostra `mesh:`/`tex:` com a origem do
  asset e abre SELETORES de `meshes/` e `textures/`; status line mostra
  objetos de GPU (`am`/`at`); `render/GpuAssets` garante 1 ref = 1 objeto GL
  (memória de GPU não duplica); `MeshRenderer` persiste `meshPath`/`texPath`
  no `.goni` com resolvers no load.
- CI: parsers OBJ/glTF/GLB, round-trip, cache, caminhos relativos e gate 4K
  cobertos por testes (196 no total).

Sem skinning/animação (F7), sem editor de materiais (F8), sem streaming (F9),
sem compressão ASTC/ETC2 (F5.1), sem SAF (F5.2) — CLÁUSULA CALMA.

## Escopo F4.2 (implementado)
Três bugs vistos no C33, corrigidos com CLÁUSULA CALMA (só layout/UiContext/
EditorUi + testes; zero física nova, zero componentes novos, zero render 3D;
mono/3 botões/landscape intactos):
- **B1 — safe-area (fix raiz do scroll morto)**: a UI assumia a superfície
  EGL inteira, mas a nav bar tapava o fundo dos painéis → o Inspector media
  contentHeight 536 contra um visibleHeight inflado (540) → maxOffset 0 →
  scroll nunca ativava. O main lê `android_app->contentRect`
  (`APP_CMD_CONTENT_RECT_CHANGED`) e injeta `safe::Insets` (novo
  `ui/SafeArea.h`, GL-free, fonte única das constantes de layout) — toolbar,
  painéis, viewport, status line, toast, TouchControls e overlays vivem
  DENTRO do contentRect; com a altura real do painel o overflow é detetado e
  o scroll ativa. Diagnóstico no logcat: `safearea: surface … content …
  insets …`
- **B2 — labels cortadas**: novo `ui/TextFit.h` (GL-free) +
  `UiContext::labelFitted` — toda label mede antes de desenhar e trunca com
  reticência ASCII `...` (o atlas não tem U+2026) mantendo o maior prefixo
  que caiba: nome do TIC, `body: rigid - sphere - chao: sim` (o caso
  reportado), mesh/input/tc, texto dos botões (linhas da Hierarchy), status
  line e toast
- **B3 — sandbox do Play**: novo `core/PlaySnapshot.h` — ENTRAR no Play
  captura pos/rot/scale de todos os Transform3D + velocity/grounded de todos
  os BodyComp; SAIR repõe a pose de editor EXATA e descarta a simulação
  (TIC destruído no Play é saltado; handle generacional nunca recria). A
  física (TickGroup::Physics) continua a correr SÓ durante o Play (gate
  `enabled` inalterado)
- CI: fix do workflow — o output `artifact_name` era descartado pelo
  mascaramento do secret `VV_KEY_ALIAS` ("vv"); o gate verify-entry-symbols
  passa a baixar por `pattern` (robusto a segredos e a mudanças de versão)

## Escopo F4.1 (implementado)
Scroll como primitivo de UI, aplicado ao Inspector e à Hierarchy — conteúdo
mais alto que o painel fica alcançável (desbloqueia a verificação da F4 no
device). Só scroll (CLÁUSULA CALMA): sem safe-area, sem colapsáveis, sem
física, sem render 3D.
- `ui/ScrollMath.h` (GL-free, testada no CI): clamp `[0, content − visible]`,
  drag-to-scroll com clamp nos dois extremos, limiar tap (12 px) vs drag,
  protocolo de claim (slider > scroll > botão dentro de região), clip de
  quads por interseção com UV proporcional, geometria do indicador
- `UiContext::beginScroll(id, region, contentHeight)` / `endScroll()` —
  slots por id (offset persiste), botões dentro da região só desenham (o tap
  é re-despachado pelo painel — "add TouchControls" clicável mesmo após
  scroll), sliders mantêm prioridade, indicador fino ACCENT só com overflow;
  recorte por interseção (sem glScissor no quad batch único)
- Hierarchy: lista TODOS os TICs (fim do corte em ~10 linhas da F3); tap na
  linha seleciona via `hierarchyRowAtTap`
- Inspector: BodyComp, `velx` e o botão "add TouchControls" no fundo ficam
  sempre alcançáveis; `ui/EditorLayout.h` é a fonte ÚNICA das alturas de
  conteúdo (desenho e testes partilham os números)
- Gestos sem conflito: drag na região = scroll; drag no slider = slider;
  tap = seleção/ação; viewport central NÃO é região de scroll (orbit/pinch
  intactos)
- CI: suíte do core (122 testes) + APK assinado

## Escopo F4 (implementado)
Física core num novo `TickGroup::Physics` (entre Update e PostUpdate) —
`physics/` é C++ puro, host-testável, sem GL:
- Formas: `Sphere`, `AABB`, `OBB` (rotQuat), `Capsule` (eixo Y local do TIC);
  interseções puras (sphere×{sphere,AABB,OBB,Capsule}, AABB×AABB, OBB×OBB SAT
  15 eixos, Capsule×Capsule) e sweep contínuo com TOI+normal para sphere/
  capsule contra AABB/OBB com CCD adaptativo (substeps máx 8 + avanço
  conservador exato via gradiente da SDF — nunca tunela paredes finas)
- `BodyComp` (Static/Character/Rigid + shape variant + velocity + grounded);
  `PhysicsSystem`: Character com input e slide estilo `move_and_slide`
  (remove a componente normal; iterações extra para cantos), repel entre
  Characters, Rigid com gravidade + colisão primitiva contra Static +
  amortecimento no chão. PLACEHOLDER DE SOLVER: dois Rigid empilhados
  intersectam (sem stacking/resting, documentado)
- `TouchControls` mínimo e FIXO (joystick esquerda + botão JUMP direita, tema
  mono, desenhado só em modo Play) como fonte de `InputSource`; `InputMap`
  liga zero ou uma fonte (sem fonte → sem input); UI criável = F6
- Presets: Player/Character/Static ganham Body, NOVO RigidBody3D; TouchControls
  adicionável via Inspector; Inspector mostra BodyComp (tipo·forma·chão) com
  slider `velx` para lançar corpos no device (testes de CCD/empurrão)
- CI: suíte do core (113 testes) + APK assinado

## Escopo F3.1 (implementado)
A pedido do dono no C33: remover as barreiras artificiais de câmara e baratear
o grid — só câmara + grid + testes (CLÁUSULA CALMA; nada de componentes,
presets, serialização, física ou UI de editor).
- Câmara: zoom 1..300 (era 1.5..40), pitch ±89° (era ±83° — top-down quase
  total sem degenerar o lookAt), yaw livre; near 0.5 / far 450 (zoom máx ×
  1.5, rácio 900:1 — sem z-fighting no depth de 24 bits)
- Grid: a malha de linhas virou UM quad de 4 vértices que segue a câmara, com
  linhas procedurais no fragment shader (`fract` + `fwidth`), anti-moiré por
  minificação e fade radial adaptativo ao zoom; extent ≥ 2000, borda nunca
  visível; interface (`init/draw/vertexCount/ok`) e tema mono mantidos
- CI: suíte do core (74 testes) + APK assinado; o build NDK valida o GLSL
  embutido

## Escopo F3 (implementado)
- `components/`: `Transform3D` (pos/Quat/scale + cache world TRS), `MeshRenderer`
  (Mesh* + Material* não-donos), `InputMap` (vazio — F4 preenche)
- `core/`: `Component` (owner + hooks attach/detach), `ComponentStorage<T>` SoA
  (deque — endereços estáveis), `ComponentRegistry` (nome→id→factory),
  `ComponentStore` (bolsa da Scene), `Tic` container
  (`addComponent<T>/getComponent<T>/removeComponent<T>`), `Presets`
  (PlayerBody3D/CharacterBody3D/StaticBody3D, nomes únicos .001),
  `Tick` (PreUpdate→Update→PostUpdate→Render), `TransformSystem`,
  `Json` mínimo + `SceneSerializer` (.goni v1, migração v0→v1, tipos
  desconhecidos ignorados no load)
- `math/`: `Quat` (unitário, euler YXZ, `toMat4` reproduz `rotX/Y/Z` da F2)
- `ui/`: `UiContext.slider` + `EditorUi` — Hierarchy esquerda (seleção + botão
  "+"), Inspector direita (9 sliders pos/rot graus/scale), overlays
  ("+" → 3 presets; Menu → Save/Load), toasts; tema mono; 3 botões F1 mantidos
- `platform/main.cpp`: pass 3D desenha todos os TICs com MeshRenderer
  (fim do cubo hardcoded); TickGroups por passo fixo; gate da câmara
  (gestos só nascem no viewport central)
- CI: `verify-entry-symbols` + suíte do core (113 testes) + APK assinado

Sem física (BodyComp é F4), sem luzes, sem assets, sem animação, sem linguagem
(CLÁUSULA CALMA).

## Histórico
- **F5.4-hotfix (0.6.5)**: SAF SEM duplicação ("main.goni (1).json", "project.goni (2)" em TODO boot): (1) causa raiz = mime `application/json` para `.goni` → o provider renomeia no createDocument (FileUtils.buildUniqueFile acrescenta extensão canónica) e o nome no disco divergia do nome procurado → verificação falhava → createDocument de novo; (2) `bridgeFindFile` NOVA — pesquisa EXATA por displayName com query fresca ao provider ANTES de qualquer createDocument (método CRÍTICO do handshake; repetição em falha de query); (3) contrato TRI-ESTADO — "não sei" (provider em falha) NUNCA decide criação: `Presence::probe` em FsStorage/SafStorage + createNew só cria com ausência CONFIRMADA; (4) mime octet-stream para tudo menos .json (nome verbatim); (5) abertura "wt" (truncate) no doc EXISTENTE — nunca create por cima; (6) CURA dos projetos 0.6.4: ficheiros já renomeados ("x.goni.json") reabrem e continuam a ser usados — as cópias " (1)"/" (2)" são lixo a apagar manualmente; Salvar materializa assets em-runtime: cubo procedural → meshes/cube.obj (formato OBJ já definido, idempotente) e todo write loga `saf: write <rel> — N bytes` (visível no Ver logs); fake do SAF agora MODELA rename+colisão do provider — 279 testes.
- **F5.4 (0.6.4)**: Gestor de Projetos (SAF multi-pasta, takePersistableUriPermission por projeto, SafStorage = ProjectStorage sobre SAF, fallback app-private intacto) + fix do UnsatisfiedLinkError (JNI_OnLoad nunca corria no device — dlopen do NativeActivity não chama; static loadLibrary na VvActivity + registo idempotente pós-handshake + gate jni_parity.py que apanhou um bug latente de assinatura) — 272 testes (docs/HANDSHAKE_AUDIT.md §6).
- **F5.2 (0.6.2)**: All Files Access (diálogo → settings → File API direta) + remoção do SAF tree picker + log viewer in-app + boot self-check com errno — 254 testes.
- **F5.3 (0.6.3)**: handshake Java↔native INVERTIDO (VvActivity regista-se no native — onCreate + onResume; causa única das pontes mortas 0.6.0→0.6.2: GetEnv EDETACHED no thread do glue) + attach de threads nomeado + mensagens honestas ("ponte Java indisponível (handshake)") + gate do manifest binário no CI — 264 testes (docs/HANDSHAKE_AUDIT.md).
- **F5.1 (0.6.0)**: maturação de assets em 4 sub-blocos.
- **F5.1-hotfix (0.6.1)**: crash dump permanente + engine.log com rotação e boot progress por passos + export p/ Downloads/GOneVV/logs (MediaStore) + auditoria/fix da ponte JNI SAF (JNI_OnLoad/RegisterNatives, higiene de exceções, resultado SAF no thread da engine) — 243 testes. **A** — vendors
  etcpak 2.1 (BSD) e astc-encoder 5.3.0 (Apache-2.0), CompressedImage com
  cadeia de mips completa em CPU (blob contíguo), HardwareCompressor (ASTC
  se GL_KHR_texture_compression_astc_ldr, senão ETC2; <256px fica RGBA),
  cache em disco `textures/cache/` (header LE + hash FNV-1a do PNG; hit/miss
  contados; corrupção → miss), gate 4K REAL (com compressão 4K entra inteira
  — ETC2 4K ≈ 8 MB; fallback sem compressão reduz para 2K com aviso),
  `glCompressedTexImage2D` por nível. **B** — parser glTF lê `images`
  (data:image/png;base64 e bufferView do GLB) + `textures` + materiais com
  `baseColorTexture`; extração para `textures/gltf_<hash>.png` com DEDUP por
  hash (N materiais → 1 ficheiro); aplicar um mesh glb no Inspector aplica
  logo a textura. **C** — SAF com exceção documentada à regra zero-Java
  (docs/SAF_EXCEPTION.md): VvActivity (pickers + takePersistableUriPermission
  + JNI), SafIo sobre DocumentsContract (sem androidx), SafStorage com
  backend injetável + RoutingStorage (fallback getExternalFilesDir).
  **D** — menu: Pasta (SAF)/Importar…/Export SAF; status line mostra o
  formato da textura (etc2/eac/astc4/astc6/rgba) e hits/misses do cache;
  round-trip completo testado no CI (glb → extração → ETC2+cache → export
  OBJ → reimport → cena recarregada). 232 testes.
- **F5.0-fix (0.5.1)**: bug do C33 no Inspector (texto sobreposto em pilhas:
  nome×Transform3D, mesh/tex/input/body×sz, tc×velx) — o cursor Y SEMPRE foi
  partilhado e sequencial; a causa raiz eram as linhas de 26/30 px (baseline
  '+8') para uma fonte de 28 px: o bloco de glifos invadia a linha de cima.
  O Inspector agora vem de um PLANO único (EditorLayout.h) com cursor Y
  cumulativo, alturas derivadas das MÉTRICAS REAIS da fonte (ascent/descent
  medidos no bake), baseline centrado, contentHeight = fundo da última linha
  e tap re-despachado POR REGIÃO (a Hierarchy comia o tap do Inspector).
  UI real testada no CI com fonte embutida: 0 colisões glifo-a-glifo (o
  código antigo apanhava 13 com a mesma fonte); 203 testes.
- **F5 (0.5.0)**: projeto `.goni` com pasta (manifesto + scenes/meshes/
  textures, refs relativos, reopen persistente), import OBJ e glTF/GLB
  (hierarquia + materiais básicos), PNG com mipmaps e gate 4K→2K, export
  cena/.goni + OBJ round-trip, ResourceManager com cache (CPU e GPU sem
  duplicação), seletores de assets no Inspector; 196 testes.
- **F4.2 (0.4.2)**: correções do device — safe-area do sistema (scroll do
  Inspector ativa com a nav bar contabilizada), truncagem de labels com
  ellipsis e sandbox do Play (pose de editor restaurada ao sair); 135 testes.
- **F4.1 (0.4.1)**: scroll como primitivo de UI — Inspector e Hierarchy com
  ScrollRegion (clamp, drag-vs-tap, indicador, tap re-despachado); fundo dos
  painéis alcançável no device.
- **F4 (0.4.0)**: física core — 4 formas + sweep/CCD, BodyComp, slide do
  Character, Rigid primitivo, TouchControls (modo Play) e preset RigidBody3D.
- **F3.1 (0.3.1)**: porto do editor — clamps generosos de câmara (zoom 300,
  pitch 89°) e grid de linhas → quad de shader (4 vértices, fade adaptativo,
  anti-moiré); near/far recalibrados.
- **F1 (0.1.0)**: fundação — NativeActivity, EGL/GLES3, core (Handle/Tic/Scene/
  Time), UI immediate-mode, crash log, CI assinado.
- **F2 (0.2.0)**: primeiro 3D — Vertex/Mesh/LitMaterial, cubo procedural,
  grid com fade ("espaço infinito"), câmara de orbit (1 dedo + pinch, clamp),
  depth test, status line com vértices/draw calls.

## CI — fluxo da assinatura (uma vez)
1. Actions → **release** → *Run workflow* → preencha `store_pw` / `key_pw`
   → o job **init-keystore** gera o artifact `vv-release-keystore` (baixe e guarde OFFLINE).
2. `base64 -w0 vv-release.jks` → configure os secrets:
   `VV_KEYSTORE` (base64), `VV_STORE_PW`, `VV_KEY_ALIAS`, `VV_KEY_PW`.
3. Todo push publica o artifact `goni-vv-0.5.1-release-signed` (APK arm64).
   Sem secrets: sai `-UNSIGNED` (nunca debug).

Trocar a keystore muda a assinatura — exige desinstalar/reinstalar no device.

## Build local (opcional)
Android SDK + NDK 26.3 + CMake 3.22.1 + JDK 17 → `./gradlew assembleRelease`.

## Testes do core (Linux)
`cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests`

## Verificação no Realme C33 (dono) — F5.1-hotfix (diagnóstico sem PC)
1. Instalar o APK **0.6.1** → abrir. **Se abrir**: ir a
   `Android/data/vv.goni/files/logs/` (gestor de ficheiros) → `engine.log`
   deve mostrar a sequência completa:
   `[boot 2/6] storage OK` … `[boot 1/6] contentRect OK` …
   `[boot 3/6] fonts OK` … `[boot 4/6] renderer OK` …
   `[boot 6/6] scene OK → editor`.
2. **Export**: toolbar **Settings → "Exportar logs"** → toast "logs
   exportados: N" → abrir **Downloads/GOneVV/logs/** no gestor de ficheiros →
   `engine.log` visível e abrível no telefone.
3. **Se ainda assim crashar**: o mesmo gestor de ficheiros mostra
   `Android/data/vv.goni/files/logs/crash-<data>.dump` (e a marca `CRASH` no
   fim do `engine.log`) — enviar o dump; ele diz o SINAL e a função C++
   exata com offset. Exportar também pelo botão Settings (o dump vai junto).
4. **SAF depois do fix**: Menu → Pasta (SAF) → escolher pasta → a app
   continua viva (o resultado agora é processado no thread certo) → Importar
   um .obj/.glb/.png → Export SAF. Reiniciar → pasta SAF reaberta.
5. **Regressões**: F5.1 (status line `etc2/astc4`, cache `c1/1`, glb com
   textura), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — 0.7.7 (TIC de câmara; APK CUMULATIVO)

Instalar o APK 0.7.7 (artifact `goni-vv-0.7.7-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Criar a câmara**: editor 3D → "+" → "Camera" → o TIC "Camera" entra
   na Hierarchy e o FRUSTUM WIREFRAME azul aparece na cena (corpo +
   lente + cone + retângulo do far + linha central). Numa cena com outra
   câmara ativa, a nova toma o lugar (uma ativa).
2. **Seleção por toque**: tocar no corpo/frustum no viewport seleciona o
   TIC da câmara (Inspector mostra a secção Camera); tocar no vazio
   desseleciona como sempre.
3. **Gizmos**: com a câmara selecionada, mover/rodar atuam na pose (o
   frustum segue); **escalar abre/estreita o frustum (fovY)** — a escala
   do transform NÃO muda; snap salta nos passos.
4. **Handles do far**: arrastar o QUADRADO DO CENTRO do far muda `far`
   (o retângulo afasta-se); arrastar um CANTO muda `fovY` (o cone
   abre/fecha). Perto de um handle o drag é DELE (não rouba o eixo do
   gizmo).
5. **Inspector**: secção Camera com fov/near/far/orthoSizo, "projecao"
   cicla perspetiva↔ortográfica (o far vira retângulo fixo), "ativa"
   sim/nao (uma ativa por cena).
6. **Alinhar à vista**: ⋮ da câmara → "Alinhar a vista" → o frustum fica
   EXATAMENTE na pose da orbit (o que se vê é o que a câmara vê).
7. **Play**: com a câmara ativa, a cena renderiza PELA CÂMARA (o
   joystick move o player em relação a ela) e NENHUM frustum se desenha;
   Stop volta ao editor com a orbit intacta. Sem câmara ativa: o
   fallback é a orbit de sempre.
8. **Regressões**: toolbar 0.7.6 (5 grupos/ícones/G4 condicional),
   paridade editor↔Play, overlays, teclado — intactos.

## Verificação no Realme C33 (dono) — 0.7.6 (histórico; toolbar final + ícones + tela de projetos)

Instalar o APK 0.7.6 (artifact `goni-vv-0.7.6-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Tela de projetos (a app abre nela)**: topo com [Novo projeto] e
   [Importar projeto] (botões de CONTORNO azul #8AB4F8, sem gradiente
   cinza) + cabeçalho "Meus projetos"; a lista mostra SÓ nome + data da
   última edição — NENHUM "primary:" em lado nenhum; sem botão de criação
   no fundo. [Novo projeto] pede o nome → seletor de pasta → entra no
   editor (a linha da lista fica com a data de agora ao voltar).
   [Importar projeto] abre o seletor direto (o nome vem da pasta).
2. **Toolbar do editor**: 5 grupos separados por linhas finas —
   [Menu ▾][Cena ▾] texto · [pause][play] ÍCONES azuis · [3D|UI]
   segmented (o ativo com fundo azul) · [inspector] ícone à direita. SEM
   botão "Settings" (está no dropdown do Menu) e SEM "Mover/Rodar/
   Escalar" em texto.
3. **G4 condicional**: sem TIC selecionado o grupo de transformação NÃO
   existe na barra; selecionar um TIC (Hierarchy ou tap) → os 4 íCONES
   (mover/rodar/escalar/snap) aparecem entre o 3D|UI e o inspector;
   mover/rodar/escalar exclusivos (um ativo), snap liga/desliga sem
   trocar o modo.
4. **Dropdowns do G1**: Menu → Settings/Guardar/Carregar/Export OBJ/
   Importar…/Export Downloads/Sair (Settings abre o menu de logs/
   armazenamento de sempre); Cena → lista de cenas do projeto (a ativa
   marcada) + "+ Nova cena".
5. **G5 inspector**: tocar o ícone → o painel direito desaparece e o
   viewport/gesto cresce para a direita; tocar de novo → volta.
6. **Regressões**: separador 3D|UI continua a trocar o viewport;
   gizmos/teclado/overlays (0.7.4/0.7.5) intactos; em Play a barra
   desaparece (só a play bar com Stop).

## Verificação no Realme C33 (dono) — 0.7.5 (histórico; overlays + TIC de UI + minúsculas)

Instalar o APK 0.7.5 (artifact `goni-vv-0.7.5-release-signed` do run do
fecho). Roteiro cumulativo — os anteriores continuam a aplicar-se:

1. **Overlays modais**: no modo UI com elementos no canvas ("TESTE",
   "Botao"), abrir o **MENU** → o ecrã fica TODO tapado pelo fundo
   escuro e SÓ o menu aparece (nenhum texto do canvas à mista); o mesmo
   com o menu contextual (⋮), o teclado, CENAS e o navegador; toque fora
   fecha e o editor volta INTEIRO (toolbar/painéis/canvas);
2. **TIC de UI próprio**: SEM nada selecionado, modo UI → "+" → Label →
   nasce o TIC **"UI"** na Hierarchy (só com UiCanvas — sem mesh/body) e
   o elemento dentro dele (toast "TIC 'UI' criado + elemento"); criar um
   segundo elemento REUTILIZA o mesmo TIC; selecionar o Player e criar →
   continua a anexar ao Player (o comportamento de sempre);
3. **Minúsculas**: renomear (⋮ → Renomear) → teclado → tocar **abc** →
   as letras escrevem em minúsculas ("cena2", "ola mundo"); **ABC** volta
   às maiúsculas; a tecla **Z** existe e escreve (o fix da tecla fantasma);
   dígitos/'-'/'_'/' '/APAGA/OK/X como sempre;
4. **Regressões**: paridade editor↔Play (0.7.4), texturas, containers,
   joystick, cenas, navegador, gestão de TICs — tudo como antes.

## Verificação no Realme C33 (dono) — 0.7.4 (paridade + texturas + containers; APK CUMULATIVO)

Instalar o APK 0.7.4 (artifact `goni-vv-0.7.4-release-signed` do run do
fecho). Roteiro cumulativo — os anteriores continuam a aplicar-se:

1. **Paridade editor↔Play** (o caso do Menu): criar um Menu com 2-3 itens
   no modo UI → **Play** → o Menu é EXATAMENTE o que o viewport 2D
   mostrava (caixas, texto à mesma proporção, mesmos espaçamentos);
   alternar editor↔Play em Panel/Label/Button/Image/Menu/Card/Article e
   conferir que NADA muda de aparência (só a interação difere);
2. **Label só texto**: criar um Label → SEM fundo (só o texto); Inspector
   → "fundo A" a 1 → aparece o fundo (cor R/G/B editável); o Button
   continua a nascer com o fundo escuro;
3. **Texturas de fundo**: selecionar um Panel → "tex:" → escolher uma
   textura de `textures/` (ou "importar..." → navegador → escolher um PNG
   da galeria → volta ao seletor) → o painel fica com a IMAGEM de fundo;
   o mesmo num Button (com o texto por cima) e num Image; "tex: none"
   limpa;
4. **Z-order**: painel COM textura + botão (sem textura) POR CIMA → o
   botão continua VISÍVEL sobre a textura (o caso que desenhava mal);
5. **Menu configurável**: selecionar um Menu → "espaco" → os itens afastam-
   se; "fundo A" a 0 → as caixas somem (só texto); "alinhamento" cicla
   start/center/end do texto das linhas;
6. **Containers**: "+" → VBox → com ele selecionado, "+" → Button → nasce
   DENTRO (empilhado; sem posição manual); + Label → em baixo; "espaco"/
   "pad"/"alinhamento" do VBox mexem nos filhos; VBox cresce sozinho
   (auto-fit); HBox põe lado a lado; VBox dentro de HBox aninha;
   arrastar um filho PARA FORA tira-o do container (fica onde largou);
   "colocar em:" no Inspector também move;
7. **Persistência**: Save → Load preserva texturas/alpha/spacing/pad/
   alinhamento/filhos; um `.goni` 0.7.x abre como sempre;
8. **Regressões**: joystick editável/compostos/navegador/cenas/UI criável/
   gestão de TICs/gizmos — tudo como antes.

## Verificação no Realme C33 (dono) — 0.7.3 (joystick editável + compostos; APK CUMULATIVO — FECHO DA CAMPANHA 0.7)

Instalar o APK 0.7.3 (artifact `goni-vv-0.7.3-release-signed` do run do
fecho). Roteiro cumulativo — os anteriores continuam a aplicar-se:

1. **Joystick no modo UI**: selecionar o Player (com TouchControls) →
   modo UI → o joystick aparece como um QUADRO com knob no viewport 2D
   (mesmo sem elementos de UI);
2. **Editar**: arrastar o joystick move-o; selecioná-lo abre o
   INSPECTOR DO JOYSTICK (pos X/Y, tamanho, sensib., cor R/G/B, remover);
   mexer no tamanho cresce/encolhe o quadro; a cor tinge a base/knob;
3. **O edit afeta o Play**: Play → o joystick está ONDE se pôs, do
   TAMANHO escolhido; mover o dedo 1/4 do raio com sensib. 2 anda o
   DOBRO (o eixo escala); a física responde;
4. **Adicionar**: "+" no modo UI → Joystick → o TIC ganha o componente
   (e fica selecionado); "remover joystick" tira-o (add TouchControls do
   Inspector de 3D continua a funcionar);
5. **Persistência**: Save → Load preserva pos/tamanho/sens/cor; um
   projeto 0.6.x abre com o joystick no sítio de sempre;
6. **Compostos**: "+" → Menu (lista vertical; "Jogar>cena2" por linha
   dispara a ação com o alvo da linha) / Card (moldura + título) /
   Article (texto comprido faz wrap dentro da largura);
7. **Regressões**: navegador/cenas/transições/UI criável/gestão de
   TICs/gizmos — tudo como antes (o APK é cumulativo e fecha a campanha).

## Verificação no Realme C33 (dono) — 0.7.2 (import robusto; APK CUMULATIVO)

Instalar o APK 0.7.2 (artifact `goni-vv-0.7.2-release-signed` do run do
fecho). Roteiro cumulativo — o da 0.7.1 continua a aplicar-se:

1. **Navegador**: Menu → Importar… → abre o NAVEGADOR na Download (com o
   all-files concedido); o CAMINHO atual aparece no topo (barra escura);
2. **Galeria**: tocar "Camera" → as fotos de DCIM/Camera aparecem
   ("tex: IMG_….jpg" não — só .png/.obj/.gltf/.glb; fotos da câmara em
   .png aparecem); "Pictures" idem; "Raiz" despeja tudo o que há;
3. **Subir/descer**: tocar uma pasta entra ("^ Subir" volta); na raiz o
   Subir fica na raiz;
4. **Pasta vazia**: entrar numa pasta sem ficheiros suportados → a
   mensagem "(vazio) <caminho>" aparece DENTRO do overlay (nunca um toast
   que não diz onde);
5. **Importar + aplicar**: selecionar um TIC com mesh → Importar… →
   navegar até um .png → tocar → pergunta "Aplicar 'x.png' ao TIC?" →
   Sim → a textura liga (Inspector "tex: x.png"; o cubo mostra a imagem);
   Nao → fica só importado (aplicável depois no seletor tex:);
6. **Sem seleção**: importar sem TIC selecionado → só o toast
   "importado: textures/x.png" (sem pergunta);
7. **Permissão**: sem all-files → o diálogo de armazenamento (o fluxo da
   0.6.10 continua); "Raiz" numa pasta sem acesso → "(sem acesso)
   <caminho>" + errno no engine.log;
8. **Regressões**: cenas/transições (0.7.1), UI criável/gestão de TICs
   (0.7.0), seletor de textura do Inspector, export, logs.

## Verificação no Realme C33 (dono) — 0.7.1 (cenas múltiplas + transições; APK CUMULATIVO)

Instalar o APK 0.7.1 (artifact `goni-vv-0.7.1-release-signed` do run do
fecho). Roteiro cumulativo — o da 0.7.0 continua a aplicar-se:

1. **Lista de cenas**: Menu → "Cenas…" → o overlay lista "main" (ou a cena
   do projeto) com ">" na ativa;
2. **Nova cena**: "+ Nova cena" → digitar "nivel2" (teclado in-app) → OK →
   cena vazia ativa (toast "cena criada"); criar um cubo e voltar a
   "Cenas…" → tocar na cena antiga → o cubo ANTERIOR volta (nada se perdeu
   na troca); repetir o nome → toast "cena ja existe";
3. **Persistir**: Sair para projetos → reabrir → a cena ativa é a mesma
   (manifesto);
4. **Transição fade em Play**: numa cena com UI, criar um Button com
   acao "trans", alvo "nivel2", estilo "fade" → Play → tocar o botão → o
   ecrã escurece até preto, a cena TROCA ao escuro e volta a clarear
   (nunca se vê a troca a seco);
5. **Transição slide**: mudar o estilo para "slide" (Inspector UI) →
   repetir → o quadro preto cobre pela esquerda e sai pela direita;
6. **Scene.Load**: acao "scene" → Play → tocar → troca INSTANTÂNEA (sem
   transição) — os dois comportamentos convivem;
7. **Cena inexistente**: alvo "xyz" → Play → tocar → toast
   "cena 'xyz' nao existe";
8. **Regressões**: UI criável/gestão de TICs (0.7.0), gizmos, seletor de
   textura, import, logs — tudo como antes (o APK é cumulativo).

## Verificação no Realme C33 (dono) — 0.7.0 (UI criável + gestão de TICs; APK CUMULATIVO)

Instalar o APK 0.7.0 (artifact `goni-vv-0.7.0-release-signed` do run do
fecho). Roteiro cumulativo — os anteriores continuam a aplicar-se:

1. **Separador 3D | UI**: com um TIC selecionado, tocar "UI" na toolbar →
   o viewport central fica um ecrã 2D escuro com moldura (sem cena 3D);
   voltar a "3D" devolve a cena com os gizmos;
2. **Criar UI**: no modo UI, "+" → Button → o botão aparece CENTRADO e
   selecionado no viewport 2D e o painel direito mostra o INSPECTOR UI
   (x/y/lar/alt/cor R-G-B/visivel/ancoras/texto/acao/remover);
3. **WYSIWYG**: arrastar o botão no viewport 2D move-o; tocar num sítio
   vazio desseleciona; tocar de novo seleciona;
4. **Ancoragem**: no Inspector UI, "ancora H" até "direita" → o elemento
   NÃO salta de posição; girar o ecrã/reabrir continua colado à direita;
5. **Ação declarativa**: criar um Panel + um Button; no Button: acao =
   "toggle", alvo = nome do Panel (via teclado in-app — tocar "texto"/
   "alvo" abre o TECLADO; nada de IME do sistema); Play → tocar o botão
   alterna o Panel (a UI fica POR CIMA da cena);
6. **Gestão de TICs**: "..." numa linha da Hierarchy → Renomear (teclado
   in-app; OK aplica, Cancelar não mexe) / Remover (pede confirmação) /
   Duplicar (cópia com ".001") / Visibilidade; o OLHO "O/X" na linha
   alterna na hora; TIC invisível desaparece do editor E do Play (a
   física continua — um chão invisível ainda segura);
7. **Desselecionar**: tocar no vazio do viewport 3D ou abaixo da última
   linha da Hierarchy limpa a seleção (arrastar continua a orbitar);
8. **Cor por TIC**: TIC com mesh → sliders "cor R/G/B" no Inspector → o
   cubo tinge-se no frame seguinte; Save → Load preserva (e `none`/branco
   = o cinzento de sempre);
9. **Teclado in-app**: renomear um TIC → digitar → APAGA → OK; as teclas
   nunca se sobrepõem e o diálogo vive na safe-area;
10. **Regressões**: gizmos/play mode/seletores de textura/import/logs —
    tudo como na 0.6.10 (o APK é cumulativo).

## Verificação no Realme C33 (dono) — 0.6.10 (seletor de textura; APK CUMULATIVO)

Instalar o APK 0.6.10 (artifact `goni-vv-0.6.10-release-signed` do run
36748264165; sha256 e detalhes no `docs/RELATORIO-F6.md`) — cobre TAMBÉM a
0.6.9 (gizmos), 0.6.8 (play mode) e 0.6.7 (lifecycle/apagar/sair).

1. **Importar** um PNG (Menu → Importar… → escolher o screenshot de
   Download) → toast de import OK → `textures/screenshot-….png` no projeto
   (status line `at` sobe);
2. **Aplicar**: selecionar o TIC (cubo) → tocar na linha **`tex:`** do
   Inspector → o seletor TEXTURA abre com o screenshot na lista → tocar a
   imagem → **o cubo mostra a textura** e o Inspector passa a
   **`tex: screenshot-…`** (o estado agora muda de verdade);
3. **Log viewer**: Settings → Ver logs → procurar a linha
   `material: textura aplicada textures/screenshot-….png`;
4. **Persistência**: Menu → **Save** → **Load** (ou sair para projetos e
   reentrar) → o cubo continua com a textura aplicada (a ref está no
   `.goni` e é re-resolvida no load);
5. **Remover**: tocar `tex:` de novo → **none** → o Inspector volta a
   `tex: none` e o cubo ao cinzento → log
   `material: textura removida`;
6. **Mesh (mesmo fix)**: tocar `mesh:` → escolher um ficheiro de `meshes/`
   → o mesh muda de verdade (toast + linha `editor: mesh … aplicado`);
7. **Regressões**: 0.6.9 (gizmos), 0.6.8 (play mode), 0.6.7 (lifecycle GL,
   apagar/sair), F5.5 (All Files Access — import/export continuam OK).

## Verificação no Realme C33 (dono) — F5.5 (All Files Access de verdade)

APK: **0.6.9 + F5.5** (versão mantida — mesmo versionCode 18; distinguível
pelo sha256 no `docs/RELATORIO-F5.5.md` e pela linha do log viewer abaixo).

1. **Lista do sistema**: Definições do Android → Privacidade →
   **"Acesso a todos os ficheiros"** (ou "Permissão de todos os ficheiros")
   → a **G.One VV AGORA APARECE** na lista (antes só o Godot aparecia).
2. **Fluxo pelo editor**: abrir um projeto → Menu → **Importar…** → diálogo
   "Precisa de acesso a todos os ficheiros…" → **Permitir** → abre a página
   DA APP nas definições → **ativar o interruptor** → voltar → toast "acesso
   concedido — File API direta" e a lista de ficheiros de **Download/Documents
   abre SEM re-pedir** (import retomado).
3. **Export**: com um TIC com mesh → Menu → **Export Downloads** → toast
   "exportado: Download/GOneVV/export/…" → confirmar no gestor de ficheiros
   em `Download/GOneVV/export/`.
4. **Log viewer**: Settings → **"Ver logs"** → procurar
   `storage: all-files granted=1` (depois de conceder; `granted=0` antes).
   A linha aparece no boot, no retorno das definições e na re-verificação
   do resume — cada transição logada.
5. **Recusar (fallback)**: (não conceder / desativar o interruptor) →
   tentar importar → recusa no diálogo ou ao voltar → toast claro
   "acesso não ativado — modo app-private" — SEM repetições/loop; o
   projeto (SAF) continua a funcionar completo.
6. **Regressões**: 0.6.9 (gizmos), 0.6.8 (play mode), 0.6.7 (lifecycle GL,
   apagar projeto, sair para projetos), F5.4 (gestor multi-pasta).

## Verificação no Realme C33 (dono) — 0.6.9 (gizmos; APK CUMULATIVO)

Instalar o APK 0.6.9 (artifact `goni-vv-0.6.9-release-signed`) — cobre
TAMBÉM a 0.6.7 (lifecycle/apagar/sair) e a 0.6.8 (play mode).

1. Abrir um projeto → criar/selecionar um TIC → o GIZMO aparece no TIC
   (modo Mover por omissão): 3 setas (X vermelho, Y verde, Z azul) + 3
   quadradinhos de plano;
2. **Mover:** tocar na seta X (destaca-se a branco) e arrastar → o TIC
   mexe SÓ em X; arrastar um quadradinho de plano → move nos 2 eixos do
   plano; a câmara NÃO orbita durante o drag (arrastar fora do gizmo
   orbita normalmente);
3. **Rodar** (botão Rodar na toolbar): 3 anéis; arrastar o anel → roda no
   eixo do anel; com Snap, saltos de 15°;
4. **Escalar** (botão Escalar): 3 handles + quadrado central; o central
   escala uniforme; com Snap, passos de 0.25; nunca desaparece (clamp);
5. **Snap:** ligar/desligar na toolbar — mover salta ao grid de 0.5;
6. **Inspector coerente:** depois de um drag, os sliders do Transform3D
   mostram a pose nova (pos/rot/scale);
7. **PLAY:** tocar Play → o gizmo DESAPARECE (nada de edição em play);
   Stop → o gizmo volta na pose restaurada;
8. Regressões 0.6.8 (play bar/touchcontrols/orbit) e 0.6.7 (lifecycle
   home/voltar; sair para projetos; apagar projeto; nome visível).

## Verificação no Realme C33 (dono) — 0.6.8 (play mode)

1. Instalar o APK 0.6.8 (artifact `goni-vv-0.6.8-release-signed`).
2. Entrar num projeto com um TIC PlayerBody3D + TouchControls (add
   TouchControls no Inspector) e mover a câmara para um ângulo reconhecível.
3. **Play**: tocar no botão Play da toolbar → esperado: a toolbar e os
   painéis SUMIR; fica o viewport fullscreen + a barra PLAY no topo com
   "Stop", "a correr · fps N" e o aviso "simulação — alterações
   descartadas ao parar"; o joystick à esquerda-baixo e o JUMP à
   direita-baixo, SEM sobrepor nada (nem a barra, nem a status line).
4. **Orbit desativado**: em play, arrastar 1 dedo no viewport (fora dos
   controlos) NÃO mexe a câmara; pinch também não.
5. Simular (andar com o stick/saltar) → tocar **Stop** → esperado: volta ao
   EDITOR com a pose ANTERIOR ao play (não a pós-simulação), painéis e
   seleção exatamente como antes, orbit a funcionar de novo.
6. Regressões: Save/Load, gizmos ainda não existem (0.6.9), lifecycle
   0.6.7 (home → voltar → texto normal).

## Verificação no Realme C33 (dono) — 0.6.7 (lifecycle GL + gestão de projetos)

Instalar o APK 0.6.7 (artifact `goni-vv-0.6.7-release-signed` do CI). O
roteiro cobre os três fixes; o log viewer in-app (Settings → Ver logs)
mostra as linhas `lifecycle:` se algo falhar.

**A — lifecycle GL (cubinhos):**
1. Abrir um projeto no gestor → criar/mover um TIC → **botão home** (ou
   recents) → voltar à app SEM a matar;
2. Esperado: o texto (toolbar, Hierarchy, Inspector, status line) renderiza
   NORMAL — sem quads brancos;
3. Settings → Ver logs: procurar `lifecycle: INIT_WINDOW #2 — contexto
   EGL RE-CRIADO` e `[boot 3/6] fonts OK … RE-UPLOAD no contexto novo`;
   e no sair: `lifecycle: TERM_WINDOW #1 — … NENHUM recurso GL assumido
   vivo`.

**B — apagar projeto (com confirmação):**
1. No gestor, criar um projeto de teste (ex.: "lixo") com uma pasta à
   escolha → entrar nele → criar um TIC → Menu → Sair para projetos;
2. Long-press na entrada "lixo" → "Apagar projeto" → confirmação
   ("Não pode ser desfeito") → "Apagar";
3. Esperado: Toast "projeto apagado", a entrada SAI da lista e a pasta
   escolhida desaparece do gestor de ficheiros do sistema;
4. Repetir com "Remover da lista" noutro projeto: a pasta PERMANECE
   (comportamento antigo intacto).

**C — Sair para projetos (sem matar a app):**
1. No editor: mover/rodar um TIC → Menu → "Sair para projetos";
2. Esperado: volta ao GESTOR (a app não fecha/reinicia — sem splash), a
   cena foi auto-salva (toast "cena salva — a sair…");
3. Reentrar no MESMO projeto: cena carregada com o TIC na pose deixada,
   texto normal, física a 1× velocidade (play não acelerado);
4. Ver logs: `editor: sair p/ projetos — auto-save OK` e `lifecycle:
   REENTRADA do android_main`.

**D — nome do projeto visível:**
1. Gestor → "+ Novo projeto" → digitar: o texto digitado TEM de estar
   visível (campo escuro, texto claro, cursor a piscar).

## Verificação no Realme C33 E RMX3624 (dono) — F5.4-hotfix (0.6.5)
> A regressão do handshake APARECEU no RMX3624 (Android 13) — a fase só
> fecha VERIFIED depois de passar nos DOIS devices.

1. Instalar o APK **0.6.5** (por cima da 0.6.4 SEM apagar os dados — os
   projetos da lista continuam lá).
2. **ANTI-DUPLICAÇÃO (o bug desta fase)**: abrir um projeto criado pela
   0.6.4 (ou criar um novo) → adicionar/editar algo → Salvar → fechar a
   app (swipe) → abrir de novo → Salvar outra vez → repetir 2–3×. NO
   gestor de ficheiros, a pasta do projeto tem de continuar com EXATAMENTE
   um `project.goni`, um `scenes/main.goni` — NENHUM ` (1)`/` (2)`. O
   projecto 0.6.4 que tinha `project.goni.json`/`main.goni.json` continua
   a abrir (cura) SEM criar ficheiros novos; as cópias ` (1)`/` (2)` antigas
   são lixo inofensivo — podem ser apagadas à mão.
3. **Salvar materializa assets**: criar um TIC (preset Cube) → Menu →
   Salvar → Settings → Ver logs: tem de aparecer
   `saf: write meshes/cube.obj — N bytes` (e `file: write …` no modo
   app-private). No gestor de ficheiros: `meshes/cube.obj` existe com
   conteúdo (OBJ de texto — abre em qualquer editor). Salvar de novo NÃO
   reescreve (idempotente).
4. **Ecrã inicial = "Projetos"** (NÃO o editor). **Criar projeto**: "+
   Novo projeto" → nome → seletor de pastas (SAF) → Documents/JogoA →
   editor. No log viewer: `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` → `jni: handshake OK — … saf=5/5` →
   `java: openProject → fila` → `projeto: '<nome>' pronto (SAF)`.
5. **2º projeto em pasta DIFERENTE** (Documents/JogoB) → ambos abrem
   independentemente (nada se mistura).
6. **Persistência**: fechar a app (swipe) → abrir de novo → a lista
   continua; tocar num projeto abre-o com as cenas gravadas — e ao Salvar
   NÃO crescem ficheiros na pasta (ponto 2).
7. **Sem All Files Access**: os projetos funcionam COMPLETOS (save/load/
   scene) SEM conceder All Files Access.
8. **All Files Access coexiste**: num projeto aberto, Menu → Importar… →
   import de Download/Documents para meshes//textures/ (subpasta certa);
   export para Download/GOneVV/export (igual 0.6.3).
9. **Mensagens honestas**: se algo falhar por ponte, o toast/log diz
   **"ponte Java indisponível (handshake)"** — nunca "sistema sem suporte".
10. **Sem UnsatisfiedLinkError**: no engine.log a linha
    `jni: JNI_OnLoad — G.One VV 0.6.5 … registado(s)` aparece ANTES do
    `java: onCreate → nativeRegisterActivity`.
11. **Regressões**: F5.4 (gestor, multi-pasta), F5.3 (import/export All
    Files), F5.2 (log viewer, modo no Settings), F5.1 (status line,
    cache), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.3 (histórico)
1. Instalar o APK **0.6.3** → confirmar "0.6.3" nas infos.
2. **Handshake no log viewer** (SEM PC): abrir a app → Settings → **"Ver
   logs"** → a sequência completa tem de aparecer:
   `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` →
   `jni: handshake OK — vm=… openAllFiles=1 exportLogs=1` →
   `storage: All Files Access — handshake=1 supported=1 manager=? …`
   (`manager=1` se a permissão já estava concedida; `0` na 1ª instalação).
3. **Import**: Menu → **Importar…** → o diálogo "Precisa de acesso a todos
   os ficheiros…" aparece → **Permitir** → abre a janela de permissões DO
   app ("All files access") → ativar o interruptor → voltar → toast "acesso
   concedido — File API direta" e a lista de Download/Documents abre (import
   retomado). 4. **Export**: com um TIC com mesh → Menu → **Export
   Downloads** → toast "exportado: Download/GOneVV/export/…" e o OBJ no
   gestor de ficheiros.
5. **Mensagem honesta (se algo falhar)**: se a ponte Java estiver em baixo,
   o toast/log diz **"ponte Java indisponível (handshake)"** — NUNCA
   "sistema sem All Files Access" (a causa real). O C33 (Android 12) SUPORTA
   All Files Access — se viu essa mensagem no 0.6.2, era a mentira antiga.
6. **Regressões**: F5.2 (diálogo/perm/fallback/log viewer), F5.1 (status
   line `etc2/astc4`, cache `c1/1`), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.2 (All Files Access + log viewer)
1. Instalar o APK **0.6.2** → confirmar "0.6.2" nas infos.
2. **Diálogo**: Menu → **Importar…** → aparece o overlay "ARMAZENAMENTO —
   Precisa de acesso a todos os ficheiros para importar/exportar projetos"
   com **Permitir / Cancelar** (na 1ª tentativa de import/export).
3. **Settings do sistema**: tocar **Permitir** → abre a janela de permissões
   DO G.One VV ("All files access"); ativar o interruptor → voltar à app →
   toast "acesso concedido — File API direta" e a lista de ficheiros de
   Download/Documents abre em "IMPORTAR" (o import da ação pendente é
   RETOMADO automaticamente).
4. **Import**: tocar num .obj/.glb/.png da lista → toast "importado:
   meshes/…" (ou textures/) e o ficheiro aparece nos seletores do Inspector.
5. **Export**: selecionar um TIC com mesh → Menu → **Export Downloads** →
   toast "exportado: Download/GOneVV/export/export_…" → abrir
   **Download/GOneVV/export/** no gestor de ficheiros → o OBJ está lá.
6. **Log viewer**: Settings → **"Ver logs"** → o viewer mostra o tail do
   engine.log com scroll (abre no FIM) + os crash dumps; **sem export**.
   A linha "self-check: … getExternalFilesDir=… fopen(…) OK" aparece no
   arranque do log; se algo falhar, a linha diz `errno=N (causa)`.
7. **Modo no Settings**: Settings mostra "armazenamento: all files" (após
   conceder) ou "app-private" (se recusar); **"Acesso a ficheiros…"** abre a
   janela de permissões a qualquer momento.
8. **Fallback**: recusar no diálogo → toast "sem acesso — modo app-private";
   os fluxos Save/Load/Export OBJ do projeto continuam a funcionar
   (app-private não precisa de permissões).
9. **Regressões**: F5.1 (status line `etc2/astc4`, cache `c1/1`, glb com
   textura embutida), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.1 (assets maduros)
1. Instalar o APK 0.6.1 → confirmar "0.6.1" nas infos.
2. **Compressão**: importar um PNG 2K/4K (Menu → Importar…) → aplicar como
   textura → a status line mostra `etc2` (ou `astc4`/`astc6` no Mali do C33)
   no fim da linha; logcat `GpuAssets: textura … 4096x4096 ETC2 RGB via
   compress` (sem "reduzida").
3. **Cache**: aplicar a MESMA textura outra vez (ou reiniciar a app e
   aplicar) → status line mostra `c1/1` (1 hit, 1 miss) e o logcat diz
   `via cache`. Alterar o PNG e importar de novo → nova entrada (hash novo).
4. **glTF/GLB com textura embutida**: importar um .glb com textura interna →
   aplicar no TIC → toast "mesh aplicado (+textura)" e o modelo aparece
   texturizado; a pasta do projeto passa a ter `textures/gltf_<hash>.png`.
5. **SAF**: Menu → **Pasta (SAF)** → escolher/criar uma pasta no armazenamento
   → toast "pasta do projeto ativa"; Menu → **Importar…** → escolher um
   .obj/.glb/.png → toast "importado: …" e aparece nos seletores; Menu →
   **Export SAF** → gravar o OBJ noutro sítio (Downloads, p.ex.) → toast
   "exportado: …". Fechar a app e reabrir → a pasta SAF volta a ser a raiz
   do projeto (URI persistida).
6. **Regressões F5/F4.2**: cena Save/Load, gate 2K sem compressão (toast de
   aviso — agora só no fallback), Play snapshot, scroll do Inspector.

## Verificação no Realme C33 (dono) — F5.0-fix (Inspector sem sobreposição)
1. Instalar o APK 0.5.1 → confirmar "0.5.1" nas infos da app.
2. **Sem sobreposição**: selecionar o PlayerBody3D (com TouchControls e um
   asset importado) → no Inspector o nome, `Transform3D`, os 9 sliders,
   `mesh:`, `tex:`, `input:`, `body:`, `velx` e `tc:` estão cada um na SUA
   linha — nada desenhado em cima de outra linha.
3. **Scroll por cima**: arrastar para cima no painel → o conteúdo desce
   inteiro (indicador à direita); o ÚLTIMO campo (tc) chega ao fundo sem
   corte e sem saltar.
4. **Sliders/taps com scroll**: arrastar um slider horizontalmente → muda o
   valor e NÃO faz scroll; tap em `mesh:` / `tex:` abre o seletor; tap em
   `add TouchControls` cria o componente.
5. Regressão F5: repetir a verificação da F5 abaixo (projeto, import, gate
   4K, export, cache) — nada mudou nesses fluxos.

## Verificação no Realme C33 (dono) — F5 (projeto + assets)
1. Instalar o APK 0.5.1 → confirmar "0.5.1" nas infos da app.
2. **Projeto**: primeiro arranque cria a estrutura
   (`adb shell ls /sdcard/Android/data/<pkg>/files/` → `project.goni`,
   `scenes/`, `meshes/`, `textures/`); criar TICs → Menu → Save cena →
   fechar a app → reabrir → os TICs VOLVEM (cena ativa do projeto).
3. **Import**: `adb push esfera.obj /sdcard/Android/data/<pkg>/files/meshes/`
   e `adb push madeira.png .../textures/` → abrir a app → selecionar um TIC →
   no Inspector tocar em `mesh: cube` → escolher `esfera.obj` no seletor →
   o mesh importa e renderiza num TIC; tocar em `tex: none` → escolher
   `madeira.png` → a textura aplica no material.
4. **Gate 4K**: empurrar uma textura 4096×4096 → ao aplicar aparece o toast
   "textura 4096x4096 reduzida para 2048x2048 (gate 2K; compressao real =
   F5.1)" (1× por carga).
5. **Export**: selecionar o TIC com asset → Menu → Export OBJ → toast
   `export: meshes/export_<nome>.obj`; `adb shell ls .../meshes/` confirma.
6. **Cache de GPU**: dois TICs com o MESMO asset → status line mostra
   `am 1` (um objeto de GL, não dois).
7. **Regressões**: safe-area/scroll/labels/sandbox da F4.2 continuam
   funcionando; física continua só no Play; tema mono, 3 botões, landscape.

## Verificação no Realme C33 (dono) — F4.2 (safe-area + labels + sandbox)
1. Instalar o APK 0.4.2 → confirmar "0.4.2" nas infos da app.
2. **B1/scroll**: selecionar o PlayerBody3D → com a nav bar visível, o
   Inspector agora DETETA o overflow (indicador fino à direita) → arrastar
   para cima revela `body: …`, `velx` e o botão **add TouchControls** no
   fundo, SEM nada tapado pela nav bar; toolbar, status line e painéis todos
   dentro da área visível. (Opcional: `adb logcat | grep safearea` mostra a
   superfície, o contentRect e os insets detetados.)
3. **B2/labels**: com um Rigid no chão, a linha `body: rigid - sphere - chao:
   sim` aparece inteira OU termina em `...` — nunca cortada a meio; nomes
   longos de TIC na Hierarchy terminam em `...` dentro do botão.
4. **B3/sandbox**: dar **Play** → deixar o corpo cair/mover (stick + JUMP) →
   sair do Play → os TICs voltam EXATAMENTE à pose de editor (o corpo
   "desce" de volta ao sítio original); entrar/sair repetidas vezes mantém
   a pose estável.
5. **Regressões**: orbit/pinch no viewport central intactos; gestos atrás da
   nav bar não orbitam a câmara; scroll dos painéis continua com limites no
   topo/fundo; slider continua com prioridade sobre o scroll.

## Verificação no Realme C33 (dono) — F4.1 (scroll)
1. Instalar o APK 0.4.1 → confirmar "0.4.1" nas infos da app.
2. **Inspector**: selecionar o PlayerBody3D → arrastar PARA CIMA na lista →
   o conteúdo desce e revela `body: …`, `velx` e o botão **add TouchControls**
   no fundo → tocar no botão (clicável após o scroll) → aparece "tc: stick +
   jump"; o indicador fino aparece à direita só quando há overflow.
3. **Hierarchy**: criar 12+ TICs (repetir "+") → arrastar a lista → TODAS as
   linhas alcançáveis; tap numa linha que estava cortada seleciona (frame
   branco + Inspector mostra o TIC).
4. **Gestos sem conflito**: arrastar na pista de um slider (px/py/velx) muda
   o valor e NÃO faz scroll; arrastar fora da pista faz scroll; o viewport
   central continua a orbitar/pinçar como sempre.
5. **Limites**: no topo e no fundo o scroll para (não passa do fim); soltar
   sem mover = tap (não scrolla).

## Verificação no Realme C33 (dono) — F4
1. Instalar o APK 0.4.0 → confirmar "0.4.0" nas infos da app.
2. **Montar a arena** (modo editor):
   - Chão: **+** → StaticBody3D → Inspector: `py −0.5`, `sx 5`, `sy 1`, `sz 5`
     (OBB 5×1×5 com topo em y=0).
   - Parede fina rotacionada: **+** → StaticBody3D → `px 1.85`, `py 0.5`,
     `sx 4`, `sy 1`, `sz 0.2`, `ry 30`.
   - Player: **+** → PlayerBody3D (nasce em (0, 0.55, 0)).
3. **TouchControls**: com o player selecionado → Inspector → botão
   **add TouchControls** → tocar **Play** (toast "modo play") → joystick à
   esquerda empurra o player; botão **JUMP** salta quando `grounded`;
   empurrar contra a parede → **colide e desliza**, nunca atravessa.
4. **CCD**: em modo editor, selecionar o player → `velx 40` → **Play** →
   o player é lançado contra a parede fina e **não tunela** (para/encosta).
5. **Rigid**: **+** → RigidBody3D → `py 3` → **Play** → cai e **para no chão**;
   `velx 8` → desliza e abranda; empurrar o player contra a bola → bloqueia
   (empurrão = `velx` no Inspector). Dois Rigid empilhados intersectam
   (PLACEHOLDER de solver aceite).
6. **Modo editor**: sem Play não há painel de controlos nem física; orbit
   (1 dedo) e pinch continuam; gestos nos controlos não giram a câmara.
7. Menu → **Save/Load cena** persiste os corpos (BodyComp round-trip).
8. Status line: fps/tics/verts/dc; crash log em
   `/data/user/0/vv.goni/files/goni_crash.log`.

## Verificação F3.1 (câmara + grid)
- Pinch afasta até 300 sem a borda do grid aparecer; aproxima até ~1; orbit
  até ~89° sem inversão; sem z-fighting; grid custa 4 vértices (status line).
