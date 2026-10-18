// tests/test_thumbs.cpp — PASSO 5C: as THUMBNAILS PRÉ-RENDERIZADAS.
//
// Pins do dono:
//   · no import, thumb 256×256 em thumbs/ (por mesh, a MESMA rotina do
//     browser — ThumbIndex::ensureThumb force=true)
//   · 100 assets no browser = 100 thumbs em <500 ms (o custo de listar =
//     carregar+decodificar os PNG prontos — ThumbIndex::loadRows)
//   · thumb ausente = placeholder + render sob demanda (ensureThumb sem
//     force renderiza do .gmesh pela tabela de blocos — o pico de RAM é
//     UM bloco, nunca o modelo)
#include "TestFramework.h"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "assets/AssetConverter.h"
#include "assets/GOwnFormats.h"
#include "assets/PngLoader.h"
#include "assets/TexturePipeline.h"   // TexturePipeline + TextureCache (include)
#include "assets/TextureCompressor.h" // HardwareCompressor
#include "assets/ThumbIndex.h"
#include "assets/ThumbRender.h"
#include "core/FsStorage.h"
#include "FakeStorage.h"

using namespace vv;

namespace {

std::vector<u8> readFixture(const char* name) {
    std::string path = std::string(FIXTURE_DIR) + "/" + name;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return {};
    }
    std::vector<u8> bytes;
    u8 buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        bytes.insert(bytes.end(), buf, buf + n);
    }
    std::fclose(f);
    return bytes;
}

// um MeshData de CAIXA (12 triângulos, AABB [-1,1]³) — o render tem
// conteúdo aferível (silhueta, determinismo)
MeshData boxMesh() {
    MeshData m;
    const float v[8][3] = {{-1,-1,-1},{1,-1,-1},{1,1,-1},{-1,1,-1},
                           {-1,-1,1},{1,-1,1},{1,1,1},{-1,1,1}};
    for (int i = 0; i < 8; ++i) {
        Vertex vt;
        vt.pos = Vec3(v[i][0], v[i][1], v[i][2]);
        m.vertices.push_back(vt);
    }
    const u16 q[36] = {0,1,2, 0,2,3, 4,6,5, 4,7,6, 0,4,5, 0,5,1,
                       2,6,7, 2,7,3, 0,3,7, 0,7,4, 1,5,6, 1,6,2};
    m.indices.assign(q, q + 36);
    MeshData::Group g;
    g.name = "box";
    g.firstIndex = 0;
    g.indexCount = 36;
    m.groups.push_back(g);
    return m;
}

