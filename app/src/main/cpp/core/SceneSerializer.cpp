#include "core/SceneSerializer.h"
#include "components/BodyComp.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/TouchControls.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "components/CameraComp.h"   // 0.7.7: câmara de cena
#include "core/CameraUtil.h"          // 0.7.7: uma ativa por cena
#include "core/ComponentStore.h"
#include "core/Scene.h"
#include <cstdio>
#include <cstring>
#include <variant>
#include <vector>

namespace vv {
namespace SceneSerializer {

namespace {

Json vec3ToJson(const Vec3& v) {
    Json a = Json::makeArray();
    a.addItem(Json::makeNumber(v.x));
    a.addItem(Json::makeNumber(v.y));
    a.addItem(Json::makeNumber(v.z));
    return a;
}

bool readVec3(const Json* j, Vec3& out) {
    if (!j || j->type != Json::Type::Array || j->items.size() != 3) {
        return false;
    }
    out = Vec3{static_cast<f32>(j->items[0].number),
               static_cast<f32>(j->items[1].number),
               static_cast<f32>(j->items[2].number)};
    return true;
}

void appendComponentJson(Json& arr, const Transform3D* tr) {
    if (!tr) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("Transform3D"));
    c.addMember("pos", vec3ToJson(tr->pos));
    Json q = Json::makeArray();   // quat: [x,y,z,w]
    q.addItem(Json::makeNumber(tr->rot.x));
    q.addItem(Json::makeNumber(tr->rot.y));
    q.addItem(Json::makeNumber(tr->rot.z));
    q.addItem(Json::makeNumber(tr->rot.w));
    c.addMember("rot", std::move(q));
    c.addMember("scale", vec3ToJson(tr->scale));
    arr.addItem(std::move(c));
}

void appendComponentJson(Json& arr, const MeshRenderer* mr) {
    if (!mr) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("MeshRenderer"));
    if (!mr->meshPath.empty()) {
        // F5: asset importado — a REF relativa é a verdade; "mesh":"file" é
        // a tag de compat (v1 antiga tinha só "cube"/"none")
        c.addMember("mesh", Json::makeString("file"));
        c.addMember("meshPath", Json::makeString(mr->meshPath));
    } else {
        c.addMember("mesh", Json::makeString(mr->mesh ? "cube" : "none"));
    }
    if (!mr->texPath.empty()) {
        c.addMember("texPath", Json::makeString(mr->texPath));
    }
    // 0.7.0 — cor por TIC: tint R/G/B só é gravado quando NÃO é o branco
    // default (ficheiros 0.6.x abrem sem o campo → branco)
    if (mr->tint[0] != 1.0f || mr->tint[1] != 1.0f || mr->tint[2] != 1.0f) {
        Json tint = Json::makeArray();
        tint.addItem(Json::makeNumber(mr->tint[0]));
        tint.addItem(Json::makeNumber(mr->tint[1]));
        tint.addItem(Json::makeNumber(mr->tint[2]));
        c.addMember("tint", std::move(tint));
    }
    arr.addItem(std::move(c));
}

void appendComponentJson(Json& arr, const InputMap* im) {
    if (!im) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("InputMap"));
    arr.addItem(std::move(c));
}

// F4: TouchControls — presença + campos EDITÁVEIS (0.7.3: pos/tamanho/
// sensibilidade/cor do joystick; só gravados quando NÃO-default — os
// .goni da 0.6.x continuam a abrir com o layout fixo de sempre)
void appendComponentJson(Json& arr, const TouchControls* tc) {
    if (!tc) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("TouchControls"));
    const bool defPos = tc->relX == 0.09375f && tc->relY == 0.7361f;
    const bool defRest = tc->size == 1.0f && tc->sens == 1.0f &&
                         tc->colR == 0.1804f && tc->colG == 0.1804f &&
                         tc->colB == 0.1804f;
    if (!defPos) {
        Json pos = Json::makeArray();
        pos.addItem(Json::makeNumber(tc->relX));
        pos.addItem(Json::makeNumber(tc->relY));
        c.addMember("pos", std::move(pos));
    }
    if (tc->size != 1.0f) {
        c.addMember("size", Json::makeNumber(tc->size));
    }
    if (tc->sens != 1.0f) {
        c.addMember("sens", Json::makeNumber(tc->sens));
    }
    if (!defRest || tc->colR != 0.1804f) {
        Json col = Json::makeArray();
        col.addItem(Json::makeNumber(tc->colR));
        col.addItem(Json::makeNumber(tc->colG));
        col.addItem(Json::makeNumber(tc->colB));
        c.addMember("color", std::move(col));
    }
    arr.addItem(std::move(c));
}

