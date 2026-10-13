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
| 0.10-M PASSO 5B · IMPORT DE NÓS COMO SUB-ÁRVORE — A VERIFICAÇÃO DOS PINS | o 0.10.5 (a fase 0.10-M-ext) já entregava o «expandir nós»; ESTE passo verificou os pins da spec do PASSO 5 contra o código em pé e fechou o buraco que a spec trouxe: o city de **122 NÓS** (72 com mesh + 50 VAZIOS intercalados — os agrupadores reais) EXPANDIDO = 72 peças/TICs com nomes (os vazios não geram peça, os índices do glTF preservam-se) e FUNDIDO = 1 mesh (o backward compat); a hierarquia pai→filhos honra-se pela composição de transformações (o mundo do filho = pai∘filho; o editor fica flat — decisão do 0.10.5) | **FEITO 0.10.7 (verificação)** (ver RELATORIO-0.10-M-PASSO5B-VERIFICACAO-IMPORT-NOS; 926/0; ZERO app) | ver commit |
| 0.10-M PASSO 5C · THUMBNAILS PRÉ-RENDERIZADAS (a spec do dono) | no import, UMA miniatura 256×256 POR MESH em `thumbs/<stem>.png` (o render SOFTWARE puro de `ThumbRender`: câmara isométrica fixa que encaixa o AABB com 8% de folga, z-buffer, lambert flat com a luz da casa, DETERMINÍSTICO; teto de 4096 triângulos — a pré-visualização é representativa e o custo fica limitado; o placeholder = quadro + iniciais do stem, regra D18); o browser lê os PNGs PRONTOS (`ThumbIndex::loadRows` — o pin: 100 thumbs carregadas+decodificadas em 15,6 ms no hospedeiro, 0,5 ms sob SAF no c33; piso <500 ms); thumb ausente = placeholder + RENDER SOB DEMANDA pela tabela de blocos (o pico de RAM é UM bloco — o peek POR FAIXA 192 B, a lição R-044: o readBytes inteiro media 1965 MB no gigante, a faixa volta ao 99 MB); o asset fantasma NUNCA nasce thumb do nada; falha de thumb = aviso + o import SEGUE; R-044 + TH1..TH4 vermelho→verde | **FEITO 0.10.7 (5C)** (ver RELATORIO-0.10-M-PASSO5C-THUMBS; 5 TESTs novos + c33 FASE 24 797/797; test_core 931/0; versionCode 63) | ver commit |
| 0.10-M HOTFIX SELEÇÃO-ÚNICA (a spec do dono, o vc 67 no C33) | o estado CONTRADITÓRIO da seleção morre: `selectTic`/`clearSelection` são os ÚNICOS escritores de `EditorState::selected` (os 26 sítios passam pelos dois), o `selectionAudit` limpa o ID PENDURADO antes dos leitores (o frame do main ANTES do gizmo + a entrada do `drawHierarchy`) e compacta os órfãos do multiSelect, a troca de prim/mesh mantém a seleção (o rebuild é DENTRO do TIC), o clear-on-empty-tap só com toque NASCIDO no viewport (o `dividerInput` mata o arm quando o toque nasce na pega), e a pasta `audio/` nasce com o projeto (`Project::kDirAudio`) com o `SafStorage::listDir` a registar AUSÊNCIA em I (nunca o E fantasma do catálogo) | **FEITO 0.10.7** (ver RELATORIO-0.10-M-HOTFIX-SELECAO-UNICA; pins ×8; M1/M2/M3 vermelho→verde; test_core 0 falhas; c33 797/797; versionCode 68) | ver commit |
| 0.10-M PASSO 5A · TEXTURAS (a spec do dono) | DUAS operações distintas e logadas distintas: COMPRESSÃO (ASTC 4x4 padrão, ETC2 fallback, `HardwareCompressor` de sempre) e REDUÇÃO (a política por resolução em `TexturePolicy::texProfileCapMaxDim`); TRÊS perfis por projeto (`texPerfil=0/1/2` em settings.goni — Qualidade sem redução [default], Equilibrado seletiva, Mobile agressiva) com OVERRIDE por asset a vencer (`textures/overrides.goni`, `TexturePipeline::setOverride`); os avisos no log separados («textura: reduzida A→B (perfil/override/teto do device)» ≠ «textura: comprimida ASTC 4x4 (N KB)»); o original fica no projeto e a entrada é SEMPRE o PNG de origem (nunca recomprimir de comprimido); mips filtrados em ESPAÇO LINEAR para cor (`MipGen::SrgbLinear`, a prova do 50%: 188 vs 127) + flags sRGB/normal no .gtext v2 e no .gtc v2 (o hit devolve os MESMOS bits); normal maps do glTF (`GltfMaterial::normalTex`) → RGBA8 SEM PERDA de canais (byte a byte); GL_MAX_TEXTURE_SIZE injetado do boot → halva do original com a linha «teto do device», NUNCA erro; o passe de texturas do conversor em POOL de até 4 workers FORA da UI com progresso no log; cache com CHAVE POR PERFIL (a mesma textura nos 3 perfis = 3 ficheiros/3 tamanhos: 21 845 / 5 461 / 1 365 KB na green4096) | **FEITO 0.10.7** (ver RELATORIO-0.10-M-PASSO5A-TEXTURAS; 9 TESTs novos; M-T1/T3/T5/T6/T7 vermelho→verde; test_core 925/0; versionCode 62) | ver commit |
| 0.10-A · SKELETAL (o BACKLOG que a mensagem «pele ainda não suportada no streaming» nomeia) | (1) o CONVERSOR escreve ossos+pesos nos blocos — o v3 JÁ reserva os atributos (`kAttrBones`/`kAttrWeights` no layout descrito do header; hoje um modelo COM PELE acima de 65535 vértices falha com «pele ainda não suportada no streaming (BACKLOG 0.10-A)» — mensagem clara com apontador para ESTA entrada; o merge ≤65535 é o que preserva joints/weights e o dragão de 32 MB entra por ser ≤65535); (2) RENDER com skinning por GPU — bones em UBO/SSBO (o upload vivo do `drawMesh`/`BlockMesh::draw` estende o contrato `bones/boneCount` que JÁ existe); (3) os CLIPS de animação do glTF também no import expandido (hoje ficam para o import fundido) | **POR FAZER** — decisão do dono (candidato a fase própria depois do PASSO 6) | — |
| 0.10-M PASSO 3-b · OBJ SEM TETO (dívida assumida) | o streaming do OBJ gigante (o parser atual acumula v/vt/vn globais sem teto) — os 3 modelos do dono são GLB; o OBJ ≤65,535 segue o caminho vigente | **POR FAZER** — candidato a fase própria; decisão do dono | — |
| HOTFIX · OS 12 DEFEITOS DA IMAGEM REAL (spec do dono) | D1 cabeçalho do inspector uma linha · D2 linha Transform no orçamento do rect (minWidth + reset ícone) · D3/D9 empty-states centrados+clipados nas 4 tabs · D4 Settings localizado + outline compacto + «concedido» texto · D5 a tab «Nós» morta (duplicado da hierarquia) · D6 a marca = glifo da função única com LOD (tile de letra morto) · D7 captura thumb.png 256×144 off-thread no save + fallback de iniciais (o ícone da app nunca é card) · D8 o glifo «U» saiu do rail (o ÍMAN redesenhado como ferradura) · D10 controlos de texto cru → ícones (Reset/List/Eye) · D11 divisor = linha 1dp + pill só no drag · D12 planos do gizmo preenchidos a 25% | **FEITO 0.9.6.18** (ver RELATORIO-0.9.6.18-HOTFIX-12-DEFEITOS; R-035 + gate ui_vocab + FASE 15 do device virtual) | ver commit |
| HOTFIX 0.9.6.19 · OS ABERTOS DO RE-SIGN-OFF | R1 valores do Transform intocáveis (letra→padding→nunca o valor; piso 26dp em 180/220/260dp) · D17 a câmara objeto pequeno (glifo 24dp + frustum fino 1-2px mudo/âmbar + handles de canto 12dp + hit-test gizmo>handles>frustum intocável — o drag na cena move a câmara, provado com gesto injetado) · D14 menu contido + a CAUSA-RAIZ px cru (offset 0 na reabertura, nunca sob a tab bar, última linha alcançável) · D15 o menu no gate (Exportar OBJ; Snapping na allowlist explícita) · D16 o guard do frame limpo + o retry Java das thumbs + o log ok/falhou · D18 iniciais 1ª+última letra · D19 o switch com knob on/off (fonte única nos 2 inspetores) | **FEITO 0.9.6.19** (ver RELATORIO-0.9.6.19-HOTFIX-ABERTOS-DO-RESIGN; R-035 estendida + FASE 16 do device virtual; 8 mutações vermelho→verde; contrato §2.16-§2.19; versionCode 51) | ver commit |
| F17 · ADIADOS COM NOME (o dono: «entra no BACKLOG agora») | (1) fade de distância da grelha (a grelha 3D desenha a malha inteira com o MESMO alfa — o esbatimento com a distância evita o moiré ao longe); (2) dithering anti-banding (o céu/degradês da cena mostram banding em 8-bit — dithering ordenado barato no clear/composto) | adiado com nome — POR DECISÃO DO DONO (não é esquecimento) | — |
| H · FICHEIROS COM ASSETS REAIS | drawer enumera source/+assets/ reais; refresh; R-019 | por fazer | — |
| I · BENCHMARKS | bench de 60s com JSON/loja/copiar; vsync; 3×; cena de benchmark | por fazer | — |

