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
| UI PASSO 3 · VIEWPORT | controlos ≤10% da área a 60% alfa: rail esquerdo (Sel/Mov/Rod/Esc/Ímã), desfazer/refazer/guardar/⋯ topo-esq (o ⋯ abre o menu ancorado; dup/colar vivem no menu), [+] 40dp redondo, gizmo 40dp topo-dir, legenda; a STRIP full-width REMOVIDA; o desenho cobre 7,7% do viewport no arranque do device (a tabela completa no relatório; o estado com o drawer aberto fica a 15,1% — §7 do relatório para o dono decidir) | **FEITO 0.9.6.17** (ver RELATORIO-0.9.6.16-17-P2BIS-PASSO3-VIEWPORT; R-023 reescrita + R-034 com o pin da alfa) | ver commit |
| UI PASSO 4 · JANELAS | logs opacos com corte/scroll/wrap (80% do ecrã — o card opaco bg + o wrap por palavras da fonte única das Docs); menus ancorados com fundo 40% (o token scrimMenu nos 3 ancorados; o scrim 60% fica modal); settings linhas 36dp (secções 48 mantêm; controlos 28 centram a dp(4)); cantos suaves fechados (os 5 cards duros + os raios dos sheets em dp) — os 3 sub-itens do Settings já tinham entrado no 0.9.6.18 (D4) | **FEITO 0.9.6.20** (ver RELATORIO-0.9.6.20-PASSO4-JANELAS; R-037 + c33 FASE 18; contrato §2.25-§2.28; versionCode 53) | ver commit |
| 0.10-M PASSO 1 · DIAGNÓSTICO (a fase .gmesh v3) | docs GMESH_formato/GTEX_formato (fonte de verdade + o gate gmesh_docs_check no CI); a TABELA DE TETOS (4 guards do 65,535 + MeshData u16 + GL_UNSIGNED_SHORT + range 256 MB/JSON 16 MB/PNG 64 MB + os achados latentes); AS MEDIÇÕES por fase + RAM pico (dragão/Buddha morrem no parse; scene morre na fusão com +217 MB de RAM — o modelo INTEIRO; dragão-fit 65,535 verts completa em 28 ms) — ZERO código de app | **FEITO 0.10.0** (ver RELATORIO-0.10-M-PASSO1-DIAGNOSTICO; o teste de medição corre no CI) | ver commit |
| 0.10-M PASSO 2 · FORMATO v3 | header 64-bit (contagens verts/tris/blocos/materiais + AABB + offset da tabela); blocos ≤65,535 verts (u16 locais, u32 quando preciso; CRC32 por bloco); layout de atributos no header; alinhamento 16 B, mmap-ável (mapFile64); leitor retrocompatível v1+v2 (as fixtures REAIS commitadas — a v2 = payload v1, decisão ao dono), escritor só v3; a integridade por camadas (meta FNV + tabela CRC + bloco CRC); R-038 + as mutações M-V3a/M-V3b vermelho→verde | **FEITO 0.10.1** (ver RELATORIO-0.10-M-PASSO2-FORMATO; 8 casos v3 + c33 631/631; versionCode 54) | ver commit |
| 0.10-M PASSO 3B · LOG + CAUSA DA IMPORTAÇÃO | o viewer stale morto (o readTail lê o ATIVO primeiro — o backup mais antigo enchia a janela de 300 e o log de HOJE nunca era lido); as linhas contrato `gmesh: fase=parse\|cut\|assembly\|verify\|load\|render ms=` + `import: copia ms=`; a causa do «fail de 203 MB» registada com números («memória insuficiente ao carregar mesh — cura no PASSO 4: render por blocos»; orçamento kMeshLoadBudgetBytes 256 MB) e o err do load CHEGA ao engine.log (o LOGE do GpuAssets só falava com o logcat); o export provado = a mesma fonte do writer (FASE 19.5); M-A/M-B vermelho→verde | **FEITO 0.10.3** (ver RELATORIO-0.10-M-PASSO3B-LOG; c33 670/670; versionCode 57) | ver commit |
| 0.10-M PASSO 3 · CONVERSOR STREAMING | lê por primitiva/range (mmap), agrupa por material, corta em blocos; pool de threads + escritor que ordena; SEM PERDA (float32 da origem, sem soldar); verificação bit a bit + log `gmesh: v3 blocos=… verificado=1`; progresso por fase + cancelar; sentinela R-039: 50M verts / 1754 MB com RAM pico 107 MB (blocos=920, verificado=1); as 11 e2e do c33 12.8/12.9 fechadas (a cascata do temp store + os meshes sem nodes com node=-1 + a degradação do mmap + VerifyCtx.droppable); M1/M2/M3 vermelho→verde | **FEITO 0.10.2** (ver RELATORIO-0.10-M-PASSO3-CONVERSOR; versionCode 56) | ver commit |
| 0.10-M PASSO 4 · RENDER POR BLOCOS | o .gmesh v3 abre pela TABELA (192 B + tabela + materiais — zero dados); frustum AABB por bloco com audit; lazy na 1ª visibilidade; cache LRU com orçamentos DECLARADOS 64+192 MB (o mesmo 256 MB da casa, limitado por construção); o hull de bounds preserva o contrato do picker/serializer; picking/colisão/cena pela tabela; o OBJ export por stream (pico = 1 bloco); o HUD «FPS · TICs · bl n/m» + o log throttled; o pino dos 920 blocos (fork+VmHWM: orbit realista pico 0 KB; pior caso 10,6 MB < 19 MB do modelo); R-040 + as mutações M1/M2/M3 vermelho→verde | **FEITO 0.10.4** (ver RELATORIO-0.10-M-PASSO4-BLOCOS; c33 706/706; versionCode 58) | ver commit |
| 0.10-M HOTFIX SAF-STREAM | o streaming sob QUALQUER storage: a recusa content:// MORREU; a fonte do bin em cascata (mmap-caminho → **mmap POR FD** — o fd do bridge `bridgeOpenFd` sobre o irmão copiado em source/ → **pread de RANGES** — o degradado honesto); o data: URI também streama; `fase=parse` só o JSON + `fase=ranges` própria (os 5395 ms do dono); o teto de 65535 deixa de ser visível ao não-skinned; R-041 + M-S1/M-S2/M-S3 vermelho→verde | **FEITO 0.10.4c** (ver RELATORIO-0.10-M-HOTFIX-SAF-STREAM; c33 724/724; versionCode 59) | ver commit |
| 0.10-M EXT · IMPORT DE NÓS COMO SUB-ÁRVORE (a spec do dono) | a opção no import «expandir nós» cria UM TIC POR NÓ COM MESH (nomes do glTF preservados, transformação do nó como Transform do TIC — o TRS MUNDO decomposto, a geometria da peça CRUA no espaço local; shear cai no bake com TRS identidade), em vez de fundir num TIC só; default FUNDIDO por performance mobile, expandido para peças editáveis (toggle nas Definições, settings.goni `expandNodes`); cada peça é um `assets/<stem>_<nó>.gmesh` com `kGmeshV3FlagPiece` (abre POR BLOCOS → culling por TIC + lazy + LRU); a sub-árvore entra sem diálogo «aplicar ao TIC?» com fit único; PIN: city expandido = 72 TICs com nomes, culling por TIC verde (c33 FASE 22: 1 visível, 71 culled); R-042 + M-E1..M-E4 vermelho→verde | **FEITO 0.10.5** (ver RELATORIO-0.10-M-EXT-IMPORT-NOS; c33 743/743; versionCode 60) | ver commit |
| 0.10.6 HOTFIX SAF-SEAM (as duas últimas costuras do SAF) | (1) o BlockMesh::open do RUNTIME usa a MESMA CASCATA do conversor — `openReadFd` (o FD DO BRIDGE sob content://) → `mapFd64` serve as 3 leituras (peek 192 B + tabela + materiais, ZERO ranges) → pread NO MESMO fd (o provider recusou o mapa) → os RANGES; a linha `gmesh: fase=load … fonte=<mmap-fd\|pread-fd\|ranges>` — o par do `asset: v3 fonte=` do conversor; em falha o `err` diz QUAL das 3 leituras + o errno + o tamanho VISTO; os MATERIAIS sem tamanho leem-se NOME A NOME (a sonda de 256 B matava caudas curtas — «cidade» são 8 B); (2) o overlay de import NUNCA «0 / 0» (length=0 → «a copiar…»/«copiando… N B»; sub-MB → B; o pin do 29 MB); (3) a QUARENTENA do asset corrompido: `ProjectStorage::rename` NOVO (`<rel>.corrupt` — Fs = `::rename` atómico; Saf = cópia streaming por fd + remove com a cache de URIS a seguir o nome) + «asset corrompido, reimporta» no err/toast/engine.log; o picker MANTÉM o estado anterior (nunca a bola silenciosa — o `GpuAssets::lastMeshError` alimenta o toast e o catálogo re-lista); peek inválido NÃO é quarentena; I/O falho NÃO é quarentena; R-043 + M-SS1/M-SS2/M-SS3 vermelho→verde | **FEITO 0.10.6** (ver RELATORIO-0.10-M-HOTFIX-SAF-SEAM; c33 781/781; versionCode 61) | ver commit |
| 0.10-A · SKELETAL (o BACKLOG que a mensagem «pele ainda não suportada no streaming» nomeia) | (1) o CONVERSOR escreve ossos+pesos nos blocos — o v3 JÁ reserva os atributos (`kAttrBones`/`kAttrWeights` no layout descrito do header; hoje um modelo COM PELE acima de 65535 vértices falha com «pele ainda não suportada no streaming (BACKLOG 0.10-A)» — mensagem clara com apontador para ESTA entrada; o merge ≤65535 é o que preserva joints/weights e o dragão de 32 MB entra por ser ≤65535); (2) RENDER com skinning por GPU — bones em UBO/SSBO (o upload vivo do `drawMesh`/`BlockMesh::draw` estende o contrato `bones/boneCount` que JÁ existe); (3) os CLIPS de animação do glTF também no import expandido (hoje ficam para o import fundido) | **POR FAZER** — decisão do dono (candidato a fase própria depois do PASSO 6) | — |
| 0.10-M PASSO 3-b · OBJ SEM TETO (dívida assumida) | o streaming do OBJ gigante (o parser atual acumula v/vt/vn globais sem teto) — os 3 modelos do dono são GLB; o OBJ ≤65,535 segue o caminho vigente | **POR FAZER** — candidato a fase própria; decisão do dono | — |
| HOTFIX · OS 12 DEFEITOS DA IMAGEM REAL (spec do dono) | D1 cabeçalho do inspector uma linha · D2 linha Transform no orçamento do rect (minWidth + reset ícone) · D3/D9 empty-states centrados+clipados nas 4 tabs · D4 Settings localizado + outline compacto + «concedido» texto · D5 a tab «Nós» morta (duplicado da hierarquia) · D6 a marca = glifo da função única com LOD (tile de letra morto) · D7 captura thumb.png 256×144 off-thread no save + fallback de iniciais (o ícone da app nunca é card) · D8 o glifo «U» saiu do rail (o ÍMAN redesenhado como ferradura) · D10 controlos de texto cru → ícones (Reset/List/Eye) · D11 divisor = linha 1dp + pill só no drag · D12 planos do gizmo preenchidos a 25% | **FEITO 0.9.6.18** (ver RELATORIO-0.9.6.18-HOTFIX-12-DEFEITOS; R-035 + gate ui_vocab + FASE 15 do device virtual) | ver commit |
| HOTFIX 0.9.6.19 · OS ABERTOS DO RE-SIGN-OFF | R1 valores do Transform intocáveis (letra→padding→nunca o valor; piso 26dp em 180/220/260dp) · D17 a câmara objeto pequeno (glifo 24dp + frustum fino 1-2px mudo/âmbar + handles de canto 12dp + hit-test gizmo>handles>frustum intocável — o drag na cena move a câmara, provado com gesto injetado) · D14 menu contido + a CAUSA-RAIZ px cru (offset 0 na reabertura, nunca sob a tab bar, última linha alcançável) · D15 o menu no gate (Exportar OBJ; Snapping na allowlist explícita) · D16 o guard do frame limpo + o retry Java das thumbs + o log ok/falhou · D18 iniciais 1ª+última letra · D19 o switch com knob on/off (fonte única nos 2 inspetores) | **FEITO 0.9.6.19** (ver RELATORIO-0.9.6.19-HOTFIX-ABERTOS-DO-RESIGN; R-035 estendida + FASE 16 do device virtual; 8 mutações vermelho→verde; contrato §2.16-§2.19; versionCode 51) | ver commit |
| F17 · ADIADOS COM NOME (o dono: «entra no BACKLOG agora») | (1) fade de distância da grelha (a grelha 3D desenha a malha inteira com o MESMO alfa — o esbatimento com a distância evita o moiré ao longe); (2) dithering anti-banding (o céu/degradês da cena mostram banding em 8-bit — dithering ordenado barato no clear/composto) | adiado com nome — POR DECISÃO DO DONO (não é esquecimento) | — |
| H · FICHEIROS COM ASSETS REAIS | drawer enumera source/+assets/ reais; refresh; R-019 | por fazer | — |
| I · BENCHMARKS | bench de 60s com JSON/loja/copiar; vsync; 3×; cena de benchmark | por fazer | — |

## Dívidas registadas durante a campanha (fora do scope do grupo em curso)

- (nenhuma no Grupo B — os dois bugs apanhados ao vivo ERAM do scope: os
  botões novos do Diagnóstico no walk do tap e o falso positivo do clip;
  o 1 ERRO do editor [label que sangra 3px] e os avisos de toque < 48dp
  são a LINHA DE BASE dos Grupos C-I, não dívida do B — o relatório
  do estado atual lista-os com os números medidos)
## 0.9.6.19b (HOTFIX B) — as decisões novas candidatas (NÃO VERIFICADO do relatório)

- (m1) a contagem da multi-seleção fica SEM leitura no cabeçalho quando o
  painel é estreito (o chip não desenha para não colar ao título; a
  limpeza segue no menu ⋮). Se o dono preferir a contagem SEMPRE visível
  (comprimindo o título), é uma decisão nova.
- (D21) o + da hierarquia mantém o alvo 28dp da linha do cabeçalho (o
  piso kHeadFloorDp da casa). Se o dono quiser o TOQUE ≥40dp, é uma
  decisão nova (o desenho PASSO 1 manda 28 na linha).

## 0.9.6.20 (PASSO 4 · JANELAS) — as decisões novas candidatas (NÃO VERIFICADO do relatório)

- (J-02) o «fundo 40%» dos menus ancorados foi lido como o VÉU (a área
  à volta do sheet). A alternativa (o vidro do próprio menu a 40%)
  quebra as pisos de contraste da casa sobre cena clara — se o dono a
  quiser, é uma decisão nova com um trade-off de contraste.
- (J-04) o `drawSettingsMenu` (o card "SETTINGS" de 7 linhas) é CÓDIGO
  MORTO no app (a página Settings 0.9.0 substituiu-o; só o test_ui o
  chama) — não foi tocado. A remoção é decisão do dono.
- (J-02) o token `scrim` (60%) ficou SEM utilizadores vivos (os cards
  centrados nunca tiveram véu) — mantido como o véu MODAL documentado;
  a remoção (ou o véu nos centrados) é decisão do dono.


## 0.10.2 (PASSO 3 · CONVERSOR STREAMING) — as decisões novas candidatas (NÃO VERIFICADO do relatório)

- **O bug de dados dos ícones (`nPts` > array)**: o ASAN apanhou
  `test_toolbar.cpp:218` a ler 4 bytes além de `kKeyboardPts`
  (Icons.cpp:731, 192 B) — algum ícone declara mais pontos do que o
  array tem (leitura de global adjacente, benigna em prática). FORA do
  scope do 0.10-M — não tocado. A cura é ou corrigir o `nPts`/array do
  ícone ou guardar o loop do teste; decisão do dono.
- **A colisão do ID R-038**: o PASSO 2 nomeou a sua sentinela
  `regress_gmesh_v3_retrocompat` como «R-038», mas o REGRESSOES.md já
  tinha o R-038 do HOTFIX B (bloco m1/m2/m3). A cláusula retrocompat
  vive no GMESH_formato.md §4 — ao próximo toque nas REGRESSÕES,
  renumerar (ex.: R-042) e alinhar as citações; decisão do dono.
- **O job separado da R-039**: a sentinela (+~37 s medidos) ficou no
  MESMO job core-tests do CI; a escotilha (mover o TEST para um job
  próprio) está documentada no relatório §7 — mover é só workflow, sem
  tocar no teste. Nada a fazer hoje.
