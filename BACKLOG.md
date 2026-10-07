# BACKLOG — campanha FASE 0.9.6-MASTER «VER, TESTAR, CORRIGIR, NÃO PARTIR»

> O rastreador da campanha: nove grupos (A-I), paragem e relatório ao dono
> entre eles. O estado de cada grupo vive AQUI (a P-01 lê este ficheiro no
> arranque de cada grupo; a P-03 fecha-o no fim). Regras da casa:
> `docs/REGRESSOES.md` (sentinelas R-NNN), `docs/RELATORIO-*.md` (fechos),
> worklog partilhado. Nada declarado «feito» sem evidência (P-05/P-06).

## Estado (atualizado ao fecho de cada grupo)

| Grupo | Âmbito | Estado | Commit |
|---|---|---|---|
| A · IMPORT glTF/GLB REAL | irmãos do .gltf; layout/integridade do GLB; textura falha não mata; browser isFile; R-014 reescrito; sentinelas R-021/R-022/R-023 | **FECHO 0.9.6.4** (ver README) | 1d89fa0 |
| B · FERRAMENTAS DE VERIFICAÇÃO | Exportar layout (PNG+JSON); validador; Auditoria; relatório do estado ATUAL com PNGs lidos | **FECHO 0.9.6.5** (ver README + RELATORIO-0.9.6.5-GRUPO-B) | 74a3860 |
| C · ESCALA E TIPOGRAFIA | dp()/sp() únicas; linha→y/col→x única no editor; perf do editor; cantos suavizados | **FECHO 0.9.6.6** (ver README + RELATORIO-0.9.6.6-GRUPO-C) | 7c2cf7b |
| D · ORÇAMENTO DO EDITOR 3D | topo/abas/FPS; hierarquia|viewport|inspector com divisores arrastáveis; barra de toque; scissor | **FECHO 0.9.6.7** (ver README + RELATORIO-0.9.6.7-GRUPO-D) | ad3aae9 |
| E · EDITOR DE SCRIPT + SÍMBOLOS | header flexível; IME; barra de símbolos 40dp sobre o IME (teclado da engine REMOVIDO); R-018/R-010 | **FECHO 0.9.6.8** (ver README + RELATORIO-0.9.6.8-GRUPO-E) | 1062b6d |
| F · IDENTIDADE | tokens mono+vidro (R-020 de tema); ícone G com 4 setas | **FECHO 0.9.6.9** (ver README + RELATORIO-0.9.6.9-GRUPO-F) | f3bbbca |
| UI · A REESCRITA DA APRESENTAÇÃO | spec G grafite+âmbar+vidro; a imagem 1 por regiões; E5+E1; R-029/R-030; painel-mãe | **FECHO 0.9.6.10** (ver README + RELATORIO-0.9.6.10-GRUPO-UI) | a50ff89 |
| G · ANIMATION | auditoria exaustiva do drawer/workspace; zero funcionalidade nova | **FECHO 0.9.6.11** (ver REGRESSOES R-031 + a secção G do RELATORIO-0.9.6.10) | ver commit |
| J · ARQUITETURA DO EDITOR (P-08) | o contrato da hierarquia; os 5 defeitos (toolbar/top bar, chips medidos, log do rect, rodapé); R-022..R-025 + gate hierarchy-check | **FECHO 0.9.6.12** (ver RELATORIO-0.9.6.12) | ver commit |
| IMPORT A2 · OS TRÊS FICHEIROS REAIS | o log com os 4 números; o bound corrigido (accOff); a degradação de primitiva; copiado==total; R-014 reescrita | **FECHO 0.9.6.12** (ver RELATORIO-0.9.6.12; sign-off do dono PENDENTE) | ver commit |
| IMPORT A2-2 · OS TETOS DE RANGE (R-032) | o teto único de 256 MB (era 64 MB no parser + 16 MB escondido no loader — o scene 118 MB e o dragão morriam); «modelo demasiado grande para a memória»; off+len ≤ real E ≤ declared; file= na linha do dono + log de 2048; staging único sem duplicar; .bin irmão DEFERIDO | **FECHO 0.9.6.12g** (ver REGRESSOES R-032; as provas de mutação 1 e 2) | ver commit |
| UI PASSO 0 · INVENTÁRIO (spec de layout) | todos os controlos com nome/função/posição; veredito dos suspeitos (chips sem função, logo marca, «prooksnsn» é dado); TABELA DE MEDIDAS baseline pelo código real — device: viewport 37,1%, cobertura 88,5%, chrome 38,1% (falha a/b/c); zero mudanças de código | **FECHO 0.9.6.13 PASSO 0** (ver RELATORIO-0.9.6.13-PASSO0-INVENTARIO) | ver commit |
| UI PASSO 1 · TAMANHOS (spec de layout) | a LEI DE OURO (desenho 32/toque 40; barra 36, cabeçalhos 28, linhas 36, campos 32, tabs 32 + FPS·TICs, status REMOVIDA, git no Sobre); o validador com pisos por classe 40/36/32/28; device chrome 38,1%→20,2% | **FEITO 0.9.6.14** (ver RELATORIO-0.9.6.14-PASSO1-TAMANHOS) | ver commit |
| UI PASSO 2 · PAINÉIS | hierarquia 18% (min 140); inspector 22% (min 180/max 260) com trilho 32dp quando sem seleção; drawer default fechado, máx 35%; pegas 24dp; chips 28dp sem o duplicado; «Erros» mostra erros; viewport 37,1%→58,8% | **FEITO 0.9.6.15** (ver RELATORIO-0.9.6.15-PASSO2-PAINEIS) | ver commit |
| UI PASSO 2-BIS · AS 2 DECISÕES DO DONO | fixar aberto: o ícone do trilho ABRE E FIXA (inspPinned persiste no layout.json; a seta de recolher desfaz e fecha — sem long-press); consola ≥60%: a lista de log ocupa ≥60% do conteúdo do drawer (chips+extras ≤40% — o campo de comando cede no drawer pequeno; sem exceções) | **FEITO 0.9.6.16** (R-034 + c33 13.7i; LAYOUT_HIERARCHY §1/§2.4/§2.10) | ver commit |
| UI PASSO 3 · VIEWPORT | controlos ≤10% da área a 60% alfa: rail esquerdo, desfazer/refazer/guardar/⋯ topo-esq, [+] 40dp redondo, gizmo 40dp topo-dir, legenda do tool; nada full-width sobre a cena | por fazer | — |
| UI PASSO 4 · JANELAS | logs opacos com corte/scroll/wrap (80% do ecrã); menus ancorados com fundo 40%; settings linhas 36dp, secundários só-contorno, «concedido» como texto; cantos suaves | por fazer | — |
| H · FICHEIROS COM ASSETS REAIS | drawer enumera source/+assets/ reais; refresh; R-019 | por fazer | — |
| I · BENCHMARKS | bench de 60s com JSON/loja/copiar; vsync; 3×; cena de benchmark | por fazer | — |

## Dívidas registadas durante a campanha (fora do scope do grupo em curso)

- (nenhuma no Grupo B — os dois bugs apanhados ao vivo ERAM do scope: os
  botões novos do Diagnóstico no walk do tap e o falso positivo do clip;
  o 1 ERRO do editor [label que sangra 3px] e os avisos de toque < 48dp
  são a LINHA DE BASE dos Grupos C-I, não dívida do B — o relatório
  do estado atual lista-os com os números medidos)