## 0.10-M PASSO 5A-b · A POOL PARALELA DO PASSE DE TEXTURAS (dívida do 5A)

- A spec do dono pede «codificação paralela fora da UI com progresso».
  O 5A entregou «fora da UI» (o job thread) e «com progresso» (as linhas
  por textura); a POOL (jobs/results com os bytes movidos do modelo +
  até 4 workers) detonou um SEGFAULT no CI (o teste data-uri; 4×;
  nunca local) — a bissecção (probe-A verde / probe-B vermelho sem
  threads / probe-D inline+API-nova verde) APONTA a reestruturação e o
  mecanismo exato ficou ABERTO (o ASAN morreu dentro do próprio
  alocador em ChunkReader::close/shrink_to_fit, FileApi.cpp:406, sem
  relatório de programa). A estrutura INLINE de sempre ficou VERDE e é
  a que segue. Reabrir com: a pool desenhada sem mover os bytes do
  modelo (copiar ou processar por janela deslizante), o ASAN do CI com
  -fsanitize-recover e sem o overflow KNOWN dos ícones no caminho, e o
  handler de crash do runner a dizer o teste (já está).
- (o fix KNOWN dos ícones — Icons.cpp `kKeyboardPts` com nPts=32 >
  24 pontos reais, BACKLOG 0.10.2 — continua POR DECISÃO DO DONO; o
  overrun de LEITURA deixou de ser «benigno em prática» no dia em que
  o layout o pôs numa fronteira de página — continua sem ser o gatilho
  deste segfault, mas a cura é uma linha: nPts 32→24.)

