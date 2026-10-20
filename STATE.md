# STATE.md — estado do projeto

Atualizado: 2026-10-20 · vc: 105 · Último commit: docs — a ABERTURA da
0.12-Y (o Salto Visual e de Assets: o Pan da câmara, o ícone IA, o
pipeline PBR + WorldEnvironment); o §0.2 correu VERDE contra o
origin/main sincronizado (d9b62a1, CI success)

> Este ficheiro é um ÍNDICE, não é a verdade — a verdade é o código.
> Em caso de divergência: investigar, reportar, corrigir o índice. Nunca inventar.
> Atualiza-se no MESMO commit que fecha a sub-fase (regra §6 abaixo).

## §1 Onde estamos

- **Campanha 0.12-Y (O SALTO VISUAL E DE ASSETS) — EM CURSO (vc 105→)**
  (a cláusula calma: SÓ o prompt — Pan/Ícone IA/PBR; a V.ONI intocada;
  a história vive no RELATORIO-0.12-Y-ABERTURA.md). O §0.2 VERDE com
  evidência (vc 105 · CI success do d9b62a1 · a QoL FECHADA aqui em
  baixo) — e o INCIDENTE do ambiente documentado (o clone herdado
  estava desatualizado; o fetch/reset sincronizou, zero trabalho
  perdido). **3.Y-A** o Pan: Camera::pan (o target no plano da câmara,
  worldPerPixel da fonte única) + 2 dedos = pinch E pan + a 6.ª
  ferramenta no rail (Hand, id 33); R-059 + M-Ya1/2. **3.Y-B** o ícone
  IA (obsidiana+fogo) → os 5 mipmaps + o fundo #0A0A0C. **3.Y-C** o
  pipeline PBR: assets/<stem>/textures/ + o bind auto dos 5 slots + o
  lit PBR-lite (normal/rough/metal) + o TIC WorldEnvironment (HDRI
  equirect + céu/névoa); R-060 + M-Yc1/2.
- **Campanha 0.12-QoL (UI POLISH + NAVEGADOR + AUTO-SAVE) — FECHADA
  (A..D, vc 102–105)** (a cláusula calma: refinamento de UI 2.5D e
  persistência, ZERO lógica de motor; a história vive no
  RELATORIO-0.12-QoL.md).
  **A (vc 102)**: o top bar SÓ com o ícone do G (o texto morre), a pesquisa recolhida na LUPA que expande ao toque (o × limpa e recolhe), o border dos INATIVOS #24242A (o âmbar APENAS no ativo/selecionado — o divisor e os cards modais incluídos), os vãos X/Y/Z 12dp + a margem mínima 8dp do Reset, o X (48×48) no menu de cenas; pins ×5 + M-QoLa1/x2 + 8 recalibrações com nota; CI verde (1befd71).
  **B (vc 103)**: os atalhos e o Subir deixam de ser BOTÕES âmbar (chips quietos, geometria/ids intactos), a seta (>) em TODAS as linhas, o « / » no breadcrumb, TUDO o que é ponto FORA (a 0.8.5 SUPERADA); R-057 + pins ×2 + M-QoLb1; o picker_oculta_dot recalibrado.
  **C (vc 104)**: a TEIA do frustum SÓ com a câmara selecionada (o glifo mudo fica), o rail SEM o pai quadrado gigante, o grid com o fade suave pela fonte única gridFadeForDist; R-058 + pins ×3 + M-QoLc1; o d17 recalibrado.
  **D (vc 105)**: o TIMER 180s com o dirty HONESTO (o dump difere), o autosave.goni NA RAIZ (o principal INTACTO), a recuperação 1× com o pop-up [Recuperar]/[Descartar], o ✓ verde que PISCA; R-056 + pins ×3 + M-QoLd1. **A CAMPANHA FECHA.**
- **Campanha 0.12-X (CÂMARA DE TERCEIRA PESSOA) — FECHADA (3.9x, vc
  101)** (o camrig + camdrag + limit no REGISTO como Contínuos; o
  yaw/pitch no TykerState; o Host::lookDelta com o consumo a ZERAR; o
  conflito camrig↔follow/look no compilador com o par do Substituir;
  pins ×14 + R-055 + M-3.9x1/x2; a história vive no
  RELATORIO-0.12-X-ABERTURA.md e no RELATORIO-0.12-X-CAMRIG.md).
