// ui/DocsScreen.cpp — o ecrã de Docs (contrato no header). As entradas
// vêm do voni/VoniDocs (DADO — o ecrã só pesquisa e desenha).
#include "ui/DocsScreen.h"

#include "ui/Icons.h"
#include "ui/TextFit.h"     // 0.9.6 (G2-5): textwrap — a descrição faz
                             // QUEBRA DE LINHA em vez de "…"
#include "ui/Theme.h"
#include "voni/VoniDocs.h"
#include "voni/VoniRegistry.h"   // 0.9.5: a equivalência Python/JS (a
                                  // MESMA fonte — o registo central)

#include <cstring>
#include <vector>

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

// 0.9.6 (G2-5) — A ALTURA DE UMA ENTRADA É MEDIDA DO TEXTO REAL (nunca
// fixa): o nome em cima (1 linha), a descrição POR BAIXO com QUEBRA DE
// LINHA pelo texto completo (wrap por palavras — nada de "…"), +8dp de
// espaço até à entrada seguinte; expandida soma sintaxe + exemplo +
// equivalência (também com wrap). O medidor é o MESMO do desenho.
u32 wrappedLines(UiContext& ui, const char* text, f32 maxW) {
    std::vector<textwrap::Line> lines;
    textwrap::wrap(text, maxW,
                   [&](const char* s) { return ui.fontWidth(s); }, lines);
    const u32 n = (u32)lines.size();
    return n == 0 ? (text && text[0] ? 1u : 0u) : n;
}

f32 entryHeight(UiContext& ui, const voni::docs::Entry& e, bool expanded,
                f32 textW) {
    const f32 lh = lineHeight(ui);
    f32 h = lh;                                   // o nome (1 linha)
    h += static_cast<f32>(wrappedLines(ui, e.desc, textW)) * lh;  // descrição
    if (expanded) {
        h += static_cast<f32>(wrappedLines(ui, e.syntax, textW)) * lh;
        h += static_cast<f32>(wrappedLines(ui, e.example, textW)) * lh;
        if (const voni::reg::Entry* re = voni::reg::find(e.name)) {
            if (re->equiv && *re->equiv) {
                char eq[192];
                std::snprintf(eq, sizeof(eq), "Python/JS: %s", re->equiv);
                h += static_cast<f32>(wrappedLines(ui, eq, textW)) * lh;
            }
        }
    }
    return h + 8.0f;   // 8dp até à entrada seguinte (spec G2-5)
}

} // namespace

