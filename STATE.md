# STATE.md — estado do projeto

Atualizado: 2026-10-17 · vc: 81 · Último commit: este (o Y-3, OS
CARTÕES — o cristal: a linha do topo nos painéis/drawer/cards + os
divisores com o âmbar a cair; antes: 6b1427f o Y-2 (o run VERDE fecha
a Y-1 também), 3a6d934 o Y-1, 25c1348 o Y-0-FIX)

> Este ficheiro é um ÍNDICE, não é a verdade — a verdade é o código.
> Em caso de divergência: investigar, reportar, corrigir o índice. Nunca inventar.
> Atualiza-se no MESMO commit que fecha a sub-fase (regra §6 abaixo).

## §1 Onde estamos

- **Campanha atual: 0.11-Y (identidade visual) · Sub-fase: Y-3 (os
  CARTÕES — o cristal) — FECHADA** (este commit; o CI é o árbitro).
  Segue-se o FECHO da campanha (os PNGs por ecrã no docs/y-*.png + o
  relatório de fecho) — por autorização expressa do dono («faz todas
  as fases e só para no fim da campanha Y»).
- **Y-1 (o tema) — FECHADA** (o commit 3a6d934): o poço #08080A (o
  clear lê o token), o vidro -5 % com o mesmo alfa, a hairline ÂMBAR
  #4A3714 α0.60, os tokens NOVOS accent2 #FF6B00 e glow #FF3D00 α0.25;
  o botão ATIVO é a chama (glow de foco 2 quads α decrescente +
  gradiente vertical accent→accent2 por vértice, zero mudanças de
  shader); a glassTopLine (1 dp, glow a 20 %) pronta para os cartões
  da Y-3; pins y1_* ×5 + M-Y1a/b/c/d vermelho→verde. NOTA HONESTA: o
  run do 3a6d934 ficou VERMELHO no C33 virtual — o probe 18.2 (o card
  de logs) comparava o pixel com o LITERAL do bg velho e escapou à
  calibração da Y-1; o CI apanhou, a cura viaja no commit da Y-2 (o
  probe lê o token) e o run VERDE da Y-2 fecha a Y-1 de vez.
- **Y-2 (o layout) — FECHADA com CI verde** (este commit): as regras
  CruzaTabBar/PainelFora no validador + a flag auditável overlay (o
  toast) + a FASE 31 (a varredura de 13 ecrãs pelo frame de produção);
  OS TRÊS BUGS REAIS apanhados ao vivo: o toast na densidade 2.0
  mergulhava na tab bar (R-018: medidas em px crus — tudo em dp()), o
  back do Settings sangrava 6px acima do content (a lei de ouro: 36×36
  na linha) e o último tick da régua da timeline sangrava a borda
  (rótulos deslizam para dentro); o Sobreposto não conta filhos
  clipped (11 falsos ERROS mortos); pins y2_* ×3 + M-Y2a/b
  vermelho→verde.
- 0.11-Y Y-3 Cartões ✅ vc 81 — o cristal: a glassTopLine (1dp do glow
  a 20 %) na hierarquia/inspector/drawer/card CRIAR/3 sheets + o
  dividerGradientV NOVO (1dp, accentDim α0.90→α0.15 a cair) nos
  divisores (a linha sólida D11 SUPERADA pela ordem da Y-3, a pill do
  drag fica); a FASE 31 confirma o layout intocado (1006/0); pins
  y3_* ×3 + M-Y3a/b vermelho→reposto→verde.
- **Y-0 (o ícone no launcher) — FECHADA com CI verde** (a2d2248) e o
  pin C33 do dono apanhou o bug da escala (o G a ~25 %; o contrato
  ~65 %) — o Y-0-FIX é este commit (a bbox de escala passa a ser o
  CONTEÚDO SÓLIDO, 65 % da zona segura; o pin y0_escala guarda).
- **0.11-CTX — FECHADA com CI verde** (deba7bb + ac2b780, 1a348f5,
  909b1ea, 85c4f94; os fixes da âncora R-015: e82155e + b577dea — o
  RELATORIO-0.9.6.md de volta a docs/ e o ctx_refs_ok a distinguir
  citação de transcrição). 98,8 % de redução no pior caso
  (docs/RELATORIO-0.11-CTX.md).
