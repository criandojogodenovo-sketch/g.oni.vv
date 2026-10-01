#pragma once
// components/AnimationPlayer.h — ANIMAÇÃO de TICs e UI (0.8.0, F7).
//
// O AnimationPlayer é o componente que DÁ MOVIMENTO à cena: tracks de
// keyframes que animam propriedades do TIC dono (pos/rot/escala do
// Transform3D — rot em GRAUS Euler XYZ, a convenção do Inspector) e de
// elementos de UI do UiCanvas do dono (pos/cor/alpha, por NOME do
// elemento). Curvas por track: LINEAR (default) ou BEZIER — cúbica por
// canal com tangentes in/out por key (OFFSETS de valor); handles a ZERO
// dão um EASE suave (smoothstep) entre keys, handles proporcionais ao
// segmento (out=Δ/3, in=−Δ/3) reproduzem a reta — o editor de timeline
// parte daí para curvar.
//
// PLAYBACK: modos once / loop / ping-pong com VELOCIDADE ajustável
// (speed multiplicador). O tempo (`time`) e o sentido (`dir` do ping-pong)
// são RUNTIME — nunca serializados (uma cena carregada nasce parada no
// t=0). O avanço em PLAY corre pelo AnimationSystem (TickGroup::Update,
// gate `enabled` como a física); no EDITOR o preview vive na timeline
// (ui/Timeline) com sandbox próprio (captura/restaura a pose).
//
// CLIPS: lista nomeada (o clip 0 "edit" é o da timeline; 0.8.1 acrescenta
// os clips importados de glTF). `activeClip` escolhe o que reproduz.
//
// SERIALIZAÇÃO (.goni): {"type":"AnimationPlayer", "mode":"loop",
// "speed":1, "active":0, "clips":[{"name":"edit","tracks":[
//   {"target":"pos","element":"","curve":"lin",
//    "keys":[{"t":0,"v":[...],"in":[...],"out":[...]}]}]}]} — defaults
// omitidos (ficheiros 0.7.x abrem sem o componente, como sempre).
//
// GL-free / host-testável: interpolação e apply são funções puras.
#include "core/Component.h"
#include "core/Types.h"
#include <string>
#include <vector>

