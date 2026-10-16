// tests/test_fase375b_impostors.cpp — 0.11-W (3.75b): IMPOSTORES — o PNG
// 3D por distância (o 3.º sistema da campanha 0.11-W).
//
// O problema: com o instancing (3.5b) o dc dos 6 clones desce para ~863
// (1 por bloco) — para mundos GRANDES (N prédios) o dc ainda cresce com
// N×blocos. A cura (a spec do arquiteto, §3.75b): o TIC além da distância
// dele desenha UM quad com o PNG alfa da silhueta (o impostor) — o dc do
// TIC longe é 1, sempre.
//
//   1. fase375b_png_alpha — o renderMeshAlpha: fundo TRANSPARENTE,
//      geometria opaca com o lambert cru, determinístico; o PNG RGBA
//      (color type 6) round-tripa byte a byte pelo loadPng;
//   2. fase375b_ensure_pico_1_bloco — o ImpostorIndex: gera sob demanda
//      (impostors/<stem>.png), o 2.º ensure é HIT (zero leituras), o
//      fantasma nunca nasce PNG (o guard R-041) e NENHUMA leitura é o
//      ficheiro inteiro (o pico de RAM é UM bloco);
//   3. fase375b_decisao_distancia — o active(): 0 = OFF, o limiar é
//      EXCLUSIVO no lado longe (o MESMO contrato do lodActive), o
//      distOf é o blockDist do centro;
//   4. fase375b_billboard — a matriz: o centro no AABB, a normal +Z PARA
//      a câmara, o lado = a maior extensão; degenerados devolvem false
//      (nunca NaN); a vista de topo troca o up SEM NaN;
//   5. fase375b_quad_mask_pixel — PIXEL A PIXEL no rasterizador REAL do
//      stub: o quad com o MASK deixa o fundo onde o alpha é 0; sem o
//      MASK o MESMO quad cobre tudo (o discard é o que corta);
//   6. fase375b_walk_beyond_draws_quad — o walk do drawTics: além da
//      distância → 1 quad + ZERO draws de blocos; no pé → os blocos de
//      sempre; a GpuAssets::impostorTexture resolve o PNG (cache 1 ref =
//      1 textura) e o fantasma devolve nullptr (o fallback honesto);
//   7. fase375b_fallback_honesto — sem PNG (ou skinado) NUNCA há quad:
//      os blocos desenham como sempre (a silhueta mentida não existe);
//   8. fase375b_compat_011t — o .goni: ausente = 0 = OFF; com 120 a
//      chave "impostorDist" nasce; o load restaura; negativo = OFF; o
//      round-trip da forma 0.11-T é byte a byte.
//
// O ORQUESTRADOR REAL (o drawTics) é aferrado pela FASE 30 do c33_virtual
// — este TU prova a POLÍTICA (a decisão + o billboard + o MASK + o
// fallback) com o MESMO walk do main.
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats + fb)

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "assets/GOwnFormats.h"
#include "assets/ImpostorIndex.h"
#include "assets/PngLoader.h"
#include "assets/ResourceManager.h"
#include "assets/ThumbRender.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/ProjectStorage.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "render/BlockMesh.h"
#include "ui/Theme.h"   // 0.11-Y: o clear do beginFrame LÊ o token bg
#include "render/GpuAssets.h"
#include "render/Impostor.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "render/ThumbPng.h"

using namespace vv;

namespace {

// ---- os fixtures (o padrão do test_fase375a: quad strips por bloco) -------
void addQuadStrip(MeshData& m, f32 cx, f32 cz, f32 half, int nx, int nz,
                  const std::string& material) {
    const u32 base = static_cast<u32>(m.vertices.size());
    for (int iz = 0; iz <= nz; ++iz) {
        for (int ix = 0; ix <= nx; ++ix) {
            Vertex v;
            v.pos = Vec3{cx - half + 2.0f * half * f32(ix) / f32(nx), 0.0f,
                         cz - half + 2.0f * half * f32(iz) / f32(nz)};
            v.normal = Vec3{0.0f, 1.0f, 0.0f};
            v.uv = Vec2{f32(ix) / f32(nx), f32(iz) / f32(nz)};
            m.vertices.push_back(v);
        }
    }
    MeshData::Group g;
    g.material = material;
    g.firstIndex = static_cast<u32>(m.indices.size());
    for (int iz = 0; iz < nz; ++iz) {
        for (int ix = 0; ix < nx; ++ix) {
            const u32 a = base + static_cast<u32>(iz * (nx + 1) + ix);
            const u32 b = a + static_cast<u32>(nx + 1);
            m.indices.push_back(static_cast<u16>(a));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(a + 1));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(b + 1));
            m.indices.push_back(static_cast<u16>(a + 1));
            g.indexCount += 6;
        }
    }
    m.groups.push_back(g);
}

