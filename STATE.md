# STATE.md — estado do projeto

Atualizado: 2026-10-18 · vc: 90 · Último commit: este (o 0.12-C C-4, O
SELETOR DE FICHEIROS + OS ESTADOS VAZIOS — o picker pela fonte única
browserSheetLayout, o breadcrumb que salta, a pesquisa propósito 10, as
pastas ponto fora, os 4 vazios com AÇÃO, os glifos únicos, os 8 pins +
M-C4a/b; a REPARAÇÃO da árvore — 614 ficheiros de volta do e8c9587;
antes: 266bd3b o C-3, A TIPOGRAFIA O IDIOMA AS SUPERFÍCIES; antes:
3e8984a o C-2, OS MENUS E A BARRA DE TOPO)

> Este ficheiro é um ÍNDICE, não é a verdade — a verdade é o código.
> Em caso de divergência: investigar, reportar, corrigir o índice. Nunca inventar.
> Atualiza-se no MESMO commit que fecha a sub-fase (regra §6 abaixo).

## §1 Onde estamos

- **Campanha 0.12-C (layout, organização e consistência) — FECHADA
  (C-1..C-4)** (o CI do push é o árbitro; os DETALHES vivem no
  RELATORIO-0.12-C.md). A cláusula da campanha é anti-superficialidade —
  nada de controlos «de fachada»; se um botão não faz nada, sai.
  **C-4 (o seletor de ficheiros + os estados vazios, vc 90)**: o PICKER
  pela FONTE ÚNICA browserSheetLayout (as linhas 48dp ícone+nome+seta; o
  BREADCRUMB que SALTA — browserCrumbs; a PESQUISA propósito 10 com o
  filtro puro browserFilterEntries; as PASTAS PONTO fora no
  listDirEntries; o Subir FIXO; a safe-area no card) + os 4 VAZIOS COM
  MENSAGEM + AÇÃO (o Inspector abre o plusMenu com o «+ criar TIC»; os
  Assets ganham o IMPORTAR que abre o navegador na Download; a
  Hierarquia aponta o + e ele fica VISÍVEL — hierEmptyInvite; os
  Projetos no espelho Java: o G FRACO + o CTA «Novo projeto») + os
  GLIFOS ÚNICOS por função (a lupa SÓ pesquisar; o Image NOVO para as
  texturas; emptyStateIcon — o vazio dos Assets deixou de ser a PASTA) +
  pins ×8 (test_012c_picker.cpp + o gate projects_ui_check) + M-C4a/b
  vermelho→verde. Os ACHADOS REAIS: o gesto de rolar importava (o
  re-despacho scrollTap SÓ), o claim do scroll comia o IMPORTAR (lista
  vazia = sem região) e o botão não cabia no drawer 240 (ancorado ao
  fundo). **A REPARAÇÃO**: os commits da C-1..C-3 entraram de uma árvore
  INCOMPLETA e empurraram 614 deleções (a engine inteira) — a árvore
  completa voltou do e8c9587 com as 6 recalibrações que faltavam
  (test_toolbar, test_sentinels ×3, test_wiring086, c33 ×9 + a flag
  fitByDesign no título da hierarquia). test_core 0 falhas · c33
  1005/0 · vc 90.
  **C-3 (a tipografia o idioma as superfícies, vc 89)**: a ESCADA DE
  SUPERFÍCIES do dono (fundo #0A0A0C · painel #131316 · cartão #1B1B20
  · elevado #24242A — o poço da Y-1 superado pela ordem expressa; o
  espelho Java alinhado — estava na 0.9.6.10!), «Áudio»/«Exportar para
  Downloads»/«N TICs», os 4 pins novos (test_012c_idioma.cpp — o scan
  da escala com o strip do gate) + M-C3a/M-C3b vermelho→verde + 3
  recalibrações (y1, o toast B-3, o c33). Os DETALHES no
  RELATORIO-0.12-C.md §3.
  **C-2 (os menus e a barra de topo, vc 88)**: o menuSheetLayout FONTE
  ÚNICA do sheet (altura máxima + scroll + âncora), o play A CHAMA em
  playMode, o GIZMO do canto REMOVIDO (o duplicado do Mover — a cláusula
  anti-superficialidade), os 4 pins novos (test_012c_chrome.cpp) +
  M-C2a/M-C2b vermelho→verde + 5 recalibrações com nota. Os DETALHES no
  RELATORIO-0.12-C.md §2.
  **C-1 (as proporções e os painéis, vc 87)**: a hierarquia 20% (era
  18% — a ordem do dono), o viewport ≥60% por defeito (o trilho de 32dp
  é o estado de fábrica; o piso ≥55% da R-033 mantém-se COM seleção nos
  ecrãs estreitos), o cabeçalho da hierarquia em linha própria
  (hierHeaderRects — o título elipsa, nunca sobrepõe as ações) e os 4
  pins novos (test_012c_paineis.cpp) + M-C1a/M-C1b vermelho→verde. SEM
  sentinela nova. Os DETALHES no RELATORIO-0.12-C.md §1.
