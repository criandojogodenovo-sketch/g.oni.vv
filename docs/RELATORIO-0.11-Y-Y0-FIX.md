# RELATORIO-0.11-Y-Y0-FIX.md — o fix da escala do G no launcher (0.11-Y)

Data: 2026-10-17 · versionCode: 78 (o 77 fica INTACTO como história) ·
Base: a2d2248 (a Y-0, CI verde no run 38919734326)

O pin C33 do dono instalou o APK vc 77 e apanhou o bug: o G no launcher
media ~25 % do ícone; o contrato da campanha é ~65 % (o «G a ~65 % do
círculo» do prompt da 0.11-Y). Este fix é cirúrgico: MUDA SÓ A ESCALA —
zero XMLs tocados, zero manifest tocado, zero código de lógica, zero
tema (o Theme é Y-1, só com CONTINUAR).

## §1 O que foi feito

**A causa raiz (medida, não adivinhada).** A extração da Y-0 está
correta — o bug vivia no ENQUADRAMENTO. O gerador escalava a bbox dos
pixels com alpha ≥ 10: isso é o HALO, o brilho suave do fogo, não o
glifo. Na fonte (docs/iconegonevv.png, 1254×1254), a bbox do halo mede
1022×1043 e a do CONTEÚDO SÓLIDO (alpha == 1 ⇔ luminância ≥ T1=90, o
mesmo T1 calibrado da extração) mede 870×927: o G sólido vivia a ~85 %
da caixa do halo em largura e a ~89 % em altura — e como o halo ainda
continua a transição suave do fogo, o que o dono lê como «o G» no
launcher ficava dentro de tudo isso. A medição do C33 (~25 %) fecha com
a conta do foreground vc 77: o sólido media 28,9 % do canvas de 108 dp —
num launcher que aplica qualquer encolhimento próprio ao foreground do
adaptativo (prática comum), o percebido desce para a casa dos 25 %.

**A regra nova do enquadramento (a ordem do dono, ao pé da letra).** A
bbox de escala passa a ser a do CONTEÚDO SÓLIDO — o núcleo opaco do
fogo — e o G sólido passa a ocupar 65 % da ZONA SEGURA do adaptativo:
0.65 × 66 dp = 42,9 dp num canvas de 108 dp = 39,7 % do canvas. Três
decisões dentro dessa regra, todas documentadas no gerador:

1. **O halo acompanha à MESMA escala** (não é espremido): o recorte
   continua a ser a bbox completa do glifo; o que muda é o fator — a
   escala é calculada pelo SÓLIDO e aplicada a tudo. O brilho do fogo
   mantém as proporções que o dono desenhou.
2. **O centro do paste é o centro do SÓLIDO** (não o do halo): o glifo
   fica centrado no canvas; o halo assimétrico (o fogo arrasta mais
   brilho para baixo) distribui-se como está na fonte.
3. **Nada clipa — provado por geometria**: a diagonal da caixa do
   sólido escalado é 56,1 dp < 66 dp da zona segura (o conteúdo
   reconhecível NUNCA sai da zona segura); a diagonal da caixa do halo
   é 71,3 dp < 72 dp do viewport do adaptativo — até o brilho cabe
   inteiro no círculo visível; e no legacy round o canto mais longe do
   sólido fica a 82,3 px de um raio de 96 px (a 192 px).

**Verificação do tile (a ordem: «excluir halo suave e rim light»).** A
moldura do tile foi re-medida no Y-0-FIX: luminância máxima de 6,3 na
faixa externa de 38 px — o rim light do tile morre inteiro no T0=40 e
NUNCA contamina a bbox. O que sobra acima do T0 é glifo: a bbox sólida
(870×927) e a do halo (1022×1043) são o fogo do G, sem rectângulo do
tile lá dentro.

**O legacy idem.** A mesma regra nos 10 PNGs legacy: o G sólido a 65 %
do lado (era 68 % DA BBOX DO HALO — o sólido ficava nos ~58 %). O
quadrado e o round regenerados nas 5 densidades com a mesma extração de
sempre (T0=40/T1=90, rampa suave, un-premultiply, piso 0.35) e a mesma
máscara circular antialiased por supersampling 4×.

