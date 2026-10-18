#pragma once
// ui/Noise2D.h — 0.12-E (E-3 · FASE 3.4) — A TEXTURA 2D DE RUÍDO das
// bordas ativas (a ordem do dono: «Para as bordas ativas, usa uma textura
// 2D de ruído em scroll lento (UV offset) em vez de shaders matemáticos»
// — o fogo SDF da D-3 morreu com a 0.12-E E-1; ESTE é o substituto 2D:
// quads texturados + UV offset, ZERO shaders novos, ZERO fbm).
//
// FONTE ÚNICA da matemática (GL-free, header-only — o padrão da casa):
//   · noisePixel(x,y) — o ruído TILEÁVEL 64×64 determinístico (o hash
//     embrulha módulo 64: a textura repete sem emenda, o GL_REPEAT é
//     honesto; o MESMO pixel (x,y) = o MESMO valor — o determinismo dos
//     pins);
//   · fireScrollOffset(time) — o OFFSET do scroll lento em UV (o mesmo t
//     = o mesmo offset — a animação é determinística e afervel).
// Consumidores: o Renderer (o upload da textura no init) e o UiContext
// (o emissor fireBorder2D — as 4 bandas das bordas do botão ativo); o
// rasterizador honesto do stub honra o GL_REPEAT (a emenda wrap incluída).
#include "core/Types.h"
#include <cmath>

namespace vv {
namespace noise2d {

// o tamanho da textura (POT 64 — obrigatório para o GL_REPEAT em ES 3.0)
constexpr u32 kNoiseSize = 64;
// a malha de células do value noise: 8×8 células de 8 px — o PERÍODO do
// tile (o hash embrulha na CÉLULA: corner(8,·) == corner(0,·) por
// construção, a emenda do REPEAT é perfeita; pixel(x+64,y) ==
// pixel(x,y) provável por pin)
constexpr u32 kNoiseCells = kNoiseSize / 8;   // 8

// o hash inteiro das CÉLULAS (o módulo NO índice da célula — o tile de 64
// px fecha a emenda por construção)
inline u32 hashCell(i32 x, i32 y) {
    u32 h = (static_cast<u32>(((x % static_cast<i32>(kNoiseCells)) +
                               static_cast<i32>(kNoiseCells)) %
                              static_cast<i32>(kNoiseCells))) *
                374761393u +
            (static_cast<u32>(((y % static_cast<i32>(kNoiseCells)) +
                               static_cast<i32>(kNoiseCells)) %
                              static_cast<i32>(kNoiseCells))) *
                668265263u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return h;
}

// o ruído VALUE suavizado (a malha 8×8 de células com o hash + o smoothstep
// bilinear — o grão «brasa»: manchas suaves de 8 px, não sal branco)
inline u8 noisePixel(i32 x, i32 y) {
    // célula 8×8: o canto da célula leva o hash; o interior interpola
    const f32 gx = static_cast<f32>(x) / 8.0f;
    const f32 gy = static_cast<f32>(y) / 8.0f;
    const i32 x0 = static_cast<i32>(std::floor(gx));
    const i32 y0 = static_cast<i32>(std::floor(gy));
    f32 tx = gx - static_cast<f32>(x0);
    f32 ty = gy - static_cast<f32>(y0);
    tx = tx * tx * (3.0f - 2.0f * tx);   // smoothstep (o hash é tileável
    ty = ty * ty * (3.0f - 2.0f * ty);   //  nos inteiros — a interpolação
    const auto corner = [](i32 cx, i32 cy) -> f32 {
        return static_cast<f32>(hashCell(cx, cy) & 0xFFu) / 255.0f;
    };
    const f32 a = corner(x0, y0), b = corner(x0 + 1, y0);
    const f32 c = corner(x0, y0 + 1), d = corner(x0 + 1, y0 + 1);
    const f32 v = (a * (1.0f - tx) + b * tx) * (1.0f - ty) +
                  (c * (1.0f - tx) + d * tx) * ty;
    // o contraste da brasa: a curva empurra para os extremos (a chama vive
    // nas manchas claras, o escuro é o fundo da brasa)
    const f32 shaped = std::pow(v, 1.6f);
    return static_cast<u8>(shaped * 255.0f + 0.5f);
}

// o OFFSET do SCROLL LENTO em UV: 6 px por segundo na textura de 64 px
// (o dono pediu «scroll lento» — 9,4 s por volta completa; o mesmo t =
// o mesmo offset, o determinismo dos pins)
constexpr f32 kScrollPxPerSec = 6.0f;
inline f32 fireScrollOffset(f32 time) {
    const f32 px = time * kScrollPxPerSec;
    const f32 uv = px / static_cast<f32>(kNoiseSize);
    return uv - std::floor(uv);   // 0..1 (o wrap é permissivo — o REPEAT
                                  // do sampler repete à mesma maneira)
}

} // namespace noise2d
} // namespace vv
