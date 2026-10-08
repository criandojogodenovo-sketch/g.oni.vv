// tests/test_wiring010.cpp — 0.8.10: FORMATOS PRÓPRIOS + 500 MB + ARCHIVES.
//
// O TU do "TESTAR MESMO": cada caso de MEDIÇÃO imprime o OUTPUT REAL que
// o relatório cola (timings, RSS de pico, rácios de compressão, contadores
// de stress) — afirmar sem output = relatório rejeitado.
//
//   1. FORMATOS: .gmesh/.gtext/.gm round-trip com rácio impresso; checksum
//      corrompido / magic trocado / versão futura → ERRO LEGÍVEL (nunca
//      crash, nunca lixo);
//   2. 500 MB: fixture OBJ REAL de ~500 MB gerada em disco; o import
//      STREAMING em PROCESSO FILHO com RSS de pico medido (getrusage) e
//      orçamento; a PROVA RED: o readAll antigo no MESMO ficheiro em outro
//      filho estoura o orçamento (o crash de 500 MB morto);
//   3. ARCHIVES: ZIP construído em código (stored + deflate) extrai CRU
//      para extracted/ (ZERO conversão); zip-slip rejeitado; bomb-guard
//      erro legível; cancelamento sem estado parcial; RAR erro legível;
//   4. MIGRAÇÃO: projeto antigo meshes/x.obj → assets/x.gmesh + ref do TIC
//      reescrita pelo postLoadMigrateAndFixup (o caminho do device);
//   5. IDENTIDADE: buildinfo nos crash dumps (nome+header) + badge ANTIGO;
//   6. SETTING fonte: largar source/ pós-import + reconvert traz de volta.
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub
#include <EGL/egl.h>
#include <dirent.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <jni.h>

#include "FakeStorage.h"
#include "core/FsStorage.h"
#include "assets/AssetConverter.h"
#include "assets/GOwnFormats.h"
#include "assets/ZipExtract.h"
#include "core/Scene.h"
#include "platform/BuildInfo.h"
#include "platform/CrashHandler.h"
#include "platform/FileApi.h"
#include "platform/EngineLog.h"
#include "render/Primitives.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"

// 0.8.10: os casos com o CAMINHO DO DEVICE (migração e2e, setting fonte —
// postLoadMigrateAndFixup/browserImportFile/ImportJob) vivem no
// test_wiring087.cpp, o ÚNICO TU que inclui platform/main.cpp (um só
// android_main no link); aqui ficam os casos PUROS dos formatos/arquivos.
using namespace vv;
using ::test::nearEqF;

namespace {

const char* kTestLogs = "test-wiring010-logs";

void rmrf(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n != "." && n != "..") {
                ::remove((dir + "/" + n).c_str());
            }
        }
        ::closedir(d);
    }
    ::remove(dir.c_str());
}

bool logHas(const char* needle) {
    std::vector<std::string> lines;
    vv::elog::readTail(lines, 900);
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

jobject kFakeActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xC001));
jclass  kFakeCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xC002));

double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0)
        .count();
}

// ---- construtor de ZIP em código (stored + deflate) -------------------------
struct ZipEntry {
    std::string name;
    std::string data;      // conteúdo CRU (o builder comprime se deflate)
    bool deflate = false;
};

u32 crc32Of(const std::string& s) {
    return static_cast<u32>(
        ::crc32(0, reinterpret_cast<const Bytef*>(s.data()),
                static_cast<uInt>(s.size())));
}

void put16(std::vector<u8>& b, u16 v) {
    b.push_back(static_cast<u8>(v & 0xFF));
    b.push_back(static_cast<u8>(v >> 8));
}
void put32(std::vector<u8>& b, u32 v) {
    put16(b, static_cast<u16>(v & 0xFFFF));
    put16(b, static_cast<u16>(v >> 16));
}

