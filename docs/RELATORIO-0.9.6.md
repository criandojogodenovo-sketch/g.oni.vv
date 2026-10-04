# RELATÓRIO 0.9.6 — IDENTIDADE + SOBREPOSIÇÕES + ECRÃS + TECLADO + MESHES + COMUNICAÇÃO + BENCHMARKS

> versionCode 49 · 0.9.6 · sete grupos sequenciais (commit cada) + este fecho · suíte 810→817 casos core + 273→316 checks do c33_virtual (FASE 12 completa: 12.1-12.9) + 6 gates + scope-check + release-identity + docs-lint · sentinelas R-001..R-017

## 0. COMO LER ESTE RELATÓRIO

Sete grupos, um commit cada, CI verde após cada um: **G0** identidade (fccf1b1) · **G1** sobreposições/insets (0c543eb) · **G2** ecrãs + R-010 (21a4798) · **G3** teclado próprio (11bd340 + o CI-fix 0405836) · **G4** import de malha + R-014 (55bae05) · **G5** comunicação + docs-lint + R-016 (6d8e68f) · **G6** benchmarks + R-017 (2011771). A rastreabilidade ponto→fix→teste→linha-do-harness está na secção 12. As provas de mutação (R-015/R-016/R-017 coladas por inteiro; R-010/R-014 citadas com o ficheiro de evidência) na secção 11. O NÃO VERIFICADO honesto na secção 13 — o bloco de bench REAL do C33 é o item 1 dessa lista (o dono cola-o aqui quando correr).

## 1. O PONTO DE PARTIDA E O MÉTODO

O HEAD de partida foi o 522ed36 (0.9.5-e, a CLÁUSULA P-01 fechada com CI verde na 1ª). A leitura por ordem da cláusula P-01 (REGRESSOES → RELATORIO-0.9.5 → README → worklog) confirmou o estado; o baseline local correu verde antes do primeiro commit (803/0 + 259/259 + 6 gates). O trabalho decorreu sob o **scope-check** novo (ci/scope.txt + scripts/scope_check.py, job core-tests com fetch-depth 0 — um ficheiro fora do scope aprovado = CI vermelho = parar e perguntar); os diffs de TODOS os commits desta fase estão dentro do scope.

## 2. G0 — O FECHO DE IDENTIDADE (fccf1b1 · run 37215555172 verde na 1ª)

A task pedia o bump para 0.9.5; a forense mostrou que o bump JÁ existia (a0ce025) e a falha real era outra: o `name:` do upload-artifact era um LITERAL `'goni-vv-0.9.4-release'` no workflow — o bump nunca chegava ao nome do artefacto (foi assim que o run verde do fecho 0.9.5 publicou o APK com o nome 0.9.4). O fix em três camadas: (1) nome DINÂMICO (`goni-vv-${{ env.VNAME }}-release…`, VNAME exportado do build.gradle); (2) o **gate release-identity** (scripts/release_identity_check.py) antes do upload — versionName/versionCode do build.gradle == template do artefacto (nenhum literal divergente) == docs/RELATORIO-\<v\>.md existe + declara o par + é o MAIS RECENTE (reverter o bump = vermelho); (3) fim-a-fim no verify-entry-symbols com `merge-multiple: false` e o gate `--artifact-name` no diretório baixado. A R-015 em REGRESSOES.md com a prova de mutação A/B/C (secção 11). O artefacto do run confirmado: **goni-vv-0.9.5-release-signed**.

## 3. G1 — INSETS REAIS + CAMADAS + BARRA DE BAIXO (0c543eb · run 37217383561)

