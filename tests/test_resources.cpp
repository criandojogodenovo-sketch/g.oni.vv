// tests/test_resources.cpp — F5-B/E: export OBJ (round-trip) e cache do
// ResourceManager por caminho relativo.
#include "TestFramework.h"
#include <algorithm>
#include <cstring>
#include "assets/ObjExporter.h"
#include "assets/ObjImporter.h"
#include "assets/ResourceManager.h"
#include "FakeStorage.h"

using namespace vv;
using ::test::nearEqF;

namespace {

bool parse(const char* src, MeshData& m, std::string& err) {
    return parseObj(src, std::strlen(src), m, err);
}

// vértice "chave de ordenação" — para comparar multiconjuntos no round-trip
struct VertKey {
    f32 v[8];   // pos(3) normal(3) uv(2)
    bool operator==(const VertKey& o) const {
        for (int i = 0; i < 8; ++i) {
            if (v[i] != o.v[i]) return false;
        }
        return true;
    }
    bool operator<(const VertKey& o) const {
        for (int i = 0; i < 8; ++i) {
            if (v[i] != o.v[i]) return v[i] < o.v[i];
        }
        return false;
    }
};

VertKey keyOf(const Vertex& x) {
    VertKey k{{x.pos.x, x.pos.y, x.pos.z,
               x.normal.x, x.normal.y, x.normal.z, x.uv.x, x.uv.y}};
    return k;
}

// quad texturizado com 2 grupos — exercita grupos/uv/normais no export
MeshData sampleMesh() {
    const char* src =
        "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "vn 0 0 1\n"
        "g frente\nusemtl tijolo\n"
        "f 1/1/1 2/2/1 3/3/1 4/4/1\n";
    MeshData m;
    std::string err;
    parse(src, m, err);
    return m;
}

} // namespace

TEST(obj_exporter_roundtrip_geometria_igual) {
    const MeshData a = sampleMesh();
    const std::string text = exportObj(a);
    EXPECT(!text.empty());

    MeshData b;
    std::string err;
    EXPECT(parse(text.c_str(), b, err));

    EXPECT(b.vertices.size() == a.vertices.size());
    EXPECT(b.indices.size() == a.indices.size());
    EXPECT(b.groups.size() == a.groups.size());
    EXPECT(b.groups[0].material == "tijolo");

    // multiconjunto de vértices igual (ordem pode variar com re-dedup)
    std::vector<VertKey> ka, kb;
    for (const Vertex& v : a.vertices) ka.push_back(keyOf(v));
    for (const Vertex& v : b.vertices) kb.push_back(keyOf(v));
    std::sort(ka.begin(), ka.end());
    std::sort(kb.begin(), kb.end());
    EXPECT(ka == kb);

    // multiconjunto de triângulos igual (por posições)
    auto triSet = [](const MeshData& m) {
        std::vector<VertKey> out;
        for (u32 i = 0; i + 2 < m.indices.size(); i += 3) {
            // chave = soma dos 3 vértices (determinística p/ comparação)
            const Vertex& x = m.vertices[m.indices[i]];
            const Vertex& y = m.vertices[m.indices[i + 1]];
            const Vertex& z = m.vertices[m.indices[i + 2]];
            VertKey k{{x.pos.x + y.pos.x + z.pos.x,
                       x.pos.y + y.pos.y + z.pos.y,
                       x.pos.z + y.pos.z + z.pos.z, 0, 0, 0, 0, 0}};
            out.push_back(k);
        }
        std::sort(out.begin(), out.end());
        return out;
    };
    EXPECT(triSet(a) == triSet(b));
}

TEST(obj_exporter_mesh_sem_grupos_faz_grupo_unico) {
    const char* src = "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
    MeshData m;
    std::string err;
    EXPECT(parse(src, m, err));
    m.groups.clear();   // simula mesh sem metadados de grupo
    const std::string text = exportObj(m);
    MeshData b;
    EXPECT(parse(text.c_str(), b, err));
    EXPECT(b.vertices.size() == 3u);
    EXPECT(b.indices.size() == 3u);
}

TEST(rm_cache_mesh_por_caminho_sem_duplicar_carga) {
    FakeStorage st;
    st.writeText("meshes/quad.obj", exportObj(sampleMesh()));

    ResourceManager rm;
    rm.setStorage(&st);
    std::string err;

    const MeshData* first = rm.mesh("meshes/quad.obj", err);
    EXPECT(first != nullptr && err.empty());
    EXPECT(first->ok());
    EXPECT(rm.meshLoads() == 1u);

    // cache hit: MESMO ponteiro, SEM nova carga
    const MeshData* again = rm.mesh("meshes/quad.obj", err);
    EXPECT(again == first);
    EXPECT(rm.meshLoads() == 1u);

    // release → próxima chamada recarrega: muda o CONTEÚDO no storage e a
    // nova carga tem de refletir a mudança (endereço pode ser reutilizado —
    // o contrato é sobre recarga, não sobre identidade de ponteiro)
    st.writeText("meshes/quad.obj", exportObj(sampleMesh()));
    rm.releaseMesh("meshes/quad.obj");
    const MeshData* third = rm.mesh("meshes/quad.obj", err);
    EXPECT(third != nullptr);
    EXPECT(rm.meshLoads() == 2u);   // houve carga REAL do storage
    EXPECT(rm.meshCount() == 1u);
}