bool makeGMesh(int nBlocks, std::vector<u8>& gmesh, std::string& err) {
    MeshData m;
    m.name = "imp";
    for (int i = 0; i < nBlocks; ++i) {
        addQuadStrip(m, f32(i) * 10.0f, 0.0f, 1.0f, 4, 4,
                     i % 2 == 0 ? "verde" : "ambar");
    }
    return writeGMesh(m, gmesh, err);
}

// um .gmesh SKINADO (o guard do impostor lê ESTA flag — o padrão do pin
// 3a da R-047: a skin vive nos atributos, a flag é do meta)
bool makeSkinnedGMesh(std::vector<u8>& gmesh, std::string& err) {
    MeshData m;
    m.name = "impSkin";
    const Vec3 P[4] = {Vec3{-0.5f, -0.5f, 0.0f}, Vec3{0.5f, -0.5f, 0.0f},
                       Vec3{0.5f, 0.5f, 0.0f}, Vec3{-0.5f, 0.5f, 0.0f}};
    for (int k = 0; k < 4; ++k) {
        Vertex v;
        v.pos = P[k];
        v.normal = Vec3{0.0f, 0.0f, 1.0f};
        m.vertices.push_back(v);
        for (int c = 0; c < 4; ++c) {
            m.skinJoints.push_back(0);
            m.skinWeights.push_back(c == 0 ? 1.0f : 0.0f);
        }
    }
    const u16 IDX[6] = {0, 1, 2, 0, 2, 3};
    m.indices.assign(IDX, IDX + 6);
    return writeGMesh(m, gmesh, err);
}

// o storage de mapa (o padrão StreamStorage da 3.75a) + a CONTABILIDADE
// das leituras (o pin do pico: NENHUMA leitura é o ficheiro inteiro)
struct ImpStorage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    mutable u64 maxRead = 0;     // o maior single read (bytes)
    mutable u64 gmeshReads = 0;  // as leituras que tocaram o .gmesh
    std::string root() const override { return "/imp"; }
    bool makeDirs(const std::string&) override { return true; }
    bool exists(const std::string& relPath) const override {
        return files.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        return files.count(relPath) != 0 ? vv::Presence::Present
                                         : vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath,
                   const std::string& text) override {
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath, std::string& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }
    bool writeBytes(const std::string& relPath, const void* data,
                    size_t n) override {
        files[relPath].assign(static_cast<const char*>(data), n);
        return true;
    }
    bool readBytes(const std::string& relPath,
                   std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        if (relPath.find(".gmesh") != std::string::npos) {
            ++gmeshReads;                 // readBytes no .gmesh é o PADRÃO
            maxRead = std::max(maxRead, it->second.size());   // PROIBIDO
        }
        out.assign(it->second.begin(), it->second.end());
        return !out.empty();
    }
    bool listDir(const std::string&,
                 std::vector<std::string>&) const override {
        return false;
    }
    bool statBytes(const std::string& relPath,
                   vv::u64& outBytes) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        outBytes = it->second.size();
        return true;
    }
    bool readBytesAt(const std::string& relPath, vv::u64 offset, size_t len,
                     std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        if (offset + len > it->second.size()) return false;
        if (relPath.find(".gmesh") != std::string::npos) {
            ++gmeshReads;
            maxRead = std::max(maxRead, static_cast<u64>(len));
        }
        out.assign(it->second.begin() + static_cast<long>(offset),
                   it->second.begin() + static_cast<long>(offset + len));
        return out.size() == len;
    }
};

