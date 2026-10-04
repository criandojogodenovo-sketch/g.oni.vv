# RELATÓRIO 0.9.5 — CLÁUSULA P-01: LINKERS & TYKERS COMPLETOS + O EDITOR QUE ENSINA

> versionCode 48 · 0.9.5 · duas metades sequenciais (commit cada) · suíte 796→803 casos core + 209→259 checks do c33_virtual (FASE 11 completa: 11.A replay linkers/tykers + 11.B lookups de ajuda + walkthrough) + 6 gates + sentinelas R-001..R-013

## 0. COMO LER ESTE RELATÓRIO

Duas metades, um commit cada, CI verde após cada uma: **METADE 1** (commit `d8b3f80` — os linkers & tykers completos sobre a spec fechada) e **METADE 2** (o editor que ensina por cima do registo que a metade 1 criou — commit deste fecho). As secções 1–16 são afetas por metade (1–9 = METADE 1; 10–16 = METADE 2 + fecho). A rastreabilidade ponto→decisão→fix→teste→linha-do-harness está na secção 15. As provas de mutação obrigatórias (ciclo e RF-em-falta) estão coladas na secção 14. O NÃO VERIFICADO honesto está na secção 16.

## 1. METADE 1 · O QUE FOI ENTREGUE (spec fechada, item a item)

| Item da spec | Estado | Onde |
|---|---|---|
| `linker(A)to(B)=RF(nome)` com RF obrigatória; A,B = objeto/TIC/propriedade/animação | ✅ | gramática `LinkerDecl` (VoniGrammar.cpp); sem o `=RF(...)` a declaração deixa de ser um linker e o **runtime ensina** a forma completa |
| `tyker(nome){ find(RF) comps… }`; `find` primeiro obrigatório; RF inexistente → erro legível, tyker não corre; vários tykers partilham RF | ✅ | gramática `TykerDef`/`TykerFind` + validação (erros com linha) + `initTykers` no VoniVm |
| Componentes contínuos: `follow()/follow(d)/follow(d,suav)`, `look()`, `orbit(d,vel)`, `copy(prop)`, `map()` (no-op sem mapa) | ✅ | VoniTykers.cpp (handlers) |
| Componentes pontuais: `Change(origem|destino)to(x)`, `point()/point(x,y,z)`, `colorpars(cor)(nome|#RRGGBB)`, `play()`, `limit(min,max)`, `delay(s)` | ✅ | VoniTykers.cpp |
| Reservado no-op `shading()` | ✅ | aceita, valida e não faz nada (docs dizem "reservado") |
| Ciclos a→b + b→a rejeitados com erro legível | ✅ | `Registry::addLinker` (direto + DFS para os longos) |
| Profundidade 256 abort legível | ✅ | o DFS do registo tem teto `kMaxDepth=256` |
| Nunca crash | ✅ | todos os handlers devolvem `false + err` (a Vm transforma em erro com linha); budget de instruções cobre o tick dos tykers |
| Teste exemplo 8.5 no C33 | ✅ (scriptado) / ❓ (humano — secção 16) | `exemplo_8_5_no_c33` (test_voni.cpp, 4 mini-scripts) + FASE 11 |
| **Registo central C++**: cada linker/tyker/componente declarado no registo; adicionar componente novo NÃO obriga a mudar o parser; Docs obrigatória por entrada | ✅ | voni/VoniRegistry.cpp (tabela com nome+sintaxe+desc+exemplo+argc+handler) + o teste `parser_independente_do_registo` |

## 2. O REGISTO CENTRAL (a decisão arquitetural da entrega)

Toda a superfície da linguagem vive AGORA numa tabela de dados (`voni/VoniRegistry.cpp`): as 19 entradas de Linguagem (§§3-8), os 10 comandos (§9), as formas linker/tyker/find e os 13 componentes — cada uma com **nome, sintaxe, descrição de 1 linha, exemplo, equivalência Python/JS, esqueleto de Tab, argc e (para componentes) o ponteiro do handler**. A gramática só conhece as FORMAS fixas (`linker(A)to(B)=RF(nome)`, `tyker(nome){…}`, `Change…to…`, `colorpars(cor)(cor)`); o CORPO do tyker é genérico (nome + caudas de args) e é o REGISTO que decide o que existe — a validação de nome/argc no compile consulta a tabela e a mensagem de erro usa a PRÓPRIA SINTAXE da entrada ("follow: escreve assim — follow() · follow(d) · follow(d,suav)").

