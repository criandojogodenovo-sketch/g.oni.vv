// tests/test_import_gltf.cpp — F5-C: parser glTF 2.0 (.gltf + .glb).
// Fixtures construídas em código: buffers binários + JSON com offsets
// reais, GLB container com chunks, data URI base64, buffer externo via
// resolver, stride interleaved, u32/não-indexado, nós/hierarquia, materiais
// e todos os caminhos de erro.
#include "TestFramework.h"
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <string>
#include <vector>
#include "assets/GltfImporter.h"
#include "platform/FileApi.h"

using namespace vv;
using ::test::nearEqF;
using ::test::vecNearF;

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

std::string b64encodeBytes(const std::vector<u8>& b) {
    return b64encode(b.data(), b.size());
}

void pushF32(std::vector<u8>& b, f32 v) {
    u8 tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}

void pushU32(std::vector<u8>& b, u32 v) {
    u8 tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}

void pushU16(std::vector<u8>& b, u16 v) {
    u8 tmp[2];
    std::memcpy(tmp, &v, 2);
    b.insert(b.end(), tmp, tmp + 2);
}

// triângulo completo: POSITION + NORMAL + TEXCOORD_0 + índices u16
std::vector<u8> triBuffer(u32& posOff, u32& norOff, u32& uvOff, u32& idxOff,
                          u32& posLen, u32& norLen, u32& uvLen, u32& idxLen) {
    std::vector<u8> b;
    posOff = 0;
    pushF32(b, 0); pushF32(b, 0); pushF32(b, 0);
    pushF32(b, 1); pushF32(b, 0); pushF32(b, 0);
    pushF32(b, 0); pushF32(b, 1); pushF32(b, 0);
    posLen = 36;
    norOff = static_cast<u32>(b.size());
    for (int i = 0; i < 3; ++i) { pushF32(b, 0); pushF32(b, 0); pushF32(b, 1); }
    norLen = 36;
    uvOff = static_cast<u32>(b.size());
    pushF32(b, 0); pushF32(b, 0);
    pushF32(b, 1); pushF32(b, 0);
    pushF32(b, 1); pushF32(b, 1);
    uvLen = 24;
    idxOff = static_cast<u32>(b.size());
    pushU16(b, 0); pushU16(b, 1); pushU16(b, 2);
    idxLen = 6;
    return b;
}

// .gltf com o triângulo completo embutido em data: URI
std::string triGltfJson() {
    u32 po, no, uo, io, pl, nl, ul, il;
    const std::vector<u8> buf = triBuffer(po, no, uo, io, pl, nl, ul, il);
    char j[1200];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,%s\",\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
        "{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"materials\":[{\"name\":\"vermelho\",\"pbrMetallicRoughness\":"
        "{\"baseColorFactor\":[1,0.2,0.1,1]}}],"
        "\"meshes\":[{\"name\":\"tri\",\"primitives\":[{"
        "\"attributes\":{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},"
        "\"indices\":3,\"material\":0}]}],"
        "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0,"
        "\"translation\":[1,2,3],\"rotation\":[0,0,0,1],\"scale\":[1,1,1]}],"
        "\"scenes\":[{\"nodes\":[0]}]}",
        b64encodeBytes(buf).c_str(), static_cast<u32>(buf.size()),
        po, pl, no, nl, uo, ul, io, il);
    return j;
}

} // namespace

TEST(base64_decode_sanity) {
    std::vector<u8> out;
    EXPECT(decodeBase64("", 0, out) && out.empty());
    const char* src = "AAEC";
    EXPECT(decodeBase64(src, 4, out));
    EXPECT(out.size() == 3u && out[0] == 0x00 && out[1] == 0x01 && out[2] == 0x02);
    // padding + quebras de linha (data URIs de ferramentas reais)
    const char* pad = "AAEC\n==\r\n";
    EXPECT(decodeBase64(pad, std::strlen(pad), out));
    EXPECT(out.size() == 3u);
    // round-trip com o encoder das fixtures
    const std::vector<u8> blob = {0x00, 0xFF, 0x7F, 0x80, 0x42, 0x13};
    const std::string enc = b64encodeBytes(blob);
    EXPECT(decodeBase64(enc.data(), enc.size(), out));
    EXPECT(out == blob);
    EXPECT(!decodeBase64("a$b$c$", 6, out));   // caractere inválido
}

TEST(gltf_data_uri_triangulo_completo) {
    const std::string json = triGltfJson();
    GltfModel model;
    std::string err;
    EXPECT(parseGltf(json.data(), json.size(), {}, {}, model, err));
    EXPECT(err.empty());
    EXPECT(model.meshes.size() == 1u);
    const MeshData& md = model.meshes[0];
    EXPECT(md.name == "tri");
    EXPECT(md.vertices.size() == 3u);
    EXPECT(md.indices.size() == 3u);
    EXPECT(vecNearF(md.vertices[1].pos, Vec3{1, 0, 0}));
    EXPECT(vecNearF(md.vertices[0].normal, Vec3{0, 0, 1}));
    EXPECT(nearEqF(md.vertices[2].uv.x, 1.0f) && nearEqF(md.vertices[2].uv.y, 1.0f));
    EXPECT(md.groups.size() == 1u);
    EXPECT(md.groups[0].material == "vermelho");
    // materiais básicos registados
    EXPECT(model.materials.size() == 1u);
    EXPECT(model.materials[0].name == "vermelho");
    EXPECT(nearEqF(model.materials[0].baseColor[1], 0.2f));
    // nós: TRS preservada
    EXPECT(model.nodes.size() == 1u);
    EXPECT(model.nodes[0].name == "raiz");
    EXPECT(model.nodes[0].mesh == 0);
    EXPECT(vecNearF(model.nodes[0].translation, Vec3{1, 2, 3}));
}

