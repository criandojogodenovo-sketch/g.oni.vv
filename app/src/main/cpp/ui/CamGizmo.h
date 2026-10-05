#pragma once
// ui/CamGizmo.h — GIZMO DA CÂMARA DE CENA (0.7.7): frustum wireframe visível
// SÓ NO EDITOR, seleção por toque, handles do plano far e a câmara de jogo.
//
// 0.7.10 — FRUSTUM DOMADO (C33): o cone desenha-se com comprimento VISUAL
// CLAMPADO (kVisualFarCap) — o tamanho no ecrã fica confortável e INDE-
// PENDENTE do far real (que continua a valer para o render no Play e vive
// no Inspector). O hit-test de seleção é RESTRITO ao CORPO+LENTE (tocar no
// cone vazio não seleciona a câmara nem bloqueia o orbit); os toques em
// meshes/TICs selecionáveis têm PRIORIDADE (pickSceneTic). O Inspector ganha
// o toggle "frustum" (esconder quando polui).
//
// O VISUAL (imagem de referência do dono): corpo wireframe (caixa + lente),
// cone de 4 arestas até ao retângulo do plano far (AO CAP VISUAL), retângulo
// do far, linha de visão central e handles nos 4 cantos + centro do far.
// Cor de gizmo/marca (#8AB4F8 — theme::kTheme.accent, a exceção documentada).
// Como os gizmos de transformação: NUNCA em Play (camgizmo::visible).
//
// GEOMETRIA PURA: o frustum deriva de Transform3D (pose) + CameraComp
// (fov/near/far/ortho) + aspeto — funções GL-free aferidas no CI (o retângulo
// far sai de tan(fov/2)·far; no orto, orthoSize em ambos os planos).
//
// INTERAÇÃO:
//   • tocar no CORPO/LENTE de uma câmara seleciona o TIC dela (hit-test
//     3D por projeção — a mesma técnica dos gizmos; 0.7.10: SÓ o corpo+
//     lente — o cone/far/linha de visão NÃO hit-testam);
//   • toques que acertam meshes/TICs selecionáveis têm PRIORIDADE
//     (pickSceneTic: a câmara só é apanhada se nada mais for acertado);
//   • com a câmara selecionada, os HANDLES do far arrastam: o CENTRO muda
//     `far`, um CANTO muda `fovY`. O hit-test do handle tem PRIORIDADE
//     sobre o eixo do gizmo de transformação (sem conflitos de drag) e SÓ
//     existe com a câmara já selecionada; os handles sentam-se no
//     retângulo do far AO CAP VISUAL (partilham a geometria do desenho);
//   • o gizmo ESCALAR sobre uma câmara ajusta fovY/orthoSize (o frustum
//     escala) — nunca a escala do transform (sem significado numa câmara);
//   • "Alinhar à vista" (menu contextual) copia a pose da orbit de edição.
//
// CÂMARA DE JOGO: gameView/gameProj derivam view/proj da pose+parâmetros;
// em Play o main renderiza pela câmara ATIVA (core/CameraUtil) com fallback
// à orbit de edição.
#include "components/Transform3D.h"   // pose (inline gameForward usa rot)
#include "core/Handle.h"    // Handle (seleção por toque devolve o TIC)
#include "core/Types.h"
#include "math/Math.h"