// 0.7.0 — UiCanvas: elementos de UI criável (kind/nome/texto/rect/cor/
// visível/âncoras/ação on-click). Texto multilinha (Menu/Article) via '\n'
// nativo do JSON (string com escape).
void appendComponentJson(Json& arr, const UiCanvas* canvas) {
    if (!canvas) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("UiCanvas"));
    Json elems = Json::makeArray();
    for (const UiElement& e : canvas->elements) {
        Json je = Json::makeObject();
        je.addMember("kind", Json::makeString(uiElementKindName(e.kind)));
        je.addMember("name", Json::makeString(e.name));
        if (!e.text.empty()) {
            je.addMember("text", Json::makeString(e.text));
        }
        if (!e.image.empty()) {
            je.addMember("image", Json::makeString(e.image));
        }
        je.addMember("x", Json::makeNumber(e.ox));
        je.addMember("y", Json::makeNumber(e.oy));
        je.addMember("w", Json::makeNumber(e.w));
        je.addMember("h", Json::makeNumber(e.h));
        Json col = Json::makeArray();
        for (int i = 0; i < 4; ++i) {
            col.addItem(Json::makeNumber(e.color[i]));
        }
        je.addMember("color", std::move(col));
        je.addMember("visible", Json::makeBool(e.visible));
        // 0.7.4 — layout de containers/compostos: espaçamento, resguardo,
        // alinhamento transversal e o CONTAINER pai (filhos por nome).
        // Ausentes = defaults (0.7.3-compat: spacing 0, pad 8, start, topo)
        if (e.spacing != 0.0f) {
            je.addMember("spacing", Json::makeNumber(e.spacing));
        }
        if (e.pad != 8.0f) {
            je.addMember("pad", Json::makeNumber(e.pad));
        }
        if (e.align != UiElement::Align::Start) {
            je.addMember("align", Json::makeString(uiAlignName(e.align)));
        }
        if (!e.parent.empty()) {
            je.addMember("parent", Json::makeString(e.parent));
        }
        // âncoras: "ah"/"av" = left|center|right / top|middle|bottom
        const char* ah = e.anchorH == UiElement::AnchorH::Left ? "left"
                       : e.anchorH == UiElement::AnchorH::Center ? "center" : "right";
        const char* av = e.anchorV == UiElement::AnchorV::Top ? "top"
                       : e.anchorV == UiElement::AnchorV::Middle ? "middle" : "bottom";
        je.addMember("ah", Json::makeString(ah));
        je.addMember("av", Json::makeString(av));
        if (e.action != UiElement::Action::None) {
            je.addMember("act", Json::makeString(uiActionName(e.action)));
            je.addMember("target", Json::makeString(e.target));
            // 0.7.1 — estilo da transição (Scene.Transition): fade|slide
            if (e.action == UiElement::Action::TransitionScene &&
                !e.param.empty()) {
                je.addMember("param", Json::makeString(e.param));
            }
        }
        elems.addItem(std::move(je));
    }
    c.addMember("elements", std::move(elems));
    arr.addItem(std::move(c));
}

// 0.7.7 — Camera: perspetiva da cena. Defaults omitidos (ficheiros 0.7.6
// abrem limpos); "active" gravado quando NÃO-default true? NÃO — grava-se
// SEMPRE que é true para o invariante ser visível no .goni (o loader
// enforce "uma ativa"; false/ausente = inativa).
void appendComponentJson(Json& arr, const CameraComp* cam) {
    if (!cam) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("Camera"));
    if (cam->fovY != CameraComp::kDefaultFov) {
        c.addMember("fov", Json::makeNumber(cam->fovY));
    }
    if (cam->nearZ != CameraComp::kDefaultNear) {
        c.addMember("near", Json::makeNumber(cam->nearZ));
    }
    if (cam->farZ != CameraComp::kDefaultFar) {
        c.addMember("far", Json::makeNumber(cam->farZ));
    }
    if (cam->projection == CameraComp::Projection::Orthographic) {
        c.addMember("proj", Json::makeString("ortho"));
        if (cam->orthoSize != CameraComp::kDefaultOrthoSize) {
            c.addMember("orthoSize", Json::makeNumber(cam->orthoSize));
        }
    }
    c.addMember("active", Json::makeBool(cam->active));
    arr.addItem(std::move(c));
}

