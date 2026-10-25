// tests/raster_golden.h — 0.12-Z2 F5: A MAQUINARIA DAS GOLDEN IMAGES
// (pura, SEM GL — o test_core aferra-a; o raster_z2 consome-a no GLES
// real). O pedido do dono (FASE 5): «golden images 回归 + sentinela com
// mutação».
//
// O QUE É: a regressão POR IMAGEM da campanha. Cada cena dourada é um
// frame DETERMINÍSTICO (dt fixo 1/60 nos 30 frames de assentamento, os
// relógios g_fxTime/g_skyTime AFOFERRADOS com stepFrame(0.0f) antes da
// leitura, o thunderTimer do clima longe do flash, os seeds do buffer de
// partículas determinísticos no INIT) cuja identidade vive em
// tests/golden/golden_z2.txt: nome + hash FNV-1a 64 (o MESMO algoritmo do
// frameHash do raster_z2 — o V9 do clima e as goldens partilham o
// contrato) + a assinatura estrutural da região central (nonBackground,
// distinct, meanLum).
//
// O VEREDICTO é o da casa (o V9 do --clima): EXATO (o hash igual — a
// referência llvmpipe é byte-estável), TOLERANCIA (a política do V9: diff
// < 0.5% dos píxeis E maxDelta ≤ 32/255 — o chip do FPS muda o número, o
// CONTEÚDO não) ou FALHA (a regressão — o diff PNG vermelho sai e o exit
// é 1). A tolerância NUNCA esconde um FALHA: o TOLERANCIA é listado no
// relatório com a contagem, o FALHA mata.
//
// DETERMINISMO ENTRE MÁQUINAS: o golden_z2.txt é gerado NESTE Mesa
// (a versão vive no cabeçalho do ficheiro e no relatório). Outro Mesa que
// mude píxeis sai pelo PROTOCOLO DE REGENERAÇÃO (--golden-write + o diff
// colado no relatório — nunca um amarelo silencioso). O CI NÃO corre o
// golden (não tem Mesa — igual ao raster_z2); a MAQUINARIA corre no
// test_core (os vetores do FNV, o parser, os veredictos — a R-074
// aferra-a com a mutação vermelha).
#ifndef RASTER_GOLDEN_H
#define RASTER_GOLDEN_H

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace gogolden {

// o FNV-1a 64 — os MESMOS constantes do frameHash (tests/raster_z2.cpp):
// o hash das goldens e o hash do clima V9 são O MESMO algoritmo (uma
// divergência aqui seria o contrato a mentir sobre si próprio)
constexpr uint64_t kFnvOffset = 1469598103934665603ull;
constexpr uint64_t kFnvPrime = 1099511628211ull;

inline uint64_t fnv1a64(const unsigned char* d, size_t n) {
    uint64_t h = kFnvOffset;
    for (size_t i = 0; i < n; ++i) {
        h ^= d[i];
        h *= kFnvPrime;
    }
    return h;
}

// a política de tolerância da casa (o V9 do clima: o chip do FPS muda o
// número; o CONTEÚDO não) — apertar aqui é deitar fora o contrato do V9,
// afrouxar aqui é a regressão a passar
constexpr double kTolPct = 0.5;    // % de píxeis diferentes (do frame todo)
constexpr int kTolMaxDelta = 32;   // o maior salto por canal (0..255)

enum class Verdict { EXATO, TOLERANCIA, FALHA };

inline const char* verdictName(Verdict v) {
    switch (v) {
        case Verdict::EXATO: return "EXATO";
        case Verdict::TOLERANCIA: return "TOLERANCIA";
        case Verdict::FALHA: return "FALHA";
    }
    return "?";
}

// a linha do golden (o formato ÚNICO: "nome hash nonBg distinct meanLum";
// o hash em %016llx para a leitura humana do diff)
struct GoldenEntry {
    std::string name;
    uint64_t hash = 0;
    long nonBg = 0;     // píxeis não-fundo na região central (o piso)
    long distinct = 0;  // cores distintas amostradas (2×2)
    double meanLum = 0.0;
};

inline std::string goldenLine(const GoldenEntry& g) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s %016llx %ld %ld %.6f",
                  g.name.c_str(), static_cast<unsigned long long>(g.hash),
                  g.nonBg, g.distinct, g.meanLum);
    return std::string(buf);
}

inline std::string trimCopy(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' ||
                     s[b - 1] == '\r' || s[b - 1] == '\n'))
        --b;
    return s.substr(a, b - a);
}

