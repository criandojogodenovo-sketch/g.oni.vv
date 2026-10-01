// tests/test_toolbar.cpp — 0.7.6: BARRA FINAL DE 5 GRUPOS + ÍCONES + THEME.
//
// Aferição da spec:
//   • toolbar com 5 grupos separados sem sobreposição em landscape;
//   • grupo de transformação some sem seleção e aparece com seleção;
//   • segmented controls mutuamente exclusivos (3D|UI e mover/rodar/escalar;
//     snap é um toggle do grupo — liga o snapping, não é um 4º estado);
//   • ícones desenhados dentro do rect e nítidos a 24/32 px (geometria
//     dentro do viewBox 0..24, traços não-degenerados, espessura uniforme,
//     peso visual comparável);
//   • Theme central: cor de marca #8AB4F8 / fundo #0B0E13 EXATOS e a barra
//     LÊ do struct (o quad do fundo = bg; o ativo = brand);
//   • o dropdown do Menu tem os itens novos (7 itens; 1 = Settings — o
//     botão próprio morreu; "Cenas…" saiu — o [Cena ▾] abre a lista).
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
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
    toolbar::GizmoModeState gz;
    Handle     tic{};
    bool       ok = false;

    Env() {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        tic = scene.create("Cubo");
        if (Tic* t = scene.get(tic)) {
            t->addComponent<Transform3D>();
        }
        st.selected = tic;
    }

    void frame(bool withSelection = true) {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        toolbar::draw(ui, st, gz, withSelection && st.selected.valid());
        ui.endFrame();
        input.clearEdges();
    }

    void tap(f32 x, f32 y, bool withSelection = true) {
        input.injectDown(0, x, y);
        frame(withSelection);
        input.injectUp(0);
        frame(withSelection);
    }

    // o quad COBERTO que contém (x,y) com a cor dada (ou nullptr)
    const QuadVertex* findQuadAt(f32 x, f32 y, const f32 col[4]) const {
        const QuadVertex* v = ui.solidsForTest().vertices();
        const u32 n = ui.solidsForTest().vertexCount() / 6;
        for (u32 q = 0; q < n; ++q) {
            const QuadVertex* a = v + q * 6;
            if (a[0].r != col[0] || a[0].g != col[1] || a[0].b != col[2]) {
                continue;
            }
            f32 x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
            for (int k = 0; k < 6; ++k) {
                if (a[k].x < x0) x0 = a[k].x;
                if (a[k].y < y0) y0 = a[k].y;
                if (a[k].x > x1) x1 = a[k].x;
                if (a[k].y > y1) y1 = a[k].y;
            }
            if (x >= x0 && x <= x1 && y >= y0 && y <= y1) {
                return a;
            }
        }
        return nullptr;
    }
};

// interseção 2D REAL: min dos fins − max das origens, nos 2 eixos
bool rectsOverlap(const UiRect& a, const UiRect& b) {
    const f32 ix = (a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w) -
                   (a.x > b.x ? a.x : b.x);
    const f32 iy = (a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h) -
                   (a.y > b.y ? a.y : b.y);
    return ix > 0.5f && iy > 0.5f;
}

} // namespace

// ---- 1. os 5 grupos separados, sem sobreposição, em landscape ----------------

TEST(toolbar_5_grupos_sem_sobreposicao_landscape) {
    for (const f32 sw : {1600.0f, 1280.0f}) {
        for (const bool g4 : {false, true}) {
            const toolbar::Layout L =
                toolbar::layout(sw, kSH, safe::Insets{}, g4);
            EXPECT(L.g4Visible == g4);
            // todos os widgets DENTRO da faixa da barra
            UiRect groups[11];
            u32 n = 0;
            groups[n++] = L.menu;
            groups[n++] = L.cena;
            groups[n++] = L.pause;
            groups[n++] = L.play;
            groups[n++] = L.mode3d;
            groups[n++] = L.modeUi;
            if (g4) {
                for (int i = 0; i < 4; ++i) {
                    groups[n++] = L.giz[i];
                }
            }
            groups[n++] = L.inspector;
            for (u32 i = 0; i < n; ++i) {
                EXPECT(groups[i].x >= L.bar.x - 0.01f);
                EXPECT(groups[i].x + groups[i].w <=
                       L.bar.x + L.bar.w + 0.01f);
                EXPECT(groups[i].y >= L.bar.y - 0.01f);
                EXPECT(groups[i].y + groups[i].h <=
                       L.bar.y + L.bar.h + 0.01f);
                EXPECT(groups[i].w > 0.0f);
                // pares de widgets NUNCA se sobrepõem (nada sobreposto —
                // o critério transversal da fase)
                for (u32 j = i + 1; j < n; ++j) {
                    EXPECT(!rectsOverlap(groups[i], groups[j]));
                }
            }
            // separadores: um entre CADA par de grupos adjacentes
            EXPECT(L.sepCount == (g4 ? 4u : 3u));
            for (u32 s = 0; s < L.sepCount; ++s) {
                EXPECT(L.sep[s].x > L.bar.x);
                EXPECT(L.sep[s].x < L.bar.x + L.bar.w);
                EXPECT(L.sep[s].w <= 1.0f);   // linha FINA
            }
            // o G5 fica ancorado à direita (não mexe com o G4)
            EXPECT(test::nearEqF(L.inspector.x + L.inspector.w,
                           L.bar.x + L.bar.w - 12.0f));
        }
    }
}

