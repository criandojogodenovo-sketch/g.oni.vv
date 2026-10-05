// ui/CamGizmo.cpp — implementação do gizmo da câmara (0.7.7).
//
// Tudo projetado para px de ecrã (a técnica dos gizmos: gizmo::projectPoint
// + distToSegmentPx) — o hit-test e o desenho partilham a MESMA geometria.
// O drag usa âNCORAS (pose final = âncora + delta; nunca acumula).
#include "ui/CamGizmo.h"

#include <cmath>

#include "components/CameraComp.h"
#include "components/MeshRenderer.h"   // 0.7.10: pickSceneTic (prioridade)
#include "render/Mesh.h"   // FASE 9 (G1-6): bounds do mesh no pick por corpo
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

// px de ecrã — 0.9.6.1 (PASSO 0): em dp REAL (R-018; no device a 2× os
// traços/handles eram metade do tamanho visual)
f32 kLineW()       { return theme::dp(3.0f); }    // traço do wireframe
f32 kLineWSel()    { return theme::dp(4.0f); }    // câmara selecionada
f32 kHandlePx()    { return theme::dp(26.0f); }   // quadrado do handle (lado)
f32 kHandleHitPx() { return theme::dp(30.0f); }   // hit de handles (prioritário)

f32 deg2rad(f32 d) { return d * 0.01745329252f; }
constexpr f32 kHitPx = 22.0f;   // hit-test de segmentos (em dp no uso)
f32 rad2deg(f32 r) { return r * 57.29577951f; }

f32 snapStep(f32 v, f32 step) {
    return step > 0.0f ? std::round(v / step) * step : v;
}

// emite uma aresta do mundo (projeta; atrás da câmara → não desenha)
// GRUPO D: (mw,mh,ox,oy) = o mapeamento da viewport (NDC→(mw×mh)+origem)
void edge(UiContext& ui, const Mat4& vp, f32 mw, f32 mh, f32 ox, f32 oy,
          const Vec3& a, const Vec3& b, f32 w, const f32 col[4]) {
    f32 ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
    if (!gizmo::projectPoint(vp, a, mw, mh, ax, ay, ox, oy)) {
        return;
    }
    if (!gizmo::projectPoint(vp, b, mw, mh, bx, by, ox, oy)) {
        return;
    }
    ui.drawLine(ax, ay, bx, by, w, col);
}