namespace vv {

class Scene;
struct Tic;

// propriedade animada por um track
enum class AnimTarget : u8 {
    TicPos   = 0,   // Transform3D.pos (v[0..2])
    TicRot   = 1,   // Transform3D.rot — GRAUS Euler XYZ (v[0..2])
    TicScale = 2,   // Transform3D.scale (v[0..2])
    UiPos    = 3,   // UiElement.ox/oy (v[0..1]; por nome em `element`)
    UiColor  = 4,   // UiElement.color RGB (v[0..2])
    UiAlpha  = 5,   // UiElement.color[3] (v[0])
    JointPos = 6,   // SkeletonComp joint TRS LOCAL — pos (0.8.2, por nome)
    JointRot = 7,   // idem rot (graus euler)
    JointScale = 8, // idem scale
};

// nome canônico do alvo (serializer + labels da timeline)
inline const char* animTargetName(AnimTarget t) {
    switch (t) {
        case AnimTarget::TicPos:   return "pos";
        case AnimTarget::TicRot:   return "rot";
        case AnimTarget::TicScale: return "escala";
        case AnimTarget::UiPos:    return "uipos";
        case AnimTarget::UiColor:  return "uicor";
        case AnimTarget::UiAlpha:  return "uialpha";
        case AnimTarget::JointPos:   return "jpos";     // 0.8.2
        case AnimTarget::JointRot:   return "jrot";     // 0.8.2
        case AnimTarget::JointScale: return "jescala";  // 0.8.2
    }
    return "pos";
}

// rótulo do alvo na UI (PT, como o resto do editor)
inline const char* animTargetLabel(AnimTarget t) {
    switch (t) {
        case AnimTarget::TicPos:   return "posicao";
        case AnimTarget::TicRot:   return "rotacao";
        case AnimTarget::TicScale: return "escala";
        case AnimTarget::UiPos:    return "ui pos";
        case AnimTarget::UiColor:  return "ui cor";
        case AnimTarget::UiAlpha:  return "ui alpha";
        case AnimTarget::JointPos:   return "joint pos";    // 0.8.2
        case AnimTarget::JointRot:   return "joint rot";    // 0.8.2
        case AnimTarget::JointScale: return "joint escala"; // 0.8.2
    }
    return "posicao";
}

// um keyframe: tempo + valor (4 canais) + tangentes bezier (OFFSETS de
// VALOR em relação ao key; tanIn pertence a ESTE key como ponto de chegada,
// tanOut como ponto de partida — handles a ZERO = ease suave, não linear)
struct AnimKey {
    f32 t = 0.0f;
    f32 v[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    f32 tanIn[4]  = {0.0f, 0.0f, 0.0f, 0.0f};
    f32 tanOut[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

enum class AnimCurve : u8 { Linear = 0, Bezier = 1 };

// um track: alvo + curva + keys ordenadas por t
struct AnimTrack {
    AnimTarget target = AnimTarget::TicPos;
    std::string element;              // UiPos/UiColor/UiAlpha: nome do elemento
    AnimCurve curve = AnimCurve::Linear;
    std::vector<AnimKey> keys;

    f32 duration() const { return keys.empty() ? 0.0f : keys.back().t; }
    void sortKeys();                  // ordena por t (estável)
};

// clip nomeado (0.8.0: o clip "edit" da timeline; 0.8.1: + os de glTF)
struct AnimClip {
    std::string name;
    std::vector<AnimTrack> tracks;

    f32 duration() const;             // max dos tracks
};

// interpolação: avalia o track em `t` (puro — CI). false = sem keys.
// Fora do intervalo: primeiro/último valor (clamp honesto).
bool evalTrack(const AnimTrack& tr, f32 t, f32 out[4]);

// blend de dois samples (0.8.3 usa isto; exposto p/ testes desde 0.8.0)
void blendKeys(const f32 a[4], const f32 b[4], f32 w, f32 out[4]);

class AnimationPlayer : public Component {
public:
    enum class Mode : u8 { Once = 0, Loop = 1, PingPong = 2 };

    std::vector<AnimClip> clips;
    i32  activeClip = 0;      // índice em clips (-1 = nenhum)
    bool playing = false;     // avança no tick (system ou timeline)
    f32  time = 0.0f;         // segundos DENTRO do clip
    f32  speed = 1.0f;        // multiplicador (0.1..3 na UI)
    Mode mode = Mode::Loop;

    // ---- 0.8.3 (F7): BLENDING -----------------------------------------------
    // O clip ATIVO mistura-se com o `blendClip` ao peso `blendWeight`
    // (0 = só o ativo, 1 = só o blend) por TRACK correspondente (mesmo
    // alvo+elemento). Crossfade = peso ANIMADO 0→1 em `blendDuration`
    // segundos; ao chegar a 1 o blend clip passa a ser o ATIVO com o tempo
    // CONTÍNUO (sem salto — a pose já era dele). Tracks de JOINT sem
    // correspondência fade para o BIND do joint; os restantes passam o
    // valor do clip ativo (crossfade pressupõe os mesmos alvos — rigs
    // iguais; documentado no relatório). RUNTIME: nunca serializado.
    i32  blendClip = -1;        // -1 = sem blend
    f32  blendWeight = 0.0f;    // 0..1
    f32  blendTime = 0.0f;      // tempo DENTRO do clip de blend
    f32  blendDuration = 0.0f;  // >0 = crossfade automático em curso
    f32  blendElapsed = 0.0f;   // tempo decorrido do crossfade

    // ---- clip de edição (timeline) -----------------------------------------
    // garante clips[0] (cria "edit" se vazio) e devolve-lhe o ponteiro
    AnimClip* editClip();
    // track do clip ativo (nullptr se sem clip/índice fora)
    AnimClip* activeClipPtr();
    const AnimClip* activeClipPtr() const;
    // cria (ou devolve o existente com o MESMO alvo+elemento) track no clip
    // de edição
    AnimTrack* addTrack(AnimTarget target, const char* element = "");

    // ---- playback ------------------------------------------------------------
    // avança `time` conforme mode/speed/dir (puro no estado do player);
    // Once para no fim (playing=false), Loop dá a volta, PingPong reflete
    // o sentido (dir interno). 0.8.3: o clip de BLEND avança em paralelo
    // (mesma velocidade) e o crossfade anima o peso até completar a troca.
    void advance(f32 dt);
    // duração do clip ativo (0 sem clip)
    f32 duration() const;
    // nome do modo (serializer + botão da timeline)
    static const char* modeName(Mode m);

    // ---- 0.8.3 (F7): blending -------------------------------------------------
    // blend MANUAL: mistura com o clip `clipIdx` ao peso fixo `w`
    void setBlend(i32 clipIdx, f32 w);
    // CROSSFADE: peso 0→1 em `dur` segundos; ao completar o clip passa a
    // ser o ATIVO com o tempo contínuo (sem salto). Re-blend troca o alvo.
    void crossfade(i32 clipIdx, f32 dur = 0.4f);
    // cancela o blend (fica o clip ATIVO, tempo onde está)
    void stopBlend();
    // há blend em curso? (clip válido com peso > 0)
    bool blending() const { return blendClip >= 0 && blendWeight > 0.0f; }

    // ---- aplicação (escreve nos componentes do TIC dono) -------------------
    // avalia TODOS os tracks do clip ativo em `time` e escreve:
    //   Tic*  → Transform3D (pos/rot-euler→quat/scale + updateWorld)
    //   Ui*   → UiElement por nome (ox/oy / RGB / alpha)
    // Tracks cujo alvo não existe (TIC sem Transform3D, elemento sumiu)
    // são SALTADOS — nunca crasha. Retorna ao mundo 3D no MESMO frame.
    void apply(Scene& scene, const Tic& owner) const;
    void apply(Scene& scene, Handle owner) const;

    // dir do ping-pong (runtime; 1 = frente, −1 = atrás)
    i32 dir() const { return dir_; }
    void resetDir() { dir_ = 1; }

private:
    i32 dir_ = 1;   // sentido corrente do ping-pong (runtime)
};

} // namespace vv
