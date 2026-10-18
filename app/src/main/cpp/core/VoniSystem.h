#pragma once
// core/VoniSystem.h — A "CENTRAL" DA V.ONI (0.9.2 §3 ✅).
//
// O dispatcher C++ que a spec descreve: "A 'central' (dispatcher C++)
// agenda: top-level 1× → on moment 1× → allmoments cada frame".
//
// COMO FUNCIONA:
//   • System no TickGroup::Update (como TransformSystem/AnimationSystem).
//   • tick() percorre os TICs ATIVOS com ScriptComp e garante a ordem:
//       Play (playEnabled): runStart 1× no 1º frame + allmoments/frame.
//       Editor (Run do editor de script): o editorStart marca a run; o tick
//       mantém allmoments a cada frame até stop (independente do Play).
//   • ERROS: NUNCA crasham (o voni::Script devolve Error com linha); o
//     sistema guarda o 1º erro por run e o main drena com popError →
//     engine.log + toast + barra de erro do editor.
//   • SANDBOX: o orçamento por tick vive no voni::Script (200k); a
//     profundidade 256 idem — aqui só se orquestra.
//
// HOST: o VoniEngineHost (voni/) é a ponte TIC/cena/RTTI; o transition.for
// precisa de mexer nas cenas (que vivem no main) → injeta-se um callback
// (setTransitionHook) — nos testes entra um fake, no main entra o real.
//
// GL-free / Android-free — host-testável (test_voni.cpp + test_wiring092).
#include "core/Handle.h"
#include "core/Tick.h"
#include "voni/Voni.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace vv {

class Scene;
struct Tic;

class VoniSystem : public System {
public:
    // main: true em modo Play (as runs automáticas correm só em Play)
    bool playEnabled = false;

    // ---- ciclo (chamado pelo main/editor) ---------------------------------
    // Play: o tick garante o start das runs automáticas (idempotente).
    // Editor: Run do editor de script — devolve false + err (compile/start).
    bool editorStart(Scene& scene, Handle tic, voni::Error& err);
    void editorStop(Handle tic);
    bool editorRunning(Handle tic) const;

    // o fonte mudou no editor → (re)compila e recomeça a run do editor
    bool editorRestart(Scene& scene, Handle tic, const char* source,
                       voni::Error& err);

    // ---- Inspector (variáveis @+ da spec §4) -------------------------------
    std::vector<voni::ExportedVar> exported(Scene& scene, Handle tic) const;
    bool setExportedVar(Scene& scene, Handle tic, const std::string& name,
                        const voni::Value& v, voni::Error& err);

    // ---- erros (main drena → engine.log/toast/editor) ----------------------
    // devolve true se havia erro novo; tic = TIC dono do script que falhou
    bool popError(Handle& tic, voni::Error& out);

    // hook da transição de cena (cena.transition.for da spec §9): o main
    // instala o real; sem hook → "transições indisponíveis aqui"
    void setTransitionHook(
        std::function<bool(const std::string& from, const std::string& to,
                           std::string& err)>
            hook) {
        transitionHook_ = std::move(hook);
    }
    void setSceneNameFn(std::function<std::string()> fn) {
        sceneNameFn_ = std::move(fn);
    }

    void tick(Scene& scene, f32 dt) override;

    // (telemetria/testes) nº de runs ativas
    u32 activeRuns() const { return static_cast<u32>(runs_.size()); }

    // (testes/main: parar TUDO — sair do Play fecha as runs de editor)
    void stopAll();
    // sair do Play: as runs DE PLAY morrem; as do editor Run continuam
    void stopPlayRuns();

    ~VoniSystem() override = default;

private:
    struct Run {
        voni::Script script;
        Handle tic{};            // TIC dono (handle vivo)
        bool editorDriven = false;   // veio do Run do editor
        bool deadLogged = false;     // erro fatal já reportado
    };

    Run* findRun(Handle tic);
    bool ensureStarted(Scene& scene, Run& r, voni::Error& err);

    std::unordered_map<u64, Run> runs_;   // chave = index<<32|generation
    std::function<bool(const std::string&, const std::string&, std::string&)>
        transitionHook_;
    std::function<std::string()> sceneNameFn_;

    // erro fatal pendente (popError drena → main: engine.log/toast/editor)
    voni::Error lastFatal_;
    Handle      lastFatalTic_{};
    bool        pendingError_ = false;
};

} // namespace vv