// caixa (8 cantos: 0..3 frente [+z local], 4..7 trás) → 12 arestas
void drawBox(UiContext& ui, const Mat4& vp, f32 mw, f32 mh, f32 ox, f32 oy,
             const Vec3 c[8], f32 w, const f32 col[4]) {
    for (int i = 0; i < 4; ++i) {
        edge(ui, vp, mw, mh, ox, oy, c[i], c[(i + 1) % 4], w, col);          // frente
        edge(ui, vp, mw, mh, ox, oy, c[4 + i], c[4 + (i + 1) % 4], w, col);  // trás
        edge(ui, vp, mw, mh, ox, oy, c[i], c[4 + i], w, col);                // ligação
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

// 0.9.6.1 (G1-4): o cap que dá ~80dp de ALTURA PROJETADA ao far — mede os
// px-por-unidade-de-mundo no OLHO (projeta eye e eye+X pelo MESMO vp) e
// resolve a distância: alturaFar_px ≈ 2·tan(fov/2)·dist·ppu
f32 visualCapForScreen(const Mat4& vp, f32 sw, f32 sh, const Vec3& eye,
                       f32 fovYDeg) {
    f32 x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    if (!gizmo::projectPoint(vp, eye, sw, sh, x0, y0) ||
        !gizmo::projectPoint(vp, eye + Vec3{1.0f, 0.0f, 0.0f}, sw, sh, x1,
                             y1)) {
        return kVisualFarCap;
    }
    const f32 ppu =
        std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    if (ppu < 1e-4f) {
        return kVisualFarCap;   // projeção degenerada — o cap antigo
    }
    const f32 targetPx = theme::dp(80.0f);
    const f32 tanF = std::tan(deg2rad(fovYDeg) * 0.5f);
    if (tanF <= 1e-5f) {
        return kVisualFarCap;
    }
    const f32 dist = targetPx / (2.0f * tanF * ppu);
    return dist < 1.5f ? 1.5f : (dist < kVisualFarCap ? dist : kVisualFarCap);
}

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
                       f32 aspect, f32 visualFarCap) {
    Frustum f;
    f.pos   = tr.pos;
    f.fwd   = tr.rot.rotate(Vec3{0.0f, 0.0f, -1.0f});
    f.right = tr.rot.rotate(Vec3{1.0f, 0.0f, 0.0f});
    f.up    = tr.rot.rotate(Vec3{0.0f, 1.0f, 0.0f});

    // 0.7.10 — FRUSTUM DOMADO: o comprimento VISUAL é CLAMPADO. O far REAL
    // (cam.farZ) continua a valer para o RENDER do Play (gameProj lê o
    // CameraComp — nunca esta estrutura) e vive no Inspector; o cone fica
    // confortável no ecrã mesmo com far 2000 (o C33 via um frustum do
    // tamanho do viewport). far curto fica REAL (informativo).
    f.drawFar = cam.farZ < visualFarCap ? cam.farZ : visualFarCap;

    // corpo: caixa atrás do olho (o olho fica NO PLANO frontal da caixa —
    // como uma máquina fotográfica: o corpo atrás, a lente à frente)
    boxFromBasis(f, f.pos - f.fwd * (kBodyBack - kBodyHalfD), kBodyHalfW,
                 kBodyHalfH, kBodyHalfD, f.box);
    // lente: caixa pequena à frente (o "olho" da câmara)
    boxFromBasis(f, f.pos + f.fwd * kLensDist, kLensHalfW, kLensHalfH,
                 kLensHalfD, f.lens);

    // retângulos near/far (a MESMA matemática da projeção — aferida;
    // 0.7.10: o far desenha-se ao CAP VISUAL, o near fica real)
    f32 nw = 0.0f, nh = 0.0f, fw2 = 0.0f, fh2 = 0.0f;
    planeHalfExtents(cam, cam.nearZ, aspect, nw, nh);
    planeHalfExtents(cam, f.drawFar, aspect, fw2, fh2);
    const Vec3 nc = f.pos + f.fwd * cam.nearZ;
    const Vec3 fc = f.pos + f.fwd * f.drawFar;
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
                 const Frustum& f, bool selected, f32 vw, f32 vh, f32 ox,
                 f32 oy) {
    const f32* col = theme::kTheme.accent;   // cor de gizmo/marca
    const f32 w = selected ? kLineWSel() : kLineW();
    // GRUPO D: o mapeamento da viewport 3D (0,0 = o ecrã todo — o de sempre)
    const f32 mw = (vw > 1.0f && vh > 1.0f) ? vw : sw;
    const f32 mh = (vw > 1.0f && vh > 1.0f) ? vh : sh;

    drawBox(ui, vp, mw, mh, ox, oy, f.box, w, col);    // corpo
    drawBox(ui, vp, mw, mh, ox, oy, f.lens, w, col);   // lente
    for (int i = 0; i < 4; ++i) {
        // near + cone near→far + far
        edge(ui, vp, mw, mh, ox, oy, f.nearC[i], f.nearC[(i + 1) % 4], w, col);
        edge(ui, vp, mw, mh, ox, oy, f.farC[i], f.farC[(i + 1) % 4], w, col);
        edge(ui, vp, mw, mh, ox, oy, f.nearC[i], f.farC[i], w, col);
    }
    // linha de visão central (do corpo ao centro do far)
    edge(ui, vp, mw, mh, ox, oy, f.pos + f.fwd * (kLensDist + kLensHalfD),
         f.farCenter, w, col);

    // handles: SÓ na câmara selecionada (4 cantos + centro do far)
    if (!selected) {
        return;
    }
    const f32 ink[4] = {theme::kTheme.accentInk[0], theme::kTheme.accentInk[1],
                        theme::kTheme.accentInk[2], theme::kTheme.accentInk[3]};
    const f32 hp = kHandlePx();
    for (int i = 0; i < 4; ++i) {
        f32 hx = 0.0f, hy = 0.0f;
        if (gizmo::projectPoint(vp, f.farC[i], mw, mh, hx, hy, ox, oy)) {
            ui.panel(hx - hp * 0.5f, hy - hp * 0.5f, hp, hp, col);
            ui.frame(hx - hp * 0.5f, hy - hp * 0.5f, hp, hp, 2.0f, ink);
        }
    }
    f32 cx = 0.0f, cy = 0.0f;
    if (gizmo::projectPoint(vp, f.farCenter, mw, mh, cx, cy, ox, oy)) {
        ui.panel(cx - hp * 0.5f, cy - hp * 0.5f, hp, hp, ink);
        ui.frame(cx - hp * 0.5f, cy - hp * 0.5f, hp, hp, 2.0f, col);
    }
}

void drawAll(UiContext& ui, Scene& scene, const Mat4& vp, f32 sw, f32 sh,
             Handle selected, f32 vw, f32 vh, f32 ox, f32 oy) {
    if (sw <= 1.0f || sh <= 1.0f) {
        return;
    }
    // GRUPO D: o ASPECTO do frustum continua o do JOGO (sw/sh da SUPERFÍCIE
    // — em Play a câmara renderiza o ecrã todo); o MAPEAMENTO do desenho é
    // o rect da viewport (vw,vh,ox,oy; 0,0,0,0 = o ecrã todo — o de sempre)
    const f32 aspect = sw / sh;
    const f32 mw = (vw > 1.0f && vh > 1.0f) ? vw : sw;
    const f32 mh = (vw > 1.0f && vh > 1.0f) ? vh : sh;
    scene.forEachActive([&](Tic& t) {
        if (!t.visible) {
            return;
        }
        const Transform3D* tr = t.getComponent<Transform3D>();
        const CameraComp* cam = t.getComponent<CameraComp>();
        if (!tr || !cam) {
            return;
        }
        // 0.7.10 — toggle do Inspector: esconder o frustum quando polui
        // (a câmara continua a valer para o render; só o GIZMO desaparece)
        if (!cam->showFrustum) {
            return;
        }
        // 0.9.6.1 (G1-4): o cap dá ~80dp no ecrã (antes: 12 unidades FIXAS —
        // a pirâmide dominava a viewport quando a câmara estava perto).
        // GRUPO D: o ppu mede-se pelo MAPEAMENTO do rect (consistente com
        // o draw que o consume)
        const Frustum f = computeFrustum(
            *tr, *cam, aspect,
            visualCapForScreen(vp, mw, mh, tr->pos, cam->fovY));
        drawFrustum(ui, vp, sw, sh, f, t.handle == selected, mw, mh, ox, oy);
    });
}

// ---- hit-test ----------------------------------------------------------------------

int pickHandle(const Mat4& vp, f32 sw, f32 sh, const Frustum& f, f32 px,
               f32 py) {
    int best = 0;
    f32 bestD = kHandleHitPx();
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
    f32 bestD = theme::dp(kHitPx);   // hit em dp real (o desenho idem)
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
        // 0.7.10 — HIT-TEST RESTRITO: SÓ o CORPO + LENTE (a caixa pequena).
        // O cone/far/linha de visão NÃO selecionam — o C33 tinha o cone a
        // roubar toques (selecionava a câmara em vez do objeto/orbit). Os
        // HANDLES continuam a ser apanhados pelo pickHandle (só com a
        // câmara JÁ selecionada — regra do feedGizmo).
        f32 d = distToBox(vp, sw, sh, f.box, px, py);
        d = (std::min)(d, distToBox(vp, sw, sh, f.lens, px, py));
        if (d < bestD) {
            bestD = d;
            best = t.handle;
        }
    });
    return best;
}