// F4: BodyComp — tipo do corpo + parâmetros LOCAIS da forma. velocity e
// grounded são runtime (não persistidos: corpo novo nasce em repouso).
void appendComponentJson(Json& arr, const BodyComp* b) {
    if (!b) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("BodyComp"));
    c.addMember("body", Json::makeString(BodyComp::typeName(b->type)));
    c.addMember("shape", Json::makeString(BodyComp::shapeName(b->shape)));
    if (const phys::Sphere* sp = std::get_if<phys::Sphere>(&b->shape)) {
        c.addMember("r", Json::makeNumber(sp->r));
        c.addMember("center", vec3ToJson(sp->center));
    } else if (const phys::AABB* bx = std::get_if<phys::AABB>(&b->shape)) {
        c.addMember("min", vec3ToJson(bx->min));
        c.addMember("max", vec3ToJson(bx->max));
    } else if (const phys::OBB* ob = std::get_if<phys::OBB>(&b->shape)) {
        Json he = Json::makeArray();
        he.addItem(Json::makeNumber(ob->halfExtents.x));
        he.addItem(Json::makeNumber(ob->halfExtents.y));
        he.addItem(Json::makeNumber(ob->halfExtents.z));
        c.addMember("he", std::move(he));
        Json q = Json::makeArray();
        q.addItem(Json::makeNumber(ob->rot.x));
        q.addItem(Json::makeNumber(ob->rot.y));
        q.addItem(Json::makeNumber(ob->rot.z));
        q.addItem(Json::makeNumber(ob->rot.w));
        c.addMember("rot", std::move(q));
    } else if (const phys::Capsule* cp = std::get_if<phys::Capsule>(&b->shape)) {
        c.addMember("r", Json::makeNumber(cp->radius));
        c.addMember("hh", Json::makeNumber(cp->halfHeight));
        c.addMember("center", vec3ToJson(cp->center));
    }
    arr.addItem(std::move(c));
}

void fillBodyComp(BodyComp* b, const Json& comp) {
    if (!b) {
        return;
    }
    if (const Json* j = comp.find("body"); j && j->type == Json::Type::String) {
        if (j->string == "character") b->type = BodyType::Character;
        else if (j->string == "rigid") b->type = BodyType::Rigid;
        else b->type = BodyType::Static;
    }
    const Json* js = comp.find("shape");
    const char* shape = (js && js->type == Json::Type::String) ? js->string.c_str() : "";
    if (std::strcmp(shape, "sphere") == 0) {
        phys::Sphere sp{};
        if (const Json* j = comp.find("r"); j && j->type == Json::Type::Number) {
            sp.r = static_cast<f32>(j->number);
        }
        readVec3(comp.find("center"), sp.center);
        b->shape = sp;
    } else if (std::strcmp(shape, "aabb") == 0) {
        phys::AABB bx{};
        readVec3(comp.find("min"), bx.min);
        readVec3(comp.find("max"), bx.max);
        b->shape = bx;
    } else if (std::strcmp(shape, "obb") == 0) {
        phys::OBB ob{};
        if (const Json* j = comp.find("he");
            j && j->type == Json::Type::Array && j->items.size() == 3) {
            ob.halfExtents = Vec3{static_cast<f32>(j->items[0].number),
                                  static_cast<f32>(j->items[1].number),
                                  static_cast<f32>(j->items[2].number)};
        }
        if (const Json* j = comp.find("rot");
            j && j->type == Json::Type::Array && j->items.size() == 4) {
            ob.rot = Quat{static_cast<f32>(j->items[0].number),
                          static_cast<f32>(j->items[1].number),
                          static_cast<f32>(j->items[2].number),
                          static_cast<f32>(j->items[3].number)};
        }
        b->shape = ob;
    } else if (std::strcmp(shape, "capsule") == 0) {
        phys::Capsule cp{};
        if (const Json* j = comp.find("r"); j && j->type == Json::Type::Number) {
            cp.radius = static_cast<f32>(j->number);
        }
        if (const Json* j = comp.find("hh"); j && j->type == Json::Type::Number) {
            cp.halfHeight = static_cast<f32>(j->number);
        }
        readVec3(comp.find("center"), cp.center);
        b->shape = cp;
    }
    // shape ausente/desconhecida → default do componente (capsule)
}