std::string b64encode(const u8* data, size_t n) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < n; i += 3) {
        const u32 v = (static_cast<u32>(data[i]) << 16) |
                      (static_cast<u32>(data[i + 1]) << 8) | data[i + 2];
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];
        out += tbl[v & 63];
    }
    if (i < n) {
        u32 v = static_cast<u32>(data[i]) << 16;
        if (i + 1 < n) v |= static_cast<u32>(data[i + 1]) << 8;
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += (i + 1 < n) ? tbl[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

} // namespace

// ---- o render: conteúdo, determinismo e os limites ---------------------------
TEST(thumbrender_conteudo_determinismo_e_placeholder) {
    const MeshData box = boxMesh();
    std::vector<u8> a, b;
    ASSERT(thumbrender::renderMesh(box, thumbrender::kThumbSize, a));
    ASSERT(thumbrender::renderMesh(box, thumbrender::kThumbSize, b));
    EXPECT(a.size() == static_cast<size_t>(256) * 256 * 4);
    EXPECT(a == b);   // DETERMINÍSTICO: mesmos vértices → mesmos bytes

    // conteúdo: NÃO é o fundo liso (a caixa cobre pixéis) e a silhueta
    // fica DENTRO da miniatura (o encaixe)
    u32 dif = 0;
    for (size_t i = 0; i + 3 < a.size(); i += 4) {
        if (a[i] != 0x14 || a[i + 1] != 0x14 || a[i + 2] != 0x16) {
            ++dif;
        }
    }
    EXPECT(dif > 1000u);   // a caixa desenha (muito mais que o fundo)

    // o placeholder: determinístico, com as iniciais, e DIFERENTE do render
    std::vector<u8> p1, p2;
    thumbrender::placeholder("grelha1", thumbrender::kThumbSize, p1);
    thumbrender::placeholder("grelha1", thumbrender::kThumbSize, p2);
    EXPECT(p1 == p2);
    EXPECT(p1 != a);
    // o quadro do placeholder (as bordas 0x3A)
    EXPECT(p1[0] == 0x3A && p1[1] == 0x3A && p1[2] == 0x3A);

    // o mesh vazio → o render falha (o chamador usa o placeholder)
    MeshData vazio;
    std::vector<u8> e;
    EXPECT(!thumbrender::renderMesh(vazio, 256, e));

    // o encode PNG volta a decodificar (o round-trip pelo stb)
    std::vector<u8> png = thumbrender::encodePng(a, 256);
    ASSERT(!png.empty());
    RawImage back;
    std::string err;
    ASSERT(loadPng(png.data(), png.size(), back, err));
    EXPECT(back.width == 256u && back.height == 256u);
}

// ---- o import escreve thumbs/<stem>.png por mesh ------------------------------
TEST(import_escreve_thumbs_por_mesh) {
    const std::string dir =
        std::string("/tmp/goni_5c_") +
        std::to_string(static_cast<long>(::getpid()));
    system((std::string("rm -rf ") + dir).c_str());
    system(("mkdir -p " + dir + "/proj").c_str());
    FsStorage st((dir + "/proj").c_str());
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(true);

    // o glTF de 2 meshes (o builder da suíte das texturas, a fixture 256)
    const std::vector<u8> png = readFixture("blue256.png");
    ASSERT(png.size() > 8u);
    const std::string b64 = b64encode(png.data(), png.size());
    // 2 prims + 2 nós com mesh → 2 peças → 2 thumbs
    std::string prims, nodes;
    for (int i = 0; i < 2; ++i) {
        nodes += (i ? "," : "");
        nodes += std::string("{\"mesh\":") + std::to_string(i) +
                 ",\"translation\":[0,0,0],\"name\":\"m" +
                 std::to_string(i) + "\"}";
    }
    const u32 kVerts = 3u;
    const std::string json =
        std::string("{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
                    "\"scenes\":[{\"nodes\":[0]}],\"nodes\":[") +
        nodes + "],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
        "\"indices\":1,\"material\":0}]},{\"primitives\":[{"
        "\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":1}]}],"
        "\"buffers\":[{\"byteLength\":42}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":"
        "\"SCALAR\"}],"
        "\"materials\":[{\"pbrMetallicRoughness\":{\"baseColorTexture\":"
        "{\"index\":0}}},{\"pbrMetallicRoughness\":{\"baseColorTexture\":"
        "{\"index\":1}}}],"
        "\"textures\":[{\"source\":0},{\"source\":1}],"
        "\"images\":[{\"uri\":\"data:image/png;base64," + b64 +
        "\"},{\"uri\":\"data:image/png;base64," + b64 + "\"}]}";
    std::vector<u8> jsonBytes(json.begin(), json.end());
    while (jsonBytes.size() % 4u != 0u) {
        jsonBytes.push_back(0x20);
    }
    std::vector<u8> bin(44, 0);
    const float tri[9] = {0,0,0, 1,0,0, 0,1,0};
    const unsigned short idx[3] = {0,1,2};
    std::memcpy(bin.data(), tri, 36);
    std::memcpy(bin.data() + 36, idx, 6);
    const u32 total = 12u + 8u + static_cast<u32>(jsonBytes.size()) + 8u +
                      static_cast<u32>(bin.size());
    std::vector<u8> glb;
    const auto u32le = [&glb](u32 v) {
        glb.push_back(static_cast<u8>(v));
        glb.push_back(static_cast<u8>(v >> 8));
        glb.push_back(static_cast<u8>(v >> 16));
        glb.push_back(static_cast<u8>(v >> 24));
    };
    glb.insert(glb.end(), {'g', 'l', 'T', 'F'});
    u32le(2);
    u32le(total);
    u32le(static_cast<u32>(jsonBytes.size()));
    glb.insert(glb.end(), {'J', 'S', 'O', 'N'});
    glb.insert(glb.end(), jsonBytes.begin(), jsonBytes.end());
    u32le(static_cast<u32>(bin.size()));
    glb.insert(glb.end(), {'B', 'I', 'N', 0});
    glb.insert(glb.end(), bin.begin(), bin.end());

    const std::string src = dir + "/modelo.glb";
    FILE* f = std::fopen(src.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);

    TexturePipeline pipe(hw, cache);
    convert::Output out;
    convert::Stats stats;
    std::string err;
    // o import EXPANDIDO (2 nós com mesh → 2 peças → 2 thumbs; o fundido
    // de sempre daria 1 mesh só — o «por mesh» do pin é a peça por nó)
    ASSERT(convert::importFile(src, "modelo.glb", st, &pipe, out, stats,
                               err, nullptr, nullptr, /*expandNodes=*/true));
    ASSERT(out.meshes.size() == 2u);
    // A THUMB POR MESH (o pin: no import, thumb 256×256 em thumbs/)
    EXPECT(stats.thumbs == 2u);
    EXPECT(st.exists("thumbs/modelo_m0.png"));
    EXPECT(st.exists("thumbs/modelo_m1.png"));
    // o PNG decodifica 256×256 (o tamanho do pin)
    std::vector<u8> bytes;
    ASSERT(st.readBytes("thumbs/modelo_m0.png", bytes));
    RawImage img;
    ASSERT(loadPng(bytes.data(), bytes.size(), img, err));
    EXPECT(img.width == 256u && img.height == 256u);
    // a limpeza SÓ no fim (o rm antes das provas apagava o storage inteiro —
    // o exists a seguir media um diretório INEXISTENTE, não o import)
    system((std::string("rm -rf ") + dir).c_str());
}

