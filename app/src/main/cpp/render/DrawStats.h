#pragma once
// render/DrawStats.h — métricas mínimas de render por draw (F2, status line).
#include "core/Types.h"

namespace vv {

struct DrawStats {
    u32 vertices  = 0;   // vértices/índices processados pelo draw
    u32 drawCalls = 0;
    // 0.11-W (3.5b): blocos fundidos em draws instanciados neste frame
    // (o HUD/log espelham — o «instanciados=N» do log dos blocos)
    u32 instancesMerged = 0;
};

inline DrawStats operator+(const DrawStats& a, const DrawStats& b) {
    return {a.vertices + b.vertices, a.drawCalls + b.drawCalls,
            a.instancesMerged + b.instancesMerged};
}

// 0.11-T (FASE 3.5a) — as métricas do SKINNED CULL do frame (o HUD e o
// engine.log espelham ISTO — o padrão do BlockFrameStats dos blocos, a
// MESMA fonte para o main, o harness do c33 e os pins).
struct SkinFrameStats {
    u32 skinados = 0;   // TICs skinados vistos pelo draw neste frame
    u32 culled = 0;     // skinados que o draw NÃO desenhou (fora do
                        // frustum deste frame — o número do HUD/log)
    bool any = false;   // há skinados em cena (a linha só nasce com eles)
};

} // namespace vv
