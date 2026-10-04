# RELATÓRIO 0.8.4 — Estabilização no C33 (crashes e freezes)

Primeira sub-fase da campanha F8. ZERO features novas — a missão é domar
os crashes e freezes que o C33 mostra com a suíte host 100% verde. O
diagnóstico provou o que a campanha suspeitava: **o gap é
WIRING/INTEGRAÇÃO, não lógica unitária** — e o fix mais grave era uma
corrupção de memória que nenhum teste cobria.

## 1. Objetivo

(1) "Adicionar mesh/UI/TIC → ecrã preto" domado com causa raiz provada em
host. (2) Freezes e "funções que param" explicados e cobertos. (3) Um
teste de integração de wiring POR FIX — o bug reportado falha o teste
antes do fix e passa depois (regra nova desta campanha). (4) Checklist
device concreta para o dono (README).

## 2. Estado inicial (HEAD de entrada)

`2afa3b0` (0.8.3-b, suíte 508 OK; CI da 0.8.3 verde, APK assinado
versionCode 34, sha256 `967bcce3…`). Sintomas reportados no C33: ecrã
preto ao adicionar mesh/UI/TIC; freezes e funções que param; (funcional
e UX ficam para 0.8.5/0.8.6).

## 3. Diagnóstico (as três causas raiz)

**1 — CORRUPÇÃO DE MEMÓRIA NO RESOLVER DE UI (o "ecrã preto").**
`resolveCanvasLayout` (ui/UiRuntime.cpp) iterava `n =
c.elements.size()` (o canvas INTEIRO) mas escrevia `out[i]` SEM
respeitar o `cap` do chamador. Os DOIS chamadores do editor passam
`CanvasLayout lay[32]` NA STACK (ui/UiEditor.cpp:125 drawUiViewport e
:1039 uiDetachElement). O desenho (linha 180: `i < 32`) e o hit-test
(linha 253: `i < nLay`) limitavam bem — só o RESOLVER não. Consequência:
ao criar o 33.º elemento de UI, cada frame do editor UI mode escrevia
`lay[32..n-1]` POR CIMA da stack (8×28 bytes = 224 px de memória alheia
com 40 elementos). Corrupção de stack = sintomas clássicos e
aleatórios: crash imediato, congelamento, ou estado GL/variáveis
corrompidas → **ecrã preto**. Reproduzido em host: sem o fix, o binário
de testes dá **SEGV** (exit 139) no teste sentinela.

**2 — CAPS DE SUBMISSÃO SILENCIOSOS (o "texto/funções que param").**
O UiContext registra RUNS de submissão (z-order real sólidos↔texturas) e
o Renderer aceita submissões por frame — ambos com caps FIXOS de 32 e
descarte SILENCIOSO. Como os GLIFOS são sempre a ÚLTIMA submissão (a
regra do tema "texto por cima"), com 32 runs o submit dos glifos caía
fora do cap → **todo o texto saía do ecrã** (browser com muitas
thumbnails = 2 runs/linha; timeline + painéis + overlay chegam lá). O
resto dos runs cortados = widgets que "não existem" = funções que param
sem uma única linha de log.

**3 — HEADER DA TIMELINE SOBREPOSTO (botões a lutar pelo toque).**
Os botões do header ficavam a offsets FIXOS da direita (play w−436, stop
w−348, mode w−280, vel w−164, + w−60) e o "clip:" a x+376..552. Com
strip < 988 px, **"clip:" e play SOBREPUNHAM** (no C33: 1600 − 2×300
painéis = 1000 úteis, menos insets do sistema → ~952). Immediate-mode:
dois widgets a receber o MESMO toque → play abre o seletor de clips,
nada responde como esperado.

Hipóteses da campanha confirmadas/descartadas: "realloc de
buffers/atlas" — os storages já eram deque (estáveis desde a F3) e o
atlas não re-bakeia a meio (ASCII fixo); o teste de steady-state GL
(novo) prova ZERO objetos GL novos por frame. "Hit-test O(n²)" — os
hit-tests são O(tics×elementos) por toque, trivial a estas escalas; o
frame-time do resolver ficou medido e limitado por teste.

## 4. Implementação (por ficheiro)