// o walk do drawTics SEM o main (a decisão → o billboard → o drawMesh com
// o MASK; o fallback é o caminho dos blocos de sempre) — a FASE 30 do c33
// aferra o orquestrador REAL; aqui a mecânica é direta.
struct ImpRun {
    DrawStats total;
    u32 quads = 0;       // os billboards desenhados
    u32 blockDc = 0;     // os dc do caminho dos blocos
};
ImpRun runImpostor(BlockMesh& bm, Renderer& r, Mesh& quad, const Mat4& vp,
                   const Vec3& camEye, const std::vector<Mat4>& models,
                   f32 impostorDistance, const Texture* impTex) {
    ImpRun out;
    for (const Mat4& m : models) {
        // o guard do drawTics: skinado NUNCA (a pose mente)
        if (impTex != nullptr && !bm.skinned() &&
            impostor::active(m, bm.boundsMin(), bm.boundsMax(), camEye,
                             impostorDistance)) {
            Mat4 bill;
            if (impostor::billboard(m, bm.boundsMin(), bm.boundsMax(), camEye,
                                    bill)) {
                DrawProps impProps;   // os defaults + o MASK (o MESMO draw
                                      // do drawTics — o quad de 4 verts)
                impProps.alphaMode = 1;
                impProps.alphaCutoff = 0.5f;
                impProps.baseColor[0] = impostor::kImpBaseColor;
                impProps.baseColor[1] = impostor::kImpBaseColor;
                impProps.baseColor[2] = impostor::kImpBaseColor;
                const DrawStats ds = r.drawMesh(quad, bill, vp, impTex,
                                                nullptr, nullptr, 0,
                                                &impProps);
                out.total = out.total + ds;
                ++out.quads;
                continue;
            }
        }
        // o caminho dos blocos de sempre (o fallback honesto)
        BlockMesh::Stats ss;
        const DrawStats ds = bm.draw(r, m, vp, nullptr, nullptr, nullptr, 0,
                                     ss, &camEye, 0.0f);
        out.total = out.total + ds;
        out.blockDc += ds.drawCalls;
    }
    return out;
}

// a textura RGBA de teste: a METADE DE CIMA opaca branca, a de baixo
// transparente (o MASK corta a de baixo — o pin do pixel). A Texture é
// não-copiável (o recurso GL) — o dono é o unique_ptr do chamador.
std::unique_ptr<Texture> makeHalfAlphaTexture() {
    const u32 w = 8, h = 8;
    std::vector<u8> px(w * h * 4, 0);   // rgb 0 + alpha 0 em tudo
    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            u8* p = &px[(y * w + x) * 4];
            if (y < h / 2) {
                p[0] = p[1] = p[2] = 0xFF;   // opaco
                p[3] = 0xFF;
            } else {
                p[3] = 0;   // transparente (o discard manda)
            }
        }
    }
    auto t = std::make_unique<Texture>();
    EXPECT(t->createFromRGBA(px.data(), w, h));
    return t;
}

} // namespace

// ---- PIN 1 — o PNG alfa: o fundo transparente e o lambert cru -------------

TEST(fase375b_png_alpha) {
    glstub::reset();
    // o MESMO mesh do quad (o impostor renderiza a silhueta DELE)
    MeshData quadMeshData;
    impostor::quadMesh(quadMeshData);
    // o render alfa: o fundo TRANSPARENTE (o corner é 0,0,0,0) e a
    // geometria opaca (alpha 0xFF) com o lambert CRU (sem ambient)
    std::vector<u8> rgba;
    EXPECT(thumbrender::renderMeshAlpha(quadMeshData, 64, rgba));
    EXPECT(rgba.size() == size_t(64) * 64 * 4);
    // o canto (sem geometria) é TRANSPARENTE
    const u8* c0 = &rgba[0];
    EXPECT(c0[0] == 0 && c0[1] == 0 && c0[2] == 0 && c0[3] == 0);
    // o centro tem o LAMBERT CRU do quad (n = (0,0,1); d = n·L = 0.2891 →
    // g = 73 EXATO — sem ambient, sem os offsets de tom da thumb; a
    // compensação do draw é o kImpBaseColor)
    const u8* cc = &rgba[(32 * 64 + 32) * 4];
    EXPECT(cc[3] == 0xFF);
    EXPECT(cc[0] == 73 && cc[1] == 73 && cc[2] == 73);
    // tudo o que é geometria é CINZA PURO opaco (o alpha é só o que o
    // MASK precisa)
    for (u32 i = 0; i < 64u * 64u; ++i) {
        const u8* p = &rgba[i * 4];
        if (p[3] == 0xFF) {
            EXPECT(p[0] == p[1] && p[1] == p[2]);   // cinza puro
        }
    }
    // DETERMINÍSTICO: o MESMO mesh → o MESMO bytes
    std::vector<u8> rgba2;
    EXPECT(thumbrender::renderMeshAlpha(quadMeshData, 64, rgba2));
    EXPECT(std::memcmp(rgba.data(), rgba2.data(), rgba.size()) == 0);
    // o renderMesh DE SEMPRE continua com o fundo OPACO e o lambert
    // ASSADO da casa (0.16 + 0.62·0.2891 = 0.33924 → r = 86) — as duas
    // rotinas diferem EXATAMENTE no que o impostor precisa
    std::vector<u8> rgbaThumb;
    EXPECT(thumbrender::renderMesh(quadMeshData, 64, rgbaThumb));
    const u8* t0 = &rgbaThumb[0];
    EXPECT(t0[0] == 0x14 && t0[1] == 0x14 && t0[2] == 0x16 && t0[3] == 0xFF);
    const u8* tc = &rgbaThumb[(32 * 64 + 32) * 4];
    EXPECT(tc[0] == 86);   // o lambert assado (o impostor tem 73)
    // o PNG RGBA (color type 6) round-tripa: encode → decode = os MESMOS
    std::vector<u8> png = thumbrender::encodePngRgba(rgba, 64);
    EXPECT(!png.empty());
    RawImage img;
    std::string err;
    EXPECT(loadPng(png.data(), png.size(), img, err));
    EXPECT(img.width == 64 && img.height == 64);
    EXPECT(img.rgba.size() == rgba.size());
    EXPECT(std::memcmp(img.rgba.data(), rgba.data(), rgba.size()) == 0);
    // o encode RGB de sempre CONTINUA RGB (o IHDR do color type 2 — o
    // paridade das thumbs fica intacta)
    std::vector<u8> pngRgb = thumbrender::encodePng(rgbaThumb, 64);
    EXPECT(!pngRgb.empty());
    EXPECT(pngRgb[25] == 2);   // o byte do color type no IHDR (RGB)
    EXPECT(png[25] == 6);      // o RGBA (color type 6)
}