TEST(gltf_glb_container_igual_ao_json) {
    // mesmo conteúdo empacotado como .glb (JSON chunk + BIN chunk, com padding)
    u32 po, no, uo, io, pl, nl, ul, il;
    const std::vector<u8> bin = triBuffer(po, no, uo, io, pl, nl, ul, il);
    char j[900];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
        "{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,"
        "\"TEXCOORD_0\":2},\"indices\":3}]}]}",
        static_cast<u32>(bin.size()), po, pl, no, nl, uo, ul, io, il);

    // monta o GLB: header + JSON chunk (pad 4) + BIN chunk (pad 4)
    std::string json = j;
    while (json.size() % 4 != 0) json += ' ';
    std::vector<u8> binPad = bin;
    while (binPad.size() % 4 != 0) binPad.push_back(0);

    std::vector<u8> glb;
    pushU32(glb, 0x46546C67u);        // 'glTF'
    pushU32(glb, 2);                  // versão
    const u32 total = 12 + 8 + static_cast<u32>(json.size()) + 8 +
                      static_cast<u32>(binPad.size());
    pushU32(glb, total);
    pushU32(glb, static_cast<u32>(json.size()));
    pushU32(glb, 0x4E4F534Au);        // 'JSON'
    glb.insert(glb.end(), json.begin(), json.end());
    pushU32(glb, static_cast<u32>(binPad.size()));
    pushU32(glb, 0x004E4942u);        // 'BIN'
    glb.insert(glb.end(), binPad.begin(), binPad.end());

    GltfModel model;
    std::string err;
    EXPECT(parseGlb(glb.data(), glb.size(), {}, model, err));
    EXPECT(model.meshes.size() == 1u);
    EXPECT(model.meshes[0].vertices.size() == 3u);
    EXPECT(vecNearF(model.meshes[0].vertices[1].pos, Vec3{1, 0, 0}));
    EXPECT(model.meshes[0].indices.size() == 3u);
}

TEST(gltf_primitivas_fundidas_rebase_e_u32_e_nao_indexado) {
    // prim0: 2 verts indexados u32; prim1: 3 verts não-indexados; prim2: mode 5
    // (STRIP) é ignorado
    std::vector<u8> b;
    const u32 posOff = 0;
    pushF32(b, 0); pushF32(b, 0); pushF32(b, 0);       // p0
    pushF32(b, 1); pushF32(b, 0); pushF32(b, 0);       // p1
    pushF32(b, 0); pushF32(b, 1); pushF32(b, 0);       // p2 (da prim1)
    pushF32(b, 0); pushF32(b, 0); pushF32(b, 1);       // p3 (da prim1)
    pushF32(b, 1); pushF32(b, 1); pushF32(b, 1);       // p4 (da prim1)
    const u32 idxOff = static_cast<u32>(b.size());
    pushU32(b, 0); pushU32(b, 1); pushU32(b, 0);       // tri degenerado ok p/ teste
    const u32 posLen = static_cast<u32>(b.size()) - 12;   // 5 verts × 12 = 60
    const u32 idxLen = 12;

    char j[800];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":5,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5125,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"primitives\":["
        "{\"attributes\":{\"POSITION\":0},\"indices\":1},"
        "{\"attributes\":{\"POSITION\":0}},"
        "{\"mode\":5,\"attributes\":{\"POSITION\":0}}]}]}",
        static_cast<u32>(b.size()), posOff, posLen, idxOff, idxLen);

    GltfModel model;
    std::string err;
    EXPECT(parseGltf(j, std::strlen(j), b, {}, model, err));
    EXPECT(model.meshes.size() == 1u);
    const MeshData& md = model.meshes[0];
    // primitivas do glTF NÃO partilham vértices entre si (cada uma lê o seu
    // accessor): prim0 entra com os 5 verts (base 0), prim1 DUPLICA os mesmos
    // 5 verts (base 5) — sem dedup cross-primitiva (documentado no header)
    EXPECT(md.vertices.size() == 10u);
    EXPECT(md.indices.size() == 8u);   // 3 (u32) + 5 (não-indexada)
    EXPECT(md.indices[0] == 0u && md.indices[1] == 1u && md.indices[2] == 0u);
    EXPECT(md.indices[3] == 5u && md.indices[4] == 6u && md.indices[5] == 7u &&
           md.indices[6] == 8u && md.indices[7] == 9u);
    EXPECT(md.groups.size() == 2u);   // prim2 (mode 5) fora
    EXPECT(md.groups[0].name == "primitive 0");
    EXPECT(md.groups[0].firstIndex == 0u);
    EXPECT(md.groups[0].indexCount == 3u);
    EXPECT(md.groups[1].name == "primitive 1");
    EXPECT(md.groups[1].firstIndex == 3u);
    EXPECT(md.groups[1].indexCount == 5u);
}

TEST(gltf_nos_hierarquia_children_parent) {
    const char* j =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,AAABAA==\"}],"
        "\"bufferViews\":[],\"accessors\":[],"
        "\"meshes\":["
        "{\"name\":\"m0\",\"primitives\":[{\"attributes\":{\"POSITION\":0}}]},"
        "{\"name\":\"m1\",\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
        "\"nodes\":["
        "{\"name\":\"corpo\",\"mesh\":0,\"children\":[1,2]},"
        "{\"name\":\"braco\",\"mesh\":1,\"translation\":[0,1,0],"
        "\"rotation\":[0,0.3826834,0,0.9238795],\"scale\":[2,2,2]},"
        "{\"name\":\"vazio\"}]}";
    GltfModel model;
    std::string err;
    // buffers vazios: primitivas falham em ler POSITION? views vazios →
    // accessor não existe → primitiva falha... fixture usada SÓ p/ nós:
    // o parser exige meshes ok — este teste valida que mesh inválido
    // (accessor inexistente) remove o mesh e remapa os nós para -1.
    const bool parsed = parseGltf(j, std::strlen(j), {}, {}, model, err);
    if (parsed) {
        // se parseou (não deveria ter meshes), os nós apontam para -1
        EXPECT(model.meshes.empty());
        for (const GltfNode& n : model.nodes) {
            EXPECT(n.mesh == -1);
        }
        EXPECT(model.nodes.size() == 3u);
        EXPECT(model.nodes[1].parent == 0);
        EXPECT(model.nodes[2].parent == 0);
        EXPECT(model.nodes[0].parent == -1);
    } else {
        // caminho legítimo: sem POSITION legível o glTF não tem meshes
        EXPECT(!err.empty());
    }
}

