#pragma once
// ui/EditorLayout.h — layout PURO dos painéis do editor (F4.1), sem GL —
// host-testável. Fonte ÚNICA das alturas de conteúdo que alimentam o scroll:
// o desenho (EditorUi.cpp) e os testes de CI partilham estes números.
#include "ui/ScrollMath.h"   // UiRect (os overlays F5.2 também partilham)
#include "ui/SafeArea.h"     // 0.6.8: playBarRect usa safe::toolbarRect/kToolbarH
//
// Coordenadas de CONTEÚDO: origem no topo da lista (debaixo do cabeçalho do
// painel); o scroll converte para ecrã com screenY = contentY − offset.
//
// F5.0-fix (bug do C33: texto do Inspector sobreposto em "pilhas"):
// ── CAUSA RAIZ ── as linhas do Inspector tinham alturas FIXAS (26/30 px)
// pensadas para uma fonte pequena, mas o device carrega a fonte a 28 px
// (bloco de glifo ≈ 22 + 6 px). O baseline "+8" fazia o bloco de texto
// começar 10 px ACIMA do topo da linha — cada label invadia a linha de
// cima (tex × input sobrepunham 7 px; velx × tc 3 px; nome × Transform3D
// encostavam). O cursor Y era SEMPRE partilhado e sequencial — a sobreposição
// era dos BLOCOS DE GLIFO, não das linhas. Nenhum teste apanhava: no
// hospedeiro não há fonte (hasFont() = false → texto invisível).
// ── FIX ── (1) as alturas das linhas derivam das MÉTRICAS REAIS da fonte
// (TextMetrics do atlas; fallback 28 px) via inspectorPlan(); (2) o plano é
// a FONTE ÚNICA: o desenho consome as linhas na ordem (y cumulativo, sem um
// único "+=" solto), contentHeight = fundo da última linha, e os testes de
// CI aferem o MESMO plano + a geometria REAL dos glifos.
#include "core/Types.h"
#include <cmath>
#include <cstdio>
#include "ui/Theme.h"      // 0.9.6.1 (PASSO 0): applyDensity() — R-018
#include "ui/FontAtlas.h"   // TextMetrics (GL-free)
#include "core/Scene.h"
#include "components/Transform3D.h"
#include "components/ScriptComp.h"   // 0.9.2: perfil do Inspector
#include "components/MeshRenderer.h"
#include "components/InputMap.h"
#include "components/BodyComp.h"
#include "components/TouchControls.h"
#include "components/CameraComp.h"   // 0.7.7: inspector da câmara
#include "components/AnimationPlayer.h"   // 0.8.0: inspector da animação
#include "components/AudioPlayer.h"   // 0.8.11: inspector de áudio
#include "components/Environment.h"   // 0.12-Y-fix: inspector do WorldEnvironment
#include "components/UiCanvas.h"   // 0.8.0: canAnim (elementos animáveis)

