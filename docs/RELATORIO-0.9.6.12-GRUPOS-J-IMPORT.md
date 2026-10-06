# RELATORIO-0.9.6.12 — GRUPOS J (ARQUITETURA DO EDITOR) + IMPORT A2

> O fecho da campanha «CORRIGIR OS 5 DEFEITOS DE ARQUITETURA DO EDITOR»
> (grupos J1-J4, cláusula P-08) + o hotfix A2 do pipeline de import (os
> três ficheiros reais do dono). Método da casa: leitura do código ANTES
> de mexer (P-01/P-04), zero funcionalidade nova, sentinelas com prova de
> mutação, e o que não está verificado está DITO no fim.

## 1. A VERDADE DA ARQUITETURA (o mapeamento honesto prompt ↔ código)

O prompt dos 5 defeitos descreve a UI em vocabulário de views Java
(RootLayout, TopNavigationBar, ViewportContainerLayout, GLSurfaceView,
TransformToolbar, BottomDockLayout, BottomBar). A leitura do código
confirmou que **o editor não tem views Java**: a `VvActivity`
(app/src/main/java/vv/goni/VvActivity.java) é uma `NativeActivity` com
UMA superfície EGL; todo o editor é UI immediate-mode desenhada em C++
(`cpp/ui`) e o passe 3D renderiza num retângulo controlado por
`glViewport`+`glScissor`. Cada pedido da spec foi portado ao equivalente
REAL — o contrato (`docs/LAYOUT_HIERARCHY.md`, §0) documenta o mapeamento
nome a nome, e o gate `hierarchy-check` afere que ele não apodrece:

| Pedido da spec (Java) | Equivalente real (C++) | Onde |
|---|---|---|
| ConstraintLayout do container | matemática pura dos rects | `ui/SafeArea.h` |
| `topToTopOf="parent"` em dp | âncoras derivadas do rect (dp) | `ui/ViewportChrome.cpp` |
| `bringToFront()` / z-index | ordem de desenho (immediate-mode) | `platform/main.cpp` frame() |
| `RENDERMODE_CONTINUOUSLY` | o loop renderiza sempre; scissor contém o 3D | `platform/main.cpp` |
| `onSizeChanged` → JNI → `glViewport` | o MESMO frame calcula o rect e chama glViewport (sem salto JNI) + o log `vp3d:` | `platform/main.cpp` |
| `wrap_content`/`minWidth`/`ellipsize` | medida da fonte + piso + `textfit` | `ui/ViewportChrome.cpp`, `ui/TextFit.h` |
| `layout_alignParentBottom` | `safe::statusRect` (a última faixa) | `ui/SafeArea.h`, `ui/BottomPanel.cpp` |

## 2. GRUPO J1 — A HIERARQUIA (defeitos 1 e 2)

**Causas (ficheiro + função):**
1. `platform/main.cpp` — `currentDrawerH()` devolvia `g_bottom.drawerH`
   CRU, enquanto `ui/BottomPanel.cpp` — `bottom::layout()` aplicava um cap
   DIFERENTE (viewport − 64dp) só ao DESENHAR. No device (drawer 240dp
   persistido, viewport 208dp) o viewRect colapsava a ZERO e a toolbar de
   transformação desenhava-se ~56dp acima do topo da viewport — SOBRE a
   top bar (o defeito 1 verbatim; o «estático» do defeito 2 era o mesmo
   desvio a flutuar sobre o drawer desenhado).
2. `ui/ViewportChrome.cpp` — `layout()` não tinha clamps: nada impedia os
   alvos de saírem do rect em viewports degenerados.
3. ACHADOS da sentinela: o `toolPanel` transbordava 8dp o viewport de
   288dp; no C33 (content 756dp) a fileira de botões saía 4dp (o Ímã fora
   do rect — os pisos da gangorra somam 760 > 756).

**Fix:** `safe::effectiveDrawerH` — a FONTE ÚNICA da altura efetiva
(clamp 160..400 → cap pelo novo piso `safe::kViewportMinH` = 104dp →
passos de 8dp), consumida pelo draw do drawer E pelo main; clamps no
`vpchrome::layout` (a toolbar nunca acima da strip, nunca fora do rect;
a strip esconde em viewport sub-piso; a legenda some antes de cruzar a
strip); a margem esquerda da toolbar cede (16→4dp) antes de transbordar;
o `toolPanel` clampa à direita.

