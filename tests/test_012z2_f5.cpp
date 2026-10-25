// tests/test_012z2_f5.cpp — 0.12-Z2 FASE 5: OS PINS DA MAQUINARIA DAS
// GOLDEN IMAGES (a regressão POR IMAGEM da campanha — o CI não tem Mesa,
// mas a MECÂNICA tem de estar viva e aferrada AQUI):
//
//   golden_fnv_conhecido   — o FNV-1a 64 é O MESMO algoritmo do frameHash
//                            (os vetores canónicos do FNV + a estabilidade
//                            do buffer de zeros); mudar o offset/prime é a
//                            mutação que o R-074 apanha no fonte
//   golden_parse_roundtrip — a linha "nome hash nonBg distinct meanLum"
//                            nasce e volta IGUAL; o comentário e o vazio
//                            são ignorados SEM erro; a linha podre é
//                            RECUSADA (o ficheiro morto morre cedo)
//   golden_veredicto       — EXATO (0 diff), TOLERANCIA (a política do V9:
//                            diff <0.5% E maxDelta ≤32 — o chip do FPS
//                            muda, o conteúdo não), FALHA (a regressão
//                            mata); o limiar NÃO é simétrico: 0.5% com
//                            maxDelta 33 é FALHA (o afrouxamento por um
//                            canal não passa)
//   golden_mask_diff       — a máscara da FALHA: igual→cinza escuro,
//                            diferente→vermelho (o capture da regressão
//                            salta à vista)
//
// O raster_z2 consome tudo no GLES real (--golden verify / --golden-write
// regeneração; o protocolo vive no RELATORIO-0.12-Z2-F5.md §F5.3).
#include "TestFramework.h"
#include "raster_golden.h"
#include <cstring>
#include <string>
#include <vector>

using namespace vv;

namespace {

void expectEntry(const gogolden::GoldenEntry& a, const gogolden::GoldenEntry& b,
                 const char* what) {
    EXPECT_MSG(a.name == b.name && a.hash == b.hash && a.nonBg == b.nonBg &&
                   a.distinct == b.distinct && a.meanLum == b.meanLum,
               what);
}

}  // namespace

TEST(golden_fnv_conhecido) {
    // o FNV-1a 64 da CASA: os MESMOS constantes do frameHash
    // (tests/raster_z2.cpp — o offset 1469598103934665603 é o constante
    // HISTÓRICO do frameHash, NÃO é o canónico 14695981039346656037 de
    // 10 dígitos; mudá-lo quebraria o contrato do V9 e a lista dourada —
    // não é decisão desta campanha). Os vetores abaixo são da SEQUÊNCIA
    // da casa (offset^byte × prime, mod 2^64 — computados à mão):
    EXPECT(gogolden::kFnvOffset == 1469598103934665603ull);
    EXPECT(gogolden::kFnvPrime == 1099511628211ull);
    const unsigned char a = static_cast<unsigned char>('a');
    EXPECT_MSG(gogolden::fnv1a64(&a, 1) == 0x44bd8ad473cd9906ull,
               "o FNV-1a 64 da casa de \"a\" tem de ser 0x44bd8ad473cd9906 "
               "(offset 1469598103934665603 — o MESMO do frameHash/V9)");
    const unsigned char foobar[] = {'f', 'o', 'o', 'b', 'a', 'r'};
    EXPECT_MSG(gogolden::fnv1a64(foobar, sizeof(foobar)) ==
                   0x88fad7c0a8ff07f2ull,
               "o FNV-1a 64 da casa de \"foobar\" tem de ser "
               "0x88fad7c0a8ff07f2");
    // o buffer de zeros: estável e ≠ do offset (o xor do 0 não move, o mul
    // sim — a sequência tem de ser a mesma em duas corridas)
    std::vector<unsigned char> zeros(1024, 0);
    const uint64_t h1 = gogolden::fnv1a64(zeros.data(), zeros.size());
    const uint64_t h2 = gogolden::fnv1a64(zeros.data(), zeros.size());
    EXPECT(h1 == h2);
    EXPECT(h1 != gogolden::kFnvOffset);
    // e o hash sobre um buffer RGBA 2×2: determinístico e SENSÍVEL (mudar
    // um byte do canal B muda o hash — o diff escondido não passa)
    const unsigned char px[16] = {1, 2, 3, 255, 4, 5, 6, 255,
                                  7, 8, 9, 255, 10, 11, 12, 255};
    const uint64_t hp = gogolden::fnv1a64(px, sizeof(px));
    EXPECT(hp == gogolden::fnv1a64(px, sizeof(px)));   // determinístico
    unsigned char px2[16];
    std::memcpy(px2, px, sizeof(px2));
    px2[14] ^= 0x01;   // um canal B a delta 1
    EXPECT(hp != gogolden::fnv1a64(px2, sizeof(px2)));
}