int draw(UiContext& ui, const InputState& in, State& st, f32 w, f32 h) {
    (void)in;
    if (!st.open) {
        return 0;
    }

    // 0.9.6 (G1-1/G1-2 — SAFE AREA): os insets REAIS (contentRect →
    // g_ui.safeArea() — o ÚNICO sítio onde vivem) afetam o ecrã TODO:
    // o fundo cobre a superfície inteira (a faixa do sistema fica por
    // trás), mas o CABEÇALHO começa NO inset do topo (altura = inset +
    // 56dp — a faixa preta de ~96px já não CORTA o título) e o conteúdo
    // pára no inset de baixo/laterais (nada desenha sob a barra de
    // navegação). Em desktop/tests insets=0 — layout idêntico ao de sempre.
    const safe::Insets ins = ui.safeArea();
    const f32 topY = ins.top;
    const f32 hdrH = safe::kTopBarH;   // 56dp (a PARTE ÚTIL do cabeçalho)
    const f32 contentW = w - ins.left - ins.right;

    ui.panel(0.0f, 0.0f, w, h, theme::kTheme.bg);

    // ---- topo: inset + 56dp ------------------------------------------------
    ui.panel(ins.left, topY, contentW, hdrH, theme::kTheme.surface);
    ui.panel(ins.left, topY + hdrH - 1.0f, contentW, 1.0f,
             theme::kTheme.border);
    icons::drawIcon(ui, icons::Icon::Back, ins.left + 16.0f,
                    topY + hdrH / 2.0f - 12.0f, 24.0f, theme::kTheme.text1);
    int result = 0;
    if (ui.widgetHit(kBackId, ins.left, topY, hdrH, hdrH)) {
        result = 1;
    }
    // título/subtítulo DENTRO da parte útil (nunca sob a faixa do sistema)
    ui.labelStyled(ins.left + hdrH + 8.0f, topY + theme::kHeaderTitleBase,
                   "Docs", theme::kTheme.text1,
                   theme::fontScale(theme::kFontScreen), 0);
    ui.labelStyled(ins.left + hdrH + 8.0f, topY + theme::kHeaderSubBase,
                   "V.ONI — linguagem, comandos, linkers e tykers",
                   theme::kTheme.text2,
                   theme::fontScale(theme::kFontCaption), 0);

    // ---- campo de pesquisa 48dp COM LUPA (spec §11) -------------------------
    // FASE 9 (G0-3): o segundo panelRounded era um FILL da cor da borda por
    // CIMA do surface (o campo ficava um bloco sólido) — é um FRAME.
    const UiRect field{ins.left + 16.0f, topY + hdrH + 8.0f,
                       contentW - 32.0f, 48.0f};
    ui.panelRounded(field.x, field.y, field.w, field.h,
                    theme::kRadiusField, theme::kTheme.surface);
    ui.frameRounded(field.x, field.y, field.w, field.h, 1.0f,
                    theme::kRadiusField, theme::kTheme.border);
    icons::drawIcon(ui, icons::Icon::Search, field.x + 16.0f,
                    field.y + 12.0f, 24.0f, theme::kTheme.text2);
    // 0.9.6 (G2-5): o texto da pesquisa CENTRADO VERTICALMENTE no campo
    // (a altura real do bloco, não um offset fixo)
    {
        f32 ty = field.y + 14.0f;
        if (ui.hasFont()) {
            const TextMetrics m = ui.textMetrics();
            ty = field.y + (field.h - m.block()) * 0.5f + m.ascent;
        }
        if (st.queryLen) {
            ui.labelFitted(field.x + 52.0f, ty, st.query,
                           theme::kTheme.text1, field.w - 68.0f);
        } else {
            ui.labelFitted(field.x + 52.0f, ty, "pesquisar comando",
                           theme::kTheme.text2, field.w - 68.0f);
        }
    }
    // o toque no campo abre o teclado IN-APP (o main trata do purpose 9 —
    // devolvemos 4 para o main saber que foi o campo das Docs)
    if (ui.widgetHit(kSearchId, field.x, field.y, field.w, field.h)) {
        result = 4;
    }

    // ---- lista com scroll ---------------------------------------------------
    const UiRect listRegion{ins.left, field.y + field.h + 8.0f, contentW,
                            h - ins.bottom - (field.y + field.h + 8.0f)};
    const std::string query(st.query, st.queryLen);
    auto entries = voni::docs::search(query);

    const f32 lh = lineHeight(ui);
    // 0.9.6 (G2-5): a largura do TEXTO das entradas (a etiqueta da
    // categoria vive à direita NA LINHA DO NOME — a descrição usa a
    // largura toda)
    const f32 textW = contentW - 32.0f;
    const f32 nameW = textW - 104.0f;
    f32 contentH = 0.0f;
    for (size_t i = 0; i < entries.size(); ++i) {
        contentH += entryHeight(ui, *entries[i], (int)i == st.expanded,
                                textW);
    }
    ui.beginScroll(kScrollId, listRegion, contentH);
    const f32 off = ui.scrollOffset();

    // desenha uma string COM WRAP (a descrição inteira, sem "…")
    auto drawWrapped = [&](const char* text, f32 x, f32 y, f32 maxW,
                           const f32 col[4], f32 scale) {
        std::vector<textwrap::Line> lines;
        textwrap::wrap(text, maxW,
                       [&](const char* s) { return ui.fontWidth(s); }, lines);
        if (lines.empty() && text && text[0]) {
            ui.labelStyled(x, y, text, col, scale, 0);
            return;
        }
        char piece[512];
        for (const textwrap::Line& ln : lines) {
            const u32 cp = ln.len < sizeof(piece) - 1
                               ? ln.len
                               : (u32)sizeof(piece) - 1;
            std::snprintf(piece, sizeof(piece), "%.*s", (int)cp,
                          text + ln.begin);
            ui.labelStyled(x, y, piece, col, scale, 0);
            y += lh;
        }
    };

    f32 y = 0.0f;
    for (size_t i = 0; i < entries.size(); ++i) {
        const voni::docs::Entry& e = *entries[i];
        const bool expanded = (int)i == st.expanded;
        const f32 eh = entryHeight(ui, e, expanded, textW);

        // 0.9.6 (G2-5) — COORDENADAS DE ECRÃ (o padrão do SettingsPage):
        // o scroll do UiContext RECORTA mas NÃO TRANSLADA — o conteúdo
        // desenha-se em listRegion.y + y - off (o código antigo desenhava
        // em y CRU: as primeiras entradas ficavam RECORTADAS pelo cabeçalho
        // e o resto saltava para baixo — a lista nunca esteve no sítio).
        // CULLING: entrada totalmente fora da janela visível não desenha.
        const f32 sy = listRegion.y + y - off;
        if (sy + eh < listRegion.y - 4.0f ||
            sy > listRegion.y + listRegion.h + 4.0f) {
            y += eh;
            continue;
        }

        // NOME (14sp, 1 linha) + ETIQUETA da categoria à direita NA LINHA
        // DO NOME (a spec G2-5: “Linguagem”/“Comando” à direita na linha
        // do nome)
        ui.labelStyled(ins.left + 16.0f, sy + 6.0f, e.name,
                       theme::kTheme.text1,
                       theme::fontScale(theme::kFontBody), 0);
        const f32 catX = ins.left + contentW - 16.0f - 104.0f;
        ui.labelStyled(catX, sy + 8.0f, voni::docs::catName(e.cat),
                       theme::kTheme.text2,
                       theme::fontScale(theme::kFontCaption), 0);
        (void)nameW;
        // DESCRIÇÃO POR BAIXO — QUEBRA DE LINHA, texto inteiro (G2-5); as
        // secções expandidas vêm SEQUENCIAIS ao texto real (nunca fixas)
        {
            const f32 b0 = sy + lh - 8.0f;
            const u32 dl = wrappedLines(ui, e.desc, textW);
            drawWrapped(e.desc, ins.left + 16.0f, b0, textW,
                        theme::kTheme.text2,
                        theme::fontScale(theme::kFontCaption));
            if (expanded) {
                f32 sy = b0 + static_cast<f32>(dl) * lh;
                const u32 sl = wrappedLines(ui, e.syntax, textW);
                drawWrapped(e.syntax, ins.left + 16.0f, sy, textW,
                            theme::kTheme.accent,
                            theme::fontScale(theme::kFontCaption));
                sy += static_cast<f32>(sl) * lh;
                const u32 xl = wrappedLines(ui, e.example, textW);
                drawWrapped(e.example, ins.left + 16.0f, sy, textW,
                            theme::kTheme.voniUser,
                            theme::fontScale(theme::kFontCaption));
                sy += static_cast<f32>(xl) * lh;
                // 0.9.5 · A EQUIVALÊNCIA Python/JS (do registo — a tabela
                // que o editor que ensina usa nos erros e na strip)
                if (const voni::reg::Entry* re = voni::reg::find(e.name)) {
                    if (re->equiv && *re->equiv) {
                        char eq[192];
                        std::snprintf(eq, sizeof(eq), "Python/JS: %s",
                                      re->equiv);
                        drawWrapped(eq, ins.left + 16.0f, sy, textW,
                                    theme::kTheme.text2,
                                    theme::fontScale(theme::kFontCaption));
                    }
                }
            }
        }
        // separador fino (no FIM da entrada — o espaço de 8dp fica LIMPO)
        ui.panel(ins.left + 16.0f, sy + eh - 8.0f - 1.0f, textW, 1.0f,
                 theme::kTheme.border);

        // tap na entrada = expandir/colapsar (a linha INTEIRA é alvo)
        if (ui.widgetHit(kEntryBase + (u64)i, ins.left,
                         listRegion.y + y - off, contentW, eh)) {
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
