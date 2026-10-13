# RELATORIO-0.10-M-PASSO6-VALIDACAO-FINAL.md

**0.10-M PASSO 6 — A VALIDAÇÃO FINAL DA CAMPANHA .gmesh v3** — a spec do
dono: o fecho da campanha com a PROVA FINAL e a TABELA DE MEDIÇÕES. O que
a campanha prometeu (o modelo INTEIRO nunca em RAM; a parede do 65 535
morta; a abertura pela tabela; o render por blocos; as texturas e as
thumbs) corre NUMA sequência única, medida, no caminho de produção — e a
tabela sai no log do teste, no CI e no device pelas MESMAS linhas de
contrato. ZERO código de app: a validação afere o que está em pé; o
versionCode 68 mantém-se.

## 1. O âmbito (o que o PASSO 6 valida — e o que NÃO é)

O PASSO 6 não é um passo de feature: é a **prova de mesa** que fecha a
campanha 0.10-M aberta no PASSO 1 (o diagnóstico da parede do 65 535 e
do modelo inteiro em RAM). A prova automática (novo
`tests/test_010m_prova_final.cpp`) corre o caminho INTEIRO da campanha
num processo filho com o pico de RSS reiniciado a cada fase:

| Fase | O que faz | De onde vem |
|---|---|---|
| [T1] IMPORT | `convert::importFile` do dragão-38MB (o perfil do dono que MORRIA na parede), caminho FUNDIDO (o default de performance); as fases pelas LINHAS CONTRATO do engine.log | PASSO 1/3/3B |
| [T2] A ABERTURA | `BlockMesh::open` — 192 B + a faixa da tabela + a faixa dos materiais, ZERO bytes dos dados | PASSO 4 |
| [T3] O CULLING | o audit puro `visibleBlocks` (frustum × AABB por bloco): a câmara centrada vê TUDO, a lateral NADA | PASSO 4 |
| [T4] O BLOCO | `materializeBlock(0)` — o pico é o SPAN da faixa + o MeshData, NUNCA o modelo | PASSO 4/5C |
| [T5] A THUMB | `ThumbIndex::ensureThumb` (force) — o re-render sob demanda, o piso do dono | PASSO 5C |

O teste não duplica o que os SEUS passos já pinam: o render GL e a
cache LRU têm os pins do `test_blockmesh` (o pin dos 920 blocos), o SAF
tem as sentinelas R-041/R-043, o expand tem as provas do P0. A PROVA
FINAL é a costura: o caminho INTEIRO, na ordem do dono, com UMA tabela.

**O toolkit partilhado** — os perfis do dono, o gerador GLB e os
medidores de pico/marcas saíram do `test_010m_medicoes.cpp` para
`tests/Medicoes010m.h` (UMA fonte de verdade: o byte do perfil é o
MESMO entre o PASSO 1 e o PASSO 6 — os números das duas provas são
comparáveis linha a linha).

## 2. A TABELA DE MEDIÇÕES (o artefacto do PASSO 6)

Corrida isolada (`GONI_ONLY=prova_final`) no hospedeiro da sandbox,
GL-free, disco limpo — a MESMA tabela sai no log do CI (job core-tests)
a cada push, e o dono reproduz os MESMOS campos no C33 pelas MESMAS
linhas de contrato do engine.log (§4):

