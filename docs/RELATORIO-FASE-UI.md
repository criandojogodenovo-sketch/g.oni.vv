# RELATORIO FASE UI — O REDESIGN VISUAL (a ordem do dono, envios 1/5 + 2/5)

> A campanha FASE UI: SÓ visual, layout e UX (a cláusula calma: core,
> V.ONI, importadores, física e .gmesh INTOCÁVEIS). As partes A..E, um
> commit por parte, CI verde entre elas. As regras da fase: toque mínimo
> 40dp, texto mínimo 12sp, sem blur/sombras pesadas, valores visuais SÓ do
> ficheiro de tokens, teste automático de sobreposição, relatório de 16
> itens + capturas em 3 resoluções (retrato e paisagem) + a lista do que
> NÃO foi feito (§8).

## §0 A abertura (as verificações antes do código)

- **vc**: 110 no HEAD 585217f (o 0.12-Y D + o fix do replay fechados) —
  confirmado no build.gradle e no STATE.md.
- **CI**: 4/4 SUCCESS no 585217f (Testes do core Linux · C33 virtual
  1009/0 · APK release arm64 assinado · verify-entry-symbols) — via API
  GitHub no arranque desta campanha.
- **STATE §9**: a ordem do dono É a fila (a FASE UI entra depois da 0.12-Y
  D; nenhuma decisão fechada reabre — a paleta é ORDEM NOVA do dono, o
  padrão Y-3/D11 de superação legítima).
- **Leituras**: STATE.md · CODEMAP.md · ui/Theme.h (o ficheiro único de
  tokens) · os gates que aferram a pele (theme_hex_check R-030 ·
  ui_vocab_check · projects_ui_check TOKENS_F · release_identity R-015) ·
  os testes que medem cor (y1_tema · y3_cristal · idioma · qol · efeitos ·
  toolbar · o sentinela R-030 regress_identidade_grafite_ambar).

## §1 PARTE A — TOKENS E IDENTIDADE (o ficheiro único + a brasa)

**O que mudou** (o Theme.h É o ficheiro único de tokens da casa — as
cores, os espaços, os raios e a tipografia já moravam lá; a parte A dá-lhe
a paleta do dono e completa a escala):

- **A PALETA DO DONO (hexes EXATOS)**: bg #0B0B0D (o fundo viewport) ·
  surface #121215 α0.85 (os painéis — o vidro da casa mantém o α) ·
  surface2 #1A1A1E α0.86 (vidro denso) · card #1B1B20 (OS ELEMENTOS — o
  dono deu o MESMO hex da escada 0.12-C, o token nem mexe) ·
  elevated/border #26262C (o elevado e AS LINHAS 1px) · text1 #E8E6E3 ·
  text2 #8A8A93 · **accent #FF5A1F — A BRASA, o destaque ÚNICO** (SÓ em
  seleção, aba ativa, Play, botão principal, cursor de tempo) ·
  accentDim #2B1810 (O FUNDO DE SELEÇÃO) · axisX/Y/Z #E5484D · #3FB950 ·
  #3B82F6 (os eixos EXATOS do dono — conteúdo, não chrome).
- **O ÂMBAR MORREU**: accent #FFB020 → #FF5A1F; accentPress #E09A00 →
  #D94A16; accent2 #FF6B00 → #E0440F (a brasa escura — a base do
  gradiente dos ativos, a MESMA família: o destaque continua ÚNICO); glow
  #FF3D00 α0.25 → #FF5A1F α0.25; fireRed/fireOrange re-alinhados à brasa
  (o gradiente dos botões ativos). A RAMPA do LOGO (fire0..fire3) fica
  INTACTA — o «G em chama» é identidade, não chrome. O warn #FF8A3D fica
  (semântica, nunca == accent). SEM azul como destaque (o azul do eixo Z
  e da V.ONI é CONTEÚDO — a exceção documentada de sempre).
- **A ESCALA DO DONO**: 4/8/12/16 — o kSpaceMid=12 entrou; o kSpace3
  (recuo dos filhos) desce de 24 para 16; os raios passam à banda 6-8
  (kRadiusField 4→6, kRadiusCard 8 fica). As bordas 1px mantêm-se.
