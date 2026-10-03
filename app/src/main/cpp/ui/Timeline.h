#pragma once
// ui/Timeline.h — EDITOR DE TIMELINE do AnimationPlayer (0.8.0, F7).
//
// Strip inferior do VIEWPORT CENTRAL (só em editor 3D, só quando o TIC
// selecionado tem AnimationPlayer): NÃO sobrepõe os painéis existentes
// (Hierarchy/Inspector ficam intactos; o viewport 3D é que perde a strip)
// e o orbit/gestos nascem só na área POR CIMA dela (o main passa o rect
// reduzido — a timeline nunca interfere com os painéis nem com o orbit).
//
//   ┌────────────────────────────────────────────────────────────┐
//   │ ANIM · edit · 2 tracks   [▶/⏸][■][loop] speed ——●—— [+track]│  header
//   │ 0s────1s────2s────3s (régua + cursor do tempo)              │  ruler
//   │ posicao    [lin][+key][−key][×]  ◆───◆───◆                  │  row
//   │ ui cor     [bez][+key][−key][×]  ◆───◆                      │  row
//   └────────────────────────────────────────────────────────────┘
//
// INTERAÇÃO (todas no slot 0, como o resto do editor):
//   • SCRUB — press+drag na área das keys/régua move o cursor do tempo e
//     APLICA a pose ao vivo (feedback imediato);
//   • KEY — press a ≤14 px de um diamante seleciona; drag MOVE o tempo da
//     key (clamp entre vizinhas); release ordena (sortKeys);
//   • +KEY (por row) — adiciona key NO CURSOR com o valor ATUAL da
//     propriedade (posa-se o objeto no viewport e clica-se "+");
//   • −KEY (por row) — remove a key mais próxima do cursor;
//   • CURVA (por row) — alterna linear↔bezier;
//   • ×TRACK (por row) — remove o track;
//   • [+track] — overlay com os 6 alvos (TIC: pos/rot/escala; UI:
//     pos/cor/alpha — só com canvas e elementos);
//   • [▶/⏸][■] — PREVIEW com sandbox: Play captura a pose (mini snapshot),
//     Stop restaura e recolhe o tempo a 0 (o preview nunca suja o editor);
//   • [loop] cicla once→loop→pingpong; speed 0.1..3.
//
// IDs (faixa exclusiva 7000+): play 7001 · stop 7002 · mode 7003 ·
// speed 7004 · +track 7005 · curve 7100+i · +key 7110+i · −key 7120+i ·
// ×track 7130+i · overlay add-track 7300+i. SEM scroll na lista de rows
// (máx 3 visíveis + "+N tracks"; scroll = dívida documentada).
#include "core/PlaySnapshot.h"
#include "ui/EditorUi.h"   // editor::EditorState (seleção/painéis)
#include "ui/UiContext.h"

namespace vv {

class Scene;
class AnimationPlayer;

namespace timeline {

// altura da strip (header 48 + régua 24 + 3 rows de 40)
constexpr f32 kTimelineH   = 192.0f;
constexpr f32 kHeaderH     = 48.0f;
constexpr f32 kRulerH      = 24.0f;
constexpr f32 kRowH        = 40.0f;
constexpr u32 kMaxRows     = 3;      // rows visíveis (cap — ver header)

// ids dos widgets (faixa 7000+; o EditorUi usa até ~6400)
constexpr u64 kIdPlay      = 7001;
constexpr u64 kIdStop      = 7002;
constexpr u64 kIdMode      = 7003;
constexpr u64 kIdSpeed     = 7004;
constexpr u64 kIdAddTrack  = 7005;
constexpr u64 kIdClip      = 7006;   // 0.8.1: seletor de clips (importados)
constexpr u64 kIdBlend     = 7007;   // 0.8.3: peso do blend (slider contextual)
constexpr u64 kIdFade      = 7370;   // +i: botão 'fade' do clip i (crossfade)
constexpr u64 kIdCurve     = 7100;   // +i
constexpr u64 kIdAddKey    = 7110;   // +i
constexpr u64 kIdDelKey    = 7120;   // +i
constexpr u64 kIdDelTrack  = 7130;   // +i
constexpr u64 kIdTrackMenu = 7300;   // +i (overlay de alvos)
constexpr u64 kIdClipItem  = 7350;   // +i (overlay de clips; 0.8.1)
constexpr u64 kIdClipNew   = 7360;   // "novo (edit)" no overlay de clips

// estado ENTRE frames (como o GizmoModeState da Toolbar)
struct State {
    bool scrubbing = false;    // drag do cursor de tempo em curso
    bool draggingKey = false;  // drag de uma key em curso
    i32  selTrack = -1;        // row selecionada
    i32  selKey = -1;          // key selecionada na row selTrack
    bool addTrackMenu = false; // overlay de escolha do alvo aberto
    bool clipMenu = false;     // 0.8.1: lista de clips aberta
    PlaySnapshot snap;         // sandbox do preview (capturada no Play)
    Handle previewTic{};       // TIC cujo preview está aberto (troca de
                               // seleção com preview a correr → stopPreview)
};

// a timeline está visível? (editor 3D + TIC selecionado ATIVO com player)
bool visible(bool playMode, bool uiMode, const Scene& scene, Handle selected);

// rect da strip: fundo do VIEWPORT CENTRAL (entre painéis, acima da status)
UiRect timelineRect(f32 sw, f32 sh, const safe::Insets& in, bool rightPanel);

// 0.8.4 — LAYOUT FLUIDO DO HEADER (função pura, afervel no CI): rects dos
// widgets do header para UMA largura de strip. No C33 (1600 px − 2 painéis
// de 300 = 1000 úteis, menos insets) os offsets FIXOS da 0.8.0 sobrepunham
// "clip:" (x+376..552) com o botão play (w−436) — w<988 = dois widgets a
// lutar pelo mesmo toque ("funções que param"). Regra nova: o cluster da
// DIREITA mantém-se fixo (play/stop/mode/vel/+, como sempre); o "clip:"
// ENCOSTA-SE ao play (folga 8) e ENCOLHE 176→120 quando precisa; o título
// usa o resto (labelFitted corta). Para toda a largura: sem par sobreposto.
struct HeaderLayout {
    UiRect title;    // área de texto do título (labelFitted)
    UiRect clip;     // botão "clip: <nome>"
    UiRect play;     // play/pause
    UiRect stop;     // stop
    UiRect mode;     // once/loop/pingpong
    UiRect slider;   // vel (ou blend — 0.8.3)
    UiRect add;      // "+" (novo track)
};
HeaderLayout headerLayout(const UiRect& r);

// para o PREVIEW (chamado pelo main ao entrar em Play de jogo ou trocar de
// seleção): restaura a pose capturada, para o player e recolhe o tempo
void stopPreview(Scene& scene, Handle tic, State& st);

// desenha + processa TUDO (header, régua, rows, keys, overlay); `dt` real
// do frame para o avanço do preview
void drawTimeline(UiContext& ui, const InputState& in, Scene& scene,
                  editor::EditorState& st, State& tl, f32 dt);
// 0.9.0 (spec E/K): overload com RECT EXPLÍCITO — o drawer do painel de
// baixo (aba Animação) passa o rect do conteúdo do drawer
void drawTimelineInRect(UiContext& ui, const InputState& in, Scene& scene,
                        editor::EditorState& st, State& tl, f32 dt,
                        const UiRect& rect);

} // namespace timeline
} // namespace vv