- **Campanha 0.12-B (arquitetura da UI) — FECHADA (B-1..B-4)** (o CI do
  push é o árbitro; os DETALHES vivem no RELATORIO-0.12-B.md).
  **B-4 (o seletor de cenas com ações, vc 86)**: as 3 ações POR LINHA no
  menu CENAS — o RENOMEAR (o teclado in-app no PROPÓSITO 8, o nome
  pré-carregado; o caminho da fonte pura sceneRenamePath, o MESMO filtro
  do addScene), o DUPLICAR (o nome único Godot-style «main.001» — contra
  o manifesto E os órfãos do disco; o conteúdo byte a byte; a cópia não
  fica ativa) e o APAGAR 2-TAP (o 1.º toque SÓ arma «apagar?», o 2.º no
  MESMO ícone confirma; o arm morre com o menu; a ATIVA recusa; o
  activeScene re-assenta) — as fontes puras partilhadas pelo draw e
  pelos pins; pins ×4 (test_012b_cenas.cpp) + M-B4a vermelho→verde;
  **R-051 NOVA**; SEM C33 (a ordem expressa do prompt). test_core 0
  falhas · c33 1006/0 · vc 86. **B-3 (o canvas, vc 85)**: o seletor de
  proporção ANDROID + a moldura com o véu escuro + as guias + a grelha +
  o toast OPACO (o contraste WCAG ≥4.5); pins ×3 + M-B3a/b.
  **B-2 (o Inspector de UI, vc 84)**: as 3 secções colapsáveis + os
  campos da base/por tipo + a ZONA MORTA nova do TouchControls + os
  resets por campo + SEM «remover joystick»; pins ×5 + M-B2a/b.
  **B-1 (a árvore de UI, vc 83)**: Control + UIRoot (os storages
  ids 11/12), um TIC filho por elemento, o UiCanvas DEPRECATED (a
  conversão migrateUiCanvasToTree), o serializer grava a árvore, o
  editor tem o modo árvore e o Play desenha/hit-testa; pins ×9 +
  M-B1a/b/c; **R-050 NOVA**; o UIRoot FANTASMA curado (§10 do
  relatório).
- **Campanha 0.12-A (bugs P0) — FECHADA** (o CI do push é o árbitro):
  A-1 (o diagnóstico provou que as rows LOD/impostor JÁ EXISTEM na UI
  desde o vc 72/76 — PÁRA no fix cego por ordem do prompt; os 5 pins da
  UI novos + M-A1a/b vermelho→verde fecham o gap de cobertura), A-2 (o
  refreshCatalog no arranque + na criação de cena; sceneRowIcon = camadas),
  A-3 (scissor3dRect no modo UI + o tooltip legendTimer 1,5 s/largar),
  A-4 (sampler bench_ram sob ifdef + o bench da curva + o veredito: a
  cache de blocos a ciclizar — RAM, não GPU). test_core 1077/0 ·
  c33_virtual 1006/0 · vc 82. PARAR — a próxima só arranca com CONTINUAR
  do dono.
