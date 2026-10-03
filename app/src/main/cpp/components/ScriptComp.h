#pragma once
// components/ScriptComp.h — COMPONENTE SCRIPT (0.9.2 §10 ✅).
//
// "Componente Script num TIC" — o TIC dono é o alvo dos comandos do script
// (move, Explode.TIC.et/.er, Import.Animation). O componente guarda SÓ
// DADOS (o fonte .voni + flags): a instância COMPILADA (voni::Script) vive
// no VoniSystem (a "central" da spec §3) — assim o componente mantém-se um
// POD copiável (o storage SoA copia componentes no add-with-init e um
// voni::Script é move-only).
//
// SERIALIZAÇÃO (.goni): {"type":"Script","source":"…","auto":true} —
// `source` é o fonte tal como está no editor (com \n escapados pelo Json);
// `auto` = correr automaticamente ao entrar em Play (default true).
// Run-time (compilado/erros) NUNCA se serializa.
//
// GL-free / Android-free: include mínimo (string + Component).
#include "core/Component.h"

#include <string>

namespace vv {

class ScriptComp : public Component {
public:
    // fonte V.ONI (extensão .voni — spec 0.9.2 §2); vazio = sem script
    std::string source;

    // correr automaticamente no Play (o runStart 1× + allmoments/frame
    // acontece pela "central" — VoniSystem::tick com playEnabled)
    bool autoPlay = true;
};

} // namespace vv
