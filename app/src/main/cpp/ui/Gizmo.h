#pragma once
// ui/Gizmo.h — GIZMOS DE TRANSFORMAÇÃO (0.6.9): Mover / Rodar / Escalar.
//
// Só no TIC selecionado, SÓ EM EDITOR (nunca em PLAY). O hit-test é 3D
// (raio do toque contra eixos/anéis/handles) e o desenho é a PROJEÇÃO da
// geometria 3D para segmentos de ecrã (QuadBatch::line — sempre visível,
// por cima do 3D e por baixo dos painéis).
//
// COORDENADAS: a matemática do gizmo é GLOBAL no espaço do mundo
// (Transform3D::pos/rot/scale do TIC — sem hierarquia de pais na F9).
//
// NÚCLEO GL-FREE: projeção/raio/hit-test/drag/snapping são funções PURAS
// (testáveis no CI Linux com stub GL); o desenho emite quads no batch de
// CPU do UiContext (mesma técnica da UI — nada de GL novo além de linhas).
//
// CORES DE EIXO — EXCEÇÃO DOCUMENTADA AO TEMA (só nos gizmos 3D, como
// nos editores convencionais): X vermelho / Y verde / Z azul (CONTEÚDO
// 3D, não chrome — os valores vivem no Theme, gate R-030). Toda a UI 2D
// mantém o grafite+âmbar intacto.
//
// SNAPPING (toggle): mover ao grid de 0.5 u, escalar em passos de 0.25,
// rodar em passos de 15°.
#include "math/Math.h"
#include "core/Types.h"
#include "ui/Theme.h"   // 0.9.6.10: os eixos ligam à fonte única (R-030)

namespace vv {

class Camera;
class UiContext;
class InputState;
class Transform3D;   // components/Transform3D.h

namespace gizmo {

// ---- modos e alvos -----------------------------------------------------------

enum class Mode : u8 {
    Move = 0,     // 3 setas por eixo + 3 quads de plano (XY/XZ/YZ)
    Rotate = 1,   // 3 anéis por eixo (drag angular no plano do anel)
    Scale = 2,    // 3 handles por eixo + handle central uniforme
};

// alvo do hit-test/drag: eixo, plano (mover) ou handle (escalar)
enum class Axis : u8 {
    None = 0,
    X, Y, Z,      // eixo (mover/rodar/escalar)
    XY, XZ, YZ,   // plano (mover)
    Center,       // handle central (escalar uniforme)
};

// cores de eixo (exceção documentada ao tema — SÓ nos gizmos 3D; CONTEÚDO,
// não chrome). 0.9.6.10 (GRUPO UI · spec G): os VALORES vivem no Theme
// (theme::kTheme.axisX/Y/Z/dim — «cores só no ficheiro de Theme», o gate
// R-030); o hover é o ACCENT âmbar (era branco mono — a identidade nova)
inline constexpr f32 kAxisX[4]     = {233.0f / 255.0f, 73.0f / 255.0f, 59.0f / 255.0f, 1.0f};   // vermelho #E9493B
inline constexpr f32 kAxisY[4]     = {90.0f / 255.0f, 203.0f / 255.0f, 95.0f / 255.0f, 1.0f};   // verde #5ACB5F
inline constexpr f32 kAxisZ[4]     = {79.0f / 255.0f, 146.0f / 255.0f, 245.0f / 255.0f, 1.0f};  // azul #4F92F5
inline constexpr f32 kAxisDim[4]   = {158.0f / 255.0f, 158.0f / 255.0f, 158.0f / 255.0f, 1.0f};// cinza #9E9E9E
static_assert(kAxisX[0] == theme::kTheme.axisX[0] && kAxisZ[2] == theme::kTheme.axisZ[2],
              "eixos desincronizados de Theme.h — fonte única violada (gate R-030)");

// cor do eixo/alvo (X/Y/Z→cor própria; planos→cor do 3º eixo mais escuro;
// Center→cinza; hover em qualquer um → kAxisHover)
const f32* axisColor(Axis a, bool hovered);

// tamanho de ecrã CONSTANTE: comprimento do gizmo no mundo = kScreenLen
// px projetados a 1 u de distância → len_world = dist * kScreenLen.
inline constexpr f32 kScreenLen = 0.16f;   // ~16% da distância da câmara

// espessuras dos traços (px de ecrã)
inline constexpr f32 kLineW     = 5.0f;    // eixo/seta
inline constexpr f32 kLineWHov  = 8.0f;    // eixo sob press/hover
inline constexpr f32 kRingSegs  = 48.0f;   // segmentos por anel
inline constexpr f32 kHandlePx  = 26.0f;    // lado do handle de escala/centro

// limiares de hit-test (px de ecrã)
inline constexpr f32 kHitAxisPx   = 22.0f;   // eixos/setas/anéis/handles (hover)
inline constexpr f32 kHitPlanePx  = 30.0f;   // quads de plano (pelo centro)
inline constexpr f32 kHitCenterPx = 26.0f;   // handle central

// 0.7.9 — GRAB-LOCK: alvo de toque GENEROSO no arranque do drag (o alvo
// mínimo de toque do Android; o hover continua fino — 22 px — para o
// destaque não "acender" meio viewport). "grab ligeiramente fora ainda
// agarra" (C33: dedos gordos em ecrã de 720 px de altura).
inline constexpr f32 kGrabPx = 44.0f;

// snapping
inline constexpr f32 kSnapMove   = 0.5f;    // unidades de mundo (grid)
inline constexpr f32 kSnapRot    = 0.2617994f;   // 15° em rad
inline constexpr f32 kSnapScale  = 0.25f;   // passos do fator

// ---- estado (um por editor — o main guarda) -----------------------------------

struct GizmoState {
    Mode mode   = Mode::Move;
    bool snap   = false;

