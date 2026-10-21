# STATE.md — estado do projeto

Atualizado: 2026-10-21 · vc: 114 · Último commit: este (a FASE UI D — a
coluna das 4 ferramentas no viewport (o ativo É a brasa), o Íman e a MÃO
desceram a chips pequenos sob o rail (os mesmos ids 33/36), os divisores
24dp + os pisos + a persistência layout.json confirmados; 
pan_rail_sexta recalibrado + ui_divisores_persistencia + M-UId)

> Este ficheiro é um ÍNDICE, não é a verdade — a verdade é o código.
> Em caso de divergência: investigar, reportar, corrigir o índice. Nunca inventar.
> Atualiza-se no MESMO commit que fecha a sub-fase (regra §6 abaixo).

## §1 Onde estamos

- **Campanha FASE UI (O REDESIGN VISUAL, envios 1/5 + 2/5) — EM CURSO
  (A..D fechadas, vc 114; a E a seguir)** (a cláusula calma: SÓ visual,
  layout e UX — core/V.ONI/importadores/física/.gmesh intocados; um
  commit por parte, CI verde entre elas; a história vive no
  RELATORIO-FASE-UI.md).
  **A (vc 111)**: O FICHEIRO ÚNICO DE TOKENS com a paleta do dono (os
  hexes EXATOS: fundo #0B0B0D · painéis #121215 · elementos #1B1B20 ·
  linhas #26262C · texto #E8E6E3/#8A8A93 · A BRASA ÚNICA #FF5A1F (o
  âmbar morreu) · seleção #2B1810 · eixos #E5484D/#3FB950/#3B82F6) + a
  escala 4/8/12/16 + raios 6-8 + o espelho Java alinhado + o contraste
  validado antes do commit; pins ui_tokens_brasa + ui_sobreposicao_topbar
  + M-UIa; 9 recalibrações com nota.
  **B (vc 112)**: A BARRA 40DP — a direita nova (undo/redo/guardar
  subiram do viewport para TODOS os modos; os ids 3/19/30/31/32
  aposentados, os 20/21/22 novos), o Pause na PLAY BAR (o mundo congelado
  de verdade), o «Android» nas Definições, a fila do viewport morta para
  o chip ⋯; O ACHADO DO CI (o A vermelho na sonda 35.2 — a lacuna dos
  DOIS alvos): a cura + o build_c33.py NOVO + 11 sondas stale
  recalibradas; ui_topbar_40dp_composicao + M-UIb; test_core 0 falhas +
  c33 1009/0 local.
  **C (vc 113)**: A HIERARQUIA — as linhas 40dp (kRowH na fonte única), o
  OLHO e o ⋮ SÓ na selecionada (draw gated + hit gated — a MESMA
  condição; nas outras a zona é corpo), o nome de largura inteira e a
  indentação pela escala (16); ui_hierarquia_40dp + M-UIc; 6
  recalibrações; test_core 0 falhas + c33 1009/0 local.
  **D (vc 114)**: O VIEWPORT — a coluna das 4 FERRAMENTAS (o ativo É a
  brasa), o Íman e a MÃO desceram a CHIPS sob o rail (os MESMOS ids
  33/36; o par INTEIRO tem de caber — o achado da M-UId), os divisores
  (24dp + os pisos) e a persistência layout.json confirmados; 
  pan_rail_sexta recalibrado + ui_divisores_persistencia + M-UId;
  test_core 0 falhas + c33 1010/0 local.
