# G.One VV — a engine de jogo que se edita no telemóvel

O G.One VV é um game engine Android (C++/Java, GLES 3) cujo código
nasce num ecrã de 6,5": todo o C++/Java/GLSL deste repo é escrito e
revisto no telemóvel do dono. Este README é um ÍNDICE curto; o
conteúdo longo por versão (as leis do fluxo, os escopos, os
checklists do device) vive em `docs/DIARIO.md`.

## Como isto é construído (as leis do fluxo)

- **O código é editado num telemóvel.** Ficheiros pequenos, nomes
  completos, comentários que documentam porquê, e uma suíte de testes
  que substitui a vigilância que um IDE daria de graça.
- **O CI é o único compilador.** Nada é compilado localmente: cada
  push corre o workflow `release.yml` no GitHub Actions. Um commit só
  conta como fechado quando o CI está VERDE em todos os jobs.
- **O device real é a vara de medir.** O alvo primário é um Realme C33
  (RMX3624, Android 13, GPU PowerVR GE8320, 60 fps como meta). O que o
  dono vê no device manda; o que os testes afirmam tem de bater certo
  com o device ou é bug.
- **As regressões são contratos.** Cada bug de peso ganha uma sentinela
  `R-NNN` em `docs/REGRESSOES.md` com prova de mutação colada (quebrar
  o fix → vermelho → repor → verde). O `c33_virtual`
  (tests/c33_virtual.cpp) repete no Linux o caminho de UI/events do
  device (FASEs numeradas).
- **O que não está verificado está dito.** Os relatórios por sub-fase
  têm uma secção de NÃO VERIFICADO explícita; o gate docs-lint (R-016)
  proíbe placeholders e linguagem de achismo na documentação.

## Onde está o quê (o índice de leitura)

| Quero... | Vai a |
|---|---|
| o estado do projeto, decisões fechadas, bloqueios | `STATE.md` (ler primeiro) |
| onde vive o quê no código + receitas de extensão | `CODEMAP.md` |
| o porquê das camadas | `ARCHITECTURE.md` |
| a referência da linguagem V.ONI | `VONI_referencia.md` (gerada do registo) |
| as sentinelas de regressão R-NNN | `docs/REGRESSOES.md` (consulta) |
| o rastreador de campanhas | `BACKLOG.md` |
| o diário por versão (o README longo de sempre) | `docs/DIARIO.md` |
| os relatórios das campanhas fechadas | `docs/arquivo/` |
| a pirâmide de leitura dos prompts | `docs/TEMPLATE_PROMPT.md` |

A convenção das referências: um relatório de campanha FECHADA vive em
`docs/arquivo/` (movido com `git mv` — o histórico do git preserva a
proveniência); os relatórios da campanha em curso vivem em `docs/`.

## A última campanha — 0.12-U (V.ONI Bloco A, a redução de trabalho) — FECHADA

O autor da V.ONI ganhou as 7 features que tiram trabalho: o `View P`
universal (qualquer dos 7 tipos no log), as 14 FUNÇÕES do registo
(abs/min/max/clamp/lerp/floor/ceil/round/sqrt/sin/cos/random/random_int/
distance — sin/cos em GRAUS), o `spawn(tipo, x, y, z) → TIC` (o script
CRIA na cena pela porta do Host — active/visible/nome único) e os
relógios `every(s)/after(s)/when(cond)` (o estado por nó AST morre no
stop). O juiz final foi o PLATFORMER: um script único de 25 linhas com
as 7 features em cadeia corre verde no CI. A referência regenerada está
em `VONI_referencia.md` (64 entradas) e a história em
`docs/RELATORIO-0.12-U-*.md` (ABERTURA + 3 sub-fases + FECHO). A 0.11-W
(Render Systems, vc 76) continua com o pin do dono no C33 real
PENDENTE — o fecho corre por ordem do dono.

## Build

- O CI é o único compilador: o workflow `release.yml` corre em cada
  push os jobs — testes do core no Linux, o dispositivo virtual C33
  (replay + sentinelas + gates), a keystore, o APK release arm64
  assinado e o gate de símbolos de entrada.
- O APK que o dono instala no device é SEMPRE o artefacto assinado do
  CI (`goni-vv-*-release-signed`), nunca um build local.
- O versionCode vive em `app/build.gradle` (hoje 100) e fecha o par com
  o relatório de cada sub-fase.
- Os gates de documentação correm em cada push: o docs-lint (R-016) e
  o gate STATE.md (0.11-CTX §1.2).

## Verificação no Realme C33 (dono)

Os checklists «Verificação no Realme C33» por versão estão no diário
(`docs/DIARIO.md`), intactos. O protocolo continua: instalar o APK
assinado do CI, correr os itens, preencher pass/fail e colar o PNG do
HUD quando o pin o pedir.
