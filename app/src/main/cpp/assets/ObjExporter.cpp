// assets/ObjExporter.cpp — MeshData → OBJ (F5).
//
// v/vt/vn alinhados: o formato interno tem 1 normal + 1 uv POR VÉRTICE, então
// o export escreve os três arrays 1:1 e a face cita o mesmo índice nos três
// slots (f a/a/a). Parse desse ficheiro re-dedupa para o mesmo conjunto.
#include "assets/ObjExporter.h"
#include <cstdio>
#include <cstring>

namespace vv {

namespace {
void appendFloat(std::string& s, f32 v) {
    // %.9g garante round-trip exato de f32 (mesma política do Json::dump)
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.9g", static_cast<double>(v));
    s += buf;
}
} // namespace

std::string exportObj(const MeshData& mesh) {
    std::string s = "# exportado pela G.One VV (formato .goni)\n";
    if (!mesh.name.empty()) {
        s += "o " + mesh.name + "\n";
    }
    char line[96];
    for (const Vertex& v : mesh.vertices) {
        s += "v ";
        appendFloat(s, v.pos.x); s += ' ';
        appendFloat(s, v.pos.y); s += ' ';
        appendFloat(s, v.pos.z);
        s += '\n';
    }
    for (const Vertex& v : mesh.vertices) {
        s += "vt ";
        appendFloat(s, v.uv.x); s += ' ';
        appendFloat(s, v.uv.y);
        s += '\n';
    }
    for (const Vertex& v : mesh.vertices) {
        s += "vn ";
        appendFloat(s, v.normal.x); s += ' ';
        appendFloat(s, v.normal.y); s += ' ';
        appendFloat(s, v.normal.z);
        s += '\n';
    }

    // faces por grupo (range contíguo de índices); triângulos já fechados
    if (mesh.groups.empty()) {
        // sem grupos: um grupo único com todos os índices
        for (u32 i = 0; i + 2 < mesh.indices.size(); i += 3) {
            const u16 a = mesh.indices[i];
            const u16 b = mesh.indices[i + 1];
            const u16 c = mesh.indices[i + 2];
            std::snprintf(line, sizeof(line), "f %u/%u/%u %u/%u/%u %u/%u/%u\n",
                          a + 1u, a + 1u, a + 1u,
                          b + 1u, b + 1u, b + 1u,
                          c + 1u, c + 1u, c + 1u);
            s += line;
        }
        return s;
    }
    for (const MeshData::Group& g : mesh.groups) {
        s += "g " + (g.name.empty() ? std::string("grupo") : g.name) + "\n";
        if (!g.material.empty()) {
            s += "usemtl " + g.material + "\n";
        }
        const u32 end = g.firstIndex + g.indexCount;
        for (u32 i = g.firstIndex; i + 2 < end && i + 2 < mesh.indices.size(); i += 3) {
            const u16 a = mesh.indices[i];
            const u16 b = mesh.indices[i + 1];
            const u16 c = mesh.indices[i + 2];
            std::snprintf(line, sizeof(line), "f %u/%u/%u %u/%u/%u %u/%u/%u\n",
                          a + 1u, a + 1u, a + 1u,
                          b + 1u, b + 1u, b + 1u,
                          c + 1u, c + 1u, c + 1u);
            s += line;
        }
    }
    return s;
}

} // namespace vv
