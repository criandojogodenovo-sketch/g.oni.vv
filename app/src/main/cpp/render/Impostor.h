#pragma once
// render/Impostor.h — 0.11-W (3.75b): O IMPOSTOR (PNG 3D por distância).
//
// O 3.º sistema da campanha 0.11-W (render systems): um TIC além da
// distância dele desenha UM quad com o PNG alfa da silhueta (o impostor)
// em vez dos blocos — o dc do TIC cai de ~863 (o instancing já fundiu os
// clones; os blocos continuam 1 dc cada) para 1. Para mundos grandes (a
// cidade de N prédios) é o que mantém o frame vivo ao longe.
//
// TUDO AQUI É PURO (GL-free, afervel no CI): a DECISÃO (a distância — o
// MESMO contrato exclusivo-no-lado-longe do lodActive da 3.5a), a
// MATRIZ do billboard (centro no AABB do TIC, a normal +Z PARA a câmara,
// o lado = a MAIOR extensão do AABB — conservativo: nunca menor que o
// modelo) e o QUAD unitário (4 verts, uv com v=0 no TOPO — a 1.ª linha
// do PNG é a de cima; o upload GL de sempre põe a linha 0 em v=0).
//
// O PNG vive em impostors/<stem>.png (o MESMO padrão do thumbs/ — o
// ImpostorIndex gera sob demanda com o pico de RAM de UM bloco) e o draw
// é o drawMesh de sempre com DrawProps{alphaMode=1 MASK} — o alpha do
// PNG corta o quad (o shader real já descarta desde a 0.11-M FASE 2; o
// rasterizador do stub ganhou o discard honesto NESTA sub-fase).
//
// O BRILHO (a derivação do kImpBaseColor): o quad é uma SUPERFÍCIE para
// o lit de sempre — c = 0.16 + 0.58·(n·L)·texel com n = a normal do
// billboard (PARA a câmara), (n·L) = 0.2891 FIXO (o kLightDir da casa).
// O PNG carrega o lambert CRU (d = max(n·L, 0) do render software, SEM
// ambient — o mesmo d da thumb, sem o 0.16). Com baseColor k:
//   c = 0.16 + 0.58·0.2891·k·d  —  k = 0.62/(0.58·0.2891) = 3.699
// produz EXATAMENTE a família do sombreado da casa (0.16 + 0.62·d — o
// MESMO alcance da thumb: 0.16..0.78), sem saturação (d ∈ [0,1]) e sem
// NENHUMA mudança ao shader (o uBaseColor é do draw — a 0.11-M FASE 2 já
// o transporta). O pop no limiar fica subtil (o impostor ≈ o modelo ao
// pé — o pin C33 do dono valida).
#include "assets/Assets.h"
#include "core/Types.h"
#include "math/Math.h"
#include <algorithm>
#include <cmath>

