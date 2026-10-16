// tests/test_fase_y0_icone.cpp — os pins da Y-0 (0.11-Y: o ícone «G em
// chama» no launcher). O contrato do prompt da Y-0: o adaptativo compõe o
// fundo sólido #08080A com o foreground nas 5 densidades (o canvas 108 dp
// × densidade); o legacy (Android 7-) é o composto quadrado + round
// (48/72/96/144/192); o manifest aponta @mipmap/ic_launcher{,_round}; o
// ícone ANTIGO (o gerador 0.9.6.10, scripts/gen_app_icon.py) não sobra em
// res/mipmap-*/. Os pins verificam EXISTÊNCIA e CONTEÚDO (a estrutura do
// PNG via IHDR, o XML fonte, o manifest fonte) — NÃO verificam o desenho
// pixel a pixel; a validação binária (aapt2 do APK) corre no job do APK
// a cada build (o gate release-identity depende do build passar).
//
// As MUTAÇÕES do relatório (M-Y0a: foreground só numa densidade; M-Y0b:
// manifest com o ícone antigo) correm contra uma CÓPIA do repo via
// GONI_ROOT_OVERRIDE — o mesmo padrão dos pins ctx_* (test_ctx_pines.cpp).
#include "TestFramework.h"

#include <sys/wait.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string y0Root() {
    if (const char* e = std::getenv("GONI_ROOT_OVERRIDE")) return e;
#ifdef REPO_ROOT
    return REPO_ROOT;
#else
    return ".";
#endif
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(f),
               std::istreambuf_iterator<char>());
    return true;
}

// popen capturado (o sha256sum do pin antigo e os ls das densidades)
std::string runCapture(const std::string& cmd, int& exitCode) {
    std::string out;
    FILE* p = ::popen(cmd.c_str(), "r");
    if (!p) {
        exitCode = -1;
        return "<popen falhou>";
    }
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    int st = ::pclose(p);
    exitCode = st == -1 ? -1 : WEXITSTATUS(st);
    return out;
}

// o cabeçalho IHDR do PNG: dimensões exatas + RGBA (color type 6) — o
// formato que os 3 PNGs da Y-0 têm de ter (o foreground é transparente
// fora do glifo; o legacy carrega o fundo #08080A no próprio bitmap)
bool pngIhdr(const std::string& path, long& w, long& h, int& colorType) {
    std::string b;
    if (!readFile(path, b) || b.size() < 26) return false;
    static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n',
                                         0x1a, '\n'};
    if (std::memcmp(b.data(), sig, 8) != 0) return false;
    w = (static_cast<unsigned char>(b[16]) << 24) |
        (static_cast<unsigned char>(b[17]) << 16) |
        (static_cast<unsigned char>(b[18]) << 8) |
        static_cast<unsigned char>(b[19]);
    h = (static_cast<unsigned char>(b[20]) << 24) |
        (static_cast<unsigned char>(b[21]) << 16) |
        (static_cast<unsigned char>(b[22]) << 8) |
        static_cast<unsigned char>(b[23]);
    colorType = static_cast<unsigned char>(b[25]);
    return true;
}

const char* kDens[5] = {"mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi"};
const long kFgSide[5] = {108, 162, 216, 324, 432};   // 108 dp × densidade
const long kLegSide[5] = {48, 72, 96, 144, 192};     // o legacy clássico

}  // namespace

// y0_foreground — o PNG do foreground existe nas 5 densidades com as
// dimensões exatas do canvas adaptativo e é RGBA
TEST(y0_foreground) {
    for (int i = 0; i < 5; ++i) {
        const std::string p = y0Root() + "/app/src/main/res/mipmap-" +
                              kDens[i] + "/ic_launcher_foreground.png";
        long w = 0, h = 0;
        int ct = 0;
        EXPECT_MSG(pngIhdr(p, w, h, ct),
                   "mipmap-%s/ic_launcher_foreground.png em falta ou PNG "
                   "inválido",
                   kDens[i]);
        if (!pngIhdr(p, w, h, ct)) continue;
        EXPECT_MSG(w == kFgSide[i] && h == kFgSide[i],
                   "mipmap-%s/ic_launcher_foreground.png é %ldx%ld — "
                   "esperado %ldx%ld (o canvas 108 dp × densidade)",
                   kDens[i], w, h, kFgSide[i], kFgSide[i]);
        EXPECT_MSG(ct == 6, "mipmap-%s/ic_launcher_foreground.png não é "
                            "RGBA (color type %d — o glifo precisa de "
                            "alpha)",
                   kDens[i], ct);
    }
}

