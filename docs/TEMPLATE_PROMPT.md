# TEMPLATE_PROMPT.md — a pirâmide de leitura dos prompts

Este template é a secção §0.1 que todo o prompt de campanha/sub-fase
traz: o que o GLM lê ANTES de trabalhar. O princípio é a pirâmide — o
Nível 0 é curto e lido SEMPRE; o resto abre-se só quando a tarefa o
exige (a regra de ouro está no fim). O problema que a pirâmide mata
está no §0.1 da campanha 0.11-CTX: sessões que começavam por ler mais
de 1,5 MB de documentação perdiam o foco e reabriam decisões fechadas.

## §0.1 Leitura obrigatória

Nível 0 (sempre):
- `STATE.md` — o estado do projeto, as decisões fechadas, os bloqueios.
- `CODEMAP.md` — onde vive o quê no código + as receitas de extensão.
- Este prompt.

Nível 1 (só se a tarefa o exigir):
- `ARCHITECTURE.md` — se mexer em camadas.
- `docs/REGRESSOES.md` — se adicionar sentinelas.
- `BACKLOG.md` — se fechar/criar dívida.

Nível 2 (só se o prompt o nomear):
- `docs/RELATORIO-<última>.md` — se continuação de uma sub-fase.
- Os ficheiros de código que a tarefa vai tocar (os caminhos vêm do
  CODEMAP; verificar antes com `git grep` — o CODEMAP é ÍNDICE, o
  código é a verdade).

## A regra de ouro

Não leias ficheiros inteiros. Usa o CODEMAP §4 (os padrões de
leitura): `git grep -n "símbolo"` e ler ±20 linhas; o header antes do
.cpp; seguir o call chain; procurar as tabelas (`kEntries`,
`kCommands`, InspRow). Ficheiros >1000 linhas NUNCA se leem por
inteiro — o diário por versão (`docs/DIARIO.md`) e os relatórios
arquivados (`docs/arquivo/`) são consulta, não leitura.

## O que um prompt de campanha traz além disto

- A natureza e a cláusula calma (o que NÃO se toca).
- As sub-fases na ordem de execução, com os pins e as mutações de cada
  uma.
- A regra de commit e a regra de PARAR (a campanha seguinte só arranca
  com CONTINUAR do dono).
- O estado de entrada confirmado contra o STATE.md §1 (se divergir:
  PÁRA e reporta).
