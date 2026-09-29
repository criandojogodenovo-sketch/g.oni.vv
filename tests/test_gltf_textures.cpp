// tests/test_gltf_textures.cpp — F5.1-B: texturas embutidas em glTF/GLB.
// Parser (data: URI base64 + bufferView do GLB + mime), materiais com
// baseColorTexture, extração para textures/<hash>.png com DEDUP por hash,
// pass-through de uri externa, PNG inválido → sem textura e o round-trip
// pelo ResourceManager (meshTextureFor).
#include "TestFramework.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "assets/GltfImporter.h"
#include "assets/GltfTextures.h"
#include "assets/ResourceManager.h"
#include "FakeStorage.h"

using namespace vv;

namespace {

// ---- base64 encode (p/ data: URIs das fixtures) -----------------------------
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

void pushF32(std::vector<u8>& b, f32 v) {
    u8 tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}

void pushU16(std::vector<u8>& b, u16 v) {
    u8 tmp[2];
    std::memcpy(tmp, &v, 2);
    b.insert(b.end(), tmp, tmp + 2);
}

// triângulo: POSITION (3×f32×3) + índices u16 — devolve offsets/lengths
std::vector<u8> triBuffer(u32& posOff, u32& posLen, u32& idxOff, u32& idxLen) {
    std::vector<u8> b;
    posOff = 0;
    pushF32(b, 0); pushF32(b, 0); pushF32(b, 0);
    pushF32(b, 1); pushF32(b, 0); pushF32(b, 0);
    pushF32(b, 0); pushF32(b, 1); pushF32(b, 0);
    posLen = 36;
    idxOff = static_cast<u32>(b.size());
    pushU16(b, 0); pushU16(b, 1); pushU16(b, 2);
    idxLen = 6;
    return b;
}

// .gltf com textura embutida em data: URI (image/png) referenciada pelo
// material; `matCount` materiais todos apontando para a MESMA textura
std::string texturedGltfJson(const std::vector<u8>& png, u32 matCount,
                             const char* uriPrefix = "") {
    u32 po, pl, io, il;
    const std::vector<u8> buf = triBuffer(po, pl, io, il);
    std::string mats;
    for (u32 i = 0; i < matCount; ++i) {
        mats += (i ? "," : "");
        mats += "{\"name\":\"mat" + std::to_string(i) + "\","
                "\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0}}}";
    }
    const std::string b64 = b64encode(png.data(), png.size());
    std::string j =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64," +
        b64encode(buf.data(), buf.size()) + "\",\"byteLength\":" +
        std::to_string(buf.size()) + "}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":" + std::to_string(po) +
        ",\"byteLength\":" + std::to_string(pl) + "},"
        "{\"buffer\":0,\"byteOffset\":" + std::to_string(io) +
        ",\"byteLength\":" + std::to_string(il) + "}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"materials\":[" + mats + "],"
        "\"textures\":[{\"source\":0}],"
        "\"images\":[{\"uri\":\"" + uriPrefix +
        "data:image/png;base64," + b64 + "\"}],"
        "\"meshes\":[{\"name\":\"tri\",\"primitives\":[{"
        "\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
        "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
        "\"scenes\":[{\"nodes\":[]}]}";
    return j;
}

// triângulo completo com textura EXTERNA (uri relativo, sem data:)
std::string texturedGltfExternJson() {
    u32 po, pl, io, il;
    const std::vector<u8> buf = triBuffer(po, pl, io, il);
    char j[900];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,%s\","
        "\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"materials\":[{\"name\":\"m\",\"pbrMetallicRoughness\":"
        "{\"baseColorTexture\":{\"index\":0}}}],"
        "\"textures\":[{\"source\":0}],"
        "\"images\":[{\"uri\":\"textures/externa.png\"}],"
        "\"meshes\":[{\"name\":\"tri\",\"primitives\":[{"
        "\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]}]}"
        ,
        b64encode(buf.data(), buf.size()).c_str(), static_cast<u32>(buf.size()),
        po, pl, io, il);
    return j;
}

