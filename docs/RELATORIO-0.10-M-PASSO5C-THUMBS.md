# RELATÓRIO 0.10-M · PASSO 5C — THUMBNAILS: o import pré-renderiza 256×256, o browser mostra-as instantâneo

> Sub-passo 5C do PASSO 5 (pipeline de assets final) — spec do dono.
> Scope respeitado: NENHUM toque na UI, seleção, física ou V.ONI (os
> seletores de assets consomem o PNG pronto pelo ThumbIndex — ZERO linhas
> de UI tocadas). versionCode **63** · baselines: test_core **931/0** e
> c33 **797/797** locais + os gates todos verdes antes do push (o CI é
> quem decide o fecho — a linha colada no fim pelo commit -docs).

## 1. O que o dono pediu

«No import, renderizar uma miniatura 256×256 e guardá-la em thumbs/; o
browser mostra-a imediatamente. Thumb ausente = placeholder + render a
pedido.» Os pins declarados na spec:

- o import escreve a thumb POR MESH (a mesma rotina do browser);
- 100 assets no browser = 100 thumbs em **<500 ms** (o custo de listar
  é ler PNGs PRONTOS, nunca renderizar);
- thumb ausente = placeholder no sítio + RENDER SOB DEMANDA (o pico de
  RAM é UM bloco da máquina de faixas do PASSO 4 — nunca o modelo).

## 2. As causas e as decisões (ficheiro + função)

1. **O render é SOFTWARE e PURO** (`ThumbRender::renderMesh`,
   assets/ThumbRender.cpp — TU novo): sem GL, sem storage — mesh →
   pixels, afervel no CI. A câmara é uma ortográfica isométrica fixa (a
   vista 3/4) construída do AABB REFERENCIADO (os vértices dos índices;
   órfãos não desenham), com o encaixe `0.42·s / halfExtent` projetado
   nos eixos da câmara e **8% de folga por lado** (a silhueta centrada —
   decisão estética medida no TEST). O rasterizador é o clássico
   bounding-box + baricêntricas com z-buffer de floats; o sombreamento é
   lambert flat com a direção de luz da casa (a MESMA `kLightDir` do
   Material — o dono reconhece a luz). DETERMINÍSTICO: mesmos vértices →
   mesmos bytes (o TEST prova a igualdade byte a byte de dois renders).
2. **O orçamento do render** (`kMaxTrisPorThumb = 4096`,
   ThumbRender.h): um .gmesh gigante contribui com os PRIMEIROS
   triângulos — a pré-visualização é representativa e o custo fica
   limitado por construção (a âncora vive no gate gmesh-docs).
3. **O placeholder** (`ThumbRender::placeholder`): fundo grafite + moldura
   2 px + as iniciais do stem (1.ª+última letra — a regra D18 das telas
   de projeto; «grelha1» → «G1»), determinístico. É o que o browser
   desenha quando a thumb não existe — e o que o `ensureThumb` ESCREVE
   quando o asset existe mas o render falha (a thumb diz QUEM ele é).
4. **O índice** (`ThumbIndex`, assets/ThumbIndex.cpp/h — TU novo):
   `relFor` ("assets/city_n3.gmesh" → "thumbs/city_n3.png"), `hasThumb`
   (o browser escolhe PNG vs placeholder), `ensureThumb` (o render sob
   demanda — o import chama com `force=true`, o browser sem o force;
   o hit devolve o PNG pronto — o pin do instantâneo), `loadRows` (o
   custo de listar: carrega+decodifica as thumbs de N assets; o que
   falta entra como placeholder SEM I/O a mais; o ms medido é o REAL —
   o que não pôde medir imprime -1).
5. **O caminho do render sob demanda é a TABELA DE BLOCOS**
   (`ThumbIndex::firstBlockMesh`): peek 192 B **por faixa**
   (`readBytesAt 0..kGHeaderBytes+kGmeshV3MetaBytes`), a tabela pelo
   `blockTableOffset`, o data do bloco 0 pelo `dataOffset` do entry e
   `gmeshV3MaterializeBlock` — **o pico de RAM é UM bloco, nunca o
   modelo**. A 1.ª implementação lia o peek com `readBytes` (o ficheiro
   INTEIRO): a sentinela R-039 mediu **1965 MB** no gigante de 50M
   verts (o .gmesh de ~2 GB inteiro em RAM para decidir se tem blocos) —
   a cura é a faixa e a lição virou a R-044 com a mutação TH2 (a prova
   vermelho→verde: 1963 MB → 99 MB). REGRA que fica: num módulo que
   promete «o pico é UM bloco», NENHUMA leitura pode ser `readBytes` de
   um artefacto cujo tamanho cresce com o modelo.
