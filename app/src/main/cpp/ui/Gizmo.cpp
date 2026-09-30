// ui/Gizmo.cpp — implementação dos gizmos de transformação (0.6.9).
//
// Tudo em espaço de MUNDO (sem hierarquia). O hit-test projeta a geometria
// para o ecrã (distâncias em px — o mesmo critério do desenho); o drag é
// raio×plano com âncoras (a pose final = âncora + delta, nunca acumulada).
// Ver ui/Gizmo.h para as convenções e a exceção documentada das cores.
#include "ui/Gizmo.h"
#include "render/Camera.h"
#include "ui/UiContext.h"
#include <cmath>

namespace vv {
namespace gizmo {

const f32* axisColor(Axis a, bool hovered) {
    if (hovered) {
        return kAxisHover;
    }
    switch (a) {
        case Axis::X:
        case Axis::YZ:      return kAxisX;     // plano YZ move ao longo de X
        case Axis::Y:
        case Axis::XZ:      return kAxisY;     // plano XZ move ao longo de Y
        case Axis::Z:
        case Axis::XY:      return kAxisZ;     // plano XY move ao longo de Z
        case Axis::Center:   return kAxisDim;
        case Axis::None:     break;
    }
    return kAxisDim;
}

bool visible(bool playMode, bool hasSelection) {
    // só em EDITOR — em PLAY a UI de edição não existe (0.6.8)
    return !playMode && hasSelection;
}

// ---- base de vista -------------------------------------------------------------

ViewBasis viewBasis(const Camera& cam, f32 aspect) {
    // Camera: eye = target + dist * dir; dir aponta do target PARA o eye
    // (render/Camera.cpp). fwd = −dir (do eye para a cena).
    const Vec3 eye = cam.eye();
    const f32  sy = std::sin(cam.yaw);
    const f32  cy = std::cos(cam.yaw);
    const f32  sp = std::sin(cam.pitch);
    const f32  cp = std::cos(cam.pitch);
    const Vec3 dir{cp * sy, sp, cp * cy};      // target → eye
    ViewBasis b;
    b.eye = eye;
    b.fwd = normalized(Vec3{-dir.x, -dir.y, -dir.z});
    // right = fwd × up_mundo (se não degenerado); up = right × fwd
    const Vec3 worldUp{0.0f, 1.0f, 0.0f};
    Vec3 r = cross(b.fwd, worldUp);
    if (length(r) < 1e-5f) {
        // câmara a olhar quase na vertical: fallback do right no eixo X
        r = Vec3{1.0f, 0.0f, 0.0f};
    }
    b.right = normalized(r);
    b.up = normalized(cross(b.right, b.fwd));
    b.tanHalfFov = std::tan(cam.fovY * 0.5f);
    b.aspect = aspect;
    return b;
}

Vec3 screenRayDir(const ViewBasis& b, f32 px, f32 py, f32 sw, f32 sh) {
    // NDC (y do ecrã é para baixo; NDC y para cima)
    const f32 ndcX = (2.0f * px / (sw > 1.0f ? sw : 1.0f)) - 1.0f;
    const f32 ndcY = 1.0f - (2.0f * py / (sh > 1.0f ? sh : 1.0f));
    // ponto no plano da imagem a 1 u à frente: fwd + right*kx + up*ky
    const Vec3 p = b.fwd + b.right * (ndcX * b.tanHalfFov * b.aspect) +
                   b.up * (ndcY * b.tanHalfFov);
    return normalized(p);
}

Vec3 planeHit(const ViewBasis& b, const Vec3& n, const Vec3& planeOrigin,
              f32 px, f32 py, f32 sw, f32 sh, bool& anyHit) {
    anyHit = false;
    const Vec3 d = screenRayDir(b, px, py, sw, sh);
    const f32 denom = dot(d, n);
    if (std::fabs(denom) < 1e-5f) {
        return Vec3{};   // raio ~ paralelo ao plano — indecidido
    }
    const f32 t = dot(planeOrigin - b.eye, n) / denom;
    if (t <= 0.0f) {
        return Vec3{};   // plano atrás da câmara
    }
    anyHit = true;
    return b.eye + d * t;
}

// ---- projeção --------------------------------------------------------------------

bool projectPoint(const Mat4& vp, const Vec3& p, f32 sw, f32 sh,
                  f32& sx, f32& sy) {
    f32 clip[4];
    Mat4::transformPoint4(vp, p, clip);
    if (clip[3] <= 1e-5f) {
        return false;   // atrás da câmara / degenerado
    }
    const f32 invW = 1.0f / clip[3];
    sx = (clip[0] * invW * 0.5f + 0.5f) * sw;
    sy = (1.0f - (clip[1] * invW * 0.5f + 0.5f)) * sh;   // y para baixo
    return true;
}

f32 distToSegmentPx(f32 px, f32 py, f32 ax, f32 ay, f32 bx, f32 by) {
    const f32 abx = bx - ax;
    const f32 aby = by - ay;
    const f32 apx = px - ax;
    const f32 apy = py - ay;
    const f32 ab2 = abx * abx + aby * aby;
    f32 t = ab2 > 1e-9f ? (apx * abx + apy * aby) / ab2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    const f32 cx = ax + abx * t;
    const f32 cy = ay + aby * t;
    return std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
}

namespace {

// direção do mundo de cada eixo
Vec3 axisDirOf(Axis a) {
    switch (a) {
        case Axis::X: return {1.0f, 0.0f, 0.0f};
        case Axis::Y: return {0.0f, 1.0f, 0.0f};
        case Axis::Z: return {0.0f, 0.0f, 1.0f};
        default:      return {0.0f, 0.0f, 0.0f};
    }
}

// normal do PLANO de drag (move): XY → Z; XZ → Y; YZ → X
Vec3 planeNormalOf(Axis a) {
    switch (a) {
        case Axis::XY: return {0.0f, 0.0f, 1.0f};
        case Axis::XZ: return {0.0f, 1.0f, 0.0f};
        case Axis::YZ: return {1.0f, 0.0f, 0.0f};
        default:       return {0.0f, 0.0f, 0.0f};
    }
}

// os dois eixos DE DENTRO do plano (p/ decompor o delta)
void planeAxesOf(Axis plane, Vec3& u, Vec3& v) {
    switch (plane) {
        case Axis::XY: u = {1.0f, 0.0f, 0.0f}; v = {0.0f, 1.0f, 0.0f}; break;
        case Axis::XZ: u = {1.0f, 0.0f, 0.0f}; v = {0.0f, 0.0f, 1.0f}; break;
        case Axis::YZ: u = {0.0f, 1.0f, 0.0f}; v = {0.0f, 0.0f, 1.0f}; break;
        default: break;
    }
}

f32 snapTo(f32 v, f32 step) {
    if (step <= 0.0f) {
        return v;
    }
    return std::round(v / step) * step;
}

} // namespace

// ---- hit-test ---------------------------------------------------------------------

Axis pickAxis(Mode mode, const Mat4& vp, const Vec3& origin, f32 len,
              f32 sw, f32 sh, f32 px, f32 py) {
    f32 ox = 0.0f, oy = 0.0f;
    if (!projectPoint(vp, origin, sw, sh, ox, oy)) {
        return Axis::None;   // gizmo atrás da câmara
    }

    Axis best = Axis::None;
    f32 bestDist = 1e9f;

    auto tryAxis = [&](Axis a, const Vec3& dir) {
        f32 ax = 0.0f, ay = 0.0f;
        if (!projectPoint(vp, origin + dir * len, sw, sh, ax, ay)) {
            return;
        }
        const f32 d = distToSegmentPx(px, py, ox, oy, ax, ay);
        if (d < kHitAxisPx && d < bestDist) {
            bestDist = d;
            best = a;
        }
    };

    if (mode == Mode::Move) {
        // 3 setas (segmentos eixo) — desempate pela menor distância
        tryAxis(Axis::X, axisDirOf(Axis::X));
        tryAxis(Axis::Y, axisDirOf(Axis::Y));
        tryAxis(Axis::Z, axisDirOf(Axis::Z));
        // 3 quads de plano: hit pelo CENTRO do quad (a 60% do len nos 2 eixos)
        auto tryPlane = [&](Axis plane, const Vec3& u, const Vec3& v) {
            const Vec3 c = origin + u * (len * 0.62f) + v * (len * 0.62f);
            f32 cx = 0.0f, cy = 0.0f;
            if (!projectPoint(vp, c, sw, sh, cx, cy)) {
                return;
            }
            const f32 d = std::sqrt((px - cx) * (px - cx) + (py - cy) * (py - cy));
            if (d < kHitPlanePx && d < bestDist) {
                bestDist = d;
                best = plane;
            }
        };
        tryPlane(Axis::XY, Vec3{1, 0, 0}, Vec3{0, 1, 0});
        tryPlane(Axis::XZ, Vec3{1, 0, 0}, Vec3{0, 0, 1});
        tryPlane(Axis::YZ, Vec3{0, 1, 0}, Vec3{0, 0, 1});
        return best;
    }

    if (mode == Mode::Rotate) {
        // 3 anéis: distância ao POLILINHA projetado (kRingSegs segmentos)
        auto tryRing = [&](Axis axis, const Vec3& u, const Vec3& v) {
            f32 pxPrev = 0.0f, pyPrev = 0.0f;
            bool okPrev = false;
            for (i32 i = 0; i <= static_cast<i32>(kRingSegs); ++i) {
                const f32 ang = (2.0f * 3.14159265f * i) / kRingSegs;
                const Vec3 p = origin +
                    (u * std::cos(ang) + v * std::sin(ang)) * len;
                f32 sx = 0.0f, sy = 0.0f;
                if (projectPoint(vp, p, sw, sh, sx, sy)) {
                    if (okPrev) {
                        const f32 d = distToSegmentPx(px, py, pxPrev, pyPrev,
                                                     sx, sy);
                        if (d < kHitAxisPx && d < bestDist) {
                            bestDist = d;
                            best = axis;
                        }
                    }
                    pxPrev = sx;
                    pyPrev = sy;
                    okPrev = true;
                } else {
                    okPrev = false;
                }
            }
        };
        tryRing(Axis::X, Vec3{0, 1, 0}, Vec3{0, 0, 1});  // anel ⟂ X
        tryRing(Axis::Y, Vec3{1, 0, 0}, Vec3{0, 0, 1});  // anel ⟂ Y
        tryRing(Axis::Z, Vec3{1, 0, 0}, Vec3{0, 1, 0});  // anel ⟂ Z
        return best;
    }

    // Scale: handles no fim dos eixos + handle central (prioridade ao centro)
    {
        const f32 dc = std::sqrt((px - ox) * (px - ox) + (py - oy) * (py - oy));
        if (dc < kHitCenterPx) {
            return Axis::Center;
        }
    }
    tryAxis(Axis::X, axisDirOf(Axis::X));
    tryAxis(Axis::Y, axisDirOf(Axis::Y));
    tryAxis(Axis::Z, axisDirOf(Axis::Z));
    return best;
}

// ---- drag -------------------------------------------------------------------------

Vec3 dragMoveAxis(const Vec3& anchorPos, const Vec3& axisDir,
                  const Vec3& hit0, const Vec3& hit1, bool snap) {
    // delta do ARRANQUE até AGORA, projetado no eixo (relativo às âncoras —
    // o jitter do dedo nunca se acumula)
    const f32 delta = dot(hit1 - hit0, axisDir);
    const f32 d = snap ? snapTo(delta, kSnapMove) : delta;
    return anchorPos + axisDir * d;
}

Vec3 dragMovePlane(const Vec3& anchorPos, const Vec3& planeNormal,
                   const Vec3& hit0, const Vec3& hit1, bool snap) {
    // eixos DENTRO do plano, derivados da normal (n aponta o eixo EXCLUÍDO):
    //   n=(0,0,1)→XY (u=X,v=Y) · n=(0,1,0)→XZ (u=X,v=Z) · n=(1,0,0)→YZ (u=Y,v=Z)
    Vec3 u{0.0f, 0.0f, 0.0f}, v{0.0f, 0.0f, 0.0f};
    if (planeNormal.x > 0.5f) {            // YZ
        u = {0.0f, 1.0f, 0.0f};
        v = {0.0f, 0.0f, 1.0f};
    } else if (planeNormal.y > 0.5f) {     // XZ
        u = {1.0f, 0.0f, 0.0f};
        v = {0.0f, 0.0f, 1.0f};
    } else {                               // XY (e fallback)
        u = {1.0f, 0.0f, 0.0f};
        v = {0.0f, 1.0f, 0.0f};
    }
    const Vec3 d = hit1 - hit0;
    f32 du = dot(d, u);
    f32 dv = dot(d, v);
    if (snap) {
        du = snapTo(du, kSnapMove);
        dv = snapTo(dv, kSnapMove);
    }
    return anchorPos + u * du + v * dv;
}

Quat dragRotate(const Quat& anchorRot, const Vec3& axisDir, const Vec3& fwd,
                f32 angle0, f32 angle1, bool snap) {
    f32 delta = angle1 - angle0;
    // o anel vê-se de "frente" quando o eixo aponta PARA a câmara; o drag em
    // ecrã (y para baixo) roda ao contrário — corrige o sinal pela direção
    // do eixo face à câmara
    const f32 facing = dot(axisDir, fwd);
    if (facing < 0.0f) {
        delta = -delta;
    }
    if (snap) {
        // arredonda o ÂNGULO TOTAL (âncora + delta) ao passo — o snap nunca
        // "foge" enquanto o dedo se move dentro do mesmo degrau
        const f32 a0 = 0.0f;   // âncora relativa: o primeiro drag parte de 0
        delta = snapTo(a0 + delta, kSnapRot) - a0;
    }
    if (std::fabs(delta) < 1e-6f) {
        return anchorRot;
    }
    // rotação GLOBAL (pós-multiplicação aplicaria no espaço local):
    // q = axisAngle(axis, delta) * anchor
    return Quat::axisAngle(axisDir, delta) * anchorRot;
}

namespace {
// referência para converter o arrasto no eixo num fator de escala (~1 u de
// arrasto no mundo = ×2)
constexpr f32 kScaleRef = 1.0f;
} // namespace

Vec3 dragScaleAxis(const Vec3& anchorScale, Axis axis, const Vec3& axisDir,
                   const Vec3& hit0, const Vec3& hit1, bool snap) {
    const f32 delta = dot(hit1 - hit0, axisDir);
    f32 f = 1.0f + delta / kScaleRef;
    if (snap) {
        f = snapTo(f, kSnapScale);
    }
    if (f < 0.05f) {
        f = 0.05f;   // nunca zero/negativo (escala degenerada)
    }
    Vec3 s = anchorScale;
    switch (axis) {
        case Axis::X: s.x *= f; break;
        case Axis::Y: s.y *= f; break;
        case Axis::Z: s.z *= f; break;
        default: break;
    }
    return s;
}

Vec3 dragScaleUniform(const Vec3& anchorScale, f32 dist0, f32 dist1,
                      bool snap) {
    if (dist0 < 1.0f) {
        return anchorScale;   // âncora degenerada (dedo em cima do centro)
    }
    f32 f = dist1 / dist0;
    if (snap) {
        f = snapTo(f, kSnapScale);
    }
    if (f < 0.05f) {
        f = 0.05f;
    }
    return anchorScale * f;
}

// ---- desenho -----------------------------------------------------------------------

namespace {

void drawArrow(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
               const Vec3& origin, const Vec3& dir, f32 len, Axis axis,
               Axis hovered) {
    f32 ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
    if (!projectPoint(vp, origin, sw, sh, ax, ay)) {
        return;
    }
    const Vec3 tip = origin + dir * len;
    if (!projectPoint(vp, tip, sw, sh, bx, by)) {
        return;
    }
    const bool hot = (hovered == axis);
    const f32* col = axisColor(axis, hot);
    ui.drawLine(ax, ay, bx, by, hot ? kLineWHov : kLineW, col);
    // ponta da seta: 2 traços do tip para trás (no ecrã, ao longo da
    // direção projetada, ortogonais ±)
    const f32 dx = bx - ax;
    const f32 dy = by - ay;
    const f32 n = std::sqrt(dx * dx + dy * dy);
    if (n > 1e-3f) {
        const f32 ux = dx / n, uy = dy / n;         // unitária do eixo (ecrã)
        const f32 vx = -uy, vy = ux;                 // ortogonal
        const f32 head = 14.0f;
        const f32 cx = bx - ux * head, cy = by - uy * head;
        const f32 wx = bx - ux * head * 1.9f, wy = by - uy * head * 1.9f;
        ui.drawLine(bx, by, wx + vx * head * 0.6f, wy + vy * head * 0.6f,
                     hot ? kLineWHov : kLineW, col);
        ui.drawLine(bx, by, wx - vx * head * 0.6f, wy - vy * head * 0.6f,
                     hot ? kLineWHov : kLineW, col);
        (void)cx; (void)cy;
    }
}

void drawPlaneHandle(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
                     const Vec3& origin, f32 len, Axis plane,
                     Axis hovered) {
    // quad de plano: contorno (4 segmentos) nos 2 eixos do plano, a 45%..75%
    // do len — pequeno e afastado do centro (não colide com as setas)
    Vec3 u, v;
    planeAxesOf(plane, u, v);
    const f32 a = len * 0.45f;
    const f32 b = len * 0.78f;
    const Vec3 c0 = origin + u * a + v * a;
    const Vec3 c1 = origin + u * b + v * a;
    const Vec3 c2 = origin + u * b + v * b;
    const Vec3 c3 = origin + u * a + v * b;
    f32 p0x, p0y, p1x, p1y, p2x, p2y, p3x, p3y;
    if (!projectPoint(vp, c0, sw, sh, p0x, p0y) ||
        !projectPoint(vp, c1, sw, sh, p1x, p1y) ||
        !projectPoint(vp, c2, sw, sh, p2x, p2y) ||
        !projectPoint(vp, c3, sw, sh, p3x, p3y)) {
        return;
    }
    const bool hot = (hovered == plane);
    const f32* col = axisColor(plane, hot);
    const f32 w = hot ? kLineWHov : 3.0f;
    ui.drawLine(p0x, p0y, p1x, p1y, w, col);
    ui.drawLine(p1x, p1y, p2x, p2y, w, col);
    ui.drawLine(p2x, p2y, p3x, p3y, w, col);
    ui.drawLine(p3x, p3y, p0x, p0y, w, col);
}

void drawRing(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
              const Vec3& origin, f32 len, Axis axis, const Vec3& u,
              const Vec3& v, Axis hovered) {
    const bool hot = (hovered == axis);
    const f32* col = axisColor(axis, hot);
    const f32 w = hot ? kLineWHov : 4.0f;
    f32 pxPrev = 0.0f, pyPrev = 0.0f;
    bool okPrev = false;
    for (i32 i = 0; i <= static_cast<i32>(kRingSegs); ++i) {
        const f32 ang = (2.0f * 3.14159265f * i) / kRingSegs;
        const Vec3 p = origin + (u * std::cos(ang) + v * std::sin(ang)) * len;
        f32 sx = 0.0f, sy = 0.0f;
        if (projectPoint(vp, p, sw, sh, sx, sy)) {
            if (okPrev) {
                ui.drawLine(pxPrev, pyPrev, sx, sy, w, col);
            }
            pxPrev = sx;
            pyPrev = sy;
            okPrev = true;
        } else {
            okPrev = false;
        }
    }
}

void drawScaleHandle(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
                     const Vec3& origin, const Vec3& dir, f32 len, Axis axis,
                     Axis hovered) {
    f32 ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
    if (!projectPoint(vp, origin, sw, sh, ax, ay)) {
        return;
    }
    const Vec3 tip = origin + dir * len;
    if (!projectPoint(vp, tip, sw, sh, bx, by)) {
        return;
    }
    const bool hot = (hovered == axis);
    const f32* col = axisColor(axis, hot);
    ui.drawLine(ax, ay, bx, by, hot ? kLineWHov : kLineW, col);
    // handle no fim: quad preenchido centrado no tip
    const f32 s = hot ? kHandlePx + 6.0f : kHandlePx;
    ui.panel(bx - s * 0.5f, by - s * 0.5f, s, s, col);
}

} // namespace

void drawGizmo(UiContext& ui, const Mat4& vp, const Vec3& origin, f32 len,
               Mode mode, Axis hovered) {
    const f32 sw = ui.screenWidth();
    const f32 sh = ui.screenHeight();
    if (sw <= 1.0f || sh <= 1.0f || len <= 0.0f) {
        return;
    }

    switch (mode) {
        case Mode::Move:
            drawArrow(ui, vp, sw, sh, origin, Vec3{1, 0, 0}, len, Axis::X, hovered);
            drawArrow(ui, vp, sw, sh, origin, Vec3{0, 1, 0}, len, Axis::Y, hovered);
            drawArrow(ui, vp, sw, sh, origin, Vec3{0, 0, 1}, len, Axis::Z, hovered);
            drawPlaneHandle(ui, vp, sw, sh, origin, len, Axis::XY, hovered);
            drawPlaneHandle(ui, vp, sw, sh, origin, len, Axis::XZ, hovered);
            drawPlaneHandle(ui, vp, sw, sh, origin, len, Axis::YZ, hovered);
            break;
        case Mode::Rotate:
            drawRing(ui, vp, sw, sh, origin, len, Axis::X,
                     Vec3{0, 1, 0}, Vec3{0, 0, 1}, hovered);
            drawRing(ui, vp, sw, sh, origin, len, Axis::Y,
                     Vec3{1, 0, 0}, Vec3{0, 0, 1}, hovered);
            drawRing(ui, vp, sw, sh, origin, len, Axis::Z,
                     Vec3{1, 0, 0}, Vec3{0, 1, 0}, hovered);
            break;
        case Mode::Scale: {
            drawScaleHandle(ui, vp, sw, sh, origin, Vec3{1, 0, 0}, len, Axis::X, hovered);
            drawScaleHandle(ui, vp, sw, sh, origin, Vec3{0, 1, 0}, len, Axis::Y, hovered);
            drawScaleHandle(ui, vp, sw, sh, origin, Vec3{0, 0, 1}, len, Axis::Z, hovered);
            // handle central (uniforme): quad no centro projetado
            f32 cx = 0.0f, cy = 0.0f;
            if (projectPoint(vp, origin, sw, sh, cx, cy)) {
                const bool hot = (hovered == Axis::Center);
                const f32* col = axisColor(Axis::Center, hot);
                const f32 s = hot ? kHandlePx + 8.0f : kHandlePx;
                ui.panel(cx - s * 0.5f, cy - s * 0.5f, s, s, col);
            }
            break;
        }
    }
}

} // namespace gizmo
} // namespace vv