// triângulo completo SEM material/textura
std::string triGltfSemMaterialJson() {
    u32 po, pl, io, il;
    const std::vector<u8> buf = triBuffer(po, pl, io, il);
    char j[700];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,%s\","
        "\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"name\":\"tri\",\"primitives\":[{"
        "\"attributes\":{\"POSITION\":0},\"indices\":1}]}]}"
        ,
        b64encode(buf.data(), buf.size()).c_str(), static_cast<u32>(buf.size()),
        po, pl, io, il);
    return j;
}

} // namespace

TEST(gltf_parser_image_base64_e_material_referencia) {
    const std::vector<u8> png = readFixture("yellow4.png");
    EXPECT(png.size() > 8u);

    const std::string json = texturedGltfJson(png, 2);
    GltfModel model;
    std::string err;
    EXPECT(parseGltf(json.data(), json.size(), {}, {}, model, err));
    EXPECT(err.empty());
    // imagem embutida decodificada — bytes IGUAIS ao PNG de origem
    EXPECT(model.images.size() == 1u);
    EXPECT(model.images[0].bytes == png);
    EXPECT(model.images[0].mime == "image/png");
    // material → textures[0].source → images[0]
    EXPECT(model.materials.size() == 2u);
    EXPECT(model.materials[0].baseColorTex == 0);
    EXPECT(model.materials[1].baseColorTex == 0);
    // mesh → material[0]
    EXPECT(model.meshMaterial.size() == 1u);
    EXPECT(model.meshMaterial[0] == 0);
}

TEST(gltf_parser_imagem_em_bufferview_do_glb) {
    u32 po, pl, io, il;
    std::vector<u8> buf = triBuffer(po, pl, io, il);
    const std::vector<u8> png = readFixture("yellow4.png");
    const u32 imgOff = static_cast<u32>(buf.size());
    buf.insert(buf.end(), png.begin(), png.end());

    char j[1100];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"materials\":[{\"name\":\"m\",\"pbrMetallicRoughness\":"
        "{\"baseColorTexture\":{\"index\":0}}}],"
        "\"textures\":[{\"source\":0}],"
        "\"images\":[{\"bufferView\":2,\"mimeType\":\"image/png\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
        "\"indices\":1,\"material\":0}]}]}",
        static_cast<u32>(buf.size()), po, pl, io, il, imgOff,
        static_cast<u32>(png.size()));

    GltfModel model;
    std::string err;
    EXPECT(parseGltf(reinterpret_cast<const char*>(j), std::strlen(j), buf, {},
                     model, err));
    EXPECT(err.empty());
    EXPECT(model.images.size() == 1u);
    EXPECT(model.images[0].bytes == png);   // extraída do BIN chunk intacta
    EXPECT(model.images[0].mime == "image/png");
    EXPECT(model.materials[0].baseColorTex == 0);
}

TEST(gltf_extracao_escreve_textures_hash_e_dedup) {
    FakeStorage st;
    const std::vector<u8> png = readFixture("yellow4.png");
    const std::string rel = gltfTextureRelPath(png);
    EXPECT(rel.rfind("textures/gltf_", 0) == 0);
    EXPECT(rel.size() > std::string("textures/gltf_.png").size());

    // 2 materiais com a MESMA imagem → 2 meshes, 1 ficheiro
    const std::string json = texturedGltfJson(png, 2);
    GltfModel model;
    std::string err;
    EXPECT(parseGltf(json.data(), json.size(), {}, {}, model, err));
    EXPECT(err.empty());

    std::vector<std::string> meshTex;
    EXPECT(extractGltfTextures(st, model, meshTex, err));
    EXPECT(err.empty());
    EXPECT(meshTex.size() == model.meshes.size());
    EXPECT(meshTex[0] == rel);
    EXPECT(st.exists(rel));
    std::vector<u8> written;
    EXPECT(st.readBytes(rel, written));
    EXPECT(written == png);   // bytes intactos

    // segundo modelo com a MESMA textura → reutiliza (sem 2º ficheiro)
    const std::string json2 = texturedGltfJson(png, 1);
    GltfModel model2;
    EXPECT(parseGltf(json2.data(), json2.size(), {}, {}, model2, err));
    std::vector<std::string> meshTex2;
    EXPECT(extractGltfTextures(st, model2, meshTex2, err));
    EXPECT(meshTex2[0] == rel);
    std::vector<std::string> files;
    EXPECT(st.listDir("textures", files));
    EXPECT(files.size() == 1u);   // DEDUP: 2 extrações → 1 ficheiro
}