**A prova de que adicionar componente não muda o parser** é o teste `parser_independente_do_registo`: instala um componente `ecoteste` no registo (hook `installForTest` só para testes), compila e corre um tyker com `ecoteste(7)` — a gramática nunca ouviu falar dele e tudo funciona (parse → validação contra o registo → dispatch no tick). O hook é removido com `resetForTest`.

## 3. AS DECISÕES SEMÂNTICAS DA METADE 1 (zero achismo — cada uma afervável)

1. **`find(RF)`** lê-se `find(nome-do-RF)` — o parêntesis traz o NOME do RF (a metassintaxe da spec usa RF como placeholder, como A/B/nome no linker).
2. **Tykers ticam a cada frame, DEPOIS dos allmoments** — o comportamento do linker "fecha" o frame. Um script SÓ com linkers/tykers (sem central main) comporta-se na mesma (o runFrame deixou de cortar por "sem allmoments").
3. **Componentes pontuais disparam 1× na ATIVAÇÃO** (depois do `delay(s)` se houver); contínuos correm por frame. O `delay` é lido no arranque (define QUANDO ativa); vale o último declarado.
4. **Args de componentes resolvem 1× na ativação** — literais OU variáveis do script (`follow(distancia)` funciona; testado).
5. **Ciclo = fatal no arranque com a linha** (o script não corre ambíguo); **RF em falta = log 1× + tyker desligado** (o resto do script segue) — a spec manda "rejeitados" para o ciclo e "tyker não corre" para o RF; a canalização dos dois é distinta de propósito.
6. **`colorpars(cor)(valor)`**: o parâmetro `'cor'` escreve a nova propriedade RTTI `cor` (Vec3 = o tint do MeshRenderer — o TIC de origem TINGE visivelmente); outros nomes de parâmetro ficam guardados no estado do tyker para o `shading()` futuro. Paleta fechada: vermelho/verde/azul/amarelo/branco/preto/cinza + `#RRGGBB`.
7. **`play()`** toca a ponta do linker que é NOME DE ANIMAÇÃO (não TIC) no TIC dono do script (o `Import.Animation` da casa).
8. **`point()`** limpa o ponto (volta ao destino vivo); `point(x,y,z)` fixa o alvo absoluto de follow/look/orbit. `Change` reescreve o lado de TODOS os links do RF (o RF é partilhado — a mudança afeta todos os tykers que o usam; testado).
9. **`limit(min,max)`** aplica-se à distância da origem ao alvo (follow e orbit); se o movimento cai EM CIMA do alvo, o limite empurra para fora na direção de onde veio.
10. **RFs são por script** (vários tykers do MESMO script partilham; os links referem TICs por nome através do Host — cross-TIC por natureza).

## 4. A GRAMÁTICA (só as formas; o resto é dado)

```
LinkerDecl  <- 'linker' '(' Path ')' 'to' '(' Path ')' '=' 'RF' '(' IdentAny ')'
TykerDef    <- 'tyker' '(' IdentAny ')' '{' TykerItem* '}'
TykerItem   <- TykerFind / ChangeComp / CompCall / Statement
TykerFind   <- 'find' '(' IdentAny ')'
ChangeComp  <- 'Change' '(' ChangeSide ')' 'to' '(' Path ')'
CompCall    <- IdentAny CompTail CompTail? !'{'
CompTail    <- '(' (CompArg (',' CompArg)*)? ')'
CompArg     <- ColorLit / Expr
ColorLit    <- < '#' HexD HexD HexD HexD HexD HexD >
```