// escreve um .zip VÁLIDO (local headers + central directory + EOCD)
std::vector<u8> buildZip(const std::vector<ZipEntry>& entries) {
    std::vector<u8> zip;
    struct Cd {
        std::string name;
        u32 crc = 0, comp = 0, uncomp = 0;
        u32 lfhOff = 0;
        u16 method = 0;
    };
    std::vector<Cd> cds;
    for (const ZipEntry& e : entries) {
        Cd cd;
        cd.name = e.name;
        cd.crc = crc32Of(e.data);
        cd.uncomp = static_cast<u32>(e.data.size());
        cd.lfhOff = static_cast<u32>(zip.size());
        std::string payload = e.data;
        cd.method = e.deflate ? 8 : 0;
        if (e.deflate) {
            // raw deflate via zlib (windowBits negativos)
            uLongf compSz = compressBound(static_cast<uLong>(e.data.size())) + 64;
            std::vector<Bytef> comp(compSz);
            // deflateInit2 com -15: um byte a mais de folga no bound
            z_stream zs{};
            deflateInit2(&zs, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
            zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(e.data.data()));
            zs.avail_in = static_cast<uInt>(e.data.size());
            zs.next_out = comp.data();
            zs.avail_out = static_cast<uInt>(compSz);
            deflate(&zs, Z_FINISH);
            deflateEnd(&zs);
            compSz = zs.total_out;
            payload.assign(reinterpret_cast<char*>(comp.data()), compSz);
        }
        cd.comp = static_cast<u32>(payload.size());
        // local file header
        put32(zip, 0x04034b50u);
        put16(zip, 0);            // version
        put16(zip, 0);            // flags
        put16(zip, cd.method);
        put16(zip, 0); put16(zip, 0);   // time/date
        put32(zip, cd.crc);
        put32(zip, cd.comp);
        put32(zip, cd.uncomp);
        put16(zip, static_cast<u16>(e.name.size()));
        put16(zip, 0);            // extra len
        zip.insert(zip.end(), e.name.begin(), e.name.end());
        zip.insert(zip.end(), payload.begin(), payload.end());
        cds.push_back(cd);
    }
    // central directory
    const u32 cdOff = static_cast<u32>(zip.size());
    for (const Cd& cd : cds) {
        put32(zip, 0x02014b50u);
        put16(zip, 0); put16(zip, 0);   // version made/need
        put16(zip, 0);                  // flags
        put16(zip, cd.method);
        put16(zip, 0); put16(zip, 0);
        put32(zip, cd.crc);
        put32(zip, cd.comp);
        put32(zip, cd.uncomp);
        put16(zip, static_cast<u16>(cd.name.size()));
        put16(zip, 0); put16(zip, 0);   // extra/comment
        put16(zip, 0); put16(zip, 0);   // disk/internal attrs
        put32(zip, 0);                  // external attrs
        put32(zip, cd.lfhOff);
        zip.insert(zip.end(), cd.name.begin(), cd.name.end());
    }
    // EOCD
    put32(zip, 0x06054b50u);
    put16(zip, 0); put16(zip, 0);
    put16(zip, static_cast<u16>(cds.size()));
    put16(zip, static_cast<u16>(cds.size()));
    put32(zip, static_cast<u32>(zip.size() - cdOff));
    put32(zip, cdOff);
    put16(zip, 0);
    return zip;
}

bool writeBytesFile(const std::string& path, const std::vector<u8>& b) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    const bool ok = b.empty() || std::fwrite(b.data(), 1, b.size(), f) == b.size();
    std::fclose(f);
    return ok;
}

} // namespace

