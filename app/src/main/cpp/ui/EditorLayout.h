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
#include "components/MeshRenderer.h"
#include "components/InputMap.h"
#include "components/BodyComp.h"
#include "components/TouchControls.h"

namespace vv {
namespace editor {

// constantes partilhadas pelo desenho e pela medição (antes no anon ns do .cpp)
constexpr f32 kPad       = 12.0f;
constexpr f32 kHeaderH   = 48.0f;
constexpr f32 kRowH      = 52.0f;
constexpr f32 kMenuW     = 340.0f;

// altura do conteúdo da Hierarchy: uma linha por TIC ativo
inline f32 hierarchyContentHeight(u32 ticCount) {
    return static_cast<f32>(ticCount) * kRowH;
}

// ---- ids dos widgets do Inspector (antes no anon ns do EditorUi.cpp —
// o plano e o hit-test do tap re-despachado partilham-nos)
constexpr u64 kInspectorSliderBase = 2000;   // 9 sliders do Transform3D
constexpr u64 kInspectorVelX       = 2100;   // slider velx do BodyComp
constexpr u64 kInspectorAddTc      = 3001;   // botão "add TouchControls"
constexpr u64 kInspectorMeshSel    = 5001;   // F5-E: linha "mesh: …"
constexpr u64 kInspectorTexSel     = 5002;   // F5-E: linha "tex: …"
constexpr u64 kInspectorVis        = 5200;   // 0.7.0: "visivel: sim/nao"
constexpr u64 kInspectorColBase    = 5300;   // 0.7.0: sliders R/G/B (+i)

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
    bool mr = false;   // MeshRenderer  (linhas mesh/tex)
    bool im = false;   // InputMap      (linha input + addTc/tc)
    bool bc = false;   // BodyComp      (linha body + slider velx)
    bool tc = false;   // TouchControls (muda addTc ↔ label tc)
};

inline InspProfile inspectorProfile(const Tic& tic) {
    InspProfile p;
    p.tr = tic.getComponent<Transform3D>() != nullptr;
    p.mr = tic.getComponent<MeshRenderer>() != nullptr;
    p.im = tic.getComponent<InputMap>() != nullptr;
    p.bc = tic.getComponent<BodyComp>() != nullptr;
    p.tc = tic.getComponent<TouchControls>() != nullptr;
    return p;
}

// ---- o PLANO do Inspector — FONTE ÚNICA do layout --------------------------
// Linhas em ORDEM, com y CUMULATIVO em coords de conteúdo (o desenho só
// subtrai o offset do scroll; nenhum consumidor faz "+=" próprio).
struct InspRow {
    enum class Kind : u8 {
        Name,        // nome do TIC (accent)
        Section,     // cabeçalho "Transform3D" + separador no fundo da linha
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
    };
    Kind kind;
    f32  y;     // topo da linha em COORDS DE CONTEÚDO (cumulativo)
    f32  h;     // altura da linha (derivada das métricas da fonte)
    u64  id;    // id do widget interativo (0 = só desenho)
};

// payload do ColorSlider por índice (0=R, 1=G, 2=B) — o desenho e o
// hit-test partilham esta ordem
inline u32 inspectorRowCount(const InspProfile& p, bool selectable) {
    u32 n = 2;                                          // nome + visivel
    if (p.tr) n += 1 + 9;                               // secção + 9 sliders
    if (p.mr) n += 2 + 3;                               // mesh + tex + R/G/B
    if (p.im) n += 1;                                   // input
    if (p.bc) n += 2;                                   // body + velx
    if (p.im) n += 1;                                   // addTc OU tc
    (void)selectable;
    return n;
}

// constrói o plano (rows deve ter capacidade inspectorRowCount; devolve count)
inline u32 inspectorPlan(const InspProfile& p, const TextMetrics& m,
                         bool selectable, InspRow* rows) {
    const f32 textH = inspTextRowH(m);
    const f32 btnH  = inspButtonRowH(m);
    const f32 sldH  = inspSliderRowH(m);
    const f32 addH  = inspAddTcH(m);

    u32 n = 0;
    f32 y = 0.0f;
    auto push = [&](InspRow::Kind kind, f32 h, u64 id) {
        rows[n].kind = kind;
        rows[n].y    = y;
        rows[n].h    = h;
        rows[n].id   = id;
        ++n;
        y += h;   // ← o ÚNICO avanço de cursor: y += altura_linha
    };

    push(InspRow::Kind::Name, textH, 0);
    push(InspRow::Kind::VisToggle, btnH, kInspectorVis);   // 0.7.0
    if (p.tr) {
        push(InspRow::Kind::Section, textH, 0);
        for (u32 i = 0; i < 9; ++i) {
            push(InspRow::Kind::Slider, sldH, kInspectorSliderBase + i);
        }
    }
    if (p.mr) {
        push(selectable ? InspRow::Kind::MeshButton : InspRow::Kind::MeshLabel,
             selectable ? btnH : textH, selectable ? kInspectorMeshSel : 0);
        push(selectable ? InspRow::Kind::TexButton : InspRow::Kind::TexLabel,
             selectable ? btnH : textH, selectable ? kInspectorTexSel : 0);
        // 0.7.0 — cor por TIC (sliders R/G/B do tint)
        for (u32 i = 0; i < 3; ++i) {
            push(InspRow::Kind::ColorSlider, sldH, kInspectorColBase + i);
        }
    }
    if (p.im) {
        push(InspRow::Kind::Label, textH, 0);           // input:
    }
    if (p.bc) {
        push(InspRow::Kind::Label, textH, 0);           // body:
        push(InspRow::Kind::Velx, sldH, kInspectorVelX);
    }
    if (p.im) {
        if (!p.tc) {
            push(InspRow::Kind::AddTc, addH, kInspectorAddTc);
        } else {
            push(InspRow::Kind::Label, textH, 0);       // tc: stick + jump
        }
    }
    return n;
}

// altura REAL do conteúdo = fundo da última linha do plano (o Y final do
// cursor partilhado). Sem linhas → 0.
inline f32 inspectorContentHeight(const InspProfile& p, const TextMetrics& m,
                                  bool selectable) {
    InspRow rows[32];
    const u32 n = inspectorPlan(p, m, selectable, rows);
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

// separador "3D | UI" da toolbar (depois dos 3 botões Menu/Play/Settings,
// ANTES do grupo do gizmo — ver toolbarModeRect)
constexpr u64 kMode3dId = 11;
constexpr u64 kModeUiId = 12;

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

// Inspector de ELEMENTO de UI (sliders 8000..8005; botões 8010+)
constexpr u64 kUiInspX      = 8000;
constexpr u64 kUiInspY      = 8001;
constexpr u64 kUiInspW      = 8002;
constexpr u64 kUiInspH      = 8003;
constexpr u64 kUiInspR      = 8004;
constexpr u64 kUiInspG      = 8005;
constexpr u64 kUiInspB      = 8006;
constexpr u64 kUiInspVis    = 8010;
constexpr u64 kUiInspAnchH  = 8011;
constexpr u64 kUiInspAnchV  = 8012;
constexpr u64 kUiInspText   = 8013;
constexpr u64 kUiInspAct    = 8014;
constexpr u64 kUiInspTarget = 8015;
constexpr u64 kUiInspRemove = 8016;

// regiões de scroll novas: Inspector de UI (46). 41/42/43 = hierarquia/
// inspector/logs (acima)
constexpr u64 kUiInspScrollId = 46;

// presets de ELEMENTO no "+" do modo UI (mesma faixa 20+i do menu de TICs —
// os menus são mutuamente exclusivos: o + abre um OU outro conforme o modo)
// 0=Panel, 1=Label, 2=Button, 3=Image (0.7.3 acrescenta Menu/Card/Article)

// rect do separador "3D | UI" na faixa da toolbar (esq.: após os 3 botões)
inline UiRect toolbarModeRect(f32 sw, f32 sh, const safe::Insets& i) {
    const UiRect bar = safe::toolbarRect(sw, sh, i);
    const f32 x = bar.x + 16.0f + 3.0f * (240.0f + 16.0f) + 8.0f;
    return {x, bar.y + (safe::kToolbarH - 56.0f) * 0.5f, 2.0f * 96.0f + 8.0f,
            56.0f};
}
// fim X do separador — o grupo do gizmo alinha a partir daqui (nunca sobrepõe)
inline f32 toolbarModeEndX(f32 sw, f32 sh, const safe::Insets& i) {
    const UiRect m = toolbarModeRect(sw, sh, i);
    return m.x + m.w + 8.0f;
}

} // namespace editor
} // namespace vv
