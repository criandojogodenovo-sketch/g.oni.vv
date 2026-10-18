# 0.11-W 3.75b — IMPOSTORES (o PNG alfa por distância: o dc do TIC longe é 1)

> versionCode 76 · 0.9.6 · a 3.ª e última sub-fase da campanha 0.11-W
> (render systems; a abertura: docs/RELATORIO-0.11-W-ABERTURA.md) · 8 pins
> novos (tests/test_fase375b_impostors.cpp) + a sentinela R-049
> (regress_impostors) + a DÍVIDA DOCUMENTAL DA R-047 fechada (a entrada
> completa no REGRESSOES.md — o registo tinha ficado para trás desde a
> 3.5b) + as 3 mutações coladas · c33 FASE 30 (+28 checks, 925→953) ·
> test_core verde (o flake r039 POR DISCO da sandbox documentado no §4) ·
> gates locais verdes.

## §1 O que foi feito

### O diagnóstico (o que sobra depois do instancing)

O 3.5b fundiu os clones: 6 TICs com o MESMO .gmesh desenham ~863 dc em vez
de ~5164. Mas o custo ainda cresce com os BLOCOS de cada TIC — num mundo
com N prédios (a cidade do SAF, os 6 modelos do dono espalhados), o dc é
N × blocos e o GE8320 volta a afogar. O impostor é a 3.ª alavanca: o TIC
ALÉM da distância dele desenha UM quad com o PNG alfa da silhueta — o dc
do TIC longe é 1, SEMPRE, independentemente dos blocos.

### A cura: a decisão + o billboard + o PNG honesto

- **`render/Impostor.h`** (NOVO, PURO — GL-free, afervel no CI):
  - `active(model, mn, mx, camPos, impostorDistance)` — a DECISÃO:
    `impostorDistance <= 0` = OFF (o byte a byte do pré-3.75b); o limiar é
    EXCLUSIVO no lado longe (no próprio limiar ainda é o normal — o MESMO
    contrato do `lodActive` da 3.5a). `distOf`/`centerOf` usam O MESMO
    centro do AABB que o `blockDist` do drawTics (uma fonte de verdade);
  - `billboard(...)` — a MATRIZ: col3 = o centro transformado, col2 = a
    normal PARA A CÂMARA (o front CCW que o cull espera — o pin apanhou o
    verso), o lado = a MAIOR extensão do AABB escalada pelas colunas do
    model (conservativo: nunca menor que o modelo); degenerados (AABB
    nulo, câmara dentro do centro) = false; a vista de TOPO (a câmara na
    vertical — o orbit do dono) troca o up SEM NaN;
  - `quadMesh(...)` — o quad unitário (4 verts CCW, 6 índices, uv com
    v=0 no TOPO — a 1.ª linha do PNG é a de cima);
  - `kImpBaseColor = 3.699` — a DERIVAÇÃO do brilho: o quad é uma
    superfície para o lit de sempre (c = 0.16 + 0.58·(n·L)·texel com
    n·L = 0.2891 fixo) e o PNG carrega o lambert CRU d; com baseColor k:
    c = 0.16 + 0.58·0.2891·k·d e k = 0.62/(0.58·0.2891) produz EXATAMENTE
    a família da casa (0.16 + 0.62·d — o MESMO alcance da thumb) SEM
    saturação e SEM NENHUMA mudança ao shader (o uBaseColor é do draw
    desde a 0.11-M FASE 2).
- **`assets/ImpostorIndex.h/.cpp`** (NOVO) — o par do ThumbIndex: o PNG
  nasce SOB DEMANDA (a 1.ª vez que o TIC fica longe) em
  `impostors/<stem>.png` (o MESMO stem da thumb, a MESMA regra). A
  geração abre o .gmesh PELA TABELA (o MESMO open do draw) e
  materializa bloco a bloco SEM cache (`materializeBlock` — o caminho do
  export OBJ) até `kImpostorMaxTris` = 4096 (o MESMO teto da thumb) ou o
  teto de 65 535 verts do u16: O PICO DE RAM É UM BLOCO (o contrato
  R-041; o pin mede o maior single read). O GUARD do fantasma é a 1.ª
  linha (o asset ausente nunca nasce PNG). E A DIFERENÇA honesta do
  thumb: SEM placeholder — o impostor falhado NÃO EXISTE (a silhueta
  mentida é pior que os blocos caros; o draw segue o caminho normal).
