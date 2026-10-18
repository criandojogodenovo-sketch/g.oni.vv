// ui/SceneFx.cpp — desenho das transições de cena (0.7.1).
//
// Tema mono: um QUAD PRETO é todo o efeito (o mesmo vocabulário do editor
// — sem cores novas, sem assets). O fade usa alpha (blend já ativo no pass
// UI); o slide cobre o ecrã vindo da esquerda e sai pela direita.
#include "ui/SceneFx.h"
#include "ui/UiContext.h"

namespace vv {
namespace ui {

void transitionDraw(UiContext& uictx, const SceneTransition& tr, f32 sw,
                    f32 sh) {
    if (!tr.active || sw <= 0.0f || sh <= 0.0f) {
        return;
    }
    const f32 cover = transitionCover(tr);
    if (cover <= 0.0f) {
        return;
    }
    const f32 black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    if (tr.style == SceneTransition::Style::Fade) {
        const f32 quad[4] = {0.0f, 0.0f, 0.0f, cover};   // alpha em rampa
        uictx.panel(0.0f, 0.0f, sw, sh, quad);
    } else {
        // slide: o quad entra pela ESQUERDA até cobrir (cover→1) e sai
        // pela DIREITA (cover 1→0 desloca para a direita)
        const f32 w = sw * cover;
        f32 x = 0.0f;
        if (tr.t > tr.dur * 0.5f) {
            x = sw - w;   // volta: o bordo esquerdo afasta-se para a direita
        }
        uictx.panel(x, 0.0f, w, sh, black);
    }
}

} // namespace ui
} // namespace vv
