// tests/test_import_obj.cpp — F5-B: parser Wavefront OBJ → MeshData.
// Cobre: v/vn/vt, todas as formas de canto de face, fan de quads, dedup de
// cantos, grupos g/o + usemtl com ranges, índices negativos, CRLF, defaults
// de normal/uv quando ausentes, falhas explícitas (sintaxe, range, u16).
#include "TestFramework.h"
#include <cstring>
#include "assets/ObjImporter.h"
#include "math/Math.h"

using namespace vv;
using ::test::nearEqF;
using ::test::vecNearF;

namespace {

bool parse(const char* src, MeshData& m, std::string& err) {
    return parseObj(src, std::strlen(src), m, err);
}

// triângulo com normal explícita (forma a//c) e uv ausente
const char* kTri =
    "# tri de teste\n"
    "v 0.0 0.0 0.0\n"
    "v 1.0 0.0 0.0\n"
    "v 0.0 1.0 0.0\n"
    "vn 0.0 0.0 1.0\n"
    "f 1//1 2//1 3//1\n";

} // namespace

TEST(obj_triangulo_basico_normais_e_default_uv) {
    MeshData m;
    std::string err;
    EXPECT(parse(kTri, m, err));
    EXPECT(err.empty());
    EXPECT(m.ok());
    EXPECT(m.vertices.size() == 3u);
    EXPECT(m.indices.size() == 3u);
    EXPECT(vecNearF(m.vertices[0].pos, Vec3{0, 0, 0}));
    EXPECT(vecNearF(m.vertices[1].pos, Vec3{1, 0, 0}));
    EXPECT(vecNearF(m.vertices[2].pos, Vec3{0, 1, 0}));
    EXPECT(vecNearF(m.vertices[0].normal, Vec3{0, 0, 1}));
    // sem vt → uv default (0,0); sem vn no canto → normal default (0,1,0)
    EXPECT(nearEqF(m.vertices[0].uv.x, 0.0f) && nearEqF(m.vertices[0].uv.y, 0.0f));
    EXPECT(m.indices[0] == 0u && m.indices[1] == 1u && m.indices[2] == 2u);
    EXPECT(m.groups.size() == 1u);   // grupo default aberto pela face
    EXPECT(m.groups[0].indexCount == 3u);
}

TEST(obj_quad_fan_e_dedup_de_cantos) {
    // quad unitário no plano Z=1: 2 tris por fan; vn partilhado dedup por canto
    const char* quad =
        "v -1 -1 1\n"
        "v  1 -1 1\n"
        "v  1  1 1\n"
        "v -1  1 1\n"
        "vn 0 0 1\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 1 1\n"
        "vt 0 1\n"
        "f 1/1/1 2/2/1 3/3/1 4/4/1\n";
    MeshData m;
    std::string err;
    EXPECT(parse(quad, m, err));
    // dedup: 4 cantos únicos (v,vt,vn distintos entre si) + fórmula de fan
    EXPECT(m.vertices.size() == 4u);
    EXPECT(m.indices.size() == 6u);          // (0,1,2) + (0,2,3)
    EXPECT(m.indices[0] == 0u && m.indices[1] == 1u && m.indices[2] == 2u);
    EXPECT(m.indices[3] == 0u && m.indices[4] == 2u && m.indices[5] == 3u);
    EXPECT(nearEqF(m.vertices[2].uv.x, 1.0f) && nearEqF(m.vertices[2].uv.y, 1.0f));
    EXPECT(vecNearF(m.vertices[3].normal, Vec3{0, 0, 1}));
}

TEST(obj_dedup_reusa_canto_igual_entre_faces) {
    // dois tris que partilham o canto v1/vt1/vn1 → o vértice 1 é reusado
    const char* src =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "v 1 1 0\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/1/1\n"     // canto 1 = (v1,vt1,vn1)
        "f 2/2/1 4/1/1 3/1/1\n";    // reusa (v1,vt1,vn1)
    MeshData m;
    std::string err;
    EXPECT(parse(src, m, err));
    EXPECT(m.vertices.size() == 4u);   // 6 cantos → 4 vértices únicos
    EXPECT(m.indices.size() == 6u);
    EXPECT(m.indices[3] == m.indices[0] + 1);   // 2ª face começa no vértice reusado
}

TEST(obj_grupos_e_usemtl_ranges) {
    const char* src =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "v 1 1 0\n"
        "g parede\n"
        "usemtl tijolo\n"
        "f 1 2 3\n"
        "g chao\n"
        "usemtl cimento\n"
        "f 2 4 3\n";
    MeshData m;
    std::string err;
    EXPECT(parse(src, m, err));
    EXPECT(m.groups.size() == 2u);
    EXPECT(m.groups[0].name == "parede");
    EXPECT(m.groups[0].material == "tijolo");
    EXPECT(m.groups[0].firstIndex == 0u);
    EXPECT(m.groups[0].indexCount == 3u);
    EXPECT(m.groups[1].name == "chao");
    EXPECT(m.groups[1].material == "cimento");
    EXPECT(m.groups[1].firstIndex == 3u);
    EXPECT(m.groups[1].indexCount == 3u);
    // "o" também abre troca de grupo
    const char* src2 =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "o objeto1\nf 1 2 3\n";
    MeshData m2;
    EXPECT(parse(src2, m2, err));
    EXPECT(m2.groups.size() == 1u);
    EXPECT(m2.groups[0].name == "objeto1");
}