TEST(gltf_extracao_casos_degradados_sem_crash) {
    FakeStorage st;

    // PNG inválido embutido → caminho vazio, erro anotado, sem ficheiro
    const std::vector<u8> lixo = {'n', 'a', 'o', ' ', 'p', 'n', 'g'};
    const std::string json = texturedGltfJson(lixo, 1);
    GltfModel model;
    std::string err;
    // o parser aceita (só bytes); a EXTRAÇÃO valida o PNG
    EXPECT(parseGltf(json.data(), json.size(), {}, {}, model, err));
    std::vector<std::string> meshTex;
    EXPECT(extractGltfTextures(st, model, meshTex, err));
    EXPECT(meshTex[0].empty());
    EXPECT(!err.empty());

    // uri externa relativa → pass-through (comportamento F5) — glTF válido
    // (triângulo completo) com a textura a apontar para um ficheiro externo
    const std::string jsonExt = texturedGltfExternJson();
    GltfModel m2;
    err.clear();   // (o parse só escreve `err` em falha — sujo do caso 1)
    EXPECT(parseGltf(jsonExt.data(), jsonExt.size(), {}, {}, m2, err));
    EXPECT(err.empty());
    // imagem externa: bytes vazios + uriPath capturado
    EXPECT(m2.images[0].bytes.empty());
    EXPECT(m2.images[0].uriPath == "textures/externa.png");
    std::vector<std::string> meshTex2;
    EXPECT(extractGltfTextures(st, m2, meshTex2, err));
    EXPECT(err.empty());
    EXPECT(meshTex2[0] == "textures/externa.png");

    // sem material/textura → vazio, sem erro
    const std::string jsonSemTex = triGltfSemMaterialJson();
    GltfModel m3;
    EXPECT(parseGltf(jsonSemTex.data(), jsonSemTex.size(), {}, {}, m3, err));
    EXPECT(err.empty());
    err.clear();
    EXPECT(m3.meshMaterial[0] == -1);
    std::vector<std::string> meshTex3;
    EXPECT(extractGltfTextures(st, m3, meshTex3, err));
    EXPECT(err.empty());
    EXPECT(meshTex3[0].empty());
}

TEST(gltf_rm_roundtrip_mesh_texture_for) {
    FakeStorage st;
    ResourceManager rm;
    rm.setStorage(&st);

    const std::vector<u8> png = readFixture("yellow4.png");
    const std::string json = texturedGltfJson(png, 1);
    EXPECT(st.writeText("meshes/model.gltf", json));

    std::string err;
    const MeshData* mesh = rm.mesh("meshes/model.gltf", err);
    EXPECT(mesh != nullptr);
    EXPECT(mesh->ok());

    // material referencia a textura extraída (dedup por hash)
    const std::string texRel = rm.meshTextureFor("meshes/model.gltf");
    EXPECT(texRel.rfind("textures/gltf_", 0) == 0);
    std::vector<u8> written;
    EXPECT(st.readBytes(texRel, written));
    EXPECT(written == png);

    // ref de modelo multi-mesh inexistente/sem textura → ""
    EXPECT(rm.meshTextureFor("meshes/desconhecido.gltf#0").empty());
    EXPECT(rm.meshTextureFor("meshes/outro.obj").empty());

    // recarregar (release + load) → extração idempotente, MESMO ficheiro
    rm.releaseAll();
    EXPECT(rm.mesh("meshes/model.gltf", err) != nullptr);
    EXPECT(rm.meshTextureFor("meshes/model.gltf") == texRel);
    std::vector<std::string> files;
    EXPECT(st.listDir("textures", files));
    EXPECT(files.size() == 1u);
}
