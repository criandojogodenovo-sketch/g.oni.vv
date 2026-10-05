# BACKLOG — campanha FASE 0.9.6-MASTER «VER, TESTAR, CORRIGIR, NÃO PARTIR»

> O rastreador da campanha: nove grupos (A-I), paragem e relatório ao dono
> entre eles. O estado de cada grupo vive AQUI (a P-01 lê este ficheiro no
> arranque de cada grupo; a P-03 fecha-o no fim). Regras da casa:
> `docs/REGRESSOES.md` (sentinelas R-NNN), `docs/RELATORIO-*.md` (fechos),
> worklog partilhado. Nada declarado «feito» sem evidência (P-05/P-06).

## Estado (atualizado ao fecho de cada grupo)

| Grupo | Âmbito | Estado | Commit |
|---|---|---|---|
| A · IMPORT glTF/GLB REAL | irmãos do .gltf; layout/integridade do GLB; textura falha não mata; browser isFile; R-014 reescrito; sentinelas R-021/R-022/R-023 | **FECHO 0.9.6.4** (ver README) | (este commit) |
| B · FERRAMENTAS DE VERIFICAÇÃO | Exportar layout (PNG+JSON); validador; Auditoria; relatório do estado ATUAL com PNGs lidos | por fazer | — |
| C · ESCALA E TIPOGRAFIA | dp()/sp() únicas; linha→y/col→x única no editor; perf do editor; cantos suavizados | por fazer | — |
| D · ORÇAMENTO DO EDITOR 3D | topo/abas/FPS; hierarquia|viewport|inspector com divisores arrastáveis; barra de toque; scissor | por fazer | — |
| E · EDITOR DE SCRIPT + SÍMBOLOS | header flexível; IME; barra de símbolos 40dp sobre o IME (teclado da engine REMOVIDO); R-018/R-010 | por fazer | — |
| F · IDENTIDADE | tokens mono+vidro (R-020 de tema); ícone G com 4 setas | por fazer | — |
| G · ANIMATION | auditoria exaustiva do drawer/workspace; zero funcionalidade nova | por fazer | — |
| H · FICHEIROS COM ASSETS REAIS | drawer enumera source/+assets/ reais; refresh; R-019 | por fazer | — |
| I · BENCHMARKS | bench de 60s com JSON/loja/copiar; vsync; 3×; cena de benchmark | por fazer | — |

## Dívidas registadas durante a campanha (fora do scope do grupo em curso)

- (nenhuma no Grupo A — os achados fora do scope viraram R-021/R-022/R-023
  porque ERAM do scope do import/browser; a lista cresce à medida que os
  grupos avançarem)