TEST(golden_parse_roundtrip) {
    gogolden::GoldenEntry g;
    g.name = "default_we";
    g.hash = 0x8115ac2c7aa9db76ull;
    g.nonBg = 403;
    g.distinct = 214;
    g.meanLum = 0.331077;

    // nasce e volta IGUAL (o formato ÚNICO do ficheiro)
    const std::string line = gogolden::goldenLine(g);
    gogolden::GoldenEntry back;
    EXPECT_MSG(gogolden::parseGoldenLine(line, back),
               "a linha do golden tem de parsear");
    expectEntry(g, back, "o roundtrip da linha tem de ser exato");

    // comentário e vazio: ignorados SEM erro (o protocolo vive no ficheiro)
    gogolden::GoldenEntry tmp;
    EXPECT(!gogolden::parseGoldenLine("# golden_z2 — Mesa 25.0.7", tmp));
    EXPECT(tmp.name.empty());
    EXPECT(!gogolden::parseGoldenLine("", tmp));
    EXPECT(!gogolden::parseGoldenLine("   \t ", tmp));

    // a linha PODRE é recusada (o ficheiro morto morre cedo — nunca uma
    // golden silenciosamente ignorada)
    EXPECT(!gogolden::parseGoldenLine("so_o_nome", tmp));
    EXPECT(!gogolden::parseGoldenLine("nome zz 1 2 3", tmp));
    EXPECT(!gogolden::parseGoldenLine("nome 11 22 33", tmp));

    // a lista inteira: comentários + dados; uma linha podre → false
    std::vector<gogolden::GoldenEntry> lst;
    const std::string bom = std::string("# cabeçalho\n") +
                            gogolden::goldenLine(g) + "\n";
    EXPECT(gogolden::parseGoldenList(bom, lst));
    EXPECT(lst.size() == 1);
    expectEntry(lst[0], g, "a lista preserva a entrada");
    EXPECT(!gogolden::parseGoldenList(bom + "lixo aqui\n", lst));
    // a ÚLTIMA linha sem \n também entra (o editor pode cortar o fim)
    EXPECT(gogolden::parseGoldenList(gogolden::goldenLine(g), lst));
    EXPECT(lst.size() == 1);
}

TEST(golden_veredicto) {
    // 32×32 = 1024 píxeis
    std::vector<unsigned char> a(1024 * 4, 90), b(1024 * 4, 90);
    // EXATO: o buffer igual (a referência llvmpipe é byte-estável)
    EXPECT_MSG(gogolden::verdict(gogolden::diffRgba(a.data(), b.data(),
                                                    a.size())) ==
                   gogolden::Verdict::EXATO,
               "buffers iguais têm de dar EXATO (0 diff)");

    // TOLERANCIA: 1 píxel a delta 10 = 0.097% e maxDelta 10 (o chip do FPS
    // — a política do V9)
    b[0] = 100;
    gogolden::DiffReport d = gogolden::diffRgba(a.data(), b.data(), a.size());
    EXPECT(d.diff == 1 && d.maxDelta == 10);
    EXPECT_MSG(gogolden::verdict(d) == gogolden::Verdict::TOLERANCIA,
               "1 píxel a delta 10 é a TOLERANCIA da casa (o V9)");

    // FALHA: 6% dos píxeis a delta 200 (a regressão SALTA)
    for (size_t i = 0; i + 3 < b.size(); i += 4) b[i] = 40;
    d = gogolden::diffRgba(a.data(), b.data(), a.size());
    EXPECT(d.pct > 6.0 && d.maxDelta == 50);
    EXPECT_MSG(gogolden::verdict(d) == gogolden::Verdict::FALHA,
               "6%% dos píxeis diferentes é FALHA (a regressão)");

    // o limiar NÃO é simétrico: diff pequeno com um canal GRANDE é FALHA
    // (o afrouxamento por um canal não passa — o céu partido seria verde)
    std::vector<unsigned char> c(1024 * 4, 90);
    c[0] = 90 + 40;   // 1 píxel (0.097%) a delta 40 > kTolMaxDelta 32
    d = gogolden::diffRgba(a.data(), c.data(), a.size());
    EXPECT(d.diff == 1 && d.maxDelta == 40 && d.pct < 0.5);
    EXPECT_MSG(gogolden::verdict(d) == gogolden::Verdict::FALHA,
               "maxDelta 40 acima do piso 32 é FALHA mesmo com 0.1%% de "
               "diff (o afrouxamento por canal não passa)");

    // o delta NO limiar (32) continua TOLERANCIA (o contrato é ≤32)
    std::vector<unsigned char> e(1024 * 4, 90);
    e[0] = 90 + 32;
    d = gogolden::diffRgba(a.data(), e.data(), a.size());
    EXPECT(d.maxDelta == 32);
    EXPECT(gogolden::verdict(d) == gogolden::Verdict::TOLERANCIA);
}

TEST(golden_mask_diff) {
    // a máscara da FALHA: igual → cinza escuro, diferente → vermelho
    std::vector<unsigned char> a(2 * 4, 90), b(2 * 4, 90);
    b[4] = 1;   // o 2.º píxel difere
    std::vector<unsigned char> m(2 * 4, 0);
    gogolden::diffMaskRgba(a.data(), b.data(), a.size(), m.data());
    EXPECT(m[0] == 24 && m[1] == 32 && m[2] == 40 && m[3] == 255);
    EXPECT(m[4] == 232 && m[5] == 64 && m[6] == 56 && m[7] == 255);
}