TEST(obj_usemtl_sem_g_divide_grupo_pelo_material) {
    const char* src =
        "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
        "f 1 2 3\n"
        "usemtl outro\n"
        "f 2 4 3\n";
    MeshData m;
    std::string err;
    EXPECT(parse(src, m, err));
    EXPECT(m.groups.size() == 2u);
    EXPECT(m.groups[0].material.empty());
    EXPECT(m.groups[1].material == "outro");
    EXPECT(m.groups[1].firstIndex == 3u);
}

TEST(obj_indices_negativos_e_mistura_de_formas) {
    // índice negativo = relativo ao último; mistura a, a/b, a/b/c, a//c
    const char* src =
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 0 1 0\n"
        "vt 0.25 0.75\n"
        "vn 0 1 0\n"
        "f -3/1/1 -2/-1/1 -1//1\n";   // todos os cantos resolvem p/ (0,1,2)
    MeshData m;
    std::string err;
    EXPECT(parse(src, m, err));
    EXPECT(m.vertices.size() == 3u);
    EXPECT(nearEqF(m.vertices[0].uv.x, 0.25f) && nearEqF(m.vertices[0].uv.y, 0.75f));
    EXPECT(nearEqF(m.vertices[1].uv.x, 0.25f));   // -1 = último vt
    EXPECT(vecNearF(m.vertices[2].normal, Vec3{0, 1, 0}));
}

TEST(obj_crlf_normalizado_parse_igual) {
    const char* lf = "v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n";
    const char* crlf =
        "v 0 0 0\r\nv 1 0 0\r\nv 0 1 0\r\nvn 0 0 1\r\nf 1//1 2//1 3//1\r\n";
    MeshData a, b;
    std::string err;
    EXPECT(parse(lf, a, err));
    EXPECT(parse(crlf, b, err));
    EXPECT(a.vertices.size() == b.vertices.size());
    EXPECT(a.indices.size() == b.indices.size());
    for (size_t i = 0; i < a.vertices.size(); ++i) {
        EXPECT(vecNearF(a.vertices[i].pos, b.vertices[i].pos));
        EXPECT(vecNearF(a.vertices[i].normal, b.vertices[i].normal));
    }
}

TEST(obj_invalido_falha_com_erro) {
    MeshData m;
    std::string err;
    EXPECT(!parse("", m, err));                       // vazio
    EXPECT(!parse("v 0 0 0\nv 1 0 0\nf 1 2\n", m, err));           // face curta
    EXPECT(!parse("v 0 0 0\nv 1 0 0\nf 1 2\nv 0 1 0\n", m, err));  // igual
    EXPECT(!parse("v 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n", m, err));  // v curto
    EXPECT(!parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 9\n", m, err));// fora do range
    EXPECT(!parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 x\n", m, err));// token lixo
    EXPECT(!parse("v 0 0 0\nv 1 0 0\nv 0 1 0\n", m, err));         // sem faces
    EXPECT(!parse("f 1 2 3\n", m, err));              // face sem v anterior
}

TEST(obj_excesso_de_vertices_unicos_recusado_u16) {
    // 70000 v + faces com cantos únicos em sequência → passa do limite u16
    std::string big;
    big.reserve(1u << 20);
    char line[64];
    for (int i = 0; i < 70000; ++i) {
        std::snprintf(line, sizeof(line), "v %d 0 0\n", i);
        big += line;
    }
    int used = 1;
    while (used + 2 <= 70000) {
        std::snprintf(line, sizeof(line), "f %d %d %d\n", used, used + 1, used + 2);
        used += 3;
        big += line;
    }
    MeshData m;
    std::string err;
    EXPECT(!parseObj(big.data(), big.size(), m, err));
    EXPECT(err.find("65535") != std::string::npos);
}

TEST(obj_v_excedente_mas_poucos_usados_ok) {
    // 100k "v" no ficheiro mas só 3 usados → mesh de 3 vértices (o limite é
    // sobre vértices ÚNICOS de saída, não sobre o tamanho do ficheiro)
    std::string src;
    char line[64];
    for (int i = 0; i < 100000; ++i) {
        std::snprintf(line, sizeof(line), "v %d 0.5 0\n", i);
        src += line;
    }
    src += "f 1 2 3\n";
    MeshData m;
    std::string err;
    EXPECT(parseObj(src.data(), src.size(), m, err));
    EXPECT(m.vertices.size() == 3u);
}