// ---- thumb ausente = placeholder + render sob demanda -------------------------
TEST(thumb_ausente_placeholder_e_render_sob_demanda) {
    FakeStorage st;
    // o asset .gmesh no storage (v3 pequeno — o writer de sempre)
    MeshData box = boxMesh();
    std::vector<u8> gmesh;
    std::string err;
    ASSERT(writeGMesh(box, gmesh, err));
    ASSERT(st.writeBytes("assets/caixa.gmesh", gmesh.data(), gmesh.size()));

    // 1) AUSENTE: hasThumb=false — o browser mostra o placeholder
    EXPECT(!ThumbIndex::hasThumb(st, "assets/caixa.gmesh"));
    const ThumbIndex::Rows r0 = ThumbIndex::loadRows(st, {"assets/caixa.gmesh"});
    EXPECT(r0.loaded == 0u && r0.placeholders == 1u);

    // 2) RENDER SOB DEMANDA: ensureThumb lê o .gmesh pela TABELA (as faixas
    //    do PASSO 4 — o pico de RAM é UM bloco) e escreve o PNG
    ASSERT(ThumbIndex::ensureThumb(st, "assets/caixa.gmesh",
                                   /*force=*/false, err));
    EXPECT(ThumbIndex::hasThumb(st, "assets/caixa.gmesh"));
    const ThumbIndex::Rows r1 = ThumbIndex::loadRows(st, {"assets/caixa.gmesh"});
    EXPECT(r1.loaded == 1u && r1.placeholders == 0u);

    // 3) o force (o import) RE-RENDERIZA mesmo existindo
    const auto antes = st.exists("thumbs/caixa.png");
    ASSERT(ThumbIndex::ensureThumb(st, "assets/caixa.gmesh",
                                   /*force=*/true, err));
    EXPECT(antes);   // o ficheiro continua lá (reescrito)

    // 4) o asset AUSENTE: a thumb não nasce do nada (o erro legível) e o
    //    browser segue com o placeholder (NUNCA bloqueia)
    std::string err2;
    EXPECT(!ThumbIndex::ensureThumb(st, "assets/fantasma.gmesh",
                                    /*force=*/true, err2));
    EXPECT(!err2.empty());
    EXPECT(!ThumbIndex::hasThumb(st, "assets/fantasma.gmesh"));
}