namespace vv {
namespace editor {

// constantes partilhadas pelo desenho e pela medição (antes no anon ns do .cpp)
// 0.9.0 (spec A/B): kPad 12→16 (8-múltiplo), linha 52→48 (alvo mínimo), e a
// hierarquia ganha a LINHA DE PESQUISA (48dp) por baixo do cabeçalho.
// 0.9.6.1 (PASSO 0 · R-018): eram constexpr em DP consumidas como PX — no
// C33 (densidade 2.0) painéis/linhas/cabeçalhos saíam a METADE do dp. São
// agora VARIÁVEIS de runtime em dp REAIS: applyDensity() (abaixo) atualiza
// TODAS num só sítio quando a densidade é conhecida (arranque / mudança de
// AConfiguration); nos testes (densidade 1.0) os valores são os de sempre.
extern f32 kPad;        // 16dp — padding de painéis/linhas
extern f32 kHeaderH;   // PASSO 1: 28dp — cabeçalho dos painéis (era 48)
extern f32 kRowH;      // PASSO 1: 36dp — linha de TIC/inspetor (era 48)
extern f32 kSearchRowH; // PASSO 1: 32dp — pesquisa de TIC (o campo da spec)
extern f32 kMenuW;     // 340dp — largura dos menus/overlays
// atualiza as variáveis acima (e as do teclado do editor de script) a partir
// da densidade corrente — chamada no arranque e em mudanças de config
void applyDensity();

// altura do conteúdo da Hierarchy: uma linha de 48dp por TIC VISÍVEL na
// árvore (0.9.0: o filtro de pesquisa encolhe o conteúdo — o scroll segue)
inline f32 hierarchyContentHeight(u32 ticCount) {
    return static_cast<f32>(ticCount) * kRowH;
}

// 0.9.0 — ids novos da hierarquia (faixa 5500..5599, livre entre a câmara
// 54xx e o áudio 57xx): campo de pesquisa, chip da multi-seleção
constexpr u64 kHierSearchId    = 5500;   // linha de pesquisa (abre teclado 8)
constexpr u64 kHierMultiClearId = 5501;  // chip "N ×" — limpa a multi-seleção
constexpr u64 kHierMultiDistId = 5502;   // "distribuir" (≥3 selecionados)
// 0.12-QoL A: o × dentro do campo EXPANDIDO (limpa + fecha a lupa)
constexpr u64 kHierSearchClearId = 5503;

// 0.12-C (C-1) — O CABEÇALHO da hierarquia em linha PRÓPRIA: o título à
// esquerda, as AÇÕES (⋮ e +) à direita, e o título ELIPSA quando não cabe
// (o dono: «os botões não sobrepõem o título»). FONTE ÚNICA da geometria
// do cabeçalho — o draw (EditorUi.cpp) e os pins partilham ESTA função;
// as X são OFFSETS dentro do painel (o draw soma o x do painel).
struct HierHeaderRects {
    f32 titleX;      // onde o título começa (offset)
    f32 titleMaxW;   // budget do título (0 = não cabe nada; nunca sobrepõe)
    UiRect dots;     // o ⋮ (offsets; y = topo do painel)
    UiRect plus;     // o + (offsets)
};
// 0.12-C (C-4) — o convite do vazio da HIERARQUIA (a FONTE ÚNICA — o draw
// e o pin empty_hierarquia partilham): a LADDER mede ao vivo (o mais
// comprido que caiba; no piso 140dp sai «+ cria um TIC» — nunca reticência
// num convite). A MENSAGEM aponta para o + e o + fica VISÍVEL no cabeçalho
// (hierHeaderRects — o pin aferra os DOIS lados do contrato)
inline const char* hierEmptyInvite(bool searching, bool fitsFull,
                                   bool fitsMid) {
    if (searching) {
        return fitsFull ? "nenhum TIC com esse nome" : "sem resultados";
    }
    return fitsFull ? "sem TICs - toca em + para criar"
                    : (fitsMid ? "toca em + para criar" : "+ cria um TIC");
}

inline HierHeaderRects hierHeaderRects(f32 panelW) {
    const f32 hdrBtn = kHeaderH;   // 28dp — o alvo é a LINHA do cabeçalho
    HierHeaderRects r;
    r.plus = {panelW - kPad - hdrBtn, 0.0f, hdrBtn, hdrBtn};
    r.dots = {r.plus.x - hdrBtn, 0.0f, hdrBtn, hdrBtn};
    r.titleX = kPad;
    // o título vive entre o padding e 8dp antes do ⋮ (o MESMO respiro
    // do hierChipSlot); em painel estreito o budget encolhe e o título
    // ELIPSA — os botões NUNCA perdem o lugar nem cobrem o título
    r.titleMaxW = r.dots.x - theme::dp(8.0f) - kPad;
    if (r.titleMaxW < 0.0f) {
        r.titleMaxW = 0.0f;
    }
    return r;
}

// GRUPO D (0.9.6.7) — DIVISORES arrastáveis hierarquia|viewport|inspector.
// Pegas de arrasto (o padrão da pega do drawer): strip VISUAL de 12dp na
// borda do painel + zona de toque de 20dp (12 do strip + 8 de folga PARA
// DENTRO do painel — nunca para o lado do viewport: um toque na pega nunca
// orbita a câmara nem agarra o gizmo). As regiões de scroll dos painéis
// INSETAM os mesmos 20dp: o toque na pega nunca vira scroll. NÃO são
// alvos de tap (não se registam no audit — o alvo 48dp da casa é para
// TAP; a pega do drawer de 12dp é o precedente spec E).
constexpr u64 kIdDividerL = 5503;   // pega do divisor da HIERARQUIA
constexpr u64 kIdDividerR = 5504;   // pega do divisor do INSPECTOR
constexpr f32 kDividerStripW = 12.0f;   // dp — o strip visível
// PASSO 2 (0.9.6.15): a PEGA de arrasto tem 24dp (era 20 — spec do dono;
// a pega do drawer também subiu 12→24). O strip VISÍVEL segue 12dp — os
// 12dp restantes do hit são folga PARA DENTRO do painel (nunca para o
// viewport: um toque na pega está sempre fora do centerRect).
constexpr f32 kDividerHitW   = 24.0f;   // dp — a zona de toque da pega

// 0.9.0 (spec C) — SECÇÕES COLAPSÁVEIS do Inspector: cabeçalho (PASSO 1:
// 28dp) com título + chevron; o estado (bitmask por secção) vive no
// EditorState e PERSISTE (spec G). Bits: 0=Transform 1=Camera 2=Malha
// 3=Material 4=Fisica 5=Audio 6=Anim. Os botões R (repõe a LINHA
// pos/rot/scale) partilham a faixa 5510+.
constexpr u64 kInspSectionBase = 5510;   // +bit da secção
constexpr u64 kInspResetBase   = 5520;   // +0/1/2 = repõe Pos/Rot/Escala
// caixas X/Y/Z das linhas de Transform (9 caixas: 3 linhas × 3 eixos) — o
// índice É o campo do teclado numérico (purpose 6, textElement 0..8)
constexpr u64 kInspFieldBase   = 5530;   // +0..8

// ---- ids dos widgets do Inspector (antes no anon ns do EditorUi.cpp —
// o plano e o hit-test do tap re-despachado partilham-nos)
constexpr u64 kInspectorSliderBase = 2000;   // 9 sliders do Transform3D
constexpr u64 kInspectorVelX       = 2100;   // slider velx do BodyComp
constexpr u64 kInspectorAddTc      = 3001;   // botão "add TouchControls"
constexpr u64 kInspectorMeshSel    = 5001;   // F5-E: linha "mesh: …"
constexpr u64 kInspectorTexSel     = 5002;   // F5-E: linha "tex: …"
// 0.10-M HOTFIX TEXTURAS-VINCULA: as 5 linhas de SLOT na secção Material
// (payload = kInspectorTexSlot0 + slot; o pick do browser escreve O SLOT
// escolhido — os irmãos ficam intactos, o pin da auditoria de estado)
constexpr u64 kInspectorTexSlot0   = 5011;   // +0 baseColor … +4 occlusion
// 0.10-M HOTFIX TEXTURAS-MULTIMAT: o SELETOR de material «material: i/N»
// (a linha só existe com N>1; o tap no terço esquerdo = ‹, no direito = ›)
constexpr u64 kInspectorMatSel     = 5311;
constexpr u64 kInspectorVis        = 5200;   // 0.7.0: "visivel: sim/nao"
constexpr u64 kInspectorColBase    = 5300;   // 0.7.0: sliders R/G/B (+i)
constexpr u64 kInspectorColHex     = 5303;   // 0.8.6: "hex: #RRGGBB" do tint
// 0.8.9 — NORMALIZAÇÃO DE IMPORT: dims originais + repõe a escala {1,1,1}
// (o fit uniforme vive no Transform3D — este botão devolve o tamanho real)
constexpr u64 kInspectorScaleOrig  = 5310;   // "escala: original"
constexpr u64 kInspectorLodDist    = 5311;   // 0.11-T (3.5a): slider do LOD
constexpr u64 kInspectorInstancing = 5312;   // 0.11-W (3.5b): toggle do
                                             // instancing (sim/não)
constexpr u64 kInspectorStreaming  = 5313;   // 0.11-W (3.75a): toggle do
                                             // streaming (sim/não)
constexpr u64 kInspectorImpostorDist = 5314; // 0.11-W (3.75b): slider do
                                             // impostor (0 = off)
// 0.7.7 — inspector da CÂMARA (sliders + botões; faixa 5400..5419)
constexpr u64 kInspectorCamFov     = 5400;
constexpr u64 kInspectorCamNear    = 5401;
constexpr u64 kInspectorCamFar     = 5402;
constexpr u64 kInspectorCamOrtho   = 5403;
constexpr u64 kInspectorCamProj    = 5410;
constexpr u64 kInspectorCamActive  = 5411;
constexpr u64 kInspectorCamFrustum = 5412;   // 0.7.10: toggle do gizmo

// 0.12-Y-fix — inspector do WORLDENVIRONMENT (faixa 5420..5439; a câmara
// vive em 5400..5412, os fields do Transform em 5530+ — zero colisões;
// 3.Y-D acrescenta o tipo de céu + os 5 campos procedurais 5425..5430)
constexpr u64 kInspectorEnvSky  = 5420;   // "céu: #RRGGBB" (teclado hex 11)
constexpr u64 kInspectorEnvHdri = 5421;   // "hdri: …" (o NAVEGADOR)
constexpr u64 kInspectorEnvAmb  = 5422;   // slider ambiente 0..2
constexpr u64 kInspectorEnvFogD = 5423;   // slider névoa 0..1
constexpr u64 kInspectorEnvFogC = 5424;   // "névoa: #RRGGBB" (teclado hex 12)
// 0.12-Y (3.Y-D) — O CÉU PROCEDURAL (o dropdown + os 5 campos)
constexpr u64 kInspectorEnvSkyType = 5425; // "céu: cor sólida|procedural"
constexpr u64 kInspectorEnvHor  = 5426;   // "horizonte: #RRGGBB" (hex 13)
constexpr u64 kInspectorEnvZen  = 5427;   // "zénite: #RRGGBB" (hex 14)
constexpr u64 kInspectorEnvSunC = 5428;   // "sol: #RRGGBB" (hex 15)
constexpr u64 kInspectorEnvCov  = 5429;   // slider nuvens 0..1
constexpr u64 kInspectorEnvSpd  = 5430;   // slider vento 0..0.2
// 0.8.0 (F7) — primitiva procedural do MeshRenderer + animação
constexpr u64 kInspectorPrimSel   = 5003;   // "prim: esfera ▸" (seletor)
constexpr u64 kInspectorPrimR     = 5600;   // slider raio/size
constexpr u64 kInspectorPrimSeg   = 5602;   // slider segmentos (esfera)
constexpr u64 kInspectorPrimRings = 5604;   // 0.8.10: slider anéis (esfera)
constexpr u64 kInspectorAddAnim   = 3060;   // botão "adicionar Animação"
// 0.8.11 — inspector de ÁUDIO (faixa 5700..5719): clip (seletor 5),
// ouvir (preview), autoplay/loop/posicional (toggles), volume/pitch e
// raios (sliders)
constexpr u64 kInspectorAuClip     = 5700;   // "clip: <nome> ▸" (assetMenu 5)
constexpr u64 kInspectorAuPlay     = 5701;   // "ouvir/parar" (preview)
constexpr u64 kInspectorAuAutoplay = 5702;   // "autoplay: sim/nao"
constexpr u64 kInspectorAuLoop     = 5703;   // "loop: sim/nao"
constexpr u64 kInspectorAuVolume   = 5704;   // slider 0..1
constexpr u64 kInspectorAuPitch    = 5705;   // slider 0.5..2
constexpr u64 kInspectorAuPos      = 5706;   // "posicional: sim/nao"
constexpr u64 kInspectorAuRint     = 5707;   // slider raio interno
constexpr u64 kInspectorAuRext     = 5708;   // slider raio externo

// ids das regiões de scroll (F4.1) — o tap re-despachado é POR ID (F5.0-fix:
// a Hierarchy comia o tap do Inspector quando a consulta era global)
constexpr u64 kHierarchyScrollId = 41;
constexpr u64 kInspectorScrollId = 42;
constexpr u64 kLogsScrollId      = 43;   // F5.2: viewer de logs in-app

// ---- Inspector: alturas derivadas das MÉTRICAS DA FONTE (F5.0-fix) ---------
// Bloco de texto = ascent + descent (reais do atlas). Cada linha acrescenta
// a folga mínima para o bloco caber INTEIRO dentro da linha — nunca mais
// glifos a invadir a linha vizinha.
// 0.9.6.6 (GRUPO C): as alturas das linhas do Inspector derivam das
// MÉTRICAS DO CONTEXTO (sp — textK incluído) com paddings em dp e PISO
// kRowH (PASSO 1: 36dp — a linha da spec; a auditoria vigia o TOQUE no
// piso 40dp/um eixo — LayoutDump.cpp). ANTES: bloco do atlas CRU + paddings px — no device (2.0) as linhas
// saíam a ~18dp (a exata classe R-018 que o dono mediu nas teclas); a
// 1.0 com o textK novo teriam ~22px. O piso garante o alvo em QUALQUER
// densidade; quando o texto manda (densidades altas com fontes grandes)
// a linha CRESCE — nunca espreme o texto.
inline f32 inspRowHFloor(f32 contentH) {
    return contentH < kRowH ? kRowH : contentH;
}
inline f32 inspTextRowH(const TextMetrics& m) {
    return inspRowHFloor(m.block() + theme::dp(6.0f));   // labels
}
inline f32 inspButtonRowH(const TextMetrics& m) {
    return inspRowHFloor(m.block() + theme::dp(8.0f));   // botões
}
inline f32 inspSliderRowH(const TextMetrics& m) {
    return inspRowHFloor(m.block() + theme::dp(8.0f));   // sliders
}
inline f32 inspAddTcH(const TextMetrics& m) {
    return inspRowHFloor(m.block() + theme::dp(14.0f));  // botão do fundo
}

// baseline CENTRADA do bloco de texto dentro da linha (topo = baseline −
// ascent, fundo = baseline + descent) — substitui a convenção "+8" que
// provocava a invasão de 10 px acima do topo da linha.
inline f32 inspBaseline(f32 rowTop, f32 rowH, const TextMetrics& m) {
    return rowTop + (rowH - m.block()) * 0.5f + m.ascent;
}

// ---- perfil de componentes do TIC (a presença que o plano reflete) --------
struct InspProfile {
    bool tr = false;   // Transform3D  (cabeçalho + 9 sliders)
    bool cam = false;  // 0.7.7: Camera (cabeçalho + fov/near/far/proj/ortho/ativa)
    bool mr = false;   // MeshRenderer  (linhas mesh/tex)
    bool im = false;   // InputMap      (linha input + addTc/tc)
    bool bc = false;   // BodyComp      (linha body + slider velx)
    bool tc = false;   // TouchControls (muda addTc ↔ label tc)
    // 0.8.0 (F7)
    bool prim = false;      // MeshRenderer com PRIMITIVA ativa (primOn)
    PrimKind primKind = PrimKind::Sphere;
    bool anim = false;      // AnimationPlayer presente
    bool canAnim = false;   // tem ALVO animável (Transform3D ou UI com elems)
    // 0.8.11 — áudio
    bool au = false;        // AudioPlayer presente (secção + linhas)
    // 0.9.2 — V.ONI: script presente + nº de vars @+ (o chamador injeta a
    // contagem via VoniSystem — o profile sozinho não a sabe)
    bool script = false;
    u32 scriptExports = 0;
    // 0.8.9 — IMPORT DE FICHEIRO: linhas dims + "escala: original" (o fit
    // uniforme normaliza o TAMANHO; estes mostram/devolvem o original)
    bool fileMesh = false;
    // 0.12-Y-fix: o WorldEnvironment presente (secção Environment com os
    // 5 controlos — céu/hdri/ambiente/névoa/cor da névoa)
    bool env = false;
    // 0.12-Y (3.Y-D): o TIPO de céu do Environment (0 = sólida, 1 =
    // procedural — o plano é CONDICIONAL a isto: o M-Yd2 vive aqui)
    u32 envSkyType = 0;
    // 0.10-M HOTFIX TEXTURAS-MULTIMAT: nº de materiais do mesh do TIC (o
    // seletor «material: i/N» só entra com >1 — a grade do MeshRenderer é
    // a fonte, zero I/O no caminho quente)
    u32 matCount = 1;
};

inline InspProfile inspectorProfile(const Tic& tic) {
    InspProfile p;
    p.tr = tic.getComponent<Transform3D>() != nullptr;
    p.mr = tic.getComponent<MeshRenderer>() != nullptr;
    p.im = tic.getComponent<InputMap>() != nullptr;
    p.bc = tic.getComponent<BodyComp>() != nullptr;
    p.tc = tic.getComponent<TouchControls>() != nullptr;
    p.cam = tic.getComponent<CameraComp>() != nullptr;   // 0.7.7
    p.anim = tic.getComponent<AnimationPlayer>() != nullptr;   // 0.8.0
    p.au = tic.getComponent<AudioPlayer>() != nullptr;   // 0.8.11
    p.script = tic.getComponent<ScriptComp>() != nullptr;   // 0.9.2
    p.env = tic.getComponent<Environment>() != nullptr;   // 0.12-Y-fix
    if (const Environment* envP = tic.getComponent<Environment>()) {
        p.envSkyType = envP->skyType;   // 0.12-Y (3.Y-D): o plano condiciona
    }
    if (const MeshRenderer* mr = tic.getComponent<MeshRenderer>()) {
        p.prim = mr->primOn;   // 0.8.0
        p.primKind = mr->prim.kind;
        // 0.8.9: mesh DE FICHEIRO (não prim, ref por meshPath) → dims+escala
        p.fileMesh = !mr->primOn && !mr->meshPath.empty();
        // 0.10-M TEXTURAS-MULTIMAT: os materiais que a grade do TIC conhece
        p.matCount = mr->matCount();
    }
    if (const UiCanvas* uic = tic.getComponent<UiCanvas>()) {
        p.canAnim = !uic->elements.empty();
    }
    p.canAnim = p.canAnim || p.tr;
    return p;
}

// 0.8.10 — que sliders de PRIMITIVA o tipo usa (SÓ esfera e box vivem):
// esfera → raio + segmentos + anéis; box → tamanho. Os helpers antigos
// (primUsesHeight/primUsesTube) MORRERAM com o cilindro/cone/torus/cápsula.
inline bool primUsesSegments(PrimKind k) {
    return k == PrimKind::Sphere;
}
inline bool primUsesRings(PrimKind k) {
    return k == PrimKind::Sphere;
}

// ---- o PLANO do Inspector — FONTE ÚNICA do layout --------------------------
// Linhas em ORDEM, com y CUMULATIVO em coords de conteúdo (o desenho só
// subtrai o offset do scroll; nenhum consumidor faz "+=" próprio).
struct InspRow {
    enum class Kind : u8 {
        Name,        // nome do TIC (accent)
        Section,     // cabeçalho COLAPSÁVEL 48dp (payload = bit da secção)
        TransformRow,// 0.9.0 (spec C): linha Pos/Rotação/Escala com caixas
                     // X/Y/Z 48dp + botão R (payload = 0/1/2)
        MaterialThumbs, // 0.9.0 (spec C): 3 miniaturas 64dp (textura,
                     // albedo+lápis, preview live = textura × tint)
        Slider,      // slider do Transform3D (9× — payload por índice)
        MeshButton,  // F5-E: "mesh: …" (selecionável se houver catálogo)
        MeshLabel,   // F5-E: "mesh: …" só leitura (sem catálogo)
        TexButton,   // F5-E: "tex: …" (selecionável)
        TexLabel,    // F5-E: "tex: …" só leitura
        TexSlotRow,  // 0.10-M HOTFIX: o slot do material (payload = slot
                     // 0..4 — kInspectorTexSlot0+i; o pick escreve O slot)
        MatSelRow,   // 0.10-M TEXTURAS-MULTIMAT: "material: i/N" com ‹ ›
                     // (terço esquerdo/direito da linha; N>1 é que entra)
        Label,       // linha de texto (input/tc)
        TwoCol,      // FASE 9 (G2-8): duas colunas nome/valor (Física) —
                     // payload 0=tipo, 1=forma, 2=no chão (BodyComp VIVO)
        Velx,        // slider velx do BodyComp
        AddTc,       // botão "add TouchControls" no fundo
        VisToggle,   // 0.7.0: "visivel: sim/nao" (checkbox do TIC)
        ColorSlider, // 0.7.0: sliders R/G/B do tint do MeshRenderer
        ColorHex,    // 0.8.6: "hex: #RRGGBB" do tint (abre o teclado hex)
        // 0.7.7 — câmara de cena
        CamSection,  // cabeçalho "Camera" + separador
        CamFov,      // slider fov (graus, 1..170)
        CamNear,     // slider near
        CamFar,      // slider far
        CamProj,     // botão "projecao: perspetiva|ortografica" (cicla)
        CamOrtho,    // slider orthoSize (meia-altura)
        CamActive,   // botão "ativa: sim|nao" (UMA ativa por cena)
        CamFrustum,  // 0.7.10: botão "frustum: sim|nao" (toggle do gizmo)
        // 0.8.0 (F7) — primitiva procedural + animação
        PrimButton,  // "prim: esfera ▸" (abre o seletor de primitivas)
        PrimSlider,  // slider de parâmetro (payload pelo id: R/Seg/Rings)
        AddAnim,     // botão "adicionar Animação" (cria o AnimationPlayer)
        AnimLabel,   // "anim: N tracks" (a edição vive na timeline)
        // 0.8.9 — import: dimensões originais + repor a escala
        DimsLabel,   // "dims: X×Y×Z" (AABB real do mesh carregado)
        ScaleOrig,   // "escala: original" (repõe Transform3D::scale a {1,1,1})
        LodDistance, // 0.11-T (FASE 3.5a): "lod distance: N u" (0 = off —
                     // o default jogável manda; slider 0..500)
        Instancing,  // 0.11-W (3.5b): "instancing: sim/não" (o toggle do
                     // fundo de blocos — default ON, visualmente idêntico)
        Streaming,   // 0.11-W (3.75a): "streaming: sim/não" (o toggle do
                     // fill por camadas — default ON; o OFF devolve o
                     // freeze de sempre)
        ImpostorDistance, // 0.11-W (3.75b): "impostor: N u" (0 = off —
                          // o PNG alfa por distância; slider 0..500)
        // 0.8.11 — ÁUDIO (secção do AudioPlayer)
        AuSection,   // cabeçalho "Audio" + separador
        ScriptEdit,  // 0.9.2: botão "Editar script" (abre o editor modal)
        ScriptAdd,   // 0.9.2: botão "Adicionar script" (cria ScriptComp)
        ScriptVar,   // 0.9.2: variável @+ exportada (payload = índice)
        AuClip,      // "clip: <nome> ▸" (abre o seletor de clips — menu 5)
        AuPlay,      // "ouvir/parar" (preview — flag que o main mapeia)
        AuAutoplay,  // "autoplay: sim/nao" (toca ao entrar em Play)
        AuLoop,      // "loop: sim/nao"
        AuVolume,    // slider volume 0..1
        AuPitch,     // slider pitch 0.5..2 (resample linear)
        AuPos,       // "posicional: sim/nao" (atenuação pela distância)
        AuRint,      // slider raio interno (ouvido a 100%)
        AuRext,      // slider raio externo (silêncio)
        // 0.12-Y-fix — WORLDENVIRONMENT (a secção Environment)
        EnvSky,      // "céu: #RRGGBB" + swatch (teclado hex, propósito 11)
        EnvHdri,     // "hdri: <nome>" (abre o NAVEGADOR .png/.hdr)
        EnvAmb,      // slider ambiente 0..2 (o multiplicador do ambient)
        EnvFogD,     // slider névoa (densidade exp2) 0..1 (0 = morta)
        EnvFogC,     // "névoa: #RRGGBB" + swatch (teclado hex, propósito 12)
        // 0.12-Y (3.Y-D) — O CÉU PROCEDURAL (o dropdown + os 5 campos;
        // os 5 SÓ entram com skyType == 1 — o M-Yd2 vive nesta condição)
        EnvSkyType,  // "céu: cor sólida|procedural" (o ciclar do CamProj)
        EnvHor,      // "horizonte: #RRGGBB" + swatch (teclado hex 13)
        EnvZen,      // "zénite: #RRGGBB" + swatch (teclado hex 14)
        EnvSunC,     // "sol: #RRGGBB" + swatch (teclado hex 15)
        EnvCov,      // slider nuvens 0..1 (a cobertura)
        EnvSpd,      // slider vento 0..0.2 (a velocidade)
    };
    Kind kind;
    f32  y;     // topo da linha em COORDS DE CONTEÚDO (cumulativo)
    f32  h;     // altura da linha (derivada das métricas da fonte)
    u64  id;    // id do widget interativo (0 = só desenho)
    u32  payload = 0;   // 0.9.0: bit da secção (Section*) ou campo 0..8
                        // (TransformRow) — o desenho/hit-test lê daqui
};

// payload do ColorSlider por índice (0=R, 1=G, 2=B) — o desenho e o
// hit-test partilham esta ordem
//
// ---- 0.10-M HOTFIX TEXTURAS-MULTIMAT — A HONESTIDADE DOS SLOTS ------------
// O bug do device: os 4 slots não-baseColor APARECEM preenchidos no
// inspector mas o shader lit de hoje SÓ amostra o baseColor — "aparecem
// mas não influenciam". A cura: cada linha de slot não-renderizado com
// ref mostra o SUFIXO HONESTO (a promessa da 0.11-L fica no ecrã); o
// baseColor NUNCA leva sufixo (ele renderiza de verdade). NUNCA mostrar
// slot preenchido como se renderizasse quando não renderiza.
constexpr const char* kTexSlotPendingRenderSuffix =
    " (vinculado \xc2\xb7 render na 0.11-L)";   // · = U+00B7 (no atlas)

// O slot `slot` RENDERIZA hoje? (o lit de hoje tem SÓ o unit 0/baseColor;
// os samplers 1-4 chegam na 0.11-L — a fronteira do relatório do hotfix)
inline bool texSlotRenders(u32 slot) {
    return slot == 0;
}

// o rótulo da linha de slot (FONTE ÚNICA do draw e dos pins — a mutação
// do sufixo morre AQUI): "<slot>: <base>" + o sufixo honesto quando o
// slot está preenchido e não renderiza; vazio = "<slot>: —"
inline void inspTexSlotLabel(char* out, size_t sz, const char* slotName,
                             u32 slot, const char* base) {
    if (base != nullptr && base[0] != '\0') {
        if (texSlotRenders(slot)) {
            std::snprintf(out, sz, "%s: %s", slotName, base);
        } else {
            std::snprintf(out, sz, "%s: %s%s", slotName, base,
                          kTexSlotPendingRenderSuffix);
        }
    } else {
        std::snprintf(out, sz, "%s: —", slotName);
    }
}
//
// 0.9.0 (spec C) — BITS das secções colapsáveis do Inspector (o bitmask
// vive no EditorState::inspCollapsed e PERSISTE — spec G):
//   bit0 Transform · bit1 Camera · bit2 Malha · bit3 Material · bit4 Fisica
//   bit5 Audio · bit6 Animacao
constexpr u32 kInspBitTransform = 1u << 0;
constexpr u32 kInspBitCamera    = 1u << 1;
constexpr u32 kInspBitMalha     = 1u << 2;
constexpr u32 kInspBitMaterial  = 1u << 3;
constexpr u32 kInspBitFisica    = 1u << 4;
constexpr u32 kInspBitAudio     = 1u << 5;
constexpr u32 kInspBitAnim      = 1u << 6;
constexpr u32 kInspBitScript    = 1u << 7;   // 0.9.2: V.ONI
constexpr u32 kInspBitEnvironment = 1u << 8; // 0.12-Y-fix: WorldEnvironment

// altura de UMA linha de Transform (PASSO 1: título 12sp + caixas 32dp +
// folga — a spec: «campos 32dp»; eram caixas de 48). 0.12-QoL A: a folga
// sobe 4→8dp (a ordem do dono: mais ar entre as linhas X/Y/Z e os botões).
inline f32 inspTransformRowH(const TextMetrics& m) {
    (void)m;
    return theme::dp(24.0f) + theme::dp(4.0f) + theme::dp(32.0f) +
           theme::dp(8.0f);   // título + caixas 32dp + folga (QoL A: 8)
}
// altura do CABEÇALHO de secção (28dp — PASSO 1: os cabeçalhos dos painéis;
// eram 48)
inline f32 inspSectionH() { return theme::dp(28.0f); }
// altura da linha de MINIATURAS de Material (PASSO 1: 44dp — a lei «nada
// ≥48»; eram 64) + legendas 12sp em LINHA RESERVADA
inline f32 inspThumbsH() {
    // PASSO 2 (0.9.6.15): o plano reserva o QUE O DRAW CONSOME —
    // dp(4) topo + miniatura dp(44) + a linha da legenda sp(12) + dp(4)
    // de respiro (antes: dp(44)+sp(12) e o draw punha a legenda DENTRO da
    // linha seguinte — a colisão de glifos que o test_ui apanhou a 260dp)
    return theme::dp(4.0f) + theme::dp(44.0f) + theme::sp(theme::kFontCaption) +
           theme::dp(4.0f);
}

inline u32 inspectorRowCount(const InspProfile& p, bool selectable,
                             u32 collapsed) {
    u32 n = 2;                                          // nome + visivel
    // ---- TRANSFORM (bit0): cabeçalho + 3 linhas Pos/Rot/Escala ----------
    n += 1;
    if (!(collapsed & kInspBitTransform)) {
        n += 3;
    }
    // ---- CAMERA (bit1) ----
    if (p.cam) {
        n += 1;
        if (!(collapsed & kInspBitCamera)) {
            n += 7;
        }
    }
    // ---- MALHA (bit2): mesh/prim/params/tex/dims ------------------------
    if (p.mr) {
        n += 1;
        if (!(collapsed & kInspBitMalha)) {
            n += 2 + 1;                                  // mesh + prim + tex
            if (p.prim) {
                n += 1;
                if (primUsesSegments(p.primKind)) n += 1;
                if (primUsesRings(p.primKind)) n += 1;
            }
            if (p.fileMesh) n += 2;                      // dims + escala orig
        }
    }
    // ---- MATERIAL (bit3): miniaturas + R/G/B + hex (+swatch na linha hex)
    // 0.10-M TEXTURAS-MULTIMAT: o seletor «material: i/N» (N>1)
    if (p.mr) {
        n += 1;
        if (!(collapsed & kInspBitMaterial)) {
            if (p.matCount > 1) {
                n += 1;        // o seletor de material (a linha própria)
            }
            n += 1;            // 0.9.0: 3 miniaturas 64dp (spec C)
            n += 3 + 1;
        }
    }
    // ---- FÍSICA (bit4): body + velx (massa/gravidade NÃO existem — zero
    // física nova; a secção fica pronta para eles) ------------------------
    if (p.bc) {
        n += 1;
        if (!(collapsed & kInspBitFisica)) {
            n += 2;
        }
    }
    if (p.im) n += 1;                                    // input (avulso)
    if (p.im) n += 1;                                    // addTc OU tc
    // ---- ANIMAÇÃO (bit6) ----
    if (!p.anim && p.canAnim) {
        n += 1;
        if (!(collapsed & kInspBitAnim)) {
            n += 1;
        }
    } else if (p.anim) {
        n += 1;
        if (!(collapsed & kInspBitAnim)) {
            n += 1;
        }
    }
    // ---- ÁUDIO (bit5) ----
    if (p.au) {
        n += 1;
        if (!(collapsed & kInspBitAudio)) {
            n += 9;
        }
    }
    // ---- ENVIRONMENT (bit8 — 0.12-Y-fix): o WorldEnvironment -------------
    if (p.env) {
        n += 1;
        if (!(collapsed & kInspBitEnvironment)) {
            n += 1;   // o tipo de céu (3.Y-D — o dropdown sempre visível)
            if (p.envSkyType == Environment::kSkyTypeProcedural) {
                n += 5;   // horizonte + zénite + sol + nuvens + vento
            }
            n += 5;   // céu + hdri + ambiente + névoa + cor da névoa
        }
    }
    // ---- SCRIPT V.ONI (bit7 — 0.9.2): secção SEMPRE presente ----------
    n += 1;                                    // cabeçalho
    if (!(collapsed & kInspBitScript)) {
        n += 1;                                // Editar / Adicionar
        n += p.scriptExports < 8 ? p.scriptExports : 8;   // vars @+
    }
    (void)selectable;
    return n;
}

// constrói o plano (rows deve ter capacidade inspectorRowCount; devolve count)
// 0.9.0: `collapsed` = bitmask das secções FECHADAS (só o cabeçalho entra)
inline u32 inspectorPlan(const InspProfile& p, const TextMetrics& m,
                         bool selectable, u32 collapsed, InspRow* rows) {
    const f32 textH = inspTextRowH(m);
    const f32 btnH  = inspButtonRowH(m);
    const f32 sldH  = inspSliderRowH(m);
    const f32 addH  = inspAddTcH(m);
    const f32 secH  = inspSectionH();
    const f32 trfH  = inspTransformRowH(m);

    u32 n = 0;
    f32 y = 0.0f;
    auto push = [&](InspRow::Kind kind, f32 h, u64 id, u32 payload = 0) {
        rows[n].kind = kind;
        rows[n].y    = y;
        rows[n].h    = h;
        rows[n].id   = id;
        rows[n].payload = payload;
        ++n;
        y += h;   // ← o ÚNICO avanço de cursor: y += altura_linha
    };

    push(InspRow::Kind::Name, textH, 0);
    push(InspRow::Kind::VisToggle, btnH, kInspectorVis);
    // ---- TRANSFORM (spec C): cabeçalho + 3 linhas X/Y/Z + R -------------
    push(InspRow::Kind::Section, secH, kInspSectionBase + 0, kInspBitTransform);
    if (!(collapsed & kInspBitTransform)) {
        for (u32 i = 0; i < 3; ++i) {
            push(InspRow::Kind::TransformRow, trfH, kInspResetBase + i, i);
        }
    }
    // ---- CAMERA ----
    if (p.cam) {
        push(InspRow::Kind::CamSection, secH, kInspSectionBase + 1,
             kInspBitCamera);
        if (!(collapsed & kInspBitCamera)) {
            push(InspRow::Kind::CamFov, sldH, kInspectorCamFov);
            push(InspRow::Kind::CamNear, sldH, kInspectorCamNear);
            push(InspRow::Kind::CamFar, sldH, kInspectorCamFar);
            push(InspRow::Kind::CamProj, btnH, kInspectorCamProj);
            push(InspRow::Kind::CamOrtho, sldH, kInspectorCamOrtho);
            push(InspRow::Kind::CamActive, btnH, kInspectorCamActive);
            push(InspRow::Kind::CamFrustum, btnH, kInspectorCamFrustum);
        }
    }
    // ---- MALHA (spec C) ----
    if (p.mr) {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 2, kInspBitMalha);
        if (!(collapsed & kInspBitMalha)) {
            push(selectable ? InspRow::Kind::MeshButton : InspRow::Kind::MeshLabel,
                 selectable ? btnH : textH, selectable ? kInspectorMeshSel : 0);
            push(InspRow::Kind::PrimButton, btnH, kInspectorPrimSel);
            if (p.prim) {
                push(InspRow::Kind::PrimSlider, sldH, kInspectorPrimR);
                if (primUsesSegments(p.primKind)) {
                    push(InspRow::Kind::PrimSlider, sldH, kInspectorPrimSeg);
                }
                if (primUsesRings(p.primKind)) {
                    push(InspRow::Kind::PrimSlider, sldH, kInspectorPrimRings);
                }
            }
            push(selectable ? InspRow::Kind::TexButton : InspRow::Kind::TexLabel,
                 selectable ? btnH : textH, selectable ? kInspectorTexSel : 0);
            if (p.fileMesh) {
                push(InspRow::Kind::DimsLabel, textH, 0);
                push(InspRow::Kind::ScaleOrig, btnH, kInspectorScaleOrig);
                // 0.11-T (FASE 3.5a): a distância do LOD por TIC (o default
                // é OFF — o slider nasce a 0; o .gmesh sem cauda GLOD
                // ignora o valor e o label mantém-se honesto)
                push(InspRow::Kind::LodDistance, sldH, kInspectorLodDist);
                // 0.11-W (3.5b): o fundo do instancing por TIC (o default
                // é ON — visualmente idêntico, o custo é que muda)
                push(InspRow::Kind::Instancing, btnH, kInspectorInstancing);
                // 0.11-W (3.75a): o fill por camadas por TIC (o default é
                // ON — a cura do freeze; o OFF é o caminho de sempre)
                push(InspRow::Kind::Streaming, btnH, kInspectorStreaming);
                // 0.11-W (3.75b): a distância do impostor por TIC (o
                // default é OFF — o slider nasce a 0; o PNG nasce sob
                // demanda na 1.ª vez que o TIC fica longe)
                push(InspRow::Kind::ImpostorDistance, sldH,
                     kInspectorImpostorDist);
            }
        }
    }
    // ---- MATERIAL (spec C): R/G/B + hex + swatch ----
    if (p.mr) {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 3,
             kInspBitMaterial);
        if (!(collapsed & kInspBitMaterial)) {
            // 0.10-M HOTFIX TEXTURAS-MULTIMAT: o SELETOR de material (só
            // com N>1 — o mesh de 1 material NÃO ganha a linha; o tap no
            // terço ‹/› troca e as 5 linhas de baixo mostram O MATERIAL)
            if (p.matCount > 1) {
                push(InspRow::Kind::MatSelRow, btnH, kInspectorMatSel);
            }
            push(InspRow::Kind::MaterialThumbs, inspThumbsH(), 0);
            // 0.10-M HOTFIX TEXTURAS-VINCULA: os 5 SLOTS visíveis (o nome
            // do .gtext em cada; o tap arma o slot e abre o browser — o
            // pick escreve O SLOT selecionado sem tocar nos irmãos)
            for (u32 s = 0; s < MeshRenderer::kMeshTexSlots; ++s) {
                push(InspRow::Kind::TexSlotRow, btnH,
                     kInspectorTexSlot0 + s);
            }
            for (u32 i = 0; i < 3; ++i) {
                push(InspRow::Kind::ColorSlider, sldH, kInspectorColBase + i);
            }
            push(InspRow::Kind::ColorHex, btnH, kInspectorColHex);
        }
    }
    // ---- FÍSICA (spec C: o que o BodyComp TEM — zero física nova) -------
    if (p.bc) {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 4, kInspBitFisica);
        if (!(collapsed & kInspBitFisica)) {
            // FASE 9 (G2-8): FÍSICA em DUAS COLUNAS nome/valor — SEM
            // truncagem (o "body: static - obb - cha…" de uma linha morreu)
            push(InspRow::Kind::TwoCol, textH, 0);      // tipo
            push(InspRow::Kind::TwoCol, textH, 1);      // forma
            push(InspRow::Kind::TwoCol, textH, 2);      // no chão
            push(InspRow::Kind::Velx, sldH, kInspectorVelX);
        }
    }
    if (p.im) {
        push(InspRow::Kind::Label, textH, 0);           // input:
    }
    if (p.im) {
        if (!p.tc) {
            push(InspRow::Kind::AddTc, addH, kInspectorAddTc);
        } else {
            push(InspRow::Kind::Label, textH, 0);       // tc: stick + jump
        }
    }
    // ---- ANIMAÇÃO ----
    if (!p.anim && p.canAnim) {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 6, kInspBitAnim);
        if (!(collapsed & kInspBitAnim)) {
            push(InspRow::Kind::AddAnim, addH, kInspectorAddAnim);
        }
    } else if (p.anim) {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 6, kInspBitAnim);
        if (!(collapsed & kInspBitAnim)) {
            push(InspRow::Kind::AnimLabel, textH, 0);
        }
    }
    // ---- ÁUDIO ----
    if (p.au) {
        push(InspRow::Kind::AuSection, secH, kInspSectionBase + 5, kInspBitAudio);
        if (!(collapsed & kInspBitAudio)) {
            push(InspRow::Kind::AuClip, btnH, kInspectorAuClip);
            push(InspRow::Kind::AuPlay, btnH, kInspectorAuPlay);
            push(InspRow::Kind::AuAutoplay, btnH, kInspectorAuAutoplay);
            push(InspRow::Kind::AuLoop, btnH, kInspectorAuLoop);
            push(InspRow::Kind::AuVolume, sldH, kInspectorAuVolume);
            push(InspRow::Kind::AuPitch, sldH, kInspectorAuPitch);
            push(InspRow::Kind::AuPos, btnH, kInspectorAuPos);
            push(InspRow::Kind::AuRint, sldH, kInspectorAuRint);
            push(InspRow::Kind::AuRext, sldH, kInspectorAuRext);
        }
    }
    // ---- ENVIRONMENT (0.12-Y-fix) — o inspector do WorldEnvironment: o
    // TIPO de céu (3.Y-D) + os 5 controlos da ordem (céu hex · hdri
    // navegador · ambiente 0..2 · névoa 0..1 · cor da névoa hex) + os 5
    // campos PROCEDURAIS condicionais (horizonte/zénite/sol/nuvens/vento).
    // O preset é SÓ Transform+Environment (sem mesh/física) — a secção é
    // o CORPO do TIC.
    if (p.env) {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 8,
             kInspBitEnvironment);
        if (!(collapsed & kInspBitEnvironment)) {
            push(InspRow::Kind::EnvSkyType, btnH, kInspectorEnvSkyType);
            // 3.Y-D: os campos procedurais SÓ com skyType == 1 (o M-Yd2
            // mata esta condição — o modo sólido NÃO tem nuvens)
            if (p.envSkyType == Environment::kSkyTypeProcedural) {
                push(InspRow::Kind::EnvHor, btnH, kInspectorEnvHor);
                push(InspRow::Kind::EnvZen, btnH, kInspectorEnvZen);
                push(InspRow::Kind::EnvSunC, btnH, kInspectorEnvSunC);
                push(InspRow::Kind::EnvCov, sldH, kInspectorEnvCov);
                push(InspRow::Kind::EnvSpd, sldH, kInspectorEnvSpd);
            }
            push(InspRow::Kind::EnvSky, btnH, kInspectorEnvSky);
            push(InspRow::Kind::EnvHdri, btnH, kInspectorEnvHdri);
            push(InspRow::Kind::EnvAmb, sldH, kInspectorEnvAmb);
            push(InspRow::Kind::EnvFogD, sldH, kInspectorEnvFogD);
            push(InspRow::Kind::EnvFogC, btnH, kInspectorEnvFogC);
        }
    }
    // ---- SCRIPT V.ONI (0.9.2 §10) — sempre disponível num TIC ----------
    if (p.script) {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 7,
             kInspBitScript);
        if (!(collapsed & kInspBitScript)) {
            push(InspRow::Kind::ScriptEdit, addH, 0);
            for (u32 e = 0; e < p.scriptExports && e < 8; ++e) {
                push(InspRow::Kind::ScriptVar, textH, e);
            }
        }
    } else {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 7,
             kInspBitScript);
        if (!(collapsed & kInspBitScript)) {
            push(InspRow::Kind::ScriptAdd, addH, 0);
        }
    }
    return n;
}