- **`ui/UiRuntime.cpp`** — o resolver clava TUDO ao cap: `Resolver`
  ganha `count = min(n, cap)`; `sizeOf`/`placeAt` recusam índices ≥
  count (nunca escrevem `out[i]` fora do buffer); os loops de filhos dos
  containers iteram só `count` (antes liam `sizeW[j]/sizeH[j]` além dos
  vetores); os três passos (tamanhos, posicionamento, órfãos) iteram
  `count`. Chamadores runtime (vectors do tamanho da cena) ficam
  byte-a-byte iguais.
- **`render/Renderer.h/.cpp`** — `subs_` fixo[32] → `std::vector` com
  `reserve(kSubWarn=66)`: NADA se descarta mais; acima de 66 submissões
  num frame LOGA 1× por frame (o dono vê no log viewer que o ecrã está
  "excessivo" — antes: desaparecimentos sem explicação). `endFrame`
  itera o vetor e faz `clear()` (capacidade preservada — zero realloc em
  steady state).
- **`ui/UiContext.h/.cpp`** — `runs_` fixo[32] → `std::vector` com
  `reserve(kMaxRuns=64)`; `recordRun` faz merge ou push_back (sem
  descarte); `init()` reserva; beginFrame/endFrame usam o vetor. Os
  accessors de teste passam a ler do vetor.
- **`ui/Timeline.h/.cpp`** — `timeline::headerLayout(r)` NOVA função
  PURA (afervel no CI): devolve os rects de título/clip/play/stop/mode/
  slider/+ para uma largura qualquer. Regra: cluster direito FIXO (os
  offsets de sempre — nada muda nos ecrãs largos); "clip:" encosta ao
  play com folga 8 e encolhe 176→120→96 se precisar (o título cede; o
  botão nunca invade o play); título = labelFitted no resto. O
  drawTimeline consome o layout (desenho e hit-test partilham os MESMOS
  rects — o padrão da casa).
- **`tests/test_wiring084.cpp`** (NOVO, 11 testes) — ver §6.
- **`tests/CMakeLists.txt`** — o novo TU na suíte.
- **`app/build.gradle`** — bump 0.8.4 / versionCode 35.
- **`.github/workflows/release.yml`** — artifact `goni-vv-0.8.4-release…`.
- **`README.md`** — escopo 0.8.4 + checklist C33 (7 passos).

## 5. Decisões técnicas relevantes

1. **O buffer do chamador é autoridade** (não o canvas): o resolver
   trabalha dentro do `cap` e ignora o resto. Os runtimes passam
   vectors do tamanho da cena (cap = n) — comportamento intacto; os
   editores passam 32 — a partir de agora seguros MESMO que o canvas
   cresça. Alternativa descartada (arrays dinâmicos nos editores):
   espalharia alocações por frame nos hot paths.
2. **Nunca mais descarte silencioso**: os caps viraram LIMIARES DE
   AVISO com armazenamento dinâmico (reserve + clear = zero realloc em
   steady state, medido pelo teste). Um ecrã anómalo LOGA e desenha TUDO
   — o dono passa a VER o sintoma no engine.log em vez de caçar fantasmas.
3. **kSubWarn = 66 = kMaxRuns(64) + glifos + folga**: mesmo com os runs
   a esgotar o limiar, o submit dos glifos CABE — o texto nunca mais
   morre por aritmética de caps (o bug exato do C33).
4. **Layout fluido como função pura**: o header da timeline segue o
   padrão `toolbar::layout` (solver puro testado) — e o teste trava a
   regressão a TODAS as larguras plausíveis (1600→680), não só à do C33.