- **Campanha 0.12-U (V.ONI BLOCO A — a REDUÇÃO DE TRABALHO) — FECHADA
  (3.9a..3.9c + o FECHO, vc 98–100)** (as 7 features no registo — o
  View P universal, as 14 Funções, o spawn pela porta do Host, os
  relógios every/after/when com o estado por nó AST; 64 entradas; pins
  ×37 + R-052/R-053/R-054 + M-3.9a1..c3; o CRITÉRIO DO PLATFORMER corre
  VERDE — o juiz final; a história vive no RELATORIO-0.12-U-ABERTURA.md
  e nos relatórios das sub-fases + o FECHO).
- **Campanha 0.12-E (EMERGÊNCIA + REDESIGN 2.5D, FOGO AVERMELHADO) —
  FECHADA (E-1..E-3, vc 95–97)** (a ordem do dono abortou o SDF 3D;
  cláusulas: ZERO matemática 3D, ZERO mexidas no FontAtlas, TICs
  intocados; E-1 o desenho simples 2D de volta + 13 pins reformados +
  6 recalibrações; E-2 a paleta EXATA do dono no Theme.h + os tokens
  fireRed/fireOrange + os static_asserts da fonte única; E-3 o vidro
  α0.85 + a linha âmbar 1dp a 20%, a sombra falsa, o gradiente
  fireRed→fireOrange nos ativos, o ui/Noise2D.h; pins ×4 + M-E3a/b;
  os DETALHES vivem no RELATORIO-0.12-E.md).
- **Campanha 0.12-D (identidade visual) — PARCIALMENTE ABORTADA PELO
  DONO (a 0.12-E)**: o D-4 (o LOGO, vc 94) SOBREVIVE; o D-1 (SDF), o
  D-2 (vidro) e o D-3 (fogo) estão DORMENTES (zero chamadores — vc
  91-93); D-5..D-8 CANCELADOS. A história vive no
  RELATORIO-0.12-D.md §1..4.
- **Campanha 0.12-C (layout, organização e consistência) — FECHADA
  (C-1..C-4, vc 87–90)** (os DETALHES no RELATORIO-0.12-C.md): C-1 as
  proporções (a hierarquia 20%, o viewport ≥60%), C-2 os menus e a
  barra de topo, C-3 a tipografia/idioma/superfícies, C-4 o seletor +
  os vazios com AÇÃO + a REPARAÇÃO da árvore (614 de volta).
- **Campanha 0.12-B (arquitetura da UI) — FECHADA (B-1..B-4, vc 83–86)**
  (os DETALHES vivem no RELATORIO-0.12-B.md): B-1 a árvore de UI (pins
  ×9, R-050), B-2 o Inspector de UI (pins ×5), B-3 o canvas (pins ×3),
  B-4 as ações de cena 2-tap (pins ×4, R-051).
- **Campanha 0.12-A (bugs P0) — FECHADA (vc 82)** (os DETALHES vivem no
  RELATORIO-0.12-A.md + no INVEST-RAM): A-1 as rows LOD JÁ EXISTIAM,
  A-2 o refreshCatalog no arranque, A-3 o scissor3dRect + o tooltip,
  A-4 o veredito RAM (a cache de blocos a ciclizar).
- **Campanha 0.11-Y (identidade visual) — FECHADA** (o fecho é o commit
  32a6785: os 13 PNGs + o pin y_fecho; a FASE 31 fica no replay).
- **0.11-CTX — FECHADA** com CI verde (os detalhes no §2 e no relatório).
- **0.11-W (Render Systems) com CI verde** — falta o PIN do dono no C33 real; o fecho (BLOCO B) só por ordem do dono.

## §2 O que está FEITO

- 0.12-QoL A (vc 102) — a LIMPEZA VISUAL (no RELATORIO-0.12-QoL.md §1):
  o top bar só com o ÍCONE do G (o slot do nome morto, o padrão D21);
  a pesquisa da hierarquia RECOLHIDA na lupa 32×32 que EXPANDE ao toque
  (o × 5503 limpa e recolhe; hierSearchOpen no EditorState); o border
  dos INATIVOS #24242A (o token border recolorado + o alias LINE + o
  divisorGradientV + as molduras ACCENT dos cards modais — o âmbar só
  no ativo/selecionado); os vãos X/Y/Z 8→12dp + a margem mínima 4→8dp
  (o piso do valor 26→22 e o passo (c) letra→padding→letra na escada
  R1) + a folga vertical 64→68dp; o X de fechar (48×48) no menu de
  cenas; pins ×5 (test_012qol.cpp) + M-QoLa1/x2 vermelho→reposto→verde
  (coladas no §1 do relatório); 8 recalibrações com nota.