- **`assets/ThumbRender.h/.cpp`** — `renderMeshAlpha`: O MESMO
  rasterizador software (a MESMA câmara isométrica 3/4, o MESMO
  z-buffer, o MESMO encaixe) com DUAS diferenças honestas: o fundo fica
  TRANSPARENTE (rgba 0 — o que o MASK corta) e a geometria carrega o
  lambert CRU d em cinza puro (sem ambient, sem os offsets de tom da
  thumb — a compensação é do draw). Corpo separado de propósito: o
  `renderMesh` de sempre fica BYTE A BYTE (os pins da thumb manda; o pin
  compara as duas rotinas no MESMO mesh: 73 vs 86 — o cru vs o assado).
  + `encodePngRgba` (o wrapper do ThumbPng RGBA).
- **`render/ThumbPng.h/.cpp`** — `encodePngRgba`: o IRMÃO color type 6
  do encodePngRgb (o MESMO deflate/CRC/chunks; o RGB de cima fica byte a
  byte — o pin aferre os dois color types no IHDR).
- **`render/GpuAssets.h/.cpp`** — `impostorTexture(assetRef)`: o cache
  1 ref = 1 textura (o MESMO contrato dos meshes/texturas); garante o
  PNG (o ensureImpostor), lê o PNG (o readBytes dos ficheiros de tamanho
  pequeno conhecido — a regra R-041), `loadPng` → `createFromRGBA` +
  sampler CLAMP_TO_EDGE/LINEAR (o billboard NUNCA repete — o wrap REPEAT
  de sempre mancha a borda). nullptr em falha (o chamador segue os
  blocos; NUNCA um quad branco). `releaseAll` limpa o cache (o TERM).
- **`platform/main.cpp`** — o caminho do impostor no drawTics, ANTES da
  colheita do instancing: o guard honesto (skinado NUNCA — a pose é
  dinâmica e um PNG estático mente; o MESMO guard do instancing), a
  decisão, a textura do GpuAssets, o cull conservativo do AABB inteiro
  (`worldAabbVisible` — fora do frustum NADA desenha, o MESMO teste dos
  blocos), o billboard, o drawMesh com `DrawProps{alphaMode=1 MASK,
  alphaCutoff 0.5, baseColor kImpBaseColor}` — o caminho de draw é O
  MESMO de sempre (zero ramos novos no renderer). Sem PNG: cai para o
  caminho de sempre (o fallback honesto). O quad unitário vive num
  `static Mesh g_impQuad` com upload lazy e destroy no TERM_WINDOW (o
  MESMO contrato do g_cubeMesh — nenhum id GL sobrevive ao term).
  `ImpostorFrameStats` NOVO (o MESMO padrão do SkinFrameStats) + o chip
  «imp N» na status line (SÓ nasce com impostores ativos — a linha de
  sempre fica byte a byte) + o log throttled «impostores: N» (o MESMO
  padrão 1×/s-só-quando-muda).
- **`tests/stub/glstub_fb.h`** — o rasterizador REAL do virtual reproduz
  o MASK do shader real (Material.cpp: «uAlphaMode == 1 && alpha <
  uAlphaCutoff → discard»): `ProgramKind` lê uAlphaMode/uAlphaCutoff,
  `sampleAlpha` (o canal 3 bilinear; R8/FLAT = opaco — o atlas nunca
  descarta), o fragmento descartado NÃO escreve cor nem depth (o
  `continue` é o discard inteiro — o pin prova que o quad de trás
  atravessa o vazio).

### O toggle e a UI

