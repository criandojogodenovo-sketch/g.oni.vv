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

- **0.12-D · D-2 — O VIDRO (REAL e FALSO)** — o VIDRO REAL nos painéis
  FLUTUANTES: a captura da cena (`Renderer::captureGlass` — o glReadPixels
  com o y convertido no sítio + o flip de linhas), a redução a 1/4 (duas
  boxes 2×2), o BLUR DUAL KAWASE (ui/Glass.h — a função pura partilhada,
  k=1 e k=2) e o upload GL_RGBA; o CONSUMO é o MESMO widget SDF da D-1
  com o ramo uSdfTexOn (o aShape.z) — o FS/stub amostra a cena borrada em
  espaço de ecrã e multiplica pela TINTA ESCURA (theme::kTheme.glassInk
  NOVO — #6B6B76 α0.94; o produto idêntico nos dois lados); os cantos, o
  bisel e a sombra do shader único continuam. Os menus trocados: o IMPORT,
  o plusMenu, o assetMenu e os 3 sheets ELEVADO. O VIDRO FALSO nos painéis
  FIXOS (a hierarquia + o inspector): `UiContext::glassPanelFake()` — o
  gradiente surface→surface2 + o REFLEXO DIAGONAL (3 veios brancos) + o
  RUÍDO FRACO (o hash glassNoise por células — determinístico do SÍTIO) +
  a BORDA CLARA 1px (a FRESNEL). A CACHE: a decisão pura
  `glassCacheDirty` (a câmara/a cena/o rect/a superfície) — o blur só
  recalcula quando o que está atrás muda (com o overlay aberto o orbit
  está morte — a captura do OPEN é a que fica; o teto documentado). pins
  ×4 NOVOS (test_012d_vidro.cpp — a função pura + o caminho REAL até
  glReadPixels) + M-D2a/M-D2b vermelho→verde + 3 recalibrações no c33
  (13.9 ×2 + 18.1 — as sondas esquivam o grão do ruído) + 3 no test_core
  (y3 ×2, uitree). OS ACHADOS: a orientação GL da captura (o flip), o
  grão nas sondas e o teto da cache documentado (o relatório próprio:
  docs/RELATORIO-0.12-D.md §2) · **versionCode 92** — o APK vigente da
  fase D-2 (o PNG do menu com a cena por trás é o C33 do dono). O 91 fica
  INTACTO como história.
- **0.12-D · D-1 — O SHADER SDF ÚNICO DE WIDGET** — o 3.º PROGRAMA da
  engine (`progSdf_`): UM shader desenha QUALQUER widget com CANTOS
  PERFEITOS (SDF analítico, AA pela própria distância), BISEL 3D (a
  normal derivada do gradiente da distância — a ordem do prompt; a luz do
  topo-esquerda: topo +30%, fundo −45%), REALCE ESPECULAR no topo, BRILHO
  INTERNO nas bordas e SOMBRA PROJETADA por baixo — numa passagem. A
  FONTE ÚNICA é o `ui/Sdf.h` (GL-free): o FS `kSdfFsSrc` replica-a LINHA
  A LINHA e o rasterizador honesto do stub INCLUI-A (os pins aferem os
  MESMOS pixéis). O TOGGLE «Efeitos: Alto / Médio / Baixo»
  (`sdf::g_level` — Baixo: só bisel + sombra simples, a ordem expressa; a
  D-7 liga ao Settings). OS DRAWS SUBSTITUÍDOS: o choke point
  `drawButtonVisual` + o iconButton da barra emitem pelo WIDGET SDF
  (`UiContext::sdfWidget` — o batch render/SdfBatch.h, UM widget = UM
  quad); o premido mostra bisel + brilho + FOGO (accent→accent2) + sombra
  SIMULTANEAMENTE (a cláusula anti-superficialidade); o frameRounded/glass
  TopLine dos botões morreram (o bisel/especular É o tratamento da borda).
  pins ×4 NOVOS (test_012d_sdf.cpp — o caminho REAL até glReadPixels;
  sondas pelo CANAL R contra o bg — o α do fb é inútil contra o clear) +
  M-D1a/M-D1b vermelho→verde + a FASE 32 do c33 (5 checks — o widget no
  replay real + o toggle) + 4 recalibrações com nota. OS ACHADOS REAIS: o
  REBIND do ARRAY_BUFFER após submissões SDF (o upload do UI ia ao
  vboSdf_ — o bind re-afirma-se por submissão) e a CORRIDA FINA dos jobs
  de import no c33 (o teto de 3000 frames = 5 ms de relógio expirava
  antes do worker; o teto é o RELÓGIO 10 s + yield) (o relatório próprio:
  docs/RELATORIO-0.12-D.md §1) · **versionCode 91** — o APK vigente da
  fase D-1 (o PNG do botão nos 3 modos é o C33 do dono). O 90 fica
  INTACTO como história.
- **0.12-C · C-4 — SELETOR DE FICHEIROS + ESTADOS VAZIOS** — o **PICKER
  REFEITO** pela FONTE ÚNICA `browserSheetLayout()` (EditorLayout.h — o
  draw e os pins partilham, o padrão menuSheetLayout da C-2): as LINHAS
  48dp com ÍCONE de função à esquerda (pasta/mesh/imagem/áudio/?) + NOME
  alinhado à esquerda (os prefixos «mesh:»/«tex:»/«/» morreram) + a SETA
  SÓ nas diretorias · o **BREADCRUMB QUE SALTA** (`browserCrumbs()` NOVA
  — os ANTECESSORES são alvos que abrem a pasta deles pelo crumbPickOut,
  o segmento ATUAL é texto accent; transbordo = os últimos com «…») · a
  **PESQUISA** (o campo 40dp → o teclado in-app no PROPÓSITO 10 «PESQUISAR
  FICHEIROS»; o filtro puro `browserFilterEntries()` case-insensitive; o
  pick devolve o ÍNDICE ORIGINAL — nunca dois mundos) · as **PASTAS
  PONTO FORA** (o filtro no `listDirEntries` — a FONTE ÚNICA; os
  FICHEIROS ponto ficam, a decisão 0.8.5 não se reabre) · o «Subir» FIXO
  fora do scroll · a safe-area no card inteiro · OS **4 VAZIOS COM
  MENSAGEM + AÇÃO** (a cláusula anti-superficialidade manda): o Inspector
  ganha o «+ criar TIC» que abre O MESMO plusMenu do + da hierarquia, os
  Assets ganham o IMPORTAR ancorado ao fundo que abre o navegador na
  Download (o Actions::importPressed), a Hierarquia aponta o + e ele fica
  VISÍVEL (a LADDER do convite extraída para `hierEmptyInvite()`), os
  Projetos (o espelho Java) mostram o G FRACO (BORDER) e o BOTÃO «Novo
  projeto» ligado ao askNewProject · OS **GLIFOS ÚNICOS POR FUNÇÃO**: a
  lupa voltou a ser SÓ pesquisar (o Image NOVO no Icons.h/.cpp para as
  texturas), o vazio dos Assets usa o BOX (usava a PASTA) pela FONTE
  ÚNICA `bottom::emptyStateIcon(tab)` · pins ×8 NOVOS (test_012c_picker.
  cpp: picker_compacto — a linha 48dp a 2.0 + o card dentro da área útil
  nos 3 ecrãs + ≤8 visíveis + o Subir fora do scroll; picker_breadcrumb —
  os segmentos/prefixos + o SALTO pelo crumbPickOut; picker_oculta_dot —
  1 diretoria visível, ZERO pastas ponto, os ficheiros ponto ficam;
  empty_inspector — o toque abre o plusMenu; empty_assets — o toque liga
  o importPressed; empty_hierarquia — a ladder + o + visível nos
  140/220/300dp; icones_unicos — as tabs/os vazios/as linhas pairwise
  distintos + a lupa fora + o Image único; o empty_projetos vive no gate
  projects_ui_check.py — o G fraco + o CTA) com as MUTAÇÕES **M-C4a** (o
  filtro das pastas ponto morto → picker_oculta_dot VERMELHA com 2
  EXPECTs, as 6 irmãs verdes) e **M-C4b** (o vazio dos Assets volta à
  PASTA → icones_unicos VERMELHA com 2 EXPECTs, as 6 irmãs verdes)
  vermelho→reposto→verde · OS ACHADOS REAIS da sub-fase: o gesto de
  rolar importava o ficheiro debaixo do dedo (a cura: o re-despacho do
  scrollTap SÓ — o padrão da fila da hierarquia), o claim ADIADO do
  scroll comia o botão IMPORTAR (a cura: lista vazia = SEM região de
  scroll) e o IMPORTAR não cabia no drawer de 240dp (a cura: ancorado ao
  fundo) · A REPARAÇÃO (o achado fora da lista): os commits da C-1..C-3
  entraram de uma árvore incompleta e empurraram 614 deleções — a
  árvore completa voltou do e8c9587 com as 6 recalibrações que não se
  conseguiam ver (test_toolbar/test_sentinels ×3/test_wiring086/c33 ×9 +
  a flag fitByDesign no título da hierarquia) · verificação: test_core 0
  falhas + c33_virtual **1005/0** · **versionCode 90** — o build vigente
  da campanha 0.12-C. O 89 fica INTACTO como história. A campanha 0.12-C
  FECHA (o PARAR do prompt). (o relatório próprio: docs/RELATORIO-0.12-C.md §4)
- **0.12-C · C-3 — TIPOGRAFIA, IDIOMA, SUPERFÍCIES** — a **ESCALA
  TIPOGRÁFICA FIXA** (o Theme já tinha os 4 tokens 12/14/16/20 desde a
  0.9.6; o pin NOVO SCANEA os fontes de UI — o padrão stripComments do
  gate ui_vocab — e aferra que NENHUM literal de escala fora do conjunto
  existe; o teto do scan é 18 chars: os literais da casa cabem em
  fontScale(20.0f) e as expressões kFont* passam) · o **IDIOMA**: «Exportar
  Downloads» → «Exportar para Downloads» (a preposição), «FPS · TICs N» →
  «FPS · N TICs» (a ordem natural), os placeholders «Perspetiva»/«Global»
  (a strip morreu no PASSO 3) e «Nós» (a vista morreu no D5) não voltam —
  o pin vigia o regresso com o MESMO strip de comentários · as
  **MAIÚSCULAS UNIFORMES**: a tab «ÁUDIO» → «Áudio» (a MESMA regra de
  «Menu»/«Cena»; «3D»/«UI» ficam — são siglas) · a **ESCADA DE
  SUPERFÍCIES** (a ordem expressa do dono, o padrão Y-3/D11 supera o poço
  da Y-1): FUNDO #0A0A0C (o bg — o clear lê o token, R-028 automático) ·
  PAINEL #131316 (a surface, o α0.80 do vidro mantém) · CARTÃO #1B1B20
  (o token card NOVO — os cards reais: o browser, o diálogo aplicar, o
  seletor de primitivas, o diálogo do teclado) · ELEVADO #24242A (o token
  elevated NOVO — os sheets: o menu principal, o CENAS, o ⋮ da hierarquia,
  o TOAST) · a borda âmbar subtil entre painéis mantém-se (a hairline
  #4A3714 α0.60 = o accentDim) · o ESPELHO JAVA alinhado (o
  ProjectManagerActivity tinha os hexes da 0.9.6.10! — BG/SURFACE/SURFACE2/
  BORDER + CARD/ELEVATED novos + o SURFACE_GLA/SURFACE2_GLA + o hex velho
  0xFF2E2E32 inline trocado pelo token; o gate projects_ui_check
  TOKENS_F recalibrado em coerência) · pins ×4 NOVOS (test_012c_idioma.cpp:
  escala_tipografica — os 4 tokens + o scan; idioma_consistente — os 3
  formatos mortos + «para Downloads» + o Downloads isolado varrido; maiusculas_uniformes — a caps morta + a tab «Áudio» + as siglas;
  superficies_4_tons — os 4 hexes exatos + distintos + a border âmbar +
  a FONTE com os 4 + o contraste WCAG sobre o novo fundo) com as MUTAÇÕES
  **M-C3a** (um fontScale(18.0f) injetado no draw do título →
  escala_tipografica VERMELHA com 2 EXPECTs — o scan apanha o literal no
  código stripado) e **M-C3b** (o cartão igualado ao painel no Theme E no
  alias — o static_assert não morre porque os DOIS lados mudam juntos e o
  PIN é que grita: superficies_4_tons VERMELHA com 4 EXPECTs, as irmãs
  verdes) vermelho→reposto→verde · recalibrações legítimas com nota no
  sítio: test_fase_y1_tema (o bg (8,8,10)→(10,10,12) + a surface
  (21,21,23)→(19,19,22) + a FONTE 1× + os tokens novos no pin), test_012b_canvas
  (o toast é O ELEVADO — o B-3 dizia surface2), c33_virtual (o literal do
  composto (8,8,10)→(10,10,12)) · verificação: test_core 0 falhas nos 23
  grupos afetados (escala_/idioma_/maiusculas_/superficies_/y1_/y2_/y3_/
  012a_/012b_/regress_/menu_/topbar_/mover_/viewport_/painel_/divisor_/
  hierarquia_/toolbar_/wiring090_/uieditor_/scenes_/safearea_ verdes +
  projects_ui_check VERDE) · **versionCode 89** — o build vigente da
  campanha 0.12-C. O 88 fica INTACTO como história. (o relatório próprio:
  docs/RELATORIO-0.12-C.md §3)