// ---- PIN 2 — o índice: sob demanda, hit, fantasma e pico UM bloco ----------

TEST(fase375b_ensure_pico_1_bloco) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(12, gmesh, err));   // 12 blocos (o .gmesh "grande")
    ImpStorage st;
    st.files["assets/imp12.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    // (a) o fantasma NUNCA nasce PNG (o guard R-041 — a lição do thumb)
    err.clear();
    EXPECT(!ImpostorIndex::ensureImpostor(st, "assets/fantasma.gmesh",
                                          false, err));
    EXPECT(!st.exists("impostors/fantasma.png"));
    // (b) a geração sob demanda: o PNG nasce em impostors/<stem>.png
    err.clear();
    EXPECT(ImpostorIndex::ensureImpostor(st, "assets/imp12.gmesh", false,
                                         err));
    EXPECT(st.exists("impostors/imp12.png"));
    const std::string& png = st.files["impostors/imp12.png"];
    EXPECT(png.size() > 8);
    EXPECT(png[25] == 6);   // color type RGBA
    // (c) o HIT: o 2.º ensure NÃO lê o .gmesh (zero trabalho)
    const u64 reads1 = st.gmeshReads;
    err.clear();
    EXPECT(ImpostorIndex::ensureImpostor(st, "assets/imp12.gmesh", false,
                                         err));
    EXPECT(st.gmeshReads == reads1);
    // (d) O PIN DO PICO: a geração inteira (a leitura pela tabela + os
    //     blocos um a um) NUNCA leu o ficheiro inteiro — o maior single
    //     read é a faixa de UM bloco + a tabela, não os ~12 blocos
    EXPECT(st.maxRead < gmesh.size());
    // (e) o PNG decodifica com alpha REAL (o draw do MASK depende)
    RawImage img;
    EXPECT(loadPng(reinterpret_cast<const u8*>(png.data()), png.size(), img,
                   err));
    EXPECT(img.width == 256 && img.height == 256);
    bool anyOpaque = false, anyClear = false;
    for (size_t i = 0; i < img.rgba.size(); i += 4) {
        if (img.rgba[i + 3] == 0xFF) anyOpaque = true;
        if (img.rgba[i + 3] == 0) anyClear = true;
    }
    EXPECT(anyOpaque && anyClear);   // silhueta + fundo — o MASK tem o que cortar
}

// ---- PIN 3 — a decisão: 0 = OFF, o limiar exclusivo, o distOf --------------

TEST(fase375b_decisao_distancia) {
    glstub::reset();
    const Mat4 model =
        Mat4::mul(Mat4::translation(30.0f, 0.0f, 0.0f), Mat4::identity());
    // o AABB do quad strip: x ∈ [-1,1] (o bloco 0), z ∈ [-1,1], y = 0
    const Vec3 mn{-1.0f, 0.0f, -1.0f}, mx{1.0f, 0.0f, 1.0f};
    const Vec3 eye{0.0f, 0.0f, 0.0f};
    // 0 = OFF (o default — o byte a byte do pré-3.75b)
    EXPECT(!impostor::active(model, mn, mx, eye, 0.0f));
    EXPECT(!impostor::active(model, mn, mx, eye, -5.0f));
    // o distOf é a distância do CENTRO transformado (o MESMO do blockDist):
    // o centro do AABB ((0,0,0)) + translation(30) = (30,0,0); o olho na
    // origem → d = 30
    const f32 d = impostor::distOf(model, mn, mx, eye);
    EXPECT(std::fabs(d - 30.0f) < 1e-4f);
    // o limiar é EXCLUSIVO no lado longe (o MESMO contrato do lodActive):
    // no próprio limiar ainda é o normal
    EXPECT(!impostor::active(model, mn, mx, eye, d));
    EXPECT(impostor::active(model, mn, mx, eye, d - 0.001f));
}