void fillTransform3D(Transform3D* tr, const Json& comp) {
    if (!tr) {
        return;
    }
    readVec3(comp.find("pos"), tr->pos);
    readVec3(comp.find("scale"), tr->scale);
    if (const Json* q = comp.find("rot");
        q && q->type == Json::Type::Array && q->items.size() == 4) {
        tr->rot = Quat{static_cast<f32>(q->items[0].number),
                       static_cast<f32>(q->items[1].number),
                       static_cast<f32>(q->items[2].number),
                       static_cast<f32>(q->items[3].number)};
    }
    tr->updateWorld();
}

void fillMeshRenderer(MeshRenderer* mr, const Json& comp, const LoadCtx& ctx) {
    if (!mr) {
        return;
    }
    const Json* m = comp.find("mesh");
    const Json* mp = comp.find("meshPath");
    const bool hasPath = mp && mp->type == Json::Type::String && !mp->string.empty();
    if (hasPath) {
        // F5-E: ref relativa → resolver do device (cache de GPU); sem
        // resolver, o TIC entra sem mesh mas mantém a ref p/ rebind
        mr->meshPath = mp->string;
        mr->mesh = ctx.resolveMesh ? ctx.resolveMesh(mr->meshPath) : nullptr;
        mr->material = mr->mesh ? ctx.material : nullptr;
    } else {
        const bool wantsCube = m && m->type == Json::Type::String && m->string == "cube";
        mr->mesh = wantsCube ? ctx.cubeMesh : nullptr;
        mr->material = wantsCube ? ctx.material : nullptr;
        mr->meshPath.clear();
    }
    // F5-E: textura do material (opcional)
    const Json* tp = comp.find("texPath");
    if (tp && tp->type == Json::Type::String && !tp->string.empty()) {
        mr->texPath = tp->string;
        mr->texture = ctx.resolveTex ? ctx.resolveTex(mr->texPath) : nullptr;
    } else {
        mr->texPath.clear();
        mr->texture = nullptr;
    }
    // 0.7.0 — cor por TIC: "tint":[r,g,b] opcional (ausente = branco)
    mr->tint[0] = mr->tint[1] = mr->tint[2] = 1.0f;
    if (const Json* jt = comp.find("tint");
        jt && jt->type == Json::Type::Array && jt->items.size() == 3) {
        for (int i = 0; i < 3; ++i) {
            mr->tint[i] = static_cast<f32>(jt->items[static_cast<size_t>(i)].number);
        }
    }
}