O `Statement` continua na lista do `TykerItem` DE PROPÓSITO: um `View P` ou `repeat(3){}` dentro do tyker PARSEIA e é o VALIDADOR que rejeita com erro que ensina ("o corpo do tyker 't' só aceita componentes — 'repeat' é código de script (pertence ao central main)") em vez de "erro de sintaxe" seco. O `!'{'` do `CompCall` faz o `repeat(3){}` cair no Statement em vez de encravar a parse. O `linker`/`tyker` fora da forma completa (ex.: `linker(a)to(b)` SEM o RF) degrada para comando desconhecido e o RUNTIME ensina a forma completa.

## 5. O RUNTIME (voni/VoniTykers.cpp + o fio no VoniVm.cpp)

`Script::Impl` ganha o `tykers::Registry rfReg` (RFs → links, por script) e os `tykerRuns` (estado por tyker: missing/activated/elapsed/delay/point/limit/orbitAngle/colorParams/contínuos resolvidos). O `runStart` regista os linkers EM ORDEM (ciclo → fatal legível com a linha), resolve os finds (RF em falta → log "voni: RF 'x' não encontrada — o tyker 'nome' não corre (linha N)" + `missing=true`), avalia os delays. O `runFrame` (reestruturado) corre os allmoments SE existirem e tic os tykers SEMPRE: ativação (pontuais disparam; contínuos resolvem args e ficam na lista) e o tick contínuo com `vm.spend()` por componente (o budget dos 200k cobre os tykers — nunca hang). A propriedade RTTI **`cor`** (Vec3, tint do MeshRenderer) entrou no EngineHost para o colorpars — e ficou exposta como propriedade de TIC geral (documentada nas Docs).

## 6. TESTES DA METADE 1 (759 → 796 casos core)

+32 no test_voni (declaração/registo de RFs; find-primeiro com erro que ensina; corpo só componentes; componente desconhecido; argc com a sintaxe do registo; ciclo direto/longo/profundidade-300; follow cola/distância/suav; look yaw; orbit 90°/s; copy escala; map+shading no-op; Change reescreve o RF partilhado; point fixa/limpa; colorpars hex/nome/outro-parâmetro/cor-desconhecida; play com/sem animação; limit; delay; args como variáveis; tykers partilham RF; sem central main; **parser_independente** (a prova); registo docs obrigatórias; docs incluem o registo; **exemplo 8.5**); +3 motor no wiring092 (follow move o Transform3D REAL; colorpars tinge o MeshRenderer REAL; ciclo no editorStart com o erro); +2 sentinelas (R-011/R-012 — secção 13).

## 7. FASE 11.A DO HARNESS (209 → 230 checks)

O replay no caminho REAL do app: o script com linker/tyker viaja NO .goni (o serializer salta ScriptComp vazio — a fonte entra antes do save), o editor abre a fonte intacta, o Run REAL arranca, `follow(2)` deixa o Ator a 2 do Alvo (15→7), `colorpars(cor)(#FF8800)` tinge o MeshRenderer, o RF fantasma LOGA o erro exato com o tyker BOM a mexer e o allmoments vivo, o CICLO acende a barra de erro com linha + "ciclo" + os lados, e as Docs pesquisam follow/colorpars com as categorias novas povoadas. (Nota de iteração: o harness não passa pelo `android_main` — onde o `g_systems` se registra — por isso o tick é dado DIRETO ao VoniSystem, o mesmo passo que o loop real dá.)

## 8. AS SENTINELAS NOVAS (docs/REGRESSOES.md)

**R-011 regress_linker_ciclo_rejeitado** — ciclo direto (com linha + os nomes + o RF) · ciclo longo (DFS) · 3 linkers VÁLIDOS correm limpos (o guard não caça inocentes) · cadeia de 300 com o abort de profundidade. **R-012 regress_rf_em_falta** — o erro EXATO da spec com o nome do RF e do tyker · o tyker bom corre (o script não morre) · o allmoments segue · o log acontece 1× (não faz spam por frame) · RF totalmente vazio idem · o estado `missing` desliga o tyker.

## 9. PROVA: ADICIONAR COMPONENTE NOVO NÃO MUDA O PARSER

