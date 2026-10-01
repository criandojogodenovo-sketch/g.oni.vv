// ui/CamGizmo.cpp — implementação do gizmo da câmara (0.7.7).
//
// Tudo projetado para px de ecrã (a técnica dos gizmos: gizmo::projectPoint
// + distToSegmentPx) — o hit-test e o desenho partilham a MESMA geometria.
// O drag usa âNCORAS (pose final = âncora + delta; nunca acumula).
#include "ui/CamGizmo.h"

#include <cmath>

#include "components/CameraComp.h"
#include "components/Transform3D.h"
#include "core/CameraUtil.h"
#include "core/Scene.h"
#include "render/Camera.h"
#include "ui/Gizmo.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"

namespace vv {
namespace camgizmo {

namespace {

// dims do corpo (unidades de MUNDO — a caixa à volta do olho)
constexpr f32 kBodyHalfW = 0.30f;
constexpr f32 kBodyHalfH = 0.22f;
constexpr f32 kBodyHalfD = 0.30f;    // frente/trás ao longo do eixo de visão
constexpr f32 kBodyBack  = 0.34f;    // quanto do olho para trás
constexpr f32 kLensDist  = 0.52f;    // plano da lente à frente do olho
constexpr f32 kLensHalfW = 0.17f;
constexpr f32 kLensHalfH = 0.12f;
constexpr f32 kLensHalfD = 0.10f;

// px de ecrã
constexpr f32 kLineW     = 3.0f;     // traço do wireframe
constexpr f32 kLineWSel  = 4.0f;     // câmara selecionada
constexpr f32 kHandlePx  = 26.0f;    // quadrado do handle (lado)
constexpr f32 kHitPx     = 22.0f;    // hit-test de segmentos (como eixos)
constexpr f32 kHandleHitPx = 30.0f;  // hit-test de handles (prioritário)

f32 deg2rad(f32 d) { return d * 0.01745329252f; }
f32 rad2deg(f32 r) { return r * 57.29577951f; }

f32 snapStep(f32 v, f32 step) {
    return step > 0.0f ? std::round(v / step) * step : v;
}

// emite uma aresta do mundo (projeta; atrás da câmara → não desenha)
void edge(UiContext& ui, const Mat4& vp, f32 sw, f32 sh, const Vec3& a,
          const Vec3& b, f32 w, const f32 col[4]) {
    f32 ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
    if (!gizmo::projectPoint(vp, a, sw, sh, ax, ay)) {
        return;
    }
    if (!gizmo::projectPoint(vp, b, sw, sh, bx, by)) {
        return;
    }
    ui.drawLine(ax, ay, bx, by, w, col);
}

// caixa (8 cantos: 0..3 frente [+z local], 4..7 trás) → 12 arestas
void drawBox(UiContext& ui, const Mat4& vp, f32 sw, f32 sh, const Vec3 c[8],
             f32 w, const f32 col[4]) {
    for (int i = 0; i < 4; ++i) {
        edge(ui, vp, sw, sh, c[i], c[(i + 1) % 4], w, col);          // frente
        edge(ui, vp, sw, sh, c[4 + i], c[4 + (i + 1) % 4], w, col);  // trás
        edge(ui, vp, sw, sh, c[i], c[4 + i], w, col);                // ligação
    }
}

// caixa orientada pela base da câmara, centrada em (center − fwd·cx etc.)
void boxFromBasis(const Frustum& f, const Vec3& center, f32 hw, f32 hh,
                  f32 hd, Vec3 out[8]) {
    // índices: 0..3 = face FRONTAL (+fwd·hd), 4..7 = trás
    const Vec3 corners[4] = {
        center + f.fwd * hd + f.right * hw + f.up * hh,
        center + f.fwd * hd - f.right * hw + f.up * hh,
        center + f.fwd * hd - f.right * hw - f.up * hh,
        center + f.fwd * hd + f.right * hw - f.up * hh,
    };
    for (int i = 0; i < 4; ++i) {
        out[i] = corners[i];
        out[4 + i] = corners[i] - f.fwd * (2.0f * hd);
    }
}

} // namespace

bool visible(bool playMode, bool uiMode) {
    // como os gizmos: SÓ em editor (nunca em play); o modo UI não tem
    // viewport 3D — o frustum também não se desenha lá
    return !playMode && !uiMode;
}

// ---- geometria -----------------------------------------------------------------

void planeHalfExtents(const CameraComp& cam, f32 dist, f32 aspect,
                      f32& halfW, f32& halfH) {
    if (cam.projection == CameraComp::Projection::Orthographic) {
        halfH = cam.orthoSize;   // meia-altura FIXA no orto
    } else {
        halfH = std::tan(deg2rad(cam.fovY) * 0.5f) * dist;
    }
    halfW = halfH * aspect;
}

Frustum computeFrustum(const Transform3D& tr, const CameraComp& cam,
                       f32 aspect) {
    Frustum f;
    f.pos   = tr.pos;
    f.fwd   = tr.rot.rotate(Vec3{0.0f, 0.0f, -1.0f});
    f.right = tr.rot.rotate(Vec3{1.0f, 0.0f, 0.0f});
    f.up    = tr.rot.rotate(Vec3{0.0f, 1.0f, 0.0f});

    // corpo: caixa atrás do olho (o olho fica NO PLANO frontal da caixa —
    // como uma máquina fotográfica: o corpo atrás, a lente à frente)
    boxFromBasis(f, f.pos - f.fwd * (kBodyBack - kBodyHalfD), kBodyHalfW,
                 kBodyHalfH, kBodyHalfD, f.box);
    // lente: caixa pequena à frente (o "olho" da câmara)
    boxFromBasis(f, f.pos + f.fwd * kLensDist, kLensHalfW, kLensHalfH,
                 kLensHalfD, f.lens);

    // retângulos near/far (a MESMA matemática da projeção — aferida)
    f32 nw = 0.0f, nh = 0.0f, fw2 = 0.0f, fh2 = 0.0f;
    planeHalfExtents(cam, cam.nearZ, aspect, nw, nh);
    planeHalfExtents(cam, cam.farZ, aspect, fw2, fh2);
    const Vec3 nc = f.pos + f.fwd * cam.nearZ;
    const Vec3 fc = f.pos + f.fwd * cam.farZ;
    f.farCenter = fc;
    const Vec3 cornersOf[4] = {
        Vec3{1.0f, 1.0f, 0.0f}, Vec3{-1.0f, 1.0f, 0.0f},
        Vec3{-1.0f, -1.0f, 0.0f}, Vec3{1.0f, -1.0f, 0.0f}};
    for (int i = 0; i < 4; ++i) {
        f.nearC[i] = nc + f.right * (cornersOf[i].x * nw) +
                     f.up * (cornersOf[i].y * nh);
        f.farC[i] = fc + f.right * (cornersOf[i].x * fw2) +
                    f.up * (cornersOf[i].y * fh2);
    }
    return f;
}

// ---- desenho ---------------------------------------------------------------------

void drawFrustum(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
                 const Frustum& f, bool selected) {
    const f32* col = theme::kTheme.brand;   // cor de gizmo/marca
    const f32 w = selected ? kLineWSel : kLineW;

    drawBox(ui, vp, sw, sh, f.box, w, col);    // corpo
    drawBox(ui, vp, sw, sh, f.lens, w, col);   // lente
    for (int i = 0; i < 4; ++i) {
        // near + cone near→far + far
        edge(ui, vp, sw, sh, f.nearC[i], f.nearC[(i + 1) % 4], w, col);
        edge(ui, vp, sw, sh, f.farC[i], f.farC[(i + 1) % 4], w, col);
        edge(ui, vp, sw, sh, f.nearC[i], f.farC[i], w, col);
    }
    // linha de visão central (do corpo ao centro do far)
    edge(ui, vp, sw, sh, f.pos + f.fwd * (kLensDist + kLensHalfD),
         f.farCenter, w, col);

    // handles: SÓ na câmara selecionada (4 cantos + centro do far)
    if (!selected) {
        return;
    }
    const f32 ink[4] = {theme::kTheme.brandInk[0], theme::kTheme.brandInk[1],
                        theme::kTheme.brandInk[2], theme::kTheme.brandInk[3]};
    for (int i = 0; i < 4; ++i) {
        f32 hx = 0.0f, hy = 0.0f;
        if (gizmo::projectPoint(vp, f.farC[i], sw, sh, hx, hy)) {
            ui.panel(hx - kHandlePx * 0.5f, hy - kHandlePx * 0.5f, kHandlePx,
                     kHandlePx, col);
            ui.frame(hx - kHandlePx * 0.5f, hy - kHandlePx * 0.5f, kHandlePx,
                     kHandlePx, 2.0f, ink);
        }
    }
    f32 cx = 0.0f, cy = 0.0f;
    if (gizmo::projectPoint(vp, f.farCenter, sw, sh, cx, cy)) {
        ui.panel(cx - kHandlePx * 0.5f, cy - kHandlePx * 0.5f, kHandlePx,
                 kHandlePx, ink);
        ui.frame(cx - kHandlePx * 0.5f, cy - kHandlePx * 0.5f, kHandlePx,
                 kHandlePx, 2.0f, col);
    }
}

void drawAll(UiContext& ui, Scene& scene, const Mat4& vp, f32 sw, f32 sh,
             Handle selected) {
    if (sw <= 1.0f || sh <= 1.0f) {
        return;
    }
    const f32 aspect = sw / sh;
    scene.forEachActive([&](Tic& t) {
        if (!t.visible) {
            return;
        }
        const Transform3D* tr = t.getComponent<Transform3D>();
        const CameraComp* cam = t.getComponent<CameraComp>();
        if (!tr || !cam) {
            return;
        }
        const Frustum f = computeFrustum(*tr, *cam, aspect);
        drawFrustum(ui, vp, sw, sh, f, t.handle == selected);
    });
}

// ---- hit-test ----------------------------------------------------------------------

int pickHandle(const Mat4& vp, f32 sw, f32 sh, const Frustum& f, f32 px,
               f32 py) {
    int best = 0;
    f32 bestD = kHandleHitPx;
    auto tryHandle = [&](int id, const Vec3& world) {
        f32 hx = 0.0f, hy = 0.0f;
        if (!gizmo::projectPoint(vp, world, sw, sh, hx, hy)) {
            return;
        }
        const f32 d = std::sqrt((px - hx) * (px - hx) + (py - hy) * (py - hy));
        if (d < bestD) {
            bestD = d;
            best = id;
        }
    };
    for (int i = 0; i < 4; ++i) {
        tryHandle(1 + i, f.farC[i]);   // 1..4 = cantos (fov)
    }
    tryHandle(5, f.farCenter);         // 5 = centro (far)
    return best;
}

namespace {

// distância do toque à CAIXA projetada (12 arestas) — helper do pickCameraTic
f32 distToBox(const Mat4& vp, f32 sw, f32 sh, const Vec3 c[8], f32 px,
              f32 py) {
    f32 best = 1e9f;
    for (int i = 0; i < 4; ++i) {
        f32 a[2] = {0, 0}, b[2] = {0, 0}, c2[2] = {0, 0}, d2[2] = {0, 0};
        if (!gizmo::projectPoint(vp, c[i], sw, sh, a[0], a[1]) ||
            !gizmo::projectPoint(vp, c[(i + 1) % 4], sw, sh, b[0], b[1]) ||
            !gizmo::projectPoint(vp, c[4 + i], sw, sh, c2[0], c2[1]) ||
            !gizmo::projectPoint(vp, c[4 + (i + 1) % 4], sw, sh, d2[0],
                                 d2[1])) {
            return 1e9f;
        }
        best = (std::min)(best, gizmo::distToSegmentPx(px, py, a[0], a[1],
                                                       b[0], b[1]));
        best = (std::min)(best, gizmo::distToSegmentPx(px, py, c2[0], c2[1],
                                                       d2[0], d2[1]));
        f32 e[2] = {0, 0};
        if (gizmo::projectPoint(vp, c[4 + i], sw, sh, e[0], e[1])) {
            best = (std::min)(best, gizmo::distToSegmentPx(px, py, a[0], a[1],
                                                           e[0], e[1]));
        }
    }
    return best;
}

} // namespace

Handle pickCameraTic(Scene& scene, const Mat4& vp, f32 sw, f32 sh, f32 px,
                     f32 py) {
    if (sw <= 1.0f || sh <= 1.0f) {
        return Handle::invalid();
    }
    const f32 aspect = sw / sh;
    Handle best = Handle::invalid();
    f32 bestD = kHitPx;
    scene.forEachActive([&](Tic& t) {
        if (!t.visible) {
            return;
        }
        const Transform3D* tr = t.getComponent<Transform3D>();
        const CameraComp* cam = t.getComponent<CameraComp>();
        if (!tr || !cam) {
            return;
        }
        const Frustum f = computeFrustum(*tr, *cam, aspect);
        f32 d = distToBox(vp, sw, sh, f.box, px, py);
        d = (std::min)(d, distToBox(vp, sw, sh, f.lens, px, py));
        // TODOS os segmentos que o DESENHO emite: near, far, cone, linha de
        // visão — o hit-test e o visual partilham a geometria
        auto seg = [&](const Vec3& a3, const Vec3& b3) {
            f32 a[2] = {0, 0}, b[2] = {0, 0};
            if (gizmo::projectPoint(vp, a3, sw, sh, a[0], a[1]) &&
                gizmo::projectPoint(vp, b3, sw, sh, b[0], b[1])) {
                d = (std::min)(d, gizmo::distToSegmentPx(px, py, a[0], a[1],
                                                         b[0], b[1]));
            }
        };
        for (int i = 0; i < 4; ++i) {
            seg(f.nearC[i], f.nearC[(i + 1) % 4]);   // retângulo near
            seg(f.farC[i], f.farC[(i + 1) % 4]);     // retângulo far
            seg(f.nearC[i], f.farC[i]);              // cone
        }
        seg(f.pos + f.fwd * (kLensDist + kLensHalfD), f.farCenter);  // visão
        if (d < bestD) {
            bestD = d;
            best = t.handle;
        }
    });
    return best;
}

// ---- drag ---------------------------------------------------------------------------

f32 dragFar(f32 anchorFar, const Vec3& hit0, const Vec3& hit1,
            const Vec3& camFwd, bool snap) {
    f32 far = anchorFar + dot(hit1 - hit0, camFwd);
    if (snap) {
        far = snapStep(far, 1.0f);
    }
    if (far < CameraComp::kMinFar) {
        far = CameraComp::kMinFar;
    }
    if (far > CameraComp::kMaxFar) {
        far = CameraComp::kMaxFar;
    }
    return far;
}

f32 dragFov(f32 anchorFovDeg, f32 d0, f32 d1, bool snap) {
    if (d0 < 4.0f) {
        return anchorFovDeg;   // âncora degenerada (dedo em cima do centro)
    }
    f32 fov = anchorFovDeg * (d1 / d0);
    if (snap) {
        fov = snapStep(fov, 5.0f);
    }
    if (fov < CameraComp::kMinFov) {
        fov = CameraComp::kMinFov;
    }
    if (fov > CameraComp::kMaxFov) {
        fov = CameraComp::kMaxFov;
    }
    return fov;
}

f32 dragScaleToFov(f32 anchorFovDeg, f32 anchorValue, bool ortho, f32 d0,
                   f32 d1, bool snap) {
    // o MESMO fator do dragScaleUniform dos gizmos (âncoras, nunca acumula)
    if (d0 < 4.0f) {
        return anchorValue;
    }
    f32 f = d1 / d0;
    if (snap) {
        f = snapStep(f, gizmo::kSnapScale);
    }
    if (f < 0.05f) {
        f = 0.05f;
    }
    const f32 v = (ortho ? anchorValue : anchorFovDeg) * f;
    if (ortho) {
        return v < 0.1f ? 0.1f : v;
    }
    return v < CameraComp::kMinFov
               ? CameraComp::kMinFov
               : (v > CameraComp::kMaxFov ? CameraComp::kMaxFov : v);
}

// ---- câmara de jogo -------------------------------------------------------------------

Mat4 gameView(const Transform3D& tr) {
    const Vec3 eye = tr.pos;
    const Vec3 fwd = tr.rot.rotate(Vec3{0.0f, 0.0f, -1.0f});
    const Vec3 up = tr.rot.rotate(Vec3{0.0f, 1.0f, 0.0f});
    // degeneração (olhar colinear com o up local — não ocorre com up≠fwd):
    // lookAt normaliza internamente; guard: fwd ~ 0 impossível (quat unit.)
    return Mat4::lookAt(eye, eye + fwd, up);
}

Mat4 gameProj(const CameraComp& cam, f32 aspect) {
    if (cam.projection == CameraComp::Projection::Orthographic) {
        const f32 hh = cam.orthoSize;
        const f32 hw = hh * aspect;
        return Mat4::ortho(-hw, hw, -hh, hh, cam.nearZ, cam.farZ);
    }
    return Mat4::perspective(deg2rad(cam.fovY), aspect, cam.nearZ, cam.farZ);
}

// ---- alinhar à vista -------------------------------------------------------------------

void alignToView(Transform3D& tr, const Camera& orbit) {
    // a orbit: dir(target→eye) = (cp·sy, sp, cp·cy); a câmara da cena olha
    // o CONTRÁRIO (eye→target) = −dir. fromEuler(−pitch, yaw, 0) mapeia o
    // −Z local nessa direção (derivado no relatório; testado no CI).
    tr.pos = orbit.eye();
    tr.rot = Quat::fromEuler(-orbit.pitch, orbit.yaw, 0.0f);
    tr.updateWorld();
}

} // namespace camgizmo
} // namespace vv
