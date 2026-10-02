// ui/Toolbar.cpp — implementação da BARRA SUPERIOR de 5 grupos (0.7.6).
//
// Desenho: fundo theme::kTheme.bg (#0B0E13 — mais escuro que os painéis,
// separa o chrome da área de trabalho) + separadores finos entre grupos.
// Widgets: G1 texto (mono), G2/G4/G5 ícones (cor de marca #8AB4F8),
// G3 segmented de texto. Ativo/held = INVERSO (fundo brand + ícone/texto
// brandInk). O gesto vem do UiContext::widgetHit (a MESMA semântica do
// button() — extraída na 0.7.6).
#include "ui/Toolbar.h"
#include "ui/EditorUi.h"   // EditorState completo (declared-only no header)
#include "ui/UiContext.h"

#include <cstdio>

namespace vv {
namespace editor {
namespace toolbar {

namespace {

// ---- métricas naturais (px de design) ----------------------------------------
constexpr f32 kPadOuter  = 12.0f;   // margem da barra aos extremos
constexpr f32 kBtnH     = 56.0f;    // altura dos botões (centrada em 88)
constexpr f32 kMenuW    = 176.0f;   // G1 "Menu" + caret
constexpr f32 kCenaW    = 152.0f;   // G1 "Cena" + caret
constexpr f32 kIconBtn  = 64.0f;    // G2/G4/G5 botão de ícone
constexpr f32 kSegW     = 96.0f;    // G3 segmento 3D/UI
constexpr f32 kG1Gap    = 12.0f;    // vão DENTRO do G1
constexpr f32 kG2Gap    = 8.0f;     // vão DENTRO do G2
constexpr f32 kSepGapN  = 28.0f;    // vão ENTRE grupos (com o separador)
constexpr f32 kSepH     = 36.0f;    // altura da linha fina do separador

} // namespace

// ---------------------------------------------------------------------------
// LAYOUT PURO — o solver: larguras naturais; se o total não cabe no
// disponível, TODAS as larguras encolhem proporcionalmente (nenhum grupo
// sobrepõe outro, nada sai do ecrã; o G5 continua ancorado à direita).
// ---------------------------------------------------------------------------
Layout layout(f32 sw, f32 sh, const safe::Insets& in, bool g4Visible) {
    Layout L;
    L.bar = safe::toolbarRect(sw, sh, in);
    L.g4Visible = g4Visible;

    const f32 avail = L.bar.w - 2.0f * kPadOuter;
    if (avail <= 100.0f) {
        return L;   // barra degenerada — só o fundo (defesa; nunca em landscape)
    }

    // larguras naturais dos grupos
    const f32 g1 = kMenuW + kG1Gap + kCenaW;
    const f32 g2 = 2.0f * kIconBtn + kG2Gap;
    const f32 g3 = 2.0f * kSegW;
    const f32 g4 = 4.0f * kIconBtn;
    const f32 g5 = kIconBtn;
    const u32 nSeps = g4Visible ? 4u : 3u;

    f32 sepGap = kSepGapN;
    f32 total = g1 + g2 + g3 + (g4Visible ? g4 : 0.0f) + g5 +
                static_cast<f32>(nSeps) * sepGap;
    if (total > avail) {
        // 1º degrau: encolher o vão entre grupos (nunca abaixo de 12)
        const f32 want = total - static_cast<f32>(nSeps) * (kSepGapN - 12.0f);
        if (want > avail) {
            sepGap = 12.0f;
            // 2º degrau: larguras proporcionais (o G5 nunca some; os grupos
            // encolhem JUNTOS — o layout continua legível)
            const f32 fixed = static_cast<f32>(nSeps) * sepGap;
            const f32 k = (avail - fixed) / (total - fixed);
            L.menu.w     = kMenuW * k;
            L.cena.w     = kCenaW * k;
            L.pause.w    = kIconBtn * k;
            L.play.w     = kIconBtn * k;
            L.mode3d.w   = kSegW * k;
            L.modeUi.w   = kSegW * k;
            L.modeAudio.w = (kSegW + 8.0f) * k;   // "ÁUDIO" é mais largo
            for (int i = 0; i < 4; ++i) {
                L.giz[i].w = kIconBtn * k;
            }
            L.inspector.w = kIconBtn * k;
            L.iconSize = 32.0f * k < 32.0f ? 32.0f * k : 32.0f;
        } else {
            sepGap = kSepGapN - (total - avail) / static_cast<f32>(nSeps);
        }
    } else {
        L.menu.w = kMenuW;
        L.cena.w = kCenaW;
        L.pause.w = kIconBtn;
        L.play.w = kIconBtn;
        L.mode3d.w = kSegW;
        L.modeUi.w = kSegW;
        L.modeAudio.w = kSegW + 8.0f;
        for (int i = 0; i < 4; ++i) {
            L.giz[i].w = kIconBtn;
        }
        L.inspector.w = kIconBtn;
    }
    L.sepGap = sepGap;
    if (L.iconSize < 18.0f) {
        L.iconSize = 18.0f;   // piso de legibilidade (ecrãs extremos)
    }

    // ---- colocação: fluxo à esquerda (G1→G4), G5 ancorado à direita -------
    const f32 by = L.bar.y + (L.bar.h - kBtnH) * 0.5f;
    const auto place = [](UiRect& r, f32 x, f32 y, f32 h) {
        r.x = x;
        r.y = y;
        r.h = h;
    };

    f32 x = L.bar.x + kPadOuter;
    place(L.menu, x, by, kBtnH);  x += L.menu.w;
    place(L.cena, x + kG1Gap, by, kBtnH);  x += kG1Gap + L.cena.w;
    L.sep[0] = {x + (sepGap - 1.0f) * 0.5f, L.bar.y + (L.bar.h - kSepH) * 0.5f,
                1.0f, kSepH};
    x += sepGap;

    place(L.pause, x, by, kBtnH);  x += L.pause.w;
    place(L.play, x + kG2Gap, by, kBtnH);  x += kG2Gap + L.play.w;
    L.sep[1] = {x + (sepGap - 1.0f) * 0.5f, L.bar.y + (L.bar.h - kSepH) * 0.5f,
                1.0f, kSepH};
    x += sepGap;

    place(L.mode3d, x, by, kBtnH);  x += L.mode3d.w;   // segmented: colados
    place(L.modeUi, x, by, kBtnH);  x += L.modeUi.w;
    place(L.modeAudio, x, by, kBtnH);  x += L.modeAudio.w;   // 0.8.11
    L.sep[2] = {x + (sepGap - 1.0f) * 0.5f, L.bar.y + (L.bar.h - kSepH) * 0.5f,
                1.0f, kSepH};
    x += sepGap;

    u32 sepIdx = 3;
    if (g4Visible) {
        for (int i = 0; i < 4; ++i) {   // segmented: colados
            place(L.giz[i], x, by, kBtnH);
            x += L.giz[i].w;
        }
        L.sep[sepIdx++] = {x + (sepGap - 1.0f) * 0.5f,
                           L.bar.y + (L.bar.h - kSepH) * 0.5f, 1.0f, kSepH};
        x += sepGap;
    }
    L.sepCount = sepIdx;

    // G5: borda direita da barra (nunca mexe quando o G4 aparece/desaparece)
    place(L.inspector, L.bar.x + L.bar.w - kPadOuter - L.inspector.w, by,
          kBtnH);
    return L;
}

namespace {

// baseline do texto centrada no botão (métricas REAIS da fonte)
f32 textBaseline(UiContext& ui, const UiRect& r) {
    if (!ui.hasFont()) {
        return r.y + r.h * 0.5f;
    }
    const TextMetrics m = ui.textMetrics();
    return r.y + (r.h - m.block()) * 0.5f + m.ascent;
}

// botão de TEXTO da G1 (mono: painel + moldura + rótulo + caret ▾ vetorial
// — o atlas é ASCII, o caret desenha-se com 2 traços no line batch)
bool textButton(UiContext& ui, u64 id, const UiRect& r, const char* text) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);