// o parser da linha: EXATAMENTE 5 campos (nome hash nonBg distinct
// meanLum) — o nome NÃO contém espaços; linha vazia ou '#' (comentário do
// protocolo) devolve false SEM ser erro; linha malformada TAMBÉM false
// (o chamador distingue por out.name vazio = comentário vs não-vazio =
// erro de formato)
inline bool parseGoldenLine(const std::string& line, GoldenEntry& out) {
    out = GoldenEntry();
    const std::string t = trimCopy(line);
    if (t.empty() || t[0] == '#') return false;
    char name[128] = {0};
    unsigned long long h = 0;
    long nb = 0, dc = 0;
    double ml = 0.0;
    const int got = std::sscanf(t.c_str(), "%127s %16llx %ld %ld %lf", name,
                                &h, &nb, &dc, &ml);
    if (got != 5 || name[0] == 0) return false;
    out.name = name;
    out.hash = static_cast<uint64_t>(h);
    out.nonBg = nb;
    out.distinct = dc;
    out.meanLum = ml;
    return true;
}

// a lista inteira (o ficheiro golden_z2.txt: comentários '#' + linhas);
// devolve false se ALGUMA linha de dados estiver malformada (o ficheiro
// podre morre cedo — nunca uma golden silenciosamente ignorada)
inline bool parseGoldenList(const std::string& text,
                            std::vector<GoldenEntry>& out) {
    out.clear();
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(start, end - start);
        GoldenEntry e;
        if (parseGoldenLine(line, e)) {
            out.push_back(e);
        } else if (!trimCopy(line).empty() && trimCopy(line)[0] != '#') {
            return false;   // linha de dados malformada
        }
        if (end == text.size()) break;
        start = end + 1;
    }
    return true;
}

// o diff RGBA byte a byte (a contagem da casa: um píxel difere se
// QUALQUER canal difere; o maxDelta é o maior salto por canal)
struct DiffReport {
    long diff = 0;
    int maxDelta = 0;
    double pct = 0.0;   // % do frame inteiro
};

inline DiffReport diffRgba(const unsigned char* a, const unsigned char* b,
                           size_t bytes) {
    DiffReport r;
    const size_t np = bytes / 4;
    for (size_t i = 0; i < np; ++i) {
        const unsigned char* pa = a + i * 4;
        const unsigned char* pb = b + i * 4;
        int md = 0;
        for (int k = 0; k < 4; ++k) {
            const int d = pa[k] > pb[k] ? pa[k] - pb[k] : pb[k] - pa[k];
            if (d > md) md = d;
        }
        if (md > 0) {
            ++r.diff;
            if (md > r.maxDelta) r.maxDelta = md;
        }
    }
    r.pct = np ? 100.0 * static_cast<double>(r.diff) /
                     static_cast<double>(np)
               : 0.0;
    return r;
}

inline Verdict verdict(const DiffReport& d) {
    if (d.diff == 0) return Verdict::EXATO;
    if (d.pct < kTolPct && d.maxDelta <= kTolMaxDelta)
        return Verdict::TOLERANCIA;
    return Verdict::FALHA;
}

// a máscara de diff para o PNG da FALHA (píxel igual → cinza escuro
// 24/32/40; píxel diferente → vermelho 232/64/56 — a regressão SALTA à
// vista no capture que vai ao relatório)
inline void diffMaskRgba(const unsigned char* a, const unsigned char* b,
                         size_t bytes, unsigned char* out) {
    const size_t np = bytes / 4;
    for (size_t i = 0; i < np; ++i) {
        const unsigned char* pa = a + i * 4;
        const unsigned char* pb = b + i * 4;
        unsigned char* po = out + i * 4;
        bool same = pa[0] == pb[0] && pa[1] == pb[1] && pa[2] == pb[2] &&
                    pa[3] == pb[3];
        if (same) {
            po[0] = 24; po[1] = 32; po[2] = 40; po[3] = 255;
        } else {
            po[0] = 232; po[1] = 64; po[2] = 56; po[3] = 255;
        }
    }
}

// o caminho RELATIVO do ficheiro dourado (o consumidor junta o REPO_ROOT;
// o literal vive AQUI para o pino da R-074 aferrar o contrato)
inline const char* goldenRelPath() { return "tests/golden/golden_z2.txt"; }

}  // namespace gogolden

#endif  // RASTER_GOLDEN_H
