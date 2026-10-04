// core/VoniSystem.cpp — a "central" V.ONI (ver header). O Host da engine
// (VoniEngineHost) vive em voni/ mas é da família da cena — aqui só se
// orquestra; a ponte RTTI/props fica no voni/VoniEngineHost.cpp.
#include "core/VoniSystem.h"
#include "core/Scene.h"
#include "core/Tic.h"
#include "components/ScriptComp.h"
#include "voni/VoniEngineHost.h"

namespace vv {

namespace {

u64 keyOf(Handle h) {
    return (static_cast<u64>(h.index) << 32) | h.generation;
}

} // namespace

VoniSystem::Run* VoniSystem::findRun(Handle tic) {
    auto it = runs_.find(keyOf(tic));
    return it == runs_.end() ? nullptr : &it->second;
}

bool VoniSystem::ensureStarted(Scene& scene, Run& r, voni::Error& err) {
    // compila do fonte atual (a 1ª vez) e faz o runStart 1×
    Tic* tic = scene.get(r.tic);
    if (!tic) {
        err = voni::Error::fail(0, "TIC do script já não existe");
        return false;
    }
    ScriptComp* sc = tic->getComponent<ScriptComp>();
    if (!sc) {
        err = voni::Error::fail(0, "o TIC já não tem componente Script");
        return false;
    }
    if (!r.script.started()) {
        // (re)compila — o fonte pode ter mudado desde a última run
        r.script = voni::Script::compile(sc->source.c_str(), err);
        if (!err.ok) {
            return false;
        }
        voni::EngineHost host(scene, r.tic, &transitionHook_, &sceneNameFn_);
        if (!r.script.runStart(host, err)) {
            return false;
        }
    }
    return true;
}

bool VoniSystem::editorStart(Scene& scene, Handle tic, voni::Error& err) {
    Tic* t = scene.get(tic);
    if (!t) {
        err = voni::Error::fail(0, "TIC inválido");
        return false;
    }
    ScriptComp* sc = t->getComponent<ScriptComp>();
    if (!sc) {
        err = voni::Error::fail(0, "o TIC não tem componente Script");
        return false;
    }
    Run& r = runs_[keyOf(tic)];
    r.tic = tic;
    r.editorDriven = true;
    r.deadLogged = false;
    r.script.stop();   // runStart idempotente exige stop prévio
    if (!ensureStarted(scene, r, err)) {
        r.deadLogged = true;   // o erro já vai no return — não duplicar
        return false;
    }
    return true;
}

void VoniSystem::editorStop(Handle tic) {
    auto it = runs_.find(keyOf(tic));
    if (it != runs_.end()) {
        if (it->second.editorDriven && !playEnabled) {
            runs_.erase(it);   // sem Play a morrer = run era só do editor
        } else {
            it->second.editorDriven = false;   // o Play mantém-na viva
        }
    }
}

bool VoniSystem::editorRunning(Handle tic) const {
    auto it = runs_.find(keyOf(tic));
    return it != runs_.end() && it->second.editorDriven &&
           it->second.script.running();
}

bool VoniSystem::editorRestart(Scene& scene, Handle tic, const char* source,
                               voni::Error& err) {
    Tic* t = scene.get(tic);
    if (!t) {
        err = voni::Error::fail(0, "TIC inválido");
        return false;
    }
    // 0.9.6 (G2-7c · R-010): o editor ARRANCA scripts NOVOS — um TIC sem
    // ScriptComp (script nunca fechado/guardado) recebe o componente AQUI,
    // o MESMO precedente do closeScriptEditor ("o fonte não se perde por
    // um ciclo"). Antes: escrever + Run sem fechar = "o TIC não tem
    // componente Script" — o beco sem saída que a spec proíbe (Run no
    // modelo fresco = 0 erros).
    ScriptComp* sc = t->getComponent<ScriptComp>();
    if (!sc) {
        sc = t->addComponent<ScriptComp>();
    }
    if (!sc) {
        err = voni::Error::fail(0, "o TIC não aceita componente Script");
        return false;
    }
    sc->source = source ? source : "";
    Run& r = runs_[keyOf(tic)];
    r.tic = tic;
    r.editorDriven = true;
    r.deadLogged = false;
    r.script.stop();
    return ensureStarted(scene, r, err);
}

std::vector<voni::ExportedVar> VoniSystem::exported(Scene& scene,
                                                    Handle tic) const {
    auto it = runs_.find(keyOf(tic));
    if (it == runs_.end()) {
        // sem run não há valores (as vars @+ nascem da execução — §3/§4)
        (void)scene;
        return {};
    }
    return it->second.script.exported();
}

bool VoniSystem::setExportedVar(Scene& scene, Handle tic,
                                const std::string& name, const voni::Value& v,
                                voni::Error& err) {
    auto it = runs_.find(keyOf(tic));
    if (it == runs_.end()) {
        (void)scene;
        err = voni::Error::fail(0, "script não está a correr");
        return false;
    }
    return it->second.script.setVar(name, v, err);
}

bool VoniSystem::popError(Handle& tic, voni::Error& out) {
    if (!pendingError_) {
        return false;
    }
    pendingError_ = false;
    tic = lastFatalTic_;
    out = lastFatal_;
    return true;
}

void VoniSystem::tick(Scene& scene, f32 dt) {
    // 1) runs automáticas em Play (spec §3: "start do TIC" — runStart 1×)
    if (playEnabled) {
        scene.forEachActive([&](Tic& tic) {
            ScriptComp* sc = tic.getComponent<ScriptComp>();
            if (!sc || !sc->autoPlay || sc->source.empty()) {
                return;
            }
            Run& r = runs_[keyOf(tic.handle)];
            r.tic = tic.handle;
            voni::Error err;
            if (!ensureStarted(scene, r, err)) {
                if (!r.deadLogged) {
                    r.deadLogged = true;
                    lastFatal_ = err;
                    lastFatalTic_ = r.tic;
                    pendingError_ = true;
                }
            }
        });
    }

    // 2) um frame de allmoments por run (a ordem §3)
    for (auto it = runs_.begin(); it != runs_.end();) {
        Run& r = it->second;
        Tic* tic = scene.get(r.tic);
        if (!tic) {
            it = runs_.erase(it);   // TIC destruído — a run morre
            continue;
        }
        if (r.script.started() && r.script.running()) {
            voni::EngineHost host(scene, r.tic, &transitionHook_,
                                  &sceneNameFn_);
            voni::Error err;
            if (!r.script.runFrame(host, (f64)dt, err)) {
                if (!r.deadLogged) {
                    r.deadLogged = true;
                    lastFatal_ = err;
                    lastFatalTic_ = r.tic;
                    pendingError_ = true;
                }
            }
        }
        ++it;
    }
}

void VoniSystem::stopAll() {
    runs_.clear();
}

void VoniSystem::stopPlayRuns() {
    for (auto it = runs_.begin(); it != runs_.end();) {
        if (!it->second.editorDriven) {
            it = runs_.erase(it);   // run de Play morre (auto-restart na
        } else {                    // próxima entrada em Play)
            ++it;
        }
    }
}

} // namespace vv