// y0_background — o fundo sólido é o #08080A (a ordem expressa do dono)
TEST(y0_background) {
    std::string text;
    EXPECT_MSG(readFile(y0Root() +
                            "/app/src/main/res/values/"
                            "ic_launcher_background.xml",
                        text),
               "values/ic_launcher_background.xml em falta");
    if (text.empty()) return;
    EXPECT(text.find("<color name=\"ic_launcher_background\">#08080A"
                     "</color>") != std::string::npos);
}

// y0_adaptive — os 2 XML do adaptativo (Android 8+) compõem fundo +
// foreground pelos nomes do contrato
TEST(y0_adaptive) {
    const char* names[2] = {"ic_launcher.xml", "ic_launcher_round.xml"};
    for (int i = 0; i < 2; ++i) {
        const std::string p = y0Root() + "/app/src/main/res/mipmap-anydpi-v26/" +
                              names[i];
        std::string text;
        EXPECT_MSG(readFile(p, text), "mipmap-anydpi-v26/%s em falta",
                   names[i]);
        if (text.empty()) continue;
        EXPECT(text.find("<adaptive-icon") != std::string::npos);
        EXPECT(text.find("@color/ic_launcher_background") !=
               std::string::npos);
        EXPECT(text.find("@mipmap/ic_launcher_foreground") !=
               std::string::npos);
    }
}

// y0_legacy — o launcher (Android 7-) nas 5 densidades: o composto
// quadrado + o round (com o canto transparente — a máscara circular)
TEST(y0_legacy) {
    for (int i = 0; i < 5; ++i) {
        const std::string base = y0Root() + "/app/src/main/res/mipmap-" +
                                 kDens[i];
        long w = 0, h = 0;
        int ct = 0;
        const std::string sq = base + "/ic_launcher.png";
        EXPECT_MSG(pngIhdr(sq, w, h, ct),
                   "mipmap-%s/ic_launcher.png em falta ou PNG inválido",
                   kDens[i]);
        if (pngIhdr(sq, w, h, ct)) {
            EXPECT_MSG(w == kLegSide[i] && h == kLegSide[i],
                       "mipmap-%s/ic_launcher.png é %ldx%ld — esperado "
                       "%ldx%ld",
                       kDens[i], w, h, kLegSide[i], kLegSide[i]);
        }
        const std::string rd = base + "/ic_launcher_round.png";
        EXPECT_MSG(pngIhdr(rd, w, h, ct),
                   "mipmap-%s/ic_launcher_round.png em falta ou PNG "
                   "inválido",
                   kDens[i]);
        if (pngIhdr(rd, w, h, ct)) {
            EXPECT_MSG(w == kLegSide[i] && h == kLegSide[i],
                       "mipmap-%s/ic_launcher_round.png é %ldx%ld — "
                       "esperado %ldx%ld",
                       kDens[i], w, h, kLegSide[i], kLegSide[i]);
            EXPECT_MSG(ct == 6, "mipmap-%s/ic_launcher_round.png não é "
                                "RGBA (a máscara circular precisa de "
                                "alpha)",
                       kDens[i], ct);
        }
    }
}

// y0_manifest — o manifest aponta para os nomes do contrato
TEST(y0_manifest) {
    std::string text;
    ASSERT(readFile(y0Root() + "/app/src/main/AndroidManifest.xml", text));
    EXPECT(text.find("android:icon=\"@mipmap/ic_launcher\"") !=
           std::string::npos);
    EXPECT(text.find("android:roundIcon=\"@mipmap/ic_launcher_round\"") !=
           std::string::npos);
}

// y0_aapt2 — a FONTE que o aapt2 consome fecha: o manifest cita
// @mipmap/ic_launcher{,_round} e os recursos citados existem (o XML do
// adaptativo resolve @color + @mipmap/foreground; o legacy existe nas 5
// densidades). A validação BINÁRIA (aapt2 no APK) corre no job do APK do
// CI — este pin aferra a fonte para o erro aparecer ANTES do APK.
TEST(y0_aapt2) {
    const std::string res = y0Root() + "/app/src/main/res";
    std::string text;
    ASSERT(readFile(y0Root() + "/app/src/main/AndroidManifest.xml", text));
    const bool citaLauncher =
        text.find("@mipmap/ic_launcher\"") != std::string::npos;
    const bool citaRound =
        text.find("@mipmap/ic_launcher_round\"") != std::string::npos;
    EXPECT(citaLauncher && citaRound);
    if (!citaLauncher && !citaRound) return;
    // @color/ic_launcher_background tem de estar definido (o adaptativo
    // referencia-o) e os XML do adaptativo têm de existir
    std::string bg;
    EXPECT(readFile(res + "/values/ic_launcher_background.xml", bg));
    std::string adaptive;
    EXPECT(readFile(res + "/mipmap-anydpi-v26/ic_launcher.xml", adaptive));
    EXPECT(adaptive.find("@mipmap/ic_launcher_foreground") !=
           std::string::npos);
    // o foreground citado pelo adaptativo existe nas 5 densidades
    for (int i = 0; i < 5; ++i) {
        std::string b;
        EXPECT_MSG(readFile(res + "/mipmap-" + kDens[i] +
                                "/ic_launcher_foreground.png",
                            b),
                   "@mipmap/ic_launcher_foreground não resolve em "
                   "mipmap-%s (o aapt2 do APK gritaria)",
                   kDens[i]);
    }
}