// ---- PIN 4 — o billboard: o centro, a normal para a câmara, degenerados ----

TEST(fase375b_billboard) {
    glstub::reset();
    const Mat4 model = Mat4::translation(0.0f, 0.0f, -30.0f);
    // o AABB com o centro em y = 0 (a matemática limpa)
    const Vec3 mn{-2.0f, -2.0f, -1.0f}, mx{2.0f, 2.0f, 1.0f};
    const Vec3 eye{0.0f, 0.0f, 0.0f};
    Mat4 bill;
    EXPECT(impostor::billboard(model, mn, mx, eye, bill));
    // (a) o centro: a col3 é o centro do AABB transformado
    EXPECT(std::fabs(bill.m[12] - 0.0f) < 1e-5f);
    EXPECT(std::fabs(bill.m[13] - 0.0f) < 1e-5f);
    EXPECT(std::fabs(bill.m[14] + 30.0f) < 1e-5f);
    // (b) a normal (+Z do quad, a col2) aponta PARA a câmara (0,0,1 — o
    //     front CCW é o que a câmara vê; o contrário era o VERSO)
    EXPECT(std::fabs(bill.m[8] - 0.0f) < 1e-5f);
    EXPECT(std::fabs(bill.m[9] - 0.0f) < 1e-5f);
    EXPECT(std::fabs(bill.m[10] - 1.0f) < 1e-5f);
    // (c) o lado = a MAIOR extensão (x/y: 4) — as colunas 0/1 têm norma 2
    const f32 n0 = std::sqrt(bill.m[0] * bill.m[0] + bill.m[1] * bill.m[1] +
                             bill.m[2] * bill.m[2]);
    const f32 n1 = std::sqrt(bill.m[4] * bill.m[4] + bill.m[5] * bill.m[5] +
                             bill.m[6] * bill.m[6]);
    EXPECT(std::fabs(n0 - 2.0f) < 1e-4f);
    EXPECT(std::fabs(n1 - 2.0f) < 1e-4f);
    // (d) os degenerados: o AABB nulo e a câmara DENTRO do centro → false
    EXPECT(!impostor::billboard(model, Vec3{0, 0, 0}, Vec3{0, 0, 0}, eye,
                                bill));
    const Vec3 eyeCenter{0.0f, 0.0f, -30.0f};   // o centro do AABB
    EXPECT(!impostor::billboard(model, mn, mx, eyeCenter, bill));
    // (e) a vista de TOPO (a câmara na vertical — o orbit do dono): o up
    //     troca SEM NaN (a matriz mantém-se ortogonal)
    const Vec3 eyeTop{0.0f, 40.0f, -30.0f};
    EXPECT(impostor::billboard(model, mn, mx, eyeTop, bill));
    for (int k = 0; k < 16; ++k) {
        EXPECT(!std::isnan(bill.m[k]) && !std::isinf(bill.m[k]));
    }
    // a escala do TIC conta (o lado escala com as colunas do model)
    const Mat4 scaled = Mat4::mul(Mat4::scale(2.0f, 2.0f, 2.0f), model);
    EXPECT(impostor::billboard(scaled, mn, mx, eye, bill));
    const f32 n0s = std::sqrt(bill.m[0] * bill.m[0] + bill.m[1] * bill.m[1] +
                              bill.m[2] * bill.m[2]);
    EXPECT(std::fabs(n0s - 4.0f) < 1e-4f);   // 2 (extensão) × 2 (escala)
}

// ---- PIN 5 — o MASK no rasterizador REAL: o alpha corta o quad -------------