- **Campanha 0.11-Y (identidade visual) — FECHADA** (o fecho é o commit
  32a6785: os 13 PNGs por ecrã + o pin y_fecho). Y-0..Y-3 + o fix — 20
  pins novos; a FASE 31 fica no replay para SEMPRE.
- **0.11-Y (o tema o layout os cartões o fecho) e 0.11-CTX — FECHADAS**
  com CI verde (os detalhes vivem no §2 e nos relatórios próprios; o
  run do 3a6d934 ficou VERMELHO no probe 18.2 e o CI apanhou — a cura
  viajou na Y-2; o RELATORIO-0.9.6.md voltou a docs/ e o ctx_refs_ok
  distingue citação de transcrição; 98,8 % de redução no pior caso).
- **0.11-W (Render Systems) com CI verde** — falta o PIN do dono no C33
  real (instalar o APK vigente, importar 203 MB, R-017, PNG do HUD);
  o fecho (BLOCO B) só por ordem do dono.

## §2 O que está FEITO

- 0.6.x–0.9.5 — fundação da engine (V.ONI core, device real, CI 5 jobs).
- 0.12-C C-4 (vc 90) — o seletor + os vazios (no RELATORIO-0.12-C.md
  §4): o picker pela fonte única (o breadcrumb que salta, a pesquisa
  propósito 10, as pastas ponto fora, o Subir fixo, as linhas
  ícone+nome+seta); os 4 vazios com AÇÃO; os glifos únicos (o Image
  novo); pins ×8 + M-C4a/b; OS ACHADOS (o rolar importava, o claim
  comia o IMPORTAR, o botão fora do drawer 240); A REPARAÇÃO da árvore
  (614 ficheiros de volta + as 6 recalibrações); a CAMPANHA 0.12-C
  FECHA.
- 0.12-C C-3 (vc 89) — a escada de superfícies + o idioma (no
  RELATORIO-0.12-C.md §3): os 4 tons + os tokens card/elevated; «Áudio»
  /«Exportar para Downloads»/«N TICs»; pins ×4 + M-C3a/b.
- 0.12-C C-2 (vc 88) — os menus e a barra de topo (no
  RELATORIO-0.12-C.md §2): menuSheetLayout; o play A CHAMA; o GIZMO
  fora (o duplicado do Mover); pins ×4 + M-C2a/b.
- 0.12-C C-1 (vc 87) — as proporções e os painéis (no
  RELATORIO-0.12-C.md §1): hierarquia 20%; viewport ≥60% de fábrica;
  hierHeaderRects; pins ×4 + M-C1a/b.
- 0.12-B B-4 (vc 86) — as ações de cena (no RELATORIO-0.12-B.md
  §25..29): o renomear/duplicar/apagar 2-tap + o manifesto round-trip;
  pins ×4 + M-B4a; R-051; a CAMPANHA 0.12-B FECHA.
- 0.12-B B-3 (vc 85) — o canvas com a moldura do device (no
  RELATORIO-0.12-B.md §20..24): o seletor + a moldura + o véu + as guias
  + a grelha + o toast opaco; pins ×3 + M-B3a/b.
- 0.12-B B-2 (vc 84) — o Inspector de UI (no RELATORIO-0.12-B.md §14..19):
  as secções colapsáveis + os campos da base/por tipo + a zona morta
  (TouchControls::dead) + os resets + o «sem remover joystick»; pins ×5 +
  M-B2a/b.
