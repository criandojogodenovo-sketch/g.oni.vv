// voni/VoniEngineHost.cpp — a bridge Host↔engine (ver header). Nomes de
// propriedades CANÓNICOS da engine (animTargetName): pos/rot/escala; o
// alias `scale` aceita-se por ser universal. Erros em PT, legíveis, SEM
// linha (a Vm acrescenta a linha do comando).
#include "voni/VoniEngineHost.h"

#include "core/Scene.h"
#include "core/Tic.h"
#include "components/AnimationPlayer.h"
#include "components/MeshRenderer.h"   // 0.9.5: cor (tint) p/ o colorpars
#include "components/ScriptComp.h"
#include "components/Transform3D.h"
#include "math/Math.h"
#include "platform/EngineLog.h"
#include "render/Primitives.h"          // 0.12-U (3.9b): os tipos do spawn

#include <cstdio>
#include <cmath>

namespace voni {

using vv::Tic;
using vv::Quat;
using vv::Vec3;
using vv::Transform3D;
using vv::AnimationPlayer;
using vv::MeshRenderer;
using vv::i32;

// (graus↔radianos — helpers locais como no AnimationPlayer/GltfAnim)
inline f32 locDeg2Rad(f32 d) { return d * 0.01745329252f; }
inline f32 locRad2Deg(f32 r) { return r * 57.2957795131f; }

EngineHost::EngineHost(vv::Scene& scene, vv::Handle hostTic,
                       const vv::voni_engine::TransitionHook* transition,
                       const vv::voni_engine::SceneNameFn* sceneName)
    : scene_(scene), hostTic_(hostTic), transition_(transition),
      sceneName_(sceneName) {}

namespace {

// 0.12-U (3.9b): o nome único do spawn — a MESMA convenção do
// createTicFromPreset (base, base.001, base.002… contra a cena viva;
// limite prático 999 — o mesmo do Presets/uiEditor)
std::string uniqueSpawnName(vv::Scene& scene, const std::string& base) {
    if (!scene.find(base).valid()) {
        return base;
    }
    char buf[96];
    for (u32 i = 1; i < 1000u; ++i) {
        std::snprintf(buf, sizeof(buf), "%s.%03u", base.c_str(), i);
        if (!scene.find(buf).valid()) {
            return std::string(buf);
        }
    }
    return base + ".999";
}

} // namespace

Tic* EngineHost::hostTic() { return scene_.get(hostTic_); }

Tic* EngineHost::ticByName(const std::string& name, std::string& err) {
    // Scene::find devolve o primeiro TIC ATIVO com o nome (F1)
    const vv::Handle h = scene_.find(name);
    Tic* t = h.valid() ? scene_.get(h) : nullptr;
    if (!t) {
        err = "TIC '" + name + "' não existe (na cena ativa)";
    }
    return t;
}

// ---------------------------------------------------------------------------
// RTTI de propriedades (registo v0 — a 0.9.3 estende com funções)
// ---------------------------------------------------------------------------
bool EngineHost::isTicProp(const std::string& prop) {
    return prop == "pos" || prop == "rot" || prop == "escala" ||
           prop == "scale" || prop == "name" || prop == "visible" ||
           prop == "active" || prop == "cor";   // 0.9.5: tint do material
}

bool EngineHost::propGet(Tic& tic, const std::string& prop, Value& out,
                         std::string& err) {
    if (prop == "pos" || prop == "rot" || prop == "escala" ||
        prop == "scale") {
        Transform3D* tr = tic.getComponent<Transform3D>();
        if (!tr) {
            err = "TIC '" + tic.name + "' não tem Transform3D";
            return false;
        }
        if (prop == "pos") {
            out = Value::ofVec3(tr->pos.x, tr->pos.y, tr->pos.z);
            return true;
        }
        if (prop == "escala" || prop == "scale") {
            out = Value::ofVec3(tr->scale.x, tr->scale.y, tr->scale.z);
            return true;
        }
        // rot: GRAUS Euler XYZ (a convenção do Inspector/engine)
        f32 px = 0, py = 0, pz = 0;
        Quat::toEuler(tr->rot, px, py, pz);
        out = Value::ofVec3(locRad2Deg(px), locRad2Deg(py), locRad2Deg(pz));
        return true;
    }
    if (prop == "name") {
        out = Value::ofTxt(tic.name);
        return true;
    }
    if (prop == "visible") {
        out = Value::ofBool(tic.visible);
        return true;
    }
    if (prop == "active") {
        out = Value::ofBool(tic.active);
        return true;
    }
    // 0.9.5 · LINKERS & TYKERS: cor do material (Vec3 0..1) — o colorpars
    // escreve aqui (o 'cor' do tyker tinge o TIC de origem)
    if (prop == "cor") {
        MeshRenderer* mr = tic.getComponent<MeshRenderer>();
        if (!mr) {
            err = "TIC '" + tic.name + "' não tem material (cor)";
            return false;
        }
        out = Value::ofVec3(mr->tint[0], mr->tint[1], mr->tint[2]);
        return true;
    }
    err = "propriedade '" + prop +
          "' não existe (propriedades: pos rot escala cor name visible "
          "active)";
    return false;
}

bool EngineHost::propSet(Tic& tic, const std::string& prop, const Value& v,
                         std::string& err) {
    if (prop == "pos" || prop == "escala" || prop == "scale" ||
        prop == "rot") {
        Transform3D* tr = tic.getComponent<Transform3D>();
        if (!tr) {
            err = "TIC '" + tic.name + "' não tem Transform3D";
            return false;
        }
        if (v.t != Type::Vec3) {
            err = "'" + prop + "' é Vec3 (veio " + typeName(v.t) + ")";
            return false;
        }
        if (prop == "pos") {
            tr->pos = vv::Vec3{v.v3[0], v.v3[1], v.v3[2]};
        } else if (prop == "escala" || prop == "scale") {
            tr->scale = vv::Vec3{v.v3[0], v.v3[1], v.v3[2]};
        } else {
            // graus → quat (convenção do engine)
            tr->rot = Quat::fromEuler(locDeg2Rad(v.v3[0]),
                                      locDeg2Rad(v.v3[1]),
                                      locDeg2Rad(v.v3[2]));
        }
        tr->updateWorld();
        return true;
    }
    if (prop == "name") {
        if (v.t != Type::Txt) {
            err = "'name' é Txt";
            return false;
        }
        tic.name = v.s;
        return true;
    }
    if (prop == "visible") {
        if (v.t != Type::Bool) {
            err = "'visible' é Bool (true/false)";
            return false;
        }
        tic.visible = v.b;
        return true;
    }
    if (prop == "active") {
        if (v.t != Type::Bool) {
            err = "'active' é Bool (true/false)";
            return false;
        }
        tic.active = v.b;
        return true;
    }
    // 0.9.5 · LINKERS & TYKERS: escrita da cor (o colorpars do tyker)
    if (prop == "cor") {
        if (v.t != Type::Vec3) {
            err = "'cor' é Vec3 (veio " + typeName(v.t) + ")";
            return false;
        }
        MeshRenderer* mr = tic.getComponent<MeshRenderer>();
        if (!mr) {
            err = "TIC '" + tic.name + "' não tem material (cor)";
            return false;
        }
        mr->tint[0] = v.v3[0];
        mr->tint[1] = v.v3[1];
        mr->tint[2] = v.v3[2];
        return true;
    }
    err = "propriedade '" + prop + "' não existe ou é só de leitura";
    return false;
}

// ---------------------------------------------------------------------------
// Host: propriedades por cadeia (TIC + prop + componentes .x/.y/.z)
// ---------------------------------------------------------------------------
bool EngineHost::getProp(const std::string& ticName,
                         const std::vector<std::string>& chain, Value& out,
                         std::string& err) {
    Tic* t = ticByName(ticName, err);
    if (!t) {
        return false;
    }
    if (chain.empty()) {
        out = Value::ofTic(t->name);
        return true;
    }
    if (!propGet(*t, chain[0], out, err)) {
        return false;
    }
    // componentes .x/.y/.z sobre o VALOR (jogador.pos.x)
    for (size_t i = 1; i < chain.size(); ++i) {
        const std::string& seg = chain[i];
        u32 idx = 9;
        if (seg == "x") idx = 0;
        else if (seg == "y") idx = 1;
        else if (seg == "z") idx = 2;
        f32 c = 0;
        if (idx != 9 && out.vecComponent(idx, c)) {
            out = Value::ofNum((f64)c);
            continue;
        }
        if (out.t == Type::Tic) {
            // cadeia através de um valor TIC (alvo.pos com alvo:TIC)
            if (!propGet(*ticByName(out.tic, err), seg, out, err)) {
                return false;
            }
            continue;
        }
        err = "'" + seg + "' não existe em '" + ticName + "." + chain[0] +
              "' (o valor é " + typeName(out.t) + ")";
        return false;
    }
    return true;
}

bool EngineHost::setProp(const std::string& ticName,
                         const std::vector<std::string>& chain, const Value& v,
                         std::string& err) {
    Tic* t = ticByName(ticName, err);
    if (!t) {
        return false;
    }
    if (chain.empty()) {
        err = "nada para escrever em '" + ticName + "'";
        return false;
    }
    // jogador.pos.x=5 → escreve o Vec3 inteiro com o componente trocado
    if (chain.size() > 1) {
        // ler o valor atual (pos → Vec3), trocar o componente, escrever
        Value cur;
        if (!propGet(*t, chain[0], cur, err)) {
            return false;
        }
        if (cur.t != Type::Vec3 && cur.t != Type::Vec2) {
            err = "'" + chain[0] + "' não tem componentes";
            return false;
        }
        Value nv = cur;
        for (size_t i = 1; i < chain.size(); ++i) {
            const std::string& seg = chain[i];
            u32 idx = 9;
            if (seg == "x") idx = 0;
            else if (seg == "y") idx = 1;
            else if (seg == "z") idx = 2;
            // componente recebe Vec do mesmo tipo OU escalar Int/Num
            // (jogador.pos.x=5 — o 5 é Int, não Vec3)
            f32 c = 0;
            bool okC = false;
            if (v.t == Type::Num) {
                c = (f32)v.n;
                okC = true;
            } else if (v.t == Type::Int) {
                c = (f32)v.i;
                okC = true;
            } else {
                okC = v.vecComponent(idx, c);
            }
            if (idx != 9 && okC && nv.setVecComponent(idx, c)) {
                continue;
            }
            err = "componente '" + seg + "' inválida";
            return false;
        }
        return propSet(*t, chain[0], nv, err);
    }
    return propSet(*t, chain[0], v, err);
}

bool EngineHost::ticExists(const std::string& ticName) {
    const vv::Handle h = scene_.find(ticName);
    return h.valid() && scene_.get(h) != nullptr;
}

// ---------------------------------------------------------------------------
// comandos do TIC anfitrião
// ---------------------------------------------------------------------------
void EngineHost::moveTic(f32 dx, f32 dy, f32 dz) {
    Tic* t = hostTic();
    if (!t) {
        return;
    }
    if (Transform3D* tr = t->getComponent<Transform3D>()) {
        tr->pos.x += dx;
        tr->pos.y += dy;
        tr->pos.z += dz;
        tr->updateWorld();
    }
}

void EngineHost::explodeTic(bool hide) {
    Tic* t = hostTic();
    if (!t) {
        return;
    }
    // et=desaparecer / er=aparecer — VISIBILIDADE (0.7.0: invisível ≠
    // inativo; a lógica/scripts continuam — o .er pode voltar a mostrar)
    t->visible = !hide;
}

bool EngineHost::importAnim(const std::string& name, std::string& err) {
    Tic* t = hostTic();
    if (!t) {
        err = "TIC anfitrião não existe";
        return false;
    }
    AnimationPlayer* ap = t->getComponent<AnimationPlayer>();
    if (!ap) {
        err = "Import.Animation: o TIC '" + t->name +
              "' não tem AnimationPlayer";
        return false;
    }
    // procura o clip pelo NOME (0.8.0: lista nomeada; clips[0] é o "edit")
    for (size_t i = 0; i < ap->clips.size(); ++i) {
        if (ap->clips[i].name == name) {
            ap->activeClip = static_cast<i32>(i);
            ap->time = 0.0f;      // nasce a tocar do início
            ap->playing = true;   // avança no Play (AnimationSystem)
            return true;
        }
    }
    err = "Import.Animation: animação '" + name + "' não encontrada";
    return false;
}

// 0.11-S (FASE 3): stop() — o par do play()/Import.Animation: PARA a
// animação do anfitrião (playing=false + tempo a 0 + blend cancelado — o
// próximo play() nasce do início; o crossfade a meio morre com o stop,
// senão a pose continuava a migrar depois de "parado")
bool EngineHost::stopAnim(std::string& err) {
    Tic* t = hostTic();
    if (!t) {
        err = "TIC anfitrião não existe";
        return false;
    }
    AnimationPlayer* ap = t->getComponent<AnimationPlayer>();
    if (!ap) {
        err = "stop(): o TIC '" + t->name + "' não tem AnimationPlayer";
        return false;
    }
    ap->playing = false;
    ap->time = 0.0f;
    ap->stopBlend();
    return true;
}

// 0.11-S (FASE 3): blend("nome", seg) — o CROSSFADE do player (o peso
// anima 0→1 em seg segundos; ao completar o clip de destino é o ATIVO com
// o tempo contínuo — a pose não salta). seg ≤ 0 → 0.3 s (o default do
// crossfade da 0.8.3).
bool EngineHost::blendAnim(const std::string& clip, f32 seconds,
                           std::string& err) {
    Tic* t = hostTic();
    if (!t) {
        err = "TIC anfitrião não existe";
        return false;
    }
    AnimationPlayer* ap = t->getComponent<AnimationPlayer>();
    if (!ap) {
        err = "blend(): o TIC '" + t->name + "' não tem AnimationPlayer";
        return false;
    }
    for (size_t i = 0; i < ap->clips.size(); ++i) {
        if (ap->clips[i].name == clip) {
            if (static_cast<i32>(i) == ap->activeClip) {
                err = "blend(): o clip '" + clip + "' já é o ativo";
                return false;
            }
            ap->crossfade(static_cast<i32>(i),
                          seconds > 0.0f ? seconds : 0.3f);
            return true;
        }
    }
    err = "blend(): animação '" + clip + "' não encontrada";
    return false;
}

// 0.12-U (3.9b): spawn(tipo, x, y, z) → TIC — CRIA na cena REAL: o tipo
// é da lista VIVA (0.8.10: esfera e box — o primFromName do render é
// defesa de LOAD; aqui o script tem de ser HONESTO e o erro ENSINA os
// tipos); nasce ACTIVE (Scene::create) + VISIBLE (o default do Tic) com
// Transform3D na pos + MeshRenderer com a primitiva (o mesmo corpo do
// preset Mesh do core/Presets.cpp) e NOME ÚNICO (a convenção base.001)
bool EngineHost::spawnTic(const std::string& tipo, f32 x, f32 y, f32 z,
                          std::string& outName, std::string& err) {
    vv::PrimKind kind;
    if (tipo == "esfera") {
        kind = vv::PrimKind::Sphere;
    } else if (tipo == "box") {
        kind = vv::PrimKind::Box;
    } else {
        err = "spawn: o tipo '" + tipo +
              "' não existe (tipos: esfera box)";
        return false;
    }
    const vv::Handle h = scene_.create(uniqueSpawnName(scene_, tipo));
    vv::Tic* t = scene_.get(h);
    if (!t) {
        err = "spawn: a cena recusou o TIC novo";
        return false;
    }
    if (Transform3D* tr = t->addComponent<Transform3D>()) {
        tr->pos = Vec3{x, y, z};
        tr->updateWorld();
    }
    if (MeshRenderer* mr = t->addComponent<MeshRenderer>()) {
        mr->primOn = true;
        mr->prim = vv::primDefaults(kind);
    }
    outName = t->name;
    return true;
}

bool EngineHost::transitionTo(const std::string& fromName,
                              const std::string& toName, std::string& err) {
    // a cena nomeada tem de ser a ATUAL (decisão 🔶: nomedacena = origem)
    const std::string cur = currentSceneName();
    if (fromName != cur) {
        err = "'" + fromName + ".transition.for': '" + fromName +
              "' não é a cena atual (" + (cur.empty() ? "?" : cur) + ")";
        return false;
    }
    if (!transition_ || !*transition_) {
        err = "transições indisponíveis aqui";
        return false;
    }
    return (*transition_)(fromName, toName, err);
}

std::string EngineHost::currentSceneName() {
    return sceneName_ && *sceneName_ ? (*sceneName_)() : std::string();
}

bool EngineHost::search(const std::string& target,
                        const std::vector<std::string>& chain, Value& out,
                        std::string& err) {
    // v0: Search.alvo.propriedade == leitura RTTI (a forma "função" —
    // Search.alvo.função — resolve no mesmo registo; funções RTTI chegam
    // com a 0.9.3, pelo que um nome desconhecido dá erro que menciona
    // propriedades E funções)
    if (!getProp(target, chain, out, err)) {
        err = "Search: " + err + " (propriedades registadas: pos rot " +
              "escala name visible active — funções RTTI chegam na 0.9.3)";
        return false;
    }
    return true;
}

void EngineHost::log(const char* line) {
    // o core já acrescentou o prefixo "voni: " (spec §9 🔶)
    vv::elog::info("%s", line);
}

f64 EngineHost::frameDt() { return dt_; }

} // namespace voni