TEST(fase375b_quad_mask_pixel) {
    glstub::reset();
    glstub::fb::enabled = true;
    Renderer r;
    EXPECT(r.init());
    r.resize(64, 64);
    const Mat4 vp = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    Mesh quad;
    {
        MeshData md;
        impostor::quadMesh(md);
        EXPECT(quad.create(md.vertices.data(),
                           static_cast<u32>(md.vertices.size()),
                           md.indices.data(),
                           static_cast<u32>(md.indices.size())));
    }
    const std::unique_ptr<Texture> texUp = makeHalfAlphaTexture();
    const Texture* tex = texUp.get();   // cima opaco, baixo alfa 0
    // o quad ×12 (meia-extensão 6 num ortho de 20 unidades/64 px → o quad
    // ocupa os rows 13..51; o cima opaco = rows 32..51, o baixo
    // transparente = rows 13..31 — as metades claras no framebuffer)
    const Mat4 model = Mat4::mul(Mat4::scale(12.0f, 12.0f, 1.0f),
                                 Mat4::translation(0.0f, 0.0f, -10.0f));
    std::vector<u8> withMask, noMask;
    // (a) com o MASK: a metade transparente NÃO escreve (o fundo fica)
    {
        r.beginFrame();
        DrawProps p;
        p.alphaMode = 1;         // MASK — o discard do lit
        p.alphaCutoff = 0.5f;
        const DrawStats ds = r.drawMesh(quad, model, vp, tex, nullptr,
                                        nullptr, 0, &p);
        EXPECT(ds.drawCalls == 1);
        std::vector<u8> px(size_t(64) * 64 * 4);
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        withMask = px;
    }
    // (b) SEM o MASK (OPAQUE): o MESMO quad cobre o quadrado inteiro
    {
        r.beginFrame();
        const DrawStats ds = r.drawMesh(quad, model, vp, tex, nullptr,
                                        nullptr, 0, nullptr);
        EXPECT(ds.drawCalls == 1);
        std::vector<u8> px(size_t(64) * 64 * 4);
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        noMask = px;
    }
    // O PIN: a metade de CIMA desenha IGUAL nos dois (a geometria opaca);
    // a metade de BAIXO é o fundo limpo com o MASK e lit com o OPAQUE —
    // o discard é O QUE CORTA (a prova pixel a pixel do discard)
    auto pxAt = [](const std::vector<u8>& fb, u32 x, u32 y) -> const u8* {
        return &fb[(size_t(y) * 64u + x) * 4u];
    };
    // cima (row 44 — o fb tem a linha 0 no FUNDO: o topo do quad é y alto)
    for (u32 x : {20u, 32u, 44u}) {
        const u8* a = pxAt(withMask, x, 44);
        const u8* b = pxAt(noMask, x, 44);
        EXPECT(std::memcmp(a, b, 4) == 0);          // a geometria é a MESMA
        EXPECT(a[3] == 0xFF);                        // opaca
    }
    // o BG do beginFrame LIDO DO TOKEN (0.11-Y: o poço #08080A) — as
    // provas de baixo comparam contra o token, não contra um literal
    const u8 bgR = static_cast<u8>(vv::theme::kTheme.bg[0] * 255.0f + 0.5f);
    const u8 bgG = static_cast<u8>(vv::theme::kTheme.bg[1] * 255.0f + 0.5f);
    // baixo (y = 20): com o MASK o fundo fica; sem o MASK o lit escreve
    for (u32 x : {20u, 32u, 44u}) {
        const u8* a = pxAt(withMask, x, 20);
        const u8* b = pxAt(noMask, x, 20);
        EXPECT(std::memcmp(a, b, 4) != 0);          // o discard FEZ diferença
        // o fundo do beginFrame (o BG do token — o poço 0.11-Y) fica
        EXPECT(a[0] == bgR && a[1] == bgG);   // o BG da casa (o token)
    }
    // (c) o depth do descartado NÃO escreve: um quad ATRÁS na região
    //     transparente passa o depth e desenha (o MASK não oclui o vazio)
    {
        r.beginFrame();
        DrawProps p;
        p.alphaMode = 1;
        p.alphaCutoff = 0.5f;
        r.drawMesh(quad, model, vp, tex, nullptr, nullptr, 0, &p);
        // o quad B: centro y=-5 (a metade opaca dele sobe até ~row 35 —
        // dentro da zona TRANSPARENTE do quad A)
        const Mat4 near2 = Mat4::mul(Mat4::scale(12.0f, 12.0f, 1.0f),
                                     Mat4::translation(0.0f, -5.0f, -8.0f));
        r.drawMesh(quad, near2, vp, tex, nullptr, nullptr, 0, &p);
        std::vector<u8> px(size_t(64) * 64 * 4);
        glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        // o row 25 (transparente no quad A): o quad B desenhou POR CIMA
        const u8* a = pxAt(px, 32, 25);
        EXPECT(a[3] == 0xFF);   // o segundo quad desenhou no vazio do 1.º
        // e o MESMO pixel no frame SÓ-A é o fundo (a prova de que a
        // diferença é do quad B atravessar o vazio descartado)
        EXPECT(pxAt(withMask, 32, 25)[0] == bgR);   // o BG (o token 0.11-Y)
    }
    glstub::fb::enabled = false;
    glstub::reset();
}

// ---- PIN 6 — o walk: além desenha o quad, no pé desenha os blocos ----------

