#pragma once
// core/AutoSave.h — 0.12-QoL D: AS DECISÕES PURAS do auto-save
// («o seguro de vida do motor», a ordem do dono).
//
//   • O TIMER: a cada 180s (3 minutos), se houver alterações na cena
//     (dirty — o dump da cena difere do último persistido), gravar
//     autosave.goni NA RAIZ DO PROJETO. A escrita corre NO FIM DO FRAME
//     (o main decide — 1×/3min não muda o perfil; um thread separado
//     complicaria o SAF sem necessidade).
//   • A RECUPERAÇÃO: ao abrir o projeto, se o autosave.goni for MAIS
//     RECENTE que o .goni principal da cena ativa, o editor pergunta
//     «Encontrámos trabalho não guardado. Desejas recuperar?» com
//     [Recuperar] e [Descartar] (o pop-up 2.5D no EditorUi).
//
// PURA / GL-free / Android-free — host-testável no CI (o R-056 aferra).
// O storage que não sabe o mtime (o SAF não expõe stat) devolve −1 e a
// decisão é HONESTA: sem as duas datas, NÃO há pergunta (nunca se
// recupera às cegas).
#include "core/Types.h"

namespace vv {
namespace autosave {

// O PERÍODO do dono (180s — «a cada 3 minutos»).
inline constexpr f32 kPeriodSec = 180.0f;

constexpr const char* kAutosaveFile = "autosave.goni";

// O tick do timer: o auto-save vence quando o relógio do frame passou o
// período desde o último tick. Puro (o main acumula o relógio com o dt
// real — o mesmo relógio do toast/fps).
inline bool due(f32 sinceLastTick, f32 period = kPeriodSec) {
    return sinceLastTick >= period;
}

// A decisão da recuperação: RECUPERAR só com AS DUAS DATAS conhecidas e
// o autosave ESTRITAMENTE mais recente que o .goni principal. Igual
// (gravado na sequência) = o principal está atual → nada a perguntar.
enum class Recovery : u8 {
    None,      // nada a perguntar (sem autosave, sem datas, ou mais velho)
    Recover,   // o autosave é mais recente → o pop-up pergunta ao dono
};
inline Recovery decide(f64 mtimeAutosave, f64 mtimeScene) {
    // −1 = o storage não sabe (SAF) ou o ficheiro não existe — honesto
    if (mtimeAutosave < 0.0 || mtimeScene < 0.0) {
        return Recovery::None;
    }
    return mtimeAutosave > mtimeScene ? Recovery::Recover : Recovery::None;
}

} // namespace autosave
} // namespace vv
