// tests/test_012a_contadores.cpp — 0.12-A (A-2): os contadores coerentes
// (Ficheiros/Assets) + o marcador da cena ativa — OS PINS DA UI.
//
// O diagnóstico (§4 do RELATORIO-0.12-A): o counter de cenas LÊ o manifesto
// (g_project.scenes via refreshFilesTree, main.cpp) e o Assets lê o
// diretório assets/ (refreshCatalog) — mas o preenchimento só ocorria ao
// ABRIR um seletor/import (refreshCatalog não corria no arranque nem na
// criação de cena): o drawer Ficheiros nascia «0 cena(s)» e o Assets
// «sem ficheiros» com modelo importado. O ícone das linhas de cena era a
// CLAQUETE (Icon::Clapper — o ícone da ANIMAÇÃO) que a 24 dp lia-se como
// glifo partido. Os pins aferram o contrato VISÍVEL:
//   1. ficheiros_conta_cenas — com 1 cena, o painel Ficheiros DIZ «1»
//      (a identidade do glifo pelo UV do atlas: o «1» desenhado é o
//      «1» da fonte — um contador hardcoded nunca passa);
//   2. assets_lista_mesh — com 1 .gmesh no catálogo, o painel Assets
//      MOSTRA-O (a sequência de glifos «casa.gmesh» no batch; sem
//      catálogo o empty-state honesto «sem ficheiros» aparece);
//   3. cena_ativa_marcador — o marcador das linhas de cena é um ÍCONE
//      VÁLIDO (sceneRowIcon() = Icon::Scene, a def tem geometria; o visto
//      da ativa também).
#include "TestFramework.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "platform/InputState.h"
#include "ui/BottomPanel.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/Icons.h"
#include "ui/SafeArea.h"
#include "ui/Theme.h"
#include "ui/Toolbar.h"
#include "ui/UiContext.h"

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

// a identidade de UM glifo: o rect de UV no atlas (o mesmo glifo desenhado
// em sítios diferentes partilha o UV — o fingerprint é estável)
struct GlyphFp {
    f32 u0, v0, u1, v1;
    bool eq(const GlyphFp& o) const {
        return std::fabs(u0 - o.u0) < 1e-3f && std::fabs(v0 - o.v0) < 1e-3f &&
               std::fabs(u1 - o.u1) < 1e-3f && std::fabs(v1 - o.v1) < 1e-3f;
    }
    bool sameAs(const GlyphFp& o) const {
        return eq(o) || (std::fabs(u0 - o.u1) < 1e-3f &&
                         std::fabs(u1 - o.u0) < 1e-3f &&
                         std::fabs(v0 - o.v1) < 1e-3f &&
                         std::fabs(v1 - o.v0) < 1e-3f);
    }
};

struct GlyphQ {   // um quad de glifo do batch
    f32 x0, y0, x1, y1;
    GlyphFp fp;
};

void collectGlyphs(const QuadBatch& b, std::vector<GlyphQ>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        GlyphQ g;
        g.x0 = v[i].x;
        g.y0 = v[i].y;
        g.x1 = v[i + 2].x;
        g.y1 = v[i + 2].y;
        g.fp = {v[i].u, v[i].v, v[i + 2].u, v[i + 2].v};
        out.push_back(g);
    }
}

// o UV de UM glifo desenhado sozinho (a referência do fingerprint)
bool glyphFpOf(UiContext& ui, InputState& input, char c, GlyphFp& out) {
    char s[2] = {c, 0};
    ui.beginFrame(nullptr, &input, kSW, kSH);
    ui.label(40.0f, 300.0f, s, theme::TEXT);
    ui.endFrame();
    input.clearEdges();
    std::vector<GlyphQ> gs;
    collectGlyphs(ui.glyphsForTest(), gs);
    if (gs.empty()) {
        return false;   // sem fonte no hospedeiro — o pin é silencioso
    }
    out = gs[0].fp;
    return true;
}