// ---------------------------------------------------------------------------
// 1. FORMATOS PRÓPRIOS — round-trips com RÁCIO REAL impresso
// ---------------------------------------------------------------------------
TEST(wiring010_gmesh_roundtrip_e_ratio) {
    // OBJ denso (esfera 16×12 = 289 verts / 960 idx + grupo)
    PrimMeshData pm;
    makePrimMesh(primDefaults(PrimKind::Sphere), pm);
    MeshData m;
    m.name = "esfera";
    m.vertices = pm.vertices;
    m.indices = pm.indices;
    MeshData::Group g{"corpo", "", 0, static_cast<u32>(pm.indices.size())};
    m.groups.push_back(g);

    const size_t objBytes = [&]() {   // o equivalente OBJ textual
        std::string o = "o esfera\n";
        char ln[96];
        for (const Vertex& v : m.vertices) {
            std::snprintf(ln, sizeof(ln), "v %.6f %.6f %.6f\n", v.pos.x,
                          v.pos.y, v.pos.z);
            o += ln;
        }
        for (u32 i = 0; i < 3; ++i) {
            std::snprintf(ln, sizeof(ln), "f %u//%u\n", i + 1, i + 1);
            o += ln;
        }
        return o.size();
    }();

    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));
    MeshData back;
    ASSERT(readGMesh(out.data(), out.size(), back, err));

    EXPECT(back.vertices.size() == m.vertices.size());
    EXPECT(back.indices.size() == m.indices.size());
    EXPECT(back.groups.size() == 1);
    // quantização 16-bit: posições a ~1e-4 relativo
    bool near = true;
    for (size_t i = 0; i < m.vertices.size(); ++i) {
        if (!::test::vecNearF(back.vertices[i].pos, m.vertices[i].pos, 1e-3f)) {
            near = false;
        }
    }
    EXPECT(near);
    // normais ~unitárias
    bool unit = true;
    for (const Vertex& v : back.vertices) {
        const f32 n = std::sqrt(v.normal.x * v.normal.x + v.normal.y * v.normal.y +
                                v.normal.z * v.normal.z);
        if (n < 0.98f || n > 1.02f) {
            unit = false;
        }
    }
    EXPECT(unit);

    // OUTPUT REAL: o rácio (o que o relatório cola). 0.10-M RECALIBRADO:
    // desde o PASSO 2 o escritor só escreve v3 (float32 SEM PERDA) — em
    // meshes PEQUENAS o float32 é maior que o texto do OBJ (o rácio <1
    // era a propriedade da QUANTIZAÇÃO 16-bit do v1; a fixture v1 real
    // commitada continua a prová-lo — test_gmeshv3). Em meshes grandes o
    // float32 volta a ganhar ao texto (sem o custo de 6 f32 por linha).
    std::printf("  [gmesh] esfera %zu verts %zu idx — OBJ-texto %zu B → "
                ".gmesh v3 %zu B (%.2fx; a v1 quantizada da fixture real: "
                "5930 B)\n",
                m.vertices.size(), m.indices.size(), objBytes, out.size(),
                static_cast<double>(objBytes) / static_cast<double>(out.size()));
}

TEST(wiring010_gtext_e_ganm_roundtrip) {
    // .gtext: passthrough RGBA de 64×48
    RawImage img;
    img.width = 64;
    img.height = 48;
    img.rgba.assign(static_cast<size_t>(64) * 48 * 4, 0x7F);
    PassthroughCompressor comp;
    CompressedImage ci;
    std::string cerr;
    ASSERT(comp.compress(img, ci, cerr));
    std::vector<u8> gtext;
    ASSERT(writeGText(ci, gtext, cerr));
    CompressedImage back;
    ASSERT(readGText(gtext.data(), gtext.size(), back, cerr));
    EXPECT(back.width == 64 && back.height == 48);
    EXPECT(back.format == ci.format);
    EXPECT(back.data.size() == ci.data.size());
    std::printf("  [gtext] 64x48 RGBA8 — raw %zu B → .gtext %zu B (mips=%zu)\n",
                img.rgba.size(), gtext.size(), back.mips.size());

    // .gm: clip com 2 tracks + esqueleto de 3 joints
    GAnimFile anim;
    AnimClip clip;
    clip.name = "girar";
    AnimTrack tr;
    tr.target = AnimTarget::TicPos;
    AnimKey k0;
    k0.t = 0.0f;
    k0.v[0] = 0.0f;
    AnimKey k1;
    k1.t = 2.0f;
    k1.v[0] = 4.0f;
    tr.keys = {k0, k1};
    clip.tracks.push_back(tr);
    anim.clips.push_back(clip);
    for (int j = 0; j < 3; ++j) {
        SkeletonComp::Joint joint;
        joint.name = "joint" + std::to_string(j);
        joint.parent = j - 1;
        joint.pos = Vec3{static_cast<f32>(j), 0.0f, 0.0f};
        anim.joints.push_back(joint);
    }
    std::vector<u8> gm;
    ASSERT(writeGAnim(anim, gm, cerr));
    GAnimFile aback;
    ASSERT(readGAnim(gm.data(), gm.size(), aback, cerr));
    EXPECT(aback.clips.size() == 1);
    EXPECT(aback.clips[0].tracks.size() == 1);
    EXPECT(aback.clips[0].tracks[0].keys.size() == 2);
    EXPECT(aback.joints.size() == 3);
    EXPECT(aback.joints[2].parent == 1);
    EXPECT(nearEqF(aback.clips[0].tracks[0].keys[1].v[0], 4.0f));
    std::printf("  [gm] 1 clip/1 track/2 keys + 3 joints — %zu B\n", gm.size());
}