| Medição | Valor | Limite/promessa |
|---|---|---|
| fonte glb (perfil dragão-38MB) | 38 041 108 B (36,3 MB) | — |
| import: copia | 21 ms | linha contrato |
| import: parse | 0 ms | linha contrato |
| import: corte (pool paralelo) | 947 ms | linha contrato |
| import: assembly | 728 ms | linha contrato |
| import: verificação (bit a bit) | 991 ms | linha contrato |
| import: TOTAL | 2725 ms | — |
| **import: RAM pico** | **59 MB (base 3)** | **≤ 512 MB (a classe da R-039)** |
| .gmesh v3 | 230,5 MB · 109 blocos · verificado=1 | — |
| **abrir pela tabela** | **0,1 ms · Δ RAM 416 KB** | **≤ 2 MB — a abertura é a TABELA** |
| culling (câmara centrada) | 109/109 blocos visíveis | — |
| culling (câmara lateral) | 0/109 | o exclusivo funciona |
| **materializar bloco 0** | **7,1 ms · faixa 2 228 190 B + MeshData 2 228 190 B · Δ 0 KB** | **≤ 16 MB — ~2× o bloco, NUNCA o modelo de 230,5 MB** |
| **thumb 256×256 (force)** | **9,3 ms · PNG 256×256** | **< 500 ms (o piso do dono)** |

Os PORQUÊS dos limites (nenhum é arbitrário):

- **import ≤ 512 MB** — a promessa da R-039: a sentinela do PASSO 3
  mediu o gigante de 50M verts / 1754 MB de fonte com pico de 107 MB;
  o dragão (36 MB de fonte) está 46× abaixo da classe e mede 59 MB no
  processo isolado (base 3 MB — só o import).
- **abertura ≤ 2 MB** — o contrato do PASSO 4: 192 B de peek + 80 B
  por entrada de tabela + os nomes dos materiais. O Δ medido (416 KB)
  é a granularidade de página do mmap + os vetores; os DADOS dos
  blocos (230,5 MB) ficam intocados.
- **bloco ≤ 16 MB** — a fórmula declarada: o SPAN da faixa lida
  (2 228 190 B) + o MeshData decodificado (a estimativa GPU da casa:
  sizeof(Vertex)·verts + 2·índices = 2 228 190 B) ≈ 4,3 MB; o teto de
  16 MB cobre o pior bloco de 65 535 verts com folga. O Δ de RSS 0 KB
  é HONESTO: as páginas já pagas pelo import são REUTILIZADAS pelo
  allocator — o custo DECLARADO (faixa+MeshData) é a prova, o Δ é a
  confirmação de que nada cresce além.
- **thumb < 500 ms** — o piso do dono do PASSO 5C (o pin original mediu
  100 thumbs em 15,6 ms no hospedeiro e 0,5 ms sob SAF no c33).

**A honestidade das pools**: a fonte tem 300 001 verts e o ficheiro
declara pools=7 110 000 — a duplicação é do CORTE em blocos (cada bloco
tem a sua pool de ≤ 65 535; a soma é do ficheiro, documentada no
PASSO 3). NESTE perfil a duplicação é o PIOR CASO (os índices do gerador
são pseudo-aleatórios — localidade ZERO: cada bloco enche a pool ao
teto); os modelos REAIS do dono têm localidade espacial e a duplicação
é menor. O .gmesh maior que a fonte (230,5 vs 36,3 MB) é o MESMO
efeito — e NÃO toca a promessa de RAM (o pico é por bloco, não pelo
ficheiro).

## 3. O inventário da campanha 0.10-M (o que o PASSO 6 valida em pé)