6. **O fantasma não nasce** (`ThumbIndex::ensureThumb`): o guard de
   existência é a 1.ª linha — asset inexistente = `false` + err legível
   («thumb: o asset não existe: …»), NUNCA uma thumb órfã em thumbs/.
   (A 1.ª implementação escrevia o placeholder para o fantasma — o TEST
   apanhou e a mutação TH4 vigia.)
7. **O import escreve as thumbs no FIM** (`writeImportThumbs` +
   `Stats::thumbs`, assets/AssetConverter.cpp/h): depois da verificação
   da cópia, antes do rácio — a MESMA rotina do browser
   (`ThumbIndex::ensureThumb` com force=true: o asset acabou de mudar, o
   render é sempre novo). POR MESH: itera `out.meshes` (o import FUNDIDO
   escreve a do mesh único; o EXPANDIDO escreve a de cada peça). Falha
   de thumb = aviso no log + o import SEGUE (o browser fica com o
   placeholder — NUNCA mata o import; o `Stats::thumbs` conta as
   escritas para o dono conferir).
8. **O formato do PNG**: RGBA → RGB determinístico e o encode é o
   `encodePngRgb` do render/ThumbPng (o zlib do 0.9.0 — ZERO
   dependência nova). O round-trip é provado pelo TEST (o PNG volta a
   decodificar 256×256 pelo loadPng de produção).
9. **O pin <500 ms é leitura, não render** (`ThumbIndex::loadRows`): o
   browser carrega+decodifica os PNGs PRONTOS (o render só corre no
   `ensureThumb` sob demanda). Medido: **15,6 ms** para 100 no
   hospedeiro (FakeStorage) e **0,5 ms** sob o provider SAF do c33.

## 3. Números medidos (host x86_64; c33 = o harness do device)

| Medição | Valor | Onde |
|---|---|---|
| 100 thumbs carregadas+decodificadas (pin) | **15,6 ms** (<500 ms) | TEST `pin_100_thumbs_em_menos_de_500ms` |
| 3 thumbs sob SAF (o provider content://) | **0,5 ms** (<500 ms) | c33 FASE 24.4 |
| R-039 com as thumbs (50M verts, fonte 1754 MB, 920 blocos) | **RAM pico 99 MB** (base 2 MB; budget 512 MB) | `sentinela_r039…` |
| R-039 com a mutação TH2 (peek ficheiro inteiro) | **RAM pico 1963 MB** — VERMELHA | mutação TH2 |
| render de 1 caixa 256×256 (12 tris) | sub-milissegundo (o pin de 100 em 15,6 ms inclui decode) | TEST |
| import 2 meshes EXPANDIDO → thumbs | `stats.thumbs=2`; 2 PNGs decodificam 256×256 | TEST + c33 24.1 |

## 4. Provas de mutação (vermelho→verde; todas por cp de backup —
nunca git checkout sobre trabalho não-commitado)

- **TH1** — o `writeImportThumbs` morre (o import deixa de escrever
  thumbs): `import_escreve_thumbs_por_mesh` VERMELHA (`stats.thumbs==2`
  falha e `thumbs/modelo_m0.png` não existe). Reposta → VERDE.
- **TH2** — o peek volta ao ficheiro INTEIRO (a regressão R-044):
  `sentinela_r039_50m_verts_ram_pico_512mb` VERMELHA (pico 1963 MB >
  512 MB). Reposta → VERDE (99 MB).
- **TH3** — o `loadRows` mente (o placeholder conta como carregada):
  `thumb_ausente_placeholder_e_render_sob_demanda` VERMELHA
  (`r0.loaded==0 && r0.placeholders==1` falha). Reposta → VERDE.
- **TH4** — o guard de existência morto (o fantasma NASCE thumb):
  o MESMO TEST VERMELHO nos 3 asserts do fantasma (o `ensureThumb`
  volta true, o err vazio e o `thumbs/fantasma.png` nasce órfão).
  Reposta → VERDE.

Estado final: test_core **931/0** · c33 **797/797** — as quatro
mutações repostas e as suítes inteiras verdes DEPOIS delas.

## 5. Os pins do dono, um a um