TEST(wiring010_checksum_corrompido_erro_legivel) {
    PrimMeshData pm;
    makePrimMesh(primDefaults(PrimKind::Box), pm);
    MeshData m;
    m.vertices = pm.vertices;
    m.indices = pm.indices;
    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));

    // corrompe UM byte do MEIO do payload → o checksum apanha
    out[out.size() / 2] ^= 0xFF;
    MeshData back;
    EXPECT(!readGMesh(out.data(), out.size(), back, err));
    EXPECT(err.find("CHECKSUM CORROMPIDO") != std::string::npos);
    std::printf("  [corrupcao] err='%s'\n", err.c_str());
}

TEST(warning010_magic_versao_endian_rejeitados) {
    PrimMeshData pm;
    makePrimMesh(primDefaults(PrimKind::Box), pm);
    MeshData m;
    m.vertices = pm.vertices;
    m.indices = pm.indices;
    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));

    // magic trocado (é um .gtext?) → rejeita
    MeshData back;
    EXPECT(!gReadHeader(out.data(), out.size(), "GVTX",
                        *reinterpret_cast<GFileHeader*>(out.data()), err));
    EXPECT(err.find("magic errado") != std::string::npos);

    // versão futura (0.10-M RECALIBRADO: a 2 ABRE — a retrocompatibilidade
    // que o dono mandou; a recusa é de versões ACIMA da 3 — o R-038 prova
    // a v1 e a v2 a abrir com as fixtures reais)
    std::vector<u8> v9 = out;
    v9[4] = 9;   // version = 9
    EXPECT(!readGMesh(v9.data(), v9.size(), back, err));
    EXPECT(err.find("desconhecida") != std::string::npos);

    // endianess trocada (a marca lida com os bytes trocados: 0x3412 ≠ 0x1A2B)
    std::vector<u8> be = out;
    be[6] = 0x34;
    be[7] = 0x12;
    EXPECT(!readGMesh(be.data(), be.size(), back, err));
    EXPECT(err.find("endianess") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 2. 500 MB — a fixture REAL, o import STREAMING e a PROVA RED
// ---------------------------------------------------------------------------
// gera a fixture: ~500 MB de OBJ com comentários pesados + geometria pequena
// válida (o caso real: ficheiros gigantes cujo CONTEÚDO útil é limitado
// pelo u16 do engine; o que crashava era o READALL, não o parse)
static bool geraFixtureObj(const std::string& path, u64 targetBytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    const char* comment =
        "# linha de ruido do exportador: metadata metadata metadata metadata "
        "metadata metadata metadata metadata 0123456789 ABCDEFGHIJ\n";
    const size_t clen = std::strlen(comment);
    std::string block;
    block.reserve(clen * 4096);
    for (int i = 0; i < 4096; ++i) {
        block += comment;
    }
    u64 written = 0;
    while (written + block.size() < targetBytes) {
        if (std::fwrite(block.data(), 1, block.size(), f) != block.size()) {
            std::fclose(f);
            return false;
        }
        written += block.size();
    }
    const char* geo = "o util\nv 0 0 0\nv 2 0 0\nv 0 1 0\nf 1 2 3\n";
    std::fwrite(geo, 1, std::strlen(geo), f);
    std::fclose(f);
    return true;
}

TEST(wiring010_import_500mb_streaming_rss_e_tempo_reais) {
    const std::string fixture = "goni_w010_500mb.obj";
    const u64 target = 500ull * 1024 * 1024;
    const auto tGen = std::chrono::steady_clock::now();
    ASSERT(geraFixtureObj(fixture, target));
    const double genMs = msSince(tGen);

    // ---- filho GREEN: o import STREAMING (nunca inteiro em RAM) ----------
    // (processo filho para o ru_maxrss nascer LIMPO — a medição é honesta)
    const pid_t pid = ::fork();
    ASSERT(pid >= 0);
    if (pid == 0) {
        // filho: FsStorage REAL em pasta temporária (o write stream de FILE*
        // é o caminho de streaming do device) + RSS via getrusage
        char root[64];
        std::snprintf(root, sizeof(root), "/tmp/goni_w010_%d", (int)::getpid());
        FsStorage st(root);
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const auto t0 = std::chrono::steady_clock::now();
        const bool ok = convert::importFile(fixture, fixture, st, nullptr,
                                            out, stats, err, nullptr, nullptr);
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0)
                              .count();
        struct rusage ru;
        ::getrusage(RUSAGE_SELF, &ru);
        const long rssKB = ru.ru_maxrss;
        std::printf("  [500MB-green] ok=%d err=%s | fonte %llu B → saida "
                    "%llu B (ratio %.1fx) | %.0f ms | RSS pico %ld MB\n",
                    ok ? 1 : 0, err.c_str(),
                    static_cast<unsigned long long>(stats.sourceBytes),
                    static_cast<unsigned long long>(stats.outputBytes),
                    stats.outputBytes > 0
                        ? static_cast<double>(stats.sourceBytes) /
                              static_cast<double>(stats.outputBytes)
                        : 0.0,
                    ms, rssKB / 1024);
        std::fflush(stdout);   // o _exit NÃO flusheia — o output é a prova
        // 0.10-M: a LIMPEZA do root do filho (a lição /tmp: 500 MB × N
        // corridas enchiam o disco — o import anterior morria de disco cheio)
        rmrf(root);
        // VEREDITO do filho: sucesso + orçamento de RAM + saída menor
        ::_exit(ok && stats.outputBytes < stats.sourceBytes &&
                rssKB / 1024 < 300   // orçamento: JAMAIS ~500 MB do readAll
                    ? 0 : 10);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    // ---- filho RED: o CAMINHO ANTIGO (readAll inteiro) estoura ----------
    // (a prova de que o crash de 500 MB era o readAll — e morreu)
    const pid_t pidRed = ::fork();
    ASSERT(pidRed >= 0);
    if (pidRed == 0) {
        std::vector<u8> bytes;
        const bool ok = fileapi::readAll(fixture.c_str(), bytes);
        struct rusage ru;
        ::getrusage(RUSAGE_SELF, &ru);
        std::printf("  [500MB-red] readAll ok=%d — RSS pico %ld MB (o "
                    "caminho antigo: o crash do dono)\n",
                    ok ? 1 : 0, ru.ru_maxrss / 1024);
        std::fflush(stdout);
        ::_exit(ru.ru_maxrss / 1024 > 400 ? 42 : 0);   // 42 = estourou (prova)
    }
    ::waitpid(pidRed, &status, 0);
    // o RED ESTOURA por definição (500 MB em RAM) — é a PROVA, não falha
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 42);

    std::printf("  [500MB] fixture gerada em %.0f ms; import verde + prova "
                "red acima (output colado no relatorio)\n", genMs);
    ::remove(fixture.c_str());
}