- 0.12-B B-1 (vc 83) — a árvore de UI (o relatório próprio:
  docs/RELATORIO-0.12-B.md): Control/UIRoot (os storages ids 11/12); a
  compat migrateUiCanvasToTree (o UiCanvas sai; DE MEMÓRIA); o
  serializer (as fontes partilhadas appendUiElementJson/
  fillUiElementJson; o UIRoot FANTASMA curado no §10); o editor (o modo
  árvore com espelho + sync-back; o Inspector de UI; uniqueNewTicName);
  o Play (drawUiTree/hitTestUiTree/uiElementFromHit); **Pins ×9 +
  M-B1a/b/c** + **R-050**; 8 recalibrações com nota.
- 0.12-A (vc 82) — os 4 bugs P0 fechados (o relatório próprio:
  docs/RELATORIO-0.12-A.md; a investigação:
  docs/RELATORIO-0.12-A-INVEST-RAM.md):
  - **A-1** — as rows `lod distance`/`impostor` JÁ EXISTEM (ficheiro:
    linha no relatório) — PÁRA no fix cego; os 5 pins NOVOS da UI
    (test_012a_lodui.cpp) fecham o gap de cobertura; M-A1a/M-A1b
    vermelho→verde; ZERO produção trocada.
  - **A-2** — o `refreshCatalog()` entra no ARRANQUE e no
    `createSceneNamed` (os contadores a 0 morrem); `sceneRowIcon()` NOVA
    (a fonte única do ícone de cena); pins ×3; M-A2a/M-A2b vermelho→verde.
  - **A-3** — `safe::scissor3dRect()` NOVA (o modo UI reserva o painel
    direito no SCISSOR; byte a byte fora do modo UI) + o tooltip do
    gizmo morre (legendTimer/legendArm; nasce no largar, vive 1,5 s);
    pins ×3; M-A3a/M-A3b vermelho→verde.
  - **A-4** — a instrumentação sob `#ifdef GONI_BENCH_RAM` (o sampler
    `bench_ram:` 1×/s no engine.log) + o alvo opcional `bench_ram`
    (option OFF) — A CURVA REPRODUZ O SALTO: linear 50k…4,1M e explosão
    a 8,2M/12,3M no cruzamento EXATO do teto de VRAM (192 MB) com
    9 758/23 470 evictions/frame; `evc_par == evc_orb`; o RSS é LINEAR —
    VEREDITO: RAM (a cache de blocos a ciclizar), NÃO GPU/CPU; o
    protocolo do dono + as recomendações no relatório próprio.
  - **O achado fora da lista (cláusula calma, NÃO fixado)**: o
    `inspectorRowCount` subestima a MALHA em 4 rows para mesh file
    (EditorLayout.h:433; sem sintoma — documentado no relatório §6).
- 0.11-Y (vc 77–81) — a identidade visual completa (o ícone, o tema, o
  layout, os cartões, o fecho com os 13 PNGs) — ver §1 e docs/
  RELATORIO-0.11-Y-FECHO.md.
- 0.9.6 + 0.9.6.4–0.9.6.20 (vc 48–67) — identidade, grupos UI, editor,
  import glTF, orçamento 3D, leis de desenho/toque, janelas.
- 0.10-M (vc 68) — .gmesh v3 por blocos (streaming/blocos/texturas/thumbs).
- 0.11-H FASE 1 hierarquia (vc 69) — drag-reparent, apagar/duplicar
  sub-árvore, colapso, ícone por tipo.
- 0.11-M FASE 2 materiais glTF (vc 70) — parse, GTB2, props no lit,
  blend por blocos, samplers no upload.
- 0.11-S FASE 3 esqueleto/animação (vc 71) — slerp/cubic/step, UBO 128
  bones, V.ONI stop/blend, o fix dos clips skinados.
- 0.11-T FASE 3.5a GPU efficiency (vc 72) — skinned cull + LOD;
  0.11-T-FIX Perna 1 (vc 73) — o contorno da corrupção do corte paralelo
  (Perna 2 ADIADA pelo dono — ver §7).