// 0.7.0 — UiCanvas: reconstrói os elementos do .goni. Campos ausentes =
// defaults do componente (forward-compat: elementos de versões futuras com
// kinds desconhecidos são IGNORADOS — a política do serializer).
void fillUiCanvas(UiCanvas* canvas, const Json& comp) {
    if (!canvas) {
        return;
    }
    canvas->elements.clear();
    const Json* elems = comp.find("elements");
    if (!elems || elems->type != Json::Type::Array) {
        return;
    }
    for (const Json& je : elems->items) {
        UiElement e;
        if (const Json* j = je.find("kind"); j && j->type == Json::Type::String) {
            const std::string& kind = j->string;
            if      (kind == "panel")        e.kind = UiElement::Kind::Panel;
            else if (kind == "label")       e.kind = UiElement::Kind::Label;
            else if (kind == "button")      e.kind = UiElement::Kind::Button;
            else if (kind == "image")       e.kind = UiElement::Kind::Image;
            else if (kind == "menu")        e.kind = UiElement::Kind::Menu;
            else if (kind == "card")        e.kind = UiElement::Kind::Card;
            else if (kind == "article")     e.kind = UiElement::Kind::Article;
            else if (kind == "vbox")        e.kind = UiElement::Kind::VBox;
            else if (kind == "hbox")        e.kind = UiElement::Kind::HBox;
            else {
                continue;   // kind desconhecido — ignora (forward-compat)
            }
        } else {
            continue;
        }
        if (const Json* j = je.find("name"); j && j->type == Json::Type::String) {
            e.name = j->string;
        }
        if (const Json* j = je.find("text"); j && j->type == Json::Type::String) {
            e.text = j->string;
        }
        if (const Json* j = je.find("image"); j && j->type == Json::Type::String) {
            e.image = j->string;
        }
        auto readF = [&je](const char* key, f32 def) -> f32 {
            const Json* j = je.find(key);
            return (j && j->type == Json::Type::Number) ? static_cast<f32>(j->number) : def;
        };
        e.ox = readF("x", 0.0f);
        e.oy = readF("y", 0.0f);
        e.w  = readF("w", 200.0f);
        e.h  = readF("h", 80.0f);
        if (const Json* jc = je.find("color");
            jc && jc->type == Json::Type::Array && jc->items.size() == 4) {
            for (int i = 0; i < 4; ++i) {
                e.color[i] = static_cast<f32>(jc->items[static_cast<size_t>(i)].number);
            }
        }
        if (const Json* j = je.find("visible"); j && j->type == Json::Type::Bool) {
            e.visible = j->boolean;
        }
        // 0.7.4 — layout de containers/compostos (ausentes = defaults)
        if (const Json* j = je.find("spacing");
            j && j->type == Json::Type::Number) {
            e.spacing = static_cast<f32>(j->number);
        }
        if (const Json* j = je.find("pad"); j && j->type == Json::Type::Number) {
            e.pad = static_cast<f32>(j->number);
        }
        if (const Json* j = je.find("align"); j && j->type == Json::Type::String) {
            if (j->string == "center")     e.align = UiElement::Align::Center;
            else if (j->string == "end")   e.align = UiElement::Align::End;
            else                            e.align = UiElement::Align::Start;
        }
        if (const Json* j = je.find("parent"); j && j->type == Json::Type::String) {
            e.parent = j->string;
        }
        auto readStr = [&je](const char* key) -> const char* {
            const Json* j = je.find(key);
            return (j && j->type == Json::Type::String) ? j->string.c_str() : "";
        };
        const char* ah = readStr("ah");
        if (std::strcmp(ah, "center") == 0)      e.anchorH = UiElement::AnchorH::Center;
        else if (std::strcmp(ah, "right") == 0)  e.anchorH = UiElement::AnchorH::Right;
        else                                     e.anchorH = UiElement::AnchorH::Left;
        const char* av = readStr("av");
        if (std::strcmp(av, "middle") == 0)      e.anchorV = UiElement::AnchorV::Middle;
        else if (std::strcmp(av, "bottom") == 0) e.anchorV = UiElement::AnchorV::Bottom;
        else                                     e.anchorV = UiElement::AnchorV::Top;
        const char* act = readStr("act");
        if (std::strcmp(act, "show") == 0)       e.action = UiElement::Action::ShowPanel;
        else if (std::strcmp(act, "hide") == 0)  e.action = UiElement::Action::HidePanel;
        else if (std::strcmp(act, "toggle") == 0) e.action = UiElement::Action::TogglePanel;
        else if (std::strcmp(act, "scene") == 0) e.action = UiElement::Action::LoadScene;
        else if (std::strcmp(act, "spawn") == 0) e.action = UiElement::Action::Spawn;
        else if (std::strcmp(act, "trans") == 0) e.action = UiElement::Action::TransitionScene;
        else                                     e.action = UiElement::Action::None;
        if (e.action != UiElement::Action::None) {
            e.target = readStr("target");
            if (const Json* j = je.find("param");
                j && j->type == Json::Type::String) {
                e.param = j->string;   // 0.7.1: fade|slide (ausente = fade)
            }
        }
        canvas->elements.push_back(std::move(e));
    }
}