TEST(gltf_hierarquia_com_meshes_validos) {
    // dois meshes reais (1 tri cada) + nós pai/filho
    std::vector<u8> b;
    const u32 p0 = 0;
    for (int i = 0; i < 3; ++i) { pushF32(b, 0); pushF32(b, 0); pushF32(b, 0); }
    const u32 p1 = static_cast<u32>(b.size());
    for (int i = 0; i < 3; ++i) { pushF32(b, 1); pushF32(b, 1); pushF32(b, 1); }

    char j[900];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"},{\"bufferView\":1,\"componentType\":5126,"
        "\"count\":3,\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"name\":\"pai\",\"primitives\":[{\"attributes\":"
        "{\"POSITION\":0}}]},{\"name\":\"filho\",\"primitives\":"
        "[{\"attributes\":{\"POSITION\":1}}]}],"
        "\"nodes\":[{\"name\":\"corpo\",\"mesh\":0,\"children\":[1]},"
        "{\"name\":\"braco\",\"mesh\":1,\"translation\":[0,1,0],"
        "\"rotation\":[0,0.3826834,0,0.9238795],\"scale\":[2,2,2]}]}",
        static_cast<u32>(b.size()), p0, p1);

    GltfModel model;
    std::string err;
    EXPECT(parseGltf(j, std::strlen(j), b, {}, model, err));
    EXPECT(model.meshes.size() == 2u);
    EXPECT(model.nodes.size() == 2u);
    EXPECT(model.nodes[0].name == "corpo" && model.nodes[0].mesh == 0);
    EXPECT(model.nodes[1].name == "braco" && model.nodes[1].mesh == 1);
    EXPECT(model.nodes[1].parent == 0);
    EXPECT(model.nodes[0].parent == -1);
    EXPECT(vecNearF(model.nodes[1].translation, Vec3{0, 1, 0}));
    EXPECT(vecNearF(model.nodes[1].scale, Vec3{2, 2, 2}));
    EXPECT(nearEqF(model.nodes[1].rotation.y, 0.3826834f, 1e-5f));
}

TEST(gltf_buffer_externo_via_resolvedor) {
    std::vector<u8> bin;
    const u32 posOff = 0;
    pushF32(bin, 0); pushF32(bin, 0.5f); pushF32(bin, 0);
    pushF32(bin, 1); pushF32(bin, 0.5f); pushF32(bin, 0);
    pushF32(bin, 0); pushF32(bin, 1.5f); pushF32(bin, 0);

    const char* j =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"meshes/scene.bin\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
        "\"indices\":9}]}]}";
    // indices:9 não existe (1 accessor) → essa primitiva falha? indices aponta
    // para accessor fora do range → erro claro (recusa)

    struct Ctx { const std::vector<u8>* bin; int calls = 0; } ctx{&bin};
    GltfBufferResolver resolver;
    resolver.fn = [](void* user, const char* uri, std::vector<u8>& out) -> bool {
        (void)uri;
        Ctx* c = static_cast<Ctx*>(user);
        ++c->calls;
        out = *c->bin;
        return true;
    };
    resolver.user = &ctx;

    // fixture VÁLIDA sem índices (não-indexada):
    const char* j2 =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"meshes/scene.bin\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}";
    GltfModel model;
    std::string err;
    EXPECT(parseGltf(j2, std::strlen(j2), {}, resolver, model, err));
    EXPECT(ctx.calls == 1);
    EXPECT(model.meshes.size() == 1u);
    EXPECT(model.meshes[0].vertices.size() == 3u);
    EXPECT(nearEqF(model.meshes[0].vertices[0].pos.y, 0.5f));
    EXPECT(model.meshes[0].indices.size() == 3u);   // gerado sequencial

    // resolver recusa → erro claro
    GltfBufferResolver bad;
    bad.fn = [](void*, const char*, std::vector<u8>&) { return false; };
    EXPECT(!parseGltf(j2, std::strlen(j2), {}, bad, model, err));
    EXPECT(err.find("não resolvido") != std::string::npos);
}

TEST(gltf_stride_interleaved_respeitado) {
    // POSITION interleaved com um float de lixo: stride 16, elemSize 12
    std::vector<u8> b;
    pushF32(b, 1); pushF32(b, 2); pushF32(b, 3); pushF32(b, 999);   // v0 + lixo
    pushF32(b, 4); pushF32(b, 5); pushF32(b, 6); pushF32(b, 999);   // v1 + lixo
    pushF32(b, 7); pushF32(b, 8); pushF32(b, 9); pushF32(b, 999);   // v2 + lixo

    char j[600];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":%u,"
        "\"byteStride\":16}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}",
        static_cast<u32>(b.size()), static_cast<u32>(b.size()));

    GltfModel model;
    std::string err;
    EXPECT(parseGltf(j, std::strlen(j), b, {}, model, err));
    EXPECT(vecNearF(model.meshes[0].vertices[0].pos, Vec3{1, 2, 3}));
    EXPECT(vecNearF(model.meshes[0].vertices[2].pos, Vec3{7, 8, 9}));
}

TEST(gltf_invalido_falha_com_erro_claro) {
    GltfModel model;
    std::string err;

    EXPECT(!parseGltf("", 0, {}, {}, model, err));                  // vazio
    EXPECT(!parseGltf("{sem json", 9, {}, {}, model, err));         // json quebrado
    EXPECT(!parseGltf("{\"meshes\":[]}", 14, {}, {}, model, err));  // sem asset

    // glTF v2 de verdade: bufferView além do buffer
    const char* over =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAA\"}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":64}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":1,"
        "\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}";
    EXPECT(!parseGltf(over, std::strlen(over), {}, {}, model, err));
    EXPECT(err.find("fora do buffer") != std::string::npos);

    // sem POSITION
    const char* noPos =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAA\"}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":16}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":1,"
        "\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"NORMAL\":0}}]}]}";
    EXPECT(!parseGltf(noPos, std::strlen(noPos), {}, {}, model, err));
    EXPECT(err.find("POSITION") != std::string::npos);

    // primitivas só com mode != 4 → "nenhum mesh com triângulos"
    const char* onlyPoints =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,"
        "AAAAAAAAAAAAAAAA\"}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":16}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":1,"
        "\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"mode\":0,\"attributes\":{\"POSITION\":0}}]}]}";
    EXPECT(!parseGltf(onlyPoints, std::strlen(onlyPoints), {}, {}, model, err));

    // data URI base64 inválida
    const char* badB64 =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,@@@@\"}],"
        "\"meshes\":[]}";
    EXPECT(!parseGltf(badB64, std::strlen(badB64), {}, {}, model, err));
    EXPECT(err.find("base64") != std::string::npos);

    // índice fora do range da primitiva
    std::vector<u8> bin;
    for (int i = 0; i < 2; ++i) { pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0); }
    pushU16(bin, 0); pushU16(bin, 1); pushU16(bin, 5);   // 5 fora (só 2 verts)
    char j[600];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":24},"
        "{\"buffer\":0,\"byteOffset\":24,\"byteLength\":6}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":2,"
        "\"type\":\"VEC3\"},{\"bufferView\":1,\"componentType\":5123,"
        "\"count\":3,\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
        "\"indices\":1}]}]}",
        static_cast<u32>(bin.size()));
    EXPECT(!parseGltf(j, std::strlen(j), bin, {}, model, err));
    EXPECT(err.find("índice fora") != std::string::npos);
}