// ---- 2. grupo de transformação CONDICIONAL (some sem seleção) -----------------

TEST(toolbar_grupo_transformacao_condicional) {
    Env e;
    EXPECT(e.ok);

    // SEM seleção: o layout não tem G4 — e um toque onde ele ESTARIA não
    // faz nada (o widget não existe = não é interativo)
    const toolbar::Layout withG4 =
        toolbar::layout(kSW, kSH, safe::Insets{}, true);
    const UiRect rot = withG4.giz[1];
    e.st.selected = Handle::invalid();
    e.tap(rot.x + rot.w * 0.5f, rot.y + rot.h * 0.5f, false);
    EXPECT(e.gz.mode == 0);   // rotate NÃO ativou — o G4 não se desenhava

    // COM seleção (3D): o G4 aparece e o MESMO toque ativa o Rodar
    e.st.selected = e.tic;
    const toolbar::Layout now =
        toolbar::layout(kSW, kSH, safe::Insets{}, true);
    EXPECT(now.g4Visible);
    e.tap(now.giz[1].x + now.giz[1].w * 0.5f, now.giz[1].y + now.giz[1].h * 0.5f);
    EXPECT(e.gz.mode == 1);

    // no modo UI o G4 volta a SUMIR (os gizmos são 3D)
    e.st.uiMode = true;
    e.frame();
    EXPECT(e.st.uiMode);
    e.tap(now.giz[2].x + now.giz[2].w * 0.5f, now.giz[2].y + now.giz[2].h * 0.5f);
    EXPECT(e.gz.mode == 1);   // scale NÃO ativou (G4 ausente no modo UI)
}

// ---- 3. segmented controls mutuamente exclusivos --------------------------------

TEST(toolbar_segmented_controls_exclusivos) {
    Env e;
    EXPECT(e.ok);
    const toolbar::Layout L = toolbar::layout(kSW, kSH, safe::Insets{}, false);

    // 3D | UI: UM dos dois — e sair do UI limpa a seleção de elemento
    e.st.uiMode = false;
    e.st.selElement = 3;
    e.tap(L.modeUi.x + L.modeUi.w * 0.5f, L.modeUi.y + L.modeUi.h * 0.5f);
    EXPECT(e.st.uiMode);
    EXPECT(e.gz.mode == 0);   // o G3 não mexe no modo do gizmo (Env novo)
    e.tap(L.mode3d.x + L.mode3d.w * 0.5f, L.mode3d.y + L.mode3d.h * 0.5f);
    EXPECT(!e.st.uiMode);
    EXPECT(e.st.selElement == -1);   // limpa ao voltar ao 3D (como o antigo)

    // mover/rodar/escalar: UM ativo de cada vez (Env novo — mode 0)
    const toolbar::Layout G = toolbar::layout(kSW, kSH, safe::Insets{}, true);
    EXPECT(e.gz.mode == 0);
    e.tap(G.giz[2].x + G.giz[2].w * 0.5f, G.giz[2].y + G.giz[2].h * 0.5f);
    EXPECT(e.gz.mode == 2);
    e.tap(G.giz[0].x + G.giz[0].w * 0.5f, G.giz[0].y + G.giz[0].h * 0.5f);
    EXPECT(e.gz.mode == 0);
    e.tap(G.giz[1].x + G.giz[1].w * 0.5f, G.giz[1].y + G.giz[1].h * 0.5f);
    EXPECT(e.gz.mode == 1);

    // snap: TOGGLE do grupo — liga/desliga SEM trocar o modo ativo
    EXPECT(!e.gz.snap);
    e.tap(G.giz[3].x + G.giz[3].w * 0.5f, G.giz[3].y + G.giz[3].h * 0.5f);
    EXPECT(e.gz.snap);
    EXPECT(e.gz.mode == 1);   // o snap não é um 4º estado
    e.tap(G.giz[3].x + G.giz[3].w * 0.5f, G.giz[3].y + G.giz[3].h * 0.5f);
    EXPECT(!e.gz.snap);
    EXPECT(e.gz.mode == 1);
}

