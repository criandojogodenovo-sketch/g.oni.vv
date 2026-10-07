#pragma once
// ui/CamGizmo.h — GIZMO DA CÂMARA DE CENA (0.7.7) · D17 (0.9.6.19): A
// CÂMARA É UM OBJETO PEQUENO QUE VÊ, NÃO UM CONE GIGANTE.
//
// 0.9.6.19 — HOTFIX D17 (a spec do dono, que substitui o D13):
//   (a) GLIFO de câmara ~24dp CONSTANTE EM ECRÃ na posição da câmara (o
//       mesmo padrão de tamanho constante dos gizmos da casa) — o ícone
//       Camera do vocabulário da casa;
//   (b) FRUSTUM FINO (1-2px) que reflete a câmara real (fov/near/far/
//       orto; o far VISUAL continua ao cap de ecrã — o anti-gigante da
//       0.7.10, o far REAL vive no CameraComp/gameProj): sem seleção =
//       cinza mudo a ~35% alfa; com seleção = âmbar fino;
//   (c) HANDLES só com seleção, 12dp, SÓ nos 4 CANTOS do far (editam o
//       fov) — nunca quadrados filled gigantes (os 26dp morreram; o
//       handle do CENTRO/far morreu — o far edita-se no Inspector);
//   (d) o GIZMO DE MOVER ancora ao GLIFO (a pos do Transform3D = o olho,
//       onde o glifo desenha — nunca a um vértice do frustum);
//   (e) HIT-TEST: gizmo de mover > handles de canto > frustum (INTOCÁVEL)
//       — arrastar na cena move a câmara, não agarra o cone (o main
//       consulta o gizmo PRIMEIRO no press edge);
//   (f) as linhas do frustum NUNCA interceptam toque (a seleção da câmara
//       é pelo GLIFO — o corpo/lente wireframe já não existem).
//
// HISTÓRIA: 0.7.10 — FRUSTUM DOMADO (C33): o cone desenha-se com
// comprimento VISUAL CLAMPADO (kVisualFarCap) — o tamanho no ecrã fica
// confortável e INDEPENDENTE do far real (que continua a valer para o
// render no Play e vive no Inspector). O Inspector tem o toggle "frustum"
// (esconder quando polui).
//
// GEOMETRIA PURA: o frustum deriva de Transform3D (pose) + CameraComp
// (fov/near/far/ortho) + aspeto — funções GL-free aferidas no CI (o
// retângulo far sai de tan(fov/2)·far; no orto, orthoSize em ambos os
// planos).
//
// INTERAÇÃO (D17):
//   • tocar no GLIFO (o quadrado ~24dp no olho, raio kGlyphHitDp)
//     seleciona o TIC da câmara — as LINHAS do frustum não selecionam;
//   • toques que acertam meshes/TICs selecionáveis têm PRIORIDADE
//     (pickSceneTic: a câmara só é apanhada se nada mais for acertado);
//   • com a câmara selecionada, os HANDLES de CANTO arrastam o fovY (o
//     MESMO dragFov da 0.7.7); o hit-test do handle corre DEPOIS do gizmo
//     (a ordem (e) do dono) e SÓ existe com a câmara já selecionada;
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

// ---- 0.9.6.19 (D17) — AS MEDIDAS DO OBJETO PEQUENO (fontes únicas) ------
inline constexpr f32 kGlyphDp      = 24.0f;  // (a) o glifo ~24dp constante
inline constexpr f32 kGlyphHitDp   = 18.0f;  // o raio de toque do glifo
                                             // (12dp do glifo + dedo)
inline constexpr f32 kHandleDp     = 12.0f;  // (c) o handle de canto 12dp
inline constexpr f32 kHandleHitDp  = 16.0f;  // o hit do canto (12 + dedo)
inline constexpr f32 kMutedAlpha   = 0.35f;  // (b) o cinza mudo ~35% alfa
// o traço do frustum em PX DE ECRÃ (a spec do dono: «frustum fino (1-2px)»)
inline constexpr f32 kFrustumLinePx = 1.0f;   // sem seleção
inline constexpr f32 kFrustumLinePxSel = 2.0f; // com seleção

// alvo de toque dos TICs 3D no picker de prioridade (px — o MESMO
// generoso do grab-lock: alvo mínimo de toque do Android)
inline constexpr f32 kTicPickPx = 44.0f;

// ---- geometria (PURO — testada no CI) ---------------------------------------