Handle pickSceneTic(Scene& scene, const Mat4& vp, f32 sw, f32 sh, f32 px,
                    f32 py) {
    if (sw <= 1.0f || sh <= 1.0f) {
        return Handle::invalid();
    }
    // FASE 9 (G1-6 — "tocar num cubo na viewport seleciona-o"): o hit-test
    // antigo media a distância ao CENTRO projetado com um teto de 44 px —
    // tocar o CORPO de um objeto grande NÃO selecionava (e o deselect da
    // 0.7.0 LIMPAVA a seleção nesse mesmo tap). AGORA: o AABB do mesh é
    // projetado (8 cantos locais pela matriz world) e o toque dentro do
    // rect de ecrã resultante SELECIONA — o 44 px do centro fica como
    // piso para objetos pequenos/longe. Prioridade: o mais PRÓXIMO DA
    // CÂMERA entre os acertados (antes era o mais próximo do toque).
    Handle best = Handle::invalid();
    f32 bestDepth = 1e30f;
    scene.forEachActive([&](Tic& t) {
        if (!t.visible) {
            return;
        }
        const MeshRenderer* mr = t.getComponent<MeshRenderer>();
        if (!mr) {
            return;   // sem mesh não há "objeto" visual a selecionar
        }
        const Transform3D* tr = t.getComponent<Transform3D>();
        if (!tr) {
            return;
        }
        // o AABB local (bounds do mesh) → 8 cantos no MUNDO → ecrã. Sem
        // mesh carregado (componente apenas — o caso dos testes/presets
        // antes do bind) o hit é pelo CENTRO (a regra antiga, 44 px)
        f32 minX = 1e30f, minY = 1e30f, maxX = -1e30f, maxY = -1e30f;
        f32 depth = 0.0f;
        u32 projected = 0;
        if (mr->mesh) {
            const Vec3 mn = mr->mesh->boundsMin();
            const Vec3 mx = mr->mesh->boundsMax();
            for (int c = 0; c < 8; ++c) {
                const Vec3 local{(c & 1) ? mx.x : mn.x,
                                 (c & 2) ? mx.y : mn.y,
                                 (c & 4) ? mx.z : mn.z};
                // world = tr->world * local (column-major: m[12..14]+bases)
                const Vec3 world{
                    tr->world.m[0] * local.x + tr->world.m[4] * local.y +
                        tr->world.m[8] * local.z + tr->world.m[12],
                    tr->world.m[1] * local.x + tr->world.m[5] * local.y +
                        tr->world.m[9] * local.z + tr->world.m[13],
                    tr->world.m[2] * local.x + tr->world.m[6] * local.y +
                        tr->world.m[10] * local.z + tr->world.m[14]};
                // projeção COM profundidade (clip.w = distância ao plano
                // da câmara — a métrica de "mais perto ganha")
                f32 clip[4];
                Mat4::transformPoint4(vp, world, clip);
                if (clip[3] <= 1e-5f) {
                    continue;   // atrás da câmara — este canto não conta
                }
                const f32 invW = 1.0f / clip[3];
                const f32 ox = (clip[0] * invW * 0.5f + 0.5f) * sw;
                const f32 oy = (1.0f - (clip[1] * invW * 0.5f + 0.5f)) * sh;
                ++projected;
                if (ox < minX) minX = ox;
                if (ox > maxX) maxX = ox;
                if (oy < minY) minY = oy;
                if (oy > maxY) maxY = oy;
                if (projected == 1 || clip[3] < depth) {
                    depth = clip[3];   // o canto MAIS PRÓXIMO vence
                }
            }
        }
        if (projected == 0 && mr->mesh) {
            return;   // com mesh mas NADA projetou (atrás da câmara)
        }
        // centro projetado: piso de 44 px (objetos pequenos/longe) e a
        // profundidade para os TICs SEM mesh (a regra antiga)
        f32 ox = 0.0f, oy = 0.0f;
        bool nearCenter = false;
        f32 centerDepth = 1e30f;
        if (gizmo::projectPoint(vp, tr->pos, sw, sh, ox, oy)) {
            const f32 d = std::sqrt((px - ox) * (px - ox) +
                                    (py - oy) * (py - oy));
            nearCenter = d < kTicPickPx;
            f32 clip[4];
            Mat4::transformPoint4(vp, tr->pos, clip);
            if (clip[3] > 1e-5f) {
                centerDepth = clip[3];
            }
        }
        if (!mr->mesh) {
            // SEM mesh carregado: a regra de sempre — SÓ o centro (44 px)
            if (nearCenter && centerDepth < bestDepth) {
                bestDepth = centerDepth;
                best = t.handle;
            }
            return;
        }
        // dentro do rect projetado (com a margem de 8 px de dedo)?
        const bool insideRect = px >= minX - 8.0f && px <= maxX + 8.0f &&
                                py >= minY - 8.0f && py <= maxY + 8.0f;
        const f32 winDepth = depth < centerDepth ? depth : centerDepth;
        if ((insideRect || nearCenter) && winDepth < bestDepth) {
            bestDepth = winDepth;
            best = t.handle;
        }
    });
    if (best.valid()) {
        return best;
    }
    // 2) SÓ DEPOIS a câmara — e só via CORPO/LENTE (pickCameraTic restrito)
    return pickCameraTic(scene, vp, sw, sh, px, py);
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
    if (f < 0.05f) {
        f = 0.05f;
    }
    const f32 v = (ortho ? anchorValue : anchorFovDeg) * f;
    // 0.7.9 — snap no VALOR FINAL (fov em passos de 5° como o handle do
    // fov; orthoSize em passos de 0.25 como a escala dos gizmos), não no
    // fator cru: o valor que fica na câmara aterra em degraus absolutos
    f32 out = v;
    if (snap) {
        out = snapStep(v, ortho ? 0.25f : 5.0f);
    }
    if (ortho) {
        return out < 0.1f ? 0.1f : out;
    }
    return out < CameraComp::kMinFov
               ? CameraComp::kMinFov
               : (out > CameraComp::kMaxFov ? CameraComp::kMaxFov : out);
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
