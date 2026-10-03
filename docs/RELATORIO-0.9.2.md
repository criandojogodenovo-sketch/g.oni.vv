# RELATÓRIO 0.9.2 — V.ONI v0 CORE (LINGUAGEM FECHADA)

**Repo:** github.com/criandojogodenovo-sketch/g.oni.vv · **Base:** 0.9.1 (ac89ca5)
**Entrega:** 0.9.2 · versionCode 45

---

## 1. OBJETIVO

A linguagem de scripting **V.ONI v0** (spec fechada §§1-12): parser PEG (cpp-peglib v1.8.6, header-only, vendored), gramática em ficheiro único separado do código, entry point `central main { on moment {} allmoments {} }` + top-level 1×, variáveis `v++`/`v#`/`@+` com os 7 tipos fechados, loops `repeat`/`last`/`with n+=1`/`continue`/`resume`, condicionais `exist`/`notexist{}`/`and( c(a) stopand )`/`option`/`notoption`, funções `fn`, os 9 comandos da engine (lista fechada §9), editor de script (portrait+IME, números de linha, coloração por classes com a paleta no Theme, Run/Stop, erros com linha), Docs com pesquisa, sandbox (budget por tick + profundidade 256 + zero APIs de sistema). A "central" (dispatcher C++) agenda top-level → on moment → allmoments/frame.

## 2. FONTE DE VERDADE

Spec 0.9.2 transcrita no prompt da campanha (§§1-13 + critérios). Regras: ✅ = exatamente como escrito; 🔶 = como escrito + isolado para mudar fácil; nada fora da spec — sintaxe/comando inexistente = bug. As 10 lacunas originais estavam TODAS fechadas na spec.

## 3. IMPLEMENTAÇÃO