    ui.panel(r.x, r.y, r.w, r.h, held ? theme::ACCENT : theme::PANEL);
    ui.frame(r.x, r.y, r.w, r.h, 1.0f, theme::LINE);

    // rótulo deslocado p/ a esquerda (o caret vive à direita)
    const f32 caretW = 16.0f;
    const f32 usable = r.w - caretW - 8.0f;
    if (ui.hasFont()) {
        char fit[64];
        const char* shown = text;
        if (ui.fontWidth(text) > usable) {
            textfit::ellipsize(text, usable,
                               [&](const char* s) { return ui.fontWidth(s); },
                               fit, sizeof(fit));
            shown = fit;
        }
        const f32 tw = ui.fontWidth(shown);
        ui.label(r.x + (usable - tw) * 0.5f + 4.0f, textBaseline(ui, r), shown,
                 held ? theme::BG : theme::TEXT);
    }
    // caret ▾ (2 traços — nada de glifos fora do atlas)
    const f32 cy = r.y + r.h * 0.5f;
    const f32 cx = r.x + r.w - caretW * 0.5f - 2.0f;
    const f32 col[4] = {theme::TEXT[0], theme::TEXT[1], theme::TEXT[2],
                        held ? theme::BG[3] : theme::TEXT[3]};
    ui.drawLine(cx - 4.0f, cy - 2.0f, cx, cy + 2.5f, 2.5f, col);
    ui.drawLine(cx, cy + 2.5f, cx + 4.0f, cy - 2.0f, 2.5f, col);
    return pressed;
}

// botão de ÍCONE (G2/G4/G5): inativo = fundo transparente (a barra) + ícone
// na cor de marca; ativo/held = fundo de marca + ícone escuro (INVERSO)
bool iconButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
                bool active, f32 iconSize) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool on = active || ui.widgetActive(id);