// altura REAL do conteúdo = fundo da última linha do plano (o Y final do
// cursor partilhado). Sem linhas → 0.
inline f32 inspectorContentHeight(const InspProfile& p, const TextMetrics& m,
                                  bool selectable, u32 collapsed) {
    InspRow rows[64];
    const u32 n = inspectorPlan(p, m, selectable, collapsed, rows);
    if (n == 0) {
        return 0.0f;
    }
    return rows[n - 1].y + rows[n - 1].h;
}

// Hierarchy: índice da linha sob um tap em coords de ECRÃ (com o offset do
// scroll aplicado) — -1 se fora da lista
inline i32 hierarchyRowAtTap(f32 tapY, f32 listTop, f32 offset, u32 ticCount) {
    if (ticCount == 0) {
        return -1;
    }
    // floor antes do cast: tap acima do topo (negativo) NÃO trunca para 0
    const i32 row = static_cast<i32>(std::floor((tapY - listTop + offset) / kRowH));
    if (row < 0 || static_cast<u32>(row) >= ticCount) {
        return -1;
    }
    return row;
}

// ---- F5.2: overlays de armazenamento (fonte ÚNICA das geometrias — o
// desenho e os testes de tap partilham estas fórmulas) ----------------------

// menu/overlay centrado na área útil (ox/oy = origem da safe-area; aw/ah =
// área útil). h é por overlay.
inline UiRect centeredMenuRect(f32 ox, f32 oy, f32 aw, f32 ah, f32 h) {
    return {ox + (aw - kMenuW) * 0.5f, oy + (ah - h) * 0.5f, kMenuW, h};
}