- **A PAREDE DE FONTE ÚNICA**: os aliases BG/PANEL/LINE/TEXT/ACCENT/
  ACCENT2/GLOW do UiContext.h e os eixos do Gizmo.h re-ligados por
  static_assert (a pele muda NUM sítio).
- **O ESPELHO JAVA**: os tokens do ProjectManagerActivity alinhados
  (BG/SURFACE/SURFACE2/CARD/ELEVATED/BORDER/TEXT1/TEXT2/ACCENT/
  ACCENT_PRESS/ACCENT_INK/ACCENT_DIM + os 2 vidros) e os 2 hexes inline
  mortos (o matiz âmbar dos iniciais → brasa escurecida; o border do tile
  → o token BORDER); o TOKENS_F do gate projects_ui_check recalibrado.
- **A SONDA honesta do c33**: a banda da toolbar lê o fundo do TOKEN (era
  o literal (20,20,20) do mono — o achado do explorador, morto aqui).

**Contraste validado ANTES do commit** (o padrão da casa): sobre o painel
composto text1 15,1:1 · text2 5,5:1 · accent 6,1:1 · danger 4,8:1 · warn
8,0:1 · ok 6,2:1; pior caso do vidro (branco por trás) text1 9,7:1 ·
accent 3,9:1; bg↔accent 6,3:1 · bg↔accent2 4,7:1 · tinta↔brasa 6,2:1 ·
accentDim↔text1 13,6:1 — todo o texto ≥4,5:1 e todo o componente ≥3:1.

**Pins**: ui_tokens_brasa (test_fase_ui.cpp NOVO — os hexes exatos, a
regra do destaque ÚNICO pela família R>G>B e a escala 4/8/12/16) ·
ui_sobreposicao_topbar (o TESTE AUTOMÁTICO DE SOBREPOSIÇÃO da fase: a
barra em 5 ecrãs ≥640dp — 3 paisagens × 2 retratos — nada tapa nada nem
sai do ecrã; a banda <480dp é a degradação da parte B, o achado fica no
§8) · y1_tokens/y1_primitivas/y1_gradiente_botao recalibrados (a mesma
fonte). Recalibrações com nota: superficies_4_tons · qol_a_border_cinza ·
qol_a_inspector_ar (o fill bg lê-se 11/255) · qol_b_navegador_compacto ·
vidro_85_topline_ambar (a linha do topo é BRASA) · theme_tabela_spec_g_exata
· theme_escala_8dp_e_raios_da_spec (kSpaceMid 12 + kSpace3 16 + raio 6) ·
regress_identidade_grafite_ambar (a banda da rampa alarga para o branco
quente do dono: B−R ∈ [−8..12], teto do frio 10; o composto do painel
16.95/255; o warn distingue-se pelo B) · regress_janelas_passo4 +
regress_hotfix_defeitos (o divisor lê o border novo).

**Mutação M-UIa** (o âmbar de volta — o destaque ÚNICO violado; Theme.h +
o alias do UiContext.h para #FFB020):

```
y1_tokens ... FALHOU  test_fase_y1_tema.cpp:104  nearColor(theme::kTheme.accent, 1.0f, 90/255, 31/255, 1.0f)
  FALHOU  test_fase_y1_tema.cpp:132  countSub(src, "VV_RGB(255, 90, 31)") == 2
  FALHOU  test_fase_y1_tema.cpp:144  countSub(src, "VV_RGB(255, 176, 32)") == 0
ui_tokens_brasa ... FALHOU  test_fase_ui.cpp:83  nearC(theme::kTheme.accent, 255, 90, 31, 1.0f)
  FALHOU  test_fase_ui.cpp:98  theme::kTheme.accent[1] < 150.0f/255.0f
```

Reposto: 0 falhas (a suíte inteira verde local — o CI é o árbitro). A 1.ª
parede é o static_assert do UiContext.h (o build morre antes dos pins — a
fonte única funciona).

**Verificação**: test_core 0 falhas na corrida completa local (os flakes
da sandbox excluídos via GONI_SKIP — o CI é o árbitro) · theme_hex/ui_vocab/
projects_ui VERDES · **versionCode 111** (o §14 do RELATORIO-0.9.6.md
declara o par no MESMO commit).