**Sentinela R-022** — `regress_hierarquia_contrato` (tests/test_sentinels.cpp):
o contrato inteiro (containment, toolbar<strip<top bar, fonte única,
status intocável, pisos) em 4 ecrãs (RMX3624@2.0, C33@2.0, harness@1.0,
1024×640) × 4 estados do drawer.

**Mutações (vermelho → verde):**

| mutação | o que morreu | vermelho | verde |
|---|---|---|---|
| R-022a: `effectiveDrawerH` devolve o cru (a divergência de volta) | o cap da fonte única | `regress_hierarquia_contrato` FALHOU (eff>cap, alvos fora, stackPanel fora) + 67 testes | reposição → 0 falhas |

## 3. GRUPO J2 — TEXTO E CLIPPING (defeito 3)

**Causas (ficheiro + função):**
1. `ui/ViewportChrome.cpp` — `layout()`: os chips da strip tinham
   larguras FIXAS (88+112+88dp = 312dp) num viewport de piso 288dp COM o
   [+] a viver no fim direito da strip (plusTopRight no device): o chip
   Global transbordava e era COBERTO pelo pai do [+] (desenhado depois) —
   o dono lia «Glob+». O recorte era COBERTURA, não ellipsis.
2. O campo «pesquisar TIC» (`ui/EditorUi.cpp` — drawHierarchy) NUNCA
   colidiu: vive confinado ao painel por construção (w − 2×kPad com
   `labelFitted`); afervado agora com a fonte real no piso 200dp.

**Fix (a spec J2):** `vpchrome::layout(view, ChipWidths*)` — chips
MEDIDOS pela fonte real (texto + 2×8dp, piso 56dp = o minWidth), a
RESERVA do [+] respeitada, degradação POR ORDEM: Perspetiva esconde
primeiro, depois Cena, o Global é o ÚLTIMO (e só ellipsize quando nem
ele cabe — «Glob…» honesto). `cw=null` mantém as larguras antigas
(compat de testes).

**Sentinela R-023** — `regress_texto_strip_campo`: o encaixe em 3
densidades (mdpi 1.0 / hdpi 1.5 / xhdpi 2.0 — a spec pede as três) com
medidor fake density-invariante; a FONTE REAL na FASE 14.1/14.2 do
c33_virtual (o chip comporta «Global»; «pesquisar TIC» cabe no piso).

**Mutações:**

| mutação | o que morreu | vermelho | verde |
|---|---|---|---|
| R-023a: as medidas ignoradas (larguras fixas de volta) | o wrap-content | `regress_texto_strip_campo` FALHOU (Global fora do [+], fora da strip, degradação morta) + 16 testes | reposição → 0 falhas |

## 4. GRUPO J3 — A SINCRONIZAÇÃO DO VIEWPORT (defeito 4)

**Causas (ficheiro + função):** a spec pede «JNI: viewport set to» +
`Camera::SetAspectRatio` — a arquitetura real não tem esse salto JNI; o
`glViewport`/`glScissor` + o aspect DO RECT existem desde o G1-3/Grupo D
(`platform/main.cpp`, o bloco `scissor3d`) e alimentam-se do centerRect
com o drawerH EFETIVO — o que restava do defeito 4 era a causa do R-022
(já curada). O que FALTAVA era o DIAGNÓSTICO: nenhuma linha dizia QUE
rect o render usou.

**Fix:** o log `vp3d: viewport set to (x, y, w x h) — aspect N.NNN` no
bloco do scissor, SÓ na mudança do rect (nunca por frame) — se um painel
abrir/fechar e a linha não aparecer, o evento não chegou ao render.

**Sentinela R-024** — `regress_viewport_rect_segue` (a matemática: o rect
muda com o painel; o aspect segue o RECT; o literal vigiado no fonte) +
a FASE 14.3 do replay (o log A ACONTECER: aberto →
`vp3d: viewport set to (400, 160, 576 x 208)`; o fecho devolve a linha
do rect cheio; o aspect = w/h do rect).

