// tests/test_bench.cpp — 0.9.6 (G6 · R-017): a parte PURA dos benchmarks.
//
// O CONTRATO (spec da task + R-017 em docs/REGRESSOES.md):
//   • o bloco colável tem 9 LINHAS FIXAS (parseável linha a linha);
//   • TODO número vem da MEDIÇÃO injetada — o format() lê o Report, nunca
//     escreve um número que não lhe deram (a sentinela R-017 é ISTO);
//   • o que não foi medido imprime "não medido" — e NUNCA um 0 disfarçado;
//   • a agregação de amostras (média/mínimo/1% low) é matemática pura;
//   • o GLB de referência é DETERMINÍSTICO (bytes idênticos em 2 chamadas)
//     e o round-trip da escala (2.5) chega ao importer de verdade;
//   • o pico de RSS lê-se do /proc/self/status (no Linux do CI é REAL).
#include "TestFramework.h"
#include "core/Bench.h"
#include "assets/GltfImporter.h"
#include "core/Types.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

using namespace vv;
using bench::Measured;
using ::test::nearEqF;

namespace {

// conta as linhas de um bloco (o '\n' final conta como fim da 9ª)
int lineCount(const std::string& s) {
    if (s.empty()) {
        return 0;
    }
    int n = 0;
    for (char c : s) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
}

bool hasLine(const std::string& block, const char* prefix) {
    std::istringstream in(block);
    std::string l;
    while (std::getline(in, l)) {
        if (l.rfind(prefix, 0) == 0) {
            return true;
        }
    }
    return false;
}

// o Report COMPLETAMENTE MEDIDO (valores arbitrários mas DISTINTOS — se o
// format() decidisse inventar/hardcodar um número, o teste vê a diferença)
bench::Report fullReport() {
    bench::Report r;
    std::snprintf(r.version, sizeof(r.version), "0.9.6");
    r.versionCode = 49;
    std::snprintf(r.device, sizeof(r.device), "RMX3624");
    r.sdk = 33;
    r.warmStartMs  = Measured{ true, 412.0 };
    r.coldStartMs  = Measured{ true, 1830.0 };
    r.def   = bench::FpsAgg{ true, 58.0, 51.0, 47.0 };
    r.scene = bench::FpsAgg{ true, 55.0, 48.0, 44.0 };
    r.defVerts   = Measured{ true, 12543.0 };
    r.defDc      = Measured{ true, 89.0 };
    r.sceneVerts = Measured{ true, 40120.0 };
    r.sceneDc    = Measured{ true, 156.0 };
    r.importMs    = Measured{ true, 96.0 };
    r.importScale = Measured{ true, 2.5 };
    r.texMs       = Measured{ true, 41.0 };
    std::snprintf(r.texFormat, sizeof(r.texFormat), "ASTC 4x4");
    r.audioOk = 60u;
    r.audioTotal = 60u;
    std::snprintf(r.audioBackend, sizeof(r.audioBackend), "oboe");
    r.rssPeakMb = Measured{ true, 214.0 };
    r.apkMb     = Measured{ true, 18.4 };
    r.apkShaOk  = Measured{ true, 1.0 };
    std::snprintf(r.apkSha, sizeof(r.apkSha), "44f39beda1b2…");
    r.projMb = Measured{ true, 3.2 };
    return r;
}

} // namespace