- **`components/MeshRenderer.h`** — `f32 impostorDistance = 0.0f` (o
  default OFF — o dono liga por TIC; skinado ignora; o .gmesh sem PNG
  gerável ignora).
- **`core/SceneSerializer.cpp`** — «impostorDist» SÓ quando != 0 (o
  MESMO contrato do lodDist; o loader nega valores ≤ 0 — o negativo
  NUNCA liga).
- **`core/UndoStack.h/.cpp`** — o TicSnap transporta (snap/restore/paste;
  o MESMO contrato do lodDistance).
- **`ui/EditorLayout.h` + `ui/EditorUi.cpp`** — o slider «impostor: N
  u»/«off» (0..500, o MESMO padrão do slider do LOD; o id
  kInspectorImpostorDist 5314; nunca um 0 silencioso).

### ZERO mudança ao formato .gmesh e ao shader

O impostor não acrescenta NADA ao ficheiro (o PNG vive em impostors/,
fora do .gmesh) e NADA ao lit (o MASK/alphaCutoff/baseColor já existem
desde a 0.11-M FASE 2 — o stub é que ganhou a emulação honesta do
discard). Os .gmesh do dono não reimportam; os TICs existentes ficam
byte a byte (o default OFF).

## §2 Provas

- **Pins novos em tests/test_fase375b_impostors.cpp: 8 casos**
  1. `fase375b_png_alpha` — o renderMeshAlpha: o canto TRANSPARENTE, o
     centro opaco com o lambert CRU (73 EXATO = 0.2891·255 — sem
     ambient), a geometria toda cinza puro, DETERMINÍSTICO; o renderMesh
     de sempre segue com o fundo opaco e o lambert assado (86 — as duas
     rotinas diferem no que o impostor precisa); o PNG RGBA round-tripa
     byte a byte pelo loadPng e o encode RGB de sempre segue color type
     2 (o RGBA é 6).
  2. `fase375b_ensure_pico_1_bloco` — o fantasma NUNCA nasce PNG; a
     geração escreve impostors/<stem>.png (color type 6); o 2.º ensure é
     HIT (zero leituras do .gmesh); O PIN DO PICO: o maior single read <
     o ficheiro (a tabela + os blocos um a um, nunca o inteiro); o PNG
     decodifica com silhueta E fundo (o MASK tem o que cortar).
  3. `fase375b_decisao_distancia` — 0 = OFF; negativo = OFF; o distOf é
     o centro transformado (30 EXATO); o limiar é EXCLUSIVO no lado
     longe (no próprio limiar ainda é o normal — o MESMO contrato do
     lodActive).
  4. `fase375b_billboard` — o centro é o AABB transformado; a normal
     (+Z) aponta PARA a câmara (0,0,1 — o pin apanhou o verso); o lado =
     a maior extensão (as colunas 0/1 com norma 2); os degenerados
     (AABB nulo, câmara no centro) = false; a vista de TOPO troca o up
     SEM NaN; a escala do TIC conta (2× escala → norma 4).
  5. `fase375b_quad_mask_pixel` — PIXEL A PIXEL no rasterizador REAL do
     stub: a metade opaca desenha IGUAL com e sem MASK; a metade
     transparente fica com o fundo da casa (14,14,16) com o MASK e lit
     sem; o descartado NÃO oclui (o quad de trás atravessa o vazio e
     desenha).
  6. `fase375b_walk_beyond_draws_quad` — o walk do drawTics: além da
     distância → 2 quads (2 dc, 6 verts cada) e ZERO dc de blocos; no pé
     (0 = OFF) → os blocos de sempre; a `GpuAssets::impostorTexture`
     resolve o PNG do storage (256×256), o cache dá o MESMO ponteiro, o
     fantasma dá nullptr e a regeneração pós-releaseAll produz id GL
     NOVO.
  7. `fase375b_fallback_honesto` — sem PNG → os blocos; SKINADO com
     impostorDistance > 0 → NUNCA quad (o guard lê a flag do meta); o
     PNG CORROMPIDO no storage → a textura não resolve (o loadPng
     recusa) → nullptr (nunca um quad branco a mentir).
  8. `fase375b_compat_011t` — o TIC novo nasce OFF; o save sem edição
     não contém a chave; com 120 a chave nasce e o load restaura; o
     negativo no ficheiro abre OFF (o guard do loader); o round-trip da
     forma 0.11-T é byte a byte.