// 0.9.0 — ÁREA DOS OVERLAYS: a faixa do VIEWPORT (por baixo do chrome de
// cima 104, por cima da tab bar de baixo + status 72). Centrar overlays na
// área útil INTEIRA punha-os por BAIXO da tab bar de modo (o botão "fechar"
// do log viewer ficava tapado — o toque comia-o a tab ÁUDIO). Os overlays
// continuam DENTRO do contentRect (a invariante da F4.2).
inline void overlayArea(f32 sw, f32 sh, const safe::Insets& i, f32& ox,
                        f32& oy, f32& aw, f32& ah) {
    ox = i.left;
    oy = i.top + safe::kToolbarH;   // chrome de cima (56+48)
    aw = sw - i.left - i.right;
    ah = sh - i.top - i.bottom - safe::kToolbarH - safe::kStatusH -
         safe::kBottomTabH;
    if (ah < 100.0f) {
        ah = 100.0f;   // defesa: telas baixas — o overlay continua centrado
    }
}

// DIÁLOGO "Precisa de acesso a todos os ficheiros?": título (kHeaderH) +
// 3 linhas de mensagem + 2 botões (Permitir/Cancelar lado a lado)
inline f32 storageDialogHeight() {
    return kHeaderH + 3.0f * 34.0f + 72.0f + kPad;
}
inline void storageDialogButtons(const UiRect& dlg, UiRect& permitir,
                                 UiRect& cancelar) {
    const f32 btnTop = dlg.y + kHeaderH + 3.0f * 34.0f + 8.0f;
    const f32 gap = kPad;
    const f32 w = (kMenuW - 2.0f * kPad - gap) * 0.5f;
    permitir = {dlg.x + kPad, btnTop, w, 56.0f};
    cancelar = {dlg.x + kPad + w + gap, btnTop, w, 56.0f};
}