- 0.12-QoL B (vc 103) — o NAVEGADOR compacto (no RELATORIO-0.12-QoL.md
  §2): os chips quietos no lugar dos botões gigantes (o choke point
  button fora das raízes/Subir), a seta (>) nas linhas todas, o « / »
  no breadcrumb, o FileApi sem pontos (pastas E ficheiros); R-057 +
  pins ×2 + M-QoLb1 vermelho→reposto→verde (colada no §2); o
  picker_oculta_dot recalibrado com nota. O CI apanhou a ordem trocada
  do R-057 (2 runs vermelhos) — corrigida no B-fix2 (fe90703).
- 0.12-QoL C (vc 104) — o VIEWPORT limpo (no RELATORIO-0.12-QoL.md
  §3): a teia do frustum sob seleção, o rail flutuante, o grid suave;
  R-058 + pins ×3 + M-QoLc1 vermelho→reposto→verde (colada no §3); o
  d17 recalibrado com nota.
- 0.12-QoL D (vc 105) — o AUTO-SAVE (no RELATORIO-0.12-QoL.md §4): o
  timer 180s + o autosave.goni na raiz + a recuperação com o pop-up +
  o ✓ que pisca; core/AutoSave.h NOVO (as decisões puras), o
  Project::saveAutosave/loadAutosave, o ProjectStorage::modifiedTime;
  R-056 + pins ×3 + M-QoLd1 vermelho→reposto→verde (colada no §4).
- 0.12-X 3.9x (vc 101) — a câmara de terceira pessoa (no
  RELATORIO-0.12-X-CAMRIG.md): camrig(d,h,suav) + camdrag(sens) +
  limit como Contínuos no REGISTO; o TykerState guarda camYaw/camPitch;
  o Host::lookDelta (o consumo ZERA — o main alimenta SÓ em Play com os
  dedos não reclamados); o compilador rejeita camrig+follow/look; pins
  ×14 (10 VM + 4 de motor real/.goni) + R-055 + M-3.9x1/x2.
- 0.12-U FECHO (vc 100) — o CRITÉRIO DO PLATFORMER corre VERDE (o pin
  platformer_bloco_a_juiz_final: 7 features em cadeia, os contadores do
  juiz batem) + README/ARCHITECTURE §5/BACKLOG atualizados (no
  RELATORIO-0.12-U-FECHO.md) — a CAMPANHA 0.12-U FECHA.
- 0.12-U 3.9c (vc 100) — os relógios every/after/when (no
  RELATORIO-0.12-U-3.9c.md): o estado por nó AST; pins ×12 + R-054 +
  M-3.9c1/2/3.
- 0.12-U 3.9b (vc 99) — o spawn (no RELATORIO-0.12-U-3.9b.md): o Host
  ganha a porta de criação; pins ×9 + R-053 + M-3.9b1/b2.
- 0.12-U 3.9a (vc 98) — o View P universal + as 14 funções do registo +
  a distance (no RELATORIO-0.12-U-3.9a.md): pins ×15 + R-052 +
  M-3.9a1/a2/a3; a campanha 0.12-U abre com o critério do platformer
  como juiz final.
- 0.12-E E-3 (vc 97) — os efeitos 2.5D + o FECHO da 0.12-E (no
  RELATORIO-0.12-E.md §3): sombra falsa, vidro 0.85+âmbar, gradiente
  fogo, ruído em scroll; pins ×4 + M-E3a/b; a FASE 35.
- 0.12-E E-2 (vc 96) — o tema FOGO AVERMELHADO (§2 do relatório): a
  paleta exata do dono, os 2 tokens novos, os gates a aferrar a fonte
  única.
- 0.12-E E-1 (vc 95) — a restauração da UI básica (§1): o SDF 3D
  abortado pelo dono, o desenho simples 2D de volta, os 13 pins
  reformados + as FASE 32/33 do c33, as 6 recalibrações de volta.
- 0.12-D D-4 (vc 94) — o logo «G em chama» (no RELATORIO-0.12-D.md §4):
  as setas mortas, a rampa por traço, o ignite, as 3 versões, o espelho
  Java, o overdraw da barra; pins ×4 + M-D4a/b; a FASE 34.
- 0.12-D D-1..D-3 (vc 91–93) — o shader SDF de widget, o vidro real e
  falso e o fogo (no RELATORIO-0.12-D.md §1..3): a fonte única ui/Sdf.h
  + o 3.º programa, o Kawase dual + a cache, o ui/Fire.h + a rampa do
  dono; pins ×13 + M-D1a/b..D3a/b; DORMENTES desde a 0.12-E (os
  achados: o rebind do ARRAY_BUFFER, a corrida dos jobs, o flip da
  captura, o continue do blend, a banda sem teto).