- **Sentinela R-049** (`regress_impostors` em tests/test_sentinels.cpp)
  — a geração sob demanda (o fantasma nunca nasce; o pico UM bloco), o
  PNG alfa com silhueta E fundo, a decisão (0 = OFF; além = 1 quad de 6
  verts e ZERO dc de blocos), o fallback (sem textura NUNCA há quad) e o
  .goni (ausente = OFF; 120 restaura).
- **A DÍVIDA DA R-047 FECHADA** — a entrada completa no REGRESSOES.md
  (a sentinela `regress_instancing` existia desde a 3.5b sem registo
  documental; a ABERTURA da campanha declarou a dívida e este commit
  fecha-a com o histórico completo: causa/fix/medição/mutações/padrões).
- **Mutações 3 vermelho→verde (coladas)**

M-3.75b1 — o MASK morto no rasterizador do stub (o discard ignorado — o
fundo coberto onde o alpha é 0):

```
$ GONI_ONLY=fase375b_quad_mask ./test_core
fase375b_quad_mask_pixel                       ...  FALHOU  tests/test_fase375b_impostors.cpp:512  std::memcmp(a, b, 4) != 0
  FALHOU  tests/test_fase375b_impostors.cpp:514  a[0] == 14 && a[1] == 14
  FALHOU  tests/test_fase375b_impostors.cpp:512  std::memcmp(a, b, 4) != 0
  FALHOU  tests/test_fase375b_impostors.cpp:514  a[0] == 14 && a[1] == 14
  FALHOU  tests/test_fase375b_impostors.cpp:512  std::memcmp(a, b, 4) != 0
  FALHOU  tests/test_fase375b_impostors.cpp:514  a[0] == 14 && a[1] == 14
  FALHOU  tests/test_fase375b_impostors.cpp:536  pxAt(withMask, 32, 25)[0] == 14
fase375b_quad_mask_pixel                       FALHOU

7 teste(s) com falha
```

Reposto (cp do backup): 0 teste(s) com falha.

M-3.75b2 — a DECISÃO morta (active sempre false — o impostor nunca
desenha):

```
$ GONI_ONLY=fase375b_walk ./test_core
fase375b_walk_beyond_draws_quad                ...  FALHOU  tests/test_fase375b_impostors.cpp:585  run.quads == 2
  FALHOU  tests/test_fase375b_impostors.cpp:586  run.blockDc == 0
  FALHOU  tests/test_fase375b_impostors.cpp:587  run.total.drawCalls == 2
  FALHOU  tests/test_fase375b_impostors.cpp:588  run.total.vertices == 6u * 2u
fase375b_walk_beyond_draws_quad                FALHOU

4 teste(s) com falha
```

Reposto (cp do backup): 0 teste(s) com falha.

M-3.75b3 — o readBytes de volta na geração (a família R-041: ler o
ficheiro inteiro para o impostor):

```
$ GONI_ONLY=fase375b_ensure ./test_core
fase375b_ensure_pico_1_bloco                   ...  FALHOU  tests/test_fase375b_impostors.cpp:363  st.maxRead < gmesh.size()
fase375b_ensure_pico_1_bloco                   FALHOU

1 teste(s) com falha
```

Reposto (cp do backup): 0 teste(s) com falha. (Nota honesta: uma 1.ª
tentativa de mutação — remover o guard do fantasma — ficou VERDE: o open
do BlockMesh recusa o asset ausente e a nastura do PNG nunca chega a
correr; a defesa é em PROFUNDIDADE por construção. A mutação colada é a
que o pin apanha SOZINHO: o pico de RAM.)

