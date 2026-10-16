# RELATORIO-0.11-Y-Y0.md — o ícone G.One VV no launcher

Campanha: 0.11-Y (identidade visual) · Sub-fase: Y-0 · vc 76 → 77 ·
Data: 2026-10-16 · Commit: este

## §1 O que foi feito

- **A extração do glifo** (scripts/y0_gen_icons.py:36 — T0=40, T1=90; :46
  `extract_rgba`): a fonte docs/iconegonevv.png é 1254×1254 **RGB SEM
  alpha** — o G em chama vive num tile preto arredondado. O alpha nasce
  da luminância com rampa suave (28–40 = 0; 40–90 = antialiasing; ≥90 =
  sólido) e o **un-premultiply** (o fundo é preto puro: `c = a·cor`, logo
  dividir por max(alpha, 0.35) devolve a cor âmbar exata; o piso limita o
  ganho de ruído). O 1.º corte (T0=28) deixava a BORDA do tile passar
  como halo fantasma — o tile mede 6–28 de luminância (medido em 6
  regiões longe do glifo) e o T0=40 mata-o inteiro. O gerador é
  commitado — a fonte do ícone reproduzível (a regra da casa do
  gen_app_icon).
- **O FOREGROUND, 5 densidades** (scripts/y0_gen_icons.py:38 FG_FIT=0.43,
  :89 `make_foreground`): mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/
  ic_launcher_foreground.png em 108/162/216/324/432 (o canvas 108 dp ×
  densidade). O glifo cabe na **zona segura** dos 66 dp: bbox máx = 43 %
  do canvas (um quadrado de 43 % tem diagonal 60,8 % — dentro do círculo
  seguro, nada clipa em máscara circular); o núcleo sólido do glifo mede
  29–39 % do canvas.
- **O BACKGROUND** (app/src/main/res/values/ic_launcher_background.xml:7):
  `<color name="ic_launcher_background">#08080A</color>` — a ordem
  expressa do prompt.
- **O ADAPTATIVO (Android 8+)** (mipmap-anydpi-v26/ic_launcher.xml:7 e
  ic_launcher_round.xml:7): os 2 `<adaptive-icon>` compõem
  `@color/ic_launcher_background` + `@mipmap/ic_launcher_foreground`.
- **O LEGACY (Android 7-)** (scripts/y0_gen_icons.py:39 LEGACY_FIT=0.68,
  :96 `make_legacy`): ic_launcher.png + ic_launcher_round.png compostos
  (fundo #08080A + glifo a 68 % do lado — a diagonal 0,96 cabe no círculo
  do round) nas 5 densidades (48/72/96/144/192); o round leva a máscara
  circular antialiased (supersampling 4×). 15 PNGs no total.
