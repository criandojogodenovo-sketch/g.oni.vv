#pragma once
// ui/CamGizmo.h — GIZMO DA CÂMARA DE CENA (0.7.7) · D17 (0.9.6.19) · D20
// (0.9.6.19b): A CÂMARA É UM OBJETO AINDA MAIS PEQUENO (o contrato medível).
//
// 0.9.6.19b — HOTFIX D20 (a spec do dono, sobre o D17):
//   (a) GLIFO 20dp (era 24) CONSTANTE EM ECRÃ na posição da câmara;
//   (b) FRUSTUM de PREVIEW com comprimento CONSTANTE EM ECRÃ =
//       clamp(12% da distância olho→câmara, 48..120dp) — o gizmo NÃO
//       desenha o extent real do far plane (o cap antigo de ~80dp de
//       altura morreu); o frustum REAL do render NÃO MUDA — só o gizmo;
//   (c) HANDLES 10dp (era 12) nos 4 cantos, SÓ com seleção;
//   (d) linhas 1-2px (mudo ~35% sem seleção, âmbar com seleção);
//   (e) o gizmo de mover continua NO GLIFO e a ordem de hit-test do D17
//       fica INTACTA (gizmo > handles > frustum intocável);
//   (f) PIN medível: o bounding do gizmo ≤6% da área do viewport COM
//       seleção e ≤4% SEM — medido pelo script de medidas (método PASSO 0)
//       em 2 densidades (gizmoBoundsPx + o FASE 17 do device virtual).
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
#include "ui/ScrollMath.h"  // 0.9.6.19b (D20): UiRect (o bounding do gizmo)

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
// 0.9.6.19b (D20): o glifo 20dp e o handle 10dp (a câmara AINDA MAIS
// PEQUENA — o contrato medível do dono).
inline constexpr f32 kGlyphDp      = 20.0f;  // (a) o glifo 20dp constante
inline constexpr f32 kGlyphHitDp   = 18.0f;  // o raio de toque do glifo
                                             // (10dp do glifo + dedo — o
                                             // agarre fácil mantém-se)
inline constexpr f32 kHandleDp     = 10.0f;  // (c) o handle de canto 10dp
inline constexpr f32 kHandleHitDp  = 16.0f;  // o hit do canto (10 + dedo)
inline constexpr f32 kMutedAlpha   = 0.35f;  // (b) o cinza mudo ~35% alfa
// o traço do frustum em PX DE ECRÃ (a spec do dono: «frustum fino (1-2px)»)
inline constexpr f32 kFrustumLinePx = 1.0f;   // sem seleção
inline constexpr f32 kFrustumLinePxSel = 2.0f; // com seleção

// ---- 0.9.6.19b (D20) — O FRUSTUM DE PREVIEW (comprimento CONSTANTE) ------
// O comprimento desenhado (EM ECRÃ) é clamp(12% da distância olho→câmara,
// 48..120dp) — não cresce com o zoom (o mundo por dp muda, o dp não) e
// NUNCA é o extent real do far plane. O frustum do RENDER (gameProj) não
// vê nada disto.
inline constexpr f32 kPreviewDistPct = 0.12f;  // 12% da distância olho→câmara
inline constexpr f32 kPreviewMinDp   = 48.0f;  // o piso do comprimento
inline constexpr f32 kPreviewMaxDp   = 120.0f; // o teto do comprimento

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

// ---- 0.9.6.19b (D20) — O CAP DO PREVIEW (substitui o cap G1-4 da 0.7.10):
// devolve o far VISUAL em unidades de mundo que projeta EXATAMENTE
// clamp(12%·|olho→câmara|, 48..120dp) de comprimento no ecrã — o ppu mede-se
// no OLHO da câmara de cena (projeta camPos e camPos+X pelo MESMO vp, o
// método da 0.7.10). Puro/afervel; a distância é a da ORBIT de edição
// (eyeEditor) ao olho da câmara de cena (camPos).
f32 previewCapWorld(const Mat4& vp, f32 sw, f32 sh, const Vec3& eyeEditor,
                    const Vec3& camPos);
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

// desenha UMA câmara no estado D17/D20: glifo + frustum fino (mudo a 35% sem
// seleção, âmbar com seleção); `selected` acrescenta os HANDLES de canto
// 10dp (D20). Editor-only (o chamador faz o gate).
void drawFrustum(UiContext& ui, const Mat4& vp, f32 sw, f32 sh,
                 const Frustum& f, bool selected, f32 vw = 0.0f,
                 f32 vh = 0.0f, f32 ox = 0.0f, f32 oy = 0.0f);

// desenha TODAS as câmaras visíveis da cena (o loop do main; cada frustum
// com o aspeto do ecrã; a selecionada ganha os handles). 0.9.6.19b (D20):
// recebe o OLHO da orbit de edição — o comprimento do preview é
// clamp(12%·dist(olho→câmara), 48..120dp) constante em ecrã.
void drawAll(UiContext& ui, Scene& scene, const Mat4& vp, f32 sw, f32 sh,
             Handle selected, const Vec3& eyeWorld, f32 vw = 0.0f,
             f32 vh = 0.0f, f32 ox = 0.0f, f32 oy = 0.0f);

// ---- 0.9.6.19b (D20) — O BOUNDING DO GIZMO (o pin medível) ----------------
// o rect (px de ecrã) que abrange TUDO o que o gizmo de UMA câmara desenha:
// glifo + frustum (near/far/cone) + handles (quando selecionado). O método
// de medidas do pin (≤6% com seleção / ≤4% sem, 2 densidades) parte daqui —
// a MESMA projeção do draw (gizmo::projectPoint).
UiRect gizmoBoundsPx(const Mat4& vp, f32 sw, f32 sh, const Frustum& f,
                      bool selected, f32 vw = 0.0f, f32 vh = 0.0f,
                      f32 ox = 0.0f, f32 oy = 0.0f);

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
