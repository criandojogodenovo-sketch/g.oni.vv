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

## A campanha em curso — 0.11-CTX (contexto enxuto)

A 0.11-CTX é uma campanha SÓ de documentação: a leitura de arranque
das sessões passou de ~1,6 MB (o README integral + REGRESSOES + os
relatórios todos) para dois índices curtos — `STATE.md` +
`CODEMAP.md` — com o arquivo do histórico (`docs/arquivo/`), o diário
(`docs/DIARIO.md`), o template de prompts e o gate STATE.md no CI (o
commit de sub-fase com código viaja COM o STATE.md). A medição real da
redução está em `docs/RELATORIO-0.11-CTX.md`.

Em paralelo, a **0.11-W (Render Systems)** está implementada com o CI
verde no vc 76 — instancing de blocos (3.5b), streaming progressivo do
import (3.75a) e impostores PNG alfa por distância (3.75b). Falta o
pin do dono no Realme C33 real: instalar o APK assinado do CI,
importar a cena de 203 MB, correr o bench R-017 e colar o PNG do HUD
(baseline FPS 5 · bl 5164/5178 · 6 TICs; alvo 30+ FPS sem crash nem
freeze no import). O fecho da 0.11-W corre depois da CTX, por ordem do
dono.

## Build

- O CI é o único compilador: o workflow `release.yml` corre em cada
  push os jobs — testes do core no Linux, o dispositivo virtual C33
  (replay + sentinelas + gates), a keystore, o APK release arm64
  assinado e o gate de símbolos de entrada.
- O APK que o dono instala no device é SEMPRE o artefacto assinado do
  CI (`goni-vv-*-release-signed`), nunca um build local.
- O versionCode vive em `app/build.gradle` (hoje 76) e fecha o par com
  o relatório de cada sub-fase.
- Os gates de documentação correm em cada push: o docs-lint (R-016) e
  o gate STATE.md (0.11-CTX §1.2).

## Verificação no Realme C33 (dono)

Os checklists «Verificação no Realme C33» por versão estão no diário
(`docs/DIARIO.md`), intactos. O protocolo continua: instalar o APK
assinado do CI, correr os itens, preencher pass/fail e colar o PNG do
HUD quando o pin o pedir.