// overlay IMPORT: título + N linhas (40 px, espaçadas 48 — como o seletor
// de assets); devolve o rect da linha `i` (0-based)
inline UiRect importRowRect(const UiRect& menu, u32 i) {
    return {menu.x + kPad, menu.y + kHeaderH + static_cast<f32>(i) * 48.0f,
            kMenuW - 2.0f * kPad, 40.0f};
}
inline f32 importMenuHeight(u32 rows) {
    return kHeaderH + static_cast<f32>(rows) * 48.0f + kPad;
}

// ---- 0.6.8: PLAY BAR (fonte ÚNICA das geometrias — desenho e testes) --------

// Botão Stop da play bar (id 5 — faixa livre entre a toolbar 1..3 e os
// presets 20..23): MESMO tamanho/posição que os botões da toolbar (esquerda,
// 240x56, centrado na altura de 88 px).
constexpr u64 kPlayStopId = 5;
// FASE UI B: o PAUSE da play bar (a ordem do dono — o pause saiu da top
// bar e vive no «menu do Play»; o vizinho do Stop na faixa exclusiva)
constexpr u64 kPlayPauseId = 6;

inline UiRect playBarRect(f32 sw, f32 sh, const safe::Insets& i) {
    // mesma faixa da toolbar (kToolbarH) — o PLAY substitui a toolbar no topo
    return safe::toolbarRect(sw, sh, i);
}
inline UiRect playStopButtonRect(const UiRect& bar) {
    return {bar.x + kPad, bar.y + (safe::kToolbarH - 56.0f) * 0.5f,
            240.0f, 56.0f};
}