**Mutações:**

| mutação | o que morreu | vermelho | verde |
|---|---|---|---|
| R-024a: o log mutado | o diagnóstico | FASE 14.3 com 4 falhas + a sentinela vermelha | reposição → 456/456 |

## 5. GRUPO J4 — O RODAPÉ (defeito 5)

**Causas (ficheiro + função):** a posição do rodapé (BottomBar 24dp)
nunca mudou no código atual (`safe::statusRect` é a última faixa —
vigiada pela R-022). O defeito REAL restante era o CORTE:
`ui/BottomPanel.cpp` — `drawStatusBar()` compunha a linha inteira
(versão · projeto · FPS · TICs) e mandava-a ao fit — o «…» comia o FIM
(o suffixo FPS/TICs sumia com nomes longos).

**Fix (a spec J4):** `textfit::ellipsizeMiddle` NOVO (`ui/TextFit.h` —
puro, GL-free, fronteiras UTF-8) + a composição prefixo + projeto
ellipsado ao SEU orçamento + suffixo — o orçamento na MESMA medida corpo
que o maxW do fit.

**Sentinela R-025** — `regress_rodape_intocavel` (o ellipsizeMiddle: cabe
inteiro / cabeça+cauda / orçamento absurdo → vazio / UTF-8 nunca partido;
o rodapé como última faixa em 2 densidades × 3 estados; a composição
mantém o suffixo no fim) + a FASE 14.4 do replay (projeto de 120 glifos
com a fonte real: o label da faixa NÃO truncado).

**Mutações:**

| mutação | o que morreu | vermelho | verde |
|---|---|---|---|
| R-025a: o middle morto (o projeto cru na composição) | a preservação do suffixo | FASE 14.4 FALHOU (o label truncado) | reposição → 456/456 |

NOTA honesta: a 1ª rodada da mutação ficou verde POR ACIDENTE — o audit
só corre no frame de export e a 14.4 lia o registo STALE; armado
`g_layoutExportPending` (a lição R-014: a prova tem de falhar pela razão
certa).

## 6. IMPORT A2 — OS TRÊS FICHEIROS REAIS (R-014 reescrita)

**Causas (ficheiro + função):**
1. `assets/GltfImporter.cpp` — `resolveView()`: o bound dupla-contava o
   `accessorByteOffset` (`total = off + accOff + bLen > real`) — um GLB
   100% VÁLIDO com views empacotadas no fim do BIN (o padrão gltfpack do
   high_poly_base_mesh.glb: accessor com byteOffset > 0 num view que
   acaba em binLen) era recusado com «bufferView fora do buffer» ainda
   que o layout dos chunks estivesse certo — exatamente o log 1 do dono
   (12+8+2288+8=2316 ✓; 2316+2792640=2794956 ✓).
2. `assets/AssetConverter.cpp` — `convertGlbFile()`: nenhum guard exigia
   que o chunk BIN vivesse INTEIRO no ficheiro (um slice truncado por
   cópia em chunks/fd errado/staging curto apareceria como erro obscuro
   lá no fundo).
3. O caso 2 (stanford_dragon_pbr.glb — a imagem que matava o import) já
   fora curado pelo A1/R-022; o caso 3 (scene.bin — o irmão externo) pelo
   A1/R-021 — re-afervados, sem regressão.

**Fix (as tarefas 1-6 da spec):**
1. O LOG DE COMPARAÇÃO COMPLETO em toda a falha de bufferView — a rotina
   ÚNICA `resolveView` ganha `ViewDiag` e os chamadores logam
   `glb: view<i> buffer<b> off=<o> len=<l> acc=<a> declared=<d> real=<r>`
   (no engine.log E no erro — NUNCA truncado; a tarefa 1).
2. (a) o guard `copiado == total`: o BIN tem de acabar dentro do ficheiro
   (binFim ≤ copiado) + o log
   `glb: copiado=X total=Y binFim=Z — copiado == total (COINCIDEM)`.
3. (b) `real` = o comprimento REAL do chunk BIN (já era a autoridade; o
   log agora mostra declared= do JSON vs real= lado a lado).
