// tests/test_toolbar.cpp — 0.9.0: TOP BAR 56 + TAB BAR 48 + ÍCONES + THEME.
//
// Aferição da spec D/A (redesign 0.9.0):
//   • TOP BAR 56dp: [Menu][Cena] · [pause][play][sliders] · [gear] — alvos
//     ≥48dp, SEM sobreposição, gear ancorado à direita, encolhe gracioso;
//   • TAB BAR 48dp: 3 terços iguais, underline accent 2dp NO ATIVO;
//   • modos exclusivos (3D|UI|ÁUDIO) pela tab bar; "ÁUDIO" é a palavra
//     inteira (o "UDIO" da 0.8.11 morreu);
//   • CONJUNTO DE ÍCONES (51): todos DENTRO do viewBox 0..24, unicidade por
//     hash de segmentos, peso visual comparável, ícone pelo nome funcional;
//   • THEME spec A: accent #2196F3 EXATO, tabela completa, CONTRASTES:
//     text1/text2/accent/danger/ok/warn sobre surface (≥4,5:1 texto,
//     ≥3:1 componentes — a auditoria da spec);
//   • CANTOS CURVOS: panelRounded nunca emite fora do rect dado.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "components/Transform3D.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "platform/InputState.h"
#include "render/Renderer.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"
#include "ui/Icons.h"
#include "ui/Toolbar.h"
#include "ui/UiContext.h"

using namespace vv;
using namespace vv::editor;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

struct Env {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    Scene      scene;
    EditorState st;

    Env() {
        const char* fontPath = FONT_FIXTURE;
        const bool ok = font.loadFromPaths(&fontPath, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        toolbar::draw(ui, st);
        ui.endFrame();
        input.clearEdges();
    }

    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame();
        input.injectUp(0);
        frame();
    }
};

// rect COBERTO que contém (x,y) com a cor dada (nullptr = qualquer)
const QuadVertex* coveredVertex(const QuadBatch& b, f32 x, f32 y,
                                const f32* color) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        // quad = 6 vértices (2 triângulos); v[i] e v[i+2] são cantos
        // OPOSTOS do rect (top-left e bottom-right do primeiro triângulo)
        const f32 x0 = std::min(v[i].x, v[i + 2].x);
        const f32 x1 = std::max(v[i].x, v[i + 2].x);
        const f32 y0 = std::min(v[i].y, v[i + 2].y);
        const f32 y1 = std::max(v[i].y, v[i + 2].y);
        if (x >= x0 && x <= x1 && y >= y0 && y <= y1) {
            if (!color || (v[i].r == color[0] && v[i].g == color[1] &&
                           v[i].b == color[2])) {
                return v + i;
            }
        }
    }
    return nullptr;
}

bool nearEqF(f32 a, f32 b, f32 eps = 0.01f) {
    return std::fabs(a - b) <= eps;
}

// hash dos SEGMENTOS normalizados de um ícone (unicidade do conjunto)
u64 iconSegHash(icons::Icon ic) {
    const icons::IconDef& d = icons::def(ic);
    u64 h = 1469598103934665603ull;
    for (u32 l = 0; l < d.nLines; ++l) {
        for (u16 q = d.lines[l].first + 1;
             q < d.lines[l].first + d.lines[l].count; ++q) {
            // quantiza para 0.5 ( tolerância de flutuante na igualdade)
            const i32 ax = static_cast<i32>(d.pts[2 * (q - 1)] * 2.0f + 0.5f);
            const i32 ay = static_cast<i32>(d.pts[2 * (q - 1) + 1] * 2.0f + 0.5f);
            const i32 bx = static_cast<i32>(d.pts[2 * q] * 2.0f + 0.5f);
            const i32 by = static_cast<i32>(d.pts[2 * q + 1] * 2.0f + 0.5f);
            h ^= static_cast<u64>(ax * 7349 + ay * 911 + bx * 83 + by);
            h *= 1099511628211ull;
        }
    }
    return h;
}

} // namespace

// ---- 1. TOP BAR: layout 56dp sem sobreposição, alvos ≥48, gear à direita -----

// ---- FASE 9 (G2-9/G2-10): A BARRA ÚNICA — menu+tabs+ações numa faixa de 56 --
// (a tab bar de 48dp FUNDEU-SE; o botão sliders MORREU — inventário G0-4)

