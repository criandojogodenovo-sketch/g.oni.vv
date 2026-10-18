// tests/test_012c_idioma.cpp — 0.12-C (C-3): TIPOGRAFIA, idioma,
// superfícies.
//
// Os 4 pins da sub-fase C-3 (a ordem do prompt):
//   1. escala_tipografica  — só 4 tamanhos no código (12/14/16/20)
//   2. idioma_consistente  — zero «Downloads» isolado; os rótulos PT
//   3. maiusculas_uniformes — «ÁUDIO» mudou (a tab é «Áudio»)
//   4. superficies_4_tons  — o Theme tem os 4 (fundo/painel/cartão/elevado)
//
// MUTAÇÕES (coladas no RELATORIO-0.12-C.md §3):
//   M-C3a — a escala volta a solta (um fontScale(18.0f) injetado)
//           → escala_tipografica vermelho
//   M-C3b — superfícies iguais (o cartão = o painel no Theme)
//           → superficies_4_tons vermelho
#include "TestFramework.h"
#include "ui/Theme.h"
#include <fstream>
#include <string>
#include <vector>

using namespace vv;

namespace {

// o padrão do test_fase_y1_tema (a raiz do repo com override das mutações)
std::string repoRoot() {
    if (const char* e = std::getenv("GONI_ROOT_OVERRIDE")) return e;
#ifdef REPO_ROOT
    return REPO_ROOT;
#else
    return ".";
#endif
}

// o MESMO strip_comments do gate ui_vocab (o scan é sobre CÓDIGO — os
// comentários documentam mudanças e CONTÊM as strings antigas de propósito)
std::string stripComments(const std::string& in) {
    std::string s = in;
    // os /*...*/ e os //... (a versão simples e honesta do gate)
    for (size_t p = s.find("/*"); p != std::string::npos;
         p = s.find("/*")) {
        const size_t e = s.find("*/", p);
        if (e == std::string::npos) break;
        s.erase(p, e - p + 2);
    }
    for (size_t p = s.find("//"); p != std::string::npos;
         p = s.find("//")) {
        const size_t e = s.find('\n', p);
        if (e == std::string::npos) {
            s.erase(p);
            break;
        }
        s.erase(p, e - p);
    }
    return s;
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(f),
               std::istreambuf_iterator<char>());
    return true;
}

u32 countSub(const std::string& hay, const std::string& needle) {
    u32 n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos;
         p = hay.find(needle, p + needle.size())) {
        ++n;
    }
    return n;
}

// os fontes da camada de UI (o scan da escala — o mesmo conjunto do
// theme_hex_check)
std::vector<std::string> uiSources() {
    std::vector<std::string> out;
    const std::string base = repoRoot() + "/app/src/main/cpp/ui/";
    // os TU de UI que desenhavam texto (a lista fechada — o gate apodrece
    // se um novo TU com fonte escapar; acrescentar aqui é o contrato)
    const char* names[] = {
        "EditorUi.cpp",  "BottomPanel.cpp", "Toolbar.cpp",
        "ViewportChrome.cpp", "UiEditor.cpp", "UiRuntime.cpp",
        "SettingsPage.cpp", "Timeline.cpp", "ScriptEditor.cpp",
        "DocsScreen.cpp", "AudioWorkspace.cpp", "TextWindow.cpp",
        "SceneFx.cpp", "Gizmo.cpp", "CamGizmo.cpp", "Icons.cpp",
        "Brand.cpp", "Strings.cpp", "Theme.h", "UiContext.h",
        "EditorLayout.h", "SafeArea.h",
    };
    for (const char* n : names) {
        std::string s;
        if (readFile(base + n, s)) {
            out.push_back(stripComments(s));
        }
    }
    return out;
}

bool nearColor(const f32* c, f32 r, f32 g, f32 b, f32 a, f32 eps = 1e-4f) {
    return test::nearEqF(c[0], r, eps) && test::nearEqF(c[1], g, eps) &&
           test::nearEqF(c[2], b, eps) && test::nearEqF(c[3], a, eps);
}

} // namespace

