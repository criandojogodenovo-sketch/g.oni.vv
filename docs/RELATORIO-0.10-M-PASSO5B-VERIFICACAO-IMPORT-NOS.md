# RELATÓRIO 0.10-M · PASSO 5B — IMPORT DE NÓS COMO SUB-ÁRVORE: a verificação dos pins (o 0.10.5 à luz da spec)

> Sub-passo 5B do PASSO 5. O import de nós como sub-árvore («expandir
> nós») JÁ FOI ENTREGUE na fase 0.10-M EXT (0.10.5, commit 3c6a7d5 — o
> BACKLOG 0.10-M-ext; entre o PASSO 4 e o PASSO 5, por ordem do dono).
> ESTE sub-passo VERIFICA os pins da spec do PASSO 5 contra o código em
> pé, fecha o buraco que a spec trouxe de novo (o city com 122 NÓS — 72
> com mesh + 50 VAZIOS) e deixa o relatório. ZERO mudanças no app
> (testes + docs só — o versionCode 62 mantém-se: nada de comportamento).

## 1. Os pins da spec, um a um (a evidência)

| Pin da spec do PASSO 5B | Estado | Prova |
|---|---|---|
| Opção «expandir nós» no import (default fundido) | ✓ (0.10.5) | o toggle nas Definições persiste `expandNodes=0/1` em settings.goni (main.cpp, loadProjectSettings); o default OFF = o fundido de sempre |
| Um TIC por nó com mesh | ✓ (0.10.5) | `convertGltfToV3Nodes` (GmeshV3Stream.cpp): um .gmesh PEÇA por nó com mesh, flag `kGmeshV3FlagPiece`; `gltfExpandInstantiate` cria os TICs (ext_gltfexpandinstantiate_um_tic_por_nó) |
| Nomes do glTF preservados | ✓ (0.10.5) | `ext_uma_peça_por_nó…`: city_n0.gmesh…city_n71.gmesh + os registros com o nome; FASE 22 do c33: «72 TICs com nomes» |
| Transformação do nó como Transform do TIC | ✓ (0.10.5) | `ext_transformação_do_nó_no_tic_e_geometria_crua`: o TRS MUNDO no TIC (o filho de pai(10,0,0) fica com (10,5,0)) e a geometria CRUA no espaço local (o AABB local prova que o mundo não vai baked — a mutação M-E2) |
| Hierarquia pai→filhos mantida | ✓ (interpretação documentada) | a hierarquia do glTF HONRA-SE na composição: o mundo do filho = pai∘filho (o mesmo teste). O EDITOR mantém a lista FLAT — o engine não resolve pais no runtime (TransformSystem flat é o desenho do 0.10.5, decisão da spec original do EXT: «o engine compõe flat — é isso que faz a peça desenhar no sítio EXATO») |
| Pin: city (122 nós, 72 meshes) expandido = 72 TICs | ✓ FECHADO NESTE PASSO | **NOVO teste `ext_city_122_nos_50_vazios_72_pecas_e_fundido_um_so`**: o city de 122 nós (72 com mesh + 50 VAZIOS intercalados — os nós de agrupamento dos city reais que as fixtures antigas não tinham) EXPANDIDO = 72 peças com nomes, os 50 nós vazios NÃO geram peça nem TIC, os índices `node` preservam a numeração do glTF (saltam os vazios sem renumerar); FUNDIDO (sem a flag) = 1 mesh só + expandNodes vazio |
| culling por TIC verde | ✓ (0.10.5) | c33 FASE 22 (o pino): o city espalhado em faixas vira 72 TICs com nomes e o frustum estreito culle 71 e deixa 1 (1 visível, 71 culled) |
| sem flag = 1 TIC (backward compat) | ✓ | o NOVO teste afirma o fundido com 122 nós = 1 mesh (`assets/city122.gmesh`) + expandNodes vazio; R-039/R-040/R-041 intocadas (o caminho fundido byte a byte) |

## 2. O que mudou neste commit (ficheiro + função)

- `tests/test_gmeshv3stream.cpp`: `NodeSpec` ganha `meshed` (default
  true — zero churn nas fixtures antigas); `buildGlb` escreve o nó VAZIO
  sem `"mesh"` (o agrupador real dos city); o TESTE novo
  `ext_city_122_nos_50_vazios_72_pecas_e_fundido_um_so` (o pin do dono:
  72 peças + o fundido de 1).
- ZERO mudanças em `app/src/main/cpp` (a verificação não toca no app).

## 3. Números medidos

- A suíte do core: **926 TESTs, 0 falhas** (o 925 do 5A + o teste novo).
- O city de 122 nós: EXPANDIDO = 72 ficheiros peça (`assets/city122_n<i>.gmesh`)
  + 72 registros de TIC; FUNDIDO = 1 ficheiro (`assets/city122.gmesh`).
- O c33 FASE 22 (o pino do culling, já no CI): 72 TICs, 71 culled, 1
  visível.

## 4. Gates e CI

Locais: docs-lint VERDE · scope VERDE (tests/ + docs/ estão no scope) ·
suíte 926/0. A linha do CI verde entra no commit -docs.

## 5. NÃO VERIFICADO (honestidade da casa)

- **Device real (C33)**: o import expandido com um city REAL de 122 nós
  do dono (as fixtures são sintéticas) — o comportamento do toggle no
  device segue o sign-off do 0.10.5; o caso dos 50 nós vazios no device
  não foi visto (o c33 do CI corre as fixtures sintéticas).
- **A hierarquia no INSPECTOR**: a spec «hierarquia pai→filhos mantida»
  foi lida como composição de transformações (a leitura do 0.10.5); se
  o dono quiser os TICs ANINHADOS na hierarquia do editor (pai/filho no
  painel), é uma decisão nova (o engine é flat por desenho).