- **0.12-C · C-2 — MENUS, SOBREPOSIÇÕES, BARRA DE TOPO** — o **MENU PRINCIPAL** com a matemática extraída para a FONTE ÚNICA `menuSheetLayout()` (EditorLayout.h — o draw e os pins partilham: a altura MÁXIMA + o maxOffset do scroll + a âncora 8dp sob o botão com clamp honesto; o D14 e o scrim 40% do PASSO 4 mantêm-se — nada cortado em baixo) · os **MENUS DA CASA JÁ ANCORADOS** (o ⋯ do viewport arma o fileMenu com as âncoras menuAx/menuAy; o CENAS e o ⋮ da hierarquia usam o MESMO contrato ax/ay — nenhum menu «solto») · o **PLAY EM DESTAQUE QUANDO ATIVO** (o iconButton da top bar ganha o parâmetro active — em playMode o play é A CHAMA: o gradiente vertical accent→accent2 da Y-1 no chip 28dp + a tinta accentInk; em editor o botão fica quieto — o accent é ESTADO) · o **MOVER DUPLICADO SAIU** (o gizmo 40dp do topo-direito mostrava O MESMO ícone da ferramenta ativa e alternava a transição selectMode↔gizmo que o RAIL já tem — sem função diferente = fachada = SAI, a cláusula da campanha; o kVpGizmoId fica APOSENTADO com o padrão D21 do [+]; a fila do topo respira até à borda direita e a legenda idem) · **BARRAS FLUTUANTES** (o layout do chrome re-provado dentro do rect: nada full-width, os alvos ≥40dp, a degradação esconde em vez de transbordar) · pins ×4 NOVOS (test_012c_chrome.cpp: menu_scroll — 896dp de conteúdo num maxH de 644 dá h<contentH com maxOffset>0 e o fundo nunca cruza a tab bar, o ecrã alto cabe inteiro com maxOffset 0, o retrato 336 idem; menu_ancorado — y=ay+8 sem clamp, o clamp horizontal/vertical ativo com âncoras extremas, o centrado sem âncora; topbar_3_zonas — esquerda≤centro≤direita nos 4 ecrãs de referência, as tabs contíguas com a mesma largura, a degradação 480dp tira o NOME e as zonas continuam sem tocar; mover_unico — o rail 5 ferramentas existe, o gizmoBtn/gizmoPanel são ZERO, a fila e os pais DENTRO do rect, o canto sub-mínimo limpo) com as MUTAÇÕES **M-C2a** (o menu cortado de volta: menuSheetLayout devolve h=contentH → menu_scroll VERMELHA com 14 EXPECTs e menu_ancorado com 8 — a mesma função, o isolamento das irmãs provado) e **M-C2b** (o segundo Mover volta: gizmoBtn/gizmoPanel repostos → mover_unico VERMELHA com 6 EXPECTs) vermelho→reposto→verde · recalibrações legítimas com nota no sítio (o canto direito: test_sentinels regress_chrome_passo3 + o ltiny + a escada 140/72dp + a coerência do rect; test_wiring090 ×3 testes — o vpchrome g11, a topbar g29 e a parte dos alvos N−2; test_timeline — a strip antiga vive no mundo do wrapper 300-fixo e os painéis do teste passam a usar o MESMO raw; c33_virtual FASEs 13.7/14.1/G2/D21 — o canto limpo é o gizmoBtn ZERO; test_uieditor — o tap do + vem da FONTE ÚNICA hierHeaderRects, o hardcoded 300−16−14 caía no ⋮ com a hierarquia 20%) · verificação: test_core 0 falhas nos 21 grupos afetados (menu_/topbar_/mover_/viewport_/painel_/divisor_/hierarquia_/regress_/012a_/012b_/safearea_/toolbar_/wiring090_/uieditor_/uilayout_/y2_/y3_/scenes_/play_/scroll_/timeline_ todos verdes) · **versionCode 88** — o build vigente da campanha 0.12-C. O 87 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.12-C.md §2)
- **0.12-C · C-1 — AS PROPORÇÕES E OS PAINÉIS** — a campanha 0.12-C abre (a pré-condição era a 0.12-B fechada): a **HIERARQUIA 20%** (safe::kHierPct 0.18→0.20 — a ordem expressa do dono «Hierarquia ~20%»; o piso 140dp mantém) e o **VIEWPORT ≥60% POR DEFEITO** — o estado de FÁBRICA é sem seleção (o inspector é o TRILHO de 32dp, a regra PASSO 2): hier 20% + trilho dá ≥60% em TODOS os ecrãs de referência (776→75,9% · 800→76,3% · 1512→79,3% · 1536→63,1% com seleção... o pin mede os DOIS estados); COM seleção nos ecrãs ≥1300dp ≥60% (o teto 260 manda) e nos estreitos o piso ≥55% da R-033 mantém-se (56,8%/57,5% — documentado, os pisos falam) · o **CABEÇALHO da hierarquia em linha PRÓPRIA**: hierHeaderRects NOVA (EditorLayout.h — a FONTE ÚNICA da geometria título/⋮/+ partilhada pelo draw e pelos pins; o título ELIPSA com «…» no budget dele via labelFittedStyled e NUNCA sobrepõe as ações — no piso 140dp o título desenhava POR CIMA do ⋮ e do +) · pins ×4 NOVOS (test_012c_paineis.cpp: viewport_60_pct — o trilho ≥60% nos 5 ecrãs de referência + o drawer não mexe a LARGURA + a seleção ≥60% nos ≥1300 + o piso ≥55% R-033 nos estreitos + hier==20% de 1600; painel_colapsado_default — BottomState{} nasce bottomTab=0, a altura consumida é 0, o EditorState::drawerH nasce 0; hierarquia_cabecalho — o budget do título termina 8dp antes do ⋮ nos 140/300dp, os alvos 28dp na linha, o + no extremo direito; divisor_drag — o estado cru hierW/inspW manda com os clamps da gangorra, o intervalo [180..260] invariante, as pegas 24dp, o layout.json leva hierW/inspW round-trip) com as MUTAÇÕES **M-C1a** (os defaults fixos de 300 de volta — o comportamento 0.8.x que dava 37% no device: resolvePanels devolve 300/300 fixos → viewport_60_pct VERMELHA com 12 EXPECTs, divisor_drag com 62, hierarquia_cabecalho VERDE — o isolamento provado) e **M-C1b** (o auto-abrir da 0.8.0 de volta: BottomState::bottomTab default = 4 → painel_colapsado_default VERMELHA com 4 EXPECTs) vermelho→reposto→verde · verificação: test_core 0 falhas nos grupos afetados (regress_/safearea_/toolbar_/wiring090_/uieditor_/hierarquia_/012a_/012b_/scenes_/y1_/y2_/y3_/uilayout_/play_/scroll_ todos verdes + os 4 pins novos) · **versionCode 87** — o build vigente da campanha 0.12-C. O 86 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.12-C.md §1)
- **0.12-B · B-4 — O SELETOR DE CENAS COM AÇÕES (fecho da campanha)** — as 3 operações de gestão do prompt em cada linha do menu CENAS: o **RENOMEAR** (o teclado in-app no PROPÓSITO 8 novo — o título «RENOMEAR CENA», o nome atual PRÉ-CARREGADO no buffer, o alvo em EditorState::textScene; o caminho novo vem da FONTE PURA sceneRenamePath com o MESMO filtro do addScene: vazio e / \ : . recusados, o mesmo nome é false), o **DUPLICAR** (o nome único Godot-style «main.001» — a MESMA convenção do uniqueTicName, a variante DUPLICAR que SEMPRE sufixa; sceneUniqueName consulta o manifesto E o duplicateSceneInProject os ÓRFÃOS do disco; o conteúdo copiado BYTE A BYTE; a cópia NÃO fica ativa — nunca rouba o foco) e o **APAGAR 2-TAP** (o padrão da casa: o 1.º toque SÓ ARMA — a linha em estado «apagar?» com o ícone Trash em danger + a moldura da zona; o arm vive no EditorState::sceneDelArm e MORRE com o menu (fora/fechar/trocar desarmam); o 2.º toque no MESMO ícone é que sai para o main pelo pick kScenesActBase+linha*4+ato; um toque no ícone de OUTRA linha RE-ARMA essa — nunca apaga direto; a cena ATIVA recusa — «ative outra cena para apagar esta» — e o activeScene re-assenta se a apagada vinha antes) · as 3 FONTES PURAS partilhadas pelo draw e pelos pins (sceneRenamePath/sceneUniqueName/sceneDeleteAction — o padrão sceneRowIcon/toastColors) + as 3 ZONAS de ação de 40 dp por linha (os ícones Rename/Duplicate/Trash do atlas, zero glifos novos; o check da ativa vem para junto ao nome) · pins ×4 NOVOS (test_012b_cenas.cpp: cena_rename — o caminho + os 5 recusados + o gesto real + o pré-carregado; cena_duplicate — a sequência .001/.002/.003 + o sempre-sufixa + o truncamento 28 + o gesto; cena_delete_confirm — a matriz da decisão + o gesto 2-tap de ponta a ponta; cena_serializa — o manifesto round-trip no FakeStorage com as 3 ações pelo MESMO caminho do main, o setting verbatim sobrevive) com a MUTAÇÃO **M-B4a** (o apagar sem confirmação — sceneDeleteAction devolve SEMPRE 2 → cena_delete_confirm VERMELHA com 14 EXPECTs, as irmãs verdes — o isolamento provado) vermelho→reposto→verde · ZERO recalibrações (os pins do menu 0.7.1 e das B-1..B-3 passaram intactos — o tap no centro da linha continua a trocar) · **R-051 NOVA** («a cena não se apaga sem o 2.º toque») · verificação: test_core 0 falhas (GONI_SKIP dos 2 flakes conhecidos: sentinela_r039 e regress_streaming_paralelo), c33_virtual 1006/0 · SEM Pin C33 na B-4 (a ordem expressa do prompt) — a campanha 0.12-B FECHA (B-1..B-4) · **versionCode 86** — o build vigente da campanha 0.12-B. O 85 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.12-B.md §25+)
- **0.12-B · B-3 — O CANVAS COM A MOLDURA DO DEVICE** — o modo UI vê o que o device vê: o **SELETOR de proporção ANDROID** (as 3 CHIPS no topo do viewport — a FONTE ÚNICA uiCanvasChipsRect partilhada com os pins; 0=livre/16:9/18:9/20:9, o tap cicla), a **MOLDURA** (o rect da proporção escolhida CENTRADO no design — o lado MAIOR casa, o letterbox do device preview; uiDeviceFrameRect puro) com a **ÁREA FORA ESCURA** (os 4 bandos α0.55 — o que o device não vê; o layout dos ELEMENTOS não muda: o elementRect é a fonte, a moldura é a pré-visualização honesta do corte) + as **GUIAS de margem segura** (ativáveis — as 4 hairlines accent ao INSET da moldura pela safe-area do design) + a **GRELHA de alinhamento** (ativável — as hairlines a cada 40px de design DENTRO da moldura) + o **TOAST OPACO** (a FONTE ÚNICA vpchrome::toastColors/toastBgAlpha/toastContrastRatio — o vidro surface2 α0.86 com o fade misturava a cena e o «joystick adicionado» ficava ilegível; agora o alfa é 1: o contraste WCAG do par ≥4.5 não depende do que está por baixo) · pins ×3 NOVOS (test_012b_canvas.cpp: canvas_moldura — a matemática dos 4 presets + o véu α0.55 fora da moldura no batch e a ausência em «livre»; canvas_guias — o toggle pelas CHIPS alterna e as hairlines aparecem/morrem na coluna do INSET (distintas da borda da moldura); canvas_toast_contraste — o alfa 1 + o rácio WCAG ≥4.5 pela fonte) com as MUTAÇÕES M-B3a (a moldura removida — 1 vermelho) M-B3b (o toast volta ao cinza-vidro — 2 EXPECTs) vermelho→reposto→verde · recalibração legítima: test_uilayout (a paridade editor↔Play — as chips são chrome de TEXTO no topo do viewport que o Play não desenha; os glifos da banda delas saem da comparação, a geometria dos ELEMENTOS continua par a par) · verificação: test_core 0 falhas (3 pins novos + 1 recalibração), c33_virtual 1006/0 · **versionCode 85** — o build vigente da campanha 0.12-B. O 84 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.12-B.md §20+)
- **0.12-B · B-2 — O INSPECTOR DE UI (2D + âncoras)** — o Inspector de CONTROLO da árvore: as **3 SECÇÕES COLAPSÁVEIS** (Transform/Aparência/Comportamento — o cabeçalho com chevron alterna; o EditorState::inspUiCollapsed guarda o colapso; uiInspectorPlanControl é a FONTE ÚNICA do plano, o mesmo contrato de y cumulativo) + os campos da base (margens L/T/R/B 0..300, pivô X/Y 0..1, rotação −180..180, o toggle ESTICAR — ao sair a posição conserva-se pelo rect atual, o padrão do detach — e a OPACIDADE 0..1) + os **campos por tipo** (Label/Button: texto/letra/estilo/fundo A/hex/tex; **Joystick: raio 40..150 px (escreve a escala do raio — o MESMO campo size de 0.7.3), ZONA MORTA NOVA no TouchControls (fração do raio em que o eixo lê 0; o default 0 = byte a byte; o .goni grava `dead` só quando != 0), sensibilidade e cor escrevem o TouchControls do MESMO TIC — a fonte de input nunca muda) + os **botões de RESET por campo** (o quadrado 30px com o ícone Reset à direita das linhas resetáveis — o valor do slider desloca-se para a esquerda, o quadrado é INTEIRO dentro da linha e do painel; os defaults vêm de elementDefaults, a fonte única; a zona do reset tem PRIORIDADE sobre o tap do corpo) + **SEM «remover joystick»** (a linha que tirava SÓ o componente morreu — o apagar é do TIC, «remover elemento», o padrão da árvore B-1; o uiJoystickPlan/drawJoystickInspector do 0.7.x saem do código) · pins ×5 NOVOS (test_012b_inspui.cpp: insp_control_2d — o plano X/Y/W/H + os sliders escrevem o Control AO VIVO; insp_ancoras — as 9 âncoras escolhíveis pelos 2 ciclos, o controlRect == elementRect nos 9; insp_sem_remover — nenhum caminho tira só o componente; insp_campos_por_tipo — os planos do Label e do Joystick diferem campo a campo; insp_reset_visivel — os resets ≥10, o quadrado inteiro, o reset da linha X volta a 0) com as MUTAÇÕES M-B2a (as âncoras fora do plano — 1 vermelho) M-B2b (o tipo não muda os campos — 5 EXPECTs) vermelho→reposto→verde; o ACHADO do drawUiInspector: a skip-list dos sliders saltava a linha ANTES do check da zona de reset — o reset passou a ter um LOOP PRÓPRIO com prioridade (a nota no sítio) · recalibração legítima: test_joystick (o joystick-proxy usa o plano de CONTROLO; o plano de 0.7.x morreu com o botão) · verificação: test_core 0 falhas (5 pins novos + 1 recalibração), c33_virtual 1006/0 · **versionCode 84** — o build vigente da campanha 0.12-B. O 83 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.12-B.md §14+)
- **0.12-B · B-1 — A ÁRVORE DE UI (UI como TICs)** — o refactor estrutural da campanha 0.12-B (a pré-condição era a 0.12-A fechada): o `Control` NOVO (components/Control.h — a base 2D que todos os elementos partilham: pos2D/tamanho/âncoras 9 cantos + ESTICAR/margens/pivô/rotação/visível/opacidade; o payload por tipo é O MESMO UiElement da 0.7.0 por valor — zero duplicação; Label/Button/Panel/Image/Menu/Card/Article/VBox/HBox/Joystick são KINDS do Control, sem classes novas — a cláusula do prompt) + o `UIRoot` NOVO (o marcador da raiz; o editor cria/reusa o TIC "UI") — cada elemento de UI é um TIC FILHO do raiz; a Hierarchy mostra a árvore com o que a 0.11-H já dá (seta/ícone por tipo/rename/duplicate/delete/drag-reparent) e os ícones por tipo entram no hierIconFor (Monitor/Person/Cursor/Sort/List/Grid/Box); o UiCanvas fica DEPRECATED (o leitor converte; os elementos novos nunca o usam) — a COMPAT: `migrateUiCanvasToTree()` no fim do loadText (UiCanvas → UIRoot + filhos automáticos, o parent por NOME da 0.7.4 vira pai TIC, o .goni no disco não é reescrito) e a animação de UI (0.8.0) resolve os alvos Ui* na ÁRVORE por nome; o serializer partilha appendUiElementJson/fillUiElementJson (extrações byte a byte do vocabulário do UiCanvas) + os campos da base só quando não-default (stretch/ml/mt/mr/mb/px/py/op); o EDITOR tem o MODO ÁRVORE (a vista espelho UiTreeView alimenta o MESMO draw/hit/drag da 0.7.x com sincronização de volta; todos os joysticks desenham; o tap seleciona o TIC) e o PLAY desenha/hit-testa a árvore (drawUiTree/hitTestUiTree/uiElementFromHit) com TODOS os TouchControls a desenhar; pins ×9 NOVOS (test_012b_uitree.cpp: control_base_campos / uirrot_tem_filhos (41 filhos — o «sem limite» é do formato) / ui_add_joystick_cria_tic / ui_add_2_joysticks / hierarquia_arvore (a seta com e sem filhos) / hierarquia_rename / hierarquia_delete / serializa_arvore / compat_uicanvas) com as MUTAÇÕES M-B1a (o joystick não cria o filho — 2 vermelhos) M-B1b (o dump deixa de escrever o Control — 5 EXPECTs) M-B1c (a conversão desligada — 3 EXPECTs) vermelho→reposto→verde; **R-050 NOVA** (a árvore é a fonte da serialização; a mutação colada); 8 recalibrações legítimas com nota no sítio (o CONTRATO DE ABERTURA mudou por ordem do prompt: os pins que aferravam o load antigo leem a árvore) + o BUG apanhado a meio (o UIRoot fantasma: o appendComponentJson(const UIRoot*) sem null check gravava o marcador em TODOS os TICs e matava a migração — curado com o check, §10 do relatório) · verificação: test_core 0 falhas (9 pins novos + 8 recalibrações), c33_virtual 1006/0 · **versionCode 83** — o build vigente da campanha 0.12-B. O 82 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.12-B.md)
- **0.12-A — OS BUGS P0 (A-1..A-4)** — a campanha de correção (zero features, zero refactors, a cláusula calma): **A-1** as rows `lod distance`/`impostor` do Inspector JÁ EXISTEM desde o vc 72/76 (o diagnóstico com ficheiro:linha no relatório) — a decisão do prompt PÁRA no fix cego e entrega os 5 pins da UI (test_012a_lodui.cpp: row_existe/aplica/persiste/compat/default_off) com as MUTAÇÕES M-A1a (o push da row removido — 2 testes vermelhos) M-A1b (o tap deixa de escrever — 1 vermelho) coladas; **A-2** os contadores: a FONTE estava certa (o manifesto em memória, o listDir de assets/) e o BUG era o TIMING — o `refreshCatalog()` não corria no ARRANQUE nem no `createSceneNamed` (o drawer nascia «0 cena(s)» / «sem ficheiros» com modelo importado) — o refresh entra nos DOIS pontos + `sceneRowIcon()` NOVA (a fonte única do ícone da linha de cena: `Icon::Scene` camadas substitui a CLAQUETE que a 24 dp lia-se como glifo partido — o C33 do dono) — pins ×3 (test_012a_contadores.cpp: ficheiros_conta_cenas pelo UV do atlas — um contador hardcoded morre nos DOIS lados; assets_lista_mesh pela sequência de glifos; cena_ativa_marcador) com as MUTAÇÕES M-A2a (o contador hardcoded «0 cena(s)» — 1 vermelho) M-A2b (o catálogo de meshes não lido — 2 EXPECTs) coladas; **A-3** a grelha que vazava por trás do vidro do Inspector no modo UI (o `inspRight` é falso em UI — o painel vive DENTRO do viewRect e o scissor do pass 3D cortava ao viewRect) — o fix é a via SCISSOR (a que o M-A3a do prompt pressupõe): `safe::scissor3dRect()` NOVA (pura, byte a byte fora do modo UI; o glViewport fica no viewRect — só o glScissor corta) + o TOOLTIP que nunca morria (a legenda do gizmo era permanente): `legendTimer`/`legendArm` no EditorState — nasce no LARGAR da escolha, vive 1,5 s com o dt REAL (o vpchrome::draw ganha `dt, in` trailing com defaults byte a byte) e morre no largar do toque SEGUINTE — pins ×3 (test_012a_grelha_tooltip.cpp) com as MUTAÇÕES M-A3a (o scissor removido — 2 EXPECTs) M-A3b (o timer removido — 3 EXPECTs, o pin REPRODUZ o bug do dono) coladas; **A-4** a investigação RAM vs GPU SEM fix (a regra da sub-fase): o sampler `bench_ram:` no engine.log sob `#ifdef GONI_BENCH_RAM` (tri/fps/frame_ms/rss/pico/livre/dc/bl/a_transmitir, 1×/s) + o alvo opcional `bench_ram` (tests/CMakeLists, option OFF) — A CURVA 50k…12,3M no hospedeiro REPRODUZ o salto: linear até 4,1M (0,01→1,11 ms) e EXPLOSÃO a 8,2M/12,3M (376/900 ms) no cruzamento EXATO do teto de VRAM da cache (192 MB) com 9 758/23 470 EVICTIONS/frame — e o discriminador `evc_par == evc_orb` (o thrash é contínuo com a câmara PARADA): **veredito RAM (a cache de blocos a ciclizar), NÃO GPU/CPU; o RSS é linear sem penhasco** — o protocolo do dono + as recomendações no docs/RELATORIO-0.12-A-INVEST-RAM.md · o ACHADO FORA DA LISTA (a cláusula calma, documentado e NÃO fixado): o `inspectorRowCount` subestima a MALHA em 4 rows para mesh file (o plano empurra 6, a função conta 2 — o desalinhamento da 3.5a→3.75b; sem sintoma: o contentHeight deriva do plano e o chamador usa 64 fixo) · verificação: test_core 1077/0 (11 pins novos; os 2 flakes conhecidos skipados), c33_virtual 1006/0, gates scope/docs-lint/theme/vocab/release-identity VERDES, main.cpp sintetizado com e sem o ifdef · pins ×11 NOVOS, 6 mutações vermelho→verde, SEM sentinela R-NNN nova (os prompts da campanha não declararam sentinela; os pins aferram) · **versionCode 82** — o build vigente da campanha 0.12-A. O 81 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.12-A.md + docs/RELATORIO-0.12-A-INVEST-RAM.md)
- **0.11-Y · Y-3 — OS CARTÕES (o cristal: borda brilhante, separadores com gradiente)** — a aplicação das primitivas da Y-1 aos painéis reais (o critério C33 ④ do dono): a **LINHA DO TOPO** (glassTopLine — 1dp do glow a 20 %) depois do painel de vidro na HIERARQUIA, no INSPECTOR, no DRAWER, no card CRIAR e nos 3 sheets (os anchored menus + o de cenas) · o **DIVISOR com gradiente** (dividerGradientV NOVO no UiContext — 1dp de largura, o âmbar da aresta a CAIR do topo: accentDim α0.90 → α0.15, o gradiente vertical): o divisor DIREITO da hierarquia, o ESQUERDO do inspector e a linha em repouso de drawPanelDividers — a linha sólida do D11 (0.9.6.18) é SUPERADA pela ordem expressa da Y-3 («separadores com gradiente»), a pill de grip só-no-drag FICA · pins ×3 NOVOS (test_fase_y3_cristal.cpp: y3_cristal_hierarquia (a linha do topo na largura do painel + o divisor com o gradiente aferrados no batch + A LINHA SÓLIDA MORTA — nenhum quad 1dp uniforme α0.60 full-height), y3_cristal_inspector (o MESMO contrato no painel direito), y3_cristal_divisores (a linha em repouso de drawPanelDividers é o gradiente)) com as MUTAÇÕES M-Y3a (a linha do topo morta na hierarquia — 1 EXPECT vermelha) M-Y3b (o divisor de volta à linha sólida — 2 EXPECTs vermelhas) coladas no relatório · a FASE 31 (a varredura de 13 ecrãs) confirma o cristal SEM romper o contrato do layout: 1006 checks, 0 falhas no device virtual · SEM sentinela R-NNN nova (a aplicação visual dos primitivos pinados — os pins y3_* guardam) · **versionCode 81** — o build vigente da campanha 0.11-Y. O 80 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-Y-Y3.md)
- **0.11-Y · Y-2 — O LAYOUT (zero overflow, nada cruza a tab bar)** — o critério C33 ③ do dono com FERRAMENTA permanente: as DUAS REGRAS NOVAS no validador puro da casa (ui/LayoutDump — o mesmo caminho do botão Auditoria): **CruzaTabBar** (a faixa da tab bar de baixo, altura safe::kBottomTabH × a densidade do registo — a entrada ou vive fora da faixa ou É DELA; atravessar a borda é o defeito; as exceções honestas: o clip do scroll, o VÉU da camada modal que contém a faixa inteira — o que desenha depois do véu é a camada sobre o modal — e o OVERLAY de propósito) e **PainelFora** (o painel/moldura não-interativo que sangra o contentRect; o véu que o contém é intencional) · a flag auditável **overlay** (auditOverlayBegin/End, o padrão do auditComposite) formaliza a decisão da casa das FASEs 15.x («o toast é overlay INTENCIONAL — fora da conta») e o dump JSON documenta (`ov`) · OS TRÊS BUGS REAIS apanhados pela varredura ao vivo: (1) **o toast na densidade 2.0 MERGULHAVA na tab bar** (a classe R-018: kBottomTabH/margens em px crus — subtraía 32px de uma faixa de 64 e media 48px em vez de 96; TODAS as medidas do toast passam por dp()), (2) **o back do Settings sangrava 6px acima do contentRect** (o quadrado 48dp centrado no cabeçalho de 36dp — (36−48)/2 = −6; a lei de ouro PASSO 1: o elemento de linha toma a altura da linha, 36×36 como o back das Docs), (3) **o rótulo do último tick da régua da timeline sangrava a borda direita** (medido [1503,530 13x14] vs 1512; o rótulo e o timecode do playhead deslizam para dentro, nunca truncados) · a recalibração honesta do **Sobreposto**: os filhos CLIPPED de um scroll saem dos pares (o rect cru do scrolled-out era 11 falsos ERROS nos drawers — o mesmo contrato do ForaDoEcra) · a **FASE 31 NOVA do device virtual**: a VARREDURA de 13 ecrãs desenhados pelo frame() de produção com o audit ligado (editor3d · 4 tabs do drawer · modo-ui · modo-audio · settings · docs · script · storage · settings+drawer · drawer-max-400) × 3 checks cada (nada cruza / nenhum painel sangra / ZERO erros) + o dump de caça GONI_DUMP31 · pins ×3 NOVOS (test_fase_y2_layout.cpp) com as MUTAÇÕES M-Y2a (o check CruzaTabBar morto — 2+1 EXPECTs vermelhas) M-Y2b (o PainelFora morto — 2 EXPECTs) coladas no relatório · o **CI apanhou o leftover da Y-1** (o probe 18.2 com o literal do bg velho — o run do 3a6d934 ficou VERMELHO nesse passo e o CI fez o seu trabalho): o probe lê o TOKEN; o run VERDE deste commit fecha a Y-1 E a Y-2 · **versionCode 80** — o build vigente da campanha 0.11-Y. O 79 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-Y-Y2.md)
- **0.11-Y · Y-1 — O TEMA (o poço + a chama no ecrã)** — a identidade visual G.One VV no C++ inteiro (o par do ícone da Y-0): a TABELA recalibrada (ui/Theme.h — a fonte única): bg #0E0E10→**#08080A** (O POÇO — o mesmo do launcher; o clear lê o token pelo contrato R-028, o céu da viewport escurece com ele), surface #161618→**#151517** α0.80 e surface2 #202023→**#1E1E21** α0.86 (o vidro -5 % com o MESMO alfa da decisão do dono), border #2E2E32 α0.55→**#4A3714 α0.60** (a hairline ÂMBAR — o RGB é o do accentDim, a aresta quente do cristal), accent #FFB020 / accentDim / danger / warn / ok INTACTOS + OS DOIS NOVOS: **accent2 #FF6B00** (O FOGO — a base do gradiente dos ativos) e **glow #FF3D00 α0.25** (o brilho da chama) · o BOTÃO ATIVO é a chama (no CHOKE POINT button(), as duas passagens): o glow de foco ATRÁS (2 quads α DECRESCENTE para fora — 4 dp a glow.a×0.8 e 8 dp a glow.a×0.4, a composição simula o blur sem blur real) + o fill com o GRADIENTE VERTICAL accent→accent2 — o painel arredondado com gradiente (panelRoundedGradientV) usa a MESMA escadaria do panelRounded com cada peça AMOSTRADA do span completo (o degradê é contínuo sem costuras; o clip amostra nas posições cortadas — emitGradientV) e o QuadBatch ganha o quadGradientV por vértice (o varying vColor do shader do pass UI interpola DE GRAÇA — ZERO mudanças de shader) · a LINHA DO TOPO DO VIDRO (glassTopLine): 1 dp com o RGB do glow a 20 %, desenhada a seguir a panelRounded/frameRounded (os primitivos de sempre INTACTOS — os painéis de cristal da Y-3 é que a aplicam) · os ALIASES BG/PANEL/LINE sincronizados + ACCENT2/GLOW novos com static_assert · a auditoria de contraste RECALIBRADA HONESTAMENTE (text1 15,4/15,6:1 · text2 7,5/7,6:1 · accent 10,0/10,1:1 · V.ONI no poço 5,8..16,9:1 · a tinta do botão ativo é o BG sobre o accent2 7,0:1 — tudo ≥4,5) · pins ×5 NOVOS (test_fase_y1_tema.cpp — y1_tokens/y1_aliases/y1_contraste/y1_primitivas/y1_gradiente_botao) + MUTAÇÕES M-Y1a (accent2→#F00), M-Y1b (bg de volta), M-Y1c (border α0.55) via GONI_ROOT_OVERRIDE e M-Y1d (o botão deixa de ser a chama — mutação de CÓDIGO compilado): todas VERMELHO→reposto→VERDE, coladas no relatório · recalibrações legítimas: R-028 (a sentinela da identidade: a borda SAI da rampa neutra — É o âmbar do accentDim por contrato; os exatos novos), theme_tabela_spec_g_exata (os exatos + os 2 tokens novos), hierQuadsAmbar (o discriminador do fill é o alfa 1.0 — a borda tem o MESMO RGB com α0.60), os compostos do c33 13.9/18.4 (recalculam via blendOver sozinhos; o literal do fundo 14,14,16→8,8,10 e o sólido 22→21) e o clear dos impostores lê o TOKEN · SEM sentinela R-NNN nova (a identidade já TEM a R-028 recalibrada + os pins novos guardam) · **versionCode 79** — o build vigente da campanha 0.11-Y. O 78 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-Y-Y1.md)
- **0.11-Y · Y-0-FIX — A ESCALA DO G NO LAUNCHER** — o fix do pin C33 (o dono instalou o APK vc 77 e mediu o G a ~25 % do ícone; o contrato é ~65 %): a CAUSA (a bbox de ENQUADRAMENTO do gerador era a do HALO — alpha ≥ 10, o brilho suave do fogo — e o G sólido vivia lá dentro a ~69 % da caixa; no launcher, ~25 %) e a CURA (a bbox de escala passa a ser o CONTEÚDO SÓLIDO — alpha == 1 ⇔ lum ≥ T1=90 na fonte: medido 870×927 de 1254; SEM rim light do tile acima de T0 — a moldura do tile morre nos 6,3 de luminância; o G sólido passa a ocupar 65 % da ZONA SEGURA: 0.65 × 66 dp de 108 dp = 39,7 % do canvas — a diagonal do sólido 56,1 dp < 66 dp e a do halo 71,3 dp < 72 dp do viewport: NADA clipa; o halo acompanha à MESMA escala — o brilho não é espremido — e o CENTRO do paste é o do sólido; o LEGACY idem: o G sólido a 65 % do lado) · os 15 PNGs REGENERADOS (5 densidades × foreground/launcher/launcher_round; os XMLs adaptativos, o values e o manifest INTOCADOS — a ordem do fix: SÓ a escala) · o gerador scripts/y0_gen_icons.py documenta a regra (GLYPH_FILL=0.65 · SOLID_AMIN=255 · a escala geométrica sai no log) · o PIN NOVO y0_escala (test_fase_y0_icone.cpp — pins ×8): MEDE os PNG reais via stb_image (a implementação única vive no PngLoader.cpp da suíte; stbi_load_from_memory — o STBI_NO_STDIO do PngLoader é o contrato): no foreground (fundo transparente) o sólido é alpha ≥ 250 — calibrado 35.9 % do canvas; no legacy (fundo #08080A opaco) é luminância ≥ 90, o mesmo T1 da extração — calibrado 59.4 % do lado; a leitura por threshold fica ~9 % abaixo da geométrica (a borda antialiased do fogo mistura no kernel do LANCZOS) e as janelas [33..39] % / [56..63] % aferram a calibração nos DOIS sentidos (voltar à escala do bug ou saltar acima sai vermelho) · MUTAÇÃO M-Y0-fix (GLYPH_FILL/LEGACY_FIT → 0.25 — a escala do bug de volta): 10 EXPECTs VERMELHAS SÓ no y0_escala (os outros 7 pins aguentam verdes — o isolamento), reposto → 8/8 VERDE, colada no relatório · SEM sentinela R-NNN nova (recursos, não estrutural — os pins guardam) · **versionCode 78** — o build anterior da campanha 0.11-Y. (o relatório próprio: docs/RELATORIO-0.11-Y-Y0-FIX.md)

- **0.11-Y · Y-0 — O ÍCONE NO LAUNCHER** — a 1.ª sub-fase da campanha 0.11-Y (identidade visual; o prompt manda: SÓ o ícone — o tema é Y-1, o layout Y-2, os cartões Y-3, cada uma com CONTINUAR do dono) · a FONTE (docs/iconegonevv.png — o «G em chama» 3D âmbar do dono, 1254×1254 RGB SEM alpha: o glifo vive num tile preto arredondado; a EXTRAÇÃO por luminância T0=40/T1=90 com rampa suave + un-premultiply (o fundo é preto puro — dividir por alpha devolve a cor âmbar; o piso 0.35 limita o ganho de ruído; o tile mede 6–28 de luminância e morre inteiro no T0 — o 1.º corte T0=28 deixava passar a borda do tile como halo fantasma e foi apertado; o anel de brilho que resta é do PRÓPRIO desenho e some nos tamanhos de launcher; o gerador: scripts/y0_gen_icons.py, commitado — a fonte do ícone reproduzível, a regra da casa do gen_app_icon) · o ADAPTATIVO (Android 8+): mipmap-anydpi-v26/ic_launcher.xml + ic_launcher_round.xml compõem o fundo SÓLIDO #08080A (values/ic_launcher_background.xml — a ordem expressa do dono) com mipmap-*/ic_launcher_foreground.png (108/162/216/324/432 — o canvas 108 dp × densidade; o glifo cabe na ZONA SEGURA dos 66 dp: bbox máx = 43 % do canvas, o núcleo sólido do glifo 29–39 %) · o LEGACY (Android 7-): ic_launcher.png + ic_launcher_round.png compostos (fundo #08080A + glifo a 68 % do lado; o round com máscara circular antialiased por supersampling 4×) nas 5 densidades (48/72/96/144/192) · o manifest JÁ apontava @mipmap/ic_launcher{,_round} — attrs INTACTOS (o passo 6 do prompt: não muda nada), só o comentário de proveniência atualiza (a entrada 0.9.6.10 do gen_app_icon.py passa ao histórico; o gone_logo.png do drawable-nodpi NÃO toca — continua a miniatura dos cards/header) · pins ×7 NOVOS (test_fase_y0_icone.cpp: y0_foreground (as 5 densidades, dimensões exatas do canvas, RGBA); y0_background (o #08080A literal); y0_adaptive (os 2 XML compõem @color + @mipmap/foreground); y0_legacy (os 10 PNG, dimensões exatas, o round RGBA); y0_manifest (os 2 attrs do contrato); y0_aapt2 (a FONTE que o aapt2 consome fecha — o manifest cita e os recursos resolvem; a validação BINÁRIA corre no job do APK do CI); y0_antigo_removido (o conjunto de ficheiros EXATO por densidade + os 5 sha256 do gerador 0.9.6.10 capturados no b577dea como impressão digital do desenho antigo — nada sobra SEM ver pixels)) · MUTAÇÕES M-Y0a (foreground só numa densidade → y0_foreground VERMELHO, 4 ficheiros em falta) M-Y0b (manifest com @mipmap/ic_launcher_antigo → y0_manifest + y0_aapt2 VERMELHOS) + a extra M-Y0c (o PNG do gerador antigo de volta no mipmap → y0_antigo_removido VERMELHO pela impressão digital sha256) — todas vermelho→reposto→verde, coladas no relatório · Brand.h/.cpp EXISTEM (0.9.6.18 D6) e ficam SEM alteração de geometria nem paleta: a cor vem do CHAMADOR (a paleta vive no Theme — R-030; tocar no Theme é Y-1) — só o comentário de proveniência atualiza (o launcher deixou de nascer do vetor da marca) · SEM sentinela R-NNN nova (mudança de RECURSOS, não estrutural — os 7 pins guardam) · **versionCode 77** — o build vigente da campanha 0.11-Y. O 76 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-Y-Y0.md)

- **0.11-W · 3.5b — GPU INSTANCING** — a 1.ª sub-fase da campanha 0.11-W (render systems; a abertura: docs/RELATORIO-0.11-W-ABERTURA.md — o baseline do dono: 5 FPS · bl 5164/5178 · 6 TICs com o MESMO mesh, 5178 = 6 × 863 EXATO) · o **caminho instanciado** (o VS do lit ganha `uniform int uInstOn` + o bloco std140 `Instances { mat4 uInst[64]; }` no MESMO padrão do UBO Bones (binding 1, cauda de identidade, upload SEMPRE 4 KB determinístico — o `m = uInst[gl_InstanceID] * uModel` do §3.5b.3; `Renderer::drawMeshInstanced` novo com o uModel IDENTIDADE — a instância leva TUDO — e o estado GL/restaure IGUAL ao drawMesh; `Mesh::drawInstanced` = glDrawElementsInstanced do GLES 3.0 core) · o **BlockMesh** extrai a classificação do frame (`classifyFrame` — o MESMO cull/LOD/z para o draw de sempre e para a colheita, ZERO drift) e ganha `collectVisible` (os picks opacos+blend, NADA desenhado nem carregado — o agrupamento corre DEPOIS do culling e ANTES do upload, §3.5b.2), `drawInstancedGroup` (o lazy é da submissão; o TETO DE 64 fatia em múltiplos draws, nunca falha, §3.5b.3) e `drawBlendPicks` (os blends NÃO se fundem — a ordem longe→perto é por fragmento; desenham DEPOIS de todos os opacos do frame, mais correto para o depth) · o **drawTics** do main vira COLHEITA→AGRUPAMENTO→SUBMISSÃO (a assinatura = mesh+bloco+textura+tint; skinado NUNCA funde — a pose é por TIC; o storage ram/vram soma 1× por mesh — somar por TIC duplicava na cena dos 6 clones) · `DrawStats::instancesMerged` NOVO (a fonte única + o operator+) + o log dos blocos ganha «instanciados=N» · o toggle `MeshRenderer::instancing` (default **ON** — visualmente idêntico, o pin do screenshot manda; «instancing: sim/não» no inspector, o TicSnap transporta, o .goni transporta "instancing":false SÓ quando false — a 0.11-T fica byte a byte) · o stub GL do hospedeiro ganha glDrawElementsInstanced + o bloco Instances no rasterizador REAL (o fb lê o UBO do slot 1 e multiplica uInst[i]×uModel por instância — o pin visual depende disto) · pins ×9 NOVOS (test_fase35b_instancing.cpp: 6_tics_1_dc EXATO; 2_materiais_2_dc; toggle_off = o caminho de sempre; skin_flag_mismatch; lod_mismatch 3+3 → 2 draws com lodDrawn=3; mais_de_64 → 2 draws 64+36 com lastInstanceCount=36; visual_identico PIXEL A PIXEL (6 draws vs 1 instanciado = o MESMO framebuffer); compat_011t (o default ON + o round-trip byte a byte); stats_corretos (os blends NÃO fundem NEM contam)) + o pin da INVESTIGAÇÃO do culling (o §1.8: o far DINÂMICO do editor — SceneBounds+editorClips — nunca desce de 450 u e contém a cena do dono; vista de cima = 24/24 visíveis REPRODUZIDOS; frustum de gameplay far 8 u = 0/24 — O CULLING ESTÁ VIVO, o R-040 confirma-se) · sentinela **R-047** `regress_instancing` (6→1 dc + o toggle + o skin/LOD mismatch + o teto 64) · MUTAÇÕES M-3.5b1 (o caminho instanciado morto → 4 EXPECTs vermelhas) M-3.5b2 (a skin flag ignorada → bmSkin.skinned() vermelho) M-3.5b3 (o teto removido → 2 EXPECTs vermelhas) coladas no relatório · c33 **FASE 28** (+22 checks, 869→891: 6 clones no drawTics REAL → 1 drawElementsInstanced × 6 + instanciados=6/desenhados=6 no BlockFrameStats + o toggle OFF = 2 dc + o skinado ZERO fundição) · test_core 0 falhas (a r039 = o flake POR DISCO da sandbox documentado, VERDE com folga; o CI é o árbitro) · **versionCode 74** — o build vigente da campanha 0.11-W. O 73 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-W-3.5B.md)
- **0.11-W · 3.75a — STREAMING PROGRESSIVO** — a 2.ª sub-fase da campanha 0.11-W (a abertura: docs/RELATORIO-0.11-W-ABERTURA.md): o freeze do import morre por ORÇAMENTO — o diagnóstico (o freeze não era a abertura nem o conversor: é o PRIMEIRO frame visível, o lazy de sempre carregava TODOS os blocos visíveis não residentes NO MESMO frame — 863 leituras+decodes+uploads na cena do dono) e a cura (o `BlockMesh` ganha `kBlockStreamBudgetPerFrame = 8` + `StreamReq` + `streamIn` (ordena por distância ASCENDENTE estável — as CAMADAS, perto primeiro; os residentes e os que FALHAM não consomem a vaga) + `resident` (a PORTA da submissão) + `streamLoad` (o MESMO ensureBlock do lazy de sempre)) · o **drawTics** ganha a fase do STREAM entre o agrupamento e a submissão (os grupos e os blends não residentes pedem vaga com a dist = MÍNIMO sobre os clones; a submissão SÓ desenha o residente — o resto conta `BlockFrameStats::pending` e o log diz «a transmitir=N») · ZERO mudança ao formato .gmesh (as camadas calculam-se da TABELA em runtime — o .gmesh de 203 MB do dono NÃO precisa de reimport) · ZERO mudança aos caminhos de draw (o lazy inline de sempre é o caminho dos chamadores antigos, byte a byte) · o toggle `MeshRenderer::streaming` (default **ON**; com o OFF o TIC sai do batch e desenha individualmente — o MESMO fallback do instancing OFF; «streaming: sim/não» no inspector id 5313, o TicSnap transporta, o .goni transporta "streaming":false SÓ quando false) · pins ×8 NOVOS (test_fase375a_streaming.cpp: sem_freeze_1a_frame (8 de 12 no 1.º frame, pending 24); camadas_perto_primeiro (vaga 3 = os 3 mais pertos; o olho no bloco 11 inverte); budget_por_frame (8→4→0, Σ loads == 12); toggle_off (12 inline, zero instanciados); compat_011t (byte a byte); blend_pending (os blends pedem vaga TAMBÉM, o filtro estável); falha_nao_consome (o CRC corrupto não esfomeia); stats_corretos (a conta fecha)) · sentinela **R-048** `regress_streaming` · MUTAÇÕES M-3.75a1 (o orçamento removido → 8 EXPECTs vermelhas) M-3.75a2 (a ordenação removida → as camadas morrem) M-3.75a3 (a PORTA morta → o inline de volta) coladas no relatório · c33 **FASE 29** (+34 checks, 891→925: o drawTics REAL carrega 8 de 12 no 1.º frame, as camadas pelos entries, a cauda no 2.º frame, o steady state sem loads, o OFF sai do batch) + o 23.2 na CONVERGÊNCIA (o fill do city de 72 blocos por camadas em ~9 frames — o contrato do SAF-SEAM mantém-se no fim) · test_core verde (os 2 flakes DA SANDBOX declarados no §4 do relatório: a r039 por disco e a R-046 intermitente in-suite — ISOLADA 9/9 verde, ASAN limpo, a classe do flake documentado do T-FIX) · **versionCode 75** — o build vigente da campanha 0.11-W. O 74 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-W-3.75A.md)
- **0.11-W · 3.75b — IMPOSTORES** — a 3.ª e última sub-fase da campanha 0.11-W (render systems; a abertura: docs/RELATORIO-0.11-W-ABERTURA.md): o TIC ALÉM da distância dele desenha UM quad com o PNG alfa da silhueta em vez dos blocos — o dc do TIC longe é 1, sempre (o instancing fundiu os clones; os ~863 blocos POR TIC continuavam) · a DECISÃO pura (`render/Impostor.h` — 0 = OFF, o limiar EXCLUSIVO no lado longe, o MESMO contrato do lodActive; o billboard com a normal PARA a câmara — o pin apanhou o verso — e o lado = a maior extensão; degenerados = false, nunca NaN) · a GERAÇÃO SOB DEMANDA (`assets/ImpostorIndex` — o par do ThumbIndex: o PNG nasce em impostors/<stem>.png na 1.ª vez que o TIC fica longe, PELA TABELA com materializeBlock um a um SEM cache até 4096 tris — O PICO DE RAM É UM BLOCO, o contrato R-041; o GUARD do fantasma; SEM placeholder — o impostor falhado NÃO EXISTE e o draw segue os blocos) · o render ALFA (`ThumbRender::renderMeshAlpha` — o MESMO rasterizador software com fundo TRANSPARENTE e lambert CRU; o renderMesh das thumbs fica byte a byte; o `encodePngRgba` color type 6 no ThumbPng) · a textura (`GpuAssets::impostorTexture` — 1 ref = 1 textura, CLAMP/LINEAR, nullptr em falha) · o drawTics ganha o caminho do impostor ANTES da colheita (skinado NUNCA; fora do frustum NADA; sem PNG o fallback honesto; o drawMesh de sempre com DrawProps{alphaMode=1 MASK, baseColor kImpBaseColor 3.699 — a derivação produz a família 0.16+0.62·d da casa SEM tocar no shader}) · o stub rasteriza o MASK (o discard do shader real da 0.11-M — o fragmento descartado não escreve cor nem depth) · o toggle por TIC `impostorDistance` (default OFF — o byte a byte do pré-3.75b; «impostor: N u»/«off» no inspector id 5314; o TicSnap transporta; o .goni transporta "impostorDist" SÓ quando != 0) · o chip «imp N» na status line + o log throttled «impostores: N» · pins ×8 NOVOS (test_fase375b_impostors.cpp: png_alpha (o cru 73 vs o assado 86, o round-trip RGBA); ensure_pico_1_bloco (o fantasma nunca nasce, o maior single read < o ficheiro); decisao_distancia (o limiar exclusivo); billboard (o verso apanhado, os degenerados, a vista de topo sem NaN); quad_mask_pixel (PIXEL A PIXEL: o fundo 14,14,16 fica onde o alpha é 0 e o descartado não oclui); walk_beyond_draws_quad (além = 2 quads + ZERO dc de blocos; a GpuAssets resolve o PNG); fallback_honesto (sem PNG/skinado/PNG corrupto NUNCA há quad); compat_011t (o default OFF, a chave, o negativo não liga, o round-trip)) · sentinela **R-049** `regress_impostors` · a DÍVIDA DOCUMENTAL da **R-047** FECHADA (a entrada completa no REGRESSOES.md — a sentinela da 3.5b existia sem registo) · MUTAÇÕES M-3.75b1 (o MASK morto no rasterizador → o fundo coberto) M-3.75b2 (a decisão morta → o impostor nunca desenha, 4 EXPECTs) M-3.75b3 (o readBytes de volta na geração → o pico morre) coladas no relatório · c33 **FASE 30** (+28 checks, 925→953: o PNG sob demanda no storage REAL, 2 clones → 2 quads + ZERO dc de blocos, o limiar exclusive, o skinado NUNCA, o PNG corrupto → os blocos, a regeneração recupera sozinha) · test_core verde (o flake r039 POR DISCO da sandbox documentado, ISOLADO VERDE; o CI é o árbitro) · **versionCode 76** — o build vigente da campanha 0.11-W. O 75 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-W-3.75B.md)