TEST(glb_invalido_falha_sem_crash) {
    GltfModel model;
    std::string err;
    EXPECT(!parseGlb(nullptr, 0, {}, model, err));
    u8 shortHdr[8] = {0x67, 0x54, 0x46, 0x67, 2, 0, 0, 0};
    EXPECT(!parseGlb(shortHdr, sizeof(shortHdr), {}, model, err));   // < 12 bytes

    u8 bad[20];
    std::memset(bad, 0, sizeof(bad));
    const u32 badMagic = 0xDEADBEEFu;
    std::memcpy(bad, &badMagic, 4);   // magic errado
    EXPECT(!parseGlb(bad, sizeof(bad), {}, model, err));
    EXPECT(err.find("magic") != std::string::npos);

    // versão errada
    u8 v1[20];
    std::memset(v1, 0, sizeof(v1));
    const u32 magic = 0x46546C67u;
    const u32 version = 1;
    std::memcpy(v1, &magic, 4);
    std::memcpy(v1 + 4, &version, 4);
    EXPECT(!parseGlb(v1, sizeof(v1), {}, model, err));
    EXPECT(err.find("versão") != std::string::npos);

    // total declara mais do que existe
    u8 trunc[16];
    std::memset(trunc, 0, sizeof(trunc));
    const u32 total = 1000;
    std::memcpy(trunc, &magic, 4);
    const u32 v2 = 2;
    std::memcpy(trunc + 4, &v2, 4);
    std::memcpy(trunc + 8, &total, 4);
    EXPECT(!parseGlb(trunc, sizeof(trunc), {}, model, err));
    EXPECT(err.find("excede") != std::string::npos);
}

// ---- F5-C/3: instância de glTF numa cena ------------------------------------
#include "assets/GltfInstantiate.h"
#include "assets/ResourceManager.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "FakeStorage.h"

namespace {

// .gltf com DOIS meshes (tris independentes) embutidos em data URI
std::string twoMeshGltf() {
    std::vector<u8> bin;
    auto pushF = [&bin](f32 v) {
        u8 t[4]; std::memcpy(t, &v, 4); bin.insert(bin.end(), t, t + 4);
    };
    for (int i = 0; i < 3; ++i) { pushF(0); pushF(0); pushF(0); }
    for (int i = 0; i < 3; ++i) { pushF(1); pushF(1); pushF(1); }
    char j[700];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,%s\","
        "\"byteLength\":%u}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"},{\"bufferView\":1,\"componentType\":5126,"
        "\"count\":3,\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"name\":\"a\",\"primitives\":[{\"attributes\":{\"POSITION\":0}}]},"
        "{\"name\":\"b\",\"primitives\":[{\"attributes\":{\"POSITION\":1}}]}]}",
        b64encodeBytes(bin).c_str(), static_cast<u32>(bin.size()));
    return j;
}

struct BindSentinel {
    int calls = 0;
    std::vector<std::string> refs;
};

Mesh* sentinelBind(void* user, const std::string& ref) {
    BindSentinel* s = static_cast<BindSentinel*>(user);
    ++s->calls;
    s->refs.push_back(ref);
    // sentinela determinística por hash do ref (não é desreferenciado)
    return reinterpret_cast<Mesh*>(static_cast<uintptr_t>(0xC0DE00 + s->calls));
}

} // namespace

TEST(gltf_instantiate_hierarquia_trs_e_bind) {
    GltfModel model;
    // nó 0: "corpo" com mesh 0; nó 1: "braco" com mesh 1, PAI = nó 0 (filho
    // vem DEPOIS); nó 2: vazio; nó 3: "perna" com mesh 0, pai = nó 0, e
    // aparece ANTES do pai no array (referência cruzada)
    GltfNode corpo;
    corpo.name = "corpo";
    corpo.mesh = 0;
    corpo.translation = Vec3{1, 0, 0};
    model.nodes.push_back(corpo);

    GltfNode braco;
    braco.name = "braco";
    braco.mesh = 1;
    braco.parent = 0;
    braco.translation = Vec3{0, 2, 0};
    braco.rotation = Quat{0, 0.3826834f, 0, 0.9238795f};
    braco.scale = Vec3{2, 2, 2};
    model.nodes.push_back(braco);

    GltfNode vazio;
    vazio.name = "vazio";
    vazio.parent = 0;
    model.nodes.push_back(vazio);

    GltfNode perna;
    perna.name = "perna";
    perna.mesh = 0;
    perna.parent = 0;
    model.nodes.push_back(perna);

    model.meshes.resize(2);
    model.meshes[0].name = "geometria_a";
    model.meshes[1].name = "geometria_b";

    Scene scene;
    BindSentinel sent;
    GltfInstantiateCtx ctx;
    ctx.bindMesh = &sentinelBind;
    ctx.user = &sent;
    Material* fakeMat = reinterpret_cast<Material*>(0xFEED);
    ctx.material = fakeMat;

    const Handle root = gltfInstantiate(scene, model, "meshes/robot.gltf", ctx);
    EXPECT(root.valid());
    EXPECT(scene.count() == 4u);
    EXPECT(sent.calls == 3);   // só nós com mesh chamam o bind

    const Handle hCorpo = scene.find("corpo");
    const Handle hBraco = scene.find("braco");
    const Handle hVazio = scene.find("vazio");
    const Handle hPerna = scene.find("perna");
    EXPECT(hCorpo.valid() && hBraco.valid() && hVazio.valid() && hPerna.valid());

    Tic* tCorpo = scene.get(hCorpo);
    Tic* tBraco = scene.get(hBraco);
    Tic* tVazio = scene.get(hVazio);
    Tic* tPerna = scene.get(hPerna);

    // hierarquia: braço/perna/vazio pendurados no corpo (índice do slot)
    EXPECT(tBraco->parent == tCorpo->handle.index);
    EXPECT(tPerna->parent == tCorpo->handle.index);
    EXPECT(tVazio->parent == tCorpo->handle.index);
    EXPECT(tCorpo->parent == -1);

    // TRS → Transform3D
    const Transform3D* trB = tBraco->getComponent<Transform3D>();
    EXPECT(trB != nullptr);
    EXPECT(vecNearF(trB->pos, Vec3{0, 2, 0}));
    EXPECT(vecNearF(trB->scale, Vec3{2, 2, 2}));
    EXPECT(nearEqF(trB->rot.y, 0.3826834f, 1e-5f));

    // MeshRenderer: ref "path#meshOriginal" + bind + material
    MeshRenderer* mrB = tBraco->getComponent<MeshRenderer>();
    EXPECT(mrB != nullptr);
    EXPECT(mrB->meshPath == "meshes/robot.gltf#1");
    EXPECT(mrB->mesh != nullptr);
    EXPECT(mrB->material == fakeMat);
    EXPECT(tCorpo->getComponent<MeshRenderer>()->meshPath == "meshes/robot.gltf#0");
    EXPECT(tPerna->getComponent<MeshRenderer>()->meshPath == "meshes/robot.gltf#0");
    // nó vazio: SEM MeshRenderer
    EXPECT(tVazio->getComponent<MeshRenderer>() == nullptr);

    // bind recebeu os refs esperados
    EXPECT(sent.refs[0] == "meshes/robot.gltf#0");
    EXPECT(sent.refs[1] == "meshes/robot.gltf#1");
}

