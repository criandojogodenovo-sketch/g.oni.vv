# STATE.md — estado do projeto

Atualizado: 2026-10-16 · vc: 76 · Último commit: caaef02 (CI verde, run 38890243744)

> Este ficheiro é um ÍNDICE, não é a verdade — a verdade é o código.
> Em caso de divergência: investigar, reportar, corrigir o índice. Nunca inventar.
> Atualiza-se no MESMO commit que fecha a sub-fase (regra §6 abaixo).

## §1 Onde estamos

- **Campanha em curso: 0.11-CTX** (contexto enxuto — só documentação).
  - CTX-1 STATE.md — EM CURSO neste commit.
  - CTX-2 CODEMAP.md — pendente.
  - CTX-3 arquivo + gate docs-lint + template + medição — pendente.
  - Cláusula calma: zero código de produção; CI só é tocado no gate docs-lint.
- **0.11-W (Render Systems) implementada e com CI verde** — falta o PIN do
  dono no C33 real: instalar o APK vc 76, importar a cena de 203 MB,
  correr o bench R-017, colar o PNG do HUD. Baseline: FPS 5 · bl 5164/5178 ·
  6 TICs. Alvo: 30+ FPS sem crash nem freeze no import.
- O fecho da 0.11-W (relatório FECHO + R-048) só corre DEPOIS da 0.11-CTX
  fechar, por ordem do dono.

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

## §3 O que está PENDENTE (por ordem)

1. **0.11-CTX CTX-2** — criar CODEMAP.md (mapa do código + receitas).
2. **0.11-CTX CTX-3** — arquivar relatórios de campanhas fechadas em
   docs/arquivo/, encurtar README (<200 linhas), criar docs/DIARIO.md e
   docs/TEMPLATE_PROMPT.md, ligar o gate STATE.md no CI, medir a redução
   de contexto em docs/RELATORIO-0.11-CTX.md.
3. **Fecho 0.11-CTX** — relatório com a medição real; commit de fecho.
4. **Fecho 0.11-W (BLOCO B, só com CONTINUAR do dono)** — verificar R-048,
   RELATORIO-0.11-W-FECHO.md, atualizar README/BACKLOG/STATE.md; o pin
   C33 (§1) entra no §3 do fecho.
5. **3 métricas qualitativas da CTX** fecham no relatório da campanha
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
- `docs/BACKLOG.md` — rastreador de campanhas.
- `docs/RELATORIO-0.11-W-*.md` — a campanha em curso (4 relatórios).
- `docs/arquivo/` — relatórios de campanhas fechadas (a partir da CTX-3).
- `docs/TEMPLATE_PROMPT.md` — a pirâmide de leitura dos prompts.
- `VONI_referencia.md` — a referência V.ONI gerada (fonte: VoniRegistry).
- `ci/scope.txt` — os caminhos que uma campanha pode tocar.

## §9 As 4 perguntas antes de qualquer trabalho

1. §1 confirma que posso avançar? (campanha/sub-fase em curso = a minha?)
2. §4 cobre o que vou tocar? (nenhuma decisão fechada reabrindo?)
3. §7 diz que estou desbloqueado? (nenhum bloqueio no meu caminho?)
4. O prompt alinha com §3? (a tarefa é mesmo a pendente da ordem?)

Se qualquer resposta for não: PÁRA e reporta ao dono.
