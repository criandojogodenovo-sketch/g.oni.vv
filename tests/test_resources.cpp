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