O teste `parser_independente_do_registo` instala `ecoteste` via `reg::installForTest("ecoteste", handler, syntax, desc, example)` — 1 handler + 1 entrada de registo, ZERO mudanças na gramática — e o ciclo completo funciona: a fonte `tyker(t){ find(p) ecoteste(7) }` parseia (o corpo do tyker é genérico), valida (argc contra o registo), corre (o dispatch do registo chama o handler) e o handler viu o `7`. Depois `reg::resetForTest()` e o componente desaparece (o find e o handlerFor voltam null). É o CONTRATO do registo afervado por teste, não por promessa.

## 10. METADE 2 · O QUE FOI ENTREGUE (uma só fonte alimenta tudo)

| Item da spec | Estado | Onde |
|---|---|---|
| A lista de comandos, a tabela de equivalências, os erros-que-ensinam, os tooltips, as Docs, o completamento e o copiar-referência saem TODOS do registo da metade 1 | ✅ | docs::all() é VISTA 1:1 do registo; kForeign (erros); reg::find/prefixMatch (strip/completamento); fullReferenceMarkdown (clipboard + ficheiros) |
| Teste de bijeção registo↔Docs↔erros↔tooltips (R-013) | ✅ | `regress_bijeção_da_ajuda` (6 frentes — secção 13) |
| Esqueletos por Tab: `exist`+Tab → `exist(){ } notexist{ }`; idem `option`, `repeat`, `tyker` | ✅ | applyEvent(Key::Tab) + skeleton/skeletonCaret no registo; TAB no teclado in-app E keycode 61 do GBoard |
| Toque numa palavra → linha de explicação com exemplo, vinda da Docs | ✅ | o tap no corpo resolve a palavra sob o dedo (linha/coluna → offset em bytes) e a strip acende com desc+exemplo |
| Mini-descrição em tempo real desde a 1ª letra (prefixo-match numa strip fina junto à barra de erros) | ✅ | helpStripLine1/2 + wordBeforeCaret + prefixMatch; strip de 32dp (1 linha) ou 56dp (2) — some quando não há nada |
| Níveis Iniciante / Normal / Silencioso | ✅ | botão I/N/S na toolbar do editor; Iniciante=desc+exemplo, Normal=desc (exemplo só no toque), Silencioso=nada |
| Sem encher o ecrã | ✅ | a strip só existe quando há casamento; alturas 32/56dp |
| Copiar referência para IA (botão que emite a referência completa como texto colável) | ✅ | botão 📋 → result 6 → `jniClipboardCopy` → VvActivity.clipboardCopy (ClipboardManager) + toast + log |
| Referência pública p/ web+IAs: `VONI_referencia.md` na raiz + `llms.txt`/`llms-full.txt` apontando para ela | ✅ | os 3 ficheiros commitados, GERADOS do registo (voni_refgen); o CI afere a sincronia byte a byte (R-013 item 6) |
| O curso e a Docs derivam do mesmo registo | ✅ | as Docs são a vista; a referência pública é a mesma saída; nada é editado à mão |
| Teste de eficácia (<10 min só com a ajuda do editor) | ✅ (scriptado no harness) / ❓ (humano — secção 16) | FASE 11.12: o walkthrough monta um script FUNCIONAL só com esqueletos+Tab+IME e o follow move o TIC |

## 11. OS ERROS-QUE-ENSINAM (a ponte para quem sabe Python/JS)

A tabela `kForeign` do registo (16 palavras: if/else/elif/while/for/break/switch/case/def/function/print/echo/True/False/None/null) alimenta DOIS ganchos: (a) o **erro de sintaxe** varre a LINHA do erro (a palavra fatal pode ter ficado atrás do ponto onde o parser morreu — `if (x) { }` morre no `{`) e ensina ("'if' não existe na V.ONI — chama-se exist: exist(condição){ }"); (b) o **runtime** — `break`/`print` à solta PARSEIAM como comandos e é o `runCommand` que ensina o equivalente. Toda a dica aponta a uma entrada REAL do registo (a sentinela R-013 afere). A Docs ganhou a linha "Python/JS: …" por entrada (a tabela de equivalências visível).

