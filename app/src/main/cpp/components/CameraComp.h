#pragma once
// components/CameraComp.h — COMPONENTE CÂMARA (0.7.7).
//
// Um TIC com CameraComp é uma CÂMARA DE CENA: perspetiva configurável
// (fovY em graus, near/far) ou projeção ortográfica (orthoSize = meia-altura
// em unidades de mundo). A POSE vem do Transform3D do mesmo TIC (pos =
// posição do olho; orientação: o eixo −Z local é a direção de visão, +Y é o
// up — a convenção lookAt).
//
// `active`: a câmara ATIVA da cena (uma só — core/CameraUtil assegura o
// invariante no load e nos toggles). Em PLAY a cena renderiza pela câmara
// ativa; sem câmara ativa (ou sem CameraComp nenhum) o Play usa a orbit de
// edição como fallback (o EDITOR usa SEMPRE a orbit — o frustum wireframe
// da câmara é um gizmo, nunca o render).
//
// Nomes: a classe chama-se CameraComp (o nome Camera já é a orbit de
// render/Camera.h); no REGISTRO e no .goni o tipo é "Camera" (spec 0.7.7).
// Serializado: "fov" (graus), "near", "far", "proj" ("persp"|"ortho"),
// "orthoSize", "active", "frustum" (0.7.10 — toggle de visibilidade do
// gizmo; omitido quando true) — defaults omitidos (ficheiros 0.7.6 abrem
// limpos).
#include "core/Component.h"
#include "core/Types.h"

namespace vv {

class CameraComp : public Component {
public:
    enum class Projection : u8 {
        Perspective = 0,
        Orthographic = 1,
    };

    static constexpr f32 kDefaultFov  = 60.0f;    // graus
    static constexpr f32 kDefaultNear = 0.5f;
    static constexpr f32 kDefaultFar  = 500.0f;
    static constexpr f32 kDefaultOrthoSize = 5.0f;   // meia-altura (mundo)

    // limites de edição (Inspector/handles — nunca degenera)
    static constexpr f32 kMinFov  = 1.0f;
    static constexpr f32 kMaxFov  = 170.0f;
    static constexpr f32 kMinNear = 0.05f;
    static constexpr f32 kMinFar  = 0.2f;
    static constexpr f32 kMaxFar  = 2000.0f;

    f32        fovY   = kDefaultFov;      // graus (projeção perspetiva)
    f32        nearZ  = kDefaultNear;
    f32        farZ   = kDefaultFar;
    Projection projection = Projection::Perspective;
    f32        orthoSize = kDefaultOrthoSize;
    bool       active = true;   // UMA ativa por cena (CameraUtil)
    bool       showFrustum = true;   // 0.7.10: toggle do gizmo no Inspector
                                     // (esconder o frustum quando polui; o
                                     // render no Play NÃO mudar)

    static const char* projectionName(Projection p) {
        return p == Projection::Orthographic ? "ortografica" : "perspetiva";
    }
};

} // namespace vv