(1) **Safe area**: os 4 ecrãs cheios (Docs/Settings/Script/Texto) passam a viver DENTRO do contentRect — cabeçalho = inset do topo + 56dp (as baselines kHeaderTitleBase/kHeaderSubBase novas no Theme; a faixa preta ~96px do C33 deixou de cortar títulos); as teclas do teclado param no inset de baixo (nunca sob a barra de navegação); em desktop/tests insets=0 = layout idêntico (a fonte é ÚNICA: ui.safeArea()). (2) **Camadas** (a regra cena<painéis<modais<teclado): `fullscreenOverlayOpen()` novo — o glifo AMARELO do áudio já não desenha POR CIMA do Settings (estava sem gate desde 0.8.11), o orbit da câmara e o input do gizmo morrem com qualquer overlay aberto (o draw era gated desde 0.7.5 mas o input NÃO era — "a cena mexe-se por trás"). (3) **A barra de baixo** (tabs+drawer+status) escondida em ecrãs cheios e com o teclado do renomear aberto. (4) O Settings passa a ECRÃ CHEIO (deixou de ser banda). FASE 12.1-12.3 (259→273 checks).

## 4. G2 — OS ECRÃS + R-010 (21a4798 · run 37219059113)

**Docs**: altura de entrada VARIÁVEL medida do texto real (conta as linhas com wrap) · quebra por PALAVRAS (`textwrap::wrap` novo no TextFit.h — nunca corta palavra nem code point) · 8dp entre entradas · pesquisa centrada. A CAUSA RAIZ apanhada pela FASE 12.6: o draw das Docs usava coordenadas de CONTEÚDO num scroll que só RECORTA (não traduz) — a lista desenhava fora do sítio **desde a 0.9.2**; agora em coordenadas de ECRÃ + culling. **Settings**: cabeçalhos sticky (pin no topo, nunca cortados) · a linha dos dumps mostra SÓ o número. **Editor**: (b) o render não mente mais — `renderPieces()` novo: as peças cobrem TODOS os bytes (os GAPS de espaços/`{ } ( ) = +` eram saltados pelo classificador e o texto colava `centralmain` enquanto o cursor deixava o espaço — R-010); (c) `editorRestart` cria o ScriptComp em falta (o beco "o TIC não tem componente Script" ao primeiro Run); (e) **os erros que ensinam com botão SUBSTITUIR**: `voni::Error.fixFrom/fixTo` + `applyFix` — o 'if' ENSINA `exist` e o botão troca a palavra inteira (o 'break' traz o equivalente do CONTEXTO: ciclo→resume, option→stopand — a pilha ctx_ do Vm). R-010 em REGRESSOES.md + FASE 12.4-12.6 (273→282).

## 5. G3 — O TECLADO PRÓPRIO COMO ALTERNATIVA (11bd340 + 0405836 · runs 37219675961/37220797721)

A POLÍTICA primeiro (como a spec pede): o teclado próprio e o IME do sistema são **ALTERNATIVAS** — o toque no corpo pede o GBoard (acentos/gestos) e o próprio FECHA; o BOTÃO NOVO do cabeçalho (ícone Keyboard) abre o próprio e o main esconde o IME; NUNCA os dois. As SETAS na linha de baixo (12 unidades: `[<][^][v][>][ESPAÇO 2u][TAB][PAG][APAGA 1.5u][ENTER 1.5u][FECHAR]`) EMITEM as Key do IME pelo MESMO applyEvent (o caret move-se pelo caminho de sempre). O SHIFT: a tecla Aa na casa livre da 3ª fila alterna maiúsculas/minúsculas (o teclado só digitava MAIÚSCULAS antes — o kbLower nunca mudava). Os símbolos `{ } ( ) [ ] = + - * / < > ! , . ; : " _ # @` já estavam na página 123 (a spec confirma); a âncora acima da barra de navegação e a barra escondida vieram do G1. FASE 12.7 (282→290). O CI-fix 0405836: a sentinela JVM do R-005 passou a aceitar as DUAS classes que provam o mecanismo da corrida (ConcurrentModificationException OU NoSuchElementException — o cursor da ArrayList pode passar o fim quando o clear() ganha; a classe é timing puro, o mecanismo é o do REG-001). Zero mudanças de produção.

## 6. G4 — O IMPORT QUE NÃO APARECIA + R-014 (55bae05 · run 37220693071)