// ---- 4. ícones: dentro do rect, nítidos a 24/32 px, espessura uniforme ---------

TEST(toolbar_icones_dentro_do_rect_nitidos_24_32) {
    // (a) TODOS os pontos de TODOS os ícones vivem no viewBox 0..24
    for (int i = 0; i < static_cast<int>(icons::Icon::Count); ++i) {
        const icons::Icon ic = static_cast<icons::Icon>(i);
        const icons::IconDef& d = icons::def(ic);
        EXPECT(d.nPts >= 2);
        f32 minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        for (u32 p = 0; p < d.nPts; ++p) {
            EXPECT(d.pts[2 * p] >= 0.0f && d.pts[2 * p] <= 24.0f);
            EXPECT(d.pts[2 * p + 1] >= 0.0f && d.pts[2 * p + 1] <= 24.0f);
            if (d.pts[2 * p] < minX) minX = d.pts[2 * p];
            if (d.pts[2 * p] > maxX) maxX = d.pts[2 * p];
            if (d.pts[2 * p + 1] < minY) minY = d.pts[2 * p + 1];
            if (d.pts[2 * p + 1] > maxY) maxY = d.pts[2 * p + 1];
        }
        // ocupa o canvas (legibilidade: geometria com massa nos 2 eixos)
        EXPECT(maxX - minX >= 9.0f);
        EXPECT(maxY - minY >= 9.0f);
        // nítido: tem traços e NENHUM segmento degenerado
        EXPECT(icons::segmentCount(ic) >= 2);
        const f32 len = icons::totalLength(ic);
        for (u32 l = 0; l < d.nLines; ++l) {
            const icons::Polyline& pl = d.lines[l];
            for (u16 q = pl.first + 1; q < pl.first + pl.count; ++q) {
                const f32 dx = d.pts[2 * q] - d.pts[2 * (q - 1)];
                const f32 dy = d.pts[2 * q + 1] - d.pts[2 * (q - 1) + 1];
                EXPECT(dx * dx + dy * dy > 0.25f);   // > 0.5 u
            }
        }
        // peso visual comparável: contornos ocos (Inspector/Cena) têm mais
        // caminho mas leem mais leves; barras curtas (Pause) leem mais
        // pesados — a banda afere a ORDEM DE GRANDEZA comum ao conjunto
        EXPECT(len >= 24.0f && len <= 85.0f);
    }

    // (b) desenhados a 24 e 32 px: tudo DENTRO do rect (+ meio traço)
    for (const f32 size : {24.0f, 32.0f}) {
        for (int i = 0; i < static_cast<int>(icons::Icon::Count); ++i) {
            Env e;
            if (!e.ok) {
                EXPECT(!"fonte ausente");
                return;
            }
            const f32 x = 200.0f, y = 120.0f;
            e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
            icons::drawIcon(e.ui, static_cast<icons::Icon>(i), x, y, size,
                            theme::kTheme.brand);
            e.ui.endFrame();
            const QuadVertex* v = e.ui.solidsForTest().vertices();
            const u32 nv = e.ui.solidsForTest().vertexCount();
            EXPECT(nv >= 12);   // ≥ 2 segmentos emitidos (tem desenho)
            const f32 bleed = size / 12.0f * 0.5f + 0.05f;   // meio traço
            for (u32 q = 0; q < nv; ++q) {
                EXPECT(v[q].x >= x - bleed && v[q].x <= x + size + bleed);
                EXPECT(v[q].y >= y - bleed && v[q].y <= y + size + bleed);
                // espessura UNIFORME: |v0−v1| de cada segmento = thickness
            }
            // espessura uniforme: em CADA segmento, o par (v0,v1) é o
            // offset ±normal — a distância entre eles é a espessura
            const f32 want = size / 12.0f;
            for (u32 q = 0; q + 5 < nv; q += 6) {
                const f32 dx = v[q].x - v[q + 1].x;
                const f32 dy = v[q].y - v[q + 1].y;
                EXPECT(test::nearEqF(std::sqrt(dx * dx + dy * dy), want, 0.05f));
            }
        }
    }
}