// ids do seletor de modo do gizmo (0.6.9 — reservados já p/ não colidir):
// toolbar Mover/Rodar/Escalar = 7/8/9, Snap = 10
constexpr u64 kGizmoModeMoveId    = 7;
constexpr u64 kGizmoModeRotateId  = 8;
constexpr u64 kGizmoModeScaleId   = 9;
constexpr u64 kGizmoSnapId        = 10;

// ---- 0.7.0 — UI criável + gestão de TICs: ids e geometria (faixas EXCLUSIVAS) --

// 0.7.6 — os ids do separador 3D|UI (11/12) passaram a viver em
// ui/Toolbar.h (G3 da barra final); os botões de olho/⋮ da Hierarchy
// mantêm as faixas ALTAS de sempre

// botões de olho/⋮ da Hierarchy (faixas ALTAS: + handle.index — nunca
// colidem com as linhas 1000+ nem com os sliders 2000+ do Inspector)
constexpr u64 kHierEyeBase  = 100000;
constexpr u64 kHierDotsBase = 200000;
// 0.11-H (FASE 1): o claim do DRAG-REPARTENT da zona do ícone (faixa alta
// nova: + handle.index — 300000 está livre entre as duas faixas acima)
constexpr u64 kHierDragBase = 300000;

// menu contextual (⋮): Renomear/Remover/Duplicar/Visibilidade
constexpr u64 kCtxRenameId  = 6500;
constexpr u64 kCtxRemoveId  = 6501;
constexpr u64 kCtxDuplicId  = 6502;
constexpr u64 kCtxVisibleId = 6503;
// diálogo de confirmação de remoção
constexpr u64 kRemoveConfirmId = 6510;
constexpr u64 kRemoveCancelId  = 6511;