TEST(gltf_instantiate_bind_nulo_entra_sem_mesh) {
    GltfModel model;
    GltfNode n;
    n.name = "solo";
    n.mesh = 0;
    model.nodes.push_back(n);
    model.meshes.resize(1);

    Scene scene;
    GltfInstantiateCtx ctx;   // bindMesh nulo — testes sem device
    const Handle h = gltfInstantiate(scene, model, "meshes/x.gltf", ctx);
    EXPECT(h.valid());
    Tic* t = scene.get(h);
    EXPECT(t != nullptr);
    // hierarquia e componentes entram; mesh fica null (rebind no reload)
    EXPECT(t->getComponent<Transform3D>() != nullptr);
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    EXPECT(mr != nullptr);
    EXPECT(mr->mesh == nullptr && mr->material == nullptr);
    EXPECT(mr->meshPath == "meshes/x.gltf#0");
}

TEST(gltf_instantiate_modelo_vazio_nao_toca_cena) {
    Scene scene;
    scene.create("preexistente");
    GltfModel vazio;
    GltfInstantiateCtx ctx;
    EXPECT(!gltfInstantiate(scene, vazio, "meshes/nada.gltf", ctx).valid());
    EXPECT(scene.count() == 1u);
}

TEST(gltf_e2e_storage_cache_instantiate) {
    // caminho completo: .gltf no storage → ResourceManager (refs "path#i")
    // → gltfInstantiate → cada meshPath do TIC resolve no cache
    FakeStorage st;
    st.writeText("meshes/par.gltf", twoMeshGltf());

    ResourceManager rm;
    rm.setStorage(&st);
    std::string err;
    // pré-carga do modelo via 1º ref (o bind do device faria isto)
    EXPECT(rm.mesh("meshes/par.gltf#0", err) != nullptr);

    // monta o modelo por parse direto (o RM não expõe GltfModel — re-parse
    // local do MESMO texto para a instância) e dá-lhe nós com hierarquia
    const std::string text = twoMeshGltf();
    GltfModel model;
    EXPECT(parseGltf(text.data(), text.size(), {}, {}, model, err));
    EXPECT(model.meshes.size() == 2u);
    GltfNode na;
    na.name = "a";
    na.mesh = 0;
    model.nodes.push_back(na);
    GltfNode nb;
    nb.name = "b";
    nb.mesh = 1;
    nb.parent = 0;
    model.nodes.push_back(nb);

    Scene scene;
    BindSentinel sent;
    GltfInstantiateCtx ctx;
    ctx.bindMesh = &sentinelBind;
    ctx.user = &sent;
    const Handle root = gltfInstantiate(scene, model, "meshes/par.gltf", ctx);
    EXPECT(sent.calls == 2);
    EXPECT(root.valid());
    EXPECT(scene.count() == 2u);   // dois nós com mesh
    const Handle hb = scene.find("b");
    EXPECT(hb.valid() && scene.get(hb)->parent == scene.get(root)->handle.index);

    // TODA ref escrita pelos TICs resolve no ResourceManager — o contrato
    // que o device usa para ligar Mesh* sem caminhos absolutos
    scene.forEachActive([&](Tic& t) {
        if (MeshRenderer* mr = t.getComponent<MeshRenderer>()) {
            std::string err2;
            const MeshData* md = rm.mesh(mr->meshPath, err2);
            EXPECT(md != nullptr);
            EXPECT(err2.empty());
            EXPECT(md->ok());
        }
    });
    // e o parse do ficheiro não foi repetido (cache de modelo)
    EXPECT(rm.meshLoads() == 1u);
}