// ---- 1. o bloco tem EXATAMENTE 9 linhas e a primeira é a identidade ------
TEST(bench_bloco_de_9_linhas_parseavel) {
    const std::string b = bench::format(fullReport());
    EXPECT(lineCount(b) == 9);
    EXPECT(b.rfind("Benchmarks · G.One VV 0.9.6 (49) · RMX3624 · Android 33",
                   0) == 0);
    // as 9 linhas pelos prefixos (a ordem é PARTE do contrato — o dono cola
    // o bloco e o relatório da 0.9.6 parseia-o)
    EXPECT(hasLine(b, "warm start: 412 ms · cold start: 1830 ms"));
    EXPECT(hasLine(b, "cena default: 58 fps (min 51, 1% low 47) · "
                      "12543 verts · 89 draw calls"));
    EXPECT(hasLine(b, "cena bench (mesh importado): 55 fps (min 48, "
                      "1% low 44) · 40120 verts · 156 draw calls"));
    EXPECT(hasLine(b, "import glTF ref: 96 ms (escala 2.5)"));
    EXPECT(hasLine(b, "texturas: ASTC 4x4 (41 ms)"));
    EXPECT(hasLine(b, "áudio: 60/60 ok · oboe"));
    EXPECT(hasLine(b, "memória: pico RSS 214 MB"));
    EXPECT(hasLine(b, "APK: 18.4 MB · sha256 44f39beda1b2… · "
                      "projeto: 3.2 MB"));
}

// ---- 2. a HONESTIDADE: tudo não-medido diz "não medido" -------------------
TEST(bench_nao_medido_e_dito) {
    bench::Report r;   // zerado: NADA medido
    const std::string b = bench::format(r);
    EXPECT(lineCount(b) == 9);   // o formato é estável MESMO vazio
    EXPECT(b.rfind("Benchmarks · G.One VV dev (0) · não medido · "
                   "Android não medido", 0) == 0);
    EXPECT(hasLine(b, "warm start: não medido · cold start: não medido"));
    EXPECT(hasLine(b, "cena default: não medido · não medido verts · "
                      "não medido draw calls"));
    EXPECT(hasLine(b, "import glTF ref: não medido (escala não medido)"));
    EXPECT(hasLine(b, "texturas: não medido (não medido)"));
    EXPECT(hasLine(b, "áudio: não medido"));
    EXPECT(hasLine(b, "memória: pico RSS não medido"));
    EXPECT(hasLine(b, "APK: não medido · sha256 não medido · "
                      "projeto: não medido"));
    // a honestidade é por CAMPO: um valor medido numa linha de resto vazio
    bench::Report r2;
    r2.importMs = Measured{ true, 123.0 };
    const std::string b2 = bench::format(r2);
    EXPECT(hasLine(b2, "import glTF ref: 123 ms (escala não medido)"));
}

// ---- 3. o format() NÃO INVENTA: cada número sai do Report (R-017) --------
TEST(bench_r017_os_numeros_vem_da_medição) {
    // dois Reports com valores DIFERENTES em CADA campo → o bloco muda em
    // CADA sítio (se QUALQUER linha tivesse um número hardcodado, os dois
    // blocos seriam iguais nesse sítio)
    bench::Report a = fullReport();
    bench::Report b;
    std::snprintf(b.version, sizeof(b.version), "9.9.9");
    b.versionCode = 99;
    std::snprintf(b.device, sizeof(b.device), "OUTRO");
    b.sdk = 34;
    b.warmStartMs  = Measured{ true, 1.0 };
    b.coldStartMs  = Measured{ true, 2.0 };
    b.def   = bench::FpsAgg{ true, 3.0, 4.0, 5.0 };
    b.scene = bench::FpsAgg{ true, 6.0, 7.0, 8.0 };
    b.defVerts   = Measured{ true, 9.0 };
    b.defDc      = Measured{ true, 10.0 };
    b.sceneVerts = Measured{ true, 11.0 };
    b.sceneDc    = Measured{ true, 12.0 };
    b.importMs    = Measured{ true, 13.0 };
    b.importScale = Measured{ true, 14.0 };
    b.texMs       = Measured{ true, 15.0 };
    std::snprintf(b.texFormat, sizeof(b.texFormat), "ETC2 RGB");
    b.audioOk = 16u;
    b.audioTotal = 17u;
    std::snprintf(b.audioBackend, sizeof(b.audioBackend), "AudioTrack");
    b.rssPeakMb = Measured{ true, 18.0 };
    b.apkMb     = Measured{ true, 19.0 };
    b.apkShaOk  = Measured{ true, 1.0 };
    std::snprintf(b.apkSha, sizeof(b.apkSha), "ffffffffffff…");
    b.projMb = Measured{ true, 20.0 };

    const std::string ta = bench::format(a);
    const std::string tb = bench::format(b);
    EXPECT(ta != tb);
    // linha a linha: NENHUMA linha é igual entre os dois relatórios (cada
    // campo medido mudou — a diferença tem de aparecer nas 9)
    std::istringstream ia(ta), ib(tb);
    std::string la, lb;
    int diffs = 0;
    while (std::getline(ia, la) && std::getline(ib, lb)) {
        if (la != lb) {
            ++diffs;
        }
    }
    EXPECT(diffs == 9);
    // e os valores de B estão LITERALMENTE no bloco de B (amostragem direta)
    const std::string tb2 = tb;
    EXPECT(tb2.find("1 ms") != std::string::npos);
    EXPECT(tb2.find("3 fps (min 4, 1% low 5)") != std::string::npos);
    EXPECT(tb2.find("(escala 14.0)") != std::string::npos);
    EXPECT(tb2.find("ETC2 RGB (15 ms)") != std::string::npos);
    EXPECT(tb2.find("16/17 ok · AudioTrack") != std::string::npos);
    EXPECT(tb2.find("pico RSS 18 MB") != std::string::npos);
}

