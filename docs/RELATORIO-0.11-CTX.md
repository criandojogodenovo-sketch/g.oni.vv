# 0.11-CTX — RELATÓRIO DA CAMPANHA (contexto enxuto para o GLM)

Data: 2026-10-16 · vc: 76 · Campanha SÓ de documentação (a cláusula
calma: zero código de produção; o CI só é tocado no gate docs-lint).

## §1 O que foi feito

Os dois índices lidos sempre + o arquivo do histórico + o template de
prompts + o gate STATE.md no CI:

- **STATE.md** (raiz, 143 linhas, §1–§9) — o estado do projeto, as
  decisões fechadas, os bloqueios, os ponteiros de detalhe e as 4
  perguntas antes de qualquer trabalho. CTX-1, commits deba7bb +
  ac2b780 (o fix do bit de execução do gate).
- **CODEMAP.md** (raiz, 205 linhas, §1–§6) — onde vive o quê + 8
  receitas de extensão, incluindo a CRÍTICA dos DOIS CMakeLists. A
  convenção de paths é relativa à RAIZ do repo, cumprida em todas as
  linhas (o pin aferra). CTX-2, commit 1a348f5.
- **docs/arquivo/** — 71 relatórios das campanhas FECHADAS movidos com
  `git mv` (0.6.x–0.9.6.x — EXCETO o RELATORIO-0.9.6.md, ver abaixo —,
  0.10-M completa, 0.11-H, 0.11-M, 0.11-S, 0.11-T, F5.5, F6). Ficam em
  `docs/`: o RELATORIO-0.9.6.md (a âncora VIVA do gate release-identity
  R-015 — o versionName 0.9.6 ainda é a identidade da release e o §14
  acumula o log dos vcs; movê-lo deixa o CI VERMELHO, que foi
  exatamente o que aconteceu e o gate apanhou), os 4 relatórios da
  0.11-W (campanha em curso — o fecho é o BLOCO B) e os 2 da
  0.11-T-FIX (Perna 2 adiada pelo dono — a campanha não está fechada).
- **docs/DIARIO.md** (4 553 linhas) — o conteúdo INTEGRAL do README
  antes da CTX-3, movido sem edição (a regra «não apagar, mover»).
- **README.md** cortado de 4 541 linhas / 292 344 bytes para 85
  linhas / 4 374 bytes — índice de leitura + campanha em curso + build.
- **docs/TEMPLATE_PROMPT.md** — a pirâmide de leitura (Nível 0 =
  STATE + CODEMAP + o prompt; Nível 1 e Nível 2 só quando nomeados).
- **Gate STATE.md no CI** — o passo «Gate STATE.md (0.11-CTX §1.2)» no
  job core-tests do release.yml corre `scripts/docs_lint_state.sh` em
  cada push (o mesmo motor que o pin ctx_state_gate_fecha aferra
  contra o histórico real). Commits de docs puro são isentos.

## §2 As provas — pins e mutações coladas

17 pins ctx_* no TU tests/test_ctx_pines.cpp (o test_core do CI corre
os 17 a cada push): ctx_state_existe/curto/seccoes/vc_bate/gate_fecha
+ ctx_codemap_existe/curto/seccoes/paths_uniformes/
ficheiros_existem/receitas + ctx_arquivo_existe/diario_existe/
readme_curto/template_existe/refs_ok.

As mutações (quebrar → vermelho → repor → verde; corridas no harness
local com GONI_ROOT_OVERRIDE contra uma cópia do repo):

- **M-CTX1a** — STATE.md inflado a 400 linhas:
  `ctx_state_curto ... FALHOU test_ctx_pines.cpp:106 STATE.md com 400
  linhas — o teto é 300`. Repor → verde.
- **M-CTX1b** — o gate contra um commit REAL de código sem STATE.md
  (o 3.5b, d0c765d): `docs-lint(state): VERMELHO — sub-fase declarada
  sem atualização do STATE.md · assunto: 3.5b: instancing de blocos
  com mesma assinatura`. O mesmo gate isenta docs (cf6279a → verde) e
  aferra o fix caaef02 → vermelho. O pin ctx_state_gate_fecha corre
  estas 4 provas no CI a cada push.
- **M-CTX2a** — citar um ficheiro que não existe:
  `ctx_codemap_ficheiros_existem ... FALHOU CODEMAP cita caminho que
  não existe no git: app/src/main/cpp/core/nao_existe_mutacao.cpp`.
  Repor → verde.
- **M-CTX2b** — paths misturados (voni/ em vez de app/src/main/cpp/voni/):
  `ctx_codemap_paths_uniformes ... FALHOU path fora da convenção
  (relativo à raiz): voni/VoniVm.cpp`. Repor → verde.
- **M-CTX3a** — README com 500 linhas:
  `ctx_readme_curto ... FALHOU README.md com 500 linhas — o teto é
  200`. Repor → verde.
- **M-CTX3b** — uma referência a um relatório que não existe em lado
  nenhum (o ficheiro RELATORIO-NAO-EXISTE-MUTACAO.md com o prefixo
  docs/, acrescentada ao BACKLOG da cópia):
  `ctx_refs_ok ... FALHOU BACKLOG.md cita docs/RELATORIO-NAO-EXISTE-
  MUTACAO.md e o ficheiro não está nem em docs/ nem em docs/arquivo/
  (ponteiro morto)`. Repor → verde.

O pin ctx_gate_state do prompt (§3.5.5) é o ctx_state_gate_fecha, que
já fechou na CTX-1 e corre no CI (as provas F1–F4 contra d0c765d,
caaef02, cf6279a e a gama vazia).

## §3 A MEDIÇÃO da redução de contexto (§3.4 do prompt — números reais)

O ANTES medido no estado anterior à campanha (commit caaef02): o
README integral + o REGRESSOES + os 78 relatórios de docs/ — o pior
caso de arranque que o §0.1 do prompt descreve. O DEPOIS: os dois
índices do Nível 0 (o prompt em si não conta — vive fora do repo).
Estimativa de tokens = bytes/4, como o template manda.

| Métrica | Antes | Depois |
|---|---|---|
| Ficheiros de arranque | 80 (README + REGRESSOES + 78 relatórios) | 2 (STATE.md + CODEMAP.md) |
| Bytes | 1 608 978 | 18 941 |
| Tokens (bytes/4) | ~402 244 | ~4 735 |
| Linhas | 6 339 (4 541 + 795 + ~1 003 de média dos relatórios × 78) | 348 (143 + 205) |
| **Redução (bytes)** | — | **98,8 %** |

Notas honestas:
- O template do prompt estimava «14 ficheiros» no ANTES; a contagem
  real dos ficheiros que o regime antigo obrigava a ler é 80 (o §0.1
  falava em «20+ relatórios»; são 78). A meta de 80 % do prompt é
  HIPÓTESE — a medição deu 98,8 % no pior caso; o número real fica
  registrado, sem inventar.
- O WORKLOG.md que o prompt cita NÃO EXISTE no repo (nunca existiu —
  ver §4). O ANTES medido não o inclui; se existisse, a redução seria
  maior.
- Os ficheiros de Nível 1/2 abrem-se só quando a tarefa nomeia — o
  custo de arranque é o do Nível 0.

## §4 O que o template citava e não existia (as correções — nada inventado)

1. **WORKLOG.md** — o prompt (PARTE 6 e §3.1) assume que existe na
   raiz. NÃO existe no repo e nunca existiu (não há renomeação —
   `git log --all --follow -- WORKLOG.md` não tem history). Consequência:
   o passo «WORKLOG.md → mantém-se, com nota no topo» é um no-op; a
   leitura obrigatória dele não acontece. Registrado aqui como a
   divergência que era.
2. **cpp/core/TickGroups.h** (receita «Adicionar um SISTEMA») — não
   existe; o TickGroup + o TickGroups::add vivem em
   **app/src/main/cpp/core/Tick.h**. O CODEMAP usa o caminho real.
3. **docs/BACKLOG.md** (STATE.md §8 da CTX-1) — o ficheiro vive na
   RAIZ (BACKLOG.md). O §8 do STATE.md corrigido no commit da CTX-2.
4. **voni/VoniTykers.h — kComponents** — o registo dos componentes
   (min/maxArgs) vive NO REGISTO central (**VoniRegistry.h**), não no
   Tykers; o Tykers é o motor que executa. O CODEMAP descreve o real.
5. **tests/test_voni_*.cpp** — o pin da receita dos comandos vive em
   **tests/test_voni.cpp** (o mapa usa o real).
6. Todos os restantes caminhos citados no CODEMAP foram aferidos pelo
   pin ctx_codemap_ficheiros_existem (git ls-files) — verde.
7. **RELATORIO-0.9.6.md NÃO é um relatório de campanha fechada** — é a
   âncora VIVA do gate release-identity (R-015): o versionName 0.9.6
   do build.gradle exige docs/RELATORIO-0.9.6.md presente em docs/ e
   declarando o par com o versionCode corrente (o §14 acumula o log).
   A CTX-3 moveu-o por engano; o run do fecho ficou VERMELHO nesse
   gate («docs/RELATORIO-0.9.6.md não existe») e o fix devolveu-o a
   docs/ (71 movidos, não 72). O gate fez EXATAMENTE o seu trabalho:
   a identidade da release não se desfaz.
8. **O scanner do ctx_refs_ok vs a transcrição da MUTAÇÃO C** — com o
   RELATORIO-0.9.6.md de volta a docs/ (documento vivo, varrido), a
   linha do gate «::error::release-identity (R-015): o RELATORIO-
   0.9.9.md (caminho docs/) não existe» — a prova da mutação, §11 do
   ficheiro — casou no regex das citações e o run do e82155e ficou
   VERMELHO no ctx_refs_ok. O ficheiro citado nunca existiu — esse é
   o PONTO da mutação (bump sem relatório → o gate grita). O fix é no
   pin, não na evidência: as linhas de anotação GitHub (::) são
   transcrição de output, não citação (uma citação viva não começa
   por "::"); a MUTAÇÃO C fica intacta como prova. A/B na sandbox: o
   pin velho reproduz o vermelho do CI ao byte; o novo fica verde,
   prende a citação real injetada no STATE.md e repõe o verde. (Esta
   prosa nomeia o ficheiro citado SEM o literal docs/ contíguo — o
   scanner casaria a descrição como citação; a evidência byte a byte
   está no §11 do RELATORIO-0.9.6.md.)

## §5 As referências validadas antes de mover (§3.1 do prompt)

A varredura (git grep "RELATORIO-", "WORKLOG", "README" fora do
arquivo) antes do `git mv`:

- **BACKLOG.md** — 7 referências a relatórios movidos
  (RELATORIO-0.10-M-PASSO6-VALIDACAO-FINAL, -HOTFIX-SELECAO-UNICA,
  -HOTFIX-P0-SUBTICS, -HOTFIX-TEXTURAS-MULTIMAT,
  -HOTFIX-TEXTURAS-VINCULA-2, -HOTFIX-TEXTURAS-VINCULA e
  RELATORIO-0.11-S-ESQUELETO) — ATUALIZADAS para docs/arquivo/ no
  mesmo commit (o BACKLOG é documento vivo; os apontadores seguem o
  ficheiro).
- **README.md** — as referências do diário viajaram INTEIRAS para o
  docs/DIARIO.md (o conteúdo não se reescreve); resolvem-se pela
  convenção do arquivo, que o pin ctx_refs_ok aferra (docs/X →
  docs/arquivo/X).
- **STATE.md / CODEMAP.md** — citam só os relatórios da 0.11-W
  (ficam em docs/) e o RELATORIO-0.11-CTX.md (este ficheiro).
- **ARCHITECTURE.md / ESTUDOS.md / VONI_referencia.md / llms*.txt** —
  sem referências a RELATORIO movidos.
- **docs/REGRESSOES.md** — sem referências a RELATORIO movidos (cita
  comandos e fases, não caminhos de relatório).
- O pin ctx_refs_ok varre a raiz de leitura + docs/ inteiro (exceto o
  arquivo) e aferra: cada docs/RELATORIO-*.md citado existe em docs/
  OU em docs/arquivo/ — verde.

## §6 Os 3 pins qualitativos (por medir na campanha SEGUINTE)

Fecham no relatório de fecho da campanha seguinte à 0.11-CTX (só aí
se sabe se a campanha funcionou):

1. **Tempo até à primeira ação útil** — quanto demora o GLM a
   identificar a tarefa e os ficheiros (registo no worklog da campanha
   seguinte).
2. **Erros de contexto** — quantas vezes o GLM tenta reabrir decisão
   fechada, mexe fora do âmbito ou confunde estado (contagem no
   relatório da campanha seguinte).
3. **Cobertura do CODEMAP** — quantas vezes foi preciso git grep
   porque o CODEMAP não apontou o ficheiro (contagem).

## §7 A divergência do arranque desta sessão (o estado real vs o esperado)

O prompt assumia começar do zero (o fecho da 0.11-W em caaef02). O
estado real ao abrir: a CTX-1 já estava implementada (deba7bb + o fix
ac2b780) por uma sessão anterior, que interrompeu ANTES de commitar o
CODEMAP.md — os pins ctx_codemap_* ficaram órfãos no HEAD e o CI de
ac2b780 ficou vermelho neles (o previsto: ctx_codemap_existe sem o
ficheiro). O tratamento, pela regra do STATE.md (o índice, não a
verdade): o §1 do STATE.md confirmou a posição real; a CTX-2 fechou
com o CODEMAP.md + o scope; o CI de 1a348f5 valida tudo (os pins
órfãos recebem o seu sujeito). A CTX-3 partiu de 1a348f5. Nenhuma
decisão fechada reaberta; nada inventado.

## §8 Ficheiros tocados (os commits da campanha)

- deba7bb — docs: criar STATE.md (estado canónico) — CTX-1.
- ac2b780 — fix: o bit de execução do gate STATE.md no git — CTX-1.
- 1a348f5 — docs: criar CODEMAP.md (o mapa do código) — CTX-2.
- 909b1ea — docs: arquivo + gate STATE.md + template + medição —
  CTX-3: docs/arquivo/ (72 git mv — o 0.9.6 entre eles, por engano,
  ver a nota §4.7), docs/DIARIO.md (novo), docs/TEMPLATE_PROMPT.md
  (novo), README.md (cortado), BACKLOG.md (7 paths),
  tests/test_ctx_pines.cpp (5 pins novos), .github/workflows/
  release.yml (o passo do gate), STATE.md (a atualização do mesmo
  commit).
- 85c4f94 — 0.11-CTX: fecho (o contexto enxuto) — o STATE.md sela a
  campanha (o §3 aponta o BLOCO B, o fecho da 0.11-W, só com
  CONTINUAR do dono).
- e82155e — fix: o RELATORIO-0.9.6.md de volta a docs/ — o gate
  release-identity (R-015) apanhou a âncora movida (os runs de
  909b1ea e 85c4f94 VERMELHOS nele); docs/RELATORIO-0.9.6.md (git mv
  de docs/arquivo/) + docs/RELATORIO-0.11-CTX.md (a nota §4.7, o
  reconto 71/72, este §8 e o §9).
- este commit — fix: o pin ctx_refs_ok distingue CITAÇÃO de
  TRANSCRIÇÃO — com a âncora viva de novo, a linha ::error:: da
  MUTAÇÃO C do RELATORIO-0.9.6.md (o RELATORIO-0.9.9.md — caminho
  docs/ — que nunca existiu; esse é o ponto da mutação) caiu no
  scanner como citação morta (o run do e82155e VERMELHO nele); as
  linhas de anotação GitHub (::) saem da varredura — evidência não
  se falsifica, o scanner aprende; A/B na sandbox: o pin velho
  VERMELHO (idêntico ao CI), o novo VERDE, a citação real injetada
  presa, o verde reposto; ficheiros: tests/test_ctx_pines.cpp +
  STATE.md (a regra §1.2: fix com código leva o STATE no mesmo
  commit) + este §4/§8/§9.

## §9 NÃO VERIFICADO

- O CI deste commit (a corrida em curso no momento da escrita) — o
  fecho da campanha declara-se só com o VERDE em TODOS os jobs. Os
  runs de 909b1ea/85c4f94 caíram no gate release-identity (a âncora
  movida, ver a nota §4.7) e o run do e82155e caiu no ctx_refs_ok (a
  transcrição da MUTAÇÃO C tratada como citação, ver a nota §4.8) —
  cada gate apanhou o seu; este commit fecha os dois.
- Os 3 pins qualitativos do §6 — por construção, medem-se na campanha
  seguinte.
- O efeito real da redução numa sessão com o prompt novo — o §6 mede.