- **0.11-T-FIX · PERNA 1 — O CONTORNO DA CORRUPÇÃO DO CORTE PARALELO** (campanha P0 — a abertura: docs/RELATORIO-0.11-T-FIX-ABERTURA.md; o bug: heap corruption no `convertGltfToV3` com ≥7 workers, 3 vítimas no CI, o §4.1 da 3.5a) · as TRÊS MEDIDAS: (1) o **teto de 4 workers** (`v3StreamWorkerCount` = clamp(núcleos−1, 1, 4) — o C33 de 8 cores caía em 7 workers, a zona de crash; o override de experimento `GONI_STREAM_WORKERS` força o nº para os pins e para o job streaming-stress da Perna 2 — produção nunca define); (2) o **mutex no onProgress** (`cutPrimitive` serializa a invocação com `progMu` — o callback nunca corre de N workers em paralelo; a verificação single-thread corre sem ele); (3) o **madvise só em mmap por construção** (`fileapi::rangeIsFileBacked` NOVA — a proveniência via /proc/self/maps com cache thread-local de 1 entrada; `fileapi::dropRange` NOVO no PONTO ÚNICO: o madvise só corre em mapeamento file-backed, heap/anónima é NO-OP INDEPENDENTE dos flags dos chamadores — a lição F3 à prova de chamador) · pins ×6 NOVOS (test_fase35fix_p1.cpp: teto 8/16/1024→4; teto_min 2/1/0→1; onprogress_seguro 1000 imports × 8 workers FORÇADOS com fixture de 17000 tris/tarefa (cruza o kProgressStride — os workers disparam o callback de verdade) e detector de sobreposição + aritmética exata; madvise_so_mmap (heap NO-OP byte a byte + mmap de ficheiro legítimo + memfd file-backed); compat_011s (o .goni round-trip byte a byte + o .gmesh write→read→write determinístico); streaming_ok (o data-uri que crashava ×10)) · sentinela **R-046** `regress_streaming_paralelo` (teto + callback seguro 100× 8 workers + madvise + streaming 10×) · MUTAÇÕES M-FIX1a (teto removido → 4 EXPECTs vermelhas) M-FIX1b (guard removido → o pin MORRE com `free(): invalid pointer` — a assinatura EXATA do F3 §2e) M-FIX1c (mutex removido → `stress.overlaps == 0` vermelho) coladas no relatório · test_core 1003 testes 0 falhas (com folga de disco; a r039 sem folga é o flake documentado) · c33_virtual 869/869 · **versionCode 73** — o build vigente da campanha 0.11-T-FIX. O 72 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-T-FIX-PERNA1.md)