// ---- 4. a agregação (média/mín/1% low) é matemática pura ------------------
TEST(bench_agregação_amostras) {
    // amostras uniformes: tudo == 60
    std::vector<double> u(600, 60.0);
    bench::FpsAgg a = bench::aggregate(u);
    EXPECT(a.ok);
    EXPECT(nearEqF(static_cast<f32>(a.avg), 60.0f, 0.01f));
    EXPECT(nearEqF(static_cast<f32>(a.min), 60.0f, 0.01f));
    EXPECT(nearEqF(static_cast<f32>(a.p1), 60.0f, 0.01f));

    // amostra com UMA quebra (um spike de frame time = fps 30): o min
    // apanha-o; o 1% low de 600 amostras NÃO se deixa arrastar por 1
    std::vector<double> m(600, 60.0);
    m[100] = 30.0;
    bench::FpsAgg b = bench::aggregate(m);
    EXPECT(b.ok);
    EXPECT(nearEqF(static_cast<f32>(b.min), 30.0f, 0.01f));
    EXPECT(b.avg > 59.9);
    EXPECT(b.p1 >= 59.0);   // 1 amostra em 600 = 0.17% — o 1% low resiste

    // 5% das amostras a 20 fps: o 1% low AGORA cai (está dentro do pior 1%)
    std::vector<double> w(600, 60.0);
    for (int i = 0; i < 30; ++i) {
        w[i] = 20.0;
    }
    bench::FpsAgg c = bench::aggregate(w);
    EXPECT(c.ok);
    EXPECT(nearEqF(static_cast<f32>(c.p1), 20.0f, 0.01f));

    // poucas amostras NÃO é medição (ok=false — nunca uma média de 3 frames)
    std::vector<double> few(5, 60.0);
    EXPECT(!bench::aggregate(few).ok);
    // amostras inválidas são ignoradas (o pause do runner não conta)
    std::vector<double> inv(20, 0.0);
    EXPECT(!bench::aggregate(inv).ok);
    std::vector<double> mix(20, 60.0);
    mix[5] = 0.0;   // um dt de 0 = frame pausado
    bench::FpsAgg d = bench::aggregate(mix);
    EXPECT(d.ok);
    EXPECT(nearEqF(static_cast<f32>(d.avg), 60.0f, 0.01f));
}

// ---- 5. o pico de RSS é REAL no Linux do CI -------------------------------
TEST(bench_rss_real_no_host) {
    const u64 kb = bench::readPeakRssKb();
    EXPECT(kb > 1024);   // >1 MB — o próprio processo de teste ocupa mais
    EXPECT(kb < 64ull * 1024 * 1024);   // <64 GB — sanity do parse
}