- **0.11-W (Render Systems) implementada e com CI verde** — falta o PIN do
  dono no C33 real: instalar o APK vigente (vc 78 hoje), importar a cena
  de 203 MB, correr o bench R-017, colar o PNG do HUD. Baseline: FPS 5 ·
  bl 5164/5178 · 6 TICs. Alvo: 30+ FPS sem crash nem freeze no import.
- O fecho da 0.11-W (relatório FECHO + R-048) só corre por ordem do dono
  (o BLOCO B).

## §2 O que está FEITO

- 0.6.x–0.9.5 — fundação da engine (V.ONI core, device real, CI 5 jobs).
- 0.9.6 + 0.9.6.4–0.9.6.20 (vc 48–67) — identidade, grupos UI, editor,
  import glTF real, orçamento 3D, leis de desenho/toque, janelas.
- 0.10-M (vc 68) — .gmesh v3 por blocos: import streaming, render por
  blocos, texturas, thumbs; PASSO 6 selou a tabela de medições.
- 0.11-H FASE 1 hierarquia (vc 69) — drag-reparent, apagar/duplicar
  sub-árvore, colapso persistente, ícone por tipo.
- 0.11-M FASE 2 materiais glTF (vc 70) — parse completo, GTB2, props no
  lit, blend por blocos, samplers no upload.
- 0.11-S FASE 3 esqueleto/animação (vc 71) — slerp/cubic/step, UBO 128
  bones, V.ONI stop/blend, fix do conversor de clips skinados.
- 0.11-T FASE 3.5a GPU efficiency (vc 72) — skinned cull + LOD por
  distância; corrida latente do corte paralelo documentada (§4.1).
- 0.11-T-FIX Perna 1 (vc 73) — contorno da corrupção do corte paralelo.
  Perna 2 ADIADA pelo dono (ver §7).
- 0.11-W ABERTURA (docs) — cf6279a.
- 0.11-W 3.5b instancing (vc 74, d0c765d) — blocos de mesma assinatura
  6 TICs → 1 dc; teto 64/draw; R-047.
- 0.11-W 3.75a streaming progressivo (vc 75, 0c11b14) — o freeze do
  import morre por orçamento; pico de RAM = 1 bloco (R-041).
- 0.11-W 3.75b impostores (vc 76, 1524148 + caaef02) — PNG alfa por
  distância; dc do TIC longe = 1; R-049; fix caaef02 (o ImpostorIndex.cpp
  faltava no CMake do app).
- 0.11-Y Y-0 Ícone no launcher ✅ vc 77 — o «G em chama»
  (docs/iconegonevv.png) no launcher: adaptativo (fundo #08080A +
  foreground na zona segura) + legacy composto nas 5 densidades; pins
  y0_* ×7 (test_fase_y0_icone.cpp); o ícone antigo (0.9.6.10) fora do
  launcher, intacto no histórico.
- 0.11-Y Y-0-FIX Escala do G ✅ vc 78 — o pin C33 apanhou o G a ~25 %
  (a bbox do HALO fazia o enquadramento); a bbox de escala passa a ser
  o CONTEÚDO SÓLIDO (alpha==1 ⇔ lum ≥ T1) e o G ocupa 65 % da zona
  segura (39,7 % do canvas; legacy 65 % do lado); 15 PNGs regenerados,
  XMLs/manifest intactos; pin y0_escala NOVO (mede os PNG reais via
  stb_image) — pins y0_* ×8; M-Y0-fix vermelho→reposto→verde.
- 0.11-Y Y-1 Tema ✅ vc 79 — o poço #08080A (o clear lê o token) + o
  vidro #151517/#1E1E21 (-5 %, mesmo alfa) + a hairline ÂMBAR
  #4A3714 α0.60 + accent2 #FF6B00 e glow #FF3D00 α0.25 NOVOS; o botão
  ativo é a chama (glow 2 quads α decrescente + gradiente vertical
  accent→accent2 por vértice, zero mudanças de shader); glassTopLine
  (1 dp, 20 %) pronta para a Y-3; pins y1_* ×5 (TU novo test_fase_y1_tema.cpp)
  + M-Y1a/b/c/d vermelho→reposto→verde; R-028 recalibrada (a border
  sai da rampa neutra — é o âmbar do accentDim).