- **0.11-T FASE 3.5a · GPU EFFICIENCY — SKINNED CULL + LOD** — a sub-fase 3.5a da campanha 0.11-T (o pin docs/FASE3.5-PIN.md manda; a abertura verificado estado: run 38466867752 verde, vc 71 confirmado no build) · o **SKINNED CULL** (o AnimationSystem salta os TICs skinados cujo AABB inchado 25% (`kSkinCullAabbSwell`) está fora do frustum — NEM advance NEM apply: o tempo do clip congela fora do ecrã e retoma ao voltar; os planos vêm do vp do frame anterior (o vp nasce depois dos systems — o inchamento conservativo cobre 1 frame); o drawTics REPETE o teste com os planos FRESCOS e salta nem skin nem draw; o contador do HUD/log é o do draw (`SkinFrameStats` no DrawStats.h — «skinned: culled N/M», o padrão do chip bl, SÓ nasce com skinados em cena) + o log throttled 1×/s no padrão dos blocos; sem frustum injetado o custo é ZERO) · o **LOD POR DISTÂNCIA** (a geração é do IMPORT: `gmeshDecimateHalf` — parity-weld determinístico, o ímpar solda ao vizinho PAR mais próximo dentro de 25% da diagonal, degenerados caem, os atributos do representante vencem; a cauda **GLOD** no .gmesh DEPOIS do GTB2 — o MESMO padrão de cauda, o payloadSize NÃO conta o GLOD, builds antigas ignoram, `kGmeshV3FlagLOD`=0x4 no meta; índice de entradas 80 B 1:1 por ordem + blobs 16-alinhados nos offsets absolutos, lazy pelo MESMO LRU/orçamentos; o streaming escreve o LOD no temporário durante o corte e emite a cauda no assembly — o pico de RAM mantém-se POR BLOCO) · o draw decide por TIC (`MeshRenderer::lodDistance`, 0 = OFF, o default jogável manda; `lodDistance` >0 + camPos + cauda = a entrada LOD desenha; `Stats::lodDrawn` conta) · a linha "lod distance: N u" no inspector (slider 0..500, "off" no 0 — nunca um 0 silencioso) · o .goni transporta "lodDist" SÓ quando != 0 (a 0.11-S fica byte a byte) · o TicSnap transporta o campo (o undo restaura) · pins ×5 NOVOS (test_fase35a_lod_cull.cpp: skinned_cull 10/5 + tempo congelado + custo zero sem frustum; lod_gera 1024→512 EXATOS + AABB contido + retro; lod_usa perto/longe/off + materialize; lod_off_default (o .goni sem chave + o guard do negativo); compat_011s (o .gmesh sem cauda desenha normal com o LOD "ligado" + o save(load(goni)) byte a byte)) · sentinela **R-045** `regress_lod_cull` (o cull conta certo, o LOD gerado+usado, o default off) com as MUTAÇÕES M-3.5a1 (cull morto → 2 EXPECTs vermelhas) e M-3.5a2 (default on → 2 EXPECTs vermelhas) coladas no relatório · c33 **FASE 27** (+12 checks, 857→869: a cauda GLOD no storage REAL do device, o draw à distância consome o LOD, o default OFF, e os 10 skinados do drawTics REAL com 5 atrás do near → 5 draws + culled=5 + 5 uploads do UBO) · FIX de passagem no harness LOCAL (o awk do build_testcore.sh excluía a ÚLTIMA fonte do test_core — as sentinelas nunca tinham corrido no sandbox; agora correm: 997 testes) · test_core 0 falhas (a suíte inteira; a r039 flutua POR DISCO na sandbox — o pico de artefactos subiu ~40% com a cauda GLOD; a corrida com folga medida: RAM pico 144 MB ≤ 512, 920 blocos, verificado=1; o CI é o árbitro) · c33_virtual 869/869 · gates locais: scope VERDE · docs-lint 87 · gmesh-docs 25 · ui-vocab · theme-hex · glyph · hierarchy · release-identity · **versionCode 72** — o build vigente da fase 0.11-T 3.5a. O 71 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-T-3.5A.md)
- **0.11-S FASE 3 · ESQUELETO/ANIMAÇÃO** — A FASE 3 do dono (o esqueleto completa o que a 0.8.x F7 deixou a meio): as INTERPOLAÇÕES glTF COMPLETAS (`Quat::slerp` novo no Math.h — arco curto com inversão no dot<0 e fallback nlerp nos quase-paralelos; `AnimCurve` ganha Step=2/Slerp=3 + `evalSlerp` exposto puro; o `evalTrack` ramifica; o parse glTF deixa de CORTAR as tangentes do CUBICSPLINE — `GltfAnimSampler.interp` transporta a interpolação e o layout [in,valor,out] chega INTACTO; o `gltfCubicToKeys` converte hermite→bezier da casa EXATO para pos/escala (identidade das bases Bernstein: 3(1−u)²u=3h10, 3(1−u)u²=−3h11) e por diferença finita em euler para rotação — aproximação documentada; LINEAR de rotação vira track SLERP; a timeline mostra o rótulo REAL lin/bez/step/slerp e o tap drop-a para lin; o serializer viaja step/slerp por nome — ficheiros antigos sem "curve" ficam byte a byte; o .gm já transportava a curva por u8) · o **UBO de 128 BONES** (o array uniform uBones[64] morre — não cabia no mínimo garantido do ES 3.0 com 128; `layout(std140) uniform Bones { mat4 uBones[128]; }` = 8 KB ≤ 16 KB garantidos; `setBones` faz glBufferSubData de 8192 B determinísticos POR draw com cauda de identidade — nenhum índice, nem corrupto, sai do bounds no shader — + glBindBufferBase no slot 0; `kMaxBones` 64→128 nos DOIS lados com pin do PAR; skin acima de 128 joints = RECUSA honesta com log que aponta o PLAN B) · o **PLAN B DOCUMENTADO** (palette de textura: 4 texels RGBA32F por bone num sampler2D, fetch no VS por JOINTS_0/WEIGHTS_0 — o design completo §4 do relatório da fase, NÃO implementado: glTF mobile típico <60 bones, anti-over-engineering) · V.ONI **stop()/blend()** (o par do play()/Import.Animation: `stopAnim` — playing=false + tempo 0 + blend cancelado; `blendAnim(nome, seg)` — o crossfade do player com o tempo contínuo; virtuals no Host com defaults honestos «indisponível», EngineHost real, handlers compStop/compBlend, o `blend(nome, seg)` leva o nome CRU (o caminho copy/colorpars) + o seg avaliado; VONI_referencia.md + llms-full.txt REGENERADOS pelo voni_refgen — a bijeção do registo mantém-se) · **FIX do conversor apanhado pela FASE 26**: os clips do .gm nasciam VAZIOS para modelos skinados (o attach dos clips corria num dummy SEM esqueleto — canais de joint caíam fora; o personagem convertido nascia estático para sempre; o skin attach agora precede o clips attach NO MESMO dummy) · a timeline honesta + pins ×12 NOVOS (test_fase3_skeleton.cpp: slerp arco-curto/inversão/paralelos, STEP segura à esquerda, slerp 350°→−5° nunca 175°, cubic==hermite de referência em 5 amostras, import end-to-end das 3 curvas, round-trip do serializer, V.ONI stop/blend no EngineHost REAL com erros legíveis, o PAR dos caps, o BENCH 10 personagens × 64 joints × 300 frames = **0.21 ms/frame de CPU** — 1.3% do orçamento de 60 fps) · pins atualizados (test_skin: cap 128 + o contrato UBO; test_gltfanim: o STEP honesto — scale segura 1.0, nunca 1.5) · c33 FASE 26 (+33 checks, 824→857: o GLB skinado com 2 joints + 2 animações no caminho REAL do device — o .gm irmão, as curvas VIAJANDO no .gm, a pose do slerp a 90°, o crossfade completo com o tempo contínuo, o STEP a segurar a identidade, o draw skinado com uSkin=1 + o UBO de 8192 B + ZERO uploads por array, 10 personagens = 10 draws com 10 uploads do UBO e a medição de CPU da simulação) · test_core 0 falhas (a suíte inteira no harness local, 940+ pins; o disco da sandbox encheu 2× — o flake r039 documentado, com disco limpo 0 falhas; o CI é o árbitro) · c33_virtual 857/857 · gates locais: scope · docs-lint 84 · ui-vocab · theme-hex · glyph · hierarchy · gmesh-docs 25 · projects-ui · reload · release-identity · **versionCode 71** — o build vigente da fase 0.11-S. O 70 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-S-ESQUELETO.md)

- **0.11-M FASE 2 · GLTF MATERIAIS** — O glTF COMPLETO (a spec do dono, os 7 itens da FASE 2): o parse COMPLETO do material (`GltfMaterial` ganha alphaMode/alphaCutoff/doubleSided + os QUATRO factors — baseColor/metallic/roughness/emissive — e por SLOT o `KHR_texture_transform` (offset/rotation/scale) + o índice do sampler; os samplers RAW do glTF em `GltfModel::samplers`) · o bloco **GTB2** no .gmesh (o MESMO padrão de cauda do GTB1: nomes→GTB1→GTB2, entrada fixa de 164 B com os samplers nos CÓDIGOS DA CASA `kSampler*` — a fonte pura é `gltfSamplerToCodes`; ficheiros novos abrem em builds antigas SEM saber, CRC mau = erro legível; documentado no GMESH_formato.md §GTB2) · o LIT ganha as props (`DrawProps` + as uniforms uBaseColor/uAlphaMode/uAlphaCutoff/uTwoSided/uEmissive/uUvRot mat2/uUvOff — o shader aplica o factor, o discard do MASK, o alpha do BLEND, o verso do two-sided e o transform UV; nullptr = o byte a byte pré-FASE 2) · o draw POR BLOCOS em DOIS passes (opacos na ordem da tabela; BLEND longe→perto pelo z de NDC — `BlockMesh::blendDrawOrder` puro, estável; blend liga SRC_ALPHA/ONE_MINUS + depthMask FALSE, two-sided desliga o cull — TUDO restaurado no draw) · os samplers no upload (`Texture::setSampler` first-wins por caminho, 1 linha de log por sampler — o conflito é logado honesto) · o CRITÉRIO do runtime: .gmesh pequeno com materialCount > 1 OU props não-default abre POR BLOCOS (o render POR MATERIAL só existe lá; o modelo pequeno de 1 material sem props fica no mesh único byte a byte) · as THUMBS DE SLOT no inspector (o quad 24dp da textura VIVA via o resolver do elemento Image, o orçamento do texto cede — o padrão MatSelRow; kMaxImageBatches 4→8) · o u32 verificado (accessor 5125 parse+conversão + o bloco use32 round-trip) · pins ×10 (test_fase2_materiais.cpp NOVO: o parse completo, o transform puro == o mat2 do shader, o GTB2 round-trip/retro-compat/CRC/default-de-fora, a ordem do blend, o DrawProps, o u32, A ACEITAÇÃO: 20 texturas (4 materiais × 5 slots) importam+vinculam+as props chegam) · c33 FASE 25 (+27 checks, 797→824: o GLB vidro-BLEND+folha-MASK-two-sided no draw REAL — o GTB2 no runtime, as uniforms no glstub, o blend com depthMask FALSE, o cull do two-sided, os samplers no glTexParameteri) · test_core 0 falhas (a suíte inteira no harness local; r039 = o flake de DISCO da sandbox documentado — o CI é o árbitro) · gates locais: scope · check_main · link_parity · docs-lint 83 · gmesh-docs 25 · ui-vocab · theme-hex · glyph · hierarchy · projects-ui · release-identity · **versionCode 70** — o build vigente da fase 0.11-M. O 69 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-M-MATERIAIS.md)
- **0.11-H FASE 1 · HIERARQUIA** — A ÁRVORE EDITA-SE (a spec do dono, os 5 itens da FASE 1): o drag-reparent (os campos `hierDrag*` eram placeholder desde 0.9.0 — o gesto nasce agora: `UiContext::dragClaim` com a prioridade do SLIDER na zona do ÍCONE DE TIPO (o scroll continua a nascer no resto da linha), o alvo segue o dedo com `updateDragTarget` pós-loop (o flick de 2 frames não perde o alvo), drop no vazio da lista = RAIZ, alvo ILEGAL (ciclo) não acende e o release cancela, o consumo no main revalida com `editor::canReparentTo` e empurra o undo do snapshot, o FANTASMA segue o dedo e o guard do claim órfão limpa o gesto se a linha morrer) + apagar com filhos (`editor::destroyWithChildren` — a corrupção latente do delete simples morre: os filhos nunca ficam com parent para slot reutilizável; o diálogo conta com `editor::countSubtree` e DIZ «e a sub-árvore (N TICs)?»; UM TIC mantém o undo de sempre) + duplicar com filhos (`editor::duplicateWithChildren` — deep clone com novos handles, nomes únicos e estrutura espelhada; o corpo do clone raso é o `cloneTicInto` partilhado — o duplicar de 1 TIC fica byte a byte) + expandir/colapsar persistente (o chevron por TIC com filhos; o estado é POR NOME em `EditorState::hierCollapsed`, persiste no layout.json com as linhas `hierColN=`/`hierCol=` no MESMO debounce G1-5, a pesquisa ignora o colapso, a troca de cena limpa) + ícones por tipo (`hierIconFor` + ScriptComp→Terminal e AnimationPlayer→Clapper; a LUZ é da FASE 5 — não há componente de luz, o placeholder está DECLARADO) · as ops de árvore são PURAS em `ui/UiEditor` (`isDescendant`/`canReparentTo`/`reparentTic`/`collectSubtree`/`countSubtree`/`destroyWithChildren`/`duplicateWithChildren` — o reparent muda SÓ o pai, o TRS local fica: a casa não tem Mat4::inverse/decompose, o «manter transform mundial» é fase futura se o dono quiser) · FIX de passagem: o despacho do tap da hierarquia calculava o iconX em px CRUOS (divergia do draw com densidade ≠ 1 — desenho e toque partilham agora a MESMA fórmula em dp) · pins ×12 (test_fase1_hierarquia.cpp NOVO: a aceitação do neto→outra raiz com os filhos a acompanhar, o undo do pai, o deep clone, o round-trip do layout, o drag REAL com gestos injetados, o ciclo cancela, o tap da multi-seleção sobrevive, o chevron esconde a sub-árvore) · test_core 0 falhas no harness local (a suíte inteira, r039 VERDE com o disco limpo — o flake da sandbox documentado) · gates locais: check_main · link_parity · docs-lint 83 · hierarchy-check (o contrato ganhou a regra 29) · ui-vocab · theme-hex VERDE · c33_virtual não estendido (nenhum caminho do replay tocado) · **versionCode 69** — o build vigente da fase 0.11-H. O 68 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.11-H-HIERARQUIA.md)

- **0.10-M HOTFIX SELEÇÃO-ÚNICA** — O ESTADO CONTRADITÓRIO MORRE (a spec do dono, o vc 67 no C33: linha âmbar com nome em branco + inspector «Nada selecionado» + sem gizmo AO MESMO TEMPO; o id pendurado após troca de prim/mesh ou guarda-restauro de layout; o highlight a sobreviver ao modelo): as causas CONFIRMADAS — 26 escritores diretos de `EditorState::selected` (cada um com a sua semântica de selElement), NENHUMA auditoria do id pendurado antes dos leitores (o gizmo era o 1.º a ler o morto), o inspector a escrever `selected` direto a meio do frame, e a pasta `audio/` a não nascer com o projeto (o `listDir` SAF registava o E fantasma «pasta ausente no projeto SAF: audio» ao abrir o catálogo); a cura nas 5 pernas — (1) `selectTic`/`clearSelection` são os ÚNICOS escritores (os 26 sítios passam pelos dois; a linha âmbar é DERIVADA no draw e inspector/gizmo leem O MESMO estado), (2) `selectionAudit` — id pendurado → clearSelection COMPLETO + os órfãos do multiSelect saem por compação (duas linhas de defesa: o frame do main ANTES do primeiro leitor e a entrada do drawHierarchy), (3) a troca de prim/mesh mantém a seleção (o rebuild é DENTRO do TIC — o pin prova o handle vivo após o pick), (4) a ORIGEM do toque: o clear só com toque NASCIDO no viewport (painel/resize/reclamado nunca limpam; o dividerInput mata o arm do deselect quando o toque nasce na pega), (5) `Project::kDirAudio` + o create nasce com `audio/` + o `SafStorage::listDir` distingue AUSÊNCIA (I honesto) de ERRO (E) · pins ×8 (o contrato do audit; apagar limpa o highlight NO DRAW com a prova VISUAL de ZERO quads âmbar; layout não mexe na seleção; a origem do toque ×4; os três leitores em harmonia; tap na hierarquia → TIC VIVA com nome no frame seguinte; troca prim/mesh mantém a seleção; projeto novo com audio/ e ZERO linhas E) · mutações M1 (highlight por índice próprio → `hierQuadsAmbar == 0` vermelho ×2) e M2 (clear sem origem → vermelho ×3 + o legado) e M3 (draw sem audit → o pendurado sobrevive, vermelho ×2) vermelho→verde · test_core 0 falhas · c33 797/797 (o 17.4 atualizado à lei nova: o conjunto da multi leva TICs VIVAS) · **versionCode 68** — o build vigente da fase 0.10-M. O 67 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.10-M-HOTFIX-SELECAO-UNICA.md)

- **0.10-M HOTFIX P0 SUB-TICs** — O EXPAND EM LOTES (a spec do dono, o travar/crashar no C33): as causas (a)-(d) CONFIRMADAS — o expand INTEIRO corria no frame do `importJobFinish` (thread da UI: N TICs + blockHull + matBindsOf de uma vez = ANR), TRÊS walks subiam pelos pais SEM visitados (`worldChainOf` + o ancestral + a cadeia local no 2.º passe — o pai em ciclo girava para sempre/OOM), a emissão de peças não tinha teto e não havia guard de reentrância; a cura: o `GltfExpandBatch` (o start cria os TICs, as ticks configuram ≤ `kGltfExpandBatchTics`=32 por frame NO PONTO SEGURO com o gancho `perPiece` para o auto-bind; o one-shot fica como wrapper — UMA fonte de verdade, o estado final é o MESMO), os TRÊS walks com conjunto de visitados (o ciclo corta e o log DIZ), o limite declarado `convert::kGmeshV3ExpandMaxTics`=2000 com o aviso claro (`Stats::expandLimited` + toast «LIMITE»), os guardas (sem expand em PLAY; sem import/extract com o expand a correr) e o cancelamento limpo (o MESMO botão do overlay; os TICs nascidos MORREM em ordem inversa); o overlay ganha a fase «A EXPANDIR… materializando sub-TICs: d/t» com a barra pelo lote; (e) os últimos «expandir nós» vivos morrem (log+comentário); (f) o import com a opção ativa continua a criar a sub-árvore no próprio import (nomes do glTF, TRS local por TIC — agora em lotes) · mutações M-P0a (walk sem visitados → **abort std::bad_alloc** no pin do ciclo) e M-P0b (a tick sem lote → 3 asserts vermelhos) vermelho→verde · test_core 947/948 (o 1 = r039, o flake de DISCO da sandbox) · c33 797/797 · **versionCode 67** — o build vigente da fase 0.10-M. O 66 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.10-M-HOTFIX-P0-SUBTICS.md)