TEST(fase375b_walk_beyond_draws_quad) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(2, gmesh, err));
    ImpStorage st;
    st.files["assets/imp2.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/imp2.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    Mesh quad;
    {
        MeshData md;
        impostor::quadMesh(md);
        EXPECT(quad.create(md.vertices.data(),
                           static_cast<u32>(md.vertices.size()),
                           md.indices.data(),
                           static_cast<u32>(md.indices.size())));
    }
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 eye{0.0f, 0.0f, 0.0f};
    // a textura do impostor: a silhueta alfa do renderMeshAlpha (o MESMO
    // caminho do GpuAssets: PNG RGBA → loadPng → createFromRGBA)
    std::vector<u8> rgba;
    EXPECT(thumbrender::renderMeshAlpha(
        [&]() {
            MeshData m;
            addQuadStrip(m, 0.0f, 0.0f, 1.0f, 4, 4, "verde");
            return m;
        }(),
        64, rgba));
    Texture impTex;
    EXPECT(impTex.createFromRGBA(rgba.data(), 64, 64));
    // (a) ALÉM da distância: 2 TICs → 2 quads (2 dc) + ZERO dc de blocos
    {
        std::vector<Mat4> models{Mat4::translation(0.0f, 0.0f, -50.0f),
                                 Mat4::translation(40.0f, 0.0f, -50.0f)};
        const ImpRun run = runImpostor(bm, r, quad, vp, eye, models, 30.0f,
                                       &impTex);
        EXPECT(run.quads == 2);
        EXPECT(run.blockDc == 0);
        EXPECT(run.total.drawCalls == 2);
        EXPECT(run.total.vertices == 6u * 2u);   // 6 índices × 2 quads
    }
    // (b) NO PÉ (distância 0 = OFF): os blocos de sempre (o lazy inline)
    {
        const u32 d0 = static_cast<u32>(glstub::stats.drawElementsCalls);
        std::vector<Mat4> models{Mat4::translation(0.0f, 0.0f, -50.0f)};
        const ImpRun run = runImpostor(bm, r, quad, vp, eye, models, 0.0f,
                                       &impTex);
        EXPECT(run.quads == 0);
        EXPECT(run.blockDc == 2);   // 2 blocos do TIC
        EXPECT(static_cast<u32>(glstub::stats.drawElementsCalls) - d0 == 2);
    }
    // (c) a GpuAssets::impostorTexture: o PNG resolve do storage (a
    //     geração sob demanda incluída), o cache dá o MESMO ponteiro e o
    //     fantasma devolve nullptr (o fallback honesto)
    {
        glstub::fb::enabled = true;   // os ids ÚNICOS (o modo no-op dá o id
                                      // 1 a TODAS as texturas — a prova da
                                      // regeneração precisa do fb)
        ResourceManager rm;
        rm.setStorage(&st);
        GpuAssets gpu;
        gpu.init(&rm);
        const Texture* t = gpu.impostorTexture("assets/imp2.gmesh");
        EXPECT(t != nullptr);
        EXPECT(t->width() == 256 && t->height() == 256);
        EXPECT(gpu.impostorTexture("assets/imp2.gmesh") == t);   // o cache
        EXPECT(gpu.impostorTexture("assets/fantasma.gmesh") == nullptr);
        const u32 h1 = t->handle();   // o id GL ANTES do release (o id
                                      // não se reutiliza no stub — a
                                      // prova SEM ler ponteiro velho)
        gpu.releaseAll();   // o cache morre com o contexto (o TERM)
        const Texture* t2 = gpu.impostorTexture("assets/imp2.gmesh");
        EXPECT(t2 != nullptr && t2->handle() != h1);   // regenerada
        glstub::fb::enabled = false;
    }
}

// ---- PIN 7 — o fallback honesto: sem PNG ou skinado NUNCA há quad ----------