- 0.6.x–0.9.5 — fundação da engine (V.ONI core, device real, CI 5 jobs).- 0.12-C (vc 87–90) — layout, organização e consistência (no
  RELATORIO-0.12-C.md §1..4): C-1 as proporções (hierarquia 20%,
  viewport ≥60%, hierHeaderRects); C-2 os menus e a barra de topo
  (menuSheetLayout, o play A CHAMA, o gizmo duplicado fora); C-3 a
  escada de superfícies + o idioma (os 4 tons, card/elevated); C-4 o
  picker + os vazios com AÇÃO + A REPARAÇÃO da árvore (614 de volta);
  pins ×20 + M-C1a/b..C4a/b.
- 0.12-B B-3/B-4 (vc 85–86) — o canvas com a moldura do device (o
  seletor, o véu, as guias, a grelha, o toast; pins ×3 + M-B3a/b) e as
  ações de cena (o renomear/duplicar/apagar 2-tap + o manifesto
  round-trip; pins ×4 + M-B4a; R-051) — no RELATORIO-0.12-B.md
  §20..29; a CAMPANHA 0.12-B FECHA.
- 0.12-B B-1/B-2 (vc 83–84) — a árvore de UI (Control/UIRoot, a compat
  migrateUiCanvasToTree, o serializer, o editor em modo árvore, o Play;
  pins ×9 + M-B1a/b/c + R-050) e o Inspector de UI (as secções
  colapsáveis, os campos da base/por tipo, a zona morta, os resets, o
  «sem remover joystick»; pins ×5 + M-B2a/b) — no
  RELATORIO-0.12-B.md §1..19; 8 recalibrações com nota.
- 0.12-A (vc 82) — os 4 bugs P0 fechados (o ficheiro:linha, os pins e
  o veredito vivem no relatório próprio: docs/RELATORIO-0.12-A.md + o
  INVEST-RAM): A-1 as rows LOD JÁ EXISTIAM (PÁRA no fix cego; pins ×5);
  A-2 o refreshCatalog no arranque + sceneRowIcon (pins ×3); A-3 o
  scissor3dRect + o tooltip do gizmo (pins ×3); A-4 a instrumentação
  GONI_BENCH_RAM com a CURVA a reproduzir o salto (veredito: RAM — a
  cache de blocos a ciclizar); pins com M-A1a/b..A3a/b vermelho→verde;
  o achado fora da lista (inspectorRowCount −4 rows mesh, sem sintoma)
  documentado no relatório §6.
- 0.11-Y (vc 77–81) — a identidade visual completa (o ícone, o tema, o layout, os cartões, o fecho com os 13 PNGs).
- 0.9.6 + 0.9.6.4–0.9.6.20 (vc 48–67) — identidade, grupos UI, editor, import glTF, orçamento 3D, leis de desenho/toque, janelas.
- 0.10-M..0.11-W (vc 68–76) — o .gmesh v3, a hierarquia, os materiais glTF, a animação skinada, o skinned cull+LOD, o instancing e o streaming progressivo (os DETALHES vivem nos relatórios da docs/ — o índice não é o arquivo).

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
- `docs/RELATORIO-0.12-QoL.md` — o relatório da 0.12-QoL (o A no §1 —
  a limpeza visual; o B no §2 — o navegador compacto; as mutações
  coladas, as recalibrações com nota; os C/D entram nos §3/§4).
- `docs/RELATORIO-0.12-X-CAMRIG.md` — o relatório da 0.12-X (a câmara
  de terceira pessoa: as convenções fechadas, o hook lookDelta, o
  TIMING do input, as mutações coladas no §5).
- `docs/RELATORIO-0.12-X-ABERTURA.md` — a abertura da 0.12-X (as 3
  verificações §0.2 + as 5 perguntas com ficheiro:linha).
- `docs/RELATORIO-0.12-E.md` — o relatório da 0.12-E (a EMERGÊNCIA: o
  E-1 no §1 — o aborto do SDF 3D, os pins reformados, as recalibrações;
  as FASE 2/3 do redesign 2.5D fecham nos §2/§3).
- `docs/RELATORIO-0.12-D.md` — o relatório da 0.12-D (a D-1 no §1 — o
  shader SDF, o toggle, os pins, as mutações, os achados do rebind e da
  corrida dos jobs; D-1..D-3 DORMENTES desde a 0.12-E — a história).
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