## Ideias da revisão externa (10-09) — registadas, POR DECISÃO DO DONO

> Entradas vindas de UMA revisão técnica externa (10-09 — o dono trouxe
> as ideias; o mérito é do revisor). Nada aqui está agendado: cada uma
> vira fase própria quando o dono mandar.

- **Occlusion culling** (o revisor externo, 10-09): evitar desenhar
  objetos completamente escondidos atrás de outros. NOTA do revisor para
  mobile: em GPUs tiled (PowerVR GE8320 do C33 incluído) a hardware
  occlusion query é CARA; as alternativas baratas são occluders estáticos
  + depth grosseiro em CPU, ou flags por bloco pré-computadas no
  conversor. O «géante mesh» que motivava a ideia já está resolvido pela
  arquitetura por blocos do PASSO 4 (o frustum AABB por bloco culle o
  que está fora da câmara); o que resta é APENAS o hidden-behind-
  others dentro do frustum — ganho dependente da cena (cidades fechadas
  ganham; natureza aberta quase nada). Candidato a medir ANTES de
  construir: contagem de blocos totalmente ocultos numa cena real.

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

## 0.10.7 HOTFIX SELEÇÃO-ÚNICA (o estado contraditório da seleção morre) — FEITO

- **FEITO 0.10.7-hotfix-selecao-unica**: o vc 67 no device mostrou a linha
  âmbar com nome em branco + inspector «Nada selecionado» + sem gizmo AO
  MESMO TEMPO, o id pendurado após troca de prim/mesh ou guarda-restauro
  de layout e o highlight a sobreviver ao modelo. As causas confirmadas:
  26 escritores diretos de `selected`, nenhuma auditoria do id pendurado
  antes dos leitores, o inspector a escrever direto, e a pasta audio/ a
  não nascer com o projeto (o E fantasma «pasta ausente» no catálogo).
  A cura nas 5 pernas: (1) UMA FONTE DE VERDADE — selectTic/clearSelection
  únicos escritores, a linha âmbar DERIVADA no draw; (2) ID PENDURADO NÃO
  É SELEÇÃO — o selectionAudit em duas linhas de defesa (o frame antes do
  gizmo + a entrada do drawHierarchy) com os órfãos do multi por compação;
  (3) a troca de prim/mesh mantém a seleção; (4) a ORIGEM do toque (o
  clear só nasce no viewport; painel/resize/layout nunca limpam nem
  selecionam); (5) a pasta audio/ no create + o listDir em I para
  ausência. Pins: o contrato do audit · apagar limpa o highlight NO DRAW
  (prova VISUAL de zero quads âmbar) · layout não mexe na seleção · a
  origem do toque ×4 · os três leitores em harmonia · tap na hierarquia →
  TIC viva com nome no frame seguinte · troca prim/mesh mantém · projeto
  novo com audio/ e zero E. Mutações M1 (highlight por índice próprio),
  M2 (clear sem origem) e M3 (draw sem audit) → vermelhas. test_core 0
  falhas · c33 797/797 (o 17.4 à lei nova: TICs VIVAS no conjunto da
  multi) · versionCode 68. Relatório:
  docs/RELATORIO-0.10-M-HOTFIX-SELECAO-UNICA.md.
