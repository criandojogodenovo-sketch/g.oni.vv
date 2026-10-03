// ui/SettingsPage.cpp — implementação da página de Settings (0.9.0, spec I).
//
// Tudo theme::kTheme (tabela spec A): fundo surface, secções colapsáveis com
// chevron, linhas 48dp, toggles à direita (pill accent quando ON), rótulos
// só-leitura em text-2. O scroll (id 49) revela o fundo; o BACK (56dp, ícone
// seta) devolve ao editor SEM tocar na seleção.
#include "ui/SettingsPage.h"
#include "ui/EditorUi.h"
#include "ui/UiContext.h"

#include <cstdio>
#include <cstring>

namespace vv {
namespace editor {
namespace settings {

namespace {

f32 baseline(UiContext& ui, const UiRect& r) {
    if (!ui.hasFont()) {
        return r.y + r.h * 0.5f;
    }
    const TextMetrics m = ui.textMetrics();
    return r.y + (r.h - m.block()) * 0.5f + m.ascent;
}

// linha com rótulo à esquerda + VALOR só-leitura à direita (text-2)
void infoRow(UiContext& ui, f32 x, f32 y, f32 w, const char* label,
             const char* value) {
    if (ui.hasFont()) {
        ui.labelFitted(x + 16.0f, baseline(ui, {x, y, w, kRowH}), label,
                       theme::kTheme.text1, w * 0.5f);
        const f32 vw = ui.fontWidth(value);
        ui.labelFitted(x + w - 16.0f - vw, baseline(ui, {x, y, w, kRowH}), value,
                       theme::kTheme.text2, w * 0.45f);
    }
}

// linha com rótulo + BOTÃO (ação) à direita
bool actionRow(UiContext& ui, u64 id, f32 x, f32 y, f32 w, const char* label,
               const char* btn) {
    if (ui.hasFont()) {
        ui.labelFitted(x + 16.0f, baseline(ui, {x, y, w, kRowH}), label,
                       theme::kTheme.text1, w * 0.55f);
    }
    const f32 bw = 152.0f;
    const UiRect b = {x + w - 16.0f - bw, y + 4.0f, bw, kRowH - 8.0f};
    const bool held = ui.widgetActive(id);
    ui.panelRounded(b.x, b.y, b.w, b.h, theme::kRadiusCard,
                    held ? theme::kTheme.accentPress : theme::kTheme.accent);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth(btn);
        ui.label(b.x + (b.w - tw) * 0.5f, baseline(ui, b), btn,
                 theme::kTheme.accentInk);
    }
    return ui.widgetHit(id, b.x, b.y, b.w, b.h);
}

// linha com rótulo + TOGGLE (pill accent ON / bordo OFF)
bool toggleRow(UiContext& ui, u64 id, f32 x, f32 y, f32 w, const char* label,
               bool on) {
    if (ui.hasFont()) {
        ui.labelFitted(x + 16.0f, baseline(ui, {x, y, w, kRowH}), label,
                       theme::kTheme.text1, w * 0.6f);
    }
    // o TOGGLE (a linha INTEIRA é o alvo — 48dp)
    const f32 tw = 56.0f, th = 28.0f;
    const UiRect t = {x + w - 16.0f - tw, y + (kRowH - th) * 0.5f, tw, th};
    ui.panelPill(t.x, t.y, t.w, t.h, on ? theme::kTheme.accent
                                        : theme::kTheme.bg);
    ui.frameRounded(t.x, t.y, t.w, t.h, 1.0f, t.h * 0.5f,
                    on ? theme::kTheme.accent : theme::kTheme.border);
    // o "botão" do toggle desliza
    const f32 knob = on ? t.w - th - 2.0f : 2.0f;
    ui.panelPill(t.x + knob, t.y + 2.0f, th - 4.0f, th - 4.0f,
                 on ? theme::kTheme.accentInk : theme::kTheme.text2);
    return ui.widgetHit(id, x, y, w, kRowH);
}

// cabeçalho de secção (colapsável): chevron + título 14sp
bool sectionHeader(UiContext& ui, u64 id, u32 bit, u32& collapsed, f32 x, f32 y,
                   f32 w, const char* title) {
    const bool open = !(collapsed & bit);
    if (ui.widgetActive(id)) {
        ui.panel(x + 4.0f, y, w - 8.0f, kSectionH, theme::kTheme.surface2);
    }
    if (ui.hasFont()) {
        ui.label(x + 16.0f, baseline(ui, {x, y, w, kSectionH}), title,
                 theme::kTheme.text1);
    }
    icons::drawIcon(ui, open ? icons::Icon::ChevronDown
                             : icons::Icon::ChevronRight,
                    x + w - 16.0f - 24.0f, y + (kSectionH - 24.0f) * 0.5f,
                    24.0f, theme::kTheme.text2);
    ui.panel(x + 16.0f, y + kSectionH - 1.0f, w - 32.0f, 1.0f,
             theme::kTheme.border);
    return ui.widgetHit(id, x, y, w, kSectionH);
}

} // namespace

Result draw(UiContext& ui, const InputState& in, EditorState& st, const Ctx& ctx) {
    (void)in;
    // página INTEIRA na banda do viewport (spec I: página scrollável)
    f32 ox, oy, aw, ah;
    overlayArea(ui.screenWidth(), ui.screenHeight(), ui.safeArea(), ox, oy, aw,
                ah);
    ui.panel(ox, oy, aw, ah, theme::kTheme.surface);

    // ---- BACK 56dp + título 20sp ------------------------------------------
    const UiRect back = {ox + 8.0f, oy + 4.0f, 48.0f, 48.0f};
    const bool backHeld = ui.widgetActive(kBackId);
    if (backHeld) {
        ui.panelRounded(back.x, back.y, back.w, back.h, theme::kRadiusCard,
                        theme::kTheme.surface2);
    }
    icons::drawIcon(ui, icons::Icon::Back, back.x + (back.w - 24.0f) * 0.5f,
                    back.y + (back.h - 24.0f) * 0.5f, 24.0f,
                    theme::kTheme.text1);
    if (ui.hasFont()) {
        ui.label(back.x + back.w + 12.0f,
                 baseline(ui, {ox, oy + 4.0f, aw, 48.0f}), "Settings",
                 theme::kTheme.text1);
    }
    ui.panel(ox, oy + 56.0f, aw, 1.0f, theme::kTheme.border);
    const bool backPressed =
        ui.widgetHit(kBackId, back.x, back.y, back.w, back.h);

    // ---- conteúdo (scroll id 49) -------------------------------------------
    // colapsáveis: bits persistem em st.settingsCollapsed (layout.json)
    const u32& collapsed = st.settingsCollapsed;
    struct Sec {
        u32 bit;
        const char* title;
        u32 rows;
    };
    const Sec secs[6] = {
        {kBitGeral, "Geral", 3},       // versão/build · repor layout · imersivo
        {kBitAudio, "Audio", 3},       // volume · fonte · reconverter
        {kBitPerm, "Permissoes", 2},   // all files · mic
        {kBitDiag, "Diagnostico", 5},  // ver logs · export · probe · dumps · modo
        {kBitDocs, "Docs", 1},         // (0.9.2 — spec I: existe SEM linha)
        {kBitSobre, "Sobre", 2},       // sha256 · licencças
    };
    f32 contentH = 8.0f;
    for (const Sec& s : secs) {
        contentH += kSectionH;
        if (!(collapsed & s.bit)) {
            contentH += static_cast<f32>(s.rows) * kRowH;
        }
    }
    const UiRect region = {ox, oy + 56.0f, aw, ah - 56.0f};
    ui.beginScroll(kScrollId, region, contentH);
    const f32 off = ui.scrollOffset();
    f32 y = oy + 56.0f + 8.0f - off;
    Result res = kNone;

    // ---- GERAL ---------------------------------------------------------------
    if (sectionHeader(ui, kSectionBase + 0, kBitGeral, st.settingsCollapsed, ox,
                      y, aw, "Geral")) {
        st.settingsCollapsed ^= kBitGeral;
    }
    y += kSectionH;
    if (!(collapsed & kBitGeral)) {
        char ver[64];
        std::snprintf(ver, sizeof(ver), "%s", ctx.version);
        infoRow(ui, ox, y, aw, "versao / build", ver);
        y += kRowH;
        if (actionRow(ui, kResetLayoutId, ox, y, aw, "layout",
                      "Repor layout")) {
            res = kResetLayout;
        }
        y += kRowH;
        if (toggleRow(ui, kImmersiveId, ox, y, aw, "Imersivo",
                      ctx.immersive)) {
            res = kToggleImmersive;
        }
        y += kRowH;
    }

    // ---- ÁUDIO ---------------------------------------------------------------
    if (sectionHeader(ui, kSectionBase + 1, kBitAudio, st.settingsCollapsed, ox,
                      y, aw, "Audio")) {
        st.settingsCollapsed ^= kBitAudio;
    }
    y += kSectionH;
    if (!(collapsed & kBitAudio)) {
        char vol[32];
        std::snprintf(vol, sizeof(vol), "%d%%",
                      static_cast<int>(ctx.audioMaster * 100.0f + 0.5f));
        infoRow(ui, ox, y, aw, "volume geral", vol);
        y += kRowH;
        infoRow(ui, ox, y, aw, "fonte apos import",
                ctx.keepSource ? "manter" : "largar");
        y += kRowH;
        if (actionRow(ui, 5829, ox, y, aw, "assets de source/",
                      "reconverter")) {
            res = kReconvert;
        }
        y += kRowH;
    }

    // ---- PERMISSÕES ----------------------------------------------------------
    if (sectionHeader(ui, kSectionBase + 2, kBitPerm, st.settingsCollapsed, ox,
                      y, aw, "Permissoes")) {
        st.settingsCollapsed ^= kBitPerm;
    }
    y += kSectionH;
    if (!(collapsed & kBitPerm)) {
        if (actionRow(ui, kAllFilesId, ox, y, aw, "Todos os ficheiros",
                      ctx.allFilesGranted ? "concedido" : "abrir")) {
            res = kAllFilesPressed;
        }
        y += kRowH;
        if (actionRow(ui, kMicId, ox, y, aw, "Microfone",
                      ctx.micGranted ? "concedido" : "pedir")) {
            res = kMicPressed;
        }
        y += kRowH;
    }

    // ---- DIAGNÓSTICO ---------------------------------------------------------
    if (sectionHeader(ui, kSectionBase + 3, kBitDiag, st.settingsCollapsed, ox,
                      y, aw, "Diagnostico")) {
        st.settingsCollapsed ^= kBitDiag;
    }
    y += kSectionH;
    if (!(collapsed & kBitDiag)) {
        if (actionRow(ui, kViewLogsId, ox, y, aw, "logs do engine",
                      "Ver logs")) {
            res = kViewLogs;
        }
        y += kRowH;
        if (actionRow(ui, kExportLogsId, ox, y, aw, "copiar para Downloads",
                      "Export")) {
            res = kExportLogs;
        }
        y += kRowH;
        if (actionRow(ui, kProbeId, ox, y, aw, "audio (50+10 ciclos)",
                      "Probe")) {
            res = kProbeAudio;
        }
        y += kRowH;
        char dumps[32];
        std::snprintf(dumps, sizeof(dumps), "%u (badge ANTIGO no viewer)",
                      ctx.dumpCount);
        infoRow(ui, ox, y, aw, "crash dumps", dumps);
        y += kRowH;
        infoRow(ui, ox, y, aw, "armazenamento",
                ctx.storageMode[0] ? ctx.storageMode : "-");
        y += kRowH;
    }

    // ---- DOCS (0.9.0: a secção EXISTE sem a linha — entra na 0.9.2) ----------
    if (sectionHeader(ui, kSectionBase + 4, kBitDocs, st.settingsCollapsed, ox,
                      y, aw, "Docs")) {
        st.settingsCollapsed ^= kBitDocs;
    }
    y += kSectionH;
    if (!(collapsed & kBitDocs)) {
        if (ui.hasFont()) {
            ui.labelFitted(ox + 16.0f, baseline(ui, {ox, y, aw, kRowH}),
                           "documentacao dos comandos (0.9.2)",
                           theme::kTheme.text2, aw - 32.0f);
        }
        y += kRowH;
    }

    // ---- SOBRE ---------------------------------------------------------------
    if (sectionHeader(ui, kSectionBase + 5, kBitSobre, st.settingsCollapsed, ox,
                      y, aw, "Sobre")) {
        st.settingsCollapsed ^= kBitSobre;
    }
    y += kSectionH;
    if (!(collapsed & kBitSobre)) {
        char sha[24];
        std::snprintf(sha, sizeof(sha), "%.20s…", ctx.soSha);
        infoRow(ui, ox, y, aw, "so sha256", sha);
        y += kRowH;
        infoRow(ui, ox, y, aw, "licencas", "motor proprio + zlib/minimp3/stb_vorbis");
        y += kRowH;
    }

    ui.endScroll();

    // ---- tap re-despachado (os headers vivem DENTRO do scroll — o padrão
    // da casa: widgetHit só desenha, o scroll devolve o tap DEPOIS do fim)
    f32 tpx = 0.0f, tpy = 0.0f;
    if (res == kNone && ui.scrollTap(kScrollId, tpx, tpy)) {
        // recalcula as posições dos headers (a MESMA matemática do draw)
        f32 hy = oy + 56.0f + 8.0f - off;
        const struct {
            u32 bit;
            u32 rowsIfOpen;
        } sec2[6] = {
            {kBitGeral, 3}, {kBitAudio, 3}, {kBitPerm, 2},
            {kBitDiag, 5},  {kBitDocs, 1},  {kBitSobre, 2},
        };
        for (int i = 0; i < 6; ++i) {
            if (tpy >= hy && tpy < hy + kSectionH && tpx >= ox &&
                tpx < ox + aw) {
                st.settingsCollapsed ^= sec2[i].bit;
                break;
            }
            hy += kSectionH;
            if (!(collapsed & sec2[i].bit)) {
                hy += static_cast<f32>(sec2[i].rowsIfOpen) * kRowH;
            }
        }
    }

    if (backPressed || res != kNone) {
        // BACK fecha a página (a seleção fica INTACTA — spec I)
        st.settingsMenu = false;
        return backPressed ? kBackPressed : res;
    }
    return kNone;
}

} // namespace settings
} // namespace editor
} // namespace vv