    Axis hovered = Axis::None;   // alvo sob o dedo (destaque no desenho)
    Axis active  = Axis::None;   // drag em curso
    i32  dragSlot = -1;          // slot do dedo que arrasta

    // âncoras do drag — a pose do TIC no arranque (o drag é RELATIVO: a
    // pose final = âncora + delta; nunca acumula em cima da pose em curso)
    Vec3 anchorPos{0.0f, 0.0f, 0.0f};
    Quat anchorRot = Quat::identity();
    Vec3 anchorScale{1.0f, 1.0f, 1.0f};
    Vec3 anchorHit{0.0f, 0.0f, 0.0f};   // interseção raio×plano no arranque
    f32  anchorAngle = 0.0f;            // rotate: ângulo do dedo (ecrã)
    f32  anchorDist  = 0.0f;            // scale: |dedo − centro| (ecrã)
};

// o gizmo desenha/aceita input? (EDITOR + TIC selecionado com Transform3D)
bool visible(bool playMode, bool hasSelection);

// ---- câmara como base de vista (raio analítico — sem inversas) ---------------

struct ViewBasis {
    Vec3 eye{};
    Vec3 fwd{};                 // normalizado (aponta DO eye PARA a cena)
    Vec3 right{};              // normalizado
    Vec3 up{};                  // normalizado
    f32  tanHalfFov = 0.5773f;  // tan(fovY/2) — 60° por omissão
    f32  aspect = 16.0f / 9.0f;
};

// base da câmara orbit do engine (render/Camera.h — GL-free)
ViewBasis viewBasis(const Camera& cam, f32 aspect);

// direção do raio de um pixel (px em coords de ecrã y-para-baixo)
Vec3 screenRayDir(const ViewBasis& b, f32 px, f32 py, f32 sw, f32 sh);

// interseção do raio do pixel com o plano {n·(p−origin)=0}; sem solução
// (raio ~ paralelo ao plano) devolve `anyHit=false`
Vec3 planeHit(const ViewBasis& b, const Vec3& n, const Vec3& planeOrigin,
              f32 px, f32 py, f32 sw, f32 sh, bool& anyHit);

// ---- 0.7.9 — GRAB-LOCK --------------------------------------------------------
//
// REGRESSÃO DO C33 (gizmo oscila e foge do dedo): o press edge NUNCA
// capturava as âncoras geométricas (anchorHit/anchorAngle/anchorDist
// ficavam a ZERO — o objeto saltava para distâncias do hit contra o plano
// de VISTA medido contra a pos ATUAL do gizmo, que MEXE com o drag →
// realimentação → oscilação/fuga). O grab-lock captura TUDO no touch down:
// o ALVO (eixo/anel/handle), o RAIO (base da câmara no grab) e o PLANO
// FIXO (⟂ à câmara no grab, passa pela pos do TIC NO ARRANQUE — nunca
// pela pos atual). Durante o move NÃO há hit-test: o delta do dedo é
// projetado no plano FIXO; o drag não depende do dedo estar sobre o
// gizmo (o gizmo move-se com o objeto). Touch up liberta o lock.
struct Grab {
    Axis      target = Axis::None;   // eixo/anel/plano/handle agarrado
    i32       slot = -1;             // dedo dono do drag
    ViewBasis basis;                 // base da câmara NO GRAB (raio fixo)
    Vec3      planeOrigin{};         // pos do TIC no arranque (o plano fixo
                                     // passa POR AQUI — nunca a pos atual)
    Vec3      planeNormal{};         // normal do plano de drag NO GRAB
    Vec3      anchorHit{};           // hit raio×plano FIXO no arranque
    f32       anchorAngle = 0.0f;    // rotate: ângulo do dedo (ecrã) no grab
    f32       anchorDist = 0.0f;     // scale: |dedo − centro| (ecrã) no grab
    f32       anchorOx = 0.0f;       // CENTRO projetado da pos de arranque
    f32       anchorOy = 0.0f;       // (rotate/scale medem contra ESTE)