TEST(rm_adopt_registra_sem_storage) {
    ResourceManager rm;
    std::string err;
    EXPECT(rm.mesh("meshes/gen.obj", err) == nullptr);   // sem storage → erro claro

    rm.adoptMesh("meshes/gen.obj", sampleMesh());
    const MeshData* got = rm.mesh("meshes/gen.obj", err);
    EXPECT(got != nullptr);
    EXPECT(rm.meshLoads() == 0u);        // adopt não conta como carga
    EXPECT(rm.hasMesh("meshes/gen.obj"));

    // substitui pelo adopt de novo (contrato: substitui)
    MeshData bigger = sampleMesh();
    bigger.indices.push_back(0);   // 6 → 7 índices
    rm.adoptMesh("meshes/gen.obj", std::move(bigger));
    EXPECT(rm.mesh("meshes/gen.obj", err)->indices.size() == 7u);
}

TEST(rm_erros_caminho_ausente_e_formato) {
    FakeStorage st;
    st.makeDirs("meshes");
    ResourceManager rm;
    rm.setStorage(&st);
    std::string err;

    EXPECT(rm.mesh("meshes/faltando.obj", err) == nullptr);
    EXPECT(err.find("não encontrado") != std::string::npos);

    st.writeText("meshes/quebrado.obj", "isto não é um obj\n");
    EXPECT(rm.mesh("meshes/quebrado.obj", err) == nullptr);
    EXPECT(err.find("OBJ inválido") != std::string::npos);

    st.writeText("meshes/modelo.fbx", "lixo");
    EXPECT(rm.mesh("meshes/modelo.fbx", err) == nullptr);
    EXPECT(err.find("não suportado") != std::string::npos);

    // .OBJ em maiúsculas também é OBJ (extensão normalizada)
    st.writeText("meshes/maiusculas.OBJ", exportObj(sampleMesh()));
    EXPECT(rm.mesh("meshes/maiusculas.OBJ", err) != nullptr);
}

TEST(rm_releaseAll_zera_cache_e_estatistica) {
    FakeStorage st;
    st.writeText("meshes/a.obj", exportObj(sampleMesh()));
    st.writeText("meshes/b.obj", exportObj(sampleMesh()));

    ResourceManager rm;
    rm.setStorage(&st);
    std::string err;
    EXPECT(rm.mesh("meshes/a.obj", err) != nullptr);
    EXPECT(rm.mesh("meshes/b.obj", err) != nullptr);
    EXPECT(rm.meshCount() == 2u);
    EXPECT(rm.meshLoads() == 2u);

    rm.releaseAll();
    EXPECT(rm.meshCount() == 0u);
    EXPECT(rm.meshLoads() == 0u);

    // cache continua utilizável após releaseAll
    EXPECT(rm.mesh("meshes/a.obj", err) != nullptr);
    EXPECT(rm.meshLoads() == 1u);
}

// ---- F5-C: despacho .gltf/.glb no ResourceManager ---------------------------

namespace {
// base64 encode mínimo (fixture de data: URI)
std::string b64enc(const std::vector<u8>& b) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < b.size(); i += 3) {
        const u32 v = (u32(b[i]) << 16) | (u32(b[i + 1]) << 8) | b[i + 2];
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];  out += tbl[v & 63];
    }
    if (i < b.size()) {
        u32 v = u32(b[i]) << 16;
        if (i + 1 < b.size()) v |= u32(b[i + 1]) << 8;
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
        out += (i + 1 < b.size()) ? tbl[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

// .gltf com DOIS meshes (tris independentes) embutidos em data URI
std::string twoMeshGltf() {
    std::vector<u8> bin;
    auto pushF = [&bin](f32 v) { u8 t[4]; std::memcpy(t, &v, 4); bin.insert(bin.end(), t, t + 4); };
    // mesh0: 3 verts @0; mesh1: 3 verts @36; índices implícitos (não-indexado)
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
        b64enc(bin).c_str(), static_cast<u32>(bin.size()));
    return j;
}
} // namespace