## 12. A REFERÊNCIA PÚBLICA (p/ web + IAs)

`fullReferenceMarkdown()` (no registo) gera o documento completo (cabeçalho com as regras de ouro + secções Linguagem/Comandos/Linkers/Tykers/Componentes com sintaxe/descrição/equivalência/exemplo por entrada). O gerador `tests/voni_refgen` escreve-a; os ficheiros `VONI_referencia.md` e `llms-full.txt` (idênticos — a convenção llms: o índice aponta, o full contém) e o `llms.txt` (índice com o apontamento) foram commitados NA RAIZ. O item 6 da R-013 regenera e compara BYTE A BYTE no CI — a referência NUNCA mente nem envelhece.

## 13. AS SENTINELAS + O TESTE DE BIJEÇÃO

**R-013 regress_bijeção_da_ajuda** (test_sentinels.cpp): (1) registo↔Docs com MESMOS campos e MESMO número (nada nas Docs vem de fora); (2) Docs obrigatória + EQUIV preenchido em TODA a entrada + esqueletos bem-formados (caret ≤ strlen); (3) toda a palavra estrangeira aponta a uma entrada REAL; (4) prefixMatch acha toda a entrada pelo seu próprio nome; (5) os 4 esqueletos obrigatórios com o TEXTO EXATO da spec; (6) a referência commitada == à gerada BYTE A BYTE (VONI_referencia.md e llms-full.txt) e o llms.txt aponta para ela. A prova de mutação implícita: editar o ficheiro à mão (ou acrescentar entrada sem regenerar) = CI vermelho no item 6.

## 14. PROVAS DE MUTAÇÃO (obrigatórias: ciclo e RF-em-falta — evidência colada)

**R-011 (ciclo)** — mutação: os dois ramos de guarda de `Registry::addLinker` desligados (`if (false && …)` no ciclo direto e no DFS):
```
ciclo_direto_rejeitado FALHOU · ciclo_longo_rejeitado FALHOU · exemplo_8_5_no_c33 FALHOU
profundidade_256_aborta_legivel FALHOU · regress_linker_ciclo_rejeitado FALHOU
voni_motor_ciclo_erro_no_poperror_do_editor FALHOU        (27 FALHOU no total no core)
  FALHOU tests/test_sentinels.cpp:991 !started · :992 !err.ok · :1010 "ciclo" · :1041/1042 profundidade/256
harness 11.4: [FAIL] a run NÃO arranca · a barra tem LINHA · diz CICLO · nomeia os lados (4 FAIL)
== C33 VIRTUAL: 230 check(s), 4 falha(s) ==
```
Reposta a guarda: **796/0 core + 230/230 harness** (ficheiros /home/z/my-project/mutacao-R011-{vermelho,verde}.txt).

**R-012 (RF em falta)** — mutação: a deteção desligada em `initTykers` (`if (false && !st.rfReg.findRf(...))`):
```
exemplo_8_5_no_c33 FALHOU · regress_rf_em_falta FALHOU · rf_em_falta_o_tyker_nao_corre FALHOU (9 no core)
  FALHOU tests/test_sentinels.cpp:1062 "tyker 'mau' não corre" · :1076/1085 n==1 (o log 1×) · :1096/1098
harness 11.3: [FAIL] o log traz o ERRO EXATO · o log diz QUAL tyker (2 FAIL)
== C33 VIRTUAL: 230 check(s), 2 falha(s) ==
```
Reposta: **796/0 core + 230/230 harness** (ficheiros /home/z/my-project/mutacao-R012-{vermelho,verde}.txt).

## 15. RASTREABILIDADE (ponto → decisão → fix → teste → linha do harness)