// teclado in-app (6600 + row*10 + col); linha de baixo tem ids próprios
constexpr u64 kKbBase     = 6600;
constexpr u64 kKbSpaceId  = 6650;
constexpr u64 kKbDashId   = 6651;
constexpr u64 kKbBackId   = 6652;
constexpr u64 kKbOkId     = 6653;
constexpr u64 kKbCancelId = 6654;
constexpr u64 kKbCaseId   = 6655;   // 0.7.5: toggle abc/ABC (minúsculas)

// Inspector de ELEMENTO de UI (sliders 8000..8005; botões 8010+)
constexpr u64 kUiInspX      = 8000;
constexpr u64 kUiInspY      = 8001;
constexpr u64 kUiInspW      = 8002;
constexpr u64 kUiInspH      = 8003;
constexpr u64 kUiInspR      = 8004;
constexpr u64 kUiInspG      = 8005;
constexpr u64 kUiInspB      = 8006;
constexpr u64 kUiInspA      = 8007;   // 0.7.4: ALPHA do fundo (Label = 0)
constexpr u64 kUiInspVis    = 8010;
constexpr u64 kUiInspAnchH  = 8011;
constexpr u64 kUiInspAnchV  = 8012;
constexpr u64 kUiInspText   = 8013;
constexpr u64 kUiInspAct    = 8014;
constexpr u64 kUiInspTarget = 8015;
constexpr u64 kUiInspRemove = 8016;
constexpr u64 kUiInspStyle  = 8017;   // 0.7.1: "estilo: fade|slide" (trans)
constexpr u64 kUiInspSpacing= 8018;   // 0.7.4: espaçamento (Menu/containers)
constexpr u64 kUiInspPad    = 8019;   // 0.7.4: padding (containers)
constexpr u64 kUiInspTex    = 8028;   // 0.7.4: "tex: …" (Panel/Button/Image)
constexpr u64 kUiInspParent = 8029;   // 0.7.4: "colocar em: …" (filho de)
constexpr u64 kUiInspAlign  = 8031;   // 0.7.4: "alinhamento: start/center/end"
constexpr u64 kUiInspHex    = 8032;   // 0.8.6: "hex: #RRGGBB" (teclado hex)
constexpr u64 kUiInspFont   = 8033;   // 0.8.6: "letra: Nx" (escala da fonte)
constexpr u64 kUiInspTStyle = 8034;   // 0.8.6: "letra estilo" (cicla)
// 0.7.3 — inspector do JOYSTICK (TouchControls editável)
// 0.12-B (B-2) — o Inspector de CONTROLO (a árvore): as secções + os
// campos novos da base + os campos por tipo do joystick
constexpr u64 kUiInspSecT   = 8040;   // cabeçalho da secção TRANSFORM
constexpr u64 kUiInspSecA   = 8041;   // cabeçalho da secção APARÊNCIA
constexpr u64 kUiInspSecC   = 8042;   // cabeçalho da secção COMPORTAMENTO
constexpr u64 kUiInspMargL  = 8043;   // margem esquerda (esticar)
constexpr u64 kUiInspMargT  = 8044;
constexpr u64 kUiInspMargR  = 8045;
constexpr u64 kUiInspMargB  = 8046;
constexpr u64 kUiInspPivX   = 8047;   // pivô X (0..1)
constexpr u64 kUiInspPivY   = 8048;   // pivô Y (0..1)
constexpr u64 kUiInspRot    = 8049;   // rotação (graus)
constexpr u64 kUiInspStrch  = 8050;   // "esticar: sim/nao" (a 10.ª âncora)
constexpr u64 kUiInspOpa    = 8051;   // opacidade (0..1)
constexpr u64 kUiInspDead   = 8052;   // joystick: zona morta (fração do raio)
constexpr u64 kUiInspRadius = 8053;   // joystick: raio (px; escreve size)
// 0.12-B (B-3) — as CHIPS do canvas de UI (a moldura do device + as toggles)
constexpr u64 kUiCvChipAsp  = 8060;   // o seletor de proporção (cicla)
constexpr u64 kUiCvChipGd   = 8061;   // as guias de margem segura
constexpr u64 kUiCvChipGr   = 8062;   // a grelha de alinhamento
constexpr u64 kJoyX      = 8020;   // pos X (fração da área útil 0..1)
constexpr u64 kJoyY      = 8021;
constexpr u64 kJoySize   = 8022;   // escala do raio
constexpr u64 kJoySens   = 8023;   // sensibilidade
constexpr u64 kJoyR      = 8024;   // cor R/G/B
constexpr u64 kJoyG      = 8025;
constexpr u64 kJoyB      = 8026;
constexpr u64 kJoyRemove = 8030;   // remover o componente

// regiões de scroll novas: Inspector de UI (46). 41/42/43 = hierarquia/
// inspector/logs (acima)
constexpr u64 kUiInspScrollId = 46;