// ---------------------------------------------------------------------------
// 3. ARCHIVES — extrai CRU, zip-slip, bomb-guard, cancel, RAR
// ---------------------------------------------------------------------------
TEST(wiring010_zip_extrai_crus_sem_converter) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    const std::string zipPath = "goni_w010_pack.zip";
    std::vector<ZipEntry> entries = {
        {"modelo.obj", "o tri\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", false},
        {"textura.png", std::string(256, '\x89'), true},   // deflate (binário)
        {"som.wav", std::string(128, 'W'), false},
        {"pasta/sub.txt", "sub\n", true},                   // estrutura preservada
    };
    ASSERT(writeBytesFile(zipPath, buildZip(entries)));

    FakeStorage st;
    zip::ExtractStats stats;
    std::string err;
    const bool ok = zip::extractArchive(zipPath, st, "extracted/pack", stats,
                                        err, nullptr, nullptr);
    EXPECT(ok);
    EXPECT(stats.files == 4);
    EXPECT(stats.rejected == 0);
    // CRUS e no sítio: bytes IGUAIS aos de origem (stored e deflate)
    std::vector<u8> got;
    ASSERT(st.readBytes("extracted/pack/modelo.obj", got));
    EXPECT(got.size() == entries[0].data.size());
    EXPECT(std::memcmp(got.data(), entries[0].data.data(), got.size()) == 0);
    ASSERT(st.readBytes("extracted/pack/textura.png", got));
    EXPECT(got.size() == 256 && got[0] == 0x89);
    ASSERT(st.readBytes("extracted/pack/pasta/sub.txt", got));
    EXPECT(got.size() == 4);
    // ZERO conversão: nada em assets/, nada em source/
    EXPECT(!st.exists("assets/modelo.gmesh"));
    {   // assets/ NEM EXISTE (zero conversão no passo de extrair)
        std::vector<std::string> lst;
        EXPECT(!st.listDir("assets", lst));
    }
    std::printf("  [zip] %u entradas → %u ficheiros crus em extracted/pack/ "
                "(%llu B de %llu B de archive; ZERO conversao)\n",
                stats.entries, stats.files,
                static_cast<unsigned long long>(stats.totalOut),
                static_cast<unsigned long long>(stats.archiveBytes));
    EXPECT(logHas("archive: extraido"));
    ::remove(zipPath.c_str());
}

