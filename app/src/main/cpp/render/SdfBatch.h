#pragma once
// render/SdfBatch.h — 0.12-D (D-1): o batch de WIDGETS SDF (o shader único
// de widget). Vértices acumulados na CPU; o Renderer sobe e desenha em
// endFrame pelo PROGRAMA SDF (progSdf_ — o 3.º programa da engine, ao lado
// do UI de quads e do lit).
//
// UM widget = UM quad (6 vértices, a convenção do QuadBatch::quad). O FS
// desenha TUDO (cantos SDF, bisel, especular, brilho, sombra) a partir de:
//   aPos   (loc0, vec2) — posição em px de ecrã (origem topo-esquerda)
//   aLocal (loc1, vec4) — lx/ly LOCAIS ao topo-esquerda do widget (podem
//                         ser negativos/overflow: a sombra vive fora) +
//                         hw/hh (meias-extensões)
//   aShape (loc2, vec4) — radius px + level (0=Baixo/1=Médio/2=Alto;
//                         reservado para o fogo por-elemento da D-3) + pad
//   aColTop/ aColBot (loc3/loc4, vec4) — o gradiente vertical (o mesmo
//                         contrato das cores do QuadVertex)
// Os PARÂMETROS do nível (bevelW/specDepth/glowW/sombra/AA) são UNIFORMS
// por frame (todos os widgets partilham o toggle global sdf::g_level) —
// ver Renderer::endFrame.
#include <vector>
#include "core/Types.h"

namespace vv {

struct SdfVertex {
    f32 x, y;                      // loc0: aPos
    f32 lx, ly, hw, hh;            // loc1: aLocal (lx,ly,hw,hh)
    f32 radius, level, pad0, pad1; // loc2: aShape
    f32 tr, tg, tb, ta;            // loc3: aColTop
    f32 br, bg, bb, ba;            // loc4: aColBot
};

class SdfBatch {
public:
    void reserve(size_t quads) { verts_.reserve(quads * 6); }
    void clear() { verts_.clear(); }

    void vertex(f32 px, f32 py, f32 lx, f32 ly, f32 hw, f32 hh, f32 radius,
                u8 level, const f32 top[4], const f32 bot[4]) {
        const SdfVertex v{px,     py,     lx,     ly,     hw,     hh,
                          radius, static_cast<f32>(level), 0.0f,   0.0f,
                          top[0], top[1], top[2], top[3],
                          bot[0], bot[1], bot[2], bot[3]};
        verts_.push_back(v);
    }

    const SdfVertex* vertices() const { return verts_.data(); }
    u32    vertexCount() const { return static_cast<u32>(verts_.size()); }
    size_t vertexBytes() const { return verts_.size() * sizeof(SdfVertex); }
    bool   empty() const { return verts_.empty(); }

private:
    std::vector<SdfVertex> verts_;
};

} // namespace vv