- **0.10-M HOTFIX TEXTURAS-MULTIMAT** — A HONESTIDADE DOS SLOTS + O SELETOR DE MATERIAL (a spec do dono, o vc 65 no device): os 4 slots não-baseColor preenchidos mostram o SUFIXO HONESTO «(vinculado · render na 0.11-L)» (a política é a função pura `texSlotRenders`/`inspTexSlotLabel` em EditorLayout.h — fonte única do draw e dos pins; o baseColor SEM sufixo; o nome trunca primeiro, o sufixo nunca sai do ecrã) e a grade INTEIRA dos materiais entra no TIC (`MeshRenderer::matExtraTex` (N−1)×5 + `matCount`/`matTex`/`matTexRef`; o auto-bind do apply/troca/import expandido sonda o GTB1 inteiro; o seletor «material: i/N» com setas ‹ › VETORIAIS — ícone ChevronLeft novo, o atlas não tem U+2039 — entra SÓ com N>1; as 5 linhas mostram O MATERIAL SELECIONADO; o pick escreve NO MATERIAL SELECIONADO — `applyAssetPick` +`materialIndex` com clamp honesto, o 0 byte a byte; serializer «matExtraTex» + undo da grade) · mutações M-MM1 (seletor hardcoded material 0 → ui pin vermelho ×2) e M-MM2 (sufixo removido → 2 asserts vermelhos) vermelho→verde · test_core 944/945 (o 1 = r039, o flake de DISCO da sandbox, verde com disco limpo) · c33 797/797 · **versionCode 66** — o build vigente da fase 0.10-M. O 65 fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.10-M-HOTFIX-TEXTURAS-MULTIMAT.md)

- **0.10-M HOTFIX TEXTURAS-VINCULA-2** — OS 5 SLOTS DO IMPORT E DO PICKER (a spec do dono, o fecho do device): o parse lê OS 5 slots do glTF (`metallicRoughnessTexture` dentro do pbr, `emissiveTexture`/`occlusionTexture` ao nível do material — o parse só lia baseColor+normal e os outros 3 nunca chegavam ao GTB1); o `GpuAssets::matBindsOf` (novo, o `baseColorBindOf` vira wrapper) devolve OS CINCO binds do material e o auto-bind do apply/`applyAssetPick` + do import EXPANDIDO enche os 5 slots do TIC (a troca de mesh leva os slots atrás — o vestígio morre); a causa (d) do picker: o ramo do `TexSlotRow` no dispatch do Inspector era **CÓDIGO MORTO** (faltava na condição externa) — o tap nas 5 linhas não fazia NADA e o único caminho era a linha clássica que arma o 0 («o picker escreve sempre no baseColor») · mutações M-TB2a (write só 1 slot → 8 asserts vermelhos) e M-TB2b (pick rejeita não-base → 2+8 testes vermelhos) vermelho→verde · test_core 935/0 · c33 797/797 · **versionCode 65** — fica INTACTO como história. (o relatório próprio: docs/RELATORIO-0.10-M-HOTFIX-TEXTURAS-VINCULA-2.md)

- **0.10-M HOTFIX TEXTURAS-VINCULA** — O IMPORT LIGA AS TEXTURAS AO MESH (a spec do dono): o bloco **GTB1** no fim da secção de materiais do `.gmesh` (material→5 slots→.gtext; os leitores de sempre param nos nomes — retro-compat total); o `GpuAssets::blockHull` resolve a baseColor por material e o draw POR BLOCO usa a textura DO SEU material; o Inspector ganha a secção Material com os 5 slots (o pick escreve O SLOT — os irmãos intactos); «expandir nós» passou a «expandir em sub-TICs» (e) e a sub-árvore do import é REAL — `parentRec` + TRS local + o `TransformSystem` compõe parent∘local (f, a dívida do F3 paga) · mutações M-TB1/M-TB3 vermelho→verde · test_core 932/0 · c33 797/797 · **versionCode 64** — fica INTACTO como história.