    if (on) {
        ui.panel(r.x, r.y, r.w, r.h, theme::kTheme.brand);
    }
    const f32 s = iconSize < r.h - 12.0f ? iconSize : r.h - 12.0f;
    if (s >= 12.0f) {
        icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f,
                        r.y + (r.h - s) * 0.5f, s,
                        on ? theme::kTheme.brandInk : theme::kTheme.brand);
    }
    return pressed;
}

// segmento do G3 (texto): ativo = fundo de marca + texto escuro; inativo =
// transparente + texto mono
bool segmentText(UiContext& ui, u64 id, const UiRect& r, const char* text,
                 bool active) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool on = active || ui.widgetActive(id);
    if (on) {
        ui.panel(r.x, r.y, r.w, r.h, theme::kTheme.brand);
    }
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth(text);
        ui.label(r.x + (r.w - tw) * 0.5f, textBaseline(ui, r), text,
                 on ? theme::kTheme.brandInk : theme::TEXT);
    }
    return pressed;
}

} // namespace

// ---------------------------------------------------------------------------
// DRAW — a barra completa de um frame
// ---------------------------------------------------------------------------
Actions draw(UiContext& ui, EditorState& st, GizmoModeState& gz,
             bool hasSelection) {
    Actions a;
    const bool g4 = hasSelection && !st.uiMode;
    const Layout L = layout(ui.screenWidth(), ui.screenHeight(), ui.safeArea(),
                            g4);

    // fundo da barra (mais escuro que os painéis — o chrome separa-se da
    // área de trabalho) + risca inferior
    ui.panel(L.bar.x, L.bar.y, L.bar.w, L.bar.h, theme::kTheme.bg);
    ui.panel(L.bar.x, L.bar.y + L.bar.h - 1.0f, L.bar.w, 1.0f, theme::LINE);

    // G1 — sistema (texto + caret; ações raras)
    if (textButton(ui, kTbMenuId, L.menu, "Menu")) {
        a.menuDropdown = true;
    }
    if (textButton(ui, kTbCenaId, L.cena, "Cena")) {
        a.cenaDropdown = true;
    }

    // G2 — playback (ícones)
    if (iconButton(ui, kTbPauseId, L.pause, icons::Icon::Pause, false,
                   L.iconSize)) {
        a.pausePressed = true;
    }
    if (iconButton(ui, kTbPlayId, L.play, icons::Icon::Play, false,
                   L.iconSize)) {
        a.playPressed = true;
    }

    // G3 — modo (segmented 3D|UI|ÁUDIO; 0.8.11: o 3º estado)
    if (segmentText(ui, kModeUiId, L.modeUi, "UI", st.uiMode)) {
        st.uiMode = true;
        st.audioMode = false;   // 0.8.11: modos exclusivos
    }
    // 0.8.11 — ÁUDIO: o workspace substitui o viewport (clips/gravar/
    // waveform); os modos são exclusivos como 3D/UI
    if (segmentText(ui, kModeAudioId, L.modeAudio, "ÁUDIO", st.audioMode)) {
        st.audioMode = true;
        st.uiMode = false;
        st.selElement = -1;
        st.elDrag = false;
    }
    // voltar a 3D também desliga o ÁUDIO
    if (segmentText(ui, kMode3dId, L.mode3d, "3D", !st.uiMode && !st.audioMode)) {
        st.uiMode = false;
        st.audioMode = false;
        st.selElement = -1;
        st.elDrag = false;
    }

    // G4 — transformação (SÓ com seleção em 3D; segmented de ícones).
    // mover/rodar/escalar são EXCLUSIVOS; snap é um TOGGLE do grupo (liga o
    // snapping do modo ativo — não é um 4º estado de transformação).
    if (L.g4Visible) {
        static const icons::Icon kGizIcons[3] = {
            icons::Icon::Move, icons::Icon::Rotate, icons::Icon::Scale};
        for (int i = 0; i < 3; ++i) {
            if (iconButton(ui, kGizmoIds[i], L.giz[i], kGizIcons[i],
                           gz.mode == i, L.iconSize)) {
                gz.mode = i;
            }
        }
        if (iconButton(ui, kTbSnapId, L.giz[3], icons::Icon::Snap, gz.snap,
                       L.iconSize)) {
            gz.snap = !gz.snap;
        }
    }

    // G5 — painéis (inspector; ativo = painel visível)
    if (iconButton(ui, kTbInspectId, L.inspector, icons::Icon::Inspector,
                   st.showInspector, L.iconSize)) {
        st.showInspector = !st.showInspector;
    }

    // separadores finos entre grupos (a linha clara do tema mono)
    for (u32 i = 0; i < L.sepCount; ++i) {
        ui.panel(L.sep[i].x, L.sep[i].y, L.sep[i].w, L.sep[i].h, theme::LINE);
    }
    return a;
}

} // namespace toolbar
} // namespace editor
} // namespace vv
