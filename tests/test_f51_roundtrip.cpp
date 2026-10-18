// tests/test_f51_roundtrip.cpp — F5.1-D: round-trip COMPLETO do pipeline
// de assets no hospedeiro:
//   .glb com textura embutida (256px) → RM.mesh (parse) → extração da
//   textura p/ textures/<hash>.png (dedup) → TexturePipeline (compressão
//   ETC2 + cache em disco) → export OBJ → reimport do OBJ → cena salva e
//   recarregada com meshPath/texPath resolvendo nos objetos certos.
#include "TestFramework.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "assets/GltfImporter.h"
#include "assets/ObjExporter.h"
#include "assets/ResourceManager.h"
#include "assets/TexturePipeline.h"
#include "components/MeshRenderer.h"
#include "core/Project.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
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

// triângulo com textura embutida (data: URI) — o mesh de import
// (std::string: o base64 de um PNG 256px passa de 700 chars)
std::string robotGltfJson(const std::vector<u8>& png) {
    std::vector<u8> buf;
    pushF32(buf, 0); pushF32(buf, 0); pushF32(buf, 0);
    pushF32(buf, 1); pushF32(buf, 0); pushF32(buf, 0);
    pushF32(buf, 0); pushF32(buf, 1); pushF32(buf, 0);
    pushU16(buf, 0); pushU16(buf, 1); pushU16(buf, 2);

    std::string j =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,";
    j += b64encode(buf.data(), buf.size());
    j += "\",\"byteLength\":" + std::to_string(buf.size()) + "}],"
         "\"bufferViews\":["
         "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
         "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
         "\"accessors\":["
         "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
         "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
         "\"materials\":[{\"name\":\"roboto\",\"pbrMetallicRoughness\":"
         "{\"baseColorTexture\":{\"index\":0}}}],"
         "\"textures\":[{\"source\":0}],"
         "\"images\":[{\"uri\":\"data:image/png;base64,";
    j += b64encode(png.data(), png.size());
    j += "\"}],"
         "\"meshes\":[{\"name\":\"robot\",\"primitives\":[{"
         "\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
         "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
         "\"scenes\":[{\"nodes\":[]}]}";
    return j;
}

} // namespace

TEST(f51_roundtrip_glb_textura_compressao_export_reimport) {
    FakeStorage st;
    ResourceManager rm;
    rm.setStorage(&st);

    // 1) IMPORT: glTF com textura embutida 256px (blue256) entra no projeto
    const std::vector<u8> png = readFixture("blue256.png");
    EXPECT(png.size() > 8u);
    EXPECT(st.writeText("meshes/robot.gltf", robotGltfJson(png)));

    std::string err;
    const MeshData* mesh = rm.mesh("meshes/robot.gltf", err);
    EXPECT(mesh != nullptr && mesh->ok());
    EXPECT(mesh->vertices.size() == 3u);

    // 2) EXTRAÇÃO: material referencia a textura extraída (hash, dedup)
    const std::string texRel = rm.meshTextureFor("meshes/robot.gltf");
    EXPECT(texRel.rfind("textures/gltf_", 0) == 0);
    std::vector<u8> extracted;
    EXPECT(st.readBytes(texRel, extracted));
    EXPECT(extracted == png);   // bytes originais intactos na pasta do projeto

    // 3) COMPRESSÃO: pipeline (ETC2 + cache) sobre a textura extraída
    FakeStorage cacheStorage;
    TextureCache cache(cacheStorage);
    HardwareCompressor hw;
    hw.setAstcSupported(false);
    TexturePipeline pipe(hw, cache);
    CompressedImage comp;
    TextureLoadInfo info;
    std::string perr;
    EXPECT(pipe.process(extracted.data(), extracted.size(), texRel.c_str(),
                        comp, info, perr));
    EXPECT(err.empty());
    EXPECT(info.format == CompressedFormat::ETC2_RGB);   // 256px, opaco
    EXPECT(!info.cacheHit);
    EXPECT(comp.width == 256u && comp.height == 256u);   // sem downscale
    // 2ª passada pelo MESMO PNG → hit no cache em disco
    CompressedImage comp2;
    TextureLoadInfo info2;
    EXPECT(pipe.process(extracted.data(), extracted.size(), texRel.c_str(),
                        comp2, info2, perr));
    EXPECT(info2.cacheHit);
    EXPECT(comp2.data == comp.data);

    // 4) EXPORT: mesh importado → OBJ no projeto
    const std::string obj = exportObj(*mesh);
    EXPECT(!obj.empty());
    EXPECT(st.writeText("meshes/export_robot.obj", obj));

    // 5) CENA: TIC com refs relativas (meshPath + texPath extraída), salva
    // no projeto e RECARREGADA (fresh RM/storage — os refs rebindam)
    Project proj;
    EXPECT(Project::openOrCreate(st, "projeto", proj));
    Scene scene;
    const Handle h = scene.create("robot");
    EXPECT(h.valid());
    MeshRenderer* mr = scene.get(h)->addComponent<MeshRenderer>();
    mr->meshPath = "meshes/robot.gltf";
    mr->texPath = texRel;
    EXPECT(proj.saveActiveScene(st, scene));
    EXPECT(proj.saveManifest(st));

    // reimport: NOVO ResourceManager (cache vazio) sobre o MESMO storage —
    // o ciclo completo tem de reconstruir tudo a partir dos ficheiros
    ResourceManager rm2;
    rm2.setStorage(&st);
    std::string err2;
    const MeshData* mesh2 = rm2.mesh("meshes/export_robot.obj", err2);
    EXPECT(mesh2 != nullptr && mesh2->ok());
    EXPECT(mesh2->vertices.size() == mesh->vertices.size());   // round-trip
    EXPECT(mesh2->indices.size() == mesh->indices.size());

    Scene scene2;
    SceneSerializer::LoadCtx ctx;
    ctx.resolveMesh = [](const std::string&) -> Mesh* { return nullptr; };
    ctx.resolveTex = [](const std::string& ref) -> const Texture* {
        return ref.rfind("textures/gltf_", 0) == 0
                   ? reinterpret_cast<const Texture*>(0x51) : nullptr;
    };
    EXPECT(proj.loadActiveScene(st, scene2, ctx));
    const Handle h2 = scene2.find("robot");
    EXPECT(h2.valid());
    MeshRenderer* mr2 = scene2.get(h2)->getComponent<MeshRenderer>();
    EXPECT(mr2 != nullptr);
    EXPECT(mr2->meshPath == "meshes/robot.gltf");
    EXPECT(mr2->texPath == texRel);                              // ref intacta
    EXPECT(mr2->texture == reinterpret_cast<const Texture*>(0x51));  // resolver ligou
    // e a textura extraída AINDA existe (dedup não reescreve nem apaga)
    std::vector<u8> stillThere;
    EXPECT(st.readBytes(texRel, stillThere));
    EXPECT(stillThere == png);
}