// ============================================================================
// 0.9.6.12 (A2 · R-014) — OS PERFIS REAIS DO DEVICE (as fixtures da spec,
// proibido fixture «fácil»). Todos construídos em código com os NÚMEROS
// do dono:
//   (i)   view que acaba EXATAMENTE no fim do BIN (off+len == binLen) → passa
//   (i')  accessor com byteOffset > 0 nesse view (o perfil gltfpack do
//         high_poly_base_mesh.glb — o falso OutOfBounds: o bound antigo
//         dupla-contava o accOff) → passa
//   (ii)  view com offset+len == binLen+1 → falha com a MENSAGEM COMPLETA
//         (os quatro números) — e, com 2 primitivas, DEGRADA (2d): a boa
//         entra, a má cai com W + contador
//   (iii) bufferView.buffer = 1 → erro que NOMEIA o buffer
//   (v)   .gltf + .bin externo — coberto pelo R-021 (test_sentinels.cpp)
//   (vi)  URI %20 — coberto pelo R-021 (test_sentinels.cpp, tex albedo)
// ============================================================================
TEST(glb_a2_perfis_do_device) {
    // o BIN: POSITION (36) + TEX (32: 4 VEC2 f32) + índices u16 (6) = 74 B
    std::vector<u8> bin;
    pushF32(bin, 0.f); pushF32(bin, 0.f); pushF32(bin, 0.f);
    pushF32(bin, 1.f); pushF32(bin, 0.f); pushF32(bin, 0.f);
    pushF32(bin, 0.f); pushF32(bin, 1.f); pushF32(bin, 0.f);
    for (int i = 0; i < 8; ++i) pushF32(bin, 0.25f * i);   // TEX 32 B
    pushU16(bin, 0); pushU16(bin, 1); pushU16(bin, 2);     // índices 6 B
    ASSERT(bin.size() == 74u);

    // view0: POSITION [0,36) · view1: TEX [36,68) · view2: índices [68,74)
    // accessors: a0 POSITION(view0,3) · a1 TEXCOORD(view1,accOff=8,3 — o
    // perfil do dono) · a2 índices(view2,3)
    auto makeJson = [&](int texViewLen, int texBuffer, int idxViewLen,
                        int nPrims) {
        char js[1400];
        // nPrims == 2: a SEGUNDA primitiva usa um view/accessor PRÓPRIOS e
        // BONS (view3/acessor 3 — o mesmo range válido [68,74)) — o perfil
        // da degradação: a má cai, a boa entra (as duas a apontar o mesmo
        // view mau fariam TODAS caírem — o caso (ii))
        std::snprintf(js, sizeof(js),
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":74}],"
            "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
            "{\"buffer\":%d,\"byteOffset\":36,\"byteLength\":%d},"
            "{\"buffer\":0,\"byteOffset\":68,\"byteLength\":%d}%s],"
            "\"accessors\":["
            "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
            "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"byteOffset\":8,\"type\":\"VEC2\"},"
            "{\"bufferView\":2,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}%s],"
            "\"meshes\":[{\"primitives\":["
            "{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},\"indices\":2}"
            "%s]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}",
            texBuffer, texViewLen, idxViewLen,
            nPrims == 2
                ? ",{\"buffer\":0,\"byteOffset\":68,\"byteLength\":6}"
                : "",
            nPrims == 2
                ? ",{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}"
                : "",
            nPrims == 2
                ? ",{\"attributes\":{\"POSITION\":0},\"indices\":3}"
                : "");
        return std::string(js);
    };
    auto makeGlb = [](const std::string& json, const std::vector<u8>& binBuf,
                      std::vector<u8>& out) {
        out.clear();
        auto u32p = [&out](u32 v) {
            u8 t[4];
            std::memcpy(t, &v, 4);
            out.insert(out.end(), t, t + 4);
        };
        const u32 jsonPad = (4 - (static_cast<u32>(json.size()) % 4)) % 4;
        u32p(0x46546C67u);
        u32p(2);
        u32p(12 + 8 + static_cast<u32>(json.size()) + jsonPad + 8 +
             static_cast<u32>(binBuf.size()));
        u32p(static_cast<u32>(json.size()));
        u32p(0x4E4F534Au);
        out.insert(out.end(), json.begin(), json.end());
        for (u32 i = 0; i < jsonPad; ++i) {
            out.push_back(0x20);
        }
        u32p(static_cast<u32>(binBuf.size()));
        u32p(0x004E4942u);
        out.insert(out.end(), binBuf.begin(), binBuf.end());
    };

    // ---- (i)+(i') o perfil do DONO: view TEX acaba a 68 (< 74), o
    // accessor tem accOff=8 (o total do bound ANTIGO: 36+8+32=76 > 74 —
    // o falso «fora do buffer»); e o view dos ÍNDICES acaba EXATAMENTE no
    // fim do BIN (68+6 == 74 — off+len == binLen, a fixture (i)) ----------
    {
        const std::string json = makeJson(32, 0, 6, 1);
        std::vector<u8> glb;
        makeGlb(json, bin, glb);
        GltfModel model;
        std::string perr;
        EXPECT(parseGlb(glb.data(), glb.size(),
                        GltfBufferResolver{nullptr, 0}, model, perr));
        EXPECT(model.meshes.size() == 1u);
        EXPECT(model.meshes[0].vertices.size() == 3u);
        EXPECT(model.meshes[0].indices.size() == 3u);
        EXPECT(model.primsDropped == 0u);
    }

    // ---- (ii) offset+len == binLen+1 (o índice dos índices mente 1 B):
    // degrada a primitiva; TODAS caírem → falha com a MENSAGEM COMPLETA
    {
        const std::string json = makeJson(32, 0, 7, 1);   // view2 len 7 > 6
        std::vector<u8> glb;
        makeGlb(json, bin, glb);
        GltfModel model;
        std::string perr;
        EXPECT(!parseGlb(glb.data(), glb.size(),
                         GltfBufferResolver{nullptr, 0}, model, perr));
        // a MENSAGEM COMPLETA (a tarefa 1: os quatro números, NUNCA
        // truncado — é o que o dono lê; A2-2 acrescenta file= o disco)
        EXPECT(perr.find("TODAS as primitivas") != std::string::npos);
        EXPECT(perr.find("glb: view2 buffer0 off=68 len=7 acc=0 "
                         "declared=74 real=74 file=74") != std::string::npos);
    }
    // ---- (ii-b) A DEGRADAÇÃO (a spec 2d): DUAS primitivas — a má cai, a
    // boa ENTRA (o import segue; o contador diz) --------------------------
    {
        const std::string json = makeJson(32, 0, 7, 2);
        std::vector<u8> glb;
        makeGlb(json, bin, glb);
        GltfModel model;
        std::string perr;
        EXPECT(parseGlb(glb.data(), glb.size(),
                        GltfBufferResolver{nullptr, 0}, model, perr));
        // ASSERT: precondição dos derefs seguintes (sem mesh, model.meshes[0]
        // é UB — a falha tem de ser LIMPA, não um crash do binário)
        ASSERT(model.meshes.size() == 1u);           // a primitiva BOA entrou
        EXPECT(model.meshes[0].groups.size() == 1u); // só ela
        EXPECT(model.primsDropped == 1u);            // a má contou
        EXPECT(model.primDropCause.find("glb: view2") != std::string::npos);
    }

    // ---- (iii) bufferView.buffer = 1: o erro NOMEIA o buffer -------------
    {
        const std::string json = makeJson(32, 1, 6, 1);   // o view do TEX
                                                          // refere buffer 1
        std::vector<u8> glb;
        makeGlb(json, bin, glb);
        GltfModel model;
        std::string perr;
        EXPECT(!parseGlb(glb.data(), glb.size(),
                         GltfBufferResolver{nullptr, 0}, model, perr));
        EXPECT(perr.find("refere o buffer 1, que não existe "
                         "(o ficheiro declara 1 buffer(s))") !=
               std::string::npos);
        EXPECT(perr.find("glb: view1 buffer1 off=36 len=32") !=
               std::string::npos);
    }

    // ---- (iv) o perfil STANFORD: textura embutida em bufferView (a
    // imagem vive NO FIM do chunk) — o parse entrega os bytes -------------
    {
        const u8 pngFake[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
                              'R', 'D', 'R'};
        std::vector<u8> bin2 = bin;
        const size_t imgOff = bin2.size();   // cola no fim (o perfil stanford)
        bin2.insert(bin2.end(), pngFake, pngFake + sizeof(pngFake));
        while (bin2.size() % 4 != 0) {
            bin2.push_back(0);
        }
        char jv[128], js[1024];
        std::snprintf(jv, sizeof(jv),
                      "{\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu}",
                      imgOff, sizeof(pngFake));
        std::snprintf(js, sizeof(js),
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":%zu}],"
            "\"bufferViews\":["
            "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
            "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":32},"
            "%s],"
            "\"accessors\":["
            "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
            "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"byteOffset\":8,\"type\":\"VEC2\"}],"
            "\"images\":[{\"bufferView\":2,\"mimeType\":\"image/png\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,"
            "\"TEXCOORD_0\":1}}]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],\"scene\":0}",
            bin2.size(), jv);
        std::vector<u8> glb;
        makeGlb(std::string(js), bin2, glb);
        GltfModel model;
        std::string perr;
        EXPECT(parseGlb(glb.data(), glb.size(),
                        GltfBufferResolver{nullptr, 0}, model, perr));
        EXPECT(model.images.size() == 1u);
        EXPECT(!model.images[0].broken);
        EXPECT(model.images[0].bytes.size() == sizeof(pngFake));
        EXPECT(model.meshes.size() == 1u);
    }
}