| Passo | O que entregou | Fecho | Relatório |
|---|---|---|---|
| PASSO 1 · diagnóstico | a tabela de tetos + as medições por fase (dragão/Buddha/scene morriam; scene com +217 MB de RAM) | 0.10.0 | `RELATORIO-0.10-M-PASSO1-DIAGNOSTICO.md` |
| PASSO 2 · formato v3 | header 64-bit + blocos ≤65 535 + CRC + retrocompat v1/v2 | 0.10.1 | `RELATORIO-0.10-M-PASSO2-FORMATO.md` |
| PASSO 3 · conversor streaming | lê por primitiva/range, pool de threads, verificação bit a bit; a parede do 65 535 MORREU | 0.10.2 | `RELATORIO-0.10-M-PASSO3-CONVERSOR.md` |
| PASSO 3B · log + causa | as linhas contrato `fase=` + a causa do fail no engine.log | 0.10.3 | `RELATORIO-0.10-M-PASSO3B-LOG.md` |
| PASSO 4 · render por blocos | abre pela TABELA, frustum por bloco, lazy, LRU 64+192 MB | 0.10.4 | `RELATORIO-0.10-M-PASSO4-BLOCOS.md` |
| HOTFIX SAF-STREAM | o streaming sob content:// (mmap-fd → pread ranges) | 0.10.4c | `RELATORIO-0.10-M-HOTFIX-SAF-STREAM.md` |
| EXT · import de nós | «expandir em sub-TICs»: um TIC por nó com mesh, TRS no TIC | 0.10.5 | `RELATORIO-0.10-M-EXT-IMPORT-NOS.md` |
| HOTFIX SAF-SEAM | o runtime pela MESMA cascata + a quarentena do corrompido | 0.10.6 | `RELATORIO-0.10-M-HOTFIX-SAF-SEAM.md` |
| PASSO 5A · texturas | compressão (ASTC/ETC2) ≠ redução (3 perfis + override) | 0.10.7 | `RELATORIO-0.10-M-PASSO5A-TEXTURAS.md` |
| PASSO 5B · verificação | os pins do import de nós (city 122 nós) | 0.10.7 | `RELATORIO-0.10-M-PASSO5B-VERIFICACAO-IMPORT-NOS.md` |
| PASSO 5C · thumbnails | 256×256 por mesh no import, browser instantâneo | 0.10.7 | `RELATORIO-0.10-M-PASSO5C-THUMBS.md` |
| HOTFIX TEXTURAS-VINCULA | o bloco GTB1 (material→5 slots) + a sub-árvore REAL | 0.10.7 | `RELATORIO-0.10-M-HOTFIX-TEXTURAS-VINCULA.md` |
| HOTFIX VINCULA-2 | os 5 slots do parse + o tap nas linhas do picker | 0.10.7 | `RELATORIO-0.10-M-HOTFIX-TEXTURAS-VINCULA-2.md` |
| HOTFIX MULTIMAT | o seletor «material: i/N» + o sufixo honesto dos slots | 0.10.7 | `RELATORIO-0.10-M-HOTFIX-TEXTURAS-MULTIMAT.md` |
| HOTFIX P0 SUB-TICs | o expand em LOTES (≤32/frame) + ciclos + teto 2000 | 0.10.7 | `RELATORIO-0.10-M-HOTFIX-P0-SUBTICS.md` |
| HOTFIX SELEÇÃO-ÚNICA | uma fonte de verdade da seleção + o audit do id pendurado | 0.10.7 | `RELATORIO-0.10-M-HOTFIX-SELECAO-UNICA.md` |
| **PASSO 6 · validação final** | **a prova E2E com a tabela de medições (ESTE relatório)** | **0.10.7** | — |

## 4. O CHECKLIST DE SIGN-OFF NO C33 (a prova de mesa do dono)

O APK assinado do CI **versionCode 68** (o mesmo do hotfix
SELEÇÃO-ÚNICA) é o artefacto para o device. O dono reproduz a mesa —
cada item tem a linha de contrato que o PROVA no engine.log (o viewer
de logs está nas Definições):

1. **Os 3 modelos reais** (dragão, Happy Buddha, scene) — importar cada
   um: o overlay por fase SEM ANR; no engine.log as linhas
   `import: copia ms=` · `gmesh: fase=parse|cut|assembly|verify ms=` ·
   `gmesh: v3 blocos=… verificado=1`; ZERO morte de memória.
2. **O dragão no viewport** — o HUD `FPS · TICs · bl n/m` com o culling
   vivo (rodar a câmara: o n/m mexe; o log throttled diz o MESMO).
3. **O expand no city** — «expandir em sub-TICs» ativo: o overlay
   «A EXPANDIR… d/t» em lotes, os 72 TICs com nomes, SEM travar; o
   cancelamento a meio devolve a cena.