- 0.11-W ABERTURA (docs) — cf6279a.
- 0.11-W 3.5b instancing (vc 74, d0c765d) — blocos de mesma assinatura
  6 TICs → 1 dc; teto 64/draw; R-047.
- 0.11-W 3.75a streaming progressivo (vc 75, 0c11b14) — o freeze do
  import morre por orçamento; pico de RAM = 1 bloco (R-041).
- 0.11-W 3.75b impostores (vc 76, 1524148 + caaef02) — PNG alfa por
  distância; dc do TIC longe = 1; R-049; fix caaef02 (o ImpostorIndex.cpp
  faltava no CMake do app).

## §3 O que está PENDENTE (por ordem)

1. **Fecho 0.11-W (BLOCO B, só com CONTINUAR do dono)** — verificar R-048,
   RELATORIO-0.11-W-FECHO.md, atualizar README/BACKLOG/STATE.md; o pin
   C33 (§1) entra no §3 do fecho.
2. **3 métricas qualitativas da CTX** fecham no relatório da campanha
   SEGUINTE (tempo até à 1.ª ação útil, erros de contexto, cobertura do
   CODEMAP).

## §4 Decisões fechadas (NÃO REABRIR)

- O culling estava CERTO: far 500 e ~31% dos blocos fora do frustum; bl
  5164/5178 explicado pela margem AABB ±4. Não é bug (§1.8 da 0.11-W).
- Instancing só para blocos de MESMA assinatura (mesh+material), teto
  64 instâncias por draw, toggle disponível, skin/LOD mismatch respeitado.
- Impostor: limiar exclusivo (0=OFF), billboard com a normal PARA a
  câmara, PNG RGBA com MASK discard, TIC skinado NUNCA tem impostor,
  pico de RAM = UM bloco (contrato R-041), impostor falhado não existe.
- Streaming: orçamento por frame no import; open do .gmesh = tabela
  (192 B + índices), dados só por bloco a materializar.
- Compat: campos novos são opcionais com default; .goni/.gmesh antigos
  reproduzem byte a byte o comportamento anterior.
- Stub de testes: rasterizador honesto, SEM GLES real; pins rodam com
  injeção de dependências.
- V.ONI: fonte única no registo (VoniRegistry); docs gerados pelo
  voni_refgen (bijeção aferrada por sentinela).
- UI: unidade é TIC (nunca Node/GameObject/Entity); cores só em
  ui/Theme.h; vocabulário e acentos vigiados por gates.
- Documentação sem placeholders nem achismo (docs-lint R-016).
- STATE.md e CODEMAP.md são ÍNDICES; o código é a verdade.
- O APK instalado no device é SEMPRE o artefacto assinado do CI.
- 0.12-C: as PASTAS ponto ficam fora do picker; os FICHEIROS ponto
  ficam (a decisão 0.8.5 mantém-se); a lupa é SÓ «pesquisar»; o vazio
  usa o glifo da PRÓPRIA função (emptyStateIcon); os vazios têm SEMPRE
  uma ação real (nunca um convite que manda procurar o botão).

## §5 Regras de convivência

- Um commit só conta como fechado com o CI VERDE em todos os jobs.
- Toda a regressão estrutural ganha sentinela R-NNN com a mutação COLADA
  no relatório (quebrar → vermelho → repor → verde).
- Compat byte a byte nos formatos .goni/.gmesh; defaults novos explícitos.
- Cláusula calma de campanha: só o que o prompt da campanha manda.
- PARAR entre campanhas — a próxima só arranca com CONTINUAR do dono.
- Se o estado real divergir do esperado: PÁRA e reporta.
- Commit que fecha sub-fase com código TEM de incluir STATE.md (o gate
  docs-lint valida; ver §6).
- A árvore vai INTEIRA no commit (a lição da reparação: um git add -A
  numa árvore parcial EMPURRA as deleções — conferir o diffstat antes
  do push; 614 deleções = um repo partido).