// 0.7.3 — TouchControls: campos editáveis (ausentes = defaults que
// reproduzem o layout fixo da 0.6.x)
void fillTouchControls(TouchControls* tc, const Json& comp) {
    if (!tc) {
        return;
    }
    if (const Json* jp = comp.find("pos");
        jp && jp->type == Json::Type::Array && jp->items.size() == 2) {
        tc->relX = static_cast<f32>(jp->items[0].number);
        tc->relY = static_cast<f32>(jp->items[1].number);
    }
    if (const Json* j = comp.find("size"); j && j->type == Json::Type::Number) {
        tc->size = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("sens"); j && j->type == Json::Type::Number) {
        tc->sens = static_cast<f32>(j->number);
    }
    if (const Json* jc = comp.find("color");
        jc && jc->type == Json::Type::Array && jc->items.size() == 3) {
        tc->colR = static_cast<f32>(jc->items[0].number);
        tc->colG = static_cast<f32>(jc->items[1].number);
        tc->colB = static_cast<f32>(jc->items[2].number);
    }
}

// 0.7.7 — Camera: reconstrói os parâmetros (ausentes = defaults)
void fillCameraComp(CameraComp* cam, const Json& comp) {
    if (!cam) {
        return;
    }
    if (const Json* j = comp.find("fov"); j && j->type == Json::Type::Number) {
        cam->fovY = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("near"); j && j->type == Json::Type::Number) {
        cam->nearZ = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("far"); j && j->type == Json::Type::Number) {
        cam->farZ = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("proj");
        j && j->type == Json::Type::String && j->string == "ortho") {
        cam->projection = CameraComp::Projection::Orthographic;
    }
    if (const Json* j = comp.find("orthoSize");
        j && j->type == Json::Type::Number) {
        cam->orthoSize = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("active"); j && j->type == Json::Type::Bool) {
        cam->active = j->boolean;
    }
}

} // namespace

Json migrate(Json doc) {
    // v0 (rascunho pré-F3-final): sem "version" e tics sem "active".
    // v1 (atual): version=1 explícito; active default true.
    const Json* pv = doc.find("version");
    const u32 from = (pv && pv->type == Json::Type::Number) ? static_cast<u32>(pv->number) : 0u;

    if (from < 1) {
        if (pv == nullptr) {
            doc.addMember("version", Json::makeNumber(kVersion));
        }
        if (Json* tics = doc.find("tics")) {
            for (Json& tic : tics->items) {
                if (tic.find("active") == nullptr) {
                    tic.addMember("active", Json::makeBool(true));
                }
            }
        }
    }
    // if (from < 2) { … renomeia campos / converte dados … }  — F4+
    return doc;
}

bool save(const Scene& scene, const char* path) {
    const std::string text = dump(scene);
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    const size_t n = text.size();
    const bool ok = std::fwrite(text.data(), 1, n, f) == n && std::fclose(f) == 0;
    return ok;
}

std::string dump(const Scene& scene) {
    Json root = Json::makeObject();
    root.addMember("version", Json::makeNumber(kVersion));

    Json tics = Json::makeArray();

    // mapa slot→posição no array (parent serializa a posição, não o slot)
    std::vector<i32> slotToPos(scene.capacity(), -1);
    i32 pos = 0;
    scene.forEachActive([&](const Tic& t) {
        slotToPos[t.handle.index] = pos++;
    });

    i32 id = 0;
    scene.forEachActive([&](const Tic& t) {
        Json jt = Json::makeObject();
        jt.addMember("id", Json::makeNumber(id++));
        jt.addMember("name", Json::makeString(t.name));
        jt.addMember("active", Json::makeBool(t.active));
        // 0.7.0 — visibilidade do TIC (gestão de TICs): só gravada quando
        // ESCONDIDO (ficheiros 0.6.x abrem visíveis — default true)
        if (!t.visible) {
            jt.addMember("visible", Json::makeBool(false));
        }
        const i32 parentPos =
            (t.parent >= 0 && static_cast<u32>(t.parent) < slotToPos.size())
                ? slotToPos[static_cast<u32>(t.parent)] : -1;
        jt.addMember("parent", Json::makeNumber(parentPos));

        // componentes lidos dos storages por tipo, ordem fixa (registry)
        Json comps = Json::makeArray();
        const ComponentStore& cs = scene.components();
        appendComponentJson(comps, cs.transforms().find(t.handle));
        appendComponentJson(comps, cs.meshRenderers().find(t.handle));
        appendComponentJson(comps, cs.inputMaps().find(t.handle));
        appendComponentJson(comps, cs.bodies().find(t.handle));
        appendComponentJson(comps, cs.touchControls().find(t.handle));
        appendComponentJson(comps, cs.uiCanvases().find(t.handle));   // 0.7.0
        appendComponentJson(comps, cs.cameras().find(t.handle));       // 0.7.7
        jt.addMember("components", std::move(comps));

        tics.addItem(std::move(jt));
    });

    root.addMember("tics", std::move(tics));
    return root.dump();
}