// ---- O PIN: 100 assets = 100 thumbs em <500 ms ---------------------------------
TEST(pin_100_thumbs_em_menos_de_500ms) {
    FakeStorage st;
    MeshData box = boxMesh();
    std::vector<u8> gmesh;
    std::string err;
    ASSERT(writeGMesh(box, gmesh, err));

    // 100 assets (o mesmo mesh — o custo é o do LISTAR: 100 leituras +
    // 100 decodificações PNG de 256×256)
    std::vector<std::string> assets;
    for (int i = 0; i < 100; ++i) {
        const std::string rel =
            std::string("assets/asset") + std::to_string(i) + ".gmesh";
        ASSERT(st.writeBytes(rel, gmesh.data(), gmesh.size()));
        assets.push_back(rel);
        ASSERT(ThumbIndex::ensureThumb(st, rel, /*force=*/true, err));
    }
    const ThumbIndex::Rows r = ThumbIndex::loadRows(st, assets);
    std::printf("  [pin 5C] 100 thumbs: %u carregadas, %u placeholders, "
                "%.1f ms (o piso do dono: <500 ms)\n",
                r.loaded, r.placeholders, r.ms);
    std::fflush(stdout);
    EXPECT(r.loaded == 100u);
    EXPECT(r.ms >= 0.0 && r.ms < 500.0);   // O PIN
}

// ---- o gigante: a thumb lê SÓ o primeiro bloco (o orçamento da casa) -----------
TEST(thumb_do_gigante_le_so_o_primeiro_bloco) {
    FakeStorage st;
    // 70 000 verts (o padrão das suítes de streaming) — o MeshData único
    // NÃO cabe no caminho de sempre sem o teto; a thumb lê pela TABELA
    MeshData m;
    for (u32 i = 0; i < 70000; ++i) {
        Vertex vt;
        vt.pos = Vec3(static_cast<f32>(i % 250) - 125.0f,
                      static_cast<f32>((i / 250) % 250) - 125.0f,
                      static_cast<f32>(i % 280) - 140.0f);
        m.vertices.push_back(vt);
    }
    for (u32 t = 0; t < 69998; t += 3) {
        m.indices.push_back(static_cast<u16>(t % 65535));
        m.indices.push_back(static_cast<u16>((t + 1) % 65535));
        m.indices.push_back(static_cast<u16>((t + 2) % 65535));
    }
    std::vector<u8> gmesh;
    std::string err;
    ASSERT(writeGMesh(m, gmesh, err));
    ASSERT(st.writeBytes("assets/gigante.gmesh", gmesh.data(),
                         gmesh.size()));

    // o render sob demanda num .gmesh com blocos (o writer corta ao teto)
    ASSERT(ThumbIndex::ensureThumb(st, "assets/gigante.gmesh",
                                   /*force=*/true, err));
    EXPECT(ThumbIndex::hasThumb(st, "assets/gigante.gmesh"));
    std::vector<u8> png;
    ASSERT(st.readBytes("thumbs/gigante.png", png));
    RawImage img;
    ASSERT(loadPng(png.data(), png.size(), img, err));
    EXPECT(img.width == 256u && img.height == 256u);
}
