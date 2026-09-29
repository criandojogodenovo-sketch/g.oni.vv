#pragma once
// assets/Assets.h — tipos de asset em CPU (F5), GL-free / host-testável.
//
// MeshData: resultado dos importadores OBJ (F5-B) e glTF/GLB (F5-C). O
// upload para GPU é feito pelo render/Mesh (F2) — um MeshData pode virar
// UM objeto GL (ResourceManager/GpuAssets garantem a dedup por caminho).
// Índices u16 = limite de 65535 vértices por mesh (o formato do engine);
// mesh maior que isso falha no import com erro explícito.
//
// RawImage: decode PNG (F5-D). Upload + mipmaps = render/Texture (device);
// o gate 4K→2K roda AQUI, em CPU, testável no CI.
#include <string>
#include <vector>
#include "core/Types.h"
#include "render/Vertex.h"

namespace vv {

struct MeshData {
    struct Group {
        std::string name;        // grupo OBJ ("g") ou primitiva glTF
        std::string material;    // usemtl / material glTF (básico — F5 sem MTL)
        u32 firstIndex = 0;      // range dentro de indices
        u32 indexCount = 0;
    };
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<u16> indices;
    std::vector<Group> groups;

    bool ok() const { return !vertices.empty() && !indices.empty(); }
};

struct RawImage {
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> rgba;    // width * height * 4

    bool ok() const {
        return width > 0 && height > 0 &&
               rgba.size() == static_cast<size_t>(width) * height * 4;
    }
};

} // namespace vv
