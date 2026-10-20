#pragma once
// render/Pbr.h — a FONTE ÚNICA da matemática PBR-lite do lit (0.12-Y 3.Y-C).
//
// PURAS (o padrão camWorldPerPixel/gridFadeForDist — o shader replica LINHA
// A LINHA e os pins aferem a MESMA conta aqui, sem GL):
//
//   • roughToShininess  — o roughness do glTF [0..1] → o expoente Blinn-
//     Phong [4..256] (o piso 4 evita o brilho infinito do espelho perfeito
//     sem IBL; o teto 256 é o brilho "quase-espelho" do GE8320);
//   • blinnSpec         — o especular por half-vector (o H = normalize(L+V),
//     pow(N·H, shininess) — a mesma fórmula do FS);
//   • metallicAlbedo    — o mix metálico: metals não têm difusa própria —
//     o albedo difuso escala por (1−metallic·0.85) e o especular SAI do
//     albedo (a aproximação "metal = tinted mirror" do lit-lite);
//   • fogFactorExp2     — a névoa exp2 (a casa): f = 1 − exp(−(d·density)²)
//     (a mesma curva do FogExp2 do Unity/irrLicht; density 0 = morta);
//   • equirectUv        — o uv do mapa 360 (o HDRI equirectangular 2:1):
//     u = 0.5 + atan2(dir.z, dir.x)/(2π), v = acos(−dir.y)/π (o −y é o
//     topo da imagem — o padrão dos HDRIs de panorama).
//
// O FS do Material.cpp replica ESTAS fórmulas (o comentário aponta para
// cá). Qualquer mudança é um flip duplo (aqui + no shader) apanhado pelos
// pins (os valores aferram os DOIS lados pela fonte única).
#include "core/Types.h"
#include "math/Math.h"
#include <cmath>

namespace vv {
namespace pbr {

// o roughness do glTF → o expoente de Blinn-Phong (piso 4, teto 256)
inline f32 roughToShininess(f32 roughness) {
    if (roughness < 0.0f) roughness = 0.0f;
    if (roughness > 1.0f) roughness = 1.0f;
    const f32 r = 1.0f - roughness;           // rough 0 = brilhante
    return 4.0f + r * r * 252.0f;             // 4..256 (o quadrado suaviza)
}

// o especular Blinn-Phong por half-vector (N e H já normalizados)
inline f32 blinnSpec(const Vec3& n, const Vec3& h, f32 shininess) {
    const f32 ndh = n.x * h.x + n.y * h.y + n.z * h.z;
    if (ndh <= 0.0f) {
        return 0.0f;
    }
    return std::pow(ndh, shininess);
}

// o mix metálico do albedo difuso (o metal não tem difusa — escala por
// 1−metallic·0.85; o especular herda o tint no FS)
inline f32 metallicDiffuseScale(f32 metallic) {
    if (metallic < 0.0f) metallic = 0.0f;
    if (metallic > 1.0f) metallic = 1.0f;
    return 1.0f - metallic * 0.85f;
}

// a névoa exp2 da casa: 1 − exp(−(d·density)²) (density 0 = morta)
inline f32 fogFactorExp2(f32 distToEye, f32 density) {
    if (density <= 0.0f) {
        return 0.0f;
    }
    const f32 x = distToEye * density;
    return 1.0f - std::exp(-x * x);
}

// o uv do equirectangular 2:1 para a direção (normalizada ou não)
inline void equirectUv(const Vec3& dir, f32 out[2]) {
    const f32 len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len <= 1e-8f) {
        out[0] = out[1] = 0.0f;
        return;
    }
    const f32 x = dir.x / len, y = dir.y / len, z = dir.z / len;
    out[0] = 0.5f + std::atan2(z, x) / (2.0f * 3.14159265f);
    out[1] = std::acos(-y) / 3.14159265f;
}

} // namespace pbr
} // namespace vv
