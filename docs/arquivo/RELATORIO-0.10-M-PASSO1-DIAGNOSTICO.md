# RELATÓRIO 0.10-M · PASSO 1 — DIAGNÓSTICO E DOCUMENTO (sem alterar código)

> FASE 0.10-M (a spec do dono): .gmesh v3 (blocos, sem teto) + conversor
> streaming + texturas. Cláusulas P-01..P-08 permanentes; scope fechado à
> pipeline de assets. RETROCOMPATIBILIDADE: os .gmesh v1 e v2 existentes
> continuam a abrir (leitor); o escritor só escreve v3. EXECUÇÃO: passos
> 1-3; **PÁRO ABSOLUTO antes do PASSO 4**. Commit próprio + CI verde +
> relatório em cada passo.
>
> **Este passo (PASSO 1): ZERO código de app alterado.** Os entregáveis
> são os dois docs de formato (fonte de verdade), o gate que os vigia, a
> tabela de tetos (§4) e as MEDIÇÕES reais (§5). A única alteração de
> build é o registo do teste de medição no CMake (`tests/`) e o passo do
> gate no CI — ambos fora do `app/`.

## 1. O que foi feito

- **1a) DOCS FONTE DE VERDADE**: `docs/GMESH_formato.md` (o formato
  .gmesh v1: o header comum de 32 bytes partilhado pelos 3 formatos
  próprios, o payload campo a campo, a quantização 16-bit — a perda de
  qualidade do v1, os tetos, onde vive no pipeline) e
  `docs/GTEX_formato.md` (o .gtext persistido, o cache .gtc, os 5
  `CompressedFormat`, os compressores, o gate 256 px/4K, a extração
  glTF com dedup, os tetos de textura). Qualquer pessoa ou IA lê estes
  dois ficheiros e sabe EXATAMENTE o que os bytes significam.
- **1a) O GATE**: `scripts/gmesh_docs_check.py` (job core-tests do CI,
  a seguir ao docs-lint) — afere que (1) os dois docs existem, (2) cada
  símbolo citado entre backticks EXISTE no fonte C++ (a regra do
  hierarchy-check aplicada aos formatos: o contrato não documenta
  fantasmas), (3) as âncoras numéricas/byte batem com o código
  (`kGHeaderBytes=32`, `0x1A2B`, `65535`, os offset/primo FNV-1a, os
  tetos do .gtext, `kChunkBytes`/`kMaxJsonBytes`/`kMaxImageBytes`, o
  gate 256 px) — 14 âncoras. Mudar o formato sem os docs (ou os docs
  sem o formato) = CI VERMELHO.
- **1b) A TABELA DE TETOS** — §4 abaixo: todos os tetos e suposições de
  16/32 bits no importador, conversor, leitor, render, picking,
  colisores, export OBJ e serialização.
- **1c) AS MEDIÇÕES** — §5 abaixo: tempos POR FASE e RAM de pico a
  importar o dragão, o Happy Buddha e o scene, no CAMINHO DE PRODUÇÃO
  (`convert::importFile` + `FsStorage` real). Medido, não estimado.

## 2. O que NÃO foi feito (por spec)

- Nenhuma alteração no `app/src/main/cpp` (o PASSO 1 é diagnóstico).
- Nenhuma decisão de design do v3 implementada (a spec do v3 entra no
  PASSO 2; este relatório apenas diagnostica o estado).

## 3. O diagnóstico em duas frases

O teto de hoje é UMA só decisão de 2023 — «índices u16, mesh único,
quantizado» — replicada em 4 guards (§4.1-4.4) e 1 tipo (`MeshData::
indices` u16, upload `GL_UNSIGNED_SHORT`). Os três modelos do dono morrem
todos nessa parede: o dragão e o Buddha DENTRO do parse (fusão de
primitivas), o scene DEPOIS do parse (a fusão dos nós) — e o scene
demonstra o 2.º problema: **o modelo inteiro materializa-se em RAM
(+217 MB medidos) antes de morrer**. O dragão-fit (65,535 verts, o
maior que cabe) completa o caminho inteiro em 29 ms — a pipeline é
rápida; o teto é que é cedo demais.

## 4. A TABELA DE TETOS (PASSO 1b)

### 4.1 Os guards do teto de vértices (a parede)