TEST(wiring010_zip_slip_bomb_e_cancelamento) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));

    // ---- zip-slip: entradas maliciosas REJEITADAS, nada escrito fora ----
    {
        const std::string zipPath = "goni_w010_evil.zip";
        std::vector<ZipEntry> entries = {
            {"../escape.txt", "fora\n", false},
            {"/absoluto.txt", "fora2\n", false},
            {"ok.txt", "dentro\n", false},
        };
        ASSERT(writeBytesFile(zipPath, buildZip(entries)));
        FakeStorage st;
        zip::ExtractStats stats;
        std::string err;
        EXPECT(zip::extractArchive(zipPath, st, "extracted/evil", stats, err,
                                   nullptr, nullptr));
        EXPECT(stats.rejected == 2);   // ../ e /absoluto
        EXPECT(stats.files == 1);
        EXPECT(st.exists("extracted/evil/ok.txt"));
        EXPECT(!st.exists("extracted/escape.txt"));   // NUNCA saiu do destino
        EXPECT(logHas("archive: zip-slip rejeitado ../escape.txt"));
        ::remove(zipPath.c_str());
    }

    // ---- bomb-guard: entrada que declara 3 GB → erro LEGÍVEL -------------
    {
        const std::string zipPath = "goni_w010_bomb.zip";
        std::vector<ZipEntry> entries = {{"bomba.bin", "x", false}};
        std::vector<u8> zip = buildZip(entries);
        // reescreve o uncompSize do central directory para 3 GB (declaração
        // mentirosa — o guard dispara ANTES de escrever)
        const u32 cdOff = [&]() {
            for (size_t i = zip.size() - 22; i + 4 <= zip.size(); --i) {
                if (zip[i] == 0x50 && zip[i + 1] == 0x4B && zip[i + 2] == 0x05 &&
                    zip[i + 3] == 0x06) {
                    return static_cast<u32>(zip[i + 16] | (zip[i + 17] << 8) |
                                            (zip[i + 18] << 16) |
                                            (zip[i + 19] << 24));
                }
            }
            return 0u;
        }();
        // uncompSize no CDE: offset cdOff+24
        const u32 big = 3ull * 1024 * 1024 * 1024;
        zip[cdOff + 24] = static_cast<u8>(big & 0xFF);
        zip[cdOff + 25] = static_cast<u8>((big >> 8) & 0xFF);
        zip[cdOff + 26] = static_cast<u8>((big >> 16) & 0xFF);
        zip[cdOff + 27] = static_cast<u8>((big >> 24) & 0xFF);
        ASSERT(writeBytesFile(zipPath, zip));
        FakeStorage st;
        zip::ExtractStats stats;
        std::string err;
        EXPECT(!zip::extractArchive(zipPath, st, "extracted/bomb", stats, err,
                                    nullptr, nullptr));
        EXPECT(err.find("bomb-guard") != std::string::npos);
        std::printf("  [bomb-guard] err='%s'\n", err.c_str());
        ::remove(zipPath.c_str());
    }

    // ---- cancelamento a meio: SEM estado parcial -------------------------
    {
        const std::string zipPath = "goni_w010_big.zip";
        std::vector<ZipEntry> entries = {{"grande.bin", std::string(8 * 1024 * 1024, 'Z'),
                                          false}};
        ASSERT(writeBytesFile(zipPath, buildZip(entries)));
        FakeStorage st;
        zip::ExtractStats stats;
        std::string err;
        // cancela logo no 1º chunk de progresso
        auto cancelar = [](void*, u64, u64) -> bool { return false; };
        EXPECT(!zip::extractArchive(zipPath, st, "extracted/big", stats, err,
                                    cancelar, nullptr));
        EXPECT(stats.canceled);
        // o ficheiro PARCIAL foi REMOVIDO (sem estado parcial)
        EXPECT(!st.exists("extracted/big/grande.bin"));
        EXPECT(logHas("sem estado parcial"));
        ::remove(zipPath.c_str());
    }
}