bool load(Scene& scene, const char* path, const LoadCtx& ctx) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        text.append(buf, n);
    }
    std::fclose(f);
    return loadText(scene, text, ctx);
}

bool loadText(Scene& scene, const std::string& text, const LoadCtx& ctx) {
    Json doc;
    if (!Json::parse(text.data(), text.size(), doc)) {
        return false;
    }
    doc = migrate(std::move(doc));

    const Json* tics = doc.find("tics");
    if (!tics || tics->type != Json::Type::Array) {
        return false;
    }

    scene.clear();

    // 1ª passada: cria os TICs (ordem do array = ordem de criação)
    std::vector<Handle> created;
    created.reserve(tics->items.size());
    for (const Json& jt : tics->items) {
        const Json* jname = jt.find("name");
        const char* name =
            (jname && jname->type == Json::Type::String) ? jname->string.c_str() : "tic";
        created.push_back(scene.create(name));
        if (Tic* t = scene.get(created.back())) {
            if (const Json* ja = jt.find("active"); ja && ja->type == Json::Type::Bool) {
                t->active = ja->boolean;
            }
            // 0.7.0 — visibilidade (ausente = true, o default do campo)
            if (const Json* jv = jt.find("visible"); jv && jv->type == Json::Type::Bool) {
                t->visible = jv->boolean;
            }
        }
    }

    // 2ª passada: parent (por posição no array) + componentes via registry
    for (size_t i = 0; i < tics->items.size(); ++i) {
        const Json& jt = tics->items[i];
        Handle h = created[i];
        Tic* t = scene.get(h);
        if (!t) {
            continue;   // active=false não impede create; só defesa
        }
        if (const Json* jp = jt.find("parent"); jp && jp->type == Json::Type::Number) {
            const i64 p = static_cast<i64>(jp->number);
            if (p >= 0 && p < static_cast<i64>(created.size()) && p != static_cast<i64>(i) &&
                scene.alive(created[static_cast<size_t>(p)])) {
                t->parent = static_cast<i32>(created[static_cast<size_t>(p)].index);
            }
        }
        const Json* comps = jt.find("components");
        if (!comps || comps->type != Json::Type::Array) {
            continue;
        }
        ComponentStore& store = scene.components();
        for (const Json& jc : comps->items) {
            const Json* jt2 = jc.find("type");
            if (!jt2 || jt2->type != Json::Type::String) {
                continue;
            }
            const i32 tid = store.registry().find(jt2->string.c_str());
            if (tid < 0) {
                continue;   // tipo desconhecido — ignora (forward-compat)
            }
            if (!store.registry().create(static_cast<u32>(tid), store, h)) {
                continue;
            }
            if (jt2->string == "Transform3D") {
                fillTransform3D(store.get<Transform3D>(h), jc);
            } else if (jt2->string == "MeshRenderer") {
                fillMeshRenderer(store.get<MeshRenderer>(h), jc, ctx);
            } else if (jt2->string == "BodyComp") {
                fillBodyComp(store.get<BodyComp>(h), jc);
            } else if (jt2->string == "UiCanvas") {
                fillUiCanvas(store.get<UiCanvas>(h), jc);   // 0.7.0
            } else if (jt2->string == "TouchControls") {
                fillTouchControls(store.get<TouchControls>(h), jc);   // 0.7.3
            } else if (jt2->string == "Camera") {
                fillCameraComp(store.get<CameraComp>(h), jc);   // 0.7.7
            }
            // InputMap: sem dados — presença basta
        }
    }
    // 0.7.7 — invariante UMA câmara ativa por cena: a primeira (ordem do
    // manifesto) fica; as restantes saem (fallback orbit se nenhuma)
    enforceSingleActiveCamera(scene);
    return true;
}

} // namespace SceneSerializer
} // namespace vv