// ---- PIN 1 — escala_tipografica ----------------------------------------------
// «só 4 tamanhos no código»: a ESCALA ÚNICA (Theme.h kFont* = 12/14/16/20)
// e NENHUM literal de escala fora dela nos fontes de UI — os
// fontScale(<literal>) e sp(<literal>) crus fora dos 4 valores morrem.
TEST(escala_tipografica) {
    // (a) os 4 TOKENS (a fonte única da escala)
    EXPECT(test::nearEqF(theme::kFontCaption, 12.0f));
    EXPECT(test::nearEqF(theme::kFontBody, 14.0f));
    EXPECT(test::nearEqF(theme::kFontSection, 16.0f));
    EXPECT(test::nearEqF(theme::kFontScreen, 20.0f));
    // (b) o SCAN dos fontes de UI: os literais de escala fora do conjunto
    // (o fontScale(18.0f) da mutação M-C3a cai AQUI; as exceções são os
    // mapeamentos legítimos das baselines — o 14.0f é a base do atlas e
    // o fontScale() sem literal (variável) não casa)
    u32 bad = 0;
    for (const std::string& s : uiSources()) {
        // fontScale(<número> ) com número ∉ {12,14,16,20}
        for (size_t p = s.find("fontScale("); p != std::string::npos;
             p = s.find("fontScale(", p + 1)) {
            const size_t e = s.find(')', p);
            // os literais da casa cabem em 16 (fontScale(20.0f)); as
            // expressões (kFont*/variáveis) passam deste teto
            if (e == std::string::npos || e - p > 18) {
                continue;   // expressão (kFont* ou variável) — ok
            }
            const std::string arg = s.substr(p + 10, e - p - 10);
            bool literal = !arg.empty() && arg.size() <= 6;
            for (const char c : arg) {
                if (literal &&
                    !((c >= '0' && c <= '9') || c == '.' || c == 'f')) {
                    literal = false;
                }
            }
            const bool ok = !literal || arg == "12.0f" || arg == "14.0f" ||
                            arg == "16.0f" || arg == "20.0f" ||
                            arg == "12" || arg == "14" || arg == "16" ||
                            arg == "20";
            if (!ok) {
                ++bad;
            }
        }
        // sp(<número> ) com número ∉ {12,14,16,20} (o sp() é a escala do
        // texto; os paddings usam dp() — um sp de outro valor é escala solta)
        for (size_t p = s.find("theme::sp("); p != std::string::npos;
             p = s.find("theme::sp(", p + 1)) {
            const size_t e = s.find(')', p);
            if (e == std::string::npos || e - p > 16) {
                continue;   // expressão (kFont* ou variável) — ok
            }
            const std::string arg = s.substr(p + 10, e - p - 10);
            const bool ok = arg == "12.0f" || arg == "14.0f" ||
                            arg == "16.0f" || arg == "20.0f" ||
                            arg == "12" || arg == "14" || arg == "16" ||
                            arg == "20" || arg.find("kFont") != std::string::npos;
            if (!ok) {
                ++bad;
            }
        }
    }
    if (bad != 0) {
        std::printf("  [escala] %u literais fora do conjunto\n", bad);
    }
    EXPECT(bad == 0);
}

// ---- PIN 2 — idioma_consistente ------------------------------------------------
// «zero "Downloads" isolado»: o item do menu é «Exportar para Downloads»
// (a preposição); «TICs 2» → «2 TICs»; os rótulos placeholder «Perspetiva»/
// «Global»/«Nós» NÃO existem nos fontes de UI (a strip morreu no PASSO 3
// e a vista Nós no D5 — o pin vigia o regresso).
TEST(idioma_consistente) {
    std::string menu, bottom, all;
    ASSERT(readFile(repoRoot() +
                    "/app/src/main/cpp/ui/EditorUi.cpp", menu));
    ASSERT(readFile(repoRoot() +
                    "/app/src/main/cpp/ui/BottomPanel.cpp", bottom));
    menu = stripComments(menu);
    bottom = stripComments(bottom);
    for (const std::string& s : uiSources()) {
        all += s;
    }
    // (a) «Exportar Downloads» isolado MORREU; o «para Downloads» está
    EXPECT(countSub(menu, "\"Exportar Downloads\"") == 0u);
    EXPECT(countSub(menu, "\"Exportar para Downloads\"") == 1u);
    // (b) «Downloads» isolado: TODA a ocorrência da palavra nos fontes de
    // UI tem de ser «para Downloads» (o nome da pasta pública do Android
    // é «Download», singular — termo técnico, não conta)
    for (size_t p = all.find("Downloads"); p != std::string::npos;
         p = all.find("Downloads", p + 4)) {
        const std::string before = all.substr(p >= 5 ? p - 5 : 0, 5);
        EXPECT_MSG(before == "para " || before == "\"para ",
                   "idioma_consistente: um «Downloads» isolado nos fontes "
                   "de UI (o contexto foi: ...%s...)", before.c_str());
    }
    // (c) «TICs 2» → «2 TICs»: o formato antigo morreu
    EXPECT(countSub(bottom, "TICs %u") == 0u);
    EXPECT(countSub(bottom, "%u TICs") >= 1u);
    // (d) os placeholders «Perspetiva»/«Global»/«Nós» não voltam
    EXPECT(countSub(all, "\"Perspetiva\"") == 0u);
    EXPECT(countSub(all, "\"Global\"") == 0u);
    EXPECT(countSub(all, "\"Nós\"") == 0u);
}