// ---- 6. o GLB de referência é determinístico e válido --------------------
TEST(bench_glb_referência_determinístico) {
    std::vector<u8> a, b;
    bench::makeReferenceGlb(a);
    bench::makeReferenceGlb(b);
    EXPECT(a.size() == b.size());
    EXPECT(std::memcmp(a.data(), b.data(), a.size()) == 0);   // byte a byte

    // é um GLB de verdade (magic 'glTF' + versão 2 + tamanho coerente)
    ASSERT(a.size() > 20);
    EXPECT(a[0] == 'g' && a[1] == 'l' && a[2] == 'T' && a[3] == 'F');
    const u32 ver = static_cast<u32>(a[4]) | (static_cast<u32>(a[5]) << 8) |
                    (static_cast<u32>(a[6]) << 16) |
                    (static_cast<u32>(a[7]) << 24);
    EXPECT(ver == 2u);
    const u32 total = static_cast<u32>(a[8]) | (static_cast<u32>(a[9]) << 8) |
                      (static_cast<u32>(a[10]) << 16) |
                      (static_cast<u32>(a[11]) << 24);
    EXPECT(total == a.size());
}

// ---- 7. o round-trip da ESCALA pelo importer REAL -------------------------
TEST(bench_glb_escala_chega_ao_importer) {
    std::vector<u8> glb;
    bench::makeReferenceGlb(glb);
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), GltfBufferResolver{nullptr, 0},
                    model, err));
    // o nó do benchref tem scale 2.5 — é o que o relatório afere (a
    // "escala" da linha 5: sai do GLB, passa o importer, chega ao TIC)
    ASSERT(model.nodes.size() >= 1);
    EXPECT(nearEqF(model.nodes[0].scale.x, 2.5f, 0.001f));
    EXPECT(nearEqF(model.nodes[0].scale.y, 2.5f, 0.001f));
    EXPECT(nearEqF(model.nodes[0].scale.z, 2.5f, 0.001f));
    EXPECT(model.nodes[0].mesh == 0);   // o nó aponta o mesh da grelha
    // o mesh tem a grelha 16×16 (256 verts / 450 índices — carga real para
    // o import ter substância sem ser enorme)
    ASSERT(model.meshes.size() >= 1);
    EXPECT(model.meshes[0].vertices.size() == 256);
    EXPECT(model.meshes[0].indices.size() == 1350);
}

// ---- 8. as marcas de arranque (cold/warm) medem TEMPOS REAIS -------------
TEST(bench_marcas_de_arranque) {
    // a suíte PARTILHA o processo: os testes de handshake/lifecycle chamam
    // o nativeRegisterActivity com origens REAIS e as marcas já foram tocadas
    // — o reset é o precedente resetForTest do VoniRegistry
    bench::resetMarksForTest();
    // agora sim: sem marcas, as leituras são "não medido" (honestidade)
    EXPECT(!bench::latestColdMs().ok);
    EXPECT(!bench::latestWarmMs().ok);

    // a sequência real: create → 1ª apresentação (cold medido; warm ainda
    // não — nunca houve resume)
    bench::markCreate();
    EXPECT(!bench::latestColdMs().ok);   // sem 1ª apresentação: nada
    bench::markFirstFrame();
    const Measured cold = bench::latestColdMs();
    EXPECT(cold.ok);
    EXPECT(cold.value >= 0.0);   // micro-tempos são aceitáveis; nunca <0
    // o warm exige um resume DEPOIS da 1ª apresentação E um frame seguinte
    EXPECT(!bench::latestWarmMs().ok);
    bench::markResume();
    EXPECT(!bench::latestWarmMs().ok);   // resume sem o frame SEGUINTE
    bench::markFirstFrame();             // o 1º swap após o resume
    const Measured warm = bench::latestWarmMs();
    EXPECT(warm.ok);
    EXPECT(warm.value >= 0.0);
    // um SEGUNDO resume reinicia a espera (o warm é SEMPRE o último)
    bench::markResume();
    EXPECT(!bench::latestWarmMs().ok);
}