**Os 15 PNGs regenerados, nada mais tocado.** 5 densidades ×
(foreground + launcher + launcher_round). Os 2 XMLs do adaptativo
(mipmap-anydpi-v26), o values/ic_launcher_background.xml (#08080A) e o
AndroidManifest.xml ficam BYTE A BYTE (a ordem do prompt: «Mantém os
XMLs adaptativos»). O gone_logo.png e o docs/icon-gone-512.png não
tocados. O gerador scripts/y0_gen_icons.py leva a lógica nova (a ordem:
«para ser reproduzível») — GLYPH_FILL=0.65, SOLID_AMIN=255, e o log do
gerador imprime a escala geométrica aplicada.

## §2 Provas

**As medições (o antes → o depois, no mesmo critério):**

| medida | vc 77 (o bug) | vc 78 (o fix) | alvo |
|---|---|---|---|
| sólido do foreground / canvas (alpha ≥ 250) | 28,9 % | **35,9 %** | 35,9 % (a leitura calibrada de 39,7 % geométricos) |
| sólido do legacy / lado (lum ≥ 90) | ~46 % | **59,4 %** | 59,4 % (a leitura calibrada de 65 % geométricos) |
| bbox de ENQUADRAMENTO usada | a do halo (alpha ≥ 10) | a do sólido (alpha == 1) | a ordem do dono |

**A nota honesta sobre os 9 %.** A escala geométrica aplicada pelo
gerador é exata: o sólido da FONTE (927 px de altura) é mapeado em
0.3972 × canvas. A LEITURA por threshold nos PNGs finais fica ~9 %
abaixo (35,9 % em vez de 39,7 %) porque a borda do fogo é antialiased:
o kernel do LANCZOS no reenquadramento mistura o núcleo opaco com a
rampa da transição e os pixels de borda caem abaixo do threshold. É o
MESMO fenómeno nas duas famílias (o legacy lê 59,4 % em vez de 65 % —
no composto sobre #08080A, lum ≥ 90 equivale a alpha ≥ ~0,47). O pin
aferra os VALORES CALIBRADOS medidos; o §2 do gerador imprime os
geométricos; nenhum dos dois esconde o outro.

**Os pins — 8/8 VERDES no repo real** (mini-runner local, o mesmo
binário que o CI compila no test_core): y0_foreground, y0_background,
y0_adaptive, y0_legacy, y0_manifest, y0_aapt2, y0_antigo_removido (os 7
da Y-0, intactos) + **y0_escala NOVO**.

**O pin y0_escala (o «porquê» dele).** O dono deixou a porta aberta:
«NÃO inventes um pin novo de escala a menos que faça sentido». Faz — é
o ÚNICO pin que teria apanhado ESTE bug antes do APK, e o bug já custou
um ciclo de pin C33. Ele mede os PNG REAIS (não a fonte, não o
gerador): descodifica os 10 PNG (5 foreground + 5 legacy quadrados) com
stb_image — a implementação única já vive no PngLoader.cpp da suíte
(declarações apenas no pin; stbi_load_from_memory porque o
STBI_NO_STDIO do PngLoader é o contrato), mede a bbox sólida em todas
as densidades e aferra as janelas [33..39] % (foreground) e [56..63] %
(legacy). A folga cobre o reenquadramento por densidade (o LANCZOS
corre em 5 escalas distintas — o medido vai de 35,2 % a 36,1 % no
foreground e de 58,3 % a 59,7 % no legacy).

**O que o pin NÃO verifica (honesto).** A aparência final no launcher
do C33 — a máscara do launcher, o encolhimento que cada launcher
aplica ao foreground, o fundo desenhado por trás — é HUMANA: é o pin
C33 do dono (§3). O pin aferra a escala dos PNG commitados; o dono
aferra a percepção.

**Gates locais.** docs-lint (R-016) verde; release-identity (R-015)
verde com o par 0.9.6 / versionCode 78 declarado no §14 do
RELATORIO-0.9.6.md (a entrada nova no topo, a da Y-0 intacta por baixo
— a história não se reescreve); o STATE.md viaja NESTE commit (a regra
§1.2: commit que toca app/ e declara sub-fase leva o STATE).

## §3 Pin C33 (o dono preenche)

- [ ] Instalar o APK **vc 78** (o artifact goni-vv-0.9.6-release-signed
      do run verde deste commit).
- [ ] Ver o ícone no launcher: o G a ~65 % do círculo (o sólido
      geométrico está em 65 % da zona segura de 66 dp; com o brilho do
      fogo à volta, o percebido no círculo de 72 dp fica na casa dos
      60–70 %).
- [ ] PNG do launcher novo → docs/y0-fix-c33.png.
- [ ] Se estiver bom → a Y-0 fecha (de vez) e a Y-1 arranca com
      CONTINUAR.

**Se o G ainda aparecer pequeno no C33**: a ordem do prompt está
registada — PÁRA, reporta com o PNG, não inventa mais escala. Nesse
caso a suspeita é o ENCOLHIMENTO do launcher do device sobre o
foreground do adaptativo (o mesmo fator que fez 28,9 % ler como ~25 %)
— decide-se com o dono com o PNG do launcher na mão, não com outro
chute de constante.

## §4 Mutações / limites

**M-Y0-fix (a mutação do fix, corrida na sandbox — cópia integral do
repo, o pin apontado por GONI_ROOT_OVERRIDE, o mesmo padrão M-Y0a/b/c):**

- A mutação: `GLYPH_FILL = 0.65 → 0.25` E `LEGACY_FIT = 0.65 → 0.25` no
  gerador da sandbox (a escala do bug de volta, como o prompt descreve)
  + regeneração dos 15 PNGs na sandbox.
- O resultado: **10 EXPECTs VERMELHAS, todas no y0_escala** — os 5
  foregrounds fora da janela (medidos 11,1–13,6 % < 33 %) e os 5
  legacys fora (abaixo de 56 %). **Os outros 7 pins aguentam VERDES** —
  dimensões, XMLs, manifest e impressão digital do ícone antigo não
  mudam com a escala; o pin novo é o ÚNICO que apanha (isolamento
  provado, não assumido).
- A reposição: constantes de volta a 0.65 + regeneração → **8/8 VERDE**
  na sandbox, e o repo real re-confirmado verde no mesmo binário.

**Limites / não-verificado:**

1. O encolhimento que o launcher do C33 aplica ao foreground do
   adaptativo é do device — não medível na sandbox; o pin C33 é o
   árbitro (§3).
2. A leitura por threshold fica ~9 % abaixo da geométrica (a borda
   antialiased) — documentada no §2 e nos comentários do pin; não é
   desvio, é a física do reenquadramento.
3. Os PNGs round do legacy não são medidos pixel a pixel pelo pin (o
   composto é idêntico ao quadrado antes da máscara; a máscara circular
   é a da Y-0, intocada). O y0_legacy continua a verificar estrutura e
   dimensões.
4. Nenhuma sentinela R-NNN nova: mudança de RECURSOS com pin próprio
   (o mesmo julgamento da Y-0) — o y0_escala é o guarda da escala.

## §5 Ficheiros tocados

- app/src/main/res/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/
  ic_launcher_foreground.png — regenerados (5)
- app/src/main/res/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/
  ic_launcher.png — regenerados (5)
- app/src/main/res/mipmap-{mdpi,hdpi,xhdpi,xxhdpi,xxxhdpi}/
  ic_launcher_round.png — regenerados (5)
- scripts/y0_gen_icons.py — a lógica de escala (sólido 65 % da zona
  segura; o halo à mesma escala; o centro do sólido)
- tests/test_fase_y0_icone.cpp — o pin y0_escala novo (stb_image,
  10 medições) + o cabeçalho do TU atualizado
- app/build.gradle — versionCode 77 → 78
- docs/RELATORIO-0.9.6.md — a entrada Y-0-FIX no §14 (o par 0.9.6/78
  para o gate R-015; a entrada da Y-0 intacta)
- STATE.md — o §1 (Y-0-FIX FECHADA), §2 (+linha), §8 (+relatório), vc 78
- CODEMAP.md — vc 78; as menções do pin de escala e do gerador
- docs/RELATORIO-0.11-Y-Y0-FIX.md — este relatório
