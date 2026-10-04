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
#include "components/UiCanvas.h"   // 0.8.0: canAnim (elementos animáveis)

namespace vv {
namespace editor {

// constantes partilhadas pelo desenho e pela medição (antes no anon ns do .cpp)
// 0.9.0 (spec A/B): kPad 12→16 (8-múltiplo), linha 52→48 (alvo mínimo), e a
// hierarquia ganha a LINHA DE PESQUISA (48dp) por baixo do cabeçalho.
constexpr f32 kPad       = 16.0f;
constexpr f32 kHeaderH   = 48.0f;
constexpr f32 kRowH      = 48.0f;
constexpr f32 kSearchRowH = 48.0f;   // 0.9.0: pesquisa de TIC no header da hierarquia
constexpr f32 kMenuW     = 340.0f;

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

// 0.9.0 (spec C) — SECÇÕES COLAPSÁVEIS do Inspector: cabeçalho 48dp com
// título 14sp + chevron; o estado (bitmask por secção) vive no EditorState e
// PERSISTE (spec G). Bits: 0=Transform 1=Camera 2=Malha 3=Material 4=Fisica
// 5=Audio 6=Anim. Os botões R (repõe a LINHA pos/rot/scale) partilham a
// faixa 5510+.
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
constexpr u64 kInspectorVis        = 5200;   // 0.7.0: "visivel: sim/nao"
constexpr u64 kInspectorColBase    = 5300;   // 0.7.0: sliders R/G/B (+i)
constexpr u64 kInspectorColHex     = 5303;   // 0.8.6: "hex: #RRGGBB" do tint
// 0.8.9 — NORMALIZAÇÃO DE IMPORT: dims originais + repõe a escala {1,1,1}
// (o fit uniforme vive no Transform3D — este botão devolve o tamanho real)
constexpr u64 kInspectorScaleOrig  = 5310;   // "escala: original"
// 0.7.7 — inspector da CÂMARA (sliders + botões; faixa 5400..5419)
constexpr u64 kInspectorCamFov     = 5400;
constexpr u64 kInspectorCamNear    = 5401;
constexpr u64 kInspectorCamFar     = 5402;
constexpr u64 kInspectorCamOrtho   = 5403;
constexpr u64 kInspectorCamProj    = 5410;
constexpr u64 kInspectorCamActive  = 5411;
constexpr u64 kInspectorCamFrustum = 5412;   // 0.7.10: toggle do gizmo
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
inline f32 inspTextRowH(const TextMetrics& m)   { return m.block() + 6.0f; }  // labels
inline f32 inspButtonRowH(const TextMetrics& m) { return m.block() + 8.0f; }  // botões
inline f32 inspSliderRowH(const TextMetrics& m) { return m.block() + 8.0f; }  // sliders
inline f32 inspAddTcH(const TextMetrics& m)     { return m.block() + 14.0f; } // botão do fundo

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
    if (const MeshRenderer* mr = tic.getComponent<MeshRenderer>()) {
        p.prim = mr->primOn;   // 0.8.0
        p.primKind = mr->prim.kind;
        // 0.8.9: mesh DE FICHEIRO (não prim, ref por meshPath) → dims+escala
        p.fileMesh = !mr->primOn && !mr->meshPath.empty();
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
        Label,       // linha de texto (input/body/tc)
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

// altura de UMA linha de Transform (spec C: "3 campos numéricos editáveis
// 48dp"): título 12sp em LINHA PRÓPRIA (24px) + caixas X/Y/Z 48dp + R 48.
// A largura do painel (268 úteis) não comporta título+3 caixas+R na MESMA
// linha — o título sobe (o layout do mockup mantém-se: rótulos X/Y/Z dentro
// das caixas, R à direita, alvo 48dp).
inline f32 inspTransformRowH(const TextMetrics& m) {
    return 24.0f + 4.0f + 48.0f + 4.0f;   // título + caixas 48dp + folga
}
// altura do CABEÇALHO de secção (48dp — spec C)
inline f32 inspSectionH() { return 48.0f; }
// altura da linha de MINIATURAS de Material (64dp + legendas; spec C).
// FASE 9 (G1-3): legendas INTEIRAS em LINHA RESERVADA (célula = útil/3) —
// 64 + bloco de texto 12sp (~24px @28px de fonte) + folga
inline f32 inspThumbsH() { return 64.0f + 28.0f; }

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
    if (p.mr) {
        n += 1;
        if (!(collapsed & kInspBitMaterial)) {
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
            }
        }
    }
    // ---- MATERIAL (spec C): R/G/B + hex + swatch ----
    if (p.mr) {
        push(InspRow::Kind::Section, secH, kInspSectionBase + 3,
             kInspBitMaterial);
        if (!(collapsed & kInspBitMaterial)) {
            push(InspRow::Kind::MaterialThumbs, inspThumbsH(), 0);
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
            push(InspRow::Kind::Label, textH, 0);       // body:
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

// presets de ELEMENTO no "+" do modo UI (mesma faixa 20+i do menu de TICs —
// os menus são mutuamente exclusivos: o + abre um OU outro conforme o modo)
// 0=Panel, 1=Label, 2=Button, 3=Image (0.7.3 acrescenta Menu/Card/Article;
// 0.7.4 acrescenta VBox/HBox — o mapa vive em uiPlusChoiceKind)

// 0.7.4 — seletor de TEXTURA de elemento de UI (menuKind 3): a escolha
// "importar…" devolve este código (o main abre o navegador 0.7.2)
constexpr int kAssetPickImport = 99;

// 0.7.6 — toolbarModeRect/toolbarModeEndX foram REMOVIDOS: a barra
// final de 5 grupos vive em ui/Toolbar.h (toolbar::layout é a fonte única).

} // namespace editor
} // namespace vv