- **test_core: verde** (a suíte inteira com os 8 pins + a R-049; o flake
  `sentinela_r039` POR DISCO na sandbox fica declarado no §4 — nenhum é
  desta sub-fase; o CI é o árbitro).
- **c33_virtual: 953/953 checks VERDE** (925 → 953: a FASE 30 nova, §2.1
  abaixo).
- **Gates locais TODOS verdes**: scope · docs-lint · gmesh-docs ·
  ui-vocab · theme-hex · glyph · hierarchy · projects-ui · reload ·
  check_main · link_parity · release-identity vc 76 (§2.2).
- **versionCode: 75 → 76** (app/build.gradle:29 + o §14 do
  RELATORIO-0.9.6.md — o par do gate release-identity/R-015).

### A FASE 30 do c33 (+28 checks, 925→953)

No caminho REAL do device (o drawTics com o blockRebind e o storage
REAL), 2 clones de um .gmesh de 2 blocos a 50 u do olho:

- 30.0 o .gmesh no storage REAL + o BlockMesh aberto (o blockRebind só
  liga o ABERTO) + o default OFF nos 2 clones.
- 30.1 o default OFF: os blocos desenham como sempre (4 = 2 TICs × 2
  blocos), NENHUM impostor (o byte a byte do pré-3.75b).
- 30.2 O PIN: com a distância 20, o PNG nasce SOB DEMANDA no storage
  REAL (o pico de RAM foi UM bloco), os 2 TICs desenham 2 quads («imp 2»
  no HUD), os blocos DESCANSAM (ZERO dc de blocos) e o PNG é o tile
  256×256 RGBA com silhueta E fundo (o MASK tem o que cortar).
- 30.3 o limiar 60 com o TIC a 50 u: os blocos voltam (o limiar é
  EXCLUSIVO no lado longe — o MESMO contrato do lodActive).
- 30.4 o SKINADO com a distância pedida: NUNCA desenha impostor (só os 2
  estáticos; o guard lê a flag do meta) e desenha pelo bloco (o caminho
  de sempre).
- 30.5 o PNG CORROMPIDO (a escrita parcial de sempre) + o reload do
  INIT: NENHUM quad (a silhueta mentida não existe), os 5 TICs desenham
  pelos blocos; o PNG corrupto removido → a REGENERAÇÃO sob demanda
  recupera o sistema sozinha (o pico de RAM continua sendo UM bloco).

## §3 Pin C33 (o dono preenche — com PNG)

- Antes (com o 3.5b+3.75a ativos): FPS <N> · dc <N> · bl <N>/<N> (o
  baseline original da ABERTURA: 5 FPS · dc 5164)
- Depois (os 6 TICs com «impostor: 60 u»): FPS <N> · dc <N> (esperado:
  os TICs longe somam 1 dc cada) · «imp N» no HUD
- PNG do HUD (o chip «imp N»; o log diz «impostores: N»)
- Zero crash dumps novos: <N>

(a cena dos 6 modelos de 203 MB: com o impostor ligado nos TICs que
ficam longe da câmara, o dc desce de ~863 para 1 por TIC longe + os
blocos dos perto. O PNG nasce na 1.ª vez que cada modelo fica longe — o
frame da geração paga ~2-4 ms por bloco lido até 4096 tris, UMA vez por
modelo. O limiar sugerido para a cena do dono: 40-80 u — o slider
«impostor» no inspector de cada TIC.)

## §4 Limites / não-verificado (nenhum escondido)

- **O sign-off do C33 não foi preenchido** (o §3 segue com os
  placeholders): o padrão da casa manda o dono medir no device. O pin
  C33 da 3.5b/3.75a também segue em aberto (declarados nos relatórios
  deles).
- **O impostor é de VISTA FIXA** — o PNG é a vista isométrica 3/4 da
  casa (a MESMA câmara da thumb). O billboard GIRA para a câmara (a
  silhueta não acompanha o azimute): ao longo de um orbit completo o
  modelo longe mostra sempre a MESMA cara. O padrão da indústria para a
  cura completa é o impostor de N vistas (um atlas por yaw) — evolução
  declarada no BACKLOG se o dono a precisar.