| # | Ficheiro:função | Teto | O que parte acima dele |
|---|---|---|---|
| 1 | `assets/GltfImporter.cpp` (fusão da primitiva, ~linha 937) | `md.vertices.size() + pos.size() > 65536` | QUALQUER mesh glTF com mais de 65,536 vértices — o dragão (300,001) e o Buddha (543,652) morrem AQUI («glTF: mesh fundido excede 65535 vértices») |
| 2 | `assets/AssetConverter.cpp` (fusão dos nós, ~linha 781) | `merged.vertices.size() + src.vertices.size() > 65535` | a SOMA dos meshes do modelo — o scene (5,242,880 verts em 80 meshes; cada um ≤65,536) passa o guard 1 e morre AQUI («glTF: o modelo fundido excede 65535 vértices (limite u16 do .gmesh)») |
| 3 | `assets/GOwnFormats.cpp` `writeGMesh` | `m.vertices.size() > 65535` | o formato v1 em si: índices `u16` — o escritor recusa («mesh com N vértices — o limite do engine é 65535 (índices u16)») |
| 4 | `assets/GOwnFormats.cpp` `readGMesh` | `nVerts > 65535` | o leitor recusa o mesmo teto (o formato não tem como representar mais) |
| 5 | `assets/ObjImporter.cpp` (~linha 251) | `vertices_.size() >= 65536` | OBJ com >65,536 vértices ÚNICOS («mesh excede 65535 vertices (limite u16 do engine)») |
| 6 | `assets/Assets.h` `MeshData::indices` | `std::vector<u16>` | o TIPO do runtime — tudo o que consome MeshData herda o teto |
| 7 | `render/Mesh.cpp` `Mesh::create`/`createSkinned` | `GL_UNSIGNED_SHORT` (glDrawElements) | a GPU desenha índices u16 — >65,535 verts por draw é impossível no runtime atual (o PASSO 4 resolve por blocos) |

### 4.2 Os tetos de tamanho/RAM (a 2.ª parede)

| # | Ficheiro:função | Teto | O que parte acima dele |
|---|---|---|---|
| 8 | `assets/GltfImporter.cpp` `resolveView` | `kMaxRangeBytes = 256 MB` (A2-2) | um ÚNICO accessor >256 MB (ex.: POSITION de 22M verts = 264 MB) → «modelo demasiado grande para a memória» — honesto, mas teto |
| 9 | `assets/AssetConverter.h` | `kMaxJsonBytes = 16 MB` | um glTF com JSON maior (centenas de milhares de primitivas) recusa |
| 10 | `assets/ResourceManager.cpp` (branch gmesh) | `storage_->readBytes` do ficheiro INTEIRO | a carga runtime NÃO é streaming: um .gmesh de 1 GB = 1 GB de RAM no load (o PASSO 3 muda para mmap/por blocos) |
| 11 | `assets/AssetConverter.cpp` (cópia/verificação) | chunks de `kChunkBytes = 6 MB` | sem teto de ficheiro (streaming) — OK hoje, mantém-se |
| 12 | `assets/PngLoader.cpp` + `kMaxImageBytes = 64 MB` | PNG em RAM para decode | foto de 64 MB+ recusa com erro legível |

### 4.3 As suposições e fronteiras latentes (achados do diagnóstico)

| # | Onde | A suposição | Risco |
|---|---|---|---|
| 13 | `GltfImporter.cpp` (guard 1) vs `AssetConverter.cpp` (guard 2) | o guard 1 aceita um mesh de EXATAMENTE 65,536 verts (`> 65536`), mas os guards 2/3/4 rejeitam acima de 65,535 | um mesh de 65,536 verts morre com a mensagem do guard 2 — fronteira INCONSISTENTE e mensagem confusa (diagnóstico: 1ª fonte de «bug estranho do limite») |
| 14 | `AssetConverter.cpp` (fusão, `static_cast<u16>(base + idx)`) | índices globais cabem em u16 | a truncagem é LATENTE — o guard 2 dispara ANTES (defesa em profundidade por acidente: o cast nunca executa com `base+idx > 65535`; o v3 elimina a classe do problema) |
| 15 | `assets/GOwnFormats.cpp` `Writer::str_` | nome de grupo/material ≤ 65,535 chars (u16 len) | truncado silenciosamente — cosmético |
| 16 | `assets/GOwnFormats.cpp` (payload .gmesh) | `firstIndex`/`indexCount` `u32` por grupo | 4.29 G índices por grupo — sem teto prático |
| 17 | `core/Types.h` + `convert::Stats` | contagens `u32` (verts/indices) | 4.29 G — sem teto prático para 1 GB+ |
| 18 | offsets de ficheiro | `size_t`/`u64` em arm64/Linux | de facto 64-bit já; o PASSO 2 formaliza (`off64_t`, mmap) — NADA a partir hoje |

