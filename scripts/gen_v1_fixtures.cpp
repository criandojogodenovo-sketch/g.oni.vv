// scripts/gen_v1_fixtures.cpp — GERA as fixtures v1/v2 do .gmesh COM O
// ESCRITOR v1 (a correr UMA VEZ, antes do escritor passar a v3 no PASSO 2
// do 0.10-M). As fixtures commitadas são a matéria-prima da sentinela
// R-038 (retrocompatibilidade): ficheiros v1 REAIS (bytes do escritor da
// era 0.9.6) + a variante v2 (a MESMA construção com o campo version=2 —
// a interpretação tolerante documentada no GMESH_formato.md; a v2 nunca
// existiu no histórico do repo, ver relatório PASSO 2 §v2).
// Uso: g++ ... gen_v1_fixtures.cpp GOwnFormats.cpp AssetConverter? NÃO —
// só GOwnFormats + dependências mínimas. Ver o comando no relatório.
#include "assets/GOwnFormats.h"
#include "core/Types.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace vv;

// esfera determinística 16×12 (a MESMA família do round-trip do wiring010)
int main() {
    const int seg = 16, ring = 12;
    MeshData m;
    m.name = "esfera-v1";
    for (int r = 0; r <= ring; ++r) {
        const f32 phi = f32(M_PI) * f32(r) / f32(ring);
        for (int s = 0; s <= seg; ++s) {
            const f32 th = 2.0f * f32(M_PI) * f32(s) / f32(seg);
            Vertex v;
            v.pos = Vec3{std::sin(phi) * std::cos(th), std::cos(phi),
                         std::sin(phi) * std::sin(th)};
            v.normal = v.pos;
            v.uv = Vec2{f32(s) / f32(seg), f32(r) / f32(ring)};
            m.vertices.push_back(v);
        }
    }
    for (int r = 0; r < ring; ++r) {
        for (int s = 0; s < seg; ++s) {
            const u16 a = u16(r * (seg + 1) + s);
            const u16 b = u16(a + seg + 1);
            m.indices.push_back(a);
            m.indices.push_back(b);
            m.indices.push_back(u16(a + 1));
            m.indices.push_back(b);
            m.indices.push_back(u16(b + 1));
            m.indices.push_back(u16(a + 1));
        }
    }
    MeshData::Group g;
    g.name = "corpo";
    g.material = "laca";
    g.firstIndex = 0;
    g.indexCount = static_cast<u32>(m.indices.size());
    m.groups.push_back(g);

    std::vector<u8> bytes;
    std::string err;
    if (!writeGMesh(m, bytes, err)) {
        std::fprintf(stderr, "ERRO: %s\n", err.c_str());
        return 1;
    }
    const char* path = "tests/fixtures/gmesh_v1_esfera.gmesh";
    FILE* f = std::fopen(path, "wb");
    if (!f) return 1;
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    std::printf("v1: %s — %zu B (%zu verts / %zu idx)\n", path, bytes.size(),
                m.vertices.size(), m.indices.size());

    // a variante v2: os MESMOS bytes com o campo version (offset 4, u16 LE)
    // = 2 — a leitura tem de abrir (a interpretação tolerante)
    std::vector<u8> v2 = bytes;
    v2[4] = 2;
    v2[5] = 0;
    path = "tests/fixtures/gmesh_v2_esfera.gmesh";
    f = std::fopen(path, "wb");
    if (!f) return 1;
    std::fwrite(v2.data(), 1, v2.size(), f);
    std::fclose(f);
    std::printf("v2: %s — %zu B (version=2)\n", path, v2.size());
    return 0;
}
