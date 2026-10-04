// tests/test_textfit.cpp — F4.2/B2: truncagem de labels (ui/TextFit.h,
// GL-free). Medidor fake (mono 10 px/BYTE) para testar o algoritmo sem
// fonte real; teste adicional com proporções realistas do atlas da C33
// (28 px: ~14 px/char) para o caso exato reportado ("body: rigid -
// sphere - chao: sim" cortado à direita no Inspector).
//
// FASE 9 (G1-2): a reticência é U+2026 "…" (3 bytes UTF-8) e a truncagem
// respeita FRONTEIRAS de code point (nunca corta um acento ao meio) —
// os casos novos afervam os dois comportamentos.
#include "TestFramework.h"
#include <cstring>
#include "ui/TextFit.h"
#include "ui/EditorLayout.h"   // kPad — largura útil do Inspector

using namespace vv;
using ::test::nearEqF;

namespace {

// medidor fake: cada BYTE = 10 px (mono — o "…" conta 30)
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
    // "abcdefgh" = 80 > 55 → prefixo+…: p máx com (p+3)*10 ≤ 55 → p=2
    textfit::ellipsize("abcdefgh", 55.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "ab…") == 0);
    EXPECT(nearEqF(Mono10{}(out), 50.0f));   // de facto cabe

    // fronteira exata: (p+3)*10 == 60 == maxW → p=3
    textfit::ellipsize("abcdefgh", 60.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "abc…") == 0);

    // nem um char + reticência cabe (40 > 25) → vazia (ver caso seguinte)
    textfit::ellipsize("abcdefgh", 25.0f, Mono10{}, out, sizeof(out));
    EXPECT(out[0] == '\0');
}

TEST(textfit_nem_a_reticencia_cabe) {
    char out[64];
    // width("…") = 30 (3 bytes) > maxW → string vazia (não desenha nada)
    textfit::ellipsize("abcdefgh", 29.0f, Mono10{}, out, sizeof(out));
    EXPECT(out[0] == '\0');
    // "…" exato → só reticência
    textfit::ellipsize("abcdefgh", 30.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "…") == 0);
    // cap pequeno → nunca overrun (comprimento < cap)
    char tiny[6];
    textfit::ellipsize("abcdefgh", 55.0f, Mono10{}, tiny, sizeof(tiny));
    EXPECT(std::strlen(tiny) < sizeof(tiny));
}

TEST(textfit_body_line_do_c33_cabe_no_inspector) {
    // largura útil do painel: kPanelW − 2*kPad = 276 px
    const f32 maxW = 300.0f - 2.0f * 12.0f;
    const char* body = "corpo: rigido - forma: esfera - chao: sim";   // FASE 9

    // sem truncagem excederia (39 × 14 = 546 > 276) — o bug reportado
    EXPECT(Realista{}(body) > maxW);

    char out[64];
    textfit::ellipsize(body, maxW, Realista{}, out, sizeof(out));
    // maior prefixo: (p+3)*14 ≤ 276 → p=16
    EXPECT(Realista{}(out) <= maxW);              // INVARIANTE: cabe
    EXPECT(std::strncmp(out, "corpo:", 6) == 0);  // prefixo legível intacto
    // termina na reticência U+2026 (3 bytes: 0xE2 0x80 0xA6)
    const u32 len = static_cast<u32>(std::strlen(out));
    EXPECT(len >= 3);
    EXPECT(static_cast<unsigned char>(out[len - 3]) == 0xE2);
    EXPECT(static_cast<unsigned char>(out[len - 2]) == 0x80);
    EXPECT(static_cast<unsigned char>(out[len - 1]) == 0xA6);

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

TEST(textfit_fase9_truncagem_respeita_code_points) {
    // FASE 9 (G1-2): a truncagem nunca CORTA um acento ao meio — o prefixo
    // para sempre numa FRONTEIRA de code point
    char out[64];
    // "seleção!" — o 'çã' são 2 bytes cada; o mono10 conta bytes:
    // s e l e ç(2B) ã(2B) o ! = 10 bytes = 100px. maxW 75 → prefixo+…
    // (p+3)*10 ≤ 75 → p=4 → "sele…" (não "seleç" cortado ao meio!)
    textfit::ellipsize("seleção!", 75.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "sele…") == 0);
    // e se a fronteira cai EXATAMENTE depois de um acento, o acento fica
    // INTEIRO: (p+3)*10 ≤ 85 → p=5… mas p=5 é meio do 'ç' → recua a 4
    textfit::ellipsize("seleção!", 85.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "sele…") == 0);
    // cabe com o acento inteiro: 90 → (p+3)*10 ≤ 90 → p=6 (depois do ç)
    textfit::ellipsize("seleção!", 90.0f, Mono10{}, out, sizeof(out));
    EXPECT(std::strcmp(out, "seleç…") == 0);
}