TEST(fase375b_fallback_honesto) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(1, gmesh, err));
    ImpStorage st;
    st.files["assets/imp1.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    // (a) SEM PNG no storage: a textura não resolve → os blocos de sempre
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/imp1.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    Mesh quad;
    {
        MeshData md;
        impostor::quadMesh(md);
        EXPECT(quad.create(md.vertices.data(),
                           static_cast<u32>(md.vertices.size()),
                           md.indices.data(),
                           static_cast<u32>(md.indices.size())));
    }
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 eye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models{Mat4::translation(0.0f, 0.0f, -50.0f)};
    {
        const ImpRun run = runImpostor(bm, r, quad, vp, eye, models, 30.0f,
                                       nullptr);   // sem textura
        EXPECT(run.quads == 0);
        EXPECT(run.blockDc == 1);   // o bloco desenhou como sempre
    }
    // (b) SKINADO com impostorDistance > 0: NUNCA quad (a pose é dinâmica
    //     — um PNG estático mente; o guard lê a FLAG do meta)
    std::vector<u8> gmeshSkin;
    EXPECT(makeSkinnedGMesh(gmeshSkin, err));
    st.files["assets/impskin.gmesh"].assign(
        reinterpret_cast<const char*>(gmeshSkin.data()), gmeshSkin.size());
    BlockMesh bmSkin;
    EXPECT(bmSkin.open(st, "assets/impskin.gmesh", err));
    EXPECT(bmSkin.skinned());   // a flag que o guard lê
    Texture impTex;
    {
        std::vector<u8> rgba;
        MeshData m;
        addQuadStrip(m, 0.0f, 0.0f, 1.0f, 4, 4, "verde");
        EXPECT(thumbrender::renderMeshAlpha(m, 64, rgba));
        EXPECT(impTex.createFromRGBA(rgba.data(), 64, 64));
    }
    {
        const ImpRun run = runImpostor(bmSkin, r, quad, vp, eye, models,
                                       30.0f, &impTex);
        EXPECT(run.quads == 0);
        EXPECT(run.blockDc == 1);   // o skinado segue o caminho de sempre
    }
    // (c) o PNG CORROMPIDO no storage: a textura não resolve (o loadPng
    //     recusa) — o TIC segue os blocos (nunca um quad branco a mentir)
    st.files["impostors/imp1.png"] = "isto não é um png";
    ResourceManager rm;
    rm.setStorage(&st);
    GpuAssets gpu;
    gpu.init(&rm);
    EXPECT(gpu.impostorTexture("assets/imp1.gmesh") == nullptr);
}

// ---- PIN 8 — o .goni: ausente = OFF, a chave só quando != 0 -----------------

TEST(fase375b_compat_011t) {
    glstub::reset();
    // (a) o TIC novo nasce OFF (0.0f — o comportamento pré-3.75b)
    {
        Scene scene;
        Handle h = scene.create("A");
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{1.0f, 2.0f, 3.0f};
        tr->updateWorld();
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        EXPECT(mr->impostorDistance == 0.0f);
        // o save SEM edição não contém a chave (os .goni da 0.11-T ficam
        // byte a byte)
        const std::string plain = SceneSerializer::dump(scene);
        EXPECT(plain.find("impostorDist") == std::string::npos);
        // com 120 a chave nasce e o load restaura
        mr->impostorDistance = 120.0f;
        const std::string on = SceneSerializer::dump(scene);
        EXPECT(on.find("\"impostorDist\":") != std::string::npos);
        SceneSerializer::LoadCtx ctx;
        Scene s1;
        EXPECT(SceneSerializer::loadText(s1, on, ctx));
        EXPECT(s1.components().meshRenderers().size() == 1);
        EXPECT(s1.components().meshRenderers().at(0).impostorDistance ==
               120.0f);
        Scene s2;
        EXPECT(SceneSerializer::loadText(s2, plain, ctx));
        EXPECT(s2.components().meshRenderers().at(0).impostorDistance ==
               0.0f);
        // (b) o negativo: o dump é HONESTO (escreve o valor que lá está —
        //     o MESMO contrato do lodDist) mas o LOADER não liga nunca com
        //     ele (o guard v > 0 — o OFF de sempre)
        mr->impostorDistance = -8.0f;
        const std::string neg = SceneSerializer::dump(scene);
        EXPECT(neg.find("\"impostorDist\":") != std::string::npos);
        Scene s5;
        EXPECT(SceneSerializer::loadText(s5, neg, ctx));
        EXPECT(s5.components().meshRenderers().at(0).impostorDistance ==
               0.0f);   // o negativo NUNCA liga (o guard do loader)
        // (c) um ficheiro com "impostorDist":-8 escrito à mão abre OFF
        const std::string hand = on;   // o com 120
        std::string injected;
        {
            const size_t at = hand.find("\"impostorDist\":120");
            ASSERT(at != std::string::npos);
            injected = hand.substr(0, at) + "\"impostorDist\":-8" +
                       hand.substr(at + std::strlen("\"impostorDist\":120"));
        }
        Scene s3;
        EXPECT(SceneSerializer::loadText(s3, injected, ctx));
        EXPECT(s3.components().meshRenderers().at(0).impostorDistance ==
               0.0f);
        // (d) o round-trip da forma 0.11-T é byte a byte (o dump após o
        //     load SEM edição reproduz o ficheiro)
        Scene s4;
        EXPECT(SceneSerializer::loadText(s4, plain, ctx));
        EXPECT(SceneSerializer::dump(s4) == plain);
    }
}