- **POR FAZER (o sign-off do dono)**: reproduzir o vc 67 no device (troca
  de prim/mesh com a TIC selecionada; guarda-restauro de layout; apagar a
  TIC selecionada; abrir o catálogo num projeto novo) e ver os três
  leitores SEMPRE em harmonia + ZERO linhas E de audio.

## 0.10.7 HOTFIX P0 — SUB-TICs em lotes (o expand deixa de travar) — FEITO

- **FEITO 0.10.7-hotfix-p0**: o dono no C33: «expandir em sub-TICs»
  travava/crashava. As 4 causas confirmadas e curadas: (a) o expand
  inteiro na thread de UI → o `GltfExpandBatch` (≤32 peças por frame no
  ponto seguro, `perPiece` para o auto-bind, o one-shot como wrapper —
  o estado final é o MESMO); (b) os walks sem visitados → conjunto de
  visitados nos TRÊS (o ciclo corta e o log diz); (c) TICs ilimitados →
  `convert::kGmeshV3ExpandMaxTics`=2000 + `Stats::expandLimited` + o
  aviso claro no log/toast; (d) a reentrância → guardas (sem expand em
  PLAY; sem import/extract com o expand a correr). + progresso no
  overlay («A EXPANDIR… d/t» + barra) · cancelamento limpo (os TICs
  nascidos morrem em ordem inversa) · (e) os últimos «expandir nós»
  vivos morrem · (f) o import cria a sub-árvore no próprio import (em
  lotes). Pins: o ciclo termina+ diz · 2100 nós → ≤2000 + aviso · o
  lote 32/64/96/100 + o cancelamento + o one-shot igual. Mutações
  M-P0a (sem visitados → abort) e M-P0b (sem lote → vermelho) →
  vermelhas. test_core 947/948 (o 1 = r039, flake de DISCO da sandbox)
  · c33 797/797 · versionCode 67. Relatório:
  docs/RELATORIO-0.10-M-HOTFIX-P0-SUBTICS.md.
- **POR FAZER (o sign-off do dono)**: o city no device com a opção
  ativa (o overlay sem ANR + os 72 TICs) e o cancelamento a meio.

## 0.10.7 HOTFIX-3 — TEXTURAS-MULTIMAT (a honestidade dos slots + o seletor de material) — FEITO