// a sequência de fingerprints da STRING (1 glifo por caractere — a fonte
// do atlas não faz ligaduras; o espaço devolve quad vazio em alguns hosts,
// por isso saltamos quads degenerados)
bool stringFps(UiContext& ui, InputState& input, const char* s,
               std::vector<GlyphFp>& out) {
    ui.beginFrame(nullptr, &input, kSW, kSH);
    ui.label(40.0f, 300.0f, s, theme::TEXT);
    ui.endFrame();
    input.clearEdges();
    std::vector<GlyphQ> gs;
    collectGlyphs(ui.glyphsForTest(), gs);
    std::sort(gs.begin(), gs.end(),
              [](const GlyphQ& a, const GlyphQ& b) { return a.x0 < b.x0; });
    for (const GlyphQ& g : gs) {
        if (g.x1 - g.x0 <= 0.01f) {
            continue;   // o espaço (sem tinta) não entra no fingerprint
        }
        out.push_back(g.fp);
    }
    return !out.empty();
}

// a sequência `ref` aparece nos glifos `hay` (identidade por UV, na ordem
// de EMISSÃO — dentro de um label os glifos saem à esquerda→direita; a
// MESMA linha = baseline comum, os descendentes ('g', '.') variam o y0 e
// não podem partir a sequência)
bool containsSequence(const std::vector<GlyphQ>& hay,
                      const std::vector<GlyphFp>& ref) {
    if (ref.empty() || hay.size() < ref.size()) {
        return false;
    }
    for (size_t i = 0; i + ref.size() <= hay.size(); ++i) {
        bool ok = hay[i].fp.sameAs(ref[0]);
        for (size_t j = 1; j < ref.size() && ok; ++j) {
            const GlyphQ& a = hay[i + j - 1];
            const GlyphQ& b = hay[i + j];
            ok = b.fp.sameAs(ref[j]) &&            // o MESMO glifo
                 b.x0 >= a.x0 - 0.5f &&            // avança para a direita
                 std::fabs(b.y0 - hay[i].y0) < 8.0f;   // a MESMA baseline
        }
        if (ok) {
            return true;
        }
    }
    return false;
}

// ambiente mínimo: só o drawer (bottom::draw) — o batch fica LIMPO para o
// fingerprint (o padrão do test_wiring090 sem os painéis de cima)
struct Env {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    EditorState st;
    bottom::BottomState bs;
    AssetCatalog catalog;
    bottom::FilesTree tree;
    std::vector<std::string> logLines;
    bool ok = false;

    Env() {
        const char* fp = FONT_FIXTURE;
        ok = font.loadFromPaths(&fp, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        st.drawerH = bs.bottomTab > 0 ? bs.drawerH : 0.0f;
        bottom::draw(ui, input, st, bs, catalog, logLines, 60, 4, tree);
        ui.endFrame();
        input.clearEdges();
    }

    // o rect do CONTEÚDO do drawer (o mesmo cálculo do draw)
    UiRect content() const {
        const bottom::Layout L =
            bottom::layout(kSW, kSH, safe::Insets{}, bs);
        // FASE UI F: o conteúdo é a FONTE ÚNICA Layout::content (a pega 24dp
        // E a fila de tabs 40dp já descontadas lá)
        return L.content;
    }

    // os glifos DENTRO de uma banda do conteúdo, NA ORDEM DE EMISSÃO
    // (a ordem do batch é a ordem do draw; NÃO ordenar por y — os
    // descendentes ('g', '.') partem a banda da linha)
    std::vector<GlyphQ> glyphsIn(f32 y0, f32 y1, f32 x0, f32 x1) {
        std::vector<GlyphQ> gs;
        collectGlyphs(ui.glyphsForTest(), gs);
        std::vector<GlyphQ> out;
        for (const GlyphQ& g : gs) {
            const f32 cy = (g.y0 + g.y1) * 0.5f;
            const f32 cx = (g.x0 + g.x1) * 0.5f;
            if (cy >= y0 && cy <= y1 && cx >= x0 && cx <= x1) {
                out.push_back(g);
            }
        }
        return out;
    }
};

} // namespace