4. (c) `bufferView.buffer` inexistente → o erro NOMEIA o índice:
   «refere o buffer 1, que não existe (o ficheiro declara 1 buffer(s))».
5. (d) view que GENUINAMENTE excede o BIN = DEGRADA, não mata: a
   primitiva desse accessor é LARGADA com W + a linha + o contador
   (`primsDropped` → `stats.primWarn` → o toast «K primitiva(s) FORA»);
   todas caírem → o erro final traz a comparação completa. As texturas
   continuam como sempre: falha + geometria ok = .gmesh sem texturas +
   W + toast (nunca silencioso).
6. UMA SÓ rotina de validação para meshes e imagens (a tarefa 3) — por
   construção: ambas passam pelo `resolveView` com a mesma linha.
7. (tarefa 4) os irmãos externos com URI-decode: vivo desde o A1/R-021
   (`copyGltfSiblings` + o log «irmao … copiado») — re-verificado.
8. (tarefa 6) o browser: `isFile` antes do job + 1 toque seleciona — vivo
   desde o A4/0.9.6.4 (`browserImportFile`) — re-verificado.

**Fixtures da spec (proibido fixture «fácil»)** — `glb_a2_perfis_do_device`
(tests/test_import_gltf.cpp), construídas EM CÓDIGO com os números reais:
(i) view que acaba EXATAMENTE em binLen → passa; (i') accessor com
accOff=8 num view flush → passa (o perfil do dono — ANTES falhava);
(ii) len = binLen+1 → falha com a MENSAGEM COMPLETA; (ii-b) duas
primitivas — a má cai, a boa entra (primsDropped=1); (iii) buffer=1 →
erro nomeado; (iv) o perfil STANFORD (imagem embutida no fim do chunk);
(v) o .bin externo e (vi) o URI %20 vivos na R-021 (test_sentinels.cpp).

**A linha que o dono vai ver agora (o formato exato):**

```
glb: view2 buffer0 off=68 len=7 acc=0 declared=74 real=74
```

**Sentinela R-014 reescrita** — `regress_import_r014_reescrita`
(tests/test_sentinels.cpp): o caminho DE FICHEIRO — (a) import →
.gmesh em assets/ → o MESMO listDir que alimenta o picker lista o mesh
novo (o <1s é a ausência de re-hash pesado; o e2e do picker ao device é
a FASE 12.8 do replay); (b) imagem podre → mesh SEM texturas + texWarn +
W; (c) slice BIN truncado de propósito → o guard morre com
binFim/copiado/total; (d) view fora → os quatro números no engine.log.

**Mutações (vermelho → verde):**

| mutação | o que morreu | vermelho | verde |
|---|---|---|---|
| A2-a: irmãos off | a cópia dos irmãos | `regress_gltf_irmaos_do_diretorio_original` FALHOU (8 testes) | reposição → 0 falhas |
| A2-b: binStart sem alinhamento | o alinhamento a 4 | `regress_glb_imagem_no_fim_com_padding` FALHOU (2 testes) | reposição → 0 falhas |
| A2-c: validação só-imagens | o bound do path das MESHES | `glb_a2_perfis_do_device` FALHOU (5 testes) | reposição → 0 falhas |
| A2-d: guard do slice morto | o copiado==total | `regress_import_r014_reescrita` FALHOU (1 teste) | reposição → 0 falhas |

## 7. O GATE HIERARCHY-CHECK (P-08 no CI)

`scripts/hierarchy_check.py` (NOVO, job core-tests depois do theme-hex):
(1) o contrato existe e tem a árvore + as regras; (2) a tabela §0 cita
símbolos REAIS nos ficheiros indicados (11 referências verificadas) —
contrato a MENTIR = vermelho; (3) as sentinelas do contrato (R-022..
R-025) vivem no fonte — sentinela apagada = vermelho.

## 8. OS PNGS (P-05 — a prova visual do harness)

| ficheiro | o que mostra |
|---|---|
| `docs/j2-device-drawer-aberto.png` (NOVO — FASE 14.x) | o editor ao TAMANHO do device (1600×720 @2.0) com o drawer aberto — o estado que produzia os defeitos 1+2+3: a toolbar dentro do rect, a strip com os chips medidos, o rodapé no fundo |
| `docs/layout-harness-editor-device.png` (13.7) | o editor ao device com o drawer fechado (a linha de base) |
| artefactos do CI | os PNGs+JSONs+auditorias de cada ecrã (o job publica) |

O «antes» não pode ser PNG commitado honesto: o bug vivia no device com
o drawer persistido (o harness não o reproduzia — por isso nunca foi
apanhado antes); os números exatos do antes estão nas causas (§2-§6) e
na sentinela (a mutação R-022a reproduz o bug e a sentinela apanha-o).

## 9. O ESTADO DAS SUÍTES

- test_core: 0 falhas (as sentinelas R-022/R-023/R-024/R-025/R-014 e os
  perfis A2 incluídos)
- c33_virtual: 456/456 (as FASEs 14.1/14.2/14.3/14.4/14.x incluídas)
- gates locais: scope-check · docs-lint (54 ficheiros) · theme-hex ·
  hierarchy-check · check_main · link_parity (115 TUs) · jni_parity — VERDES

## 10. CHECKLIST DEVICE (P-07 — o dono preenche no C33; o grupo só fecha com isto)

**Após J1 (hierarquia):**
- [ ] TransformToolbar não cobre o menu superior (com o painel de baixo aberto E fechado)
- [ ] TransformToolbar move-se com o viewport quando painéis inferiores abrem
- [ ] Canvas OpenGL não fica preto/coberto quando abas expandem
- [ ] GLSurfaceView renderiza continuamente (sem flicker)

**Após J2 (texto):**
- [ ] Botão «Global» mostra o texto completo (não «Glob+»)
- [ ] Campo «pesquisar TIC» não colide com a hierarquia
- [ ] Textos usam sp (testar com o tamanho de fonte do sistema aumentado)

**Após J3 (sincronização do viewport):**
- [ ] O log `vp3d: viewport set to` aparece quando um painel inferior abre/fecha (engine.log)
- [ ] O viewport 3D redimensiona suavemente (sem piscar)
- [ ] O aspect ratio da câmara atualiza corretamente (a cena não estica)

**Após J4 (rodapé):**
- [ ] Rodapé fixo no fundo, nunca muda de posição
- [ ] Nenhum painel cobre o rodapé
- [ ] Texto do projeto longo é elipsado (não estoura o layout)

**Sign-off IMPORT A2 (a-d):**
- [ ] (a) `high_poly_base_mesh.glb` importa e aparece no picker e nos Ficheiros
- [ ] (b) `stanford_dragon_pbr.glb` idem (com texturas, ou sem + toast)
- [ ] (c) `scene.gltf` + `scene.bin` idem
- [ ] (d) 1 toque num ficheiro no browser seleciona

## 11. NÃO VERIFICADO (honesto)

1. **O sign-off no device real (P-07) não foi feito** — todos os itens da
   secção 10 estão POR PREENCHER pelo dono. O CI verde + o harness 456/456
   provam o caminho virtual; o device é a vara final.
2. **Os PNGs «antes» dos defeitos não existem como ficheiro** — o bug da
   toolbar vivia no device com o layout.json persistido (drawer 240) e o
   harness não o reproduzia; a reprodução é a mutação R-022a (que a
   sentinela apanha) + os números exatos nas causas.
3. **O feel do «redimensiona suavemente» (sem piscar)** não é afervável
   no harness (o stub rasteriza frame a frame, não tem os olhos do dono).
4. **O high_poly_base_mesh.glb REAL não está no repo** (a spec manda
   commitar se a licença o permitir — os ficheiros não foram fornecidos
   a esta sessão); as fixtures são equivalentes ESTRUTURAIS com os
   números do header do log do dono. Se o dono anexar os ficheiros, a
   suíte ganha-os como fixtures de licença livre.
5. **O sp com a fonte do sistema aumentada** (checklist J2 item 3) depende
   do fontScale do sistema no device — o harness afere a densidade 1.0/2.0
   fixas; a escala de acessibilidade do Android é um NÃO VERIFICADO do
   device.