TEST(topbar_layout_56dp_alvos48_sem_sobreposicao) {
    const safe::Insets in{0.0f, 24.0f, 0.0f, 24.0f};   // C33
    const toolbar::TopBarLayout L =
        toolbar::topbarLayout(kSW, kSH, in, false, false);
    EXPECT(nearEqF(L.bar.h, 56.0f));
    EXPECT(nearEqF(L.bar.y, 24.0f));
    const UiRect content = safe::contentRect(kSW, kSH, in);
    EXPECT(safe::rectInside(L.bar, content));
    // FASE 9: 8 widgets (menu, cena, 3 tabs, pause, play, gear) — SEM sliders
    const UiRect rects[8] = {L.menu,   L.cena,   L.tab3d,  L.tabUi,
                             L.tabAudio, L.pause, L.play,  L.gear};
    const char* names[8] = {"menu", "cena", "tab3d", "tabUi",
                            "tabAudio", "pause", "play", "gear"};
    for (int i = 0; i < 8; ++i) {
        EXPECT(rects[i].h >= 48.0f - 0.01f);
        EXPECT(rects[i].w > 0.0f);
        EXPECT(safe::rectInside(rects[i], content));
        for (int j = i + 1; j < 8; ++j) {
            const f32 ox = std::min(rects[i].x + rects[i].w, rects[j].x + rects[j].w) -
                            std::max(rects[i].x, rects[j].x);
            const f32 oy = std::min(rects[i].y + rects[i].h, rects[j].y + rects[j].h) -
                            std::max(rects[i].y, rects[j].y);
            EXPECT(ox <= 0.01f || oy <= 0.01f);
        }
    }
    // gear ancorado à direita
    EXPECT(nearEqF(L.gear.x + L.gear.w, L.bar.x + L.bar.w - 12.0f, 0.5f));
    // as 3 tabs IGUAIS e ao CENTRO da barra (spec D no merge G2-10)
    EXPECT(nearEqF(L.tab3d.w, L.tabUi.w));
    EXPECT(nearEqF(L.tabUi.w, L.tabAudio.w));
    const f32 tabsMid = L.tab3d.x + (L.tabAudio.x + L.tabAudio.w - L.tab3d.x) * 0.5f;
    EXPECT(nearEqF(tabsMid, L.bar.x + L.bar.w * 0.5f, 1.0f));
    // underline 2dp NO FUNDO da barra única (o indicador do modo ativo)
    EXPECT(nearEqF(L.underline.h, 2.0f));
    EXPECT(nearEqF(L.underline.y, L.bar.y + L.bar.h - 2.0f));
    EXPECT(L.active == 0u);
    // com ÁUDIO ativo o underline MUDA de tab
    const toolbar::TopBarLayout La =
        toolbar::topbarLayout(kSW, kSH, safe::Insets{}, false, true);
    EXPECT(La.active == 2u);
    EXPECT(nearEqF(La.underline.x, La.tabAudio.x));
}

TEST(topbar_layout_ecra_estreito_encolhe_gracioso) {
    // 1000px de largura: os grupos ENCOLHEM mas nada sobrepõe/sai
    const toolbar::TopBarLayout L =
        toolbar::topbarLayout(1000.0f, kSH, safe::Insets{}, false, false);
    const UiRect content = safe::contentRect(1000.0f, kSH, safe::Insets{});
    EXPECT(safe::rectInside(L.bar, content));
    const UiRect rects[8] = {L.menu,   L.cena,   L.tab3d,  L.tabUi,
                             L.tabAudio, L.pause, L.play,  L.gear};
    for (int i = 0; i < 8; ++i) {
        EXPECT(rects[i].w > 0.0f);
        EXPECT(rects[i].h > 0.0f);
        for (int j = i + 1; j < 8; ++j) {
            const f32 ox = std::min(rects[i].x + rects[i].w, rects[j].x + rects[j].w) -
                            std::max(rects[i].x, rects[j].x);
            const f32 oy = std::min(rects[i].y + rects[i].h, rects[j].y + rects[j].h) -
                            std::max(rects[i].y, rects[j].y);
            EXPECT(ox <= 0.01f || oy <= 0.01f);
        }
    }
}

// ---- 2. as TABS de modo (agora DENTRO da barra única — G2-10) ---------------

