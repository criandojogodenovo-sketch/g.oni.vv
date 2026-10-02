#pragma once
// components/AudioPlayer.h — o TIC DE ÁUDIO (0.8.11).
//
// "ESTRUTURA PRIMEIRO" (o prompt): o componente é DADOS + getters — o
// playback vive no AudioEngine do main (o Play liga autoplay/posicional;
// o preview do Inspector e o tyker play() usam o MESMO caminho).
//
// Campos (o Inspector edita; o serializer grava):
//   clipPath  — ref relativa "audio/x.gi" (vazia = sem clip)
//   autoplay  — começa ao ENTRAR em Play
//   loop      — repete
//   volume    — 0..1
//   pitch     — 0.5..2 (velocidade/afinação — resample linear)
//   posicional + raioInterno/raioExterno — atenuação pela distância ao
//   ouvinte (o wireframe do EDITOR desenha estes raios; NUNCA em Play)
//
// Runtime (não serializado): voiceId no AudioEngine (-1 = parado).
//
// VISUAL no editor: glifo de ALTIFALANTE em polilinha na posição do TIC +
// esfera wireframe do raio EXTERNO (só editor). Em PLAY nada desenha —
// só soa.
#include "core/Component.h"
#include "core/Types.h"
#include <string>

namespace vv {

class AudioPlayer : public Component {
public:
    std::string clipPath;         // "audio/salto.gi" ("" = sem clip)
    bool autoplay = false;
    bool loop = false;
    f32 volume = 1.0f;            // 0..1
    f32 pitch = 1.0f;             // 0.5..2
    bool posicional = false;
    f32 raioInterno = 1.0f;
    f32 raioExterno = 8.0f;

    // runtime — nunca serializado
    i32 voiceId = -1;
    bool previewing = false;      // o preview do Inspector (para ao sair)

    bool hasClip() const { return !clipPath.empty(); }
    // clamps de defesa (o Inspector chama ao editar; o load ao ler)
    void clampFields() {
        if (volume < 0.0f) volume = 0.0f;
        if (volume > 1.0f) volume = 1.0f;
        if (pitch < 0.5f) pitch = 0.5f;
        if (pitch > 2.0f) pitch = 2.0f;
        if (raioInterno < 0.1f) raioInterno = 0.1f;
        if (raioExterno < raioInterno) raioExterno = raioInterno + 0.1f;
    }
};

} // namespace vv