A forense das 5 hipóteses da task (por leitura de código): pasta ERRADA não (importFile escreve `assets/<stem>.gmesh` e o refreshCatalog lista kDirAssets=assets/ — PARTILHAM) · índice stale não (o catálogo refresca no fim do import E ao abrir o seletor) · extensão não (ambos .gmesh) · manifesto N/A · content:// não (a fonte SAF lê-se por streaming, o convertido escreve pelo storage). **A CAUSA REAL: o CAP DE 5 FICHEIROS SEM SCROLL no drawAssetMenu** (a pendência da F8 que nunca chegou) — com 5+ .gmesh no projeto, o import novo ficava FORA da lista visível para sempre. O fix: a lista lista TODOS com SCROLL (o padrão Hierarchy/Inspector: janela encaixada, drag=scroll, tap parado=escolha pelo scrollTap com re-despacho, culling). Sentinelas `assetpick_r014_*` (com check de GLIFOS no rect da linha — apanha linha não desenhada) + FASE 12.8: um .glb REAL pelo convert::importFile DE PRODUÇÃO → catálogo → seletor → drag → tap → meshPath aplicado (282→295). R-014 com a mutação REFEITA honestamente (a 1ª rodada falhava por razão errada — ver secção 11.4).

## 7. G5 — A COMUNICAÇÃO HONESTA + O GATE DOCS-LINT/R-016 (6d8e68f · run 37222823026)