// o frustum da câmara: near/far + base (o CORPO é o glifo — as caixas
// wireframe da 0.7.7 morreram no D17)
struct Frustum {
    Vec3 pos{};                 // olho (mundo) — onde o glifo ancora (d)
    Vec3 fwd{}, right{}, up{};  // base local (mundo, normalizada)
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

// o frustum (aspect = w/h do render do jogo). 0.9.6.1 (G1-4): o cap VISUAL
// por TAMANHO NO ECRÃ — a pirâmide da câmara deixa de dominar a viewport: o
// comprimento do cone é o que projetar o far a ~80dp de altura, a partir de
// px-por-unidade MEDIDO no olho pelo próprio vp. Devolve SEMPRE um cap
// saneado (piso 1.5, teto kVisualFarCap).
f32 visualCapForScreen(const Mat4& vp, f32 sw, f32 sh, const Vec3& eye,
                       f32 fovYDeg);
// `visualFarCap` clampa o COMPRIMENTO VISUAL (far desenhado =
// min(farZ, visualFarCap); default kVisualFarCap — passar um valor
// maior devolve a geometria real).
Frustum computeFrustum(const Transform3D& tr, const CameraComp& cam,
                       f32 aspect, f32 visualFarCap = kVisualFarCap);

// ---- desenho (emite no UiContext — line batch dos gizmos) --------------------
// GRUPO D: (vw,vh,ox,oy) = o MAPEAMENTO da viewport 3D (NDC→(vw×vh) local
// + (ox,oy) de origem). Defaults 0,0,0,0 = o ECRÃ TODO (o de sempre — os
// testes e o Play ficam IGUAIS). O ASPECTO do frustum continua sw/sh da
// SUPERFÍCIE (o jogo renderiza o ecrã todo em Play — o shape não muda).

// desenha UMA câmara no estado D17: glifo + frustum fino (mudo a 35% sem
// seleção, âmbar com seleção); `selected` acrescenta os HANDLES de canto
// 12dp. Editor-only (o chamador faz o gate).
void drawFrustum(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
                 const Frustum& f, bool selected, f32 vw = 0.0f,
                 f32 vh = 0.0f, f32 ox = 0.0f, f32 oy = 0.0f);

// desenha TODAS as câmaras visíveis da cena (o loop do main; cada frustum
// com o aspeto do ecrã; a selecionada ganha os handles)
void drawAll(UiContext& ui, Scene& scene, const Mat4& vp, f32 sw, f32 sh,
             Handle selected, f32 vw = 0.0f, f32 vh = 0.0f, f32 ox = 0.0f,
             f32 oy = 0.0f);

// ---- hit-test ------------------------------------------------------------------

// o handle de CANTO sob o toque (câmara selecionada): 0 = nada, 1..4 =
// canto i-1 (o CENTRO/far da 0.7.7 MORREU no D17-c — o far edita-se no
// Inspector). Distâncias em dp de ecrã (kHandleHitDp).
int pickHandle(const Mat4& vp, f32 sw, f32 sh, const Frustum& f,
               f32 px, f32 py);

// o TIC da câmara cujo GLIFO está sob o toque (D17-f: hit-test pelo GLIFO
// — raio kGlyphHitDp em torno do olho projetado; as LINHAS do frustum
// NUNCA interceptam toque). Handle::invalid se nenhum.
Handle pickCameraTic(Scene& scene, const Mat4& vp, f32 sw, f32 sh,
                     f32 px, f32 py);

// 0.7.10 — PICKER DO VIEWPORT COM PRIORIDADE DE OBJETOS: primeiro os TICs
// SELECIONÁVEIS (meshes — projeção do CENTRO a kTicPickPx, o mais próximo
// do toque), SÓ DEPOIS a câmara (via GLIFO). A câmara nunca rouba o toque
// de um objeto (o C33: tocar num objeto DENTRO do cone selecionava a
// câmara). Handle::invalid se nada.
Handle pickSceneTic(Scene& scene, const Mat4& vp, f32 sw, f32 sh,
                    f32 px, f32 py);

// ---- drag dos handles (PURO — âncoras, nunca acumulado) ------------------------

// handle de CANTO: fator radial do dedo em torno do CENTRO PROJETADO da
// câmara (px) — fov = âncora × d1/d0 (snap: arredonda a 5°). (O dragFar
// da 0.7.7 morreu com o handle do centro — D17-c.)
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