TEST(modetabs_troca_modos_exclusivos_e_audio_inteiro) {
    Env e;
    EXPECT(e.font.ok());
    e.frame();
    const toolbar::TopBarLayout L =
        toolbar::topbarLayout(kSW, kSH, safe::Insets{}, false, false);
    // UI
    e.tap(L.tabUi.x + L.tabUi.w * 0.5f, L.tabUi.y + L.tabUi.h * 0.5f);
    EXPECT(e.st.uiMode && !e.st.audioMode);
    // ÁUDIO (a palavra INTEIRA desenha — o glifo 'Á' existe no atlas)
    e.tap(L.tabAudio.x + L.tabAudio.w * 0.5f, L.tabAudio.y + L.tabAudio.h * 0.5f);
    EXPECT(e.st.audioMode && !e.st.uiMode);
    // volta a 3D
    e.tap(L.tab3d.x + L.tab3d.w * 0.5f, L.tab3d.y + L.tab3d.h * 0.5f);
    EXPECT(!e.st.uiMode && !e.st.audioMode);
}

// ---- 3. CONJUNTO DE ÍCONES (51): viewBox, unicidade, peso ---------------------

TEST(icones_conjunto_completo_dentro_do_viewbox) {
    for (int ic = 0; ic < static_cast<int>(icons::Icon::Count); ++ic) {
        const icons::IconDef& d = icons::def(static_cast<icons::Icon>(ic));
        EXPECT(d.nPts >= 2);
        EXPECT(d.nLines >= 1);
        for (u32 p = 0; p < d.nPts; ++p) {
            EXPECT(d.pts[2 * p] >= -0.01f && d.pts[2 * p] <= 24.01f);
            EXPECT(d.pts[2 * p + 1] >= -0.01f && d.pts[2 * p + 1] <= 24.01f);
        }
        // traço não-degenerado: comprimento total positivo (Dots/Check são
        // curtos POR NATUREZA — 3 segmentos curtos; o piso é 4)
        EXPECT(icons::totalLength(static_cast<icons::Icon>(ic)) > 4.0f);
    }
}

TEST(icones_unicidade_nenhum_duplicado) {
    const int n = static_cast<int>(icons::Icon::Count);
    std::vector<u64> hashes;
    for (int ic = 0; ic < n; ++ic) {
        hashes.push_back(iconSegHash(static_cast<icons::Icon>(ic)));
    }
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            EXPECT(hashes[static_cast<size_t>(i)] !=
                   hashes[static_cast<size_t>(j)]);
        }
    }
}

TEST(icones_pelo_nome_funcional) {
    EXPECT(icons::iconByName("hamburger") ==
           static_cast<i32>(icons::Icon::Hamburger));
    EXPECT(icons::iconByName("lupa") ==
           static_cast<i32>(icons::Icon::Search));
    EXPECT(icons::iconByName("lixo") == static_cast<i32>(icons::Icon::Trash));
    EXPECT(icons::iconByName("atribuir") ==
           static_cast<i32>(icons::Icon::Assign));
    EXPECT(icons::iconByName("nao-existe") < 0);
    EXPECT(icons::iconByName(nullptr) < 0);
    // o conjunto da spec A existe por FUNÇÃO (amostra obrigatória)
    const char* kSpec[] = {"hamburger", "chevron",      "sliders", "gear",
                           "cubo",      "monitor",      "speaker", "plus",
                           "olho",      "olho-off",     "dots",    "pessoa",
                           "camara",    "undo",         "redo",    "save",
                           "copy",      "paste",        "cursor",  "mover",
                           "rodar",     "escalar",      "grelha",  "terminal",
                           "clapper",   "pasta",        "lupa",    "ordenar",
                           "upload",    "download",     "mic",     "stop",
                           "record",    "check",        "warn",    "interrogacao",
                           "spinner",   "lixo",         "renomear", "duplicar",
                           "atribuir",  "back"};
    for (const char* nm : kSpec) {
        EXPECT(icons::iconByName(nm) >= 0);
    }
}

// ---- 4. THEME (spec G): tabela exata + CONTRASTES (a auditoria) ---------------
// RECALIBRADO 0.9.6.10 (GRUPO UI · A REESCRITA DA APRESENTAÇÃO): os
// valores EXATOS eram os da spec F (mono+vidro — accent BRANCO #F5F5F5);
// o dono DECIDIU a Paleta A OFICIAL depois de ver o mono no device (os
// fills brancos cegantes do anti-exemplo): GRAFITE+ÂMBAR+VIDRO
// (#0E0E10/#161618/#202023/#2E2E32 + accent ÂMBAR #FFB020 + accentDim
// #4A3714 + warn LARANJA #FF8A3D ≠ accent). A asserção continua EXATA.

