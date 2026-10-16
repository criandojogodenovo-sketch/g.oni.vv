// tests/test_fase_y_fecho.cpp — o pin do FECHO da campanha 0.11-Y (o
// critério C33 ⑤ do dono: «PNG de cada ecrã → docs/y-*.png»). Os 13 PNGs
// da VARREDURA (a FASE 31 do device virtual — cada ecrã desenhado pelo
// frame() de produção com o audit ligado, o export do Grupo B) copiados
// para docs/: o pin aferra EXISTÊNCIA + PNG válido (a assinatura) + as
// dimensões da superfície do harness (1600x720) + conteúdo não-trivial.
//
// A MUTAÇÃO do relatório (M-Fecho: um PNG removido) corre contra uma
// CÓPIA do repo via GONI_ROOT_OVERRIDE — o padrão dos pins y0_*/y1_*.
#include "TestFramework.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace vv;

namespace {

std::string fechoRoot() {
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

const char* const kEcras[13] = {
    "editor3d",  "drawer-ficheiros", "drawer-assets", "drawer-consola",
    "drawer-animacao", "modo-ui", "modo-audio", "settings", "docs",
    "script", "storage", "settings+drawer", "drawer-max-400"};

} // namespace

TEST(y_fecho_pngs) {
    for (const char* nome : kEcras) {
        const std::string path =
            fechoRoot() + "/docs/y-" + nome + ".png";
        std::string b;
        ASSERT(readFile(path, b));   // em falta = o ASSERT diz QUAL
        // a assinatura do PNG
        static const unsigned char sig[8] = {0x89, 'P', 'N', 'G',
                                             '\r', '\n', 0x1a, '\n'};
        EXPECT_MSG(b.size() > 33 &&
                       std::memcmp(b.data(), sig, 8) == 0,
                   "y_fecho: docs/y-%s.png não é um PNG válido", nome);
        if (b.size() <= 33) continue;
        // as dimensões da superfície do harness (a varredura corre a
        // 1600x720 — a instância das FASEs finais do device virtual)
        const long w = (static_cast<long>(
                            static_cast<unsigned char>(b[16])) << 24) |
                       (static_cast<long>(
                            static_cast<unsigned char>(b[17])) << 16) |
                       (static_cast<long>(
                            static_cast<unsigned char>(b[18])) << 8) |
                       static_cast<long>(static_cast<unsigned char>(b[19]));
        const long h = (static_cast<long>(
                            static_cast<unsigned char>(b[20])) << 24) |
                       (static_cast<long>(
                            static_cast<unsigned char>(b[21])) << 16) |
                       (static_cast<long>(
                            static_cast<unsigned char>(b[22])) << 8) |
                       static_cast<long>(static_cast<unsigned char>(b[23]));
        EXPECT_MSG(w == 1600 && h == 720,
                   "y_fecho: docs/y-%s.png tem de ser a superfície "
                   "1600x720 (é %ldx%ld)", nome, w, h);
        // conteúdo: um screenshot do editor tem estrutura (o PNG comprime
        // o poço liso, mas os glifos/painéis dão bytes); o piso honesto
        // é o vazio absoluto (0 KB = o export falhou)
        EXPECT_MSG(b.size() > 2048,
                   "y_fecho: docs/y-%s.png vazio ou degenerado (%zu B)",
                   nome, b.size());
    }
}