### 4.4 Picking, colisores, export OBJ, serialização

| Consumidor | Como consome | Teto herdado |
|---|---|---|
| Picking (seleção de TICs) | AABB do mesh (`core/SceneBounds.h`, `Mesh::boundsMin/Max` — dados calculados no `create`) | NENHUM próprio: não lê índices; herda o teto só porque o mesh nunca chega (guard 1/2) |
| Colisores (física) | shapes primitivas (`physics/Shapes.h`) sobre o Transform3D | NENHUM próprio — não toca MeshData |
| Export OBJ (`assets/ObjExporter.cpp`) | escreve `v/vt/vn` 1:1 de MeshData (u16 herdados) | o teto do MeshData (6); o export em si é 32-bit limpo (texto %.9g) |
| Serialização (.goni) | refs de mesh como STRINGS (`core/SceneSerializer.cpp`) + `AssetPersist` escreve o cubo via `exportObj` | NENHUM próprio — os bytes .gmesh nunca entram no .goni |

## 5. AS MEDIÇÕES (PASSO 1c) — tempos por fase + RAM de pico

**Como foi medido** (`tests/test_010m_medicoes.cpp`, novo, no CI a
partir deste commit): cada perfil corre num PROCESSO FILHO (fork) com o
pico de RSS REINICIADO (`/proc/self/clear_refs` "5" — o kernel zera o
VmHWM), gera o GLB em STREAMING (chunk de 6 MB — o gerador nunca tem o
modelo em RAM), e importa pelo caminho DE PRODUÇÃO (`convert::
importFile` + `FsStorage` real sobre ficheiros). As fases saem dos
MARCADORES de log com timestamps do `engine.log` («import: fonte
copiada», «parse ok», «limites finais», «registado na lista»). O pico
de RAM é o delta VmHWM (o `readPeakRssKb` do Bench, após reset).

**Os perfis**: os ficheiros REAIS do dono vivem no C33 (o dragão
38,051,884 B e o scene 212 MB são os tamanhos dos logs A2-2; o Buddha é
a composição clássica 543,652 verts / 10.7M tris). As cópias de perfil
têm a MESMA ESCALA com composição DECLARADA:

| Perfil | Bytes gerados | Composição (declaração) |
|---|---|---|
| dragão-38MB | 38,041,108 | 1 mesh / 1 primitiva · 300,001 verts · 2,370,000 tris · índices u32 |
| buddha-classico | 81,424,716 | 1 mesh / 12 primitivas (u16/prim) · 543,652 verts · 10,670,000 tris · índices u16 |
| scene-213MB | 227,827,036 | 80 meshes (65,536 verts cada) · 5,242,880 verts · 10,000,000 tris · índices u16 · 3 materiais |
| dragao-fit | 2,878,184 | 1 mesh · 65,535 verts · 130,000 tris — o MAIOR que cabe hoje (completa) |

**Ambiente da corrida citada**: sandbox de CI local (Linux 5.10,
2 núcleos, /tmp em tmpfs — por isso «cópia=0 ms»; no CI do GitHub os
tempos de cópia serão não-zero, a proporção mantém-se). O CI corre
este teste EM CADA PUSH (job core) — os números da corrida oficial
ficam no log do CI (a evidência é reproduzível).

| Perfil | cópia | parse JSON + primitivas | fusão dos nós | escrita .gmesh | TOTAL | RAM pico (Δ VmHWM) | Veredito |
|---|---|---|---|---|---|---|---|
| dragão-38MB | 0 ms | **MORRE AQUI** («mesh fundido excede 65535») | — | — | 25 ms | **+7 MB** | FALHA NO TETO (guard 1) |
| buddha-classico | 0 ms | **MORRE AQUI** («mesh fundido excede 65535») | — | — | 63 ms | **+19 MB** | FALHA NO TETO (guard 1) |
| scene-213MB | 0 ms | 311 ms | **MORRE AQUI** («o modelo fundido excede 65535») | — | 449 ms | **+217 MB** | FALHA NO TETO (guard 2) — e o modelo INTEIRO esteve em RAM |
| dragao-fit (65,535 verts) | 0 ms | 4 ms | 4 ms | 13 ms | **28 ms** | **+7 MB** | OK — 1 .gmesh, 65,535 verts, 390,000 índices |