- **FEITO 0.10.7-hotfix-3**: o vc 65 no device revelou 2 gaps e a cura
  fecha-os: (1) a HONESTIDADE — os 4 slots não-baseColor preenchidos
  mostram o sufixo «(vinculado · render na 0.11-L)» (a política é a
  função pura `texSlotRenders`/`inspTexSlotLabel` em EditorLayout.h; o
  baseColor sem sufixo; o nome trunca primeiro — o sufixo nunca sai do
  ecrã); (2) o MULTI-MATERIAL — a grade (N−1)×5 no `MeshRenderer`
  (`matExtraTex` + matCount/matTex/matTexRef), o auto-bind sonda o GTB1
  inteiro, o seletor «material: i/N» com setas ‹ › vetoriais (ícone
  ChevronLeft novo) entra só com N>1, as 5 linhas mostram o material
  selecionado e o pick escreve NELE (`applyAssetPick` +materialIndex,
  clamp honesto); serializer («matExtraTex») + undo acompanham. Pins:
  3 materiais = 15 texturas + a grade · pick no material 2 sem tocar no
  0 · sufixo honesto · UI real (‹ › + pick no material). Mutações M-MM1
  (seletor hardcoded material 0) e M-MM2 (sufixo removido) → vermelhas.
  test_core 944/945 (o 1 = r039, flake de DISCO da sandbox) · c33
  797/797 · versionCode 66. Relatório:
  docs/RELATORIO-0.10-M-HOTFIX-TEXTURAS-MULTIMAT.md.
- **POR FAZER (a fronteira documentada)**: o draw refletir o pick por
  material e os samplers 1-4 no shader lit = a 0.11-L (o sufixo honesto
  anuncia-a); .gmesh antigos = reimportar.

## 0.10.7 HOTFIX-2 — TEXTURAS-VINCULA-2 (os 5 slots do import e do picker) — FEITO

- **FEITO 0.10.7-hotfix-2**: o device revelou 2 bugs residuais do vc 64
  e a cura fecha-os: (1) o parse lê OS 5 slots do glTF
  (metallicRoughness/emissive/occlusion faltavam — o GTB1 nunca os
  recebia) e o auto-bind (apply + import EXPANDIDO) enche OS 5 slots do
  TIC pelo `GpuAssets::matBindsOf` novo (o `baseColorBindOf` vira
  wrapper); a troca de mesh leva os slots atrás; (2) o ramo do
  `TexSlotRow` no dispatch do Inspector era CÓDIGO MORTO (faltava na
  condição externa — o tap nas 5 linhas não fazia nada e o único caminho
  era a linha clássica que arma o 0: «o picker escreve sempre no
  baseColor»). Mutações M-TB2a/M-TB2b vermelho→verde · test_core 935/0 ·
  c33 797/797. Relatório:
  docs/RELATORIO-0.10-M-HOTFIX-TEXTURAS-VINCULA-2.md.
- **POR FAZER (o dono agenda)**: os samplers dos slots 1-4 no shader lit
  (mantém-se do hotfix 1); a edição por MATERIAL no Inspector (o conjunto
  do TIC é o material 0 do auto-bind — o render por material já é do
  blockHull); a migração dos .gmesh antigos é reimportar.

## 0.10.7 HOTFIX — TEXTURAS-VINCULA (+ sub-TICs reais) — FEITO

- **FEITO 0.10.7-hotfix**: o import liga as texturas ao mesh (o bloco
  GTB1 no fim da secção de materiais do .gmesh — material→5 slots→.gtext;
  retro-compat total: os leitores de sempre param nos nomes), o draw por
  blocos usa a textura DO SEU material, o Inspector ganha os 5 slots
  (o pick escreve O SLOT — irmãos intactos), «expandir em sub-TICs» (o
  vocabulário TIC) e a sub-árvore REAL (parentRec + TRS local + o
  TransformSystem a compor pais — a dívida «editor polish» do F3 paga).
  Mutações M-TB1/M-TB3 vermelho→verde · test_core 932/0 · c33 797/797.
  Relatório: docs/RELATORIO-0.10-M-HOTFIX-TEXTURAS-VINCULA.md.
- **POR FAZER (o dono agenda)**: os samplers dos slots 1-4 no shader lit
  (os slots ficam vinculados; o GL só carrega o baseColor de hoje —
  NÃO VERIFICADO no relatório §6); a migração dos .gmesh antigos é
  reimportar/reconverter (o GTB1 nasce no conversor).
