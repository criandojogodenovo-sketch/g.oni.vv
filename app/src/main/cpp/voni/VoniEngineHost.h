#pragma once
// voni/VoniEngineHost.h — A ENGINE VISTA PELA V.ONI (a bridge do Host).
//
// Implementa voni::Host sobre a Scene real:
//   • getProp/setProp — RTTI de TICs (registo de propriedades AQUI; a
//     0.9.3 estende-o com funções RTTI para o Search.alvo.função)
//   • moveTic/explodeTic/importAnim — agem no TIC ANFITRIÃO (o dono do
//     componente Script; a central define-o antes de cada tick)
//   • transitionTo — injetado pelo main (as cenas vivem lá); sem hook dá
//     erro legível "transições indisponíveis"
//   • log — elog::info (o prefixo "voni: " já vem do core)
//
// PROPRIEDADES REGISTADAS (v0 — spec só mostra `pos`; os restantes nomes
// seguem os canónicos da engine, ver AnimationPlayer::animTargetName):
//   pos:Vec3 · rot:Vec3 (GRAUS Euler XYZ, a convenção do Inspector) ·
//   escala:Vec3 (alias: scale) · name:Txt · visible:Bool · active:Bool
// Componíveis com .x/.y/.z depois da propriedade (jogador.pos.x).
#include "core/Handle.h"
#include "core/Types.h"
#include "voni/Voni.h"

#include <functional>
#include <string>
#include <vector>

namespace vv {

class Scene;
struct Tic;

namespace voni_engine {

using TransitionHook = std::function<bool(const std::string& from,
                                          const std::string& to,
                                          std::string& err)>;
using SceneNameFn = std::function<std::string()>;
// 0.12-X: o delta do arrasto de olhar (camdrag) — o main alimenta o
// acumulador por frame; o consumo DEVE zerar (o 2.º consumidor do mesmo
// frame recebe zeros — a auto-proteção do Host::lookDelta)
using LookDeltaFn = std::function<bool(f32& dx, f32& dy)>;

} // namespace voni_engine
} // namespace vv

namespace voni {

// (implementação em VoniEngineHost.cpp — vv::Scene por members)
class EngineHost : public Host {
public:
    EngineHost(vv::Scene& scene, vv::Handle hostTic,
               const vv::voni_engine::TransitionHook* transition,
               const vv::voni_engine::SceneNameFn* sceneName,
               const vv::voni_engine::LookDeltaFn* lookDelta = nullptr);

    void log(const char* line) override;
    bool getProp(const std::string& ticName,
                 const std::vector<std::string>& chain, Value& out,
                 std::string& err) override;
    bool setProp(const std::string& ticName,
                 const std::vector<std::string>& chain, const Value& v,
                 std::string& err) override;
    bool ticExists(const std::string& ticName) override;
    void moveTic(f32 dx, f32 dy, f32 dz) override;
    void explodeTic(bool hide) override;
    bool importAnim(const std::string& name, std::string& err) override;
    bool stopAnim(std::string& err) override;                     // 0.11-S
    bool blendAnim(const std::string& clip, f32 seconds,
                   std::string& err) override;                    // 0.11-S
    bool spawnTic(const std::string& tipo, f32 x, f32 y, f32 z,
                  std::string& outName, std::string& err) override;  // 0.12-U
    bool lookDelta(f32& dx, f32& dy) override;                       // 0.12-X
    bool transitionTo(const std::string& fromName, const std::string& toName,
                      std::string& err) override;
    std::string currentSceneName() override;
    bool search(const std::string& target,
                const std::vector<std::string>& chain, Value& out,
                std::string& err) override;
    f64 frameDt() override;

    // ---- RTTI (o registo que a 0.9.3 estende com funções) -----------------
    // true se `prop` é uma propriedade registada de TIC
    static bool isTicProp(const std::string& prop);

private:
    vv::Scene& scene_;
    vv::Handle hostTic_;

    vv::Tic* hostTic();
    vv::Tic* ticByName(const std::string& name, std::string& err);

    // lê/escreve UMA propriedade (o 1º elemento da cadeia) num TIC
    bool propGet(vv::Tic& tic, const std::string& prop, Value& out,
                 std::string& err);
    bool propSet(vv::Tic& tic, const std::string& prop, const Value& v,
                 std::string& err);

    const vv::voni_engine::TransitionHook* transition_;
    const vv::voni_engine::SceneNameFn* sceneName_;
    const vv::voni_engine::LookDeltaFn* lookDelta_;
    f64 dt_ = 1.0 / 60.0;   // o VoniSystem pode afinar por tick
public:
    void setDt(f64 dt) { dt_ = dt; }
};

} // namespace voni