Leituras honestas da tabela:

1. **A parede é o teto u16, não a velocidade**: o dragão-fit converte
   2.9 MB em 28 ms — a pipeline escalada linearmente daria ~370 ms para
   38 MB se o teto não existisse.
2. **A RAM do scene (+217 MB) é a prova do problema de fundo**: a fusão
   materializa o MODELO INTEIRO (5.24M verts × 32 B + 10M tris × 6 B de
   índices + o GltfModel) antes de o rejeitar. O PASSO 3 (streaming por
   primitiva + blocos escritos à medida) mata este pico — o orçamento
   declarado é «um bloco por worker».
3. **O dragão e o Buddha morrem ANTES de materializar** (os +7/+19 MB
   são o accessor corrente + o JSON): os guards 1 disparam cedo — a
   falha é barata, mas falha.
4. **A cópia=0 ms é tmpfs** (o CI terá números de disco reais); as fases
   de conversão não dependem disso.

## 6. Testes e provas

- `medicoes_010m_perfis_do_dono_por_fase` (NOVO, `tests/test_010m_
  medicoes.cpp` no `test_core`): 4 filhos, um por perfil. VEREDITOS:
  (a) dragão/buddha/scene: `importFile` devolve FALSE e o erro contém
  o teto 65535 (o diagnóstico tem de bater certo — se um dia o teto
  mudar sem este teste saber, o teste fica vermelho); (b) dragão-fit:
  TRUE com `stats.verts == 65535` e 1 .gmesh. Os tempos/RAM são
  REPORTADOS (não assertados — variam com a máquina). A limpeza de /tmp
  é TOTAL (rmRf recursivo no fim de cada filho — a lição do wiring010).
- Suite completa: **test_core 0 falhas** (com o teste novo incluído).
- Gates locais verdes: docs-lint (65 ficheiros), scope-check,
  hierarchy-check, ui-vocab, theme-hex, gmesh-docs-check (NOVO).

## 7. A prova do gate (mutação documental, vermelho→verde)

A mutação foi colada na sessão (o registo está no log da corrida):

- **M-DOCS-1**: um símbolo fantasma (`simboloFantasmaInexistente`)
  citado em `docs/GMESH_formato.md` → `gmesh_docs_check.py` FALHOU
  («símbolos citados que NÃO existem no fonte: GMESH_formato.md:
  `simboloFantasmaInexistente`», exit 1) → doc reposto → VERDE (2 docs,
  símbolos e 14 âncoras). O gate caça a documentação que mente.

## 8. NÃO VERIFICADO (honesto)

- **Os ficheiros reais do dono** (o dragão 38,051,884 B, o Happy
  Buddha e o scene 212 MB tal como estão no C33): as medições desta
  secção correm sobre CÓPIAS DE PERFIL com a MESMA escala e composição
  declarada — a composição real dos ficheiros do dono (razão
  verts/tris, u16 vs u32, número de primitivas) é DESCONHECIDA daqui; os
  tempos por fase variam com ela. Quando o dono quiser as medições dos
  ficheiros exatos, basta copiá-los para a pasta de fontes e correr o
  mesmo teste com os perfis apontados aos ficheiros.
- **As fases por dentro** (o corte fino JSON vs ranges): os marcadores
  de log de hoje separam cópia / parse(+primitivas) / fusão / escrita;
  o «parse» inclui a materialização dos ranges (que interleave com a
  fusão das primitivas). O corte fino chega no PASSO 3 como o progresso
  por fase que a spec manda aí.
- **A RAM no device** (Android): as medições são do hospedeiro (Linux);
  o VmHWM do device difere (o kernel conta diferente, o GL native
  activity partilha o processo). A ordem de grandeza e a PROVA do pico
  (modelo inteiro em RAM) mantêm-se.

## 9. Retoma (o registo do worklog)

- Último commit: este (o 1.º do 0.10-M — docs + gate + teste de medição).
- PASSO 2 (formato v3): header 64-bit, blocos ≤65,535 verts com índices
  locais u16 (u32 quando preciso), tabela de blocos com CRC32, layout de
  atributos no header, alinhamento a 16, mmap-ável, leitor
  retrocompatível v1+v2, escritor só v3, R-038 + mutações.
- PÁRO ABSOLUTO antes do PASSO 4 (render por blocos) — espera
  aprovação explícita do dono.