| Ponto | Decisão/fix (ficheiro:função) | Teste | Harness |
|---|---|---|---|
| `linker(A)to(B)=RF(nome)` | VoniGrammar.cpp `LinkerDecl` (com os parênteses das formas) | linker_declara_e_regista_no_rf | 11.1 (a fonte com linkers abre e corre) |
| RF obrigatória | VoniVm.cpp `runCommand` §3.5 (ensina a forma) | linker_sem_rf_erro_que_ensina | — |
| `tyker(nome){ find(RF) … }` | VoniGrammar.cpp `TykerDef`+`TykerFind`; VoniCompile.cpp `validateTykers` | tyker_find_obrigatorio_de_primeiro | 11.12 (o Tab traz o find no esqueleto) |
| Corpo só componentes (erros que ensinam) | VoniCompile.cpp `validateTykers`/`checkComp` + `!'{'` na gramática | tyker_corpo_so_componentes_ensina | 11.6 |
| Registo central + parser intocado | VoniRegistry.cpp (tabela) + installForTest | parser_independente_do_registo; registo_docs_obrigatorias | 11.5 |
| follow/look/orbit/copy/map | VoniTykers.cpp compFollow/Look/Orbit/Copy/Map | 5 casos dedicados | 11.1 (follow move o Ator) |
| Change/point/colorpars/play/limit/delay/shading | VoniTykers.cpp (handlers pontuais) | 8 casos dedicados | 11.2 (colorpars tinge) |
| Ciclos + profundidade 256 | VoniTykers.cpp `Registry::addLinker` (direto+DFS+cap) | ciclo_direto/longo + profundidade_256 + R-011 | 11.4 (a barra de erro) |
| RF em falta | VoniVm.cpp `initTykers` (log+missing) | rf_em_falta_o_tyker_nao_corre + R-012 | 11.3 |
| Exemplo 8.5 | — | exemplo_8_5_no_c33 (4 mini-scripts) | 11.1–11.4 |
| Docs obrigatória por entrada | VoniDocs.cpp (vista) + registo | registo_docs_obrigatorias + R-013(1)(2) | 11.5 |
| Uma só fonte (M2) | VoniRegistry.cpp (equiv/skeleton/kForeign/fullReference) | R-013 (6 frentes) | 11.6–11.11 |
| Esqueletos por Tab | ScriptEditor.cpp applyEvent(Key::Tab) + ImeQueue Key::Tab | scriptwin_tab_os_esqueletos_da_spec | 11.8 + 11.12(3) |
| Mini-descrição desde a 1ª letra | ScriptEditor.cpp wordBeforeCaret/helpStripLine1 | scriptwin_strip_mini_descricao_desde_a_1_letra | 11.7 |
| Toque numa palavra | ScriptEditor.cpp wordAtOffset + o tap do draw | scriptwin_toque_na_palavra_explica_com_exemplo | 11.9 |
| Níveis I/N/S | ScriptEditor.cpp helpLevel + o botão | scriptwin_botao_nivel_cicla | 11.10 |
| Copiar referência | main.cpp (result 6) + StorageBridge jniClipboardCopy + VvActivity.clipboardCopy | scriptwin_botao_copiar_referencia_devolve_6 | 11.11 (JNI + texto completo) |
| Referência pública | voni_refgen + VONI_referencia.md/llms.txt/llms-full.txt | R-013(6) sincronia byte a byte | 11.11 |
| Walkthrough <10 min | — | — | 11.12 (o script FUNCIONA: follow 15→7) |

## 16. NÃO VERIFICADO (honesto — o que SÓ o device humano afere)

1. **Os 60 fps no C33 com tykers ativos** — o harness afere o comportamento, não o frame time do device; verificar com a barra de status + um script com 2 tykers (follow+look) e painéis abertos.
2. **O Tab do GBoard REAL** — o keycode 61 chegou pela fila no harness; confirmar que o teclado do device envia KEYCODE_TAB (alguns IMEs só mandam texto — o TAB do teclado IN-APP está lá como garantia).
3. **O clipboard REAL** — o botão 📋 foi afervado contra o JNI fake (a chamada + a string completa); colar num bloco de notas no device para confirmar o ClipboardManager.
4. **A strip fina em uso prolongado** — não enche o ecrã por construção (32/56dp, some sem casamento); confirmar o conforto real nos níveis I/N/S.
5. **O teste humano dos <10 minutos** — o walkthrough scriptado (11.12) prova o MECANISMO (um script funcional montado só com esqueletos+Tab+IME); cronometrar uma pessoa REAL que saiba Python/JS no C33 (checklist do README, item 10).
6. **O ciclo/RF no device** — os erros legíveis foram afervados no harness e na suíte; ver as barras vermelhas reais no editor do C33 (checklist 7/8).