(1) **O gate**: `ci/forbidden_docs_patterns.txt` (a lista é a FONTE — 14 padrões em 3 famílias: placeholders de template; marcadores de pendência por PALAVRA INTEIRA case-sensitive, em que `TODOS`/`todo` do português NÃO casam; texto de enchimento; hedging PT/EN por palavra inteira — `despacho`/`macho`/`colorem` não casam) + `scripts/docs_lint_check.py` (o motor; docs/**.md RECURSIVO — os RELATÓRIOS são documentação, não rascunho — + README + ESTUDOS + ARCHITECTURE + VONI_referencia + llms); no CI no job core-tests logo após o scope-check. Escape de linha ÚNICO e visível (`<!-- docs-lint:allow -->` no FIM da linha — o '# noqa' da casa, para o texto que DISCUTE os padrões como a própria R-016). (2) **A limpeza**: 24 ocorrências em 13 ficheiros — 21 eram 'TODO' PORTUGUÊS em maiúsculas ("em TODO build") → minúsculas (o português correto); 4 citações a marcadores reescritas por extenso; as citações às palavras de hedging nos RELATORIO-0.9.0/0.9.2 passaram a apontar à LISTA do gate (repetir a palavra proibida para dizer que não a usamos era o bug meta). (3) **README**: 'Como isto é construído (How this is built)' no topo — o telemóvel, o CI como único compilador, o device como vara de medir, as regressões como contratos, o não-verificado DITO. (4) **ARCHITECTURE.md** novo: o diagrama de camadas completo (Java→platform→ui→núcleo→V.ONI; a seta só desce; o main.cpp despachante; o REGISTO a única fonte da linguagem; DrawStats a única fonte dos números de render). (5) **Dois rascunhos Reddit** (docs/reddit/): r/gameenginedevs (apresentação com a lista honesta do que NÃO tem: sombras/PBR/desktop/física além do básico) e r/cpp (o CI como único compilador, o c33_virtual, as sentinelas com mutação colada, o parser que não muda quando a linguagem cresce) — tom de engenheiro, as falhas assumidas por extenso. R-016 + a mutação A/B/C/D + 2 controlos (secção 11.5). <!-- docs-lint:allow -->

## 8. G6 — OS BENCHMARKS REAIS + R-017 (2011771 · run 37225827600)

O bloco colável de **9 LINHAS FIXAS** (a fonte única é `bench::format`): identidade (versionName/vc/device/Android) · warm/cold start · cena default (fps média+mín+1% low + verts + draw calls) · cena bench com mesh importado (idem) · import glTF ref (ms + escala) · texturas (formato + ms) · áudio (ok/total + backend) · pico RSS · APK (MB + sha256) + projeto (MB). **Cada valor sabe se foi medido** (`struct Measured` — o que não pôde ser medido imprime "não medido"; a honestidade é parte da sentinela, nunca um 0 disfarçado). A máquina de fases (platform/main.cpp, dentro do frame): DefRun (a cena default aberta) → BuildScene (64 TICs em grelha 8×8 com o mesh EMPACOTADO — determinístico, zero I/O) → Import (GLB de referência DETERMINÍSTICO byte a byte: grelha 16×16 + nó com scale 2.5; o convert::importFile DE PRODUÇÃO com cronómetro; o TIC do mesh importado pela gltfInstantiate com o bindMesh do GpuAssets REAL — a "escala" do relatório é o ROUND-TRIP do 2.5 até ao Transform3D do TIC) → BenchRun → Texture (512×512 determinístico pela MESMA HardwareCompressor do pipeline: ASTC se a extensão existe, senão ETC2) → Audio (o MESMO probe 50+10 do Settings; guarda de COMPILAÇÃO __ANDROID__ — o host diz "não medido" em vez de medir o stub) → Finish (VmHWM do /proc + device/APK pela JNI benchDeviceInfo + projeto por statBytes) → Done. Os verts/draw calls saem da MESMA soma da statusLine NO MESMO frame (R-017). **P-02**: fora do bench o custo é UM if por frame. Os arranques são as marcas REAIS do processo (nativeRegisterActivity onCreate/onResume contra a 1ª apresentação e a 1ª pós-resume — quem nunca saiu do foreground não TEM warm, e o relatório diz). `ProjectStorage::statBytes` novo (FsStorage=stat do SO; FakeStorage=bytes em memória — o harness MEDE o projeto dele; SAF=não suportado → "não medido"). VvActivity.benchDeviceInfo (Build.MODEL + SDK_INT + base.apk bytes + sha256) pela ponte g_midBenchInfo — uma chamada por bench, zero custo em frames. Settings→Diagnóstico: "Correr bench" + "Copiar relatório" (o clipboard recebe o bloco EXATO byte a byte). Sentinela `regress_bench_nao_mente` + test_bench.cpp (8 casos) + FASE 12.9 (295→316: o bench INTEIRO pelo caminho da UI com o relógio INJETADO — o CI não tem GPU; o relatório MEDE o relógio que lhe deram, e se o formato hardcodasse o check não casava; os "não medido" do host são PARTE da prova).

## 9. OS BUGS QUE O LOOP APANHOU (esta fase)

1. **O botão do bench nasceu MORTO** (G6): as actionRows desenhavam mas o WALK do scrollTap não re-despachava os dois alvos novos — a MESMA classe do bug 0.9.1 ("os botões da página estavam MORTOS": desenhar ≠ tocar). A FASE 12.9 apanhou no PRIMEIRO run do harness (o tap não fechava o Settings); o walk do kBitDiag ganhou as 2 entradas com a MESMA matemática do draw.
2. **As marcas de arranque partilham o processo** (G6): os testes de handshake/lifecycle chamam o nativeRegisterActivity com origens REAIS e as marcas do bench já vinham tocadas — o test_bench via "warm medido" sem nunca ter feito a sequência. O `resetMarksForTest` (o precedente resetForTest do VoniRegistry) fecha o caso.
3. **A coordenada do tap no harness** (G6): a 1ª fórmula do yRun contava os headers das secções que vêm DEPOIS do Diagnóstico (Docs/Sobre) — o tap caía na linha da janela de texto (que abria, não o bench). Recalculado: só os 3 headers ANTES do Diag contam.
4. **A mutação B da R-017 falhava por RAZÃO ERRADA** (G6): a 1ª rodada injectava 1/30 para "distinguir" o hardcode — e o vermelho vinha do piso de 10 amostras do aggregate (0.3s ÷ 1/30 = 9), NÃO do hardcode. Refeita com o hardcode direto (30 ≠ 60 injectados) — o vermelho agora vem da DIFERENÇA entre o medido e o inventado (a lição da R-014 aplicada).
5. **O audio do bench no host** (G6): a 1ª guarda era `buildinfo::g_versionCode > 0` — mas o c33_virtual define uma build fake (vc 43) no arranque e o probe correria contra o STUB (uma medição de mentira). A guarda passou a ser de COMPILAÇÃO (__ANDROID__) — o host não tem device de áudio e o R-017 manda DIZER "não medido".
6. **O projeto por dynamic_cast** (G6): a 1ª versão do benchProjectBytes só media com FsStorage — o harness usa FakeStorage e o tamanho ficaria "não medido" (sem prova). O `statBytes` na INTERFACE fecha: Fs=stat, Fake=memória (o harness MEDE), SAF=não medido (honesto).
7. **O flake de runners do GitHub** (G3): o run 37219675961 morreu no passo "Sentinelas JVM do reload" com NoSuchElementException — o mecanismo da corrida DETETADO com a classe errada (timing puro). O CI-fix 0405836 alargou a asserção às DUAS classes que provam o mecanismo (zero mudanças de produção).
8. **wiring010 no host** (G6): a falha intermitente do import 500MB era DISCO outra vez (/tmp a 96% dos fixtures de runs anteriores); limpo → verde (a lição da FASE 9).

## 10. AS SENTINELAS NOVAS (docs/REGRESSOES.md agora termina em R-017)

| ID | o que vigia | prova de mutação |
|---|---|---|
| R-010 | o editor de script mente no render (gaps de espaços/`{` saltados; o caret media a linha inteira) + o beco do Run sem ScriptComp + o SUBSTITUIR | gap-fill desligado → sentinela FALHOU em 4 frentes; reposto → verde (ficheiro mutacao-R010-{vermelho,verde}.txt) |
| R-014 | o import não aparecia no seletor (cap de 5 sem scroll) | cap de 5 de volta → sentinela VERMELHA no check de glifos; reposto → verde (mutacao-R014-vermelho2.txt — REFEITA com o teste corrigido; a 1ª rodada falhava por razão errada) |
| R-015 | o artefacto de release com identidade errada | (A) literal 0.9.4 de volta → vermelho; (B) reverter o bump → vermelho; (C) bump sem relatório → vermelho (mutacao-R015-vermelho-verde.txt) |
| R-016 | a documentação com placeholders e achismo | (A) TODO → vermelho; (B) 'Acho que' → vermelho; (C) `<repository-URL>` → vermelho; (D) 'should work' → vermelho; CONTROLO: português legítimo (TODOS/todo/despacho/macho/colorem) → verde; CONTROLO: o escape de linha é o único escape (mutacao-R016-vermelho-verde.txt) | <!-- docs-lint:allow -->
| R-017 | o bench que mente (números hardcodados em vez de medidos) | (A) format() com a linha do import HARDCODADA → 5 testes FALHARAM; (B') benchTick com fps HARDCODADO (30 ≠ 60 injectados) → 12.9 FAIL; repostos → 817/0 + 316/0 (mutacao-R017-vermelho-verde.txt) |

## 11. AS PROVAS DE MUTAÇÃO (evidência)

### 11.1 R-015 (colada por extenso em mutacao-R015-vermelho-verde.txt)

```
== MUTAÇÃO A: o literal volta ao workflow (a causa raiz do bug) ==
::error::release-identity (R-015): nome de artifact LITERAL 'goni-vv-0.9.4-release'
  no workflow != versionName '0.9.5' do build.gradle
== MUTAÇÃO B: reverter o bump (versionName 0.9.4 / versionCode 47) ==
::error::release-identity (R-015): a versão a fechar (0.9.4) não é a mais recente
  documentada (última: RELATORIO-0.9.5.md)
== MUTAÇÃO C: bump sem RELATORIO (versionName 0.9.9) ==
::error::release-identity (R-015): docs/RELATORIO-0.9.9.md não existe
== REPOSTO: verde ==
GATE VERDE (release-identity/R-015): versionName 0.9.5 · versionCode 48 ·
  artifact goni-vv-0.9.5-release(-signed|-UNSIGNED) · docs/RELATORIO-0.9.5.md
```

### 11.2 R-010 (mutacao-R010-{vermelho,verde}.txt)

```
== MUTAÇÃO: o gap-fill DESLIGADO (renderPieces só devolve os tokens) ==
regress_r010_editor_roundtrip ... FALHOU  p.begin == pos
                                ... FALHOU  pos == std::strlen(ln)   (×4 frentes)
== REPOSTO: verde ==
```

### 11.3 R-014 (mutacao-R014-vermelho2.txt — a 2ª rodada, com o teste corrigido)

```
assetpick_r014_todos_os_ficheiros_aparecem_no_seletor ... FALHOU  naLinha >= 5
== REPOSTO (2ª prova): r014 OK · suíte verde ==
== C33 VIRTUAL: 295 check(s), 0 falha(s) ==
```

### 11.4 R-016 (mutacao-R016-vermelho-verde.txt)

```
=== MUTAÇÃO A: 'TODO fix this before release' no fim da README === <!-- docs-lint:allow -->
docs-lint: README.md:2974: padrão [\bTODO\b] em: TODO fix this before release <!-- docs-lint:allow -->
docs-lint: VERMELHO — 1 ocorrência(s)  → reposto → VERDE
=== MUTAÇÃO B: 'Acho que o render está estável.' no RELATORIO-0.9.4 === <!-- docs-lint:allow -->
docs-lint: docs/RELATORIO-0.9.4.md:295: padrão [(?i:\bacho\b)]  → VERMELHO → reposto → VERDE
=== MUTAÇÃO C: 'git clone <repository-URL>' === <!-- docs-lint:allow -->
docs-lint: docs/RELATORIO-0.9.4.md:295: padrão [<repository-URL>]  → VERMELHO → reposto → VERDE <!-- docs-lint:allow -->
=== MUTAÇÃO D: 'The renderer should work on most phones.' num rascunho reddit === <!-- docs-lint:allow -->
docs-lint: docs/reddit/r-cpp-tecnico.md:85: padrão [(?i:\bshould work\b)]  → VERMELHO → reposto → VERDE
=== CONTROLO 1: 'TODOS os testes passam e todo o texto saiu; o despacho e o macho
                 e o colorem-se ficam.' ===
docs-lint: VERDE (zero falsos positivos do português)
=== CONTROLO 2: o escape de linha é o ÚNICO escape ===
linha com marcador: TODO qualquer <!-- docs-lint:allow -->  → VERDE <!-- docs-lint:allow -->
linha sem marcador: TODO qualquer                          → VERMELHO <!-- docs-lint:allow -->
```

### 11.5 R-017 (mutacao-R017-vermelho-verde.txt)

```
=== BASELINE === 0 teste(s) com falha · == C33 VIRTUAL: 316 check(s), 0 falha(s) ==
=== MUTAÇÃO A: o format() HARDCODA a linha do import (ignora o Report) ===
bench_r017_os_numeros_vem_da_medição ... FALHOU  diffs == 9
regress_bench_nao_mente              ... FALHOU  diffs == 9   (+3 do test_bench; 5 no total)
--- restaurado: 0 teste(s) com falha
=== MUTAÇÃO B': o benchTick escreve fps HARDCODADO (30) em vez de 1/dt ===
  (o harness continua a injetar 1/60 — se o tick MEDISSE, avg≈60; hardcodado, avg=30)
[FAIL] 12.9 cena default: média ≈60fps (avg=30 ∉ (55,65): VERMELHO)
== C33 VIRTUAL: 316 check(s), 1 falha(s) ==
--- restaurado: == C33 VIRTUAL: 316 check(s), 0 falha(s) ==
NOTA honesta: a 1ª rodada da mutação B usava injeção de 1/30 e falhava por
RAZÃO ERRADA (0.3s ÷ 1/30 = 9 amostras < piso de 10 do aggregate → ok=false,
não o hardcode). Reposta com o hardcode DIRETO (30 ≠ 60 injetados) — a lição
R-014 aplicada.
```

## 12. RASTREABILIDADE (ponto → fix → teste → linha do harness)

| ponto da task | causa raiz (ficheiro) | fix | teste | harness |
|---|---|---|---|---|
| G0 artifact 0.9.4-release-signed | `name:` LITERAL no upload-artifact (release.yml:309) | nome dinâmico + gate release-identity + fim-a-fim no verify | o gate É a sentinela | — |
| G1 faixa preta corta títulos | ecrãs cheios fora do contentRect | cabeçalho inset+56dp, baselines no Theme | test_ui (fullscreenOverlayOpen) | 12.1 |
| G1 glifo áudio sobre o Settings | drawAudioGlyph sem gate de modal (0.8.11) | fullscreenOverlayOpen() no glifo/orbit/input | idem | 12.2 |
| G1 cena mexe atrás do Settings | input NÃO era gated (o draw era) | input alinhado com o draw | idem | 12.2 |
| G2 Docs corta com "…" | coords de CONTEÚDO num scroll que só recorta (desde 0.9.2) + clamp 64px | coords de ECRÃ + wrap por palavras + altura medida | textwrap | 12.6 |
| G2 editor perde espaços/{ | classificador salta os GAPS (render token a token) | renderPieces() (plano de render com gaps) | **R-010** | 12.5 |
| G2 Run=0 erros no modelo | editorRestart recusava TIC sem ScriptComp | cria o componente (precedente closeScriptEditor) | R-010 (frente 2) | 12.4 |
| G2 ensinar if/else/switch | — | kForeign + fixFrom/fixTo + applyFix (contexto do break pela pilha ctx_) | R-010 (frente 3) | 12.5 |
| G3 teclado só maiúsculas | kbLower nunca mudava | tecla Aa (shift de caso) | scriptwin | 12.7 |
| G4 import não aparece | cap de 5 SEM scroll no drawAssetMenu | lista scrollável (padrão Hierarchy) | **R-014** | 12.8 |
| G5 docs com placeholders/achismo | inexistência de vigília | docs-lint (lista+motor) + limpeza 24 | o gate É a sentinela (**R-016**) | — |
| G6 sem medições coláveis | inexistência do instrumento | core/Bench + máquina de fases + statBytes + benchDeviceInfo | **R-017** + test_bench | 12.9 |

## 13. NÃO VERIFICADO (honesto — o que SÓ o device humano afere)

1. **O BLOCO DE BENCH REAL DO C33** — o FASE 12.9 prova o mecanismo com o relógio INJETADO (o CI não tem GPU); os números do aparelho (fps real com 64 TICs + mesh, warm/cold reais, ASTC do Mali, probe do Oboe, RSS do device) só existem quando o dono tocar "Correr bench" e "Copiar relatório". **O dono COLA o bloco aqui** (a task pede exatamente isso: o bloco REAL do C33 colado no relatório):
   ```
   (bloco de 9 linhas — colar do clipboard do device)
   ```
2. **Os 60 fps com tykers + painéis** — o mesmo item do 0.9.5 (a barra de status do C33 + um script com 2 tykers).
3. **O teclado próprio em uso prolongado** — as teclas ≥48dp e a âncora são aferidas; o conforto de digitar códigos inteiros no próprio é humano.
4. **O docs-lint contra a prática futura** — o gate apanha placeholders/hedging no push; se o dono escrever docs FORA do repo, o gate não vê (o escopo dele é o repo).
5. **O README/ARCHITECTURE/reddit como comunicação** — os rascunhos estão no repo; se SÃO bem recebidos (r/gameenginedevs + r/cpp) só a publicação dirá.

## 14. A EVIDÊNCIA (colada)

- **G0** fccf1b1 — CI run 37215555172 (5/5 jobs, 1ª tentativa) · artefacto **goni-vv-0.9.5-release-signed** confirmado.
- **G1** 0c543eb — run 37217383561 (1ª) · suíte 804/0 + 273/273 (FASE 12.1-12.3).
- **G2** 21a4798 — run 37219059113 (1ª) · 808/0 + 282/282 (12.4-12.6) · R-010.
- **G3** 11bd340 — run 37219675961 (após o flake do R-005 JVM) + CI-fix **0405836** run 37220797721 · 808/0 + 290/290 (12.7).
- **G4** 55bae05 — run 37220693071 · 810/0 + 295/295 (12.8) · R-014.
- **G5** 6d8e68f — run 37222823026 (1ª) · docs-lint VERDE (46 ficheiros) · R-016.
- **G6** 2011771 — run 37225827600 · **817/0 core + 316/316 harness** (12.9) · LINK PARITY **105 TUs** (core/Bench.cpp o 105º) · R-017.
- **FECHO** (este commit) — versionCode 49/0.9.6 · este relatório · README 0.9.6.
