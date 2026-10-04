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

// 0.9.2 — id da linha Docs (faixa do Settings; imediato único)
constexpr u64 kDocsRowId = 6409;

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

// rect do BOTÃO de uma actionRow (FONTE ÚNICA — o draw e o re-despacho do
// scrollTap partilham a MESMA matemática; o scroll reclama o gesto dentro
// da região e o tap volta por scrollTap, por isso o rect tem de ser
// recalculável fora do draw)
UiRect actionBtnRect(f32 x, f32 y, f32 w) {
    const f32 bw = 152.0f;
    return UiRect{x + w - 16.0f - bw, y + 4.0f, bw, kRowH - 8.0f};
}

// linha com rótulo + BOTÃO (ação) à direita
bool actionRow(UiContext& ui, u64 id, f32 x, f32 y, f32 w, const char* label,
               const char* btn) {
    if (ui.hasFont()) {
        ui.labelFitted(x + 16.0f, baseline(ui, {x, y, w, kRowH}), label,
                       theme::kTheme.text1, w * 0.55f);
    }
    const UiRect b = actionBtnRect(x, y, w);
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
    // 0.9.6 (G1) — ECRÃ CHEIO MODAL (a regra das camadas): o Settings deixa
    // de ser a BANDA do overlayArea (que deixava o glifo amarelo do áudio e
    // o orbit da câmara por trás) e passa a ocupar o contentRect TODO com o
    // CABEÇALHO PADRÃO: fundo na superfície inteira (a faixa do sistema
    // fica por trás), cabeçalho = inset do topo + 56dp (o título nunca
    // corta sob a faixa preta), conteúdo até ao inset de baixo/laterais.
    // Em desktop/tests insets=0 — a banda e o ecrã cheio coincidem.
    const safe::Insets ins = ui.safeArea();
    const f32 ox = ins.left;
    const f32 oy = ins.top;
    const f32 aw = ui.screenWidth() - ins.left - ins.right;
    const f32 ah = ui.screenHeight() - ins.top - ins.bottom;
    ui.panel(0.0f, 0.0f, ui.screenWidth(), ui.screenHeight(),
             theme::kTheme.surface);

    // ---- BACK 48dp + título 20sp no CABEÇALHO PADRÃO (56dp) ---------------
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
        // 0.9.6 (G1-2): a baseline do CABEÇALHO PADRÃO (a MESMA das outras
        // telas — Theme é a fonte única; o título 20sp centrado sem corte)
        ui.labelStyled(back.x + back.w + 12.0f,
                       oy + theme::kHeaderTitleBase, "Settings",
                       theme::kTheme.text1,
                       theme::fontScale(theme::kFontScreen), 0);
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
        {kBitAudio, "Áudio", 3},       // volume · fonte · reconverter
        {kBitPerm, "Permissões", 2},   // all files · mic
        {kBitDiag, "Diagnóstico", 6},  // ver logs · export · probe · dumps · modo · texto
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
    // 0.9.6 (G2-6): o cabeçalho de secção NUNCA fica cortado sob o título
    // — o header que atravessa o topo da região faz PIN (sticky): desenha
    // inteiro colado ao topo enquanto a sua secção ainda está visível; os
    // que já subiram TODO não desenham (o clip comeria metade deles)
    const f32 regionTop = region.y;
    auto stickyY = [regionTop](f32 yy) {
        if (yy + kSectionH <= regionTop) {
            return yy;   // já subiu todo — o clip do scroll esconde
        }
        return yy < regionTop ? regionTop : yy;
    };

    // ---- GERAL ---------------------------------------------------------------
    if (sectionHeader(ui, kSectionBase + 0, kBitGeral, st.settingsCollapsed, ox,
                      stickyY(y), aw, "Geral")) {
        st.settingsCollapsed ^= kBitGeral;
    }
    y += kSectionH;
    if (!(collapsed & kBitGeral)) {
        char ver[64];
        std::snprintf(ver, sizeof(ver), "%s", ctx.version);
        infoRow(ui, ox, y, aw, "versão / build", ver);
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
                      stickyY(y), aw, "Áudio")) {
        st.settingsCollapsed ^= kBitAudio;
    }
    y += kSectionH;
    if (!(collapsed & kBitAudio)) {
        char vol[32];
        std::snprintf(vol, sizeof(vol), "%d%%",
                      static_cast<int>(ctx.audioMaster * 100.0f + 0.5f));
        infoRow(ui, ox, y, aw, "volume geral", vol);
        y += kRowH;
        infoRow(ui, ox, y, aw, "fonte após import",
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
                      stickyY(y), aw, "Permissões")) {
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
                      stickyY(y), aw, "Diagnóstico")) {
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
        // 0.9.6 (G6 · R-017) — BENCHMARKS REAIS: duas linhas (correr +
        // copiar). Todo o número do bloco de 9 linhas é MEDIDO; o que não
        // pôde ser medido diz "não medido" (a honestidade é a sentinela)
        if (actionRow(ui, kRunBenchId, ox, y, aw,
                      "fps/import/memória (~20s)", "Correr bench")) {
            res = kRunBench;
        }
        y += kRowH;
        if (actionRow(ui, kCopyBenchId, ox, y, aw,
                      "bloco de 9 linhas", "Copiar relatório")) {
            res = kCopyBench;
        }
        y += kRowH;
        // 0.9.1 — JANELA DE TEXTO: portrait + IME do sistema (a semente do
        // editor de script 0.9.2; vive em Diagnóstico enquanto não há
        // componente Script num TIC — decisão documentada no relatório)
        if (actionRow(ui, kTextWindowId, ox, y, aw,
                      "editor de texto (IME)", "abrir")) {
            res = kOpenTextWindow;
        }
        y += kRowH;
        // 0.9.6 (G2-6): a nota "badge ANTIGO" é DEBUG do viewer — aqui
        // mostra-se SÓ o número (o badge vive no log viewer, onde o dono
        // compara dumps; R-004 continua vigiado lá)
        char dumps[32];
        std::snprintf(dumps, sizeof(dumps), "%u", ctx.dumpCount);
        infoRow(ui, ox, y, aw, "crash dumps", dumps);
        y += kRowH;
        infoRow(ui, ox, y, aw, "armazenamento",
                ctx.storageMode[0] ? ctx.storageMode : "-");
        y += kRowH;
    }

    // ---- DOCS (0.9.0: a secção EXISTE sem a linha — entra na 0.9.2) ----------
    if (sectionHeader(ui, kSectionBase + 4, kBitDocs, st.settingsCollapsed, ox,
                      stickyY(y), aw, "Docs")) {
        st.settingsCollapsed ^= kBitDocs;
    }
    y += kSectionH;
    if (!(collapsed & kBitDocs)) {
        // 0.9.2 §11: A LINHA entrou — abre o ecrã de Docs (pesquisa lupa)
        if (ui.hasFont()) {
            const TextMetrics m = ui.textMetrics();
            const f32 base = y + (kRowH - m.block()) * 0.5f + m.ascent;
            ui.labelFitted(ox + 16.0f, base, "Ver docs da V.ONI",
                           theme::kTheme.text1, aw - 64.0f);
            icons::drawIcon(ui, icons::Icon::ChevronRight,
                            ox + aw - 40.0f, y + (kRowH - 24.0f) * 0.5f,
                            24.0f, theme::kTheme.text2);
        }
        if (ui.widgetHit(kDocsRowId, ox, y, aw, kRowH)) {
            res = kOpenDocs;
        }
        y += kRowH;
    }

    // ---- SOBRE ---------------------------------------------------------------
    if (sectionHeader(ui, kSectionBase + 5, kBitSobre, st.settingsCollapsed, ox,
                      stickyY(y), aw, "Sobre")) {
        st.settingsCollapsed ^= kBitSobre;
    }
    y += kSectionH;
    if (!(collapsed & kBitSobre)) {
        char sha[24];
        std::snprintf(sha, sizeof(sha), "%.20s…", ctx.soSha);
        infoRow(ui, ox, y, aw, "so sha256", sha);
        y += kRowH;
        infoRow(ui, ox, y, aw, "licenças", "motor próprio + zlib/minimp3/stb_vorbis");
        y += kRowH;
    }

    ui.endScroll();

    // ---- tap re-despachado (os headers vivem DENTRO do scroll — o padrão
    // da casa: widgetHit só desenha, o scroll devolve o tap DEPOIS do fim)
    f32 tpx = 0.0f, tpy = 0.0f;
    if (res == kNone && ui.scrollTap(kScrollId, tpx, tpy)) {
        // 0.9.1 — FIX da mecânica de toque: os alvos DENTRO da região de
        // scroll (headers, botões de ação, toggles) não capturam pelo
        // widgetHit (scroll::buttonCaptures=false — o scroll reclama o
        // gesto); o tap volta AQUI e é re-despachado por COORDENADAS com a
        // MESMA matemática do draw (o walk abaixo avança y na mesma ordem).
        // O walk antigo só via os HEADERS — os botões de ação/toggles da
        // página estavam MORTOS (Ver logs/Export/Probe/reconverter/All
        // Files/Mic/Repor layout/Imersivo) — apanhado pelo teste do tap na
        // nova linha "editor de texto (IME)".
        f32 hy = oy + 56.0f + 8.0f - off;
        // a linha B de uma actionRow / a linha inteira do toggle:
        const auto hit = [](f32 px, f32 py, const UiRect& r) {
            return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
        };
        // walk: cada secção = header + (linhas SE aberta, na ordem do draw)
        const struct { u32 bit; bool open; } secs3[6] = {
            {kBitGeral, !(collapsed & kBitGeral)},
            {kBitAudio, !(collapsed & kBitAudio)},
            {kBitPerm,  !(collapsed & kBitPerm)},
            {kBitDiag,  !(collapsed & kBitDiag)},
            {kBitDocs,  !(collapsed & kBitDocs)},
            {kBitSobre, !(collapsed & kBitSobre)},
        };
        for (const auto& s3 : secs3) {
            // HEADER (colapsar/expandir)
            if (hit(tpx, tpy, UiRect{ox, hy, aw, kSectionH})) {
                st.settingsCollapsed ^= s3.bit;
                break;
            }
            hy += kSectionH;
            if (!s3.open) {
                continue;
            }
            // as linhas da secção (a MESMA ordem do draw) — só as que têm
            // alvo: action (botão à direita) e toggle (linha inteira);
            // infoRows não têm alvo e avançam cursor.
            switch (s3.bit) {
                case kBitGeral:
                    hy += kRowH;   // info versão
                    if (hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kResetLayout;   // Repor layout
                    }
                    hy += kRowH;
                    if (res == kNone &&
                        hit(tpx, tpy, UiRect{ox, hy, aw, kRowH})) {
                        res = kToggleImmersive;
                    }
                    hy += kRowH;
                    break;
                case kBitAudio:
                    hy += kRowH;   // info volume
                    hy += kRowH;   // info fonte
                    if (hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kReconvert;
                    }
                    hy += kRowH;
                    break;
                case kBitPerm:
                    if (hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kAllFilesPressed;
                    }
                    hy += kRowH;
                    if (res == kNone &&
                        hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kMicPressed;
                    }
                    hy += kRowH;
                    break;
                case kBitDiag:
                    if (hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kViewLogs;
                    }
                    hy += kRowH;
                    if (res == kNone &&
                        hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kExportLogs;
                    }
                    hy += kRowH;
                    if (res == kNone &&
                        hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kProbeAudio;
                    }
                    hy += kRowH;
                    // 0.9.6 (G6 · R-017): os DOIS botões do bench — sem
                    // ESTAS linhas o walk não re-despacha e o botão nasce
                    // MORTO (a mesma classe do bug 0.9.1: desenhar ≠ tocar;
                    // apanhado pela FASE 12.9 no primeiro run do harness)
                    if (res == kNone &&
                        hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kRunBench;
                    }
                    hy += kRowH;
                    if (res == kNone &&
                        hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kCopyBench;
                    }
                    hy += kRowH;
                    if (res == kNone &&
                        hit(tpx, tpy, actionBtnRect(ox, hy, aw))) {
                        res = kOpenTextWindow;   // 0.9.1
                    }
                    hy += kRowH;
                    hy += kRowH;   // info dumps
                    hy += kRowH;   // info armazenamento
                    break;
                case kBitDocs:
                    // FASE 9 (G0-3): a linha "Ver docs da V.ONI" existia
                    // desde a 0.9.2 mas o walk NUNCA a re-despachava (só
                    // avançava o cursor) — o toque morria no scroll e o
                    // ecrã de Docs ficava INALCANÇÁVEL. Linha INTEIRA é o
                    // alvo (o mesmo hit-test do draw).
                    if (hit(tpx, tpy, UiRect{ox, hy, aw, kRowH})) {
                        res = kOpenDocs;
                    }
                    hy += kRowH;
                    break;
                case kBitSobre:
                    hy += kRowH;   // info sha256
                    hy += kRowH;   // info licenças
                    break;
            }
            if (res != kNone) {
                break;
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