- **Campanha 0.12-Y (O SALTO VISUAL E DE ASSETS) — EM CURSO (A, B, C, o
  C-FIX e o D fechadas, vc 110; o FECHO a seguir)** (a cláusula calma: SÓ o
  prompt; a V.ONI intocada; a história vive no RELATORIO-0.12-Y-ABERTURA.md
  + o RELATORIO-0.12-Y.md).
  **D (vc 110)**: o CÉU PROCEDURAL (no RELATORIO-0.12-Y-SKY.md) — o shader do céu (render/SkyRenderer.{h,cpp} NOVOS, o padrão Pbr.h: o quad fullscreen + o gradiente horizonte→zénite + o sol disco/halo + as nuvens FBM 2D com hash inteiro e O TEMPO no UV; a base da câmara extraída da vista, válida na orbit E no Play), o Environment ganha skyType/cores do céu/sol/direção/cobertura/vento (o clampFields normaliza o sol), o .goni transporta com defaults omitidos, o Inspector ganha o dropdown "céu: cor sólida|procedural" + os 5 campos SÓ no procedural (ids 5425..5430, os hex 13/14/15, os sliders nuvens/vento), o main desenha o quad ANTES dos modelos com skyType==1 (o 0 = o clear de sempre byte a byte) e o relógio g_skyTime avança 1× por frame; pins ×2 (env_sky_type · inspector_sky) + R-061 regress_procedural_sky + M-Yd1/2 vermelho→reposto→verde (o achado: o falso verde POR COMENTÁRIO nasceu o test::stripCppComments); test_core 1208 OK (os flakes da sandbox excluídos — o CI é o árbitro).
  **C-FIX (vc 109)**: o TESTE DO DONO (Inspector do WorldEnvironment
  VAZIO + modelo BRANCO) — a secção Environment NO Inspector (céu hex ·
  hdri pelo NAVEGADOR · ambiente · névoa · névoa hex, tudo no .goni), o
  albedo = uBaseColorTex (unit 0), o LOG «[material] textura extraida» +
  o skip jpeg falado + o thumb do baseColor; pins ×2 + M-Yf1/2.
  **C (vc 108)**: o PIPELINE PBR — os 5 slots na PASTA DO MODELO
  assets/<stem>/textures/ (dedup FNV-1a, uri externa passa), o BIND AUTO
  pelo meshTexSlotFor, o lit PBR-LITE pela FONTE ÚNICA render/Pbr.h (o
  normal por DERIVADAS, o rough/metal, a névoa exp2 0=MORTA, o
  uAmbientScale), o TIC WorldEnvironment NOVO (o 7.º preset); pins ×4 +
  R-060 + M-Yc1/2; 3 recalibrações com nota.
  **B (vc 107)**: o ÍCONE IA — a FONTE com o prompt EXATO do dono
  (obsidiana + magma), a extração por TEXTURA high-pass, o tile a 65% da
  ZONA SEGURA, os 15 PNGs (5 densidades × 3) + o fundo #0A0A0C (A ORDEM
  DO DONO); icon_ia_mipmaps + 2 recalibrações com nota + M-Yb1.
  **A (vc 106)**: o PAN da câmara — a 6.ª FERRAMENTA no rail (o glifo
  Hand, id 33, a legenda «Pan»), o Camera::pan pela FONTE ÚNICA pura
  (camWorldPerPixel/camRightVec/camUpVec; o target no plano da câmara,
  agarra a cena), o modo Pan (1 dedo arrasta SEM orbitar) e o pan de 2
  DEDOS LIVRE (o midpoint em qualquer modo, o pinch intacto, o 1.º frame
  só ancora); pins ×5 (test_012y.cpp) + R-059 + M-Ya1/2
  vermelho→reposto→verde; os achados fora da lista (o kCheck do glifo ✓
  com um traço morto) documentados. O §0.2 VERDE com evidência (vc 105 ·
  CI success do d9b62a1) e o INCIDENTE do ambiente (o clone herdado
  desatualizado; o fetch/reset sincronizou — zero trabalho perdido).
- **Campanha 0.12-QoL (UI POLISH + NAVEGADOR + AUTO-SAVE) — FECHADA
  (A..D, vc 102–105)** (a cláusula calma: refinamento de UI 2.5D e
  persistência, ZERO lógica de motor; a história vive no
  RELATORIO-0.12-QoL.md: A a limpeza visual, B o navegador compacto,
  C o viewport limpo, D o auto-save — pins ×13 + M-QoLa1/x2..QoLd1 +
  R-056/R-057/R-058).
- **Campanha 0.12-X (CÂMARA DE TERCEIRA PESSOA) — FECHADA (3.9x, vc
  101)** (o camrig + camdrag + limit no REGISTO como Contínuos; o
  yaw/pitch no TykerState; o Host::lookDelta com o consumo a ZERAR; o
  conflito camrig↔follow/look no compilador com o par do Substituir;
  pins ×14 + R-055 + M-3.9x1/x2; a história vive no
  RELATORIO-0.12-X-ABERTURA.md e no RELATORIO-0.12-X-CAMRIG.md).
- **Campanha 0.12-U (V.ONI BLOCO A) — FECHADA (3.9a..3.9c + o FECHO, vc 98–100)** (as 7 features no registo, 64 entradas; pins ×37 + R-052..R-054; o platformer do juiz VERDE; nos RELATORIO-0.12-U-*.md).
- **Campanha 0.12-E (EMERGÊNCIA + REDESIGN 2.5D) — FECHADA (E-1..E-3, vc 95–97)** (o SDF 3D abortado; E-1 o desenho simples, E-2 a paleta no Theme.h — superada pela FASE UI A —, E-3 os efeitos 2.5D; no RELATORIO-0.12-E.md).
- **Campanha 0.12-D (identidade visual) — PARCIALMENTE ABORTADA (a 0.12-E)**: o D-4 (o LOGO) sobrevive; o D-1/D-2/D-3 DORMENTES; D-5..D-8 CANCELADOS (no RELATORIO-0.12-D.md).
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

- 0.12-QoL A..D (vc 102–105) — a limpeza visual, o navegador compacto,
  o viewport limpo e o auto-save (no RELATORIO-0.12-QoL.md §1..§4 — o
  índice não é o arquivo).
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
- 0.12-B (vc 83–86) — a árvore de UI, o Inspector de UI, o canvas com a
  moldura e as ações de cena 2-tap (pins ×21 + R-050/R-051; no
  RELATORIO-0.12-B.md).
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
