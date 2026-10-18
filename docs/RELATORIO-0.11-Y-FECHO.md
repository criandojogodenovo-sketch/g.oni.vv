# RELATORIO-0.11-Y-FECHO.md — o fecho da campanha (a identidade visual G.One VV)

Campanha: 0.11-Y · Fecho · vc 81 (o build vigente — o fecho não bumpa) ·
Data: 2026-10-17 · Commit: este

## §1 A campanha fechada (as 5 sub-fases + o fix)

| Sub-fase | O que entregou | vc | Commit | CI |
|---|---|---|---|---|
| Y-0 | o ícone «G em chama» no launcher (adaptativo + legacy, 15 PNGs) | 77 | a2d2248 | VERDE |
| Y-0-FIX | a escala do G (o pin C33 apanhou ~25 %; o sólido a 65 % da zona segura) | 78 | 25c1348 | VERDE |
| Y-1 | O TEMA: o poço #08080A + a chama (accent2 #FF6B00 / glow #FF3D00) + o gradiente dos ativos + as primitivas do cristal | 79 | 3a6d934 | VERDE (o run da Y-2 fecha; o CI apanhou o probe 18.2 — fix no 6b1427f) |
| Y-2 | O LAYOUT: as regras CruzaTabBar/PainelFora + a varredura FASE 31 (13 ecrãs) + 3 bugs reais curados | 80 | 6b1427f | VERDE (run 38952999467) |
| Y-3 | OS CARTÕES: o cristal (a linha do topo + os divisores com o âmbar a cair) | 81 | 011791e | VERDE (run 38954188103) |
| Fecho | os 13 PNGs por ecrã no docs/ + o pin y_fecho + este selo | 81 | este | o run deste commit |

## §2 Os critérios C33 do dono — a prova (uma a uma)

1. **O ícone correto (o G a ~65 % do canvas)** — o Y-0-FIX (o sólido a
   65 % da zona segura de 66dp; o pin y0_escala mede os PNG reais via
   stb_image). A percepção final é o pin C33 do dono (instalar o APK e
   ver).
2. **O tema escuro + âmbar quente + vidro com brilho** — a Y-1 (o poço
   #08080A, o vidro #151517/#1E1E21 com os mesmos alfas, a hairline
   âmbar, os tokens accent2/glow NOVOS; o botão ATIVO é a chama) + a
   Y-3 (a linha do topo 1dp do glow a 20 % nos painéis/drawer/cards).
3. **Nenhum ecrã com overflow; nada a cruzar a tab bar** — a Y-2 (as
   regras CruzaTabBar/PainelFora no validador + a FASE 31 a varrer os
   13 ecrãs pelo frame de produção; os 3 bugs reais curados: o toast
   dp(), o back do Settings 36×36, os rótulos da régua).
4. **Os painéis de cristal (borda brilhante, separadores com
   gradiente)** — a Y-3 (a linha do topo + o dividerGradientV
   α0.90→0.15; a linha sólida D11 superada pela ordem expressa; a pill
   do drag fica).
5. **O PNG de cada ecrã → docs/y-*.png** — ESTE fecho: os 13 PNGs da
   varredura (1600×720, o export de produção do Grupo B) copiados para
   docs/ + o pin y_fecho (a existência + a assinatura + as dimensões +
   conteúdo; M-Fecho: um PNG removido → VERMELHO → reposto → VERDE).

## §3 Os 13 PNGs (docs/y-*.png)

y-editor3d · y-drawer-ficheiros · y-drawer-assets · y-drawer-consola ·
y-drawer-animacao · y-modo-ui · y-modo-audio · y-settings · y-docs ·
y-script · y-storage · y-settings+drawer · y-drawer-max-400 — todos
1600×720 (a instância das FASEs finais do device virtual), o export de
produção (o MESMO caminho do botão Diagnóstico/Exportar do dono).

## §4 O que a campanha NÃO fez (os limites declarados)

1. **A tela de projetos (Java)** mantém o espelho de tokens próprio —
   a harmonização Java não está no prompt da campanha (o mesmo limite
   declarado na Y-1/Y-3).
2. **Os thumbs/impostores** mantêm o bg próprio (asset de preview, não
   chrome).
3. **O pin C33 do dono** (instalar o APK vigente e confirmar a
   percepção — o ícone, o cristal, o brilho) é a ação final HUMANA;
   os PNGs do dono → docs/y{0,1,2,3}-c33.png quando houver.

## §5 A dívida e o estado pós-campanha

- O BLOCO B (o fecho da 0.11-W) continua à espera do CONTINUAR do dono
  (STATE.md §3).
- A corrida latente do corte paralelo (§7) continua suspensa — o A/B
  da Y-1 voltou a provar que o crash local existe no HEAD sem as
  mudanças da campanha (ambiente da sandbox, o CI é o árbitro).
- O gate de identidade PERMANECE: os pins y0_* (×8) / y1_* (×5) /
  y2_* (×3) / y3_* (×3) / y_fecho (×1) = 20 pins novos na suíte, a
  FASE 31 no replay de TODOS os runs futuros.