// ---- 5. Theme central: cores EXATAS e a barra LÊ do struct ---------------------

TEST(toolbar_le_o_theme_central) {
    // cor de marca #8AB4F8 e fundo #0B0E13 — EXATOS (a spec manda estes
    // valores; a exceção documentada ao tema mono dos painéis)
    EXPECT(test::nearEqF(theme::kTheme.brand[0], 138.0f / 255.0f, 1e-4f));
    EXPECT(test::nearEqF(theme::kTheme.brand[1], 180.0f / 255.0f, 1e-4f));
    EXPECT(test::nearEqF(theme::kTheme.brand[2], 248.0f / 255.0f, 1e-4f));
    EXPECT(test::nearEqF(theme::kTheme.bg[0], 11.0f / 255.0f, 1e-4f));
    EXPECT(test::nearEqF(theme::kTheme.bg[1], 14.0f / 255.0f, 1e-4f));
    EXPECT(test::nearEqF(theme::kTheme.bg[2], 19.0f / 255.0f, 1e-4f));
    // mono dos painéis INTACTO (o struct espelha os tokens históricos)
    EXPECT(test::nearEqF(theme::kTheme.panel[0], 0.1176471f, 1e-4f));
    EXPECT(test::nearEqF(theme::kTheme.text[0], 0.9019608f, 1e-4f));

    Env e;
    EXPECT(e.ok);
    e.frame();   // modo 3D ativo por omissão
    const toolbar::Layout L = toolbar::layout(kSW, kSH, safe::Insets{}, true);
    // o FUNDO da barra é o bg do Theme
    EXPECT(e.findQuadAt(L.bar.x + 6.0f, L.bar.y + L.bar.h - 6.0f,
                        theme::kTheme.bg) != nullptr);
    // o segmento ATIVO (3D) tem fundo de MARCA
    EXPECT(e.findQuadAt(L.mode3d.x + L.mode3d.w * 0.5f,
                        L.mode3d.y + L.mode3d.h * 0.5f,
                        theme::kTheme.brand) != nullptr);
    // o ícone do play (G2, inativo) é desenhado na COR DE MARCA (linhas)
    bool brandLine = false;
    const QuadVertex* v = e.ui.solidsForTest().vertices();
    const u32 nv = e.ui.solidsForTest().vertexCount();
    for (u32 q = 0; q + 5 < nv; q += 6) {
        const f32 ax = v[q].x, ay = v[q].y;
        const f32 bx = v[q + 4].x, by = v[q + 4].y;
        if (ax >= L.play.x && ax <= L.play.x + L.play.w &&
            ay >= L.play.y && ay <= L.play.y + L.play.h &&
            bx >= L.play.x && bx <= L.play.x + L.play.w &&
            by >= L.play.y && by <= L.play.y + L.play.h &&
            v[q].r == theme::kTheme.brand[0] &&
            v[q].g == theme::kTheme.brand[1] &&
            v[q].b == theme::kTheme.brand[2]) {
            brandLine = true;
            break;
        }
    }
    EXPECT(brandLine);
}

// ---- 6. ações da barra (G1 dropdowns, G2 playback, G5 inspector) ---------------