// 0.7.2 — navegador de ficheiros: raízes 6850..6854, subir 6860, fechar
// 6899, linhas da lista 6870+; diálogo aplicar-após-import 6900/6901
constexpr u64 kBrowserRootBase = 6850;
constexpr u64 kBrowserUpId     = 6860;
constexpr u64 kBrowserRowBase  = 6870;
constexpr u64 kBrowserCloseId  = 6899;
constexpr u64 kApplyYesId      = 6900;
constexpr u64 kApplyNoId       = 6901;
constexpr u64 kBrowserScrollId = 45;   // região de scroll da lista
// 0.12-C (C-4) — o picker: o CAMPO de pesquisa (propósito 10) + os
// SEGMENTOS do breadcrumb (o salto para a pasta do segmento)
constexpr u64 kBrowserSearchId = 6902;
constexpr u64 kBrowserCrumbBase = 6910;   // 6910..6915 (6 segmentos basta)
// 0.12-QoL D — o pop-up do AUTO-SAVE (a recuperação): [Recuperar]/[Descartar]
constexpr u64 kAutoYesId = 6930;
constexpr u64 kAutoNoId  = 6931;
// o IMPORTAR do estado vazio dos Assets (o drawer, tab 2)
constexpr u64 kAssetsImportId = 6920;
// o CRIAR TIC do estado vazio do Inspector (abre o plusMenu de presets)
constexpr u64 kInspCreateTicId = 6921;

// 0.12-C (C-4) — A FONTE ÚNICA da GEOMETRIA da folha do NAVEGADOR (o draw
// e os pins partilham — o padrão menuSheetLayout da C-2). O card 92% com o
// teto 900dp, CENTRADO na ÁREA ÚTIL (a safe-area respeitada: o card nunca
// entra nos insets), a LINHA 48dp do dono e a ordem vertical:
// breadcrumb → pesquisa → raízes → subir → lista (o Subir FIXO fora do
// scroll — a regra do dono).
struct BrowserSheetLayout {
    f32 x, y, w, h;        // o card
    f32 rowH;              // 48dp — a linha do dono (raízes/subir/linhas)
    f32 crumbY, crumbH;    // a linha do BREADCRUMB (os segmentos saltam)
    f32 searchY, searchH;  // o campo de pesquisa (40dp)
    f32 rootsY;            // os atalhos (a linha de raízes)
    f32 upY;               // o Subir (FIXO — nunca desliza com a lista)
    f32 listTop;           // o topo da lista (dentro do card)
};
inline BrowserSheetLayout browserSheetLayout(f32 sw, f32 sh, f32 safeL,
                                             f32 safeT, f32 safeR, f32 safeB,
                                             f32 hdrH, u32 nRows) {
    BrowserSheetLayout r;
    const f32 aw = sw - safeL - safeR;
    const f32 ah = sh - safeT - safeB;
    const f32 rowH = theme::dp(48.0f);   // A LINHA do dono (~48dp)
    r.rowH = rowH;
    const f32 w = aw * 0.92f > theme::dp(900.0f) ? theme::dp(900.0f)
                                                 : aw * 0.92f;
    // o conteúdo FIXO da folha (título + breadcrumb + pesquisa + raízes +
    // subir + folga) — a LISTA encolhe para caber e o SCROLL revela o
    // resto (a regra D14 do menu: a folha nunca sai do contentRect)
    const f32 fixH = hdrH + theme::dp(6.0f) + theme::dp(30.0f) +
                     theme::dp(6.0f) + theme::dp(40.0f) + theme::dp(6.0f) +
                     rowH + theme::dp(4.0f) + rowH + theme::dp(4.0f);
    // no máx. 8 linhas VISÍVEIS (o scroll mostra as outras — o picker fica
    // COMPACTO mesmo numa pasta cheia)
    const f32 shown = static_cast<f32>(nRows < 8u ? nRows : 8u);
    const f32 maxH = ah - theme::dp(16.0f);
    f32 h = fixH + shown * rowH;
    if (h > maxH) {
        h = maxH > fixH ? maxH : fixH;   // ecrã minúsculo: a lista é que cede
    }
    r.w = w;
    r.h = h;
    r.x = safeL + (aw - w) * 0.5f;
    r.y = safeT + (ah - h) * 0.5f > safeT + theme::dp(8.0f)
              ? safeT + (ah - h) * 0.5f
              : safeT + theme::dp(8.0f);
    r.crumbY = r.y + hdrH + theme::dp(6.0f);
    r.crumbH = theme::dp(30.0f);
    r.searchY = r.crumbY + r.crumbH + theme::dp(6.0f);
    r.searchH = theme::dp(40.0f);
    r.rootsY = r.searchY + r.searchH + theme::dp(6.0f);
    r.upY = r.rootsY + rowH + theme::dp(4.0f);
    r.listTop = r.upY + rowH + theme::dp(4.0f);
    return r;
}

// presets de ELEMENTO no "+" do modo UI (mesma faixa 20+i do menu de TICs —
// os menus são mutuamente exclusivos: o + abre um OU outro conforme o modo)
// 0=Panel, 1=Label, 2=Button, 3=Image (0.7.3 acrescenta Menu/Card/Article;
// 0.7.4 acrescenta VBox/HBox — o mapa vive em uiPlusChoiceKind)

// 0.7.4 — seletor de TEXTURA de elemento de UI (menuKind 3): a escolha
// "importar…" devolve este código (o main abre o navegador 0.7.2)
constexpr int kAssetPickImport = 99;

// 0.7.6 — toolbarModeRect/toolbarModeEndX foram REMOVIDOS: a barra
// final de 5 grupos vive em ui/Toolbar.h (toolbar::layout é a fonte única).

// ---- 0.12-C (C-2) — O SHEET ANCORADO (a fonte ÚNICA do menu principal) -----
// O menu principal é uma FOLHA ancorada ao botão que o abriu: altura
// MÁXIMA (nunca sai do contentRect — o resto ROLA lá dentro, o D14), o
// fundo escurecido atrás (o scrimMenu do draw) e a âncora 8dp sob o botão
// (spec H) com clamp ao contentRect. Os outros sheets da casa (CENAS/HIER)
// partilham o MESMO contrato de âncora; este é o caso mais cheio (o
// scroll). O draw (EditorUi.cpp drawFileMenu) e os pins partilham ESTA
// função.
struct MenuSheetLayout {
    f32 h;          // a altura do sheet (min(contentH, maxH))
    f32 maxOffset;  // o máximo do scroll (contentH − h; 0 = cabe inteiro)
    f32 x;          // a posição final no ecrã (ancorada com clamp ou
    f32 y;          //   centrada na área útil se sem âncora)
};
inline MenuSheetLayout menuSheetLayout(f32 sw, f32 sh, f32 safeTop,
                                       f32 safeBottom, f32 safeLeft,
                                       f32 safeRight, f32 contentH, f32 ax,
                                       f32 ay, f32 sheetW) {
    MenuSheetLayout r;
    // a ALTURA máxima disponível (o sheet NUNCA sai do contentRect — a
    // invariante F4.2; o resto rola lá dentro). D14: a reserva do fundo
    // é a TAB BAR (32dp — o «Documentação V. …» ficava debaixo dela).
    const f32 maxH = sh - safeTop - theme::dp(safe::kToolbarH) -
                     theme::dp(8.0f) - theme::dp(safe::kBottomTabH);
    r.h = contentH < maxH ? contentH : maxH;
    r.maxOffset = contentH > r.h ? contentH - r.h : 0.0f;
    if (ax >= 0.0f && ay >= 0.0f) {
        r.x = ax;
        r.y = ay + theme::dp(8.0f);   // 8dp sob o botão (spec H)
        // clamp ao contentRect (nada sai do ecrã — a invariante F4.2);
        // D14: o fundo do sheet nunca passa do TOPO da tab bar
        const f32 maxX = safeLeft + sw - safeRight - sheetW - 4.0f;
        if (r.x > maxX) {
            r.x = maxX > safeLeft ? maxX : safeLeft;
        }
        const f32 maxY = sh - safeBottom - r.h - theme::dp(safe::kBottomTabH);
        if (r.y > maxY) {
            r.y = maxY > safeTop + safe::kToolbarH ? maxY
                                                   : safeTop +
                                                         safe::kToolbarH;
        }
    } else {
        // sem âncora: centrado na área útil (o compat dos testes)
        f32 ox, oy, aw, ah;
        const safe::Insets in{safeLeft, safeTop, safeRight, safeBottom};
        overlayArea(sw, sh, in, ox, oy, aw, ah);
        r.x = ox + (aw - sheetW) * 0.5f;
        r.y = oy + (ah - r.h) * 0.5f;
    }
    return r;
}

} // namespace editor
} // namespace vv