## 17. NOTA SOBRE A SENTINELA R-010 ("herdada")

A task menciona "SENTINELAS: R-011 … R-012 … R-013 … (R-010 herdada)". **O docs/REGRESSOES.md termina em R-009** (a FASE 9 fechou com R-001..R-009) e NENHUM ficheiro do repo define R-010 — a definição não sobreviveu ao contexto da sessão anterior. DECISÃO (zero achismo): NÃO se inventou um R-010; em vez disso, TODAS as sentinelas herdadas R-001..R-009 foram mantidas VERDES nas duas metades (a suíte completa corre em cada commit), e a R-010 fica registada AQUI como ausência documentada — quando o dono fornecer a definição, entra como sentinela nova com o número dela.

## 18. AS ITERAÇÕES DO LOOP (o que o processo apanhou)

1. **A gramática do linker sem os parênteses das formas** — `linker Path to Path` não parseava `linker(a)to(b)`; o erro no col 9 ("unexpected '{', expecting View/IdentAny") mostrou o Statement a comer `tyker(t)` como PathCall. Corrigido para as formas com parênteses (e o `tyker(nome)` idem).
2. **O ScriptComp vazio não viaja no .goni** — o serializer salta `source.empty()` (razoável desde 0.9.2); a FASE 11 passou a gravar o script ANTES do save (o cenário real de um script que já existia).
3. **O harness não passa pelo android_main** — o `g_systems` (onde o VoniSystem se registra) fica vazio; o tick passou a ser dado DIRETO ao `g_voni.tick` (o mesmo passo do loop real; documentado no passo 11.1).
4. **O backspace só apaga ANTES do caret** — o walkthrough limpava o buffer com o caret no meio (sobrou " } notexist{ }"); a sequência passou a mandar o caret ao FIM (setas →) antes do backspace.
5. **A matemática do caret no placeholder** — LEFT×4 apagava "(d" em vez de "RF"; LEFT×2+DEL×2 corrige; e o " follow(2)" entrava DENTRO dos parênteses do find — um RIGHT saltou o ')'.
6. **Os erros-que-ensinam em duas camadas** — o 'if' morre no PARSE (varre-se a linha do erro, não o error_pos) e o 'break'/'print' morrem no RUNTIME (o runCommand consulta o kForeign) — os dois ganchos foram necessários.
7. **O flake de runners do GitHub** — o CI da METADE 1 morreu 2× no passo "Compilar" com "runner has received a shutdown signal" (SEM erro de código; o job c33-virtual compilou a MESMA fonte e passou); re-corrído o job (a mesma classe de exit-143 documentada no RELATORIO-0.9.4 de 2026-10-04).
8. **wiring010 no host** — a falha intermitente do import 500MB era DISCO (6×500MB de /tmp de runs anteriores); limpo o /tmp e verde (a lição já documentada na FASE 9).

## 19. A EVIDÊNCIA (colada)

- **METADE 1**: commit `d8b3f80` (0.9.5-a) — suíte local 796/0 core + 230/230 harness + 6 gates verdes (check_main, link_parity **104 TUs** — os 2 novos sem comentário de propósito para o gate os apanhar, jni_parity, projects_ui_check, reload_concurrency, glyph_source_check); CI run 37204025800 (o flake de runner documentado na secção 18.7 — re-corrído; o job c33-virtual verde na 1ª).
- **METADE 2** (este commit): suíte local **803/0 core + 259/259 harness** + os mesmos 6 gates; FASE 11 completa (11.A + 11.B com o walkthrough).
- O output integral da FASE 11 (28 checks novos: 11.1–11.5 + 11.6–11.12) está no log do ctest do CI; os trechos com as provas de mutação estão colados na secção 14.