// ---------------------------------------------------------------------------
// 5. IDENTIDADE — crash dumps com build/versionCode + badge ANTIGO
// ---------------------------------------------------------------------------
TEST(wiring010_crash_dump_identidade_e_badge) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    // a JNI "chegou" com a identidade (o que a VvActivity faz no onCreate)
    vv::buildinfo::set("0.8.10-teste", 40, "abcd1234", "deadbeef...", 1790000000ull);
    EXPECT(std::strcmp(vv::buildinfo::g_version, "0.8.10-teste") == 0);
    EXPECT(vv::buildinfo::g_versionCode == 40);

    // dump de teste com a identidade no NOME e no HEADER
    const std::string dumpPath =
        std::string(kTestLogs) + "/crash-1790000000" +
        vv::buildinfo::dumpSuffix() + ".dump";
    void* pcs[1] = {reinterpret_cast<void*>(0x1234)};
    const int rc = vv::crash::writeDumpFromFrames(dumpPath.c_str(), "SIGSEGV",
                                                  11, nullptr, pcs, 1);
    EXPECT(rc == 0);
    FILE* f = std::fopen(dumpPath.c_str(), "r");
    ASSERT(f != nullptr);
    char buf[1024] = {0};
    std::fread(buf, 1, sizeof(buf) - 1, f);
    std::fclose(f);
    EXPECT(std::strstr(buf, "build: 0.8.10-teste (versionCode 40)") != nullptr);
    EXPECT(std::strstr(buf, "git: abcd1234") != nullptr);
    EXPECT(std::strstr(buf, "epoch: 1790000000") != nullptr);
    std::printf("  [identidade] dump '%s' com build/versionCode/git/epoch no "
                "header\n", dumpPath.c_str());

    // badge ANTIGO: dump de OUTRA build (vc 39) e dump PRÉ-0.8.10
    EXPECT(vv::buildinfo::dumpIsFromOtherBuild("crash-1780000000-vc39.dump"));
    EXPECT(vv::buildinfo::dumpIsFromOtherBuild("crash-1780000000.dump"));
    EXPECT(!vv::buildinfo::dumpIsFromOtherBuild("crash-1790000000-vc40.dump"));

    // banner de boot (a 1ª linha do log viewer) — 0.8.12: formato exigido
    // "boot: G.One VV <versão> versionCode <N> sha256 <…>" (o sha256 REAL
    // da .so; git quando existe)
    const std::string banner = vv::buildinfo::banner();
    EXPECT(banner.find("G.One VV 0.8.10-teste") != std::string::npos);
    EXPECT(banner.find("versionCode 40") != std::string::npos);
    EXPECT(banner.find("sha256 deadbeef...") != std::string::npos);
    EXPECT(banner.find("git abcd1234") != std::string::npos);
    std::printf("  [banner] %s\n", banner.c_str());

    // 0.8.12 — BADGE do viewer: dump de OUTRA build → "[ANTIGO (build N)]";
    // dump PRÉ-0.8.10 (sem -vc) → "[ANTIGO (pre-0.8.10)]"; da MESMA → ""
    EXPECT(vv::buildinfo::dumpBadge("crash-1780000000-vc39.dump") ==
           "  [ANTIGO (build 39)]");
    EXPECT(vv::buildinfo::dumpBadge("crash-1780000000.dump") ==
           "  [ANTIGO (pre-0.8.10)]");
    EXPECT(vv::buildinfo::dumpBadge("crash-1790000000-vc40.dump").empty());

    // reset para não vazar para os outros casos
    vv::buildinfo::set("dev", 0, "", "", 0);
}