## §6 Regra de atualização

O STATE.md é atualizado no MESMO commit que fecha a sub-fase: se o diff
toca código (cpp/, app/, tests/) e a mensagem declara sub-fase/feat/fix,
o STATE.md viaja no mesmo commit — o gate docs-lint fica vermelho com
"sub-fase declarada sem atualização do STATE.md" caso contrário.
Commits puramente documentais não são afetados.

## §7 Bloqueios conhecidos

- **Perna 2 do 0.11-T-FIX** — adiada pelo dono (o dono sabe; retoma só
  por ordem dele).
- **Pin C33 da 0.11-W** — depende do dono (instalar APK vc 76, importar
  203 MB, R-017, PNG do HUD). Nada na fila depende disto para arrancar.
- **R-048 (streaming progressivo)** — a verificar no fecho da 0.11-W;
  se não existir entrada no REGRESSOES.md, é dívida declarada, não
  inventada.
- Corrida latente do corte paralelo sob TSAN (§4.1 do 0.11-T) — passo de
  diagnóstico continue-on-error no CI; a caça continua suspensa.

## §8 Onde ir buscar detalhe

- `CODEMAP.md` — onde vive o quê + receitas de extensão (o ler código).
- `ARCHITECTURE.md` — o mapa dos módulos (o porquê das camadas).
- `docs/REGRESSOES.md` — sentinelas R-NNN (consulta, não leitura inteira).
- `BACKLOG.md` — rastreador de campanhas (na RAIZ do repo).
- `docs/RELATORIO-0.12-C.md` — o relatório da 0.12-C (a C-1 no §1, a C-2
  no §2, a C-3 no §3, a C-4 no §4 — o picker, os vazios, os achados do
  gesto/claim/drawer, a REPARAÇÃO da árvore, as mutações coladas).
- `docs/RELATORIO-0.12-B.md` — o relatório da 0.12-B (a B-1 no §1..13,
  a B-2 no §14+, a B-3 no §20+, a B-4 no §25+ — as ações de cena, o
  R-051 e o fecho da campanha).
- `docs/RELATORIO-0.12-A.md` + `docs/RELATORIO-0.12-A-INVEST-RAM.md` —
  a 0.12-A (os 4 bugs P0 com ficheiro:linha, os pins ×11, as 6 mutações
  coladas; a investigação RAM vs GPU com o veredito CACHE THRASH).
- `tests/bench_ram_curve.cpp` + o alvo `bench_ram` (tests/CMakeLists,
  option GONI_BENCH_RAM OFF) — a curva 50k…12,3M do A-4 no hospedeiro.
- `scripts/y0_gen_icons.py` — o gerador dos PNGs do launcher a partir
  da fonte do dono (o threshold de luminância documentado lá dentro).
- `docs/arquivo/` — os relatórios das campanhas FECHADAS (71, git mv;
  o RELATORIO-0.9.6.md NÃO está lá — é a âncora do gate release-identity
  R-015; a 0.11-T-FIX também não — Perna 2 adiada, a campanha não fechou).
- `docs/TEMPLATE_PROMPT.md` — a pirâmide de leitura dos prompts.
- `VONI_referencia.md` — a referência V.ONI gerada (fonte: VoniRegistry).
- `ci/scope.txt` — os caminhos que uma campanha pode tocar.

## §9 As 4 perguntas antes de qualquer trabalho

1. §1 confirma que posso avançar? (campanha/sub-fase em curso = a minha?)
2. §4 cobre o que vou tocar? (nenhuma decisão fechada reabrindo?)
3. §7 diz que estou desbloqueado? (nenhum bloqueio no meu caminho?)
4. O prompt alinha com §3? (a tarefa é mesmo a pendente da ordem?)

Se qualquer resposta for não: PÁRA e reporta ao dono.