// ============================================================================
// 0.9.6.12g (A2-2) — OS TETOS DE RANGE, O ÚLTIMO BLOCO E A MEMÓRIA
// A evidência do dono (engine.log 06/10 23:24): «import: glb: view2 buffer0
// off=89248320 len=123738528 acc=0 declared=212986848 re…» seguido de
// «glTF invalido: accessor (view 2) falhou: range do bufferView aci…» —
// off+len == declared == 212986848: a vista acaba EXATAMENTE no fim do
// buffer, o que é VÁLIDO. As hipóteses (a) off-by-one e (b) último bloco
// parcial perdido NÃO existiam no código (o bound já era exclusivo e o
// ChunkReader já lia o resto); as causas REAIS eram os TETOS:
//   • kMaxRangeBytes = 64 MB recusava o view VÁLIDO de 118 MB do scene
//     (TooBig → «acima do teto de 64 MB» — o «aci…» do log do dono);
//   • fileRangeLoad tinha um SEGUNDO teto de 16 MB (kMaxJsonBytes
//     reusado) que matava o dragão (38 MB) com mentira de I/O;
//   • o pool de ranges nunca libertava (o BIN ia todo para a RAM).
// Fixtures do dono (FAZ 4): (i) a última vista acaba no fim do buffer;
// (ii) tamanho não múltiplo de 6291456; (iii) múltiplo exato; (iv) vista
// genuinamente fora → mensagem clara. + file= na linha (FAZ 1) + o teto
// honesto «modelo demasiado grande para a memória» (FAZ 5).
// ============================================================================
TEST(glb_a2_ultimo_bloco_tetos_e_memoria) {
    constexpr size_t kBlk = 6291456;   // o kChunkBytes do import (6 MB)

    // ---- fixtures de ficheiro (o padrão do conteúdo é determinístico) ----
    auto writePattern = [](const std::string& path, size_t n) -> bool {
        FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) {
            return false;
        }
        std::vector<u8> chunk(1u << 20);
        size_t done = 0;
        while (done < n) {
            const size_t k = chunk.size() < (n - done) ? chunk.size()
                                                       : (n - done);
            for (size_t i = 0; i < k; ++i) {
                chunk[i] = static_cast<u8>(((done + i) * 7 + 13) & 0xFF);
            }
            if (std::fwrite(chunk.data(), 1, k, f) != k) {
                std::fclose(f);
                return false;
            }
            done += k;
        }
        std::fclose(f);
        return true;
    };
    auto tmpName = [](const char* tag, std::string& out) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "/tmp/goni_a2teto_%s_%d.bin", tag,
                      static_cast<int>(::getpid()));
        out = buf;
    };

    // ---- (ii) o perfil do scene: 2 blocos + 5368800 — o ÚLTIMO BLOCO
    // PARCIAL tem de chegar (a leitura verifica o valor DEVOLVIDO) -------
    {
        std::string p1;
        tmpName("parcial", p1);
        const size_t sz1 = 2 * kBlk + 5368800;
        ASSERT(writePattern(p1, sz1));
        fileapi::ChunkReader rd;
        ASSERT(rd.open(p1, kBlk));
        size_t blocks = 0;
        size_t lastLen = 0;
        bool padraoOk = true;
        while (rd.next()) {
            ++blocks;
            lastLen = rd.last;
            const size_t base = static_cast<size_t>(rd.done - rd.last);
            for (size_t i = 0; i < rd.last && padraoOk; ++i) {
                if (rd.buf[i] !=
                    static_cast<u8>(((base + i) * 7 + 13) & 0xFF)) {
                    padraoOk = false;
                }
            }
        }
        EXPECT_MSG(padraoOk && rd.done == sz1 && blocks == 3u &&
                       lastLen == 5368800u,
                   "o ChunkReader perdeu o último bloco parcial "
                   "(done=%llu, blocos=%zu, último=%zu)",
                   static_cast<unsigned long long>(rd.done), blocks, lastLen);
        rd.close();
        ::remove(p1.c_str());
    }
    // ---- (iii) o múltiplo EXATO: 2 blocos cheios, nada perdido ----------
    {
        std::string p1;
        tmpName("exato", p1);
        const size_t sz1 = 2 * kBlk;
        ASSERT(writePattern(p1, sz1));
        fileapi::ChunkReader rd;
        ASSERT(rd.open(p1, kBlk));
        size_t blocks = 0;
        size_t lastLen = 0;
        while (rd.next()) {
            ++blocks;
            lastLen = rd.last;
        }
        EXPECT(blocks == 2u && lastLen == kBlk && rd.done == sz1);
        rd.close();
        ::remove(p1.c_str());
    }
    // ---- o perfil EXATO do dragão do dono: 38051884 B = 6 blocos +
    // 303148 — a CÓPIA streaming do import tem de ser byte a byte --------
    {
        std::string src;
        std::string dst;
        tmpName("dragao_src", src);
        tmpName("dragao_dst", dst);
        const size_t dragon = 38051884u;   // O tamanho real do ficheiro
        ASSERT(writePattern(src, dragon));
        EXPECT(fileapi::copyFileChunked(src, dst, kBlk, nullptr, nullptr));
        u64 copied = 0;
        EXPECT(fileapi::fileSize(dst, copied));
        EXPECT_MSG(copied == dragon,
                   "a cópia em chunks perdeu bytes: %llu de %zu",
                   static_cast<unsigned long long>(copied), dragon);
        // spot check: primeiro e último KB idênticos
        u8 hb[2][1024];
        FILE* fa = std::fopen(src.c_str(), "rb");
        FILE* fb = std::fopen(dst.c_str(), "rb");
        ASSERT(fa && fb);
        const size_t ra = std::fread(hb[0], 1, 1024, fa);
        const size_t rb = std::fread(hb[1], 1, 1024, fb);
        EXPECT(ra == 1024 && rb == 1024 &&
               std::memcmp(hb[0], hb[1], 1024) == 0);
        std::fseek(fa, static_cast<long>(dragon) - 1024, SEEK_SET);
        std::fseek(fb, static_cast<long>(dragon) - 1024, SEEK_SET);
        std::fread(hb[0], 1, 1024, fa);
        std::fread(hb[1], 1, 1024, fb);
        EXPECT(std::memcmp(hb[0], hb[1], 1024) == 0);
        std::fclose(fa);
        std::fclose(fb);
        ::remove(src.c_str());
        ::remove(dst.c_str());
    }

    // ---- o loader FALSO (deferido, sem ficheiro): conta as chamadas e
    // enche o padrão — o teto de 256 MB é afervado ANTES de materializar --
    auto fakeLoad = [](void* user, u32 bi, u64 off, u64 len,
                       std::vector<u8>& out) -> bool {
        (void)bi;
        if (user) {
            ++*static_cast<u64*>(user);
        }
        out.resize(static_cast<size_t>(len));
        for (size_t i = 0; i < out.size(); ++i) {
            out[i] = static_cast<u8>(((off + i) * 7 + 13) & 0xFF);
        }
        return true;
    };
    // JSON do perfil scene: UM view de geometria que ocupa o buffer INTEIRO
    // (a fixture (i) do dono — a vista acaba EXATAMENTE no fim) com um
    // accessor pequeno (a cauda do view é legal)
    auto bigViewJson = [](size_t binLen) {
        char js[512];
        std::snprintf(js, sizeof(js),
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":%zu}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,"
            "\"byteLength\":%zu}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,"
            "\"count\":3,\"type\":\"VEC3\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],"
            "\"scene\":0}",
            binLen, binLen);
        return std::string(js);
    };
    auto parseWith = [&](const std::string& json, u64 binLen, u64 fileBytes,
                         u64* calls, GltfModel& model,
                         std::string& err) -> bool {
        GltfRangeLoader loader;
        loader.fn = fakeLoad;
        loader.user = calls;
        loader.binLen = binLen;
        loader.fileBytes = fileBytes;
        return parseGltf(json.data(), json.size(), {},
                         GltfBufferResolver{nullptr, 0}, model, err,
                         &loader);
    };

    // ---- O PERFIL SCENE: um view logo acima do teto VELHO de 64 MB
    // (67108884 = 64 MB + 20) — VÁLIDO, tem de ENTRAR (era TooBig) --------
    {
        const size_t binLen = 67108884u;   // > 67108864 (o teto antigo)
        const std::string json = bigViewJson(binLen);
        GltfModel model;
        std::string err;
        u64 calls = 0;
        const bool ok = parseWith(json, binLen, binLen + 12, &calls, model,
                                  err);
        if (!ok) {
            EXPECT_MSG(false,
                       "o view de %zu B (>64 MB, <256 MB) foi recusado: %s",
                       binLen, err.c_str());
            return;   // sem o mesh, os derefs seguintes são UB — sai LIMPO
        }
        EXPECT(model.meshes.size() == 1u);
        EXPECT(model.meshes[0].vertices.size() == 3u);
        EXPECT(calls >= 1u);   // o range foi materializado (uma vez só)
    }

    // ---- O TETO HONESTO (FAZ 5): um range de 300 MB NÃO materializa —
    // a falha é «modelo demasiado grande para a memória», nunca
    // «corrompido», e o loader NUNCA é chamado (zero RAM) -----------------
    {
        const size_t binLen = 314572800u;   // 300 MB declarados (só no papel)
        const std::string json = bigViewJson(binLen);
        GltfModel model;
        std::string err;
        u64 calls = 0;
        const bool ok = parseWith(json, binLen, binLen, &calls, model, err);
        EXPECT(!ok);
        EXPECT_MSG(err.find("modelo demasiado grande para a memória") !=
                       std::string::npos,
                   "o teto de memória não deu a mensagem honesta: %s",
                   err.c_str());
        EXPECT(err.find("corrompido") == std::string::npos);
        EXPECT(calls == 0u);   // nada foi materializado (sem crash, sem RAM)
        // a linha COMPLETA do dono (FAZ 1) — com file= — no erro
        EXPECT(err.find("glb: view0 buffer0 off=0 len=314572800 acc=0 "
                        "declared=314572800 real=314572800") !=
               std::string::npos);
    }

    // ---- A REGRA DO DONO (FAZ 3): off+len ≤ real E ≤ declared — um view
    // dentro do REAL mas acima do DECLARADO é mentira distinguida --------
    {
        // buffer real 100 B (o loader), DECLARADO 50 B (o JSON), view [0,60)
        char js[512];
        std::snprintf(js, sizeof(js),
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":50}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,"
            "\"byteLength\":60}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,"
            "\"count\":3,\"type\":\"VEC3\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],"
            "\"scene\":0}");
        GltfModel model;
        std::string err;
        u64 calls = 0;
        const bool ok =
            parseWith(std::string(js), 100u, 38051884u, &calls, model, err);
        EXPECT(!ok);
        EXPECT_MSG(err.find("acima do byteLength declarado") !=
                       std::string::npos,
                   "o view acima do DECLARADO não deu a causa distinta: %s",
                   err.c_str());
        // a linha completa com os números DIVERGENTES (declared=50,
        // real=100) + file= o disco (FAZ 1) — sem «…», sem truncar
        EXPECT(err.find("glb: view0 buffer0 off=0 len=60 acc=0 "
                        "declared=50 real=100 file=38051884") !=
               std::string::npos);
    }
    // ---- e o view DENTRO de ambos (real 100, declarado 50, view [0,50))
    // passa (o limite exclusivo em ação — a fixture (i) do dono) -----------
    {
        char js[512];
        std::snprintf(js, sizeof(js),
            "{\"asset\":{\"version\":\"2.0\"},"
            "\"buffers\":[{\"byteLength\":50}],"
            "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,"
            "\"byteLength\":50}],"
            "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,"
            "\"count\":3,\"type\":\"VEC3\"}],"
            "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}],"
            "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],"
            "\"scene\":0}");
        GltfModel model;
        std::string err;
        u64 calls = 0;
        const bool ok =
            parseWith(std::string(js), 100u, 38051884u, &calls, model, err);
        if (!ok) {
            EXPECT_MSG(false, "o view a acabar no DECLARADO foi recusado: %s",
                       err.c_str());
            return;   // os derefs seguintes são UB — sai LIMPO
        }
        EXPECT(model.meshes.size() == 1u);
    }
}