- **O MANIFEST** (app/src/main/AndroidManifest.xml:48-49): JÁ apontava
  `android:icon="@mipmap/ic_launcher"` + `android:roundIcon=
  "@mipmap/ic_launcher_round"` — attrs INTACTOS (o passo 6: "não muda
  nada"); só o comentário de proveniência (:33-45) atualiza — a entrada
  0.9.6.10 do gen_app_icon.py passa ao histórico, com o destino dos
  assets antigos documentado.
- **O BRAND in-app** (app/src/main/cpp/ui/Brand.h:16-26): os ficheiros
  EXISTEM (0.9.6.18 D6) — mas não há paleta neles para atualizar: a cor
  vem do CHAMADOR (o parâmetro `color[4]` de `brand::drawIcon`), a paleta
  vive no Theme (gate R-030 proíbe hex fora dele) e tocar no Theme é Y-1.
  A única atualização honesta: o comentário de proveniência (o launcher
  deixou de nascer do vetor da marca). Geometria e contrato INTACTOS — o
  jni_parity (UiIcons.drawBrand ↔ Brand.cpp) não é afetado.
- **OS PINS** (tests/test_fase_y0_icone.cpp, 297 linhas, TU novo no
  tests/CMakeLists.txt:334-337): os 7 do §2 do prompt, no padrão da casa
  (GONI_ROOT_OVERRIDE para as mutações, o mesmo dos pins ctx_*).
- **O BUMP** (app/build.gradle:29): versionCode 76 → 77.
- **OS ÍNDICES**: STATE.md (o §1 declara a 0.11-Y/Y-0 FECHADA, o §2
  ganha a linha da Y-0, o cabeçalho vc 77), CODEMAP.md (o tests/ e o
  scripts/ ganham as entradas novas), docs/RELATORIO-0.9.6.md (o §14
  ganha o bullet da Y-0 com o par versionCode 77 — a âncora do gate
  release-identity).

## §2 Provas

- **Pins novos: 7/7 VERDE** local (g++ test_main + test_fase_y0_icone,
  GONI_ROOT_OVERRIDE no repo):
  y0_foreground · y0_background · y0_adaptive · y0_legacy · y0_manifest ·
  y0_aapt2 · y0_antigo_removido — «0 teste(s) com falha».
- **M-Y0a — foreground só numa densidade** (4 foregrounds removidos na
  sandbox):
  ```
  y0_foreground  ...  FALHOU  tests/test_fase_y0_icone.cpp:97
    mipmap-mdpi/ic_launcher_foreground.png em falta ou PNG inválido
    FALHOU  ...:97  mipmap-hdpi/... em falta ou PNG inválido
    FALHOU  ...:97  mipmap-xhdpi/... em falta ou PNG inválido
    FALHOU  ...:97  mipmap-xxhdpi/... em falta ou PNG inválido
  y0_foreground  FALHOU
  ```
  Reposto: «0 teste(s) com falha». ✓
- **M-Y0b — o manifest com o ícone antigo** (@mipmap/ic_launcher_antigo):
  ```
  y0_manifest  ...  FALHOU  tests/test_fase_y0_icone.cpp:185
    text.find("android:icon=\"@mipmap/ic_launcher\"") != std::string::npos
  y0_manifest  FALHOU
  y0_aapt2     ...  FALHOU  tests/test_fase_y0_icone.cpp:204
    citaLauncher && citaRound
  y0_aapt2     FALHOU
  2 teste(s) com falha
  ```
  Reposto: «0 teste(s) com falha». ✓
- **M-Y0c (extra) — o PNG antigo de volta** (o blob do gerador 0.9.6.10
  restaurado em mipmap-mdpi/ic_launcher.png):
  ```
  y0_antigo_removido  ...  FALHOU  tests/test_fase_y0_icone.cpp:288
    mipmap-mdpi/ic_launcher.png é o PNG ANTIGO do gerador 0.9.6.10
    (sha256 2a41711a…be45) — o ícone antigo sobrou
  1 teste(s) com falha
  ```
  Reposto: «0 teste(s) com falha». ✓
- **test_core**: o CI é o árbitro (o run deste commit; os 7 pins
  y0_* entram na suíte ao lado dos ctx_*).
- **c33_virtual**: sem alteração (a Y-0 não toca no C++ do device) — o
  CI confirma no run deste commit.
- **aapt2**: a validação BINÁRIA corre no job «APK release arm64» do CI
  (o build compila os recursos novos e o gate release-identity fecha
  por cima); o pin y0_aapt2 aferra a FONTE que o aapt2 consome.
- **versionCode: 76 → 77** (app/build.gradle:29, declarado no §14 do
  RELATORIO-0.9.6.md para o gate R-015).

## §3 Pin C33 (o dono preenche)

- Instalar o APK vc 77 no C33.
- Ver o ícone novo no launcher (G em chama).
- PNG do launcher com o ícone novo.
- (o dono cola os PNGs aqui)

## §4 Limites / não-verificado

- **O PNG não tinha alpha** — fundo preto sólido com tile (§0.2.1:
  cantos 0–5 de luminância, o tile 6–28). Threshold usado: **T0=40,
  T1=90** com rampa suave + un-premultiply (piso 0.35) —
  scripts/y0_gen_icons.py:36. O 1.º corte (T0=28) deixou a borda do tile
  passar como halo fantasma (visível na pré-visualização) e foi apertado
  para 40 — o tile morre inteiro. **Qualidade**: a chama e os biséis do
  G preservados; o que resta de halo é o anel de brilho do PRÓPRIO
  desenho (visível na fonte), invisível nos tamanhos de launcher
  (48–192 px). Pré-visualizações compostas verificadas por inspeção
  (foreground sobre #08080A e o legacy 192).
- **A divergência do nome da fonte** (§0.2.1): o prompt dizia
  `docs/iconegongevv.png` — o ficheiro real é **`docs/iconegonevv.png`**
  (um «g» a menos). Único candidato a ícone novo no repo (o ANTIGO,
  docs/icon-gone-512.png, é citado no próprio prompt como histórico e
  fica intacto); o conteúdo confirma (o G em chama âmbar, centro
  254,187,2). Registrado como a divergência que era.
- **Brand.h/.cpp**: existem; a paleta NÃO vive neles (a cor vem do
  chamador; R-030; o Theme é Y-1) — o passo 7 fecha sem alteração de
  código de desenho, só o comentário de proveniência. Reportado como
  manda o prompt.
- **O §3 do STATE.md não tinha linha da Y-0 para remover** (o prompt
  previa «§3 perde a linha da Y-0» — a pendente nunca chegou a ser
  escrita; o no-op fica registrado aqui).
- **A zona segura e o legacy a olho**: o FG_FIT=0.43 e o LEGACY_FIT=0.68
  são decisões visuais seguras (as matemáticas das máscaras na secção
  §1); o gosto final é do dono — o §3 (pin C33) fecha com os PNGs dele;
  afrouxar/apertar é regenerar com os 2 constantes do gerador.
- **O CI deste commit** — a corrida em curso no momento da escrita; a
  sub-fase declara-se só com o VERDE em TODOS os jobs.
- **A sentinela R-NNN**: nenhuma nova — a Y-0 é mudança de RECURSOS (a
  regra «regressão estrutural ganha sentinela» não se aplica); os 7 pins
  y0_* guardam o contrato. O próximo R-NNN livre continua o R-050.

## §5 Ficheiros tocados

- app/src/main/res/mipmap-mdpi/ic_launcher.png (substituído — legacy 48)
- app/src/main/res/mipmap-mdpi/ic_launcher_round.png (substituído)
- app/src/main/res/mipmap-mdpi/ic_launcher_foreground.png (NOVO, 108)
- app/src/main/res/mipmap-hdpi/ic_launcher.png (substituído — 72)
- app/src/main/res/mipmap-hdpi/ic_launcher_round.png (substituído)
- app/src/main/res/mipmap-hdpi/ic_launcher_foreground.png (NOVO, 162)
- app/src/main/res/mipmap-xhdpi/ic_launcher.png (substituído — 96)
- app/src/main/res/mipmap-xhdpi/ic_launcher_round.png (substituído)
- app/src/main/res/mipmap-xhdpi/ic_launcher_foreground.png (NOVO, 216)
- app/src/main/res/mipmap-xxhdpi/ic_launcher.png (substituído — 144)
- app/src/main/res/mipmap-xxhdpi/ic_launcher_round.png (substituído)
- app/src/main/res/mipmap-xxhdpi/ic_launcher_foreground.png (NOVO, 324)
- app/src/main/res/mipmap-xxxhdpi/ic_launcher.png (substituído — 192)
- app/src/main/res/mipmap-xxxhdpi/ic_launcher_round.png (substituído)
- app/src/main/res/mipmap-xxxhdpi/ic_launcher_foreground.png (NOVO, 432)
- app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml (NOVO)
- app/src/main/res/mipmap-anydpi-v26/ic_launcher_round.xml (NOVO)
- app/src/main/res/values/ic_launcher_background.xml (NOVO)
- app/src/main/AndroidManifest.xml (o comentário da proveniência; attrs
  intactos)
- app/src/main/cpp/ui/Brand.h (o comentário da proveniência; geometria e
  contrato intactos)
- app/build.gradle (versionCode 76 → 77)
- tests/test_fase_y0_icone.cpp (NOVO — os 7 pins)
- tests/CMakeLists.txt (+ o TU dos pins)
- scripts/y0_gen_icons.py (NOVO — o gerador commitado)
- STATE.md (o §1/§2/cabeçalho + o §8 — a regra §1.2: o commit leva o
  STATE no mesmo commit)
- CODEMAP.md (o tests/ + o scripts/ + o cabeçalho vc 77)
- docs/RELATORIO-0.9.6.md (o §14 ganha o bullet da Y-0 — o par
  versionCode 77 do gate R-015)
- docs/RELATORIO-0.11-Y-Y0.md (NOVO — este relatório)

INTOCADO (de propósito): docs/icon-gone-512.png (o histórico, a ordem do
prompt), app/src/main/res/drawable-nodpi/gone_logo.png (o logo in-app dos
cards/header — fora da lista «O QUE FAZER»), ui/Theme.h (é Y-1), o
restante do cpp/ e do java/ (zero lógica tocada).
