#pragma once
// ui/SceneFx.h — TRANSIÇÕES DE CENA (0.7.1, F6): fade / slide.
//
// Header-only e GL-free (o desenho vive em transitionDraw com UiContext —
// os testes leem os batches de CPU). O contrato é PURO e afervel:
//   • transitionStep avança o tempo e devolve true NO FRAME EXATO do SWAP
//     (o ponto médio — o chamador troca de cena aí, com o ecrã tapado);
//   • o swap acontece UMA vez (o flag `swapped` interna evita repetições);
//   • transitionDraw emite o overlay por cima de tudo (o ecrã tapado):
//       fade  → quad preto fullscreen com alpha em rampa 0→1→0
//       slide → quad preto que COBRE o ecrã vindo da esquerda (ida) e
//               continua para a direita (volta)
//   • nada de física/render/componentes (CLÁUSULA CALMA): só um quad.
#include "core/Types.h"
#include <string>

namespace vv {

class UiContext;

namespace ui {

struct SceneTransition {
    enum class Style : u8 { Fade = 0, Slide = 1 };

    bool active = false;
    Style style = Style::Fade;
    f32   t = 0.0f;          // tempo decorrido (s)
    f32   dur = 0.6f;        // duração TOTAL (ida + volta)
    std::string target;      // nome da cena alvo (o chamador resolve)
private:
    bool  swapped_ = false;  // o swap do midpoint já correu?
    friend bool transitionStep(SceneTransition& tr, f32 dt);
};

// avança a transição; devolve true NO frame do SWAP (ponto médio, uma
// única vez). Com a transição inativa devolve sempre false.
inline bool transitionStep(SceneTransition& tr, f32 dt) {
    if (!tr.active) {
        return false;
    }
    tr.t += dt;
    // swap NO PONTO MÉDIO — o ecrã está tapado (fade alpha=1 / slide cheio)
    const bool swapNow = !tr.swapped_ && tr.t >= tr.dur * 0.5f;
    if (tr.t >= tr.dur * 0.5f) {
        tr.swapped_ = true;
    }
    if (tr.t >= tr.dur) {
        tr.active = false;   // acabou — o overlay desaparece no próximo draw
        tr.t = tr.dur;
    }
    return swapNow;
}

// fração de cobertura do ecrã [0..1] — 1 no ponto médio (o swap), 0 nas
// pontas (entrada e saída). PURE: partilhada com os testes.
inline f32 transitionCover(const SceneTransition& tr) {
    if (!tr.active || tr.dur <= 0.0f) {
        return 0.0f;
    }
    const f32 half = tr.dur * 0.5f;
    if (tr.t <= half) {
        return tr.t / half;              // rampa de subida (0→1)
    }
    return 1.0f - (tr.t - half) / half;  // rampa de descida (1→0)
}

// desenha o overlay (por cima de tudo — o chamador desenha por último).
// Sem transição ativa não emite NADA (custo zero no frame comum).
void transitionDraw(UiContext& uictx, const SceneTransition& tr, f32 sw,
                    f32 sh);

} // namespace ui
} // namespace vv