4. **As thumbs** — o browser com as miniaturas desde o fim do import;
   reconverter um asset re-renderiza a thumb.
5. **As texturas** — o dragão com a baseColor vinculada (a linha
   `slot baseColor <nome>` por peça no log); o inspector com os 5 slots
   e o seletor `material: i/N` nos multi-material.
6. **A SELEÇÃO-ÚNICA (o sign-off pendente do hotfix 4)** — as 4 provas
   do §6 do `RELATORIO-0.10-M-HOTFIX-SELECAO-UNICA.md`: troca de
   prim/mesh com a TIC selecionada, guarda-restauro de layout, apagar a
   TIC selecionada, abrir o catálogo num projeto novo — SEM linha âmbar
   fantasma, SEM «Nada selecionado» contraditório, ZERO linhas E de
   audio.
7. **Reabrir a cena** — a persistência dos meshes/texturas da cena
   (o serializer) com o modelo grande.

## 5. Os gates locais (todos verdes)

scope (todo o diff dentro de ci/scope.txt: `tests/`, `docs/`,
`README.md`, `BACKLOG.md` — ZERO código de app) · docs-lint ·
gmesh-docs 25 âncoras (o formato NÃO muda) · ui-vocab · theme ·
hierarchy · gizmo · projects-ui · reload · glyph · jni-parity ·
check_main · link-parity 122 TUs · release-identity versionCode 68
(inalterado — ZERO código de app, o precedente do PASSO 1/5B).

## 6. Os números finais

- **test_core: 0 falhas** (a suíte inteira com a prova nova; a sentinela
  r039 corre VERDE com o disco limpo — o flake de DISCO da sandbox
  documentado desde o hotfix P0, o CI é o árbitro)
- **c33: 797/797** (o replay do device virtual intocado — o PASSO 6 não
  mexe na UI)
- **a prova final: 8/8 pins** (import+verificado · blocos≥2 e a tabela
  concorda · RAM ≤512 · abertura ≤2 MB · culling 109/109 e 0/109 ·
  bloco ≤16 MB · thumb <500 ms · as 6 linhas contrato existem)

## 7. NÃO VERIFICADO (honesto)

- **Os números do DEVICE** — a tabela do §2 é do hospedeiro (sandbox e
  CI); o C33 tem outro hardware (GPU PowerVR GE8320, ARM8 núcleos
  pequenos): os TEMPOS por fase diferem (o corte paralelo escala com
  os núcleos), os LIMITES de RAM são os MESMOS (a promessa é por
  construção). O dono reproduz a tabela pelas linhas de contrato (§4.1)
  e o número do device fica no sign-off dele.
- **A prova de mesa física** — o checklist §4 é AÇÃO DO DONO (o APK vc
  68 do CI); nada disto é verificável na sandbox.
- **Os modelos REAIS** — os perfis são cópias de composição declarada
  (o MESMO tamanho de ficheiro do dono, índices pseudo-aleatórios); os
  ficheiros reais vivem no device e têm localidade espacial (a
  duplicação de pools é MENOR que a do perfil — o §2 diz o pior caso).
- **A MUTAÇÃO da prova** — a prova final é uma COSTURA de pins que já
  têm mutações nos SEUS passos (M1/M2/M3 do PASSO 4, TH1-TH4 do 5C, as
  mutações do hotfix 4); a costura em si não ganha mutação própria (a
  causa que ela caçaria já está caçada por cada perna).

## 8. PÁRA

**A campanha 0.10-M FECHA com o PASSO 6** — o PÁRA ABSOLUTO da spec:
diagnóstico → formato v3 → conversor streaming → log → render por
blocos → SAF → expand → texturas → thumbs → os 5 hotfixes → a prova
final com a tabela. O que segue é o plano novo do dono (FASE 1 —
hierarquia: drag-reparent, ícones por tipo, apagar/duplicar com
filhos, expandir/colapsar persistente), que começa quando o dono
escrever CONTINUAR — com o sign-off do §4 em mãos.