5. **A prova antes-do-depois**: o stash do fix + rerun do binário deu
   SEGV; com o fix, 519/519 verdes. A regra da campanha ("o teste teria
   apanhado o bug") está PROVADA para o fix mais grave, não só afirmada.

## 6. Testes novos (CI Linux)

`tests/test_wiring084.cpp` (+11 — 508→519):

1. **resolver_cap_nao_escalava_o_buffer_do_chamador** — 40 elementos num
   `lay[32]` com SENTINELA de 224 floats imediatamente a seguir: guard
   intacto (sem o fix: SEGV) + os primeiros 32 dispostos com rect
   válido;
2. **resolver_cap_com_container_e_filhos_fora_do_cap** — VBox com 33
   filhos: guard intacto (os loops de filhos liam além dos vetores) e
   filhos dentro do cap ficam ligados ao pai;
3. **caps_runs_e_glifos_sobrevivem_ao_frame_cheio** — 64 alternâncias
   sólido/textura = 128 runs: TODOS submetidos (o cap 32 descartava
   metade);
4. **caps_glifos_sobrevivem_com_os_runs_no_limite** — 64 runs + batch de
   glifos = 65 submissões aceites (o caso EXATO em que o texto sumia);
5. **storm_add_remove_50_mesh_50_ui_10_tics_sem_perder_invariantes** — o
   fluxo do critério de aceitação: 50 Mesh + 10 câmaras + 50 elementos
   UI (10 frames do wiring EXATO do editor com buffer[32]), remover
   20, re-adicionar 20, handles sobreviventes íntegros;
6. **play_stop_repetido_20x_pose_integra_e_sem_leak_de_ui** — 20 ciclos
   capture→advance×30→apply→restore: a pose de editor volta SEMPRE (pos
   exato), a UI de editor volta, players param no zero;
7. **save_load_com_cena_grande_roundtrip_do_storm** — dump/load de 51
   TICs com anim + 50 elementos: contagens e tracks voltam;
8. **steady_state_zero_realloc_gl_em_loop_de_frames** — 30 frames após
   warm-up: ZERO glGen*/glDelete* (o realloc em loop — gatilho de
   freeze — é agora uma falha de teste garantida) + 30 drawElements;
9. **frame_time_do_resolver_50_elementos_limitado** — 600 frames do
   resolver (canvas de 50) < 50 ms (margin ×100 para o CI);
10. **header_timeline_sem_sobreposicao_do_c33_a_720px** — 7 larguras
    (1600/1000/988/952/800/720/680): nenhum par sobreposto, tudo dentro
    da strip, título ≥ 120, clip clicável, cluster direito EXATAMENTE
    onde estava a 1600;
11. **header_timeline_no_c33_os_dois_botoes_que_lutavam_separam_se** — a
    952 px: clip termina ANTES do play com folga ≥ 8 (o par que
    sobrepunha 36 px antes do fix).

## 7. Checklist device (resumo — versão completa no README)

Storm de UI 35+ · storm 3D 10 mesh + 2 câmaras + presets · add/remove ×3
voltas · play/stop ×20 · texto no browser carregado · header da timeline
clicável · regressões 0.8.x. VERIFIED no C33 = dono confirma cada item.

## 8. Riscos

- **Comportamento do resolver com canvases > 32 no EDITOR**: os
  elementos 33+ não são desenhados nem selecionáveis no mini-ecrã (já
  era assim — o draw limitava a 32); o que muda é que a CORRUPÇÃO
  desapareceu. O cap de 32 no editor é dívida documentada (ver §9).
- **Vetores dinâmicos nos hot paths**: reserve em init + clear por frame
  mantém zero realloc em steady state (testado); um frame anómalo
  cresce o vetor (realoc) e LOGA — aceitável, é o frame que já estava
  errado.
- **Renderizadores com > 66 submissões**: o LOGE 1×/frame pode encher o
  log se um ecrã anómalo persistir — intencional (é o sinal pedido pela
  campanha).

## 9. Dívida (documentada, fora do escopo CALMA)

- Cap de 32 elementos no EDITOR de UI (desenho/hit-test do mini-ecrã):
  para além disso o elemento existe, anima e joga, mas não é editável no
  viewport 2D. Levantar o cap = migrar os dois chamadores para heap (com
  o resolver já seguro, é agora trivial) — candidato a 0.8.6.
- `textfit::ellipsize` é O(L²) no pior caso (L ≤ 256, bounded) — medido
  como não-problemático; se um dia crescer, memoizar o widthOf.
- Uploads GL lazy a meio do pass (textura nova a 1ª vez que desenha):
  one-time por asset (cache), mas o stall é sincrono. Pipelining =
  fora do escopo (complexidade nova).

## 10. CLÁUSULA CALMA (cumprida)

Só fixes e wiring listados. Zero features novas, zero física nova, zero
scripting. Nenhuma mudança de formato `.goni` (o round-trip continua
exato — testado). Nenhuma mudança de gameplay/UX além do layout fluido
do header (mesmos widgets, mesmos IDs, mesmas funções).