// ---- PIN 1 — com 1 cena, o painel Ficheiros diz «1» -------------------------
TEST(ficheiros_conta_cenas) {
    Env e;
    ASSERT(e.ok);
    GlyphFp zero, um;
    ASSERT(glyphFpOf(e.ui, e.input, '0', zero));   // a referência «0»
    ASSERT(glyphFpOf(e.ui, e.input, '1', um));     // a referência «1»
    EXPECT(!zero.sameAs(um));   // dígitos distintos no atlas (o pressuposto)

    // --- com 1 cena: o 1.º glifo do contador é o «1» ---
    e.bs.bottomTab = 1;          // a tab Ficheiros
    e.bs.drawerH = 240.0f;
    e.tree.n = 0;                // sem pastas (o foco é o contador)
    e.tree.sceneCount = 1;
    e.frame();
    const UiRect c = e.content();
    const f32 rowH = theme::dp(48.0f);
    std::vector<GlyphQ> band = e.glyphsIn(c.y, c.y + rowH,
                                          c.x + c.w * 0.5f, c.x + c.w);
    ASSERT(!band.empty());       // o contador está desenhado à direita
    EXPECT(band[0].fp.sameAs(um));   // «1 cena(s)» — o dígito É um 1

    // --- com 0 cenas: o mesmo contador diz «0» (o formato é honesto nos
    // dois lados — um hardcoded «1» também morria aqui) ---
    e.tree.sceneCount = 0;
    e.frame();
    band = e.glyphsIn(c.y, c.y + rowH, c.x + c.w * 0.5f, c.x + c.w);
    ASSERT(!band.empty());
    EXPECT(band[0].fp.sameAs(zero));   // «0 cena(s)»
}

// ---- PIN 2 — com 1 .gmesh, o painel Assets mostra-o --------------------------
TEST(assets_lista_mesh) {
    Env e;
    ASSERT(e.ok);
    std::vector<GlyphFp> casa;
    ASSERT(stringFps(e.ui, e.input, "casa.gmesh", casa));
    std::vector<GlyphFp> semFich;
    ASSERT(stringFps(e.ui, e.input, "sem ficheiros", semFich));

    e.bs.bottomTab = 2;          // a tab Assets
    e.bs.assetsList = true;      // a vista em LISTA (o nome inteiro)
    e.bs.filesFolder = -1;       // sem filtro de pasta
    e.bs.drawerH = 240.0f;

    // --- SEM catálogo: o empty-state honesto aparece ---
    e.frame();
    {
        const UiRect c = e.content();
        const std::vector<GlyphQ> all = e.glyphsIn(c.y, c.y + c.h,
                                                   c.x, c.x + c.w);
        EXPECT(containsSequence(all, semFich));   // «sem ficheiros…»
    }
    // --- COM 1 .gmesh no catálogo: o painel MOSTRA-O (o nome no batch) ---
    e.catalog.meshes.push_back("assets/casa.gmesh");
    e.frame();
    {
        const UiRect c = e.content();
        const std::vector<GlyphQ> all = e.glyphsIn(c.y, c.y + c.h,
                                                   c.x, c.x + c.w);
        EXPECT(containsSequence(all, casa));   // «casa.gmesh» listado
        EXPECT(!containsSequence(all, semFich));   // e o vazio SUMIU
    }
}

// ---- PIN 3 — o marcador das linhas de cena é um ícone válido -----------------
TEST(cena_ativa_marcador) {
    // a FONTE ÚNICA: o seletor de cenas usa Icon::Scene (camadas) — a
    // claquete era o ícone da ANIMAÇÃO e lia-se como glifo partido
    EXPECT(editor::sceneRowIcon() == icons::Icon::Scene);

    // o ícone É VÁLIDO: geometria completa no conjunto (pts + polilinhas)
    const icons::IconDef& d = icons::def(icons::Icon::Scene);
    EXPECT(d.pts != nullptr);
    EXPECT(d.nPts >= 8u);
    EXPECT(d.nLines >= 4u);
    EXPECT(icons::segmentCount(icons::Icon::Scene) >= 4u);

    // e o VISTO da ativa também (o marcador «cena ativa» da spec H)
    const icons::IconDef& k = icons::def(icons::Icon::Check);
    EXPECT(k.pts != nullptr);
    EXPECT(k.nLines >= 1u);

    // o sheet REAL desenha o nome da cena (o marcador acompanha a linha):
    // «main» no batch com o sheet aberto e a cena 0 ativa
    {
        Env e;
        ASSERT(e.ok);
        std::vector<GlyphFp> main_;
        ASSERT(stringFps(e.ui, e.input, "main", main_));
        e.st.scenesMenu = true;
        const std::vector<std::string> scenes{"scenes/main.goni"};
        e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
        (void)drawScenesMenu(e.ui, e.input, kSW, kSH, e.st, scenes, 0u);
        e.ui.endFrame();
        e.input.clearEdges();
        std::vector<GlyphQ> all;
        collectGlyphs(e.ui.glyphsForTest(), all);
        EXPECT(containsSequence(all, main_));   // a linha «main» desenhada
    }
}