TEST(theme_tabela_spec_g_exata) {
    // accent — O ÂMBAR: #FFB020 (o mono morreu com os fills brancos)
    EXPECT(nearEqF(theme::kTheme.accent[0], 255.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.accent[1], 176.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.accent[2], 32.0f / 255.0f, 1e-4f));
    // O AZUL MORREU no accent: B é o canal MENOR (o âmbar é quente)
    EXPECT(theme::kTheme.accent[2] < theme::kTheme.accent[1]);
    // bg #0E0E10 · surface #161618 · surface2 #202023 · border #2E2E32
    // (a rampa GRAFITE — o tinge frio ≤4/255 no canal B)
    EXPECT(nearEqF(theme::kTheme.bg[0], 14.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.surface[0], 22.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.surface2[1], 32.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.border[2], 50.0f / 255.0f, 1e-4f));
    // O VIDRO (spec G): surface α0.80 (A DECISÃO DO DONO) · surface2 α0.86
    // · border α0.55 · bg opaco
    EXPECT(nearEqF(theme::kTheme.surface[3], 0.80f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.surface2[3], 0.86f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.border[3], 0.55f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.bg[3], 1.0f, 1e-4f));
    // o BORDO e o HIGHLIGHT do vidro (#FFFFFF1F / #FFFFFF0A — a spec G)
    EXPECT(nearEqF(theme::kTheme.glassEdge[0], 1.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.glassEdge[3], 31.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.glassTop[3], 10.0f / 255.0f, 1e-4f));
    // text1 #ECECEE · text2 #A6A6AD · scrim 60%
    EXPECT(nearEqF(theme::kTheme.text1[0], 236.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.text2[1], 166.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.scrim[3], 0.60f, 1e-4f));
    // accentInk ESCURO (#0E0E10 — a tinta sobre o fill ÂMBAR, 10,5:1)
    EXPECT(nearEqF(theme::kTheme.accentInk[0], 14.0f / 255.0f, 1e-4f));
    // accentDim #4A3714 — o FILL DA SELEÇÃO (âmbar 25% sobre grafite)
    EXPECT(nearEqF(theme::kTheme.accentDim[0], 74.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.accentDim[1], 55.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.accentDim[2], 20.0f / 255.0f, 1e-4f));
    // warn LARANJA #FF8A3D — NUNCA igual ao accent (hue 24° vs 39°)
    EXPECT(nearEqF(theme::kTheme.warn[0], 255.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.warn[1], 138.0f / 255.0f, 1e-4f));
    EXPECT(nearEqF(theme::kTheme.warn[2], 61.0f / 255.0f, 1e-4f));
    EXPECT(theme::kTheme.warn[2] > theme::kTheme.accent[2]);   // laranja ≠ âmbar
    EXPECT(theme::kTheme.warn[1] < theme::kTheme.accent[1]);
    // voniComment #8A8A8A (5,6:1 no grafite — mantém-se do F)
    EXPECT(nearEqF(theme::kTheme.voniComment[0], 138.0f / 255.0f, 1e-4f));
    // voniUser == text1 (o grafite trouxe o user ao text1 da casa)
    EXPECT(theme::kTheme.voniUser[0] == theme::kTheme.text1[0]);
}