namespace vv {
namespace impostor {

// o tamanho do PNG do impostor (o tile da casa — o MESMO kThumbSize)
constexpr u32 kImpostorSize = 256u;
// o ORÇAMENTO do render sob demanda (o MESMO teto da thumb — o pico de
// RAM é UM bloco + o acumulado; o impostor de um modelo de 863 blocos
// lê PELA TABELA até encher 4096 tris, nunca o ficheiro inteiro)
constexpr u32 kImpostorMaxTris = 4096u;
// o diretório (o par do thumbs/ — "assets/city.gmesh" → "impostors/city.png")
constexpr char kDir[] = "impostors";
// o fator de brilho do draw (a derivação no cabeçalho; baseColor do
// DrawProps — alphaMode 1 MASK + cutoff 0.5 completam o draw)
constexpr f32 kImpBaseColor = 3.699f;

// o centro do AABB transformado pelo model (O MESMO centro que o
// blockDist do drawTics usa para o z dos blends e para a vaga do stream —
// UMA fonte de verdade)
Vec3 centerOf(const Mat4& model, const Vec3& mn, const Vec3& mx);

// a distância do camPos ao centro (unidades de mundo)
f32 distOf(const Mat4& model, const Vec3& mn, const Vec3& mx,
           const Vec3& camPos);

// A DECISÃO: o TIC além da distância desenha o impostor. O limiar é
// EXCLUSIVO no lado longe (no próprio limiar ainda é o normal — o MESMO
// contrato do lodActive da 3.5a). impostorDistance <= 0 = OFF (o
// default — o comportamento byte a byte do pré-3.75b).
bool active(const Mat4& model, const Vec3& mn, const Vec3& mx,
            const Vec3& camPos, f32 impostorDistance);

// a MATRIZ do billboard: col0 = right·(lado/2), col1 = up·(lado/2),
// col2 = fwd (PARA a câmara), col3 = o centro. O lado = a MAIOR extensão
// do AABB escalada pelas colunas do model (conservativo). false =
// degenerado (extensão nula ou a câmara DENTRO do centro — nada
// razoável a desenhar; o chamador segue o caminho normal).
bool billboard(const Mat4& model, const Vec3& mn, const Vec3& mx,
               const Vec3& camPos, Mat4& out);

// o QUAD unitário (4 verts CCW visto de +Z, 6 índices; uv com v=0 no
// TOPO do PNG). O chamador faz o upload UMA vez (o Mesh do drawTics) —
// o draw é o drawMesh de sempre com as props MASK.
void quadMesh(MeshData& out);

// ---- implementação (PURO — inline no cabeçalho, zero TU novo) -----------

inline Vec3 centerOf(const Mat4& model, const Vec3& mn, const Vec3& mx) {
    const f32 cx = (mn.x + mx.x) * 0.5f;
    const f32 cy = (mn.y + mx.y) * 0.5f;
    const f32 cz = (mn.z + mx.z) * 0.5f;
    return Vec3{model.m[0] * cx + model.m[4] * cy + model.m[8] * cz +
                    model.m[12],
                model.m[1] * cx + model.m[5] * cy + model.m[9] * cz +
                    model.m[13],
                model.m[2] * cx + model.m[6] * cy + model.m[10] * cz +
                    model.m[14]};
}

inline f32 distOf(const Mat4& model, const Vec3& mn, const Vec3& mx,
                  const Vec3& camPos) {
    const Vec3 c = centerOf(model, mn, mx);
    const f32 dx = c.x - camPos.x;
    const f32 dy = c.y - camPos.y;
    const f32 dz = c.z - camPos.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

inline bool active(const Mat4& model, const Vec3& mn, const Vec3& mx,
                   const Vec3& camPos, f32 impostorDistance) {
    if (impostorDistance <= 0.0f) {
        return false;   // OFF — o byte a byte do pré-3.75b
    }
    return distOf(model, mn, mx, camPos) > impostorDistance;
}

inline bool billboard(const Mat4& model, const Vec3& mn, const Vec3& mx,
                      const Vec3& camPos, Mat4& out) {
    // o lado = a MAIOR extensão do AABB escalada pelas colunas do model
    // (o MESMO produto do blockDist — a escala do TIC conta)
    const f32 ex = (mx.x - mn.x), ey = (mx.y - mn.y), ez = (mx.z - mn.z);
    const f32 sx = std::sqrt(model.m[0] * model.m[0] +
                             model.m[1] * model.m[1] +
                             model.m[2] * model.m[2]);
    const f32 sy = std::sqrt(model.m[4] * model.m[4] +
                             model.m[5] * model.m[5] +
                             model.m[6] * model.m[6]);
    const f32 sz = std::sqrt(model.m[8] * model.m[8] +
                             model.m[9] * model.m[9] +
                             model.m[10] * model.m[10]);
    const f32 side = std::max(std::max(ex * sx, ey * sy), ez * sz);
    if (!(side > 1e-6f)) {
        return false;   // AABB nulo — nada razoável a desenhar
    }
    const Vec3 center = centerOf(model, mn, mx);
    // fwd = PARA A CÂMARA (camPos − center): a normal do quad (+Z, a col2)
    // tem de apontar AO OLHO — é o que o cull do draw de espera (o front
    // CCW visto da câmara) e o que a derivação do kImpBaseColor assume
    // (n·L com n PARA a câmara). O contrário desenhava o VERSO (cull
    // mata) — o pin do billboard apanhou-o.
    Vec3 fwd{camPos.x - center.x, camPos.y - center.y, camPos.z - center.z};
    const f32 fl = std::sqrt(fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z);
    if (!(fl > 1e-6f)) {
        return false;   // a câmara DENTRO do centro — o quad não resolve
    }
    fwd.x /= fl; fwd.y /= fl; fwd.z /= fl;
    // right = worldUp × fwd (a vertical de sempre); a câmara na VERTICAL
    // (a vista de topo do dono) troca o up de referência — nunca NaN
    Vec3 worldUp{0.0f, 1.0f, 0.0f};
    if (std::fabs(worldUp.x * fwd.x + worldUp.y * fwd.y + worldUp.z * fwd.z) >
        0.999f) {
        worldUp = Vec3{1.0f, 0.0f, 0.0f};
    }
    Vec3 right{worldUp.y * fwd.z - worldUp.z * fwd.y,
               worldUp.z * fwd.x - worldUp.x * fwd.z,
               worldUp.x * fwd.y - worldUp.y * fwd.x};
    const f32 rl = std::sqrt(right.x * right.x + right.y * right.y +
                             right.z * right.z);
    if (!(rl > 1e-6f)) {
        return false;
    }
    right.x /= rl; right.y /= rl; right.z /= rl;
    const Vec3 up{fwd.y * right.z - fwd.z * right.y,
                  fwd.z * right.x - fwd.x * right.z,
                  fwd.x * right.y - fwd.y * right.x};
    const f32 half = side * 0.5f;
    out = Mat4::identity();
    out.m[0] = right.x * half; out.m[1] = right.y * half;
    out.m[2] = right.z * half;
    out.m[4] = up.x * half;    out.m[5] = up.y * half;
    out.m[6] = up.z * half;
    out.m[8] = fwd.x;          out.m[9] = fwd.y;      out.m[10] = fwd.z;
    out.m[12] = center.x;      out.m[13] = center.y;  out.m[14] = center.z;
    return true;
}

inline void quadMesh(MeshData& out) {
    out = MeshData{};
    // CCW visto de +Z (o front da casa — o cull do draw de sempre manda);
    // uv com v=0 no TOPO: a 1.ª linha do PNG (o upload GL de sempre põe a
    // linha 0 em v=0) encontra o TOPO do billboard
    const Vec3 P[4] = {Vec3{-1.0f, -1.0f, 0.0f}, Vec3{1.0f, -1.0f, 0.0f},
                       Vec3{1.0f, 1.0f, 0.0f}, Vec3{-1.0f, 1.0f, 0.0f}};
    const Vec2 UV[4] = {Vec2{0.0f, 1.0f}, Vec2{1.0f, 1.0f}, Vec2{1.0f, 0.0f},
                        Vec2{0.0f, 0.0f}};
    for (int k = 0; k < 4; ++k) {
        Vertex v;
        v.pos = P[k];
        v.normal = Vec3{0.0f, 0.0f, 1.0f};
        v.uv = UV[k];
        out.vertices.push_back(v);
    }
    const u16 IDX[6] = {0, 1, 2, 0, 2, 3};
    out.indices.assign(IDX, IDX + 6);
}

} // namespace impostor
} // namespace vv