namespace vv {

class CameraComp;
class Transform3D;
class Camera;         // a orbit (render/Camera.h)
class Scene;
class UiContext;
struct Tic;

namespace camgizmo {

// o frustum desenha/aceita input? (EDITOR 3D — como os gizmos)
bool visible(bool playMode, bool uiMode);

// 0.7.10 — comprimento VISUAL máximo do cone (unidades de mundo): o
// retângulo do far desenha-se a min(far, kVisualFarCap) — confortável no
// ecrã e INDEPENDENTE do far real (o render do Play usa o far REAL via
// gameProj; o valor vive no Inspector, não no tamanho do cone)
inline constexpr f32 kVisualFarCap = 12.0f;

// alvo de toque dos TICs 3D no picker de prioridade (px — o MESMO
// generoso do grab-lock: alvo mínimo de toque do Android)
inline constexpr f32 kTicPickPx = 44.0f;

// ---- geometria (PURO — testada no CI) ---------------------------------------

// wireframe completo da câmara: caixa (corpo) + lente + near/far + eixo
struct Frustum {
    Vec3 pos{};                 // olho (mundo)
    Vec3 fwd{}, right{}, up{};  // base local (mundo, normalizada)
    Vec3 box[8]{};              // corpo: cantos da caixa (índices de quad:
                                // 0..3 frente, 4..7 trás)
    Vec3 lens[8]{};             // lente: caixa pequena à frente do corpo
    Vec3 nearC[4]{};            // cantos do retângulo do near
    Vec3 farC[4]{};             // cantos do retângulo do far (RT,LB.. ordem
                                // consistente: [+r+u, -r+u, -r-u, +r-u])
    Vec3 farCenter{};
    f32  drawFar = 0.0f;        // 0.7.10: o far EFETIVAMENTE desenhado
                                // (min(farZ, kVisualFarCap) — o real vive
                                // no CameraComp/gameProj)
};

// meia-altura/largura do retângulo a `dist` (persp: tan(fov/2)·dist;
// orto: orthoSize fixo) — a matemática aferida pelo teste da spec
void planeHalfExtents(const CameraComp& cam, f32 dist, f32 aspect,
                      f32& halfW, f32& halfH);

// o wireframe completo (aspect = w/h do render do jogo). 0.7.10:
// 0.9.6.1 (G1-4): o cap VISUAL por TAMANHO NO ECRÃ — a pirâmide da câmara
// deixava de dominar a viewport: o comprimento do cone é o que projetar o
// far a ~80dp de altura, a partir de px-por-unidade MEDIDO no olho pelo
// próprio vp (a distância editor↔câmara deixa de importar). Devolve SEMPRE
// um cap saneado (piso 1.5, teto kVisualFarCap).
f32 visualCapForScreen(const Mat4& vp, f32 sw, f32 sh, const Vec3& eye,
                       f32 fovYDeg);
// `visualFarCap` clampa o COMPRIMENTO VISUAL (far desenhado =
// min(farZ, visualFarCap); default kVisualFarCap — passar um valor
// maior devolve a geometria real).
Frustum computeFrustum(const Transform3D& tr, const CameraComp& cam,
                       f32 aspect, f32 visualFarCap = kVisualFarCap);

// ---- desenho (emite no UiContext — line batch dos gizmos) --------------------

// desenha o frustum UMA câmara (cor de marca; `selected` acrescenta os
// HANDLES do far — 4 cantos + centro). Editor-only (o chamador faz o gate).
void drawFrustum(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
                 const Frustum& f, bool selected);

// desenha TODAS as câmaras visíveis da cena (o loop do main; cada frustum
// com o aspeto do ecrã; a selecionada ganha os handles)
void drawAll(UiContext& ui, Scene& scene, const Mat4& vp, f32 sw, f32 sh,
             Handle selected);

// ---- hit-test ------------------------------------------------------------------

// o handle sob o toque (câmara selecionada): 0 = nada, 1..4 = canto i-1,
// 5 = centro do far. Distâncias em px de ecrã (kHandleHitPx).
int pickHandle(const Mat4& vp, f32 sw, f32 sh, const Frustum& f,
               f32 px, f32 py);

// o TIC da câmara cujo CORPO/LENTE está sob o toque (0.7.10: hit-test
// RESTRITO — o cone/far/linha de visão NÃO selecionam; segmentos
// projetados a kHitPx; o MAIS PRÓXIMO ganha). Handle::invalid se nenhum.
Handle pickCameraTic(Scene& scene, const Mat4& vp, f32 sw, f32 sh,
                     f32 px, f32 py);

// 0.7.10 — PICKER DO VIEWPORT COM PRIORIDADE DE OBJETOS: primeiro os TICs
// SELECIONÁVEIS (meshes — projeção do CENTRO a kTicPickPx, o mais próximo
// do toque), SÓ DEPOIS a câmara (via corpo/lente). A câmara nunca rouba o
// toque de um objeto (o C33: tocar num objeto DENTRO do cone selecionava
// a câmara). Handle::invalid se nada.
Handle pickSceneTic(Scene& scene, const Mat4& vp, f32 sw, f32 sh,
                    f32 px, f32 py);

// ---- drag dos handles (PURO — âncoras, nunca acumulado) ------------------------

// handle do CENTRO do far: delta do arrasto projetado no eixo de visão
// (hits = interseções raio×plano [normal = fwd da câMARA, por pos]).
f32 dragFar(f32 anchorFar, const Vec3& hit0, const Vec3& hit1,
            const Vec3& camFwd, bool snap);

// handle de CANTO: fator radial do dedo em torno do CENTRO PROJETADO da
// câmara (px) — fov = âncora × d1/d0 (snap: arredonda a 5°)
f32 dragFov(f32 anchorFovDeg, f32 d0, f32 d1, bool snap);

// gizmo ESCALAR sobre uma câmara: o fator do drag escala fov/orthoSize
// (nunca o transform). ortho = projeção ortográfica.
f32 dragScaleToFov(f32 anchorFovDeg, f32 anchorValue, bool ortho,
                   f32 d0, f32 d1, bool snap);

// ---- câmara de jogo ------------------------------------------------------------

// view da pose do TIC (olho = pos; −Z local = visão; +Y local = up)
Mat4 gameView(const Transform3D& tr);

// proj dos parâmetros (persp: fovY° → rad; orto: ±orthoSize·aspect)
Mat4 gameProj(const CameraComp& cam, f32 aspect);

// direção de visão da pose (−Z local no mundo)
inline Vec3 gameForward(const Transform3D& tr) {
    return tr.rot.rotate(Vec3{0.0f, 0.0f, -1.0f});
}

// ---- alinhar à vista ------------------------------------------------------------

// copia a pose da ORBIT de edição para o transform da câmara: pos = eye,
// orientação = olhar de eye para target. (fromEuler(−pitch, yaw, 0) mapeia
// o −Z local exatamente na direção eye→target — derivado da convenção da
// orbit: dir(target→eye) = (cp·sy, sp, cp·cy).)
void alignToView(Transform3D& tr, const Camera& orbit);

} // namespace camgizmo
} // namespace vv