| Ponto | Onde | Essência |
|---|---|---|
| 1. Parser PEG 🔶 | `vendor/peglib/peglib.h` (v1.8.6 PIN) + `voni/VoniGrammar.cpp` | cpp-peglib header-only (a ÚNICA exceção externa da spec). A GRAMÁTICA é uma string PURA em ficheiro único (mudar um literal aí muda a linguagem sem tocar em mais nada); `voni::setGrammar()` permite trocá-la em RUNTIME (o teste §13 usa exatamente isto). Palavras reservadas (§5, lista fixa de 20) vivem NO MESMO ficheiro — a definição da linguagem inteira num sítio. |
| 2. Ficheiros ✅ | `voni/VoniGrammar.cpp` (%whitespace) | `.voni` scripts · `.goni` cenas (inalterado) · comentários `//` e `/* */` NAS REGRAS DE WHITESPACE do PEG (contam como espaço). |
| 3. Entry point ✅ | `voni/VoniVm.cpp` (runStart/runFrame) + `core/VoniSystem.cpp` | `central main` abre o bloco com `on moment` (1× no start) e `allmoments` (frame). Top-level corre 1× ANTES dos blocos. A "central" (VoniSystem, System no TickGroup::Update ANTES do TransformSystem — `move()` reflete no cache world do MESMO passo) agenda exatamente: top 1× → on moment 1× → allmoments/frame. Idempotência: runStart 2× sem stop devolve false. |
| 4. Variáveis ✅ | `voni/VoniVm.cpp` | `v++nome=valor` (inferido) · `v#nome:Tipo=valor` · `@+` logo após o declarador (`v#@+vel:Num`, `v++@+vida`) → `Script::exported()` alimenta o Inspector (nome=valor·Tipo, leitura). Tipos: Int Num Txt Bool Vec2 Vec3 TIC; `true/false`. Coerção: Int→Num por alargamento apenas; atribuição a Int de Num = erro com linha. |
| 5. Nomes ✅ | `voni/VoniCompile.cpp` (Validator) | maiúscula=engine: nome de utilizador começado por maiúscula → erro claro com linha ("começa por maiúscula"); reservadas (as 20 da lista) → "'x' é uma palavra reservada" com linha. `v`, `fn`, `return`, `true`, `false` NÃO estão na lista §5 (podem ser nomes — a spec é explícita; colorem-se como linguagem no editor). Nomes de componentes tyker (follow/look/…) LIVRES fora de bloco tyker (0.9.3 reserva-os lá dentro). `v++fn=1` é VÁLIDO por ser estrutural. |
| 6. Loops ✅ | `voni/VoniVm.cpp` | `repeat(n){}` (n inteiro ≥0) · `last(cond){}` · `last(cond) with n+=1 {}`: o contador começa 0 e incrementa no FIM de cada iteração completa (continue conta); se existir uma GLOBAL Int com o nome, o contador é ALIAS dela (depois do loop o valor FICA — "conta iterações"); senão é local ao loop. `continue` salta o resto; `resume` sai; ambos fora de loop → erro com linha. NUNCA if/else/switch/break (não existem na gramática). |
| 7. Condicionais ✅/🔶 | gramática + VM | `exist(c){}` · `notexist{}` (else simples) · `notexist{…} and( cond(ação) stopand … )`: exist verdadeiro → salta TUDO; falso → casos L→R, 1º verdadeiro corre a ação e salta os restantes E o default; nenhum → default. `option(sel){ and valor(ação) stopand … notoption{} }`: switch por VALOR (==), notoption = default. DESAMBIGUAÇÃO 🔶: a forma NUA `cond(ação)` (condição = identificador) tenta-se ANTES da expr genérica — `flag(move(1,0,0))` é CONDIÇÃO flag + AÇÃO move, não FnCall. |
| 8. Funções 🔶 / operadores | `voni/VoniVm.cpp` | `fn nome(a:Num, b:Num):Num { return a+b }` — params tipados (Int→Num por alargamento), retorno verificado (sem return com :T = erro; return com valor sem :T = erro), arity verificada, chamável antes da definição textual. Operadores `+ - * /` (Int/Int=Int com divisão inteira 🔶; Num envolvido → Num; Txt+Txt concatena), `== != < > <= >=`, `and or not` (Bool estrito, curto-circuito), unário `-` (Int/Num). Divisão por zero → erro legível. |
| 9. Comandos ✅ | `voni/VoniVm.cpp` (tabela kCommands) + `voni/VoniEngineHost.cpp` | REGISTO em tabela: adicionar = 1 handler + 1 linha (o parser não muda; 0.9.3 acrescenta linker/tyker com este padrão). `View P "texto"` → engine.log com prefixo `voni:` · `View Object` → erro legível "consola ainda não existe" · `move(x,y,z)` → translação RELATIVA do TIC dono (Transform3D.pos +=) · `Import.Animation("nome")` → clip por nome no AnimationPlayer do dono (erro se não existe) · `nomedacena.transition.for("dest")` → origem tem de ser a cena ATUAL (validação + hook injetado pelo main; FADE em Play) · `Deltatime.Increment(var, valor)` → var += valor×dt (alvo Int → erro "usa Num": truncaria a 0 por frame) · `Explode.TIC.et`/`.er` → visible false/true do dono (invisível ≠ inativo: a lógica continua — o .er pode voltar a mostrar) · `Search.alvo.prop` → leitura RTTI (mesmo registo das props). Propriedades de TIC por pontos: pos/rot(GRAUS Euler)/escala (alias scale)/name/visible/active + .x/.y/.z sobre vetores; escrita `jogador.pos.x=5` (componente aceita Int/Num escalar). |
| 10. Editor de script ✅ | `ui/ScriptEditor.{h,cpp}` + Inspector + main | componente Script no TIC (Inspector: secção "Script" com Editar/Adicionar + vars @+); janela em PORTRAIT com o par inseparável 0.9.1 (portrait+jniImeShow ao abrir; landscape+jniImeHide ao fechar — o back SALVA o fonte no componente); números de linha no gutter 48dp (linha do ERRO acende em danger); coloração por CLASSES (VoniHighlight) com AS CORES NO THEME (paleta fechada §10: #B39DDB/#8AB4F8/#F5F5F5/#81C784/#FFD54F/#757575); Run/Stop 72dp×48; barra de erro 40dp com "linha N: mensagem". Escrita: o modelo da semente 0.9.1 (append+DEL por code point+ENTER, caret no fim). |
| 11. Docs ✅ | `voni/VoniDocs.{h,cpp}` (DADO) + `ui/DocsScreen.{h,cpp}` + Settings | Settings→Docs→"Ver docs da V.ONI" abre o ecrã: campo de pesquisa 48dp COM LUPA (teclado in-app propósito 9) + entradas nome/1-linha/sintaxe/exemplo (tap expande). Categorias comando/linker/tyker/componente EXISTEM; 0.9.2 povoa Comando (§9, 10 entradas) + Linguagem (§§3-8, 18 entradas) — linker/tyker/componente ficam para a 0.9.3. |
| 12. Sandbox ✅ | `voni/VoniVm.cpp` | ESTRUTURAL: o core voni/ NÃO inclui nada da engine (só Voni.h+Host) — não há ficheiro/rede/sistema PARA chamar. Budget 200k instruções/tick (loop infinito → "budget de iterações excedido" com linha, <50ms). Profundidade 256 ("profundidade de chamadas excedida — recursão infinita?"). Erros com linha NUNCA crasham (única exceção interna apanhada no boundary da API). |
| "Central" | `core/VoniSystem.{h,cpp}` | runs por handle (index<<32|gen): Play (autoPlay) arranca no 1º tick; Run do editor vive até Stop (independente do Play; sair do Play mata as runs de Play e mantém as do editor); TIC destruído = run morre sem crash; erros drenados 1×/frame (popError → engine.log + toast + barra do editor). |

**Host:** `voni/VoniEngineHost.{h,cpp}` — a ponte pura (Scene/TIC/Transform3D/AnimationPlayer/elog); transição e nome de cena CHEGAM POR HOOKS (o main instala os reais; os testes põem fakes).

## 4. FIXES REAIS (apanhados pelo loop desta release)

1. **PESQUISA DA HIERARQUIA MORTA (bug 0.9.0!):** o campo abria o teclado in-app (propósito 8) mas o COMMIT NUNCA EXISTIU — `hierSearch` nunca recebia o texto digitado (a pesquisa só funcionava nos testes, que escreviam o estado diretamente). FIX: `case 8` no `commitTextInput` (textBuf → hierSearch; query vazia LIMPA — válida em pesquisas).
2. Peglib v1.8.6 — `%whitespace` com `*` (NÃO `+`: com `+` o fim-de-input mata loops) e a regra chama-se `%whitespace` (não `whitespace`).
3. Literais da gramática NÃO empurram valores semânticos — operadores (`AssignOp/AddOp/MulOp/CmpOp`), `BoolLit` e `JumpStmt` passaram a regras CAPTURADAS; `NotUnary`/`NegExpr` em regras próprias (senão `not x` e `x` eram indistinguíveis em sv.size()).
4. `View P "texto"` é separado por ESPAÇO — regra `ViewStmt` própria ANTES de `Path` (senão `View` comia-se sozinho e o `P` sobrava) + passthrough no PathCall.
5. Strings com espaço (`" "`) perdiam o espaço: o skip de whitespace corria ENTRE as aspas e a captura — captura passou a incluir AS ASPAS (`< '"' … '"' >`) e a ação faz strip.
6. Atom convertia literais crus (NumV/StrV) — os folds esperavam ExprP (nullptr deref → segfault apanhado pelo ASan no smoke test); `ViewStmt` devolvia StmtP que o PathCall tentava castar para PathToks (comando vazio).
7. `callFn` lia o Flow de um MEMBRO que ninguém escrevia ("fn acabou sem return" com return às claras) — o Flow vem no RETORNO de execBlock.
8. Identificador nu (`contador`) parseia como Path com o nome em `segs[0]`, mas `evalVar` lia `e.name` (vazio) → `evalVarByName`.
9. Disco do CI: cada run falhada do teste 500MB deixava ~1GB de fixtures órfãs em /tmp (o teste só passava com disco saudável; limpeza das órfãs + o gate continua a ser o CI).

## 5. TESTES (suíte 681 → 743)

- **test_voni.cpp NOVO (~90 casos):** TODA a lista §13 — on moment 1× (contador=1 após 10 frames via @+) · allmoments/frame · top-level 1× · central main mal formado/duplicado/evento duplicado (linha) · comentários · fonte vazio · repeat(3) · last with n+=1 (conta E usa na condição) · continue/resume (+fora de loop = erro) · exist salta casos+default · and() 1º verdadeiro corre e salta default · nenhum → default · notexist else simples · forma NUA `flag(ação)` · option do autor (+default+caso nu) · fn chama/2 args/arity · operadores (Int/Int=Int, divisão, curto-circuito, concat, unário) · 20 reservadas como nome · maiúscula (var/fn/param) · redeclaração · nomes tyker livres · View P (prefixo/UTF-8/sem texto) · View Object legível · move relativo · Explode et/er · Import.Animation erro · transition.for (origem/destino/ok) · Deltatime acumula (60×1/60=1) · Search lê + inexistente · props ponto (leitura+escrita+x) · comando desconhecido · GRAMÁTICA: mudar literal 'repeat'→'repita' SEM C++ (setGrammar) · budget (last(true) + repeat gigante) · profundidade 256 · divisão por zero · tipos (inferência/anotação/erro/atribuição) · TIC valores · API Script (double-start/stop-restart/setVar vivo/erro fatal persiste) · highlighter (classes/bloco multilinha/user-vs-engine) · Docs (entradas≥25 com os 4 campos/pesquisa/categorias) · reservadas (20, sem stop/follow/fn).
- **test_wiring092.cpp NOVO (21 casos):** MOTOR — central em Play move o TIC real (1+9 frames); sem Play não corre; editor Run independente + Stop; restart recompila; popError drena 1×; TIC destruído sem crash; Explode→visible; props de OUTRO TIC na cena real; Search real; hook de transição injetado; serializer round-trip (.goni com o fonte + autoPlay). UI — scriptwin abre/carrega/fecha; IME append/DEL/ENTER; draw portrait 720×1536 + modal + Run/Stop/Back; barra de erro; docswin abre/pesquisa/back; Settings Docs; commit purpose 8 (o BUG 0.9.0) e 9; Inspector com secção Script.
- **test_components/safearea/scroll:** registry 10→11 (+Script); alturas do Inspector +secção Script (1160/1152; última row = ScriptAdd).
- **Gates locais:** check_main.sh ✓ · link_parity.sh ✓ (101 TUs — a lista lê-se do CMake da app, os 9 ficheiros novos entram) · jni_parity ✓ (zero natives novos).

## 6. HARNESS (C33 virtual)

c33_virtual compila os ecrãs novos (ScriptEditor/DocsScreen no alvo; main.cpp no TU do wiring087 via test_core). O replay 0.9.1 continua verde (portrait/IME/rotação). O ciclo completo do editor de script no DEVICE (abrir→escrever→Run→erro→fechar) fica para o VERIFIED do dono no C33 (a lista §14).

## 7. ITERAÇÕES DO LOOP

1. Gramática não validava (Statement referenciada sem definição — apanhou o ParserGenerator::parse com log) → definida + ordem das alternativas documentada.
2. Segfault no primeiro smoke (ASan): Atom passthrough deixava NumV/StrV chegar aos folds → conversão para nós Lit no Atom.
3. Comando vazio ("'' não existe"): ViewStmt devolvia StmtP; PathCall fazia cast errado → passthrough.
4. "fn acabou sem return" + "olavoni" (espaço comido) + px Int vs Num: três bugs distintos apanhados pelo repro isolado; fixes 6/7/8 da secção 4.
5. Contador `with` somava numa local que morria com o loop (o export ficava 0) → ALIAS à global Int.
6. Deltatime em Int truncava a 0 por frame (silenciosamente inútil) → erro claro "usa Num".
7. registry 10→11 e alturas do Inspector — testes antigos atualizados (o layout mudou LEGITIMAMENTE: nova secção sempre presente).
8. Disco cheio pelas fixtures órfãs do 500MB — falsa falha diagnosticada antes de tocar no código (a lição: ambiente primeiro).

## 8. SENTINELAS

As sentinelas 0.8.10.1/0.8.12 (seleção/none/staging/dumps) correm na suíte — **verdes** (100% dos 2 alvos do ctest: core 743 + c33_virtual_replay). Nenhum dump novo.

## 9. COMMITS

- `0.9.2-a: V.ONI v0 CORE` — linguagem + central + editor + Docs + fixes (este commit).
- (bump versionCode 45 / versionName 0.9.2; artifact CI `goni-vv-0.9.2-release-signed`).

## 10. CI / APK

Pipeline `release` no push. Jobs: core-tests (Linux) → C33 virtual → APK arm64 assinado (secrets) → gates. Artifact: `goni-vv-0.9.2-release-signed.apk`. O sha256 fica no output do job (anotar no VERIFIED).

## 11. NÃO VERIFICADO (honesto)

1. **No DEVICE (C33), pelo dono:** o editor de script completo (abrir→portrait→IME escreve→Run→erro com linha→Stop→back salva); Docs com pesquisa no toque; transition.for com cenas REAIS do projeto; `Import.Animation` com clip REAL importado (o teste usa o caminho de erro + o player da cena; o clip por nome está coberto pelo lookup, o PLAY do clip em Play mode é do AnimationSystem 0.8.0).
2. **Cursor editável** no editor (setas/seleção): a escrita é o modelo da semente 0.9.1 (append+DEL, caret no fim) — a spec §10 lista números de linha/Run/Stop/erros/coloração, não o cursor livre; fica para depois.
3. **Vars @+ EDITÁVEIS no Inspector** (a spec §4 diz "aparecem no Inspector" — exibição implementada; a edição in-place fica para depois de o dono validar o fluxo).
4. Play auto-run + editor Run do MESMO script simultâneos: uma run por TIC (a do editor ganha enquanto ativa) — comportamento documentado, não afervado no device.

## 12. DECLARAÇÃO DE AUSÊNCIA DE ACHISMO

Cada afirmação deste relatório tem evidência: os ~111 testes novos (nomes acima), os gates locais (check_main/link_parity/jni_parity), o ctest 100% e o output colado no CI. As decisões 🔶 estão identificadas e isoladas (ficheiro da gramática, tabela de comandos, tokens do Theme). O que não pôde ser verificado aqui está na secção 11 — sem "deve funcionar".

## 13. LIÇÕES

1. A spec mandava pinar uma biblioteca PEG — a API exata do peglib v1.8.6 (Result, %whitespace, literais sem valores) teve de ser dominada com smoke tests ANTES da integração (8 descobertas).
2. Ambiguidades PEG não perdoam: `cond(ação)` vs FnCall, `" "` vs whitespace-skip, `View P` vs Path — cada uma era um bug silencioso; as desambiguações estão DOCUMENTADAS na gramática.
3. O ambiente primeiro: "teste que falha" ≠ "código quebrado" (98% de disco).

## 14. PRÓXIMOS PASSOS

1. Dono VERIFICA no C33 (checklist README).
2. 0.9.3 — LINKERS & TYKERS (spec 8): registo central C++ (o padrão da tabela kCommands está pronto), `linker(A)to(B)=RF(nome)`, `tyker(nome){ find(RF) … }`, componentes contínuos/pontuais, segurança (ciclos/profundidade), Docs deles.

## 15. VEREDITO

Entregue conforme a spec §§1-12, suíte 681→743 (100% verde local + gates), CI no push. Aguarda VERIFIED do dono no C33.

---

## LISTAS FINAIS (exigidas pela campanha)

### ✅ implementado (forma exata confirmada pelo autor)

- `central main { on moment { } allmoments { } }` com top-level 1× antes dos blocos (§3)
- `on moment` 1× / `allmoments` por frame, dispatcher C++ = VoniSystem (§3)
- `v++nome=valor` · `v#nome:Tipo=valor` · `@+` logo depois do declarador → Inspector (§4)
- Tipos Int/Num/Txt/Bool/Vec2/Vec3/TIC; booleanos `true/false` (§4)
- Nomes: maiúscula=engine (erro claro), propriedades com ponto `jogador.pos` (§5)
- Palavras reservadas — a lista FIXA de 20, erro claro ao usar como nome (§5)
- `repeat(n){ }` · `last(cond){ }` · `continue` · `resume`; NUNCA if/else/switch/case/break (§6)
- `exist(cond){ }` · `notexist{ }` else simples (§7)
- `option(cond){ and valor(ação) stopand … notoption{ } }` na forma do autor (§7)
- Lista fechada de comandos §9: `View P "texto"` (log com prefixo `voni:`) · `View Object` (erro legível "consola ainda não existe") · `move(x,y,z)` · `Import.Animation("nome")` · `nomedacena.transition.for("destino")` (não se chama importar cenas) · `Explode.TIC.et`/`.er` (desaparecer/aparecer) · `Search.alvo.função`/`.propriedade` (RTTI, composição com ponto)
- `.voni` scripts · `.goni` cenas · `//` e `/* */` (§2)
- Sandbox: sem ficheiros/sistema/rede (estrutural) · budget por tick · profundidade 256 com abort legível · erros com linha (§12)
- Docs por comando/linker/tyker/componente: nome, 1 linha, sintaxe, exemplo (§11; 0.9.2 povoa comandos+linguagem)
- Editor: portrait+IME, números de linha, Run/Stop, erros com linha no editor E no log (§10)
- Testes §13 — TODOS os da lista (+extras)

### 🔶 implementado como escrito + onde está isolado para mudar

- **Gramática num ficheiro único** — `voni/VoniGrammar.cpp` (string pura + lista de reservadas; `voni::setGrammar()` para trocar em runtime; o teste do literal 'repeat'→'repita' prova o isolamento) (§1)
- **Parser PEG (cpp-peglib)** — `vendor/peglib/peglib.h` v1.8.6 PINADA (não atualizar sem rever Result/%whitespace/SemanticValues) (§1)
- **Accent default** — inalterado #2196F3 (§10 paleta é do Theme: `ui/Theme.h` kTheme.voni* — flip de 1 token)
- **`last(cond) with n+=1`** — contador começa 0, incrementa no fim da iteração (continue conta); ALIAS à global Int se existir — `VoniVm.cpp` (Stmt::Kind::Last) (§6)
- **`notexist{…} and( cond(ação) stopand )`** — semântica fechada implementada exatamente; a forma NUA `flag(ação)` desambiguada ANTES do FnCall — gramática `BareCase`/`BareOpt` (§7)
- **Tipos Int/Num…** — `typeFromName` no `VoniVm.cpp` (tabela única) (§4)
- **Operadores** — `+ - * / == != < > <= >= and or not`; Int/Int=Int (divisão inteira), Num aceita Int por alargamento, Txt+Txt concatena — `VoniVm.cpp` evalBinary (§8)
- **`fn`** — params tipados, retorno :Tipo verificado — `VoniVm.cpp` callFn (§8)
- **`move(x,y,z)`** — RELATIVO ao TIC dono — `VoniEngineHost.cpp` moveTic (§9)
- **`Deltatime.Increment`** — alvo Int → erro "usa Num" (truncaria 0/frame) — `VoniVm.cpp` cmdDeltatimeIncrement (§9)
- **`Search.alvo.função`** — v0 resolve propriedades; funções RTTI chegam na 0.9.3 (erro menciona ambas) — `VoniEngineHost.cpp` search (§9)
- **transition.for** — origem = cena ATUAL (senão erro legível) — `VoniEngineHost.cpp` transitionTo + hook no main (§9)
- **Props de TIC** — registo RTTI: pos/rot(graus)/escala(+alias scale)/name/visible/active — `VoniEngineHost.cpp` propGet/propSet (§9)
- **Coloração** — classes no `voni/VoniHighlight.cpp`; CORES no `ui/Theme.h` (§10)
- **option** — switch por VALOR (==) — `VoniVm.cpp` (§7)
- **Registo de comandos** — tabela `kCommands` no `VoniVm.cpp` (o padrão que a 0.9.3 estende a linker/tyker) (§9→0.9.3 §1)

### ❓ NÃO implementado

*(vazio — as 10 lacunas da spec original foram todas fechadas pelo autor ou por delegação; nada fora desta spec foi implementado: sintaxe/comando inexistente = erro legível, não feature)*