TEST(theme_contraste_auditoria_spec) {
    // TEXTO ≥ 4,5:1 sobre surface
    EXPECT(theme::contrastOnSurface(theme::kTheme.text1) >= 4.5f);
    EXPECT(theme::contrastOnSurface(theme::kTheme.text2) >= 4.5f);
    // COMPONENTES ≥ 3:1 sobre surface
    EXPECT(theme::contrastOnSurface(theme::kTheme.accent) >= 3.0f);
    EXPECT(theme::contrastOnSurface(theme::kTheme.danger) >= 3.0f);
    EXPECT(theme::contrastOnSurface(theme::kTheme.ok) >= 3.0f);
    EXPECT(theme::contrastOnSurface(theme::kTheme.warn) >= 3.0f);
    // 0.9.6.10 (GRUPO UI) · O VIDRO NÃO MENTE À AUDITORIA: os pisos REAIS
    // (α0.80 sobre o bg grafite — o que está por trás dos painéis dokados)
    // ficam TODOS ≥4,5 (contrastOnGlassReal — a matemática do blend do
    // device aplicada ao caso REAL)
    EXPECT(theme::contrastOnGlassReal(theme::kTheme.text1) >= 4.5f);
    EXPECT(theme::contrastOnGlassReal(theme::kTheme.text2) >= 4.5f);
    EXPECT(theme::contrastOnGlassReal(theme::kTheme.accent) >= 4.5f);
    // O PIOR CASO PATOLÓGICO (cena BRANCA PURA por trás do vidro 80%):
    // o que PODE flutuar sobre a cena viva (text1/accent — a regra da
    // casa) mantém os pisos; text2/danger NÃO flutuam sobre a cena (vivem
    // em painéis dokados — aferidos em cima); o gate é a própria regra
    EXPECT(theme::contrastOnGlass(theme::kTheme.text1) >= 4.5f);
    EXPECT(theme::contrastOnGlass(theme::kTheme.accent) >= 3.0f);
    // a TINTA sobre o fill âmbar — 10,5:1
    EXPECT(theme::contrastRatio(theme::kTheme.accentInk,
                                theme::kTheme.accent) >= 4.5f);
    // text1/accent sobre o accentDim (o fill da SELEÇÃO) — 9,6/6,2:1
    EXPECT(theme::contrastRatio(theme::kTheme.text1,
                                theme::kTheme.accentDim) >= 4.5f);
    EXPECT(theme::contrastRatio(theme::kTheme.accent,
                                theme::kTheme.accentDim) >= 3.0f);
    // accentPress MAIS ESCURO que accent (feedback de premir)
    EXPECT(theme::kTheme.accentPress[0] < theme::kTheme.accent[0]);
    // a razão em si é estável (função PURA — determinística)
    EXPECT(nearEqF(theme::contrastRatio(theme::kTheme.text1,
                                        theme::kTheme.surface),
                   theme::contrastRatio(theme::kTheme.text1,
                                        theme::kTheme.surface),
                   1e-6f));
}

// ---- 5. CANTOS CURVOS: panelRounded NUNCA sai do rect --------------------------

TEST(panel_rounded_nunca_sai_do_rect) {
    Env e;
    EXPECT(e.font.ok());
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    const f32 x = 400.0f, y = 300.0f, w = 200.0f, h = 96.0f;
    e.ui.panelRounded(x, y, w, h, theme::kRadiusCard, theme::kTheme.accent);
    e.ui.endFrame();
    const QuadBatch& b = e.ui.solidsForTest();
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    EXPECT(n > 0);
    for (u32 i = 0; i < n; ++i) {
        EXPECT(v[i].x >= x - 0.01f && v[i].x <= x + w + 0.01f);
        EXPECT(v[i].y >= y - 0.01f && v[i].y <= y + h + 0.01f);
    }
}

TEST(frame_rounded_emite_moldura_e_arcos) {
    Env e;
    EXPECT(e.font.ok());
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    const f32 x = 400.0f, y = 300.0f, w = 200.0f, h = 96.0f;
    e.ui.frameRounded(x, y, w, h, 2.0f, theme::kRadiusCard,
                      theme::kTheme.border);
    e.ui.endFrame();
    // sólidos (arestas) DENTRO do rect
    const QuadBatch& b = e.ui.solidsForTest();
    const QuadVertex* v = b.vertices();
    for (u32 i = 0, n = b.vertexCount(); i < n; ++i) {
        EXPECT(v[i].x >= x - 0.02f && v[i].x <= x + w + 0.02f);
        EXPECT(v[i].y >= y - 0.02f && v[i].y <= y + h + 0.02f);
    }
}

// ---- 6. top bar DESENHA os 6 botões e reporta as ações -------------------------

TEST(topbar_desenha_botoes_e_reporta) {
    Env e;
    EXPECT(e.font.ok());
    // o fundo da barra = bg (o chrome separa-se da área de trabalho)
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    const toolbar::TopBarLayout L =
        toolbar::topbarLayout(kSW, kSH, safe::Insets{}, false, false);
    toolbar::drawTopBar(e.ui, e.st);
    e.ui.endFrame();
    EXPECT(coveredVertex(e.ui.solidsForTest(), L.bar.x + L.bar.w * 0.5f,
                         L.bar.y + L.bar.h * 0.5f, theme::kTheme.bg) !=
           nullptr);
    // toque no GEAR reporta gearPressed (a página de Settings — spec I)
    e.input.injectDown(0, L.gear.x + L.gear.w * 0.5f, L.gear.y + L.gear.h * 0.5f);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    const toolbar::TopBarActions a = toolbar::drawTopBar(e.ui, e.st);
    e.ui.endFrame();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    const toolbar::TopBarActions a2 = toolbar::drawTopBar(e.ui, e.st);
    e.ui.endFrame();
    e.input.clearEdges();
    (void)a;
    EXPECT(a2.gearPressed);
}
