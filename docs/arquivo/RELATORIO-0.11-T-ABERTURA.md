# 0.11-T — ABERTURA (FASE 3.5 GPU EFFICIENCY)

## §0 Estado de entrada (verificado)

- 0.11-S (FASE 3-CORE) fechada — versionCode 71, CI verde (run
  38466867752, verificado via API GitHub nesta abertura: status
  `completed`, conclusion `success`, head_sha `3575344` = HEAD do main)
- Jobs do run (verificados um a um): Testes do core (Linux) ✓ ·
  C33 virtual (dispositivo + sentinelas + gates) ✓ · init-keystore
  `skipped` (por desenho — só corre na 1.ª vez que a keystore não
  existe) · APK release arm64 (assinado via secrets) ✓ — o gate
  release-identity (R-015) corre DENTRO deste job («Gate
  release-identity (R-015 — artifact == build == relatorio)») ✓ ·
  verify-entry-symbols ✓
- **Nota honesta sobre os «5 jobs»**: o prompt nomeia
  (core-tests, c33-virtual, build-release, symbol-gate,
  release-identity). A realidade do workflow: 5 jobs, dos quais 4
  executáveis VERDES e o init-keystore skipped por desenho; o
  release-identity NÃO é um job — é um STEP de gate dentro do job do
  APK (R-015). Nenhum vermelho; a substância do requisito está verde.
- Nome do artefacto: `goni-vv-0.9.6-release-signed` (1820489 B,
  não-expirado) + `layout-screens-c33-virtual` +
  `c33-virtual-replay-log`. O R-015 compara o **versionName** (0.9.6)
  com o nome do artefacto e com a versão do relatório — o versionCode
  (71) viaja DENTRO do APK (`app/build.gradle` linha 29:
  `versionCode 71`, verificado no repo). O gate fechou verde no run —
  a identidade está consistente.
- APK no C33: **o sign-off físico é do dono** (o artefacto do run
  38466867752 está publicado para instalação; o banner de boot tem de
  dizer vc 71 — NÃO re-verificável nesta sessão, que não tem o device).
  Proxy do harness: c33_virtual **857/857 checks VERDE** no run.
- Cena dos 10 personagens no C33: **o sign-off físico é do dono**
  (a FASE 26 do device virtual — +33 checks — repete no Linux o caminho
  real: 10 personagens = 10 draws + 10 uploads do UBO, animação sem
  deformação, crossfade completo). Se o device real mostrar algo
  diferente, PÁRA e reporta (regra 1.7).
- Bench herdado: **0.211 ms/frame de CPU** (10 personagens × 64 ossos ×
  300 frames = 63.3 ms; pin `fase3_bench_10_personagens_x_300_frames`)
- Confirmação: o CPU NÃO é o gargalo da animação (1.3% de 60 fps) —
  se o frame total exceder 16.6 ms no C33, o gargalo é GPU (draws,
  verts, state changes) — é isso que a FASE 3.5 ataca.

## §0.1 DESVIOS do prompt detetados na verificação (nenhum escondido)

1. **As sentinelas: a R-044 JÁ ESTÁ OCUPADA** — o prompt assumia
   «sentinelas até à R-043» e atribuía R-044 à 3.5a. O registo real
   (docs/REGRESSOES.md) tem a **R-044 · AS THUMBS PRÉ-RENDERIZADAS**
   (0.10-M PASSO 5C; sentinelas em test_thumbs.cpp +
   test_010m_medicoes.cpp). A numeração da campanha DESLOCA-SE:
   **3.5a → R-045 (regress_lod_cull) · 3.5b → R-046
   (regress_instancing) · 3.5c → R-047 (regress_sorting_earlyz) ·
   3.5d → R-048 (regress_occlusion)**. R-045..R-048 verificados LIVRES
   no registo e no código.
2. O relatório PASSO 4 chama-se `docs/RELATORIO-0.10-M-PASSO4-BLOCOS.md`
   (o prompt dizia «PASSO4-RENDER-BLOCOS») — lido na mesma (a arquitetura
   do frustum por AABB + os 920 blocos + os orçamentos).
3. Os caminhos cpp do prompt (`cpp/core/…`, `cpp/render/…`) vivem em
   `app/src/main/cpp/…` no repo — os mesmos ficheiros.

## §1 Regra de convivência

> "não declarar a FASE 3.5 fechada até o pin docs/FASE3.5-PIN.md
>  passar no C33 com o motor selado"

## §2 Sub-fases

- 3.5a — skinned cull + LOD por distância
- 3.5b — GPU instancing (meshes repetidos)
- 3.5c — sorting de draws + early-Z pre-pass
- 3.5d — occlusion (só se 3.5a-c não chegarem ao pin)

## §3 Fundação herdada (NÃO REFAZER — está feito)

Da 0.11-S:
- UBO 128 bones (glstub já modela)
- Interpolações glTF completas (Step/Slerp/CUBICSPLINE)
- V.ONI stop()/blend() (registo + referência regenerados)
- FIX do conversor (clips do .gm)

NÃO TOCAR neste trabalho. A FASE 3.5 é por cima.

## §4 Dívida declarada (fora desta campanha)

- 3.75 (Brain-Ready Anim) — root motion + on_event + state machine.
  **Fica para a próxima.**
- Retargeting — dispara com o 3.º humanóide de rig diferente.
- Streaming de skin >65535 verts — BACKLOG 0.10-A.
- Occlusion (3.5d) — só se 3.5a-c falharem o pin.

## §5 Segurança (o passo 0.2.1 — não é opcional)

O PAT da campanha **voltou a circular em texto plano** na conversa
(o MESMO token já exposto antes). **Eu não consigo revogar o token por
ti** — a revogação é feita em github.com/settings/tokens pelo dono.
**AÇÃO OBRIGATÓRIA do dono: revogar o token atual e gerar um novo para
esta campanha no fim da sessão em que ele voltou a aparecer** (o
princípio do 0.2.1 aplica-se: exposto = queimado). Enquanto esta
campanha correr, o token entra por canal combinado com o dono (nunca
recolocado em relatórios, docs ou código).