- **A geração paga-se no PRIMEIRO frame longe** — o ensureImpostor corre
  na thread do render (o drawTics) e lê blocos até 4096 tris (~1-3 ms
  por bloco, UMA vez por asset). Na cena do dono (863 blocos/modelo) a
  geração lê ATÉ encher o teto de tris — os primeiros blocos da tabela
  chegam. O frame da geração é mais longo que o normal; os seguintes são
  o draw do quad. A alternativa (gerar no import) multiplicaria o tempo
  de import — declarado e recusado nesta sub-fase.
- **O skinado NUNCA desenha impostor** (a pose é dinâmica — um PNG
  estático mente). O corredor de personagens do dono não beneficia;
  a cura deles continua sendo o skinned cull (3.5a) + o LOD.
- **O impostor substitui os BLOCOS INTEIROS do TIC** — o limiar é por
  TIC (a distância do centro do AABB do meta). Um TIC enorme (a cidade
  inteira num TIC) com o impostor ligado some POR INTEIRO além do limiar
  — a granulometria certa é um TIC por objeto.
- **O brilho é a derivação do kImpBaseColor** (o §1) — o pop no limiar
  fica subtil (a família 0.16+0.62·d é a mesma do modelo ao pé), mas a
  validação visual final é o PNG do HUD do dono.
- **O flake DA SANDBOX (9.9G de disco) — nenhum desta sub-fase:** a
  `sentinela_r039` falha POR DISCO na suíte cheia (errno 28 — a geração
  da fonte de 1.75 GB esgota os 9.1G livres com os artefactos das fases
  anteriores; ISOLADA VERDE com o MESMO binário, provado nesta sessão) e
  o `medicoes_010m_perfis` da mesma família (errno 28 no fixture de 213
  MB; ISOLADA VERDE após a limpeza de /tmp). O CI (o árbitro, disco
  maior) corre-os verdes.
- **O V.ONI não ganhou entradas** (a regra 1.6 — o VoniRegistry.cpp não
  foi tocado; VONI_referencia.md intacto).

## §5 Ficheiros tocados

| Área | Ficheiros |
|---|---|
| render | `render/Impostor.h` (NOVO — a decisão/billboard/quad puros + o kImpBaseColor), `render/GpuAssets.h/.cpp` (impostorTexture + o cache + o releaseAll), `render/ThumbPng.h/.cpp` (encodePngRgba) |
| assets | `assets/ImpostorIndex.h/.cpp` (NOVO — a geração sob demanda, o pico UM bloco, o guard do fantasma), `assets/ThumbRender.h/.cpp` (renderMeshAlpha + o wrapper RGBA) |
| components | `components/MeshRenderer.h` (o impostorDistance, default OFF) |
| core | `core/SceneSerializer.cpp` ("impostorDist" só quando != 0), `core/UndoStack.h/.cpp` (o TicSnap transporta) |
| ui | `ui/EditorLayout.h` + `ui/EditorUi.cpp` (o slider «impostor: N u», id 5314) |
| platform | `platform/main.cpp` (o caminho do impostor no drawTics + o g_impQuad + o ImpostorFrameStats + o chip «imp N» + o log throttled + o destroy no TERM) |
| testes | `tests/test_fase375b_impostors.cpp` (NOVO, 8 pins), `tests/test_sentinels.cpp` (R-049), `tests/c33_virtual.cpp` (FASE 30), `tests/stub/glstub_fb.h` (o MASK no rasterizador: sampleAlpha + uAlphaMode/uAlphaCutoff + o discard sem write), `tests/CMakeLists.txt` (os TUs novos) |
| build | `app/build.gradle` (versionCode 75→76) |
| docs | `docs/RELATORIO-0.11-W-3.75B.md` (este), `docs/REGRESSOES.md` (a R-049 + a DÍVIDA da R-047 fechada), `docs/RELATORIO-0.9.6.md` (§14 vc 76) |