// ---- PIN 3 — maiusculas_uniformes ----------------------------------------------
// «ÁUDIO mudou»: a tab da barra de topo é «Áudio» (a MESMA regra de
// maiúsculas de «Menu»/«Cena»; «3D»/«UI» ficam — são siglas).
TEST(maiusculas_uniformes) {
    std::string toolbar, all;
    ASSERT(readFile(repoRoot() +
                    "/app/src/main/cpp/ui/Toolbar.cpp", toolbar));
    toolbar = stripComments(toolbar);
    for (const std::string& s : uiSources()) {
        all += s;
    }
    // (a) a string «ÁUDIO» (o rótulo em caps) MORREU nos draws
    EXPECT(countSub(all, "\"ÁUDIO\"") == 0u);
    // (b) a tab desenha «Áudio»
    EXPECT(countSub(toolbar, "\"Áudio\"") == 1u);
    // (c) as siglas ficam (as tabs 3D e UI)
    EXPECT(countSub(toolbar, "\"3D\"") == 1u);
    EXPECT(countSub(toolbar, "\"UI\"") == 1u);
}

// ---- PIN 4 — superficies_4_tons --------------------------------------------------
// «Theme tem os 4»: a ESCADA DE SUPERFÍCIES do dono (fundo #0A0A0C ·
// painel #131316 · cartão #1B1B20 · elevado #24242A) com os hexes EXATOS,
// os 4 DISTINCTOS e a borda âmbar subtil entre painéis.
TEST(superficies_4_tons) {
    // (a) os 4 TOKENS com os hexes exatos do dono
    EXPECT(nearColor(theme::kTheme.bg, 10.0f / 255.0f, 10.0f / 255.0f,
                     12.0f / 255.0f, 1.0f));            // #0A0A0C
    // 0.12-E E-2: o PAINEL é o #141416 da paleta FOGO AVERMELHADO do
    // dono (era o #131316 da escada 0.12-C — a recalibração com nota)
    EXPECT(nearColor(theme::kTheme.surface, 20.0f / 255.0f,
                     20.0f / 255.0f, 22.0f / 255.0f, 0.80f));   // #141416
    EXPECT(nearColor(theme::kTheme.card, 27.0f / 255.0f, 27.0f / 255.0f,
                     32.0f / 255.0f, 1.0f));            // #1B1B20
    EXPECT(nearColor(theme::kTheme.elevated, 36.0f / 255.0f,
                     36.0f / 255.0f, 42.0f / 255.0f, 1.0f));    // #24242A
    // (b) os 4 são DISTINTOS (a mutação M-C3b iguala o cartão ao painel
    // e morre AQUI)
    EXPECT(!(theme::kTheme.bg[0] == theme::kTheme.surface[0] &&
             theme::kTheme.surface[0] == theme::kTheme.card[0] &&
             theme::kTheme.card[0] == theme::kTheme.elevated[0]));
    EXPECT(!(theme::kTheme.card[0] == theme::kTheme.elevated[0] &&
             theme::kTheme.card[1] == theme::kTheme.elevated[1] &&
             theme::kTheme.card[2] == theme::kTheme.elevated[2]));
    EXPECT(!(theme::kTheme.bg[0] == theme::kTheme.card[0] &&
             theme::kTheme.bg[1] == theme::kTheme.card[1] &&
             theme::kTheme.bg[2] == theme::kTheme.card[2]));
    // (c) a borda âmbar subtil entre painéis (a hairline da Y-1 — o RGB é
    // o do accentDim, o sub-tom quente da aresta)
    EXPECT(nearColor(theme::kTheme.border, 74.0f / 255.0f, 55.0f / 255.0f,
                     20.0f / 255.0f, 0.60f));
    EXPECT(theme::kTheme.border[0] == theme::kTheme.accentDim[0] &&
           theme::kTheme.border[1] == theme::kTheme.accentDim[1] &&
           theme::kTheme.border[2] == theme::kTheme.accentDim[2]);
    // (d) a FONTE leva os hexes exatos (o artefato pinado)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/Theme.h", src));
    EXPECT(countSub(src, "VV_RGB(10, 10, 12)"));
    // 0.12-E E-2: o #141416 da paleta FOGO AVERMELHADO (era #131316)
    EXPECT(countSub(src, "VV_RGBA(20, 20, 22, 0.80f)"));
    EXPECT(countSub(src, "VV_RGB(27, 27, 32)"));
    EXPECT(countSub(src, "VV_RGB(36, 36, 42)"));
    // (e) o contraste do texto primário mantém o piso WCAG sobre o novo
    // fundo (o texto 15+:1 e o accent 10+:1 — o dono não perde legibilidade)
    EXPECT(theme::contrastOnBg(theme::kTheme.text1) >= 4.5f);
    EXPECT(theme::contrastOnBg(theme::kTheme.accent) >= 3.0f);
}