- **G0** fccf1b1 — CI run 37215555172 (5/5 jobs, 1ª tentativa) · artefacto **goni-vv-0.9.5-release-signed** confirmado.
- **0.10-M PASSO 5C** (este commit) — THUMBNAILS PRÉ-RENDERIZADAS (o sub-passo 5C do PASSO 5, a spec do dono): o import escreve `thumbs/<stem>.png` 256×256 POR MESH (o render SOFTWARE puro de `ThumbRender` — câmara isométrica fixa, z-buffer, lambert com a luz da casa, determinístico; teto de 4096 tris; placeholder D18 com as iniciais); o browser lê os PNGs PRONTOS (`ThumbIndex::loadRows` — 100 thumbs em 15,6 ms no hospedeiro, 0,5 ms sob SAF no c33 FASE 24; piso do dono <500 ms); o render sob demanda lê a TABELA de blocos com o peek POR FAIXA 192 B (a lição R-044 — o `readBytes` inteiro media 1965 MB no gigante de 50M verts; a faixa volta ao 99 MB) e o asset fantasma nunca nasce thumb; R-044 + as mutações TH1..TH4 vermelho→verde (o relatório próprio: docs/RELATORIO-0.10-M-PASSO5C-THUMBS.md) · **versionCode 63** — o build vigente da fase 0.10-M. O 62 fica INTACTO como história.
- **0.10-M PASSO 5A** (este commit) — TEXTURAS: COMPRESSÃO E REDUÇÃO SEPARADAS (o sub-passo 5A do PASSO 5, a spec do dono): a política por resolução vive em `TexturePolicy::texProfileCapMaxDim` (512/1024 mantém · 2048→1K nos perfis limitados · 4096→2K/1K · 5120+ conforme perfil); TRÊS perfis por projeto (`texPerfil=0/1/2` em settings.goni — Qualidade sem redução [default], Equilibrado, Mobile) com OVERRIDE por asset a vencer (`textures/overrides.goni`); as linhas de log DISTINTAS («reduzida A→B (perfil/override/teto do device)» ≠ «comprimida ASTC 4x4 (N KB)»); mips filtrados em ESPAÇO LINEAR (`MipGen::SrgbLinear` — a prova do 50%: 188 vs 127) + flags sRGB/normal no .gtext v2/.gtc v2; normal maps do glTF → RGBA8 SEM PERDA de canais (byte a byte); GL_MAX_TEXTURE_SIZE do boot → mip mais baixo com aviso, NUNCA erro; o passe de texturas do conversor em POOL de 4 workers FORA da UI; cache com CHAVE POR PERFIL (a mesma textura nos 3 perfis = 3 ficheiros: 21 845 / 5 461 / 1 365 KB) (o relatório próprio: docs/RELATORIO-0.10-M-PASSO5A-TEXTURAS.md) · **versionCode 62** — o build vigente da fase 0.10-M. O 61 fica INTACTO como história.
- **0.10.6 HOTFIX SAF-SEAM** (este commit) — AS DUAS ÚLTIMAS COSTURAS DO SAF: o `BlockMesh::open` do RUNTIME usa a MESMA cascata do conversor (`openReadFd` o FD DO BRIDGE → `mapFd64` serve as 3 leituras com ZERO ranges → pread NO MESMO fd → os ranges; a linha `gmesh: fase=load … fonte=<mmap-fd|pread-fd|ranges>` — o par do conversor; em falha o err diz QUAL das 3 leituras + o errno + o tamanho VISTO); os MATERIAIS sem tamanho leem-se NOME A NOME (a sonda de 256 B matava caudas curtas); o overlay de import NUNCA «0 / 0» (length=0 → «a copiar…»/«copiando… N B»; sub-MB → B; o pin do 29 MB); a QUARENTENA do asset corrompido (`ProjectStorage::rename` NOVO: Fs `::rename` atómico, Saf a cópia streaming por fd + remove com a cache de URIS a seguir o nome) + «asset corrompido, reimporta» no err/toast/engine.log — o picker MANTÉM o estado anterior (nunca a bola silenciosa; R-043 + as mutações M-SS1/M-SS2/M-SS3) (o relatório próprio: docs/RELATORIO-0.10-M-HOTFIX-SAF-SEAM.md) · **versionCode 61** — o APK que o dono instala para o sign-off do C33 (3 itens, §8 do relatório do hotfix). O 60 acima fica INTACTO como história — a declaração do build VIGENTE vive aqui.
- **G1** 0c543eb — run 37217383561 (1ª) · suíte 804/0 + 273/273 (FASE 12.1-12.3).
- **G2** 21a4798 — run 37219059113 (1ª) · 808/0 + 282/282 (12.4-12.6) · R-010.
- **G3** 11bd340 — run 37219675961 (após o flake do R-005 JVM) + CI-fix **0405836** run 37220797721 · 808/0 + 290/290 (12.7).
- **G4** 55bae05 — run 37220693071 · 810/0 + 295/295 (12.8) · R-014.
- **G5** 6d8e68f — run 37222823026 (1ª) · docs-lint VERDE (46 ficheiros) · R-016.
- **G6** 2011771 — run 37225827600 · **817/0 core + 316/316 harness** (12.9) · LINK PARITY **105 TUs** (core/Bench.cpp o 105º) · R-017.
- **FECHO** (este commit) — versionCode 49/0.9.6 · este relatório · README 0.9.6.
- **HOTFIX 0.9.6.18** a277d0d — os 12 defeitos D1–D12 + a captura D7 + os pins/gates (o relatório próprio: docs/RELATORIO-0.9.6.18-HOTFIX-12-DEFEITOS.md) · **versionCode 50** · o run do a277d0d ficou VERMELHO em dois degraus ESCALONADOS (o 2.º tapado pelo 1.º): (i) o javac do InitialsThumbView (2 erros — o setStroke que não existe em Paint + o density() de contexto static) e (ii) ESTE gate R-015 (o bump 49→50 não tinha declaração aqui). **0.9.6.18b** c1b58c8 cura (i) (ECJ nível 8 na sandbox: a classe fixada compila, a do a277d0d reproduz os DOIS erros do CI palavra a palavra; o bordo do tile volta a STROKE — o grafite do contrato D7) e ESTA LINHA declara (ii): **versionCode 50** — o APK que o dono instala para o re-sign-off do C33 (13 itens, §8 do relatório do hotfix). O FECHO acima (versionCode 49) fica INTACTO como história — a declaração do build VIGENTE vive aqui.
- **HOTFIX 0.9.6.19** (este commit) — OS ABERTOS DO RE-SIGN-OFF + A CÂMARA GIGANTE + OS RESTOS (o relatório próprio: docs/RELATORIO-0.9.6.19-HOTFIX-ABERTOS-DO-RESIGN.md) · **versionCode 51** — o APK que o dono instala para o re-sign-off do C33 (7 itens, do relatório 0.9.6.19). O versionCode 50 acima fica INTACTO como história — a declaração do build VIGENTE vive aqui.
- **HOTFIX 0.9.6.19b** — O HOTFIX B: OS 4 CHECKS DO 0.9.6.19 «4/4 PASS» NO C33 + m1/m2/m3 + D20 (a câmara ainda mais pequena, o contrato medível) + D21 (o [+] do viewport morre) (o relatório próprio: docs/RELATORIO-0.9.6.19b-HOTFIX-B.md) · versionCode 52. O 52 acima fica INTACTO como história.
- **0.10-M EXT** (este commit) — IMPORT DE NÓS COMO SUB-ÁRVORE (o BACKLOG 0.10-M-ext do dono): a opção «expandir nós» cria UM TIC POR NÓ COM MESH (nomes do glTF preservados, a transformação do nó no Transform3D — o TRS MUNDO decomposto, a geometria da peça CRUA no espaço local; o shear cai no bake com TRS identidade); default FUNDIDO intocado (o caminho vigente byte a byte idêntico); cada peça é um `assets/<stem>_<nó>.gmesh` com `kGmeshV3FlagPiece` (bit1 do meta v3) que o `GpuAssets::mesh` abre POR BLOCOS mesmo pequena — o culling por TIC do pin (`c33` FASE 22: o city espalhado = 72 TICs com nomes, o frustum culle 71 e deixa 1); a sub-árvore entra sem diálogo «aplicar ao TIC?» com o fit único; a mensagem da pele aponta o BACKLOG 0.10-A pelo nome; R-042 + as mutações M-E1..M-E4 (o relatório próprio: docs/RELATORIO-0.10-M-EXT-IMPORT-NOS.md) · **versionCode 60** — o APK que o dono instala para o sign-off do C33 (3 itens, §8 do relatório do EXT). O 59 acima fica INTACTO como história — a declaração do build VIGENTE vive aqui.
- **PASSO 4** (este commit) — AS JANELAS (o BLOCO B da spec de layout): logs opacos 80% com corte/scroll/wrap + menus ancorados com fundo 40% (o token scrimMenu) + settings linhas 36dp + os cantos suaves fechados (o contrato J-01..J-04, §2.25-§2.28 do LAYOUT_HIERARCHY) (o relatório próprio: docs/RELATORIO-0.9.6.20-PASSO4-JANELAS.md) · **versionCode 53** — o APK que o dono instala para o sign-off do PORTÃO B (4 itens, §7 do relatório 0.9.6.20). O versionCode 52 acima fica INTACTO como história — a declaração do build VIGENTE vive aqui.
- **0.10-M HOTFIX SAF-STREAM** (este commit) — O STREAMING SOB QUALQUER STORAGE: o gate `content://` que desligava o streaming nos projetos SAF MORREU (a causa do city: o import caía no legado do teto de 65535); a fonte do bin em CASCATA — mmap-caminho (o de sempre) → **mmap POR FD** (o fd do bridge `bridgeOpenFd` sobre o irmão copiado em source/ — `mapFd64` NOVO + `ProjectStorage::openReadFd`) → **pread de RANGES** (o degradado honesto quando o provider recusa o mapa — o pico é o span da tarefa em voo); o data: URI também streama (o buffer 0 embutido sai do parse); `gmesh: fase=parse` só o JSON + `fase=ranges` em linha própria (os 5395 ms do dono com dono); o teto de 65535 deixa de ser VISÍVEL ao não-skinned e a pele acima do teto diz «pele ainda não suportada no streaming (BACKLOG)» (o relatório próprio: docs/RELATORIO-0.10-M-HOTFIX-SAF-STREAM.md) · **versionCode 59** — o APK que o dono instala para o re-sign-off do C33 (2 itens, §8 do relatório do hotfix). O 58 acima fica INTACTO como história — a declaração do build VIGENTE vive aqui.
- **0.10-M PASSO 4** (este commit) — O RENDER POR BLOCOS: o .gmesh v3 abre pela TABELA (192 B + faixa da tabela + faixa dos materiais — ZERO bytes dos dados), o frustum AABB escolhe os blocos (audit puro), o lazy carrega na 1ª visibilidade, a cache LRU respeita os orçamentos DECLARADOS `kBlockCacheRamBudgetBytes`/`kBlockCacheVramBudgetBytes` (64+192 MB — o mesmo 256 MB da casa, limitado por construção), o hull de bounds preserva o contrato do picker/serializer, o OBJ export anda pela tabela (pico = 1 bloco) e o HUD diz «FPS · TICs · bl n/m» (o pino dos 920 blocos: orbit realista pico 0 KB; pior caso 10,6 MB < 19 MB do modelo; R-040 + as mutações M1/M2/M3) (o relatório próprio: docs/RELATORIO-0.10-M-PASSO4-BLOCOS.md) · **versionCode 58** — o build vigente da fase 0.10-M. O 57 fica INTACTO como história.
- **0.10-M PASSO 3B** (este commit) — LOG + CAUSA DA IMPORTAÇÃO (o hotfix do «log de ontem» e do «fail de 203 MB»): o readTail lê o ficheiro ATIVO primeiro (o bug do viewer stale morreu — com rotações o backup mais antigo enchia a janela e o log de HOJE nunca era lido); as linhas contrato `gmesh: fase=parse|cut|assembly|verify|load|render ms=<n>` + `import: copia ms=<n>` dão dono por fase aos minutos do dono; a causa do load registada com números — «memória insuficiente ao carregar mesh (cura no PASSO 4: render por blocos)» (o orçamento `kMeshLoadBudgetBytes` = 256 MB) e a falha CHEGA ao engine.log pelo elog do GpuAssets (o LOGE só falava com o logcat) (o relatório próprio: docs/RELATORIO-0.10-M-PASSO3B-LOG.md) · **versionCode 57** — o build vigente da fase 0.10-M. O 56 fica INTACTO como história.
- **0.10-M PASSO 3** (este commit) — O CONVERSOR STREAMING E PARALELO: glTF/GLB → .gmesh v3 por mmap, corte por material em blocos (pool de núcleos−1), assembly ordenado e verificação POR TRIÂNGULO bit a bit (`gmesh: v3 blocos=<n> ... verificado=1`); a parede do 65 535 MORREU (a sentinela R-039 converte 50 M vértices / 1 754 MB com pico de 107 MB) (o relatório próprio: docs/RELATORIO-0.10-M-PASSO3-CONVERSOR.md) · **versionCode 56** — o build vigente da fase 0.10-M. O 55 fica INTACTO como história.
- **0.10-M PASSO 3 · PREPARAÇÃO** (fronteira de sessão) — o modo STREAMING do `parseGltf` (`streamMeshes=true`: os nós/materiais/anims/skins parseiam, a GEOMETRIA não materializa; as `GltfPrimRef` guardam as refs de primitiva por nó) — a infraestrutura do conversor em streaming e paralelo que o PASSO 3 completa; ZERO mudança de comportamento no caminho vigente (o default é o parse integral de sempre; test_core 0 falhas). **versionCode 55** — o build vigente desta fronteira. O 54 fica INTACTO como história.
- **0.10-M PASSO 2** (commit anterior) — O FORMATO v3: o escritor do .gmesh só escreve v3 (header 64-bit, blocos ≤65,535 verts configuráveis com índices locais u16/u32, CRC32 por bloco e na tabela, layout de atributos descrito no header, alinhamento a 16, mmap-ável) e o LEITOR abre v1+v2+v3 (a cláusula do dono: «quebrar isto é vermelho por definição» — as fixtures reais v1/v2 commitadas + R-038) (o relatório próprio: docs/RELATORIO-0.10-M-PASSO2-FORMATO.md) · **versionCode 54** — o APK vigente da fase 0.10-M. O versionCode 53 acima fica INTACTO como história — a declaração do build VIGENTE vive aqui.
- **0.12-D · D-3 — O FOGO (a borda animada dos ativos, o glow por baixo, as brasas)** — a identidade da chama no que está ATIVO: o `ui/Fire.h` NOVO (a FONTE ÚNICA com 3 consumidores — o FS replica linha a linha, o stub inclui o header, os pins chamam direto; `fireBorder` usa o `roundRectSdf` da D-1 — zero duplicação), a RAMPA do dono no Theme (fire0 #7A1F0A → fire3 #FFE08A; a proveniência é o scripts/d3_palette.py NOVO contra o ícone REAL — a família quente R≥G>B confirmada), a BORDA DE FOGO animada do ativo (o aShape.w=1 no batch SDF, o fbm de 2 oitavas a correr pela aresta em MISTURA ADITIVA GL_SRC_ALPHA/GL_ONE), o GLOW POR BAIXO (2 quads α decrescente), as BRASAS com PORTÃO (16 partículas paramétricas; `celebration=false` emite ZERO — o editor NUNCA as vê; a splash é da D-6 e o projetos da D-8), a COMPOSIÇÃO SIMULTÂNEA do premido (glow de foco + glow por baixo + widget + chama + bisel + especular + sombra NO MESMO frame — a cláusula anti-superficialidade), o relógio sdf::g_time no main loop e o TOGGLE da D-7 a morder os três emissores (Baixo = sem borda/glow/brasas) · pins ×5 NOVOS (test_012d_fogo.cpp: fogo_rampa/fogo_borda_anima/fogo_glow_below/brasas_portao/fogo_toggle_baixo — o anima prova a ARITMÉTICA ADITIVA por píxel) + M-D3a (o fbm morto → Δ=0.0 vermelho) e M-D3b (o portão removido → 2 EXPECTs vermelhas) vermelho→reposto→verde (coladas no RELATORIO-0.12-D.md §3.3) · OS ACHADOS REAIS (o «não confia no build»): o continue que saltava o blend (o fogo nunca chegava ao fb), o 0x0304 no lugar de GL_SRC_ALPHA (o aditivo nunca ligava), a banda SEM TETO para dentro (o widget INTEIRO ardia — Δ73 no centro morto; a fórmula certa clamp01(min(1−d, 1+d/bw)) replicada nos 3 lados), a duplicação do SDF no stub substituída pela fonte única e o «play em destaque» da C-2 registado como inalcançável no replay (a FASE 33 usa o gesto REAL no Stop da play bar) · recalibração com nota: y1_gradiente_botao (a chama distingue-se pelo aShape.w; o gradiente do widget INTACTO) · FASE 33 NOVA do c33 (4 checks) · verificação: test_core 0 falhas (os 2 skips de disco conhecidos), c33_virtual 1014/0 · **versionCode 93** — o relatório próprio: docs/RELATORIO-0.12-D.md §3.
- **0.12-D · D-4 — O LOGO (o «G em chama», a identidade única)** — o glifo da marca reescrito pela ordem do dono: as 4 SETAS do «G-com-4-setas» MORRERAM nos dois LODs (o ícone real nunca as teve), a cor por traço é a RAMPA fire0..fire3 amostrada pela ALTURA (a base escura, o bico amarelo — o âmbar MONO morreu), o `ignite` 0..1 revela a rampa da base para o topo (o parâmetro que a splash da D-6 anima), o LOD vive no traço (×1.2 no <32dp) e as 3 versões do dono ficam servidas: PEQUENA (o cabeçalho do editor — o slot do LOGO), MÉDIA (o launcher da Y-0, intocado) e GRANDE (a splash da D-6) · o ACHADO da auditoria: a tabela da barra tinha 4 pontos e o loop desenhava 3 segmentos com o do MEIO a re-tracear a barra ao contrário (overdraw puro desde sempre; o Java já desenhava 2) — geometria certa, 16 segmentos · o espelho Java alinhado (UiIcons.drawBrand sem setas com a rampa por segmentos + o ignite; os tokens FIRE0..3 no espelho do Theme; o projects_ui_check aceita os 4 hexes — a edição é o pedido) · pins ×4 NOVOS (test_012d_logo.cpp: logo_rampa_viva/logo_ignite_porta/logo_setas_mortas/logo_lod_traco) + M-D4a (a rampa morta → o fóssil mono apanhado quad a quad) e M-D4b (as setas de volta → 20 quads + vértices a r=8.29) vermelho→reposto→verde (coladas no RELATORIO-0.12-D.md §4.3) · recalibração com nota: a D12 (8) — a contagem IGUAL nos LODs, o LOD no traço · FASE 34 NOVA do c33 (2 checks) · verificação: test_core 0 falhas (os 2 skips de disco conhecidos), c33_virtual 1016/0 · **versionCode 94** — o relatório próprio: docs/RELATORIO-0.12-D.md §4.
- **0.12-E · E-1 — A RESTAURAÇÃO DA UI BÁSICA (abortar SDF 3D)** — a ORDEM DE EMERGÊNCIA do dono: o shader SDF único de widget (0.12-D D-1) estragou o TEXTO e o LAYOUT no device real (texto invisível, painéis sem tamanho, caixas pretas flutuantes) e o desenho volta ao SIMPLES 2D no UiContext — o CHOKE POINT drawButtonVisual e o iconButton regressam ao padrão Y-1 (o glow de foco + o panelRoundedGradientV accent→accent2 no ativo; o panelRounded + o frameRounded 1dp no repouso), o VIDRO REAL (D-2) sai do frame (o bloco de captura glReadPixels+Kawase do main.cpp inteiro) e o VIDRO FALSO sai dos painéis (a hierarquia/inspector voltam ao ui.panel + glassTopLine), os 6 menus voltam ao panelRounded ELEVADO/PANEL + glassTopLine; a maquinaria SDF/vidro/fogo fica DORMENTE (zero chamadores no produto — ui/Sdf.h, SdfBatch.h, Glass.h, Fire.h, progSdf_, captureGlass, os 6 emissores do UiContext) e o LOGO de D-4 SOBREVIVE (desenho por segmentos 2D, nunca foi SDF); ZERO mexidas no FontAtlas e na arquitetura de TICs (as cláusulas do dono) · pins REFORMADOS com o caminho morto (test_012d_sdf/vidro/fogo APAGADOS — 13 pins; as FASE 32/33 do c33 saem com nota — 9 checks; 1016 → 1007) + recalibrações DE VOLTA com nota no sítio (playui Stop no solids, y1_gradiente_botao inteiro à versão solids, y3 cristal 1dp, regress_cantos_suavizados escadaria, cameratic solids, 13.9 vidro liso) · verificação: test_core 0 falhas (o skip de disco conhecido), c33_virtual 1007/0, os 12 gates locais verdes · **versionCode 95** — o APK da restauração; o 94 fica INTACTO como história (o relatório próprio: docs/RELATORIO-0.12-E.md §1).
- **0.12-E · E-2 — O TEMA FOGO AVERMELHADO (a paleta EXATA do dono)** — a ordem: «Atualiza o ui/Theme.h com esta paleta EXATA»: o `surface` #131316 → **#141416** (o PAINEL; o α0.80 do vidro mantém até a FASE 3 subir a 0.85), o `text1` #ECECEE → **#F2F2F5** (o texto primário — e o `voniUser` segue o text1, o contrato de sempre), os tokens NOVOS `fireRed` **#D92B04** (brasa escura) e `fireOrange` **#FF5E00** (brasa quente) — o combustível do gradiente dos ativos da FASE 3; o `bg` #0A0A0C (obsidiana) e o `accent` #FFB020 (âmbar, o brilho de destaque) já eram EXATOS e ficam; os gates aferram a fonte única: os static_asserts do UiContext.h (PANEL/TEXT desincronizados = build VERMELHO — provado no local) e os 2 pins de varredura de fonte recalibrados com nota (y1_tokens/superficies_4_tons contam o `VV_RGBA(20, 20, 22, 0.80f)` único) · recalibrações com nota: test_toolbar (surface[0] 20 + o voniUser==text1), test_sentinels (a identidade grafite+âmbar com o painel novo), test_fase_y1_tema (o token do painel), test_012c_idioma (superficies_4_tons — o #141416 na escada) · theme_hex_check VERDE (0 hex fora do Theme) · verificação: test_core 0 falhas (o skip de disco conhecido), c33_virtual 1007/0 · **versionCode 96** — o build vigente da 0.12-E. O 95 fica INTACTO como história (o relatório próprio: docs/RELATORIO-0.12-E.md §2).
- **0.12-E · E-3 — OS EFEITOS 2.5D (só desenho 2D) + O FECHO DA CAMPANHA** — as 4 receitas da ordem do dono, TODAS SIMULTÂNEAS no premido: (3.1) O VIDRO — o `surface` α0.80 → **α0.85** (o fundo escuro dos painéis) + a LINHA 1dp do topo com o ÂMBAR a 20% (`glassTopLine` troca o RGB do glow da Y-1 pelo `accent` #FFB020 — o reflexo da luz); (3.2) A PROFUNDIDADE FALSA — o retângulo preto **α0.15** deslocado **3dp** por baixo de CADA botão (o token `dropShadow` NOVO no Theme; o deslocamento em dp — a lei R-018 — o 3px do dono à densidade de referência; o CHOKE POINT drawButtonVisual emite-a em repouso E no ativo); (3.3) OS ATIVOS — o gradiente vertical **fireRed→fireOrange** (a paleta da E-2 finalmente consumida; o y1_gradiente_botao recalibrado com nota — o topo 217/43/4, o fundo 255/94/0, a continuidade pela faixa g 43..94); (3.4) O FOGO ANIMADO — a textura 2D de ruído em SCROLL LENTO (o `ui/Noise2D.h` NOVO — a FONTE ÚNICA: value noise 64×64 TILEÁVEL determinístico, o hash embrulha na célula 8×8 e a emenda do REPEAT fecha por construção; `fireScrollOffset` = 6 px/s, o mesmo t = a mesma borda) + o `UiContext::fireBorder2D` (as 4 bandas 2dp no batch de imagens, o vColor = fireOrange, o FS amostra tex.r) + a textura no `Renderer::init` (GL_REPEAT, POT 64) + o rasterizador honesto do stub HONRA o REPEAT (o `glTexParameteri` escreve o wrap na textura ligada; sampleR/sampleRGB fazem o fract) · pins ×4 NOVOS (test_012e_efeitos.cpp: sombra_25d — a coluna central preta do offset no repouso E no ativo + o token; vidro_85_topline_ambar — o α0.85 + a linha âmbar pelo emissor real; fogo2d_scroll — o caminho REAL com o fb: a banda do topo com o u0 = fireScrollOffset(t) exato em t=0 e t=2; fogo2d_determinismo — o tile perfeito (x+64,y)==(x,y), o wrap do offset, a suavidade do grão) + M-E3a (a sombra removida → sombra_25d vermelho) e M-E3b (o scroll morto → fogo2d_scroll + fogo2d_determinismo vermelhos) vermelho→reposto→verde · a FASE 35 NOVA do c33 (3 checks: a sombra no replay, a linha âmbar no replay, a textura viva) — 1010/0 · recalibrações com nota: y1_tokens/superficies_4_tons (o α0.85 no fonte-única), theme_tabela_spec_g_exata/regress_identidade (o α), y1_primitivas/y3 cristal ×2 (a linha âmbar), y1_gradiente_botao (o gradiente fogo) · verificação: test_core 0 falhas (o skip de disco conhecido), os 12 gates locais verdes · **versionCode 97** — o build vigente; o 96 fica INTACTO como história (o relatório próprio: docs/RELATORIO-0.12-E.md §3)
- **0.12-U · 3.9a — VIEW P UNIVERSAL + A MATEMÁTICA DO REGISTO (o Bloco A abre)** — a campanha da REDUÇÃO DE TRABALHO do autor da V.ONI: o `View P` aceita EXPRESSÃO dos 7 tipos (a gramática `ViewStmt <- 'View' IdentAny (CallTail / Expr)?` — o CallTail tenta-se primeiro, a string de sempre segue o caminho de sempre, compat byte a byte; o log formata via `valueText()`), as 14 FUNÇÕES do registo (`Kind::Funcao` NOVO — abs/min/max/clamp/lerp/floor/ceil/round/sqrt/sin/cos/random/random_int/distance; sin/cos em GRAUS, a convenção da casa; o despacho no `evalCall` consulta `reg::find` + o kind — o nome só existe se a tabela o disser; o argc afere-se pelos minArgs/maxArgs da ENTRADA; a fn do utilizador VENCE — o shadow da casa) · pins ×15 NOVOS (test_voni.cpp: view_p_012u ×3, mat_ ×10, distance_tic_e_vec3 — o eixo z puro fecha a vaca) + **R-052** (`regress_voni_matematica`) + M-3.9a1 (o kDegToRad → 1.0) M-3.9a2 (o View P de só-Txt) M-3.9a3 (a distance sem o z) vermelho→reposto→verde (coladas no RELATORIO-0.12-U-3.9a.md §3) · verificação: test_core 0 falhas (os skips de disco/sandbox conhecidos), c33_virtual 1010/0 · **versionCode 98** — o build vigente da 0.12-U. O 97 fica INTACTO como história (o relatório próprio: docs/RELATORIO-0.12-U-3.9a.md).
- **0.12-U · 3.9b — SPAWN: O SCRIPT CRIA TIC NA CENA (o Host ganha a porta de criação)** — `spawn(tipo, x, y, z) → TIC`: a entrada nova no registo (Kind::Funcao, argc 4/4, Docs completa — a FONTE ÚNICA), o despacho no `evalCall` → `evalSpawn` (o TIPO é NOME CRU — o padrão dos slots de nome do tykerwire; o argc vem da ENTRADA), o `Host::spawnTic(tipo, x, y, z, outName, err)` NOVO com o default «indisponível neste hospedeiro» (o padrão do stopAnim/blendAnim — o host sem cena é honesto, nunca crash) e a `EngineHost::spawnTic` real: a lista VIVA esfera/box (a decisão 0.8.10; o erro ENSINA os tipos — o primFromName de LOAD é defesa do .goni antigo, não porta), o TIC nasce ACTIVE/VISIBLE com Transform3D na pos + MeshRenderer com a primitiva (o corpo do preset Mesh) e o NOME ÚNICO base.001 (a convenção do Presets/uiEditor — o find é por nome, o colidido é fantasma) · pins ×9 NOVOS (6 VM no test_voni.cpp + 2 ENGINE REAL no test_wiring092.cpp — active/visible/prim kind/nome único/exported + o popError com a lista e NADA nasceu — + 1 no host sem cena) + **R-053** (`regress_spawn`) + M-3.9b1 (o despacho fora) M-3.9b2 (o nome único fora) vermelho→reposto→verde (coladas no RELATORIO-0.12-U-3.9b.md §3) · verificação: test_core 0 falhas (os skips de disco/sandbox conhecidos), c33_virtual 1010/0 · **versionCode 99** — o build vigente da 0.12-U. O 98 fica INTACTO como história (o relatório próprio: docs/RELATORIO-0.12-U-3.9b.md).
- **0.12-U · 3.9c — EVERY / AFTER / WHEN: OS RELÓGIOS DA LINGUAGEM (o estado é por nó AST)** — a nova sintaxe no AST + parser (o `TimerStmt <- EveryStmt / AfterStmt / WhenStmt` na gramática — o ficheiro único 🔶; o `Stmt::Kind::Timer` com `TimerKind` no AST; as ações no VoniCompile pela lição FASE 9 — ação por VALOR; as 3 palavras RESERVADAS) e as 3 entradas no REGISTO (Kind::Linguagem — Docs/equiv completos); o ESTADO POR NÓ AST vive no `Script::Impl::timerStates` (`unordered_map<const Stmt*, TimerState>` — o program é estável, o reset() do stop() limpa, ZERO static/global: o de dois scripts não se toca e o do mesmo script morre com ele); os contratos fechados (decisões 🔶): o **every** carrega o RESTO do período (sem drift) e dispara no máx. 1× por frame, o **after** dispara 1× desde o 1.º encontro da linha, o **when** dispara 1× TOTAL no 1.º frame verdadeiro; o corpo NÃO é ciclo (o resume/continue referem-se ao construto de fora — o flow propaga) · pins ×12 NOVOS (test_voni.cpp) + **R-054** (`regress_voni_timers`) + M-3.9c1 (o resto do every) M-3.9c2 (o portão do after) M-3.9c3 (o portão 1×-total do when) vermelho→reposto→verde (coladas no RELATORIO-0.12-U-3.9c.md §3) · verificação: test_core 0 falhas (os skips de disco/sandbox conhecidos), c33_virtual 1010/0 · **versionCode 100** — o build vigente da 0.12-U. O 99 fica INTACTO como história (o relatório próprio: docs/RELATORIO-0.12-U-3.9c.md).
- **0.12-QoL · D — AUTO-SAVE E RECUPERAÇÃO (o seguro de vida do motor)** —
  o TIMER do dono (a cada 180s, com o dirty HONESTO — o dump difere do
  último persistido, o baseline renasce em cada load — grava
  **autosave.goni NA RAIZ** pelo MESMO dump do serializer; o .goni
  principal INTACTO; NO FIM DO FRAME; SÓ no editor) + a RECUPERAÇÃO (o
  `modifiedTime` NOVO no storage — o default −1 é o desconhecido
  honesto, o FsStorage devolve o stat; o autosave ESTRITAMENTE mais
  recente arma o pop-up 2.5D «Encontrámos trabalho não guardado. Desejas
  recuperar?» com **[Recuperar]/[Descartar]** — o toque fora é não
  destrutivo; o [Recuperar] carrega pelo MESMO loadText + pós-load; o
  corrompido sai com o principal intacto) + o INDICADOR (o ✓ verde
  PISCA ~2,4s à esquerda do play quando grava) · **R-056**
  (`regress_qol_autosave` — as decisões puras + o CICLO REAL no
  FsStorage com o mtime atrasado + os ids) + pins ×3 NOVOS (qol_d_*) +
  M-QoLd1 vermelho→reposto→verde (colada no RELATORIO-0.12-QoL.md §4) ·
  verificação: test_core 1194 OK / 0 falhas · c33_virtual 1010/0 ·
  docs-lint VERDE · **versionCode 105** — o build vigente da CAMPANHA
  0.12-QoL FECHADA. O 104 fica INTACTO como história.

- **0.12-Y · 3.Y-C-FIX — A UI DO ENVIRONMENT E O BIND DAS TEXTURAS (o
  teste do dono no telemóvel)** — DOIS problemas do APK (vc 108): o
  Inspector do WorldEnvironment VAZIO (só Transform/Animação/Script) e o
  modelo BRANCO. (1) A SECÇÃO **Environment** NO Inspector (o perfil
  `env` + o bit8 persistente + os ids 5420..5424): as 5 linhas da ordem
  do dono — **céu** (hex + swatch vivo, teclado 11), **hdri** (o botão
  abre o NAVEGADOR; o pick do .png valida, escreve
  `textures/hdri_<stem>.png`, aponta o Environment e calcula o ambient
  pela MÉDIA CPU; .hdr = aviso honesto — o decoder RGBE é out-of-scope
  hoje), **ambiente** (slider 0..2), **névoa** (slider 0..1), **cor da
  névoa** (hex, teclado 12) — TODOS leem/escrevem a struct Environment e
  o .goni transporta (o roundtrip do C manda); (2) O BIND: o sampler do
  albedo chama-se **uBaseColorTex** (o contrato do slot 0 no FS + no
  init, o fallback uTex é o retro-guarda) e o setTexture liga ao UNIT 0
  (normal 1 / rough-metal 2 nunca colidem) + O LOG DO MATERIAL (a ordem
  do dono, LITERAL): `[material] textura extraida: <nome> -> <caminho>`
  por extração + o skip jpeg NUNCA mais silencioso (a causa comum do
  branco diz-se no engine.log) + o log do parse por slot do material +
  o THUMB do baseColor com a VERDADE do slot 0 (a grade vazia herda o
  texPath — o dono VÊ se carregou) · pins ×2 NOVOS
  (`inspector_worldenv` — o plano com as 5 linhas na ordem + o colapso +
  o TIC sem Environment limpo; `pbr_texture_bind` — o contrato no fonte
  + o LitMaterial REAL no stub: uHasTex=1, unit 0, sem bind no nullptr) +
  M-Yf1 (o nome do contrato morto) M-Yf2 (a secção morta)
  vermelho→reposto→verde (coladas no RELATORIO-0.12-Y.md §3-FIX) ·
  verificação: test_core os pins VERDES (os flakes da sandbox
  documentados — o fork do r039 e o streaming_paralelo preexistente; o
  CI é o árbitro) · c33_virtual 1009/0 · docs-lint VERDE ·
  **versionCode 109** — o build vigente da campanha 0.12-Y. O 108 fica
  INTACTO como história.

- **FASE UI · E — O INSPECTOR** — o CABEÇALHO do TIC: o NOME em brasa
  (14sp, a reticência só quando não couber) e o TIPO por baixo (12sp —
  a lista das secções presentes, o MESMO perfil do plano; o cabeçalho
  cresce 28→44dp com seleção, o «Inspector» fica para o vazio; o
  orçamento do nome reserva a célula do recolher 40dp — nada tapa
  nada) · a BARRINHA COLORIDA do eixo nas caixas Pos/Rotação/Escala
  (3dp × 12dp na cor do eixo, centrada na caixa de 32dp; as LETRAS
  X/Y/Z morreram — «sem letras gigantes»; o valor começa a 12dp e o
  seu espaço CRESCIU com a letra morta, o piso R1 mantém-se) · as
  SECÇÕES RECOLHÍVEIS (Transform/Física/Material…) JÁ EXISTIAM (os 9
  bits do 0.9.0 que persistem) — a ordem confirma e o pin prova no
  plano (os cabeçalhos ficam, os campos morrem com o bit) · o ESTADO
  VAZIO discreto («Nada selecionado») com o «+ criar TIC» que abre o
  MESMO plusMenu do + da hierarquia (o caminho do C-4) · pin
  ui_inspector_cabecalho (a fonte + as barrinhas + as letras mortas +
  as secções no plano + o orçamento do nome com o pin ativo) · M-UIe
  (as letras de volta) vermelho→reposto→verde (colada no
  RELATORIO-FASE-UI.md §5) · recalibrações com nota (test_wiring090 ×2
  os toques do Transform, test_safearea o scroll do Player 500→484,
  test_ui ×6 as geometrias do cabeçalho 44) · verificação: test_core
  0 falhas (o único vermelho local = o fork do sentinela_r039, o flake
  da sandbox documentado — o CI é o árbitro) · c33_virtual 1010/0
  local (os DOIS alvos) · theme_hex/ui_vocab/glyph/hierarchy/
  projects_ui VERDES · **versionCode 115** — o build vigente da FASE
  UI. O 114 fica INTACTO como história.

- **FASE UI · D — VIEWPORT E FERRAMENTAS** — a coluna das 4 FERRAMENTAS
  (Selecionar/Mover/Rodar/Escalar — o desenho 32 na banda 30-36 do dono,
  o ativo É A BRASA) · o Íman e a MÃO (Pan) desceram a CHIPS pequenos sob
  o rail (os MESMOS ids 33/36 e o MESMO despacho; o par INTEIRO tem de
  caber — o achado da M-UId) · os divisores (24dp + os pisos) e a
  persistência layout.json confirmados e aferrados pelo pin novo · pins
  pan_rail_sexta recalibrado + ui_divisores_persistencia NOVO + M-UId
  vermelho→reposto→verde (colada no RELATORIO-FASE-UI.md §4) ·
  verificação: test_core 0 falhas · c33_virtual 1010/0 local ·
  **versionCode 114** — o build vigente da FASE UI. O 113 fica INTACTO
  como história.

- **FASE UI · C — A HIERARQUIA 40DP** — as linhas 40dp (kRowH na fonte
  única; o scroll acompanha) · o OLHO e o ⋮ SÓ na linha selecionada (o
  draw gated `if (selected)` + o hit gated `controlsRow` — a MESMA
  condição; nas outras linhas a zona é corpo) · o nome de largura inteira
  nas não selecionadas (a reticência só quando não couber) · a indentação
  lê a escala do dono (kSpace3 16 — o literal 24 morreu) · pin
  ui_hierarquia_40dp + M-UIc vermelho→reposto→verde (colada no
  RELATORIO-FASE-UI.md §3) · 6 recalibrações com nota · verificação:
  test_core 0 falhas · c33_virtual 1009/0 local · **versionCode 113** —
  o build vigente da FASE UI. O 112 fica INTACTO como história.

- **FASE UI · B — A BARRA SUPERIOR 40DP** — a direita NOVA (undo/redo/
  guardar subiram do viewport + play/stop/gear; o Pause vive na PLAY BAR
  com o mundo REALMENTE congelado e o «Android» é a linha «plataforma»
  das Definições; os ids 3/19/30/31/32 aposentados, os 20/21/22 novos) ·
  a fila do viewport morre para o chip ⋯ · O ACHADO DO CI: o A ficou
  vermelho na sonda 35.2 do c33 (a linha âmbar) — a cura + o
  scripts/build_c33.py NOVO (o gate da casa: compilar os DOIS alvos;
  11 sondas stale recalibradas com o harness a mirrar o overlayArea) ·
  pins ui_topbar_40dp_composicao + ui_sobreposicao_topbar estendido ·
  M-UIb vermelho→reposto→verde (colada no RELATORIO-FASE-UI.md §2) ·
  verificação: test_core 0 falhas · c33_virtual 1009/0 local (os DOIS
  alvos) · **versionCode 112** — o build vigente da FASE UI. O 111 fica
  INTACTO como história.

- **FASE UI · A — TOKENS E IDENTIDADE** — O FICHEIRO ÚNICO DE TOKENS
  (ui/Theme.h) com A PALETA DO DONO (os hexes EXATOS: fundo #0B0B0D ·
  painéis #121215 α0.85 · elementos #1B1B20 · linhas 1px #26262C · texto
  #E8E6E3/#8A8A93 · A BRASA ÚNICA #FF5A1F — o âmbar #FFB020 morreu ·
  seleção #2B1810 · eixos #E5484D/#3FB950/#3B82F6) + a escala 4/8/12/16 +
  os raios 6-8 · o espelho Java alinhado (o TOKENS_F do gate
  recalibrado, os 2 hexes inline mortos) · pins ×2 (ui_tokens_brasa ·
  ui_sobreposicao_topbar, test_fase_ui.cpp NOVO — o teste automático de
  sobreposição da fase) + M-UIa vermelho→reposto→verde (colada no
  RELATORIO-FASE-UI.md §1) · 9 recalibrações com nota · verificação:
  test_core 0 falhas na corrida completa (o CI é o árbitro) ·
  theme_hex/ui_vocab/projects_ui VERDES · **versionCode 111** — o build
  vigente da FASE UI. O 110 fica INTACTO como história.

- **0.12-Y · 3.Y-D — O CÉU PROCEDURAL COM NUVENS ANIMADAS** — o shader do
  céu (render/SkyRenderer.{h,cpp} NOVOS: o quad fullscreen, o gradiente
  horizonte→zénite, o sol disco/halo, as nuvens FBM 2D com hash inteiro e
  O TEMPO no UV; as puras sky:: replicadas LINHA A LINHA pelo FS), o
  Environment com skyType/cores do céu/sol/direção/cobertura/vento no
  .goni (defaults omitidos), o Inspector com o dropdown + os 5 campos
  condicionais (os hex 13/14/15, os sliders nuvens/vento), o main desenha
  ANTES dos modelos com o relógio g_skyTime 1× por frame · pins ×2
  (env_sky_type · inspector_sky, test_012y.cpp) + R-061
  regress_procedural_sky (test_sentinels.cpp + REGRESSOES) + M-Yd1/2
  vermelho→reposto→verde (coladas no RELATORIO-0.12-Y-SKY.md §3; o achado
  do falso verde POR COMENTÁRIO → test::stripCppComments) · verificação:
  test_core 1208 OK com os flakes da sandbox excluídos (o CI é o árbitro)
  · docs-lint VERDE · **versionCode 110** — o build vigente da campanha
  0.12-Y. O 109 fica INTACTO como história.

- **0.12-Y · 3.Y-A — O PAN DA CÂMARA (a MÃOZINHA)** — a 6.ª FERRAMENTA
  no rail do viewport (Selecionar/Mover/Rodar/Escalar/**PAN**/Íman — o
  glifo Hand novo, o id 33, a legenda «Pan»): o Camera::pan NOVO move o
  TARGET no plano da câmara pela FONTE ÚNICA pura (camWorldPerPixel =
  2·dist·tan(fovY/2)/viewH + camRightVec/camUpVec — a velocidade do dedo
  1:1 em qualquer zoom; dedo→direita ⇒ target ESQUERDA — agarra a cena);
  o modo Pan: 1 dedo arrasta o foco SEM orbitar (a seleção por tap fica);
  o pan de 2 DEDOS é LIVRE (o midpoint arrasta em qualquer modo de
  ferramenta, o pinch mantém o zoom — o 1.º frame só ancora) · R-059
  (`regress_pan` — a direção + a magnitude + o pan de 2 dedos + a
  exclusividade) + pins ×5 NOVOS (pan_camera_math/pan_modo_um_dedo/
  pan_dois_dedos_livre/pan_pinch_intacto/pan_rail_sexta — test_012y.cpp) +
  M-Ya1 (o pan invertido — 4 vermelhos) M-Ya2 (o pan de 2 dedos morto — 2
  vermelhos) vermelho→reposto→verde (coladas no RELATORIO-0.12-Y.md §1) ·
  verificação: test_core os pins VERDES (as únicas falhas locais são os
  flakes da sandbox documentados — o fork do medicoes; o crash do
  regress_streaming_paralelo é preexistente, PROVADO no HEAD pristine) ·
  docs-lint VERDE · **versionCode 106** — o build vigente da campanha
  0.12-Y. O 105 fica INTACTO como história.

- **0.12-Y · 3.Y-B — O ÍCONE IA (obsidiana + magma)** — a FONTE gerada
  pela ferramenta de imagem AI com o prompt EXATO do dono
  (docs/iconegonevv-ai.png) + o gerador scripts/y_gen_ai_icon.py (a
  extração por TEXTURA high-pass — o luminância da Y-0 comeria a
  obsidiana e deixaria passar o reflexo) + o tile a 65% da ZONA SEGURA
  (o contrato Y-0-FIX) + os 15 PNGs (5 densidades × launcher/round/
  foreground) + o fundo **#0A0A0C** (A ORDEM DO DONO, supera o #08080A)
  · pins: icon_ia_mipmaps (as 15 dimensões IHDR + a fonte + o gerador +
  o manifest) + as recalibrações com nota (y0_background, y0_escala) +
  M-Yb1 (a safe zone violada a 25% — 2 vermelhos) vermelho→reposto→verde
  (colada no RELATORIO-0.12-Y.md §2) · verificação: test_core os pins
  VERDES (o flake do medicoes é o único vermelho local — o CI é o
  árbitro) · docs-lint VERDE · **versionCode 107** — o build vigente da
  campanha 0.12-Y. O 106 fica INTACTO como história.

- **0.12-Y · 3.Y-C — O PIPELINE PBR + O WORLDENVIRONMENT** — o glTF lê
  AGORA os 5 slots (baseColor/normal/metalRough/emissive/occlusion — o
  parse já lia, a extração só levava o baseColor) e escreve na PASTA DO
  MODELO **assets/<stem>/textures/** (a ordem do dono; a dedup FNV-1a
  por bytes; a uri externa passa-through; o modelo sem nome usa
  "modelo") · o BIND AUTO liga os slots vazios ao MeshRenderer pelo
  `meshTexSlotFor` (o caminho é a verdade — o slot 0 alimenta o
  texPath/texture) · o LIT ganha o **PBR-LITE** pela FONTE ÚNICA NOVA
  render/Pbr.h (o normal map por DERIVADAS DE ECRÃ dFdx/dFdy — a TBN
  sem atributos novos, o mesh antigo byte a byte; o rough/metal
  empacotado glTF no unit 2; os fatores do GTB2 entram; a NÉVOA exp2 com
  density 0 = MORTA = o lit de sempre; o uAmbientScale) · o TIC
  **WorldEnvironment** NOVO no "+" (o 7.º preset — Transform+Environment
  SEM mesh/física): o CÉU é o clear, a névoa + o ambient do HDRI pela
  MÉDIA CPU (averageColor + ambientFromAvg — o PISO 0.25 nunca deixa o
  HDRI escuro apagar a cena), o 1.º ATIVO manda · o .goni guarda
  sky/fog/density/ambient/hdri/active (defaults omitidos =
  forward-compat) · o AssetConverter cria a pasta do alvo e o migrate
  aceita as 2 gerações (assets/<stem>.gmesh OU assets/<stem>/<stem>.gmesh)
  · pins ×4 NOVOS (pbr_fontes · env_media · gltf_5slots_pasta ·
  env_roundtrip) + as recalibrações com nota (registry 13→14, os meshes
  do texbind na pasta do modelo, o Count 6→7 + a geometria 8 linhas do
  menu "+") + **R-060** (`regress_pbr_worldenv`) + M-Yc1 (a névoa
  invertida) M-Yc2 (o .goni perde o HDRI) vermelho→reposto→verde
  (coladas no RELATORIO-0.12-Y.md §3) · verificação: test_core os pins
  VERDES (corrida completa 1206 OK; os flakes da sandbox documentados —
  o fork do medicoes e o crash intermitente do streaming_paralelo
  provado preexistente; o CI é o árbitro) · c33_virtual 1009/0 ·
  docs-lint VERDE · **versionCode 108** — o build vigente da campanha
  0.12-Y. O 107 fica INTACTO como história.

- **0.12-QoL · C — O VIEWPORT 3D LIMPO** — a TEIA de linhas do frustum
  SÓ desenha com a câmara SELECIONADA (o glifo mudo fica — o alvo do
  toque; o toggle do Inspector mantém-se) + o pai de vidro do RAIL sai
  do draw (os botões flutuam com os SEUS chips; o rect fica no layout —
  os pins C-2 sem recalibração) + o grid ganha o fade SUAVE pela fonte
  única `gridFadeForDist` (o caída dobra: 2.2×foco piso 60 — o
  horizonte perde-se sem borda dura) · **R-058** (`regress_qol_viewport`
  — ZERO arestas sem seleção / o desenho volta / a janela nos 3
  regimes) + pins ×3 NOVOS (qol_c_*) + M-QoLc1 vermelho→reposto→verde
  (colada no RELATORIO-0.12-QoL.md §3) + o d17 recalibrado com nota ·
  verificação: test_core 1183 OK / 0 falhas ×2 · c33_virtual 1010/0 ·
  docs-lint VERDE · **versionCode 104** — o build vigente. O 103 fica
  INTACTO como história.

- **0.12-QoL · B — O NAVEGADOR DE FICHEIROS (ZArchiver Style)** — os
  atalhos de raiz e o Subir deixam de ser os BOTÕES âmbar do choke
  point (os «botões gigantes» do Redbr1) e passam a CHIPS QUIETOS
  (surface + borda cinza + text2; a geometria e os ids intactos — o
  picker_compacto segue sem recalibração) + a seta (>) em TODAS as
  linhas (α 0.4 nos ficheiros) + o SEPARADOR « / » visível no
  breadcrumb + TUDO o que começa por ponto FORA (a decisão 0.8.5
  SUPERADA pela ordem do dono; as pastas ponto já saíam desde a C-4) +
  a ordem pastas-primeiro aferrada por sentinela · **R-057**
  (`regress_qol_picker` — a árvore própria: ZERO ocultos, a ordem
  exata, o « / » na fonte) + o pin 012c_picker_oculta_dot recalibrado
  com nota + pins ×2 NOVOS (qol_b_navegador_compacto /
  qol_b_ponto_fora) + M-QoLb1 vermelho→reposto→verde (colada no
  RELATORIO-0.12-QoL.md §2) · verificação: test_core 1180 OK / 0
  falhas · c33_virtual 1010/0 · docs-lint VERDE · **versionCode 103** —
  o build vigente. O 102 fica INTACTO como história.

- **0.12-QoL · A — LIMPEZA VISUAL DA UI (o feedback do Redbr1)** — o
  top bar SEM o texto do nome (o slot `name` e o `showName` mortos, o
  padrão D21; o degrau k<0.78 da degradação morreu — o chip da
  plataforma é o 1.º a sair) + a pesquisa da hierarquia RECOLHIDA na
  LUPA 32×32 que EXPANDE ao toque (o × limpa e recolhe;
  `hierSearchOpen`) + o border dos INATIVOS **#24242A** (o token
  `border` recolorado + o alias LINE + o `dividerGradientV` + as
  molduras âmbar dos cards modais; o ÂMBAR só no ativo/selecionado) +
  os vãos X/Y/Z 8→12dp, a margem mínima 4→8dp (o Reset nunca cola) e a
  folga vertical 64→68dp (o piso do valor 26→22 + o passo (c) da escada
  R1) + o X de fechar 48×48 no menu de cenas · pins ×5 NOVOS
  (test_012qol.cpp) + M-QoLa1/x2 vermelho→reposto→verde (coladas no
  RELATORIO-0.12-QoL.md §1) · 8 recalibrações com nota (a limiar da
  letra m3 212→216dp incluída; o device virtual fecha 1010/0) ·
  verificação: test_core 1178 OK / 0 falhas (o bench r039 saltado no
  sandbox — o CI é o árbitro) · docs-lint VERDE · **versionCode 102** —
  o build vigente da campanha 0.12-QoL. O 101 fica INTACTO como
  história. (o relatório próprio: docs/RELATORIO-0.12-QoL.md §1)

- **0.12-X · 3.9x — CAMRIG + CAMDRAG: A CÂMARA DE TERCEIRA PESSOA (o autor deixa de escrever câmara à mão)** — as 2 entradas NOVAS no registo (Kind::Componente, CompKind::**Continuo**, argc 2..3 / 1..1 — Docs com o exemplo do TRIO: `find(cam) camrig(8, 3, 6) camdrag(0.3) limit(3, 12)`): o **camrig(d,h[,suav])** assenta a câmara à distância 3D d do PIVÔ (alvo+h) na direção esférica do yaw/pitch do TykerState, olha para alvo+h/2 (o rot em GRAUS pelo canal da casa — a convenção da CÂMARA −Z, yaw atan2(−dx,−dz), pitch asin(dy/|d|); o look dos OBJETOS usa a DELE e fica FORA — a cláusula calma), suaviza com o decaimento da casa 1−e^(−suav·dt) (sem suav = snap) e o limit(min,max) clampa a distância FINAL APÓS a suavização contra o pivô; o **camdrag(sens)** consome o `Host::lookDelta(dx,dy)` NOVO (o default false é honesto — o hospedeiro sem input nunca mata o script; o consumo ZERA o acumulador — 2 tykers nunca dobram o gesto), yaw 360° livre NORMALIZADO, pitch clamped FIXO −20..70 (decisão fechada da campanha); o yaw/pitch vivem no TykerState (camYaw/camPitch, junto ao orbitAngle); o **main.cpp** alimenta o acumulador SÓ em Play com os deltas dos dedos que os controlos/canvas/gizmo NÃO reclamaram (o dedo nasce com a máscara do press — se nasceu reclamado NUNCA é olhar; o TIMING é o da casa: o frame alimenta, o 1.º passo fixo seguinte consome — a MESMA latência do g_physics.frame.fwd, os dois sincronizados) + o **compilador** rejeita camrig+follow/look no mesmo tyker (o erro ENSINA com o par fixFrom/fixTo do Substituir) + sem alvo válido = erro CEDO com linha · pins ×14 NOVOS (10 VM no test_voni.cpp + 4 de MOTOR REAL no test_wiring092.cpp — o camrig no Transform3D real, o camdrag pelo VoniSystem.setLookDeltaFn ponta a ponta, o erro cedo sem Transform3D, o TRIO pelo caminho REAL do .goni byte a byte + a vida depois do disco) + **R-055** (`regress_camrig`) + M-3.9x1 (o consumo do olhar fora) M-3.9x2 (o conflito ignorado no compilador) vermelho→reposto→verde (coladas no RELATORIO-0.12-X-CAMRIG.md §4) · a referência regenerada (16566 → 17549 bytes, 66 entradas; R-013 byte a byte) · verificação: test_core 0 falhas (o skip de disco conhecido — o CI é o árbitro) · os 12 gates locais verdes · STATE.md 299 (<300) · **versionCode 101** — o build vigente da 0.12-X. O 100 fica INTACTO como história (o relatório próprio: docs/RELATORIO-0.12-X-CAMRIG.md).

- **FASE UI · F — BARRA INFERIOR FINA + PAINEL ENCAIXADO (a ordem do
  dono, envio 3/5)** — a barra de 32dp com SÓ o botão **«Painéis ▾»** à
  esquerda (CONTORNO BRASA com o painel aberto; o chevron por geometria)
  e o estado **«60 FPS · 6 TICs»** à direita (o «· bl n/m» APOSENTOU — a
  reserva kFpsW ficou FIXA); as 4 TABS (Ficheiros/Assets/Consola/
  Animação) subiram para a FILA DE 40dp no CABEÇALHO do painel
  ENCAIXADO (o viewport SOBE — o mecanismo P-08 intocado; o conteúdo é a
  FONTE ÚNICA Layout::content partilhada pelo draw e pelo main); o
  Painéis abre com a ÚLTIMA aba (lastTab no layout.json, retrocompatível)
  e fecha no ▾/Painéis (a aba ativa não fecha mais); a ALTURA INICIAL
  ~30% (Fich/Assets/Consola) e ~40% (Animação) pela fonte pura
  tabDefaultHPct/tabDefaultH com o arrasto do dono a mandar depois
  (hCustom); a Animação tem teto 40% (kDrawerMaxPctAnim) E PISO que cabe
  a timeline (kTimelineH+pega+fila) com a TIMELINE CLIPADA ao rect
  (ScopedClip na strip — os overlays fora do clip) — nada cruza a barra
  em 13 ecrãs (FASE 31); o PONTO VERMELHO (danger 6dp) na aba Consola e
  no botão quando há erro no log · pin `ui_paineis_dock` (test_fase_ui:
  a barra em 5 ecrãs × 5 estados, o encaixe, os defaults, o piso, a
  fonte, o roundtrip lastTab) + M-UIf (o encaixe frouxo) vermelho→
  reposto→verde · recalibrações com nota (test_012a_contadores,
  test_wiring090, c33 13.7h/13.7i/20.5/FASE 31) · verificação: test_core
  0 falhas · c33_virtual 1010/0 · docs-lint VERDE · **versionCode 116** —
  o build vigente. O 115 fica INTACTO como história. (o relatório
  próprio: docs/RELATORIO-FASE-UI.md §9)

- **FASE UI · G — MENUS E DIÁLOGOS CONTIDOS (a ordem do dono, envio
  3/5)** — o TETO DE 85% DO ECRÃ em todas as folhas (kSheetMaxPct na
  fonte única EditorLayout.h — o menuSheetLayout, o browserSheetLayout e
  o menu de Cenas; o MENOR do teto e do cap de contentRect manda) + o
  NAVEGADOR nunca desce POR BAIXO da barra inferior (bottomReserve na
  fonte única — o card centra-se acima da barra de 32dp) + o X DE FECHAR
  no MENU PRINCIPAL (kMenuCloseId 7411 — a faixa fixa de 40dp no topo do
  sheet; os irmãos 6799 Cenas/6899 navegador já viviam) + a ordem
  vertical do navegador confirmada (breadcrumb → pesquisa → raízes →
  Subir → lista com scroll — o C-4 intocado) + o indicador de scroll da
  casa nas folhas · pin `ui_sheets_conteudos` (o teto em 5 ecrãs com
  conteúdo gigante, o fundo ≤ o topo da barra, a ordem vertical, a fonte
  do X e do indicador) + M-UIg (o teto morto) vermelho→reposto→verde
  (a lição do HEADER stale: o TU do pin precisa de touch — a 4.ª rodada
  da lição incremental) · recalibrações com nota (os chamadores da fonte
  única com a reserva, o toque da âncora +dp(40), o 17.2 D21 no c33 — o
  offset 400 + a caça documentada) · verificação: test_core 0 falhas ·
  c33_virtual 1010/0 · docs-lint VERDE · **versionCode 117** — o build
  vigente. O 116 fica INTACTO como história. (o relatório próprio:
  docs/RELATORIO-FASE-UI.md §10)

### §14-h — FASE UI H (o editor de código) — versionCode 118

**G8** (o par declarado no MESMO commit, a regra do §14): o editor de
código da FASE UI H — o TIC + os exportáveis no topo, as ABAS com a
ativa em brasa, o undo/redo por evento, o copiar/colar na família de
ícones, a barra enxuta `{ } ( ) [ ] ; = " . , Tab`, a brasa SÓ nas
reservadas, o mono do 2.º atlas, a banda da linha corrente — e AS CAUSAS
dos defeitos reportadas no relatório da fase (o cap 256 do draw vs o
caret = o desalinho; o follow ABSOLUTO de todos os frames = o tremor).
verificação: test_core 0 falhas · c33_virtual 1008/0 · docs-lint VERDE ·
**versionCode 118** — o build vigente. O 117 fica INTACTO como história.
(o relatório próprio: docs/RELATORIO-FASE-UI.md §11)

### §14-i — FASE UI I (as abas Áudio e UI) — versionCode 119

**G8** (o par declarado no MESMO commit, a regra do §14): o workspace
ÁUDIO com o ORÇAMENTO VERTICAL (a fonte única): o rótulo «CLIPS» numa
faixa FIXA fora do scroll (nunca sob os botões nem sob a lista), a
waveform FIXA 80dp, as ações do clip numa linha e a confirmação de
apagar como MODAL no rect da waveform com botões de 40dp — a lista é
que cede e nada desenha fora do rect (o guard honesto abaixo do
orçamento); o estado vazio legível (o helper da casa). O editor de UI
com o estado vazio LEGÍVEL (o MESMO drawEmptyState das tabs do drawer)
e o botão «+» claro de 44dp que abre O MESMO plusMenu de presets (o
caminho do C-4). verificação: test_core 0 falhas (o pin ui_audio_ui) ·
c33_virtual 1008/0 · docs-lint VERDE · **versionCode 119** — o build
vigente. O 118 fica INTACTO como história. (o relatório próprio:
docs/RELATORIO-FASE-UI.md §12)

### §14-j — FASE UI J (a tela de projetos) — versionCode 120

**G8** (o par declarado no MESMO commit, a regra do §14): os cartões
com O ÍCONE DA ENGINE no fallback (o BrandThumbView desenha o glifo
pela ÚNICA função UiIcons.drawBrand na cor fraca — AS LETRAS
GENÉRICAS «TE»/«AS» MORRERAM com o InitialsThumbView; a miniatura da
cena continua a ser o thumb.png quando existe), o nome, a data e o ⋮
(confirmados), «Novo projeto» o ÚNICO botão em brasa (fill accent) e
«Importar projeto» SECUNDÁRIO (contorno neutro border/text1 — a brasa
fica só no primário), a pesquisa e o ordenar discretos (confirmados),
a MESMA paleta/tokens do editor (confirmados). verificação: o gate
projects_ui_check recalibrado (a mutação M-UIj vermelho→verde ao
vivo) · docs-lint VERDE · **versionCode 120** — o build vigente. O 119
fica INTACTO como história. (o relatório próprio:
docs/RELATORIO-FASE-UI.md §13)

### §14-k — 0.12-Y-E (o ciclo dia/noite) — versionCode 121

**G8** (o par declarado no MESMO commit, a regra do §14): o CÉU
PROCEDURAL ganha o CICLO DIA/NOITE — a grade dos 4 estados chave
(noite/amanhecer/meio-dia/pôr do sol) corre NO CPU 1× por frame e o FS
consome os uniforms JÁ graduados (ZERO ALU novo por pixel — a
otimização do C33); as ESTRELAS atrás de um branch uniforme (o hash
inteiro sem sin); o sol roda pelo relógio (06h leste, 12h zénite, 18h
oeste) e a DIRECTIONAL sincroniza (o setLight do lit — o luar azul
fraco à noite, o alaranjado ao pôr do sol); cloudHeight (0.25..4)
escala o plano das nuvens; o autoCycle avança o relógio no frame() com
o anel de 24h; o Inspector ganha hora/auto ciclo/vel./altura (ids
5431..5434); o .goni transporta tod/cycSpd/cycOn/cloudH com defaults
omitidos. verificação: pins env_sky_clouds · env_daynight_cycle ·
env_serialize · inspector_daynight + R-061 (5) + M-Ye1/M-Ye2 VERMELHAS
AO VIVO (a M-Ye2 herdada aplicada na árvore — a lição do objeto stale
com testes a provar) → suíte 0 falhas · c33_virtual 1008/0 · docs-lint
VERDE · **versionCode 121** — o build vigente. O 120 fica INTACTO como
história. (o relatório próprio: docs/RELATORIO-0.12-Y-E-SKY.md)

### §14-l — 0.12-Y-F1 (o TIC ParticleEmitter) — versionCode 122

**G8** (o par declarado no MESMO commit, a regra do §14): o SISTEMA
BASE de PARTÍCULAS GPU — VERTEX SHADER ANIMATION (GLES 3.0, ZERO
compute): o CPU gera UMA VEZ o buffer de seeds (2000 × 4 floats — o
teto do buffer; o slider só muda o count) e o VS faz a balística p =
p0 + v·t + ½g·t² com o tempo local mod(uTime + seed·100, lifetime) (as
sementes desafazem o tempo — ninguém nasce junto); UM
glDrawArraysInstanced por emissor (o divisor 1 nos seeds); o FS lê a
textura OU o disco procedural e aplica a cor interpolada com o FADE
OUT; o jorro de fábrica (sobe +3, cai −4, âmbar→vermelho); o preset
ParticleEmitter (Transform + PE, SEM mesh/física) no menu "+" (o 9.º);
o Inspector com a secção "Particle Emitter" (máx./vida/tamanhos/cores
hex 16/17/gravidade/textura pelo NAVEGADOR — a intenção é o MESMO
contrato do hdri); o .goni transporta com defaults omitidos; a PAUSA
congela o g_fxTime (a honra do céu). verificação: pins
particle_emitter_cria · particle_serializa · particle_inspector + R-062
regress_particles + M-Yf1/M-Yf2 VERMELHAS AO VIVO (a M-Yf2 apanhou um
pin AMBÍGUO — apertado para a linha exata, a lição M-UIj) → suíte 0
falhas · c33_virtual 1008/0 · docs-lint VERDE · **versionCode 122** —
o build vigente. O 121 fica INTACTO como história. (o relatório
próprio: docs/RELATORIO-0.12-Y-F1-PARTICLES.md)

### §14-m — 0.12-V (os 3 bugs do viewport 3D) — versionCode 123

**G8** (o par declarado no MESMO commit, a regra do §14): o DIAGNÓSTICO
COM FICHEIRO:LINHA antes de mexer (a ordem do dono) e a cura dos 3 bugs
do viewport: (1) o TELEPORTE do arrasto — o feedback imediato escrevia
world = TRS LOCAL (o mesh saltava para o gizmo durante o drag e voltava
ao largar: «o modelo só aparece enquanto toco»); (2) o GIZMO fora do
mundo dos TICs com pai (desenhava/hit-testava o tr->pos local-as-world
enquanto o mesh desenha na TRANSLAÇÃO do world); (3) o TAMANHO variável
(dist_da_órbita·k, sem tan(fov/2) — gigante perto do olho). A cura: a
FONTE ÚNICA composeWorld no TransformSystem (o tick e o feedback
partilham), worldToLocalPos (o drag corre no mundo, o TRS guarda local),
gizmoAnchor (a MESMA origem+comprimento no hit e no draw) e a fórmula do
dono dist·tan(fov/2)·k com o toque ≥48px. O importador/preset PROVADOS
inocentes (o parenting veio da cena do dono — o motor não re-parenta).
verificação: pins viewport_modelo_sem_gizmo · viewport_gizmo_no_mundo ·
viewport_drag_sem_teleporte · viewport_gizmo_px_constante (o caminho
REAL do device — sondas de píxel) + R-063 regress_viewport_gizmo_world +
M-V1/M-V2/M-V3 VERMELHAS AO VIVO → suíte 0 falhas · c33_virtual 1008/0 ·
gates VERDES · **versionCode 123** — o build vigente. O 122 fica INTACTO
como história. (o relatório próprio: docs/RELATORIO-0.12-V-VIEWPORT.md)

### §14-n — 0.12-Y-F2 (o clima e o vento: AmbientEnvironment + WindTIC) — versionCode 124

**G8** (o par declarado no MESMO commit, a regra do §14): a DECISÃO DE
ARQUITETURA do dono honrada à letra (a opção A): o AmbientEnvironment
NÃO renderiza partículas — CONFIGURA o ParticleEmitter do MESMO TIC
(applyWeatherPreset na criação E na mudança no Inspector); o WindTIC
fornece o vento global lido pelo ParticleRenderer (uWind antes do draw
call — o 1.º WindTIC ATIVO manda, o windVector é a fonte); a trovoada é
um FLASH DE ECRÃ via uniform no sky shader (a linha exata do prompt no
FS, sem geometria de raio); sem emissor o clima avisa e não aplica;
GLES 3.0 — zero compute. Os presets do «+»: Wind / Rain / Snow / Storm
(13 itens; o Rain/Snow/Storm nascem com o clima APLICADO). O .goni
transporta weather/intensity + dir/strength (defaults omitidos; o
runtime da trovoada NUNCA viaja). verificação: pins wind_tic_cria ·
ambient_rain_preset · ambient_snow_preset · ambient_serialize ·
wind_uniform_passed (tests/test_012y_f2.cpp NOVO) + R-064
regress_weather + M-Yg1/M-Yg2 VERMELHAS AO VIVO → suíte 0 falhas
(GONI_SKIP dos 2 flakes de disco da sandbox + os 2 devoradores das
medições — o CI é o árbitro) · gates VERDES · **versionCode 124** — o
build vigente. O 123 fica INTACTO como história. (o relatório próprio:
docs/RELATORIO-0.12-Y-F2-WEATHER.md)

### §14-o — 0.12-Z F0 (o diagnóstico do contexto GL) — versionCode 125

**G8** (o par declarado no MESMO commit, a regra do §14): a FASE 0 da
campanha anti-perda de contexto OpenGL (o RELATORIO-0.12-Z-GLCTX.md §F0)
não muda comportamento de render nenhum — entra a VARA DE MEDIR:
render/GlDebug.h NOVO (os interruptores POR PASSE com o mestre OFF por
defeito, os contadores de recursos GL por kind, o snapshot de estado à
entrada/saída de cada passe, o glGetError por passe), os gates no frame
real do main.cpp, o inventário GL no fim do boot, os timestamps de
lifecycle no INIT/TERM/LOST_FOCUS/GAINED_FOCUS (os 2 últimos NOVOS) e os
11 toggles nas Definições → Diagnóstico (de sessão, nunca persistidos).
O §F0.3 do relatório fecha as 4 CANDIDATAS da sombra com ficheiro:linha
(a H1: o scrim/sheet preso que cobre o ecrã inteiro E come os toques —
EditorUi.cpp:3947/4086/4258 + main.cpp:7424 — é a que explica o
conjunto inteiro de sintomas; os toggles fecham no device). O §F0.4
analisa o clima/partículas «não funciona» (a causa mais provável: os
presets do prompt são sub-pixel no C33 — 0.015 m ≈ 1.3 px a 720p/7 u;
a decisão dos novos tamanhos é do dono). pins ×4 (tests/test_012z_ctx.cpp
NOVO) + R-065 regress_glctx_fase0 + M-Z1/M-Z2 VERMELHAS AO VIVO ·
test_core 0 falhas (GONI_SKIP dos flakes de disco documentados da
sandbox — o CI é o árbitro) · gates VERDES · **versionCode 125** — o
par declarado aqui e no build config (a regra do §14).

### §14-p — 0.12-Z F1 (o GLResourceManager + a câmara sã) — versionCode 126

**G8** (o par declarado no MESMO commit, a regra do §14): a FASE 1 da
campanha anti-perda de contexto (o RELATORIO-0.12-Z-GLCTX.md §F1):
render/GlResources.h NOVO (o GLResourceManager — a FONTE ÚNICA da lista
de invalidação/recriação; o achado: o g_particles.destroy() FALTAVA na
lista manual do term da 0.6.7), as 8 camadas registadas no android_main,
o term via invalidateAll(), o boot [3/6]/[4/6] via recreateLayer() com o
break do renderer obrigatório preservado, cameraEnsureSane() no
INIT/RESIZED/RESUME e editorRecalcHitCaches(). A auditoria da fila
UI→render (o item 6 do dono): NÃO há corrida por construção (a UI corre
na thread de render; o Java drena via storage::pollResult()/ImeQueue no
topo do frame). pins ×2 (glresources_ciclo, camera_sana) + R-066
regress_glctx_fase1 · test_core 0 falhas (o CI é o árbitro) · gates
VERDES · **versionCode 126** — o par declarado aqui e no build config.

### §14-q — 0.12-Z F2 (a origem da sombra: o WE sã + a névoa highp) — versionCode 127

**G8** (o par declarado no MESMO commit, a regra do §14): a FASE 2 da
campanha (o RELATORIO-0.12-Z-GLCTX.md §F2): a CAUSA RAIZ da sombra com
ficheiro:linha (o fogDensity NaN passava pela clamp — comparações com
NaN são sempre falsas — e envenenava a exp2 do lit FS: outColor NaN = o
preto do dono), a cura na FONTE (Environment::sane — a rejeição de
NaN/inf em TODOS os campos f32), a névoa do FS em highp float + sem pow
(fogD*fogD), a grelha com o trio do dono EXPLÍCITO (depth test ON, func
LESS, write OFF) e a entrada do pass 3D a repor o depth func. A ordem
dos passes aferrada (antes = depois; a sentinela vigia). pin
env_sana_nan + R-067 regress_glctx_fase2 · test_core 0 falhas ·
c33_virtual 1008/0 · gates VERDES · **versionCode 127** — o par
declarado aqui e no build config.

### §14-r — 0.12-Z F3 (a deteção e a recuperação do contexto) — versionCode 128

**G8** (o par declarado no MESMO commit, a regra do §14): a FASE 3 da
campanha (o RELATORIO-0.12-Z-GLCTX.md §F3): a sonda por frame no topo do
frame() (2 chamadas EGL + glIsProgram/glIsTexture + o glGetError em série
com limiar 30), a recuperação PELO GESTOR da F1 (o reset da interação → o
detach → o releaseAll → o invalidateAll → o recreateAll → a câmara sã; a
falha arma o cooldown), os resets no PAUSE/LOST_FOCUS (o ACTION_CANCEL já
soltava — InputState.cpp:41 verificado), o chip «GL: OK/RECRIADO n» só no
debug e o botão «Reiniciar renderer» nas Definições (o MESMO caminho do
automático). O picking provado CPU puro (0 leituras GL no caminho do
gizmo). pin context_recovery_ciclo + R-068 regress_glctx_fase3 ·
test_core 0 falhas · gates VERDES · **versionCode 128** — o par
declarado aqui e no build config.

### §14-s — 0.12-Z F4 (a simulação da perda + os 20 cenários) — versionCode 129

**G8** (o par declarado no MESMO commit, a regra do §14): a FASE 4 da
campanha (o RELATORIO-0.12-Z-GLCTX.md §F4): o botão «Simular perda de
contexto» (SÓ com o mestre do debug ON — o flag arma 1 frame de sonda
morta e a recuperação REAL corre, zero atalho) e os 20 CENÁRIOS do dono
no replay do device (10 SEM WE + 10 COM WE: boot/TERM+INIT/RESIZED/tabs/
painel/PAUSE+RESUME+foco/perda simulada — cada um aferra a saúde inteira
das camadas do gestor). O critério «deteta, recria e continua em menos de
1 segundo» provado: a recuperação corre DENTRO de 1 frame. pin
simulate_loss_recupera + R-069 regress_glctx_fase4 · test_core 0 falhas ·
c33_virtual 1113/0 (era 1008 — +105 checks dos cenários) · gates VERDES ·
**versionCode 129** — o par declarado aqui e no build config.

### §14-t — 0.12-Z2 F0 (o ambiente de prova com GLES REAL) — versionCode 130

**G8** (o par declarado no MESMO commit, a regra do §14): a FASE 0 da
campanha Z2 (o RELATORIO-0.12-Z2-RASTER.md): o raster_z2 — a engine a
correr contra o DRIVER GLES REAL (Mesa/llvmpipe 25.0.7, OpenGL ES 3.2) no
hospedeiro via EGL pbuffer (o ÚNICO desvio do header canónico é o shim
eglCreateWindowSurface→pbuffer, documentado no próprio ficheiro). A grelha,
o céu, o cubo, o gizmo e a UI PROVADOS em PIXELS (glReadPixels — 27 checks,
0 falhas; fwidth corre). Os 4 sintomas em captura ANTES (a zona azul = o
below-horizon do FS do céu — SkyRenderer.cpp `clamp(dir.y,0,1)`; a perda
TERM/INIT REAL recupera grelha+texto+cena no caminho fiel ao device; 2
falsos positivos REFUTADOS como artefactos do hospedeiro). A bisseção por
passe aferrada (7 passes vivos). O flash do STORM acende e morre (452205 px
pico→morto). ZERO linhas de produto tocadas · test_core/c33 intactos ·
raster_z2 27/0 · **versionCode 130** — o par declarado aqui e no build
config.

### §14-u — 0.12-Z2 F1 (o fuzzing do contexto + o invalidate sempre) — versionCode 131

**G8** (o par declarado no MESMO commit, a regra do §14): a FASE 1 da
campanha Z2 (o RELATORIO-0.12-Z2-RASTER.md §F1): o FUZZING no GLES REAL —
1050 ciclos de perda de contexto REAL (TERM+INIT do EGL, 7 seeds) com
cenários aleatórios (WE on/off, storm, gesto a meio) e o VEREDICTO por
hash de píxeis: o frame seguinte EXATO ao de referência em TODOS, o input
vivo nos 28 checks, ZERO falhas (o critério do dono: ≥1000 ciclos). O
ACHADO que os ciclos apanharam (R-070): o invalidateAll saltava as
camadas cujo recreate tinha falhado uma vez — o handle morto sobrevivia ao
contexto seguinte (o TEXTO MORTO após sair e voltar; a mono no OEM enxuto
falha SEMPRE). O fix: o invalidate corre SEMPRE (idempotente por contrato)
· pin do term duplo no contrato novo (inv0+6) · sentinela
regress_glctx_z2_fase1 · test_core 0 falhas · c33_virtual 1113/0 ·
raster_z2 --fuzz 1050/0 · gates VERDES · **versionCode 131** — o par
declarado aqui e no build config.

### §14-v — 0.12-Z2 F2 (o tom de chão + o varrimento + o ⌖) — versionCode 132

**G8** (o par declarado no MESMO commit, a regra do §14): a FASE 2 da
campanha Z2 (o RELATORIO-0.12-Z2-RASTER.md §F2): o below-horizon do céu
tinha o clamp `t=clamp(dir.y,0,1)` que pintava o meio-plano INFERIOR com a
cor do horizonte (a «zona azul/escura fixa que corta a grelha» do dono) —
a cura: o TOM DE CHÃO (horizonte×0.45 com a transição 0.18) na pura
skyGradient e no FS (a réplica linha a linha). O VARRIMENTO DE CÂMARA
(--sky2): 400 renders (200 poses × WE on/off — as bordas ±5000/±89°/dist
0.05..8000) — a grelha INTEIRA em todos (o pior pose 403 linhas) e ZERO
pixels pretos (0.00%); a GRELHA IMUNE à névoa (0 px mudam com fog 0→0.12).
A CÂMARA NUNCA PRESA: o ⌖ «Repor câmara» na barra (o glifo Reset) + o
duplo toque no vazio + a cameraEnsureSane que já existia — os dois
primeiros PROVADOS por input real no GLES. O VLM (createVision) confirma
as capturas. pin env_sky_type no contrato novo · sentinela
regress_sky_ground (R-071) · test_core 0 falhas · c33_virtual 1113/0 ·
--sky2 200/0 · gates VERDES · **versionCode 132** — o par declarado aqui
e no build config.
### §14-w — 0.12-Z2 F1-BIS (a auditoria adicional do ciclo de vida) — versionCode 133

**G8** (o par declarado no MESMO commit, a regra do §14): a AUDITORIA
ADICIONAL (o RELATORIO-0.12-Z2-AUDIT.md — os achados disjuntos dos das
F1/F2): o eglInitialize(nullptr) que matava o boot no glvnd (PROVADO por
probe — &maj/&mn); o swap verificado (o lastSwapOk_ alimenta a sonda); o
init parcial com auto-limpeza; o TERM resetA o input; o font-mono
OPCIONAL (a device sem mono já não fica presa no cooldown); o
Renderer::init pelo destroy + os loc* repostos; os prims na recuperação;
o default do NaN em dayCycleSpeed; a fuga de /tmp dos testes texbind
(RAII). O 2.º desvio do shim (RASTER_SURFACELESS=1) — a prova corre sem
X11. R-072 + o pin z2_audit_term_reseta_input (a mutação VERMELHA ao
vivo) · raster_z2 27/0 · raster_z2 --fuzz 150/0 · test_core 0 falhas ·
c33 1113/0 · **versionCode 133** — o par declarado aqui e no build
config.



### §14-x — 0.12-Z2 F3 (o clima visível + o P0 do uLifetime) — versionCode 134

**G8** (o par declarado no MESMO commit, a regra do §14): O ACHADO P0 —
o uniform uLifetime NUNCA era carregado pelo draw das partículas (o
uniform ficava 0.0 → mod(x,0)=NaN → TODOS os vértices morriam no clip —
o sistema NÃO desenhava NENHUM píxel no driver real desde a 0.12-Y-F1; o
stub do CI conta draws, a prova de píxeis da FASE 3 apanhou) + o CLIMA
VISÍVEL: o piso em píxeis (kMinParticlePx 3.0 — o max() com o w do centro
no VS), o emissor a seguir ao OLHAR da câmara (followOrigin + uSpawnScale,
os raios 6/5 pela densidade), o orçamento do FPS (a EMA 50/58, piso 0.25,
SÓ o clima), o spawnRadius no .goni ("rad" opcional) e no Inspector
(PeSpawn 5448 + o rowCount 8→9), o ACHADO F3 (a pausa não congelava os
relógios do render — clockFrozen), o NONE que repõe o jorro. As provas
POR PÍXEL: chuva 1431 / neve 1627 / trovoada 1986 px, o follow 6984 px a
2000 do mundo, o piso 1805 px a 200 m, a recuperação de contexto EXATA
(V9), a pausa/retoma (2335 px) — o --clima V1..V10 0 falhas + o VLM
(trovoada/neve visíveis). R-073 + os 4 pins novos (a mutação VERMELHA ao
vivo: o max() e o upload do uLifetime) · raster_z2 27/0 · raster_z2
--clima 0/0 · raster_z2 --fuzz 150/0 · test_core 0 falhas · c33 1113/0 ·
**versionCode 134** — o par declarado aqui e no build config.