// y0_antigo_removido — nada do ícone ANTIGO sobra em res/mipmap-*/: (a) o
// conjunto de ficheiros por densidade é EXATAMENTE o da Y-0 (nada velho
// com outro nome); (b) nenhum ficheiro tem o sha256 dos PNG do gerador
// 0.9.6.10 (capturados no b577dea ANTES da substituição — o launcher ==
// round tinha o mesmo blob por densidade; os hashes são a impressão
// digital do desenho antigo SEM ver pixels)
TEST(y0_antigo_removido) {
    static const char* kOldHash[5] = {
        // mdpi · hdpi · xhdpi · xxhdpi · xxxhdpi (o gen_app_icon 0.9.6.10)
        "2a41711a2f7dc85d682946fe9099d0d73dd0fca8165d9ee8a308c9270788be45",
        "65e7b8f43d3e966fd6417bea0d0023951c457d49ae9b87d15fc86a559887ac11",
        "af4e4aa15288c2e38e0d8cfe45e1763c47c3821e1eaa790fb5fe7a60b2996725",
        "9792bda6b70d466ca2d91258d5d485a1073b42aa65b38e9a885979b39908a293",
        "379f236619813d58c1b7f035e9c86adf80cd6e5470274ece56eaff6b58d1b212"};
    for (int i = 0; i < 5; ++i) {
        const std::string dir = y0Root() + "/app/src/main/res/mipmap-" +
                                kDens[i];
        int code = 0;
        const std::string listing =
            runCapture("ls -1 \"" + dir + "\" 2>/dev/null", code);
        ASSERT(code == 0);
        // (a) o conjunto exato — nada além dos 3 do contrato
        long n = 0;
        size_t pos = 0;
        while (pos <= listing.size()) {
            const size_t nl = listing.find('\n', pos);
            if (nl == std::string::npos && pos >= listing.size()) break;
            const std::string f =
                listing.substr(pos, nl == std::string::npos
                                        ? std::string::npos
                                        : nl - pos);
            pos = nl + 1;
            if (f.empty()) continue;
            ++n;
            EXPECT_MSG(f == "ic_launcher.png" ||
                           f == "ic_launcher_round.png" ||
                           f == "ic_launcher_foreground.png",
                       "mipmap-%s tem o ficheiro inesperado «%s» — sobra "
                       "do ícone antigo?",
                       kDens[i], f.c_str());
        }
        EXPECT_MSG(n == 3, "mipmap-%s tem %ld ficheiros — esperado o "
                           "conjunto exato da Y-0 (3)",
                   kDens[i], n);
        // (b) o sha256 de cada PNG presente não pode ser o do gerador
        // antigo (a impressão digital do desenho 0.9.6.10)
        pos = 0;
        while (pos <= listing.size()) {
            const size_t nl = listing.find('\n', pos);
            if (nl == std::string::npos && pos >= listing.size()) break;
            const std::string f =
                listing.substr(pos, nl == std::string::npos
                                        ? std::string::npos
                                        : nl - pos);
            pos = nl + 1;
            if (f.empty() || f.find(".png") == std::string::npos) continue;
            int c2 = 0;
            const std::string sh =
                runCapture("sha256sum \"" + dir + "/" + f +
                               "\" 2>/dev/null | cut -d' ' -f1",
                           c2);
            for (int k = 0; k < 5; ++k) {
                EXPECT_MSG(sh.find(kOldHash[k]) == std::string::npos,
                           "mipmap-%s/%s é o PNG ANTIGO do gerador "
                           "0.9.6.10 (sha256 %c%s…) — o ícone antigo "
                           "sobrou",
                           kDens[i], f.c_str(), kOldHash[k][0],
                           kOldHash[k] + 1);
            }
        }
    }
}