TEST(toolbar_acoes_g1_g2_g5) {
    Env e;
    EXPECT(e.ok);
    const toolbar::Layout L = toolbar::layout(kSW, kSH, safe::Insets{}, true);

    // G5 inspector: alterna o painel direito
    EXPECT(e.st.showInspector);
    e.tap(L.inspector.x + L.inspector.w * 0.5f,
          L.inspector.y + L.inspector.h * 0.5f);
    EXPECT(!e.st.showInspector);
    e.tap(L.inspector.x + L.inspector.w * 0.5f,
          L.inspector.y + L.inspector.h * 0.5f);
    EXPECT(e.st.showInspector);

    // G1 Menu/Cena: os Actions disparam (o main abre os dropdowns/overlays)
    toolbar::Actions a{};
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    a = toolbar::draw(e.ui, e.st, e.gz, true);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(!a.menuDropdown && !a.cenaDropdown && !a.playPressed);

    e.input.injectDown(0, L.menu.x + L.menu.w * 0.5f,
                       L.menu.y + L.menu.h * 0.5f);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    a = toolbar::draw(e.ui, e.st, e.gz, true);
    e.ui.endFrame();
    e.input.clearEdges();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    a = toolbar::draw(e.ui, e.st, e.gz, true);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(a.menuDropdown);
    EXPECT(!a.cenaDropdown);

    e.input.injectDown(0, L.cena.x + L.cena.w * 0.5f,
                       L.cena.y + L.cena.h * 0.5f);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    a = toolbar::draw(e.ui, e.st, e.gz, true);
    e.ui.endFrame();
    e.input.clearEdges();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    a = toolbar::draw(e.ui, e.st, e.gz, true);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(a.cenaDropdown);
    EXPECT(!a.menuDropdown);

    e.input.injectDown(0, L.play.x + L.play.w * 0.5f,
                       L.play.y + L.play.h * 0.5f);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    a = toolbar::draw(e.ui, e.st, e.gz, true);
    e.ui.endFrame();
    e.input.clearEdges();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    a = toolbar::draw(e.ui, e.st, e.gz, true);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(a.playPressed);
}

// ---- 7. dropdown do Menu: itens novos (Settings entrou; Cenas saiu) ------------

TEST(toolbar_menu_dropdown_itens_0p7p6) {
    Env e;
    EXPECT(e.ok);
    e.st.fileMenu = true;
    e.frame();
    EXPECT(e.st.fileMenu);

    // geometria do menu: kHeaderH + 7 itens + kPad (os 7 do dropdown novo —
    // "Cenas…" SAÍU, "Settings" ENTROU na 1ª linha)
    const f32 menuH = 48.0f + 7.0f * 64.0f + 12.0f;
    const f32 ox = 0.0f, oy = 0.0f;
    const f32 x = ox + (kSW - 340.0f) * 0.5f;
    const f32 y = oy + (kSH - menuH) * 0.5f;
    // linha i: escolha i+1 (id 30+i, como sempre)
    auto rowY = [&](int i) { return y + 48.0f + static_cast<f32>(i) * 64.0f; };

    // 1ª linha = SETTINGS (o main abre o overlay de Settings — devolve 1)
    e.st.fileMenu = true;
    e.tap(x + 170.0f, rowY(0) + 28.0f);
    // (o Env da toolbar não desenha o fileMenu — o dispatch é do main; aqui
    // aferimos o drawFileMenu diretamente: reabrir e tocar devolve 1)

    UiContext& ui = e.ui;
    InputState& in = e.input;
    e.st.fileMenu = true;
    int choice = 0;
    in.injectDown(0, x + 170.0f, rowY(0) + 28.0f);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    choice = drawFileMenu(ui, in, kSW, kSH, e.st);
    ui.endFrame();
    in.clearEdges();
    in.injectUp(0);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    choice = drawFileMenu(ui, in, kSW, kSH, e.st);
    ui.endFrame();
    in.clearEdges();
    EXPECT(choice == 1);        // Settings
    EXPECT(!e.st.fileMenu);     // o clique fecha o dropdown

    // última linha = SAIR (7)
    e.st.fileMenu = true;
    in.injectDown(0, x + 170.0f, rowY(6) + 28.0f);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    choice = drawFileMenu(ui, in, kSW, kSH, e.st);
    ui.endFrame();
    in.clearEdges();
    in.injectUp(0);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    choice = drawFileMenu(ui, in, kSW, kSH, e.st);
    ui.endFrame();
    in.clearEdges();
    EXPECT(choice == 7);

    // o menu agora tem 7 itens de SEMPRE (sem o "Cenas…" — a linha 3 é
    // Carregar): tocar a 3ª devolve 3 (o main carrega a cena, não abre CENAS)
    e.st.fileMenu = true;
    in.injectDown(0, x + 170.0f, rowY(2) + 28.0f);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    choice = drawFileMenu(ui, in, kSW, kSH, e.st);
    ui.endFrame();
    in.clearEdges();
    in.injectUp(0);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    choice = drawFileMenu(ui, in, kSW, kSH, e.st);
    ui.endFrame();
    in.clearEdges();
    EXPECT(choice == 3);
}