    bool valid() const { return target != Axis::None && slot >= 0; }
};

// captura o grab no press edge: hit-test com raio GENEROSO (kGrabPx) e
// âncoras todas medidas NO ARRANQUE. Alvo None = não agarrou.
Grab beginGrab(Mode mode, const Mat4& vp, const Vec3& origin, f32 len,
               f32 sw, f32 sh, f32 px, f32 py, i32 slot,
               const ViewBasis& basis);

// hit do dedo AGORA no plano FIXO do grab (raio da base do GRAB — a
// câmara poder orbitar com outro dedo que o delta não salta)
Vec3 grabHit(const Grab& g, f32 px, f32 py, f32 sw, f32 sh, bool& anyHit);

// ---- hit-test 3D (distâncias em px de ECRÃ — consistente com o desenho) -------

// projeção de um ponto do mundo com a vp (proj*view); false se atrás da
// câmara (w <= 0). GRUPO D: (ox,oy) = a ORIGEM do rect da viewport — o
// NDC mapeia para (sw×sh) e SOMA a origem (o viewport 3D deixou de ser o
// ecrã todo: a câmara tem o aspect DO RECT e o ecrã-real é rect+origem;
// default 0,0 = o ecrã todo — os testes e o Play ficam IGUAIS)
bool projectPoint(const Mat4& vp, const Vec3& p, f32 sw, f32 sh,
                  f32& sx, f32& sy, f32 ox = 0.0f, f32 oy = 0.0f);

// distância (px) do ponto ao SEGMENTO projetado (a,b em ecrã)
f32 distToSegmentPx(f32 px, f32 py, f32 ax, f32 ay, f32 bx, f32 by);

// escolhe o alvo sob o toque para o modo dado (o gizmo está em `origin`,
// desenhado com comprimento `len` no mundo). Axis::None se nada a alcançável.
// 0.7.9: `grabRadius` alarga o alvo dos EIXOS/ANÉIS no press edge (44 px
// — kGrabPx); o hover continua a chamar com o default fino (22 px).
Axis pickAxis(Mode mode, const Mat4& vp, const Vec3& origin, f32 len,
              f32 sw, f32 sh, f32 px, f32 py, f32 grabRadius = kHitAxisPx);

// ---- drag (matemática pura; devolve a NOVA pose a partir das âncoras) --------

// mover ao longo de um EIXO: hits = interseções raio×(plano de vista por
// origin) no arranque e agora. Delta projetado no eixo.
// 0.7.9: o snap arredonda a COORDENADA FINAL no eixo (âncora + delta),
// não o delta cru — âncoras fora do grid ficam em passos ABSOLUTOS do
// grid (o snap nunca "foge" do degrau em que o dedo está).
Vec3 dragMoveAxis(const Vec3& anchorPos, const Vec3& axisDir,
                  const Vec3& hit0, const Vec3& hit1, bool snap);

// mover num PLANO (XY/XZ/YZ): hits = interseções raio×plano do drag.
// 0.7.9: snap nas coordenadas FINAIS u/v (o mesmo princípio do eixo).
Vec3 dragMovePlane(const Vec3& anchorPos, const Vec3& planeNormal,
                   const Vec3& hit0, const Vec3& hit1, bool snap);

// rodar: ângulos do dedo em torno do CENTRO projetado (ecrã, atan2).
// O sinal é corrigido pela orientação do eixo face à câmara (fwd).
Quat dragRotate(const Quat& anchorRot, const Vec3& axisDir, const Vec3& fwd,
                f32 angle0, f32 angle1, bool snap);

// escalar num eixo: hits no plano de vista (como o move) → fator = 1 +
// delta/kScaleRef; uniforme (Center): fator = dist1/dist0 (px ao centro).
// 0.7.9: o snap arredonda a COMPONENTE FINAL da escala (âncora×fator),
// não o fator cru — o valor final fica em passos absolutos de 0.25.
Vec3 dragScaleAxis(const Vec3& anchorScale, Axis axis, const Vec3& axisDir,
                   const Vec3& hit0, const Vec3& hit1, bool snap);
Vec3 dragScaleUniform(const Vec3& anchorScale, f32 dist0, f32 dist1,
                      bool snap);

// ---- desenho (emite linhas/quads no batch da UI — CPU) -----------------------

// desenha o gizmo do modo dado na origem (pose do TIC), comprimento `len`,
// com o alvo `hovered` destacado. Emite no UiContext (solids_).
// GRUPO D: o MAPEAMENTO da viewport 3D — (vw,vh) = o TAMANHO do rect e
// (ox,oy) = a sua ORIGEM no ecrã (o NDC mapeia para (vw×vh) local e soma
// a origem). Defaults (0,0,0,0) = o ECRÃ TODO (os testes e o Play ficam
// IGUAIS ao de sempre — a fonte única é a assinatura)
void drawGizmo(UiContext& ui, const Mat4& vp, const Vec3& origin, f32 len,
               Mode mode, Axis hovered, f32 vw = 0.0f, f32 vh = 0.0f,
               f32 ox = 0.0f, f32 oy = 0.0f);

// comprimento no mundo para tamanho de ecrã constante
inline f32 gizmoLength(f32 camDist) { return camDist * kScreenLen; }

} // namespace gizmo
} // namespace vv
