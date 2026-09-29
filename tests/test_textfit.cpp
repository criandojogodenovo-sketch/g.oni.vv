// tests/test_textfit.cpp — F4.2/B2: truncagem de labels (ui/TextFit.h,
// GL-free). Medidor fake (mono 10 px/char) para testar o algoritmo sem fonte
// real; teste adicional com proporções realistas do atlas da C33 (28 px:
// ~14 px/char) para o caso exato reportado ("body: rigid - sphere - chao:
// sim" cortado à direita no Inspector).
#include "TestFramework.h"
#include <cstring>
#include "ui/TextFit.h"
#include "ui/EditorLayout.h"   // kPad — largura útil do Inspector

using namespace vv;
using ::test::nearEqF;

namespace {

// medidor fake: cada char = 10 px (mono)
struct Mono10 {
    f32 operator()(const char* s) const {
        return static_cast<f32>(std::strlen(s)) * 10.0f;
    }
};

// proporção realista do atlas (28 px, sans do sistema): ~14 px/char
struct Realista {
    f32 operator()(const char* s) const {
        return static_cast<f32>(std::strlen(s)) * 14.0f;
    }
};

} // namespace

TEST(textfit_cabe_inteiro_nao_mexe) {
    char out[64];
    textfit::ellipsize("abc", 30.0f, Mono10{}, out, sizeof(out));   // 30 ≤ 30
    EXPECT(std::strcmp(out, "abc") == 0);
    textfit::ellipsize("abc", 50.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "abc") == 0);
    // string vazia / nula → vazia
    textfit::ellipsize("", 50.0f, Mono10{}, out, sizeof(out));
    EXPECT(out[0] == '\0');
    textfit::ellipsize(nullptr, 50.0f, Mono10{}, out, sizeof(out));
    EXPECT(out[0] == '\0');
}

TEST(textfit_trunca_com_maior_prefixo) {
    char out[64];
    // "abcdefgh" = 80 > 55 → tenta prefixo+... : p máx com (p+3)*10 ≤ 55 → p=2
    textfit::ellipsize("abcdefgh", 55.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "ab...") == 0);
    EXPECT(nearEqF(Mono10{}(out), 50.0f));   // de facto cabe

    // fronteira exata: (p+3)*10 == 60 == maxW → p=3
    textfit::ellipsize("abcdefgh", 60.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "abc...") == 0);

    // nem um char + reticência cabe (40 > 25) → vazia (ver caso seguinte)
    textfit::ellipsize("abcdefgh", 25.0f, Mono10{}, out, sizeof(out));
    EXPECT(out[0] == '\0');
}

TEST(textfit_nem_a_reticencia_cabe) {
    char out[64];
    // width("...") = 30 > maxW → string vazia (não desenha nada)
    textfit::ellipsize("abcdefgh", 29.0f, Mono10{}, out, sizeof(out));
    EXPECT(out[0] == '\0');
    // "..." exato → só reticência
    textfit::ellipsize("abcdefgh", 30.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "...") == 0);
    // cap pequeno → nunca overrun (comprimento < cap)
    char tiny[6];
    textfit::ellipsize("abcdefgh", 55.0f, Mono10{}, tiny, sizeof(tiny));
    EXPECT(std::strlen(tiny) < sizeof(tiny));
}

TEST(textfit_body_line_do_c33_cabe_no_inspector) {
    // largura útil do painel: kPanelW − 2*kPad = 276 px
    const f32 maxW = 300.0f - 2.0f * 12.0f;
    const char* body = "body: rigid - sphere - chao: sim";   // 32 chars

    // sem truncagem excederia (32 × 14 = 448 > 276) — o bug reportado
    EXPECT(Realista{}(body) > maxW);

    char out[64];
    textfit::ellipsize(body, maxW, Realista{}, out, sizeof(out));
    // maior prefixo: (p+3)*14 ≤ 276 → p=16 → "body: rigid - sp..."
    EXPECT(std::strcmp(out, "body: rigid - sp...") == 0);
    EXPECT(std::strncmp(out, "body:", 5) == 0);   // prefixo legível intacto
    EXPECT(out[std::strlen(out) - 1] == '.');     // termina em "..."
    EXPECT(Realista{}(out) <= maxW);              // INVARIANTE: cabe

    // labels curtas do Inspector ficam intactas
    textfit::ellipsize("Transform3D", maxW, Realista{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "Transform3D") == 0);
    // label curta do slider fica intacta no slot de 66 px (até ao trilho)
    textfit::ellipsize("velx", 66.0f, Realista{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "velx") == 0);
    // valor numérico (slot direito, mais largo) também não é tocado
    textfit::ellipsize("-180.00", 180.0f, Realista{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "-180.00") == 0);
}