- 0.11-Y Y-2 Layout ✅ vc 80 — as regras CruzaTabBar/PainelFora no
  validador (a faixa pela fonte única, os véus, o scope modal) + a
  flag auditável overlay (o toast) + a FASE 31 (a varredura de 13
  ecrãs × 3 checks pelo frame de produção); 3 BUGS REAIS curados (o
  toast dp(), o back 36×36, os rótulos da régua); Sobreposto sem
  filhos clipped; o probe 18.2 lê o TOKEN (o leftover da Y-1 que o CI
  apanhou); pins y2_* ×3 + M-Y2a/b vermelho→reposto→verde.

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

## §5 Regras de convivência

- Um commit só conta como fechado com o CI VERDE em todos os jobs.
- Toda a regressão estrutural ganha sentinela R-NNN com a mutação COLADA
  no relatório (quebrar → vermelho → repor → verde).
-Compat byte a byte nos formatos .goni/.gmesh; defaults novos explícitos.
- Cláusula calma de campanha: só o que o prompt da campanha manda.
- PARAR entre campanhas — a próxima só arranca com CONTINUAR do dono.
- Se o estado real divergir do esperado: PÁRA e reporta.
- Commit que fecha sub-fase com código TEM de incluir STATE.md (o gate
  docs-lint valida; ver §6).

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
- `docs/RELATORIO-0.11-CTX.md` — o relatório da campanha em curso (a
  medição da redução, as mutações, as divergências corrigidas).
- `docs/RELATORIO-0.11-W-*.md` — a 0.11-W (o fecho é o BLOCO B).
- `docs/RELATORIO-0.11-Y-Y0.md` — o relatório da Y-0 (o ícone no
  launcher; as mutações e os limites da extração).
- `docs/RELATORIO-0.11-Y-Y0-FIX.md` — o relatório do Y-0-FIX (a escala
  do G: a medição do C33, a regra do sólido, o pin y0_escala, a
  M-Y0-fix).
- `docs/RELATORIO-0.11-Y-Y1.md` — o relatório da Y-1 (o tema: o poço,
  a chama, o gradiente por vértice, as primitivas do cristal, as
  mutações M-Y1a..d).
- `docs/RELATORIO-0.11-Y-Y2.md` — o relatório da Y-2 (o layout: as
  regras CruzaTabBar/PainelFora, a varredura FASE 31, os 3 bugs reais,
  as mutações M-Y2a/b).
- `docs/RELATORIO-0.11-Y-Y3.md` — o relatório da Y-3 (o cristal: a
  linha do topo + os divisores gradientes, o D11 superado, as mutações
  M-Y3a/b).
- `scripts/y0_gen_icons.py` — o gerador dos PNGs do launcher a partir
  da fonte do dono (o threshold de luminância documentado lá dentro).
- `docs/arquivo/` — os relatórios das campanhas FECHADAS (71, git mv;
  o RELATORIO-0.9.6.md NÃO está lá — é a âncora viva do gate
  release-identity R-015; a 0.11-T-FIX também não — Perna 2 adiada, a
  campanha não fechou).
- `docs/TEMPLATE_PROMPT.md` — a pirâmide de leitura dos prompts.
- `VONI_referencia.md` — a referência V.ONI gerada (fonte: VoniRegistry).
- `ci/scope.txt` — os caminhos que uma campanha pode tocar.

## §9 As 4 perguntas antes de qualquer trabalho

1. §1 confirma que posso avançar? (campanha/sub-fase em curso = a minha?)
2. §4 cobre o que vou tocar? (nenhuma decisão fechada reabrindo?)
3. §7 diz que estou desbloqueado? (nenhum bloqueio no meu caminho?)
4. O prompt alinha com §3? (a tarefa é mesmo a pendente da ordem?)

Se qualquer resposta for não: PÁRA e reporta ao dono.