TEST(rm_despacha_gltf_com_subrefs_e_cache_de_modelo) {
    FakeStorage st;
    st.writeText("meshes/par.gltf", twoMeshGltf());

    ResourceManager rm;
    rm.setStorage(&st);
    std::string err;

    // ficheiro com 2 meshes sem '#' → erro claro pedindo sub-ref
    EXPECT(rm.mesh("meshes/par.gltf", err) == nullptr);
    EXPECT(err.find("#<i>") != std::string::npos);

    const MeshData* m0 = rm.mesh("meshes/par.gltf#0", err);
    const MeshData* m1 = rm.mesh("meshes/par.gltf#1", err);
    EXPECT(m0 != nullptr && m1 != nullptr);
    EXPECT(m0->name == "a" && m1->name == "b");
    EXPECT(m0->vertices.size() == 3u && m1->vertices.size() == 3u);
    // o MODELO foi parseado UMA vez (cache de modelo compartilhado entre refs)
    EXPECT(rm.meshLoads() == 1u);
    EXPECT(rm.modelCount() == 1u);

    // cache hit por ref (sem carga nova)
    EXPECT(rm.mesh("meshes/par.gltf#0", err) == m0);
    EXPECT(rm.meshLoads() == 1u);

    // sub-ref fora do range → erro claro
    EXPECT(rm.mesh("meshes/par.gltf#5", err) == nullptr);
    EXPECT(err.find("fora do range") != std::string::npos);

    // release do ref; releaseAll derruba modelo + refs
    rm.releaseMesh("meshes/par.gltf#0");
    EXPECT(!rm.hasMesh("meshes/par.gltf#0"));
    rm.releaseAll();
    EXPECT(rm.meshCount() == 0u && rm.modelCount() == 0u);
    EXPECT(rm.mesh("meshes/par.gltf#1", err) != nullptr);
    EXPECT(rm.meshLoads() == 1u);   // recarregou o modelo (agora 1 parse pós-clear)
}

TEST(rm_glb_via_storage_e_obj_mistos) {
    FakeStorage st;
    // .glb mínimo: header + JSON chunk + BIN chunk (1 tri não-indexado)
    std::vector<u8> bin;
    auto pushF = [&bin](f32 v) { u8 t[4]; std::memcpy(t, &v, 4); bin.insert(bin.end(), t, t + 4); };
    for (int i = 0; i < 3; ++i) { pushF(0.5f); pushF(0.25f); pushF(0.125f); }
    const char* jsonBody =
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}";
    std::string json = jsonBody;
    while (json.size() % 4 != 0) json += ' ';
    std::vector<u8> binPad = bin;
    while (binPad.size() % 4 != 0) binPad.push_back(0);

    std::vector<u8> glb;
    auto u32v = [&glb](u32 v) { u8 t[4]; std::memcpy(t, &v, 4); glb.insert(glb.end(), t, t + 4); };
    u32v(0x46546C67u); u32v(2);
    u32v(12 + 8 + static_cast<u32>(json.size()) + 8 + static_cast<u32>(binPad.size()));
    u32v(static_cast<u32>(json.size())); u32v(0x4E4F534Au);
    glb.insert(glb.end(), json.begin(), json.end());
    u32v(static_cast<u32>(binPad.size())); u32v(0x004E4942u);
    glb.insert(glb.end(), binPad.begin(), binPad.end());
    st.writeBytes("meshes/tri.glb", glb.data(), glb.size());
    st.writeText("meshes/quad.obj", exportObj(sampleMesh()));

    ResourceManager rm;
    rm.setStorage(&st);
    std::string err;
    const MeshData* g = rm.mesh("meshes/tri.glb", err);   // 1 mesh → uso direto
    EXPECT(g != nullptr);
    EXPECT(g->vertices.size() == 3u);
    EXPECT(nearEqF(g->vertices[1].pos.y, 0.25f));
    EXPECT(rm.mesh("meshes/quad.obj", err) != nullptr);
    EXPECT(rm.meshLoads() == 2u);   // 1 parse por ficheiro

    // .glb corrompido (magic errado, tamanho mínimo válido) → erro claro
    const u8 badMagic[20] = {'X', 'X', 'X', 'X', 2, 0, 0, 0, 20, 0, 0, 0};
    st.writeBytes("meshes/mau.glb", badMagic, sizeof(badMagic));
    EXPECT(rm.mesh("meshes/mau.glb", err) == nullptr);
    EXPECT(err.find("magic") != std::string::npos);
}

TEST(rm_gltf_buffer_externo_resolvido_pelo_storage) {
    FakeStorage st;
    // buffers externos são relativos à PASTA do .gltf
    std::vector<u8> bin;
    auto pushF = [&bin](f32 v) { u8 t[4]; std::memcpy(t, &v, 4); bin.insert(bin.end(), t, t + 4); };
    for (int i = 0; i < 3; ++i) { pushF(2); pushF(3); pushF(4); }
    st.writeBytes("meshes/scene.bin", bin.data(), bin.size());
    st.writeText("meshes/ext.gltf",
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"scene.bin\",\"byteLength\":36}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}");

    ResourceManager rm;
    rm.setStorage(&st);
    std::string err;
    const MeshData* m = rm.mesh("meshes/ext.gltf", err);
    EXPECT(m != nullptr);
    EXPECT(m->vertices.size() == 3u);
    EXPECT(nearEqF(m->vertices[0].pos.x, 2.0f));

    // buffer externo ausente → erro claro com o nome do ficheiro
    st.writeText("meshes/parte.gltf",
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"falta.bin\",\"byteLength\":36}],"
        "\"meshes\":[]}");
    ResourceManager rm2;
    rm2.setStorage(&st);
    EXPECT(rm2.mesh("meshes/parte.gltf", err) == nullptr);
    EXPECT(err.find("falta.bin") != std::string::npos);
}
