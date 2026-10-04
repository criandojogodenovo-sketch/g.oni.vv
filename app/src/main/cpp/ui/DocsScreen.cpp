// ui/DocsScreen.cpp — o ecrã de Docs (contrato no header). As entradas
// vêm do voni/VoniDocs (DADO — o ecrã só pesquisa e desenha).
#include "ui/DocsScreen.h"

#include "ui/Icons.h"
#include "ui/Theme.h"
#include "voni/VoniDocs.h"
#include "voni/VoniRegistry.h"   // 0.9.5: a equivalência Python/JS (a
                                  // MESMA fonte — o registo central)

#include <cstring>

namespace vv {
struct InputState;

namespace editor {
namespace docswin {

namespace {

f32 lineHeight(UiContext& ui) {
    const TextMetrics m = ui.textMetrics();
    const f32 h = m.ascent + m.descent + 6.0f;
    return h > 28.0f ? h : 28.0f;
}

// altura de uma entrada: colapsada = 1 linha; expandida =
// +sintaxe+exemplo+EQUIVALÊNCIA (0.9.5: a tabela de equivalências Python/JS
// vive no registo e aparece aqui — quem sabe Python/JS reconhece à 1ª)
f32 entryHeight(UiContext& ui, bool expanded) {
    const f32 lh = lineHeight(ui);
    return expanded ? lh * 5.0f + 12.0f : lh;
}

} // namespace

int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h) {
    (void)in;
    if (!st.open) {
        return 0;
    }

    ui.panel(0.0f, 0.0f, w, h, theme::kTheme.bg);

    // ---- topo 56dp ---------------------------------------------------------
    ui.panel(0.0f, 0.0f, w, kTopH, theme::kTheme.surface);
    ui.panel(0.0f, kTopH - 1.0f, w, 1.0f, theme::kTheme.border);
    icons::drawIcon(ui, icons::Icon::Back, 16.0f, kTopH / 2.0f - 12.0f, 24.0f,
                    theme::kTheme.text1);
    int result = 0;
    if (ui.widgetHit(kBackId, 0.0f, 0.0f, kTopH, kTopH)) {
        result = 1;
    }
    ui.labelStyled(kTopH + 8.0f, 16.0f, "Docs", theme::kTheme.text1,
                   theme::fontScale(theme::kFontScreen), 0);
    ui.labelStyled(kTopH + 8.0f, 42.0f, "V.ONI — linguagem, comandos, linkers e tykers",
                   theme::kTheme.text2,
                   theme::fontScale(theme::kFontCaption), 0);

    // ---- campo de pesquisa 48dp COM LUPA (spec §11) -------------------------
    // FASE 9 (G0-3): o segundo panelRounded era um FILL da cor da borda por
    // CIMA do surface (o campo ficava um bloco sólido) — é um FRAME.
    const UiRect field{16.0f, kTopH + 8.0f, w - 32.0f, 48.0f};
    ui.panelRounded(field.x, field.y, field.w, field.h,
                    theme::kRadiusField, theme::kTheme.surface);
    ui.frameRounded(field.x, field.y, field.w, field.h, 1.0f,
                    theme::kRadiusField, theme::kTheme.border);
    icons::drawIcon(ui, icons::Icon::Search, field.x + 16.0f,
                    field.y + 12.0f, 24.0f, theme::kTheme.text2);
    if (st.queryLen) {
        ui.labelFitted(field.x + 52.0f, field.y + 14.0f, st.query,
                       theme::kTheme.text1, field.w - 68.0f);
    } else {
        ui.labelFitted(field.x + 52.0f, field.y + 14.0f, "pesquisar comando",
                       theme::kTheme.text2, field.w - 68.0f);
    }
    // o toque no campo abre o teclado IN-APP (o main trata do purpose 9 —
    // devolvemos 4 para o main saber que foi o campo das Docs)
    if (ui.widgetHit(kSearchId, field.x, field.y, field.w, field.h)) {
        result = 4;
    }

    // ---- lista com scroll ---------------------------------------------------
    const UiRect listRegion{0.0f, field.y + field.h + 8.0f, w,
                            h - (field.y + field.h + 8.0f)};
    const std::string query(st.query, st.queryLen);
    auto entries = voni::docs::search(query);

    const f32 lh = lineHeight(ui);
    f32 contentH = 0.0f;
    for (size_t i = 0; i < entries.size(); ++i) {
        contentH += entryHeight(ui, (int)i == st.expanded);
    }
    ui.beginScroll(kScrollId, listRegion, contentH);
    const f32 off = ui.scrollOffset();

    f32 y = 0.0f;
    for (size_t i = 0; i < entries.size(); ++i) {
        const voni::docs::Entry& e = *entries[i];
        const bool expanded = (int)i == st.expanded;
        const f32 eh = entryHeight(ui, expanded);

        // linha: NOME 14sp bold-ish + categoria à direita 12sp text2
        ui.labelStyled(16.0f, y + 6.0f, e.name, theme::kTheme.text1,
                       theme::fontScale(theme::kFontBody), 0);
        const f32 catX = w - 16.0f - 96.0f;
        ui.labelStyled(catX, y + 8.0f, voni::docs::catName(e.cat),
                       theme::kTheme.text2,
                       theme::fontScale(theme::kFontCaption), 0);
        // 1 linha de descrição (12sp text2)
        ui.labelFitted(16.0f, y + lh - 8.0f, e.desc, theme::kTheme.text2,
                       w - 32.0f);
        if (expanded) {
            // sintaxe (accent) + exemplo (user/mono)
            ui.labelFitted(16.0f, y + lh + 4.0f, e.syntax,
                           theme::kTheme.accent, w - 32.0f);
            ui.labelFitted(16.0f, y + 2.0f * lh + 8.0f, e.example,
                           theme::kTheme.voniUser, w - 32.0f);
            // 0.9.5 · A EQUIVALÊNCIA Python/JS (do registo — a tabela que
            // o editor que ensina usa nos erros e na strip)
            if (const voni::reg::Entry* re = voni::reg::find(e.name)) {
                if (re->equiv && *re->equiv) {
                    char eq[160];
                    std::snprintf(eq, sizeof(eq), "Python/JS: %s",
                                  re->equiv);
                    ui.labelFitted(16.0f, y + 3.0f * lh + 12.0f, eq,
                                   theme::kTheme.text2, w - 32.0f);
                }
            }
        }
        // separador fino
        ui.panel(16.0f, y + eh - 1.0f, w - 32.0f, 1.0f,
                 theme::kTheme.border);

        // tap na entrada = expandir/colapsar (a linha INTEIRA é alvo)
        if (ui.widgetHit(kEntryBase + (u64)i, 0.0f, listRegion.y + y - off, w,
                         eh)) {
            st.expanded = expanded ? -1 : (int)i;
        }
        y += eh;
    }
    ui.endScroll();

    return result;
}

} // namespace docswin
} // namespace editor
} // namespace vv