| Pin | Estado | Prova |
|---|---|---|
| no import, thumb 256×256 em thumbs/ (por mesh) | CUMPRIDO | TEST `import_escreve_thumbs_por_mesh` (2 peças → 2 PNGs 256×256) + c33 24.1 sob content:// (`stats.thumbs=2`) |
| 100 assets = 100 thumbs <500 ms | CUMPRIDO | 15,6 ms hospedeiro · 0,5 ms SAF (c33 24.4) — a margem é 30×+ |
| thumb ausente = placeholder + render sob demanda | CUMPRIDO | TEST `thumb_ausente…` (hasThumb false → ensureThumb → true; o gigante de 70k verts renderiza do bloco 0) + c33 24.2 |
| o pico de RAM é UM bloco, nunca o modelo | CUMPRIDO | R-039 99 MB com as thumbs no caminho · a mutação TH2 prova o contrário (1963 MB) |

## 6. Gates e CI

- scope (todo o diff dentro de ci/scope.txt — app/src/main/cpp/, tests/,
  docs/, README.md, BACKLOG.md, app/build.gradle) · docs-lint 75
  ficheiros · gmesh-docs **23 âncoras** (2 NOVAS: thumb 256, thumb teto
  tris — o tamanho e o teto do render vigiados pelo CI) · ui-vocab ·
  theme · hierarchy · gizmo · projects-ui · reload · glyph · jni-parity ·
  check_main (os 3 TUs device-only) · link-parity · release-identity
  versionCode **63** (declarado no RELATORIO-0.9.6 §14). A linha do run
  do CI entra pelo commit -docs (a prática da casa desde a 0.10.5).

## 7. NÃO VERIFICADO (honestidade da casa)

- **O browser a MOSTRAR as thumbs no device real** (o dono a ver a
  grelha de assets com os PNGs no C33): o ThumbIndex entrega os bytes
  prontos e o c33 prova a leitura sob SAF, mas o DESENHO no seletor de
  assets é UI — FORA do scope deste passo (a cláusula P-01 da campanha)
  e por isso fica para o passo de UI que o dono agendar. NADA no
  engine.log afirma o contrário.
- **O sign-off do C33 físico** (3 itens, abaixo): o harness virtual
  cobre o caminho content:// de ponta a ponta, mas o device do dono é
  quem dá a palavra final — a lista vive no §8.
- **O custo do render sob demanda num .gmesh REAL de produção** (o city
  29 MB do dono): o bloco 0 do city é ~72k verts por 920 blocos — o
  render de 4096 tris é sub-milissegundo medido nas fixtures, mas o
  número do city no device fica para o sign-off.

## 8. Sign-off C33 físico (o dono instala o APK assinado do CI)

1. Importar o city (29 MB) EXPANDIDO — as 72 peças listam no seletor
   com as miniaturas desenhadas (o render do bloco 0, a vista 3/4 com a
   luz da casa) e SEM lock visível na listagem.
2. Importar um asset com a thumb que falha (ou apagar um PNG em thumbs/
   pelo SAF) — o seletor mostra o placeholder com as iniciais e o
   engine.log diz a causa; a listagem não trava.
3. Reconverter um asset — a thumb RE-RENDERIZA (o force do import) e o
   PNG em thumbs/ muda (a data/hora do ficheiro).

## 9. Decisões novas candidatas (o dono decide)

- **As thumbs ficam quando o asset é apagado?** Hoje: SIM (a thumb é
  órfã, ocupa ~5-20 KB e o browser nunca a lê sem o asset). A cura é um
  `remove(relFor)` no caminho de apagar assets — FORA do scope do 5C
  (toca o caminho de remoção que este passo não toca). Barato e seguro
  — o dono diz quando.
- **O stem com subpastas** (`assets/pack1/peca.gmesh` →
  `thumbs/peca.png`): dois assets com o MESMO stem em pastas diferentes
  colidem na mesma thumb. No projeto real não existe subpasta em assets/
  (o conversor escreve flat) — registado como limitação consciente; se
  um dia houver subpastas, o `relFor` ganha o path sanitizado.
- **O render usa o primeiro bloco apenas** — para modelos cujos
  primeiros triângulos não representam o todo (raríssimo: o conversor
  agrupa por material na ordem), a thumb pode mostrar uma parte. O teto
  de 4096 tris é o compromisso declarado; aumentar = mais custo por
  thumb no import.

## 10. PÁRA

O sub-passo 5C fecha o PASSO 5 (5A texturas · 5B verificação do
expandir-nós · 5C thumbnails). **PÁRA ABSOLUTO** — o próximo passo é o
**PASSO 6 (validação final)**, BLOQUEADO à espera do OK explícito do
dono (com o sign-off do §8, se o dono o quiser; o APK assinado do CI é
versionCode 63).
