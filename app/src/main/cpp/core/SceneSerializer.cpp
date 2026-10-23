#include "core/SceneSerializer.h"
#include "components/BodyComp.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/TouchControls.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "components/Control.h"   // 0.12-B: a árvore de UI (UIRoot + filhos)
#include "components/UIRoot.h"
#include "components/CameraComp.h"   // 0.7.7: câmara de cena
#include "components/AnimationPlayer.h"   // 0.8.0: animação (F7)
#include "components/AudioPlayer.h"      // 0.8.11: áudio
#include "components/Environment.h"     // 0.12-Y: WorldEnvironment
#include "components/ParticleEmitter.h"  // 0.12-Y-F1: partículas GPU
#include "components/WindTIC.h"          // 0.12-Y-F2: vento global
#include "components/AmbientEnvironment.h" // 0.12-Y-F2: clima
#include "components/SkeletonComp.h"  // 0.8.2: esqueleto (F7)
#include "core/CameraUtil.h"          // 0.7.7: uma ativa por cena
#include "core/ComponentStore.h"
#include "core/Scene.h"
#include "platform/EngineLog.h"      // 0.8.9: cura de ciclos com erro legível
#include <cstdio>
#include <cstring>
#include <variant>
#include <vector>

namespace vv {
namespace SceneSerializer {

namespace {

// comparação de Vec3 com tolerância (0.8.3: detecta bind ≠ TRS atual)
inline bool serVecNearF(const Vec3& a, const Vec3& b, f32 eps = 1e-5f) {
    const f32 dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx < eps && dx > -eps && dy < eps && dy > -eps &&
           dz < eps && dz > -eps;
}

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

// 0.9.2 — Script: o fonte .voni viaja como string JSON (escapes do Json)
void appendComponentJson(Json& arr, const ScriptComp* sc) {
    if (!sc || sc->source.empty()) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("Script"));
    c.addMember("source", Json::makeString(sc->source));
    if (!sc->autoPlay) {
        c.addMember("auto", Json::makeNumber(0));
    }
    arr.addItem(std::move(c));
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
    // 0.8.0 (F7): PRIMITIVA PROCEDURAL — "mesh":"prim" + o bloco
    // "prim" (tipo + parâmetros). Só gravado quando ATIVA (escolher
    // cube/asset limpa o prim — uma fonte de mesh de cada vez).
    if (mr->primOn) {
        c.addMember("mesh", Json::makeString("prim"));
        Json jp = Json::makeObject();
        jp.addMember("type", Json::makeString(primName(mr->prim.kind)));
        jp.addMember("r", Json::makeNumber(mr->prim.radius));
        jp.addMember("h", Json::makeNumber(mr->prim.height));
        jp.addMember("r2", Json::makeNumber(mr->prim.radius2));
        jp.addMember("seg", Json::makeNumber(static_cast<f64>(mr->prim.segments)));
        jp.addMember("rings", Json::makeNumber(static_cast<f64>(mr->prim.rings)));
        jp.addMember("size", Json::makeNumber(mr->prim.size));
        c.addMember("prim", std::move(jp));
    } else if (!mr->meshPath.empty()) {
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
    // 0.10-M HOTFIX TEXTURAS-VINCULA: os SLOTS 1-4 do material (o 0 é o
    // texPath; ausente = vazio — os ficheiros antigos abrem sem os campos)
    if (!mr->texNormal.empty()) {
        c.addMember("texNormal", Json::makeString(mr->texNormal));
    }
    if (!mr->texMetalRough.empty()) {
        c.addMember("texMetalRough", Json::makeString(mr->texMetalRough));
    }
    if (!mr->texEmissive.empty()) {
        c.addMember("texEmissive", Json::makeString(mr->texEmissive));
    }
    if (!mr->texOcclusion.empty()) {
        c.addMember("texOcclusion", Json::makeString(mr->texOcclusion));
    }
    // 0.10-M HOTFIX TEXTURAS-MULTIMAT: os SLOTS dos MATERIAIS 1..N-1 (a
    // grade achatada (N-1)*5 na ordem kBind*; VAZIOS incluídos — o
    // round-trip é byte a byte; ausente = mesh de 1 material, o de sempre)
    if (!mr->matExtraTex.empty()) {
        Json me = Json::makeArray();
        for (const std::string& s : mr->matExtraTex) {
            me.addItem(Json::makeString(s));
        }
        c.addMember("matExtraTex", std::move(me));
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
    // 0.11-T (FASE 3.5a) — LOD por distância: SÓ quando != 0 (o default
    // OFF fica fora do ficheiro — os .goni da 0.11-S ficam byte a byte;
    // ausente = 0 = OFF no loader)
    if (mr->lodDistance != 0.0f) {
        c.addMember("lodDist", Json::makeNumber(mr->lodDistance));
    }
    // 0.11-W (3.5b) — instancing: SÓ quando false (o default ON fica fora
    // do ficheiro — os .goni da 0.11-T ficam byte a byte; ausente = ON no
    // loader)
    if (!mr->instancing) {
        c.addMember("instancing", Json::makeBool(false));
    }
    // 0.11-W (3.75a) — streaming: o MESMO contrato do instancing (só o
    // false nasce; ausente = ON no loader — os .goni antigos intocados)
    if (!mr->streaming) {
        c.addMember("streaming", Json::makeBool(false));
    }
    // 0.11-W (3.75b) — impostores: SÓ quando != 0 (o MESMO contrato do
    // lodDist — ausente = 0 = OFF no loader; os .goni antigos intocados)
    if (mr->impostorDistance != 0.0f) {
        c.addMember("impostorDist", Json::makeNumber(mr->impostorDistance));
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
    if (tc->dead != 0.0f) {
        c.addMember("dead", Json::makeNumber(tc->dead));   // 0.12-B: a zona morta
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

// 0.12-B — o GERADOR de JSON de UM elemento de UI: a FONTE ÚNICA partilhada
// pelo UiCanvas (compat 0.7.0) e pelo Control (a árvore nova) — o MESMO
// vocabulário byte a byte (kind/name/text/image/x/y/w/h/color/visible/
// spacing/pad/align/parent/ah/av/act/target/param/fscale/tstyle/rot).
// Texto multilinha (Menu/Article) via '\n' nativo do JSON (string com escape).
void appendUiElementJson(Json& je, const UiElement& e) {
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
    // 0.8.6 — tipografia + rotação: ausentes = defaults (ficheiros 0.8.x
    // abrem limpos; política forward-compat de sempre)
    if (e.fontScale != 1.0f) {
        je.addMember("fscale", Json::makeNumber(e.fontScale));
    }
    if (e.textStyle != UiElement::TextStyle::Normal) {
        je.addMember("tstyle", Json::makeString(uiTextStyleName(e.textStyle)));
    }
    if (e.rot != 0.0f) {
        je.addMember("rot", Json::makeNumber(e.rot));
    }
}

// 0.7.0 — UiCanvas: elementos de UI criável (kind/nome/texto/rect/cor/
// visível/âncoras/ação on-click). COMPAT (0.12-B): DEPRECATED — o gerador
// do JSON do elemento vive em appendUiElementJson (partilhado com o
// Control); o leitor converte a árvore (migrateUiCanvasToTree).
void appendComponentJson(Json& arr, const UiCanvas* canvas) {
    if (!canvas) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("UiCanvas"));
    Json elems = Json::makeArray();
    for (const UiElement& e : canvas->elements) {
        Json je = Json::makeObject();
        appendUiElementJson(je, e);   // a fonte única (byte a byte)
        elems.addItem(std::move(je));
    }
    c.addMember("elements", std::move(elems));
    arr.addItem(std::move(c));
}

// 0.12-B — UIRoot: o marcador da raiz da árvore de UI (presença basta).
// ⚠ SÓ grava com componente REAL (o null check é OBRIGATÓRIO — sem ele cada
// TIC nascia com um UIRoot fantasma no .goni e a migração de compat morria)
void appendComponentJson(Json& arr, const UIRoot* root) {
    if (!root) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("UIRoot"));
    arr.addItem(std::move(c));
}

// 0.12-B — Control: UM elemento de UI como TIC (a base 2D + o payload). Os
// campos do elem usam O MESMO vocabulário do UiCanvas (o gerador partilhado);
// os campos da base só viajam quando NÃO-default (a política de sempre):
// stretch/ml/mt/mr/mb (esticar+margens), px/py (pivô), op (opacidade).
void appendComponentJson(Json& arr, const Control* ctl) {
    if (!ctl) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("Control"));
    c.addMember("kind", Json::makeString(controlKindName(ctl->kind())));
    appendUiElementJson(c, ctl->elem);
    if (ctl->stretch) {
        c.addMember("stretch", Json::makeBool(true));
    }
    if (ctl->marginL != 0.0f) {
        c.addMember("ml", Json::makeNumber(ctl->marginL));
    }
    if (ctl->marginT != 0.0f) {
        c.addMember("mt", Json::makeNumber(ctl->marginT));
    }
    if (ctl->marginR != 0.0f) {
        c.addMember("mr", Json::makeNumber(ctl->marginR));
    }
    if (ctl->marginB != 0.0f) {
        c.addMember("mb", Json::makeNumber(ctl->marginB));
    }
    if (ctl->pivotX != 0.5f) {
        c.addMember("px", Json::makeNumber(ctl->pivotX));
    }
    if (ctl->pivotY != 0.5f) {
        c.addMember("py", Json::makeNumber(ctl->pivotY));
    }
    if (ctl->opacity != 1.0f) {
        c.addMember("op", Json::makeNumber(ctl->opacity));
    }
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
    // 0.7.10: toggle de visibilidade do GIZMO (default true omitido —
    // ficheiros 0.7.9 abrem limpos com o frustum visível)
    if (!cam->showFrustum) {
        c.addMember("frustum", Json::makeBool(false));
    }
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
    const bool isPrim = m && m->type == Json::Type::String && m->string == "prim";
    if (isPrim) {
        // 0.8.10 — SÓ CUBO E ESFERA + MIGRAÇÃO: um .goni antigo com
        // "cilindro"/"cone"/"plano"/"triangulo"/"torus"/"capsula" carrega
        // como BOX (cube) com o callback onPrimMigrated (o main faz log +
        // toast 1× por load). NUNCA crash — a cena abre sempre.
        // O mesh NÃO é resolvido aqui: primOn+params ficam e mesh null =
        // PEDIDO PENDENTE — o main sobe no ponto seguro do frame (troca
        // determinística sem cache, um só caminho).
        PrimKind kind = PrimKind::Sphere;
        const Json* jp = comp.find("prim");
        if (jp && jp->type == Json::Type::Object) {
            if (const Json* j = jp->find("type");
                j && j->type == Json::Type::String) {
                if (primRemoved(j->string)) {
                    kind = PrimKind::Box;   // migração: prim removida → cube
                    if (ctx.onPrimMigrated) {
                        ctx.onPrimMigrated(j->string.c_str());
                    }
                } else {
                    kind = primFromName(j->string);
                }
            }
        }
        PrimParams p = primDefaults(kind);
        if (jp && jp->type == Json::Type::Object) {
            auto readF = [jp](const char* key, f32 def) -> f32 {
                const Json* j = jp->find(key);
                return (j && j->type == Json::Type::Number)
                           ? static_cast<f32>(j->number) : def;
            };
            p.radius  = readF("r", p.radius);
            p.size    = readF("size", p.size);
            if (const Json* j = jp->find("seg");
                j && j->type == Json::Type::Number) {
                p.segments = static_cast<i32>(j->number);
            }
            if (const Json* j = jp->find("rings");
                j && j->type == Json::Type::Number) {
                p.rings = static_cast<i32>(j->number);
            }
            // height/radius2: campos de prims REMOVIDAS — lidos e ignorados
            // (compatibilidade; o clamp mantém-nos sanos sem efeito)
            (void)readF("h", p.height);
            (void)readF("r2", p.radius2);
        }
        primClamp(p);
        mr->primOn = true;
        mr->prim = p;
        mr->primPending = true;   // PEDIDO — sobe no ponto seguro do frame
        mr->primNeg = false;      // load = pedido NOVO (nova tentativa)
        mr->mesh = nullptr;       // cena nova: nada para manter
        mr->material = nullptr;
        mr->primRetire = nullptr;
        mr->meshPath.clear();
    } else if (hasPath) {
        // F5-E: ref relativa → resolver do device (cache de GPU); sem
        // resolver, o TIC entra sem mesh mas mantém a ref p/ rebind
        mr->primOn = false;
        mr->primPending = false;
        mr->primRetire = nullptr;
        mr->meshPath = mp->string;
        mr->mesh = ctx.resolveMesh ? ctx.resolveMesh(mr->meshPath) : nullptr;
        mr->material = mr->mesh ? ctx.material : nullptr;
    } else {
        const bool wantsCube = m && m->type == Json::Type::String && m->string == "cube";
        mr->primOn = false;
        mr->primPending = false;
        mr->primRetire = nullptr;
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
    // 0.10-M HOTFIX TEXTURAS-VINCULA: os slots 1-4 (ausentes = vazios)
    struct SlotKey { const char* key; u32 slot; };
    for (const SlotKey& sk :
         {SlotKey{"texNormal", 1}, SlotKey{"texMetalRough", 2},
          SlotKey{"texEmissive", 3}, SlotKey{"texOcclusion", 4}}) {
        if (const Json* j = comp.find(sk.key);
            j && j->type == Json::Type::String) {
            mr->texSlotRef(sk.slot) = j->string;   // vazio limpa — honesto
        }
    }
    // 0.10-M HOTFIX TEXTURAS-MULTIMAT: a grade dos materiais extra
    // ("matExtraTex":[...]; ausente = 1 material; a largura floors para
    // múltiplo de 5 — ficheiro cortado não cria material fantasma)
    mr->matExtraTex.clear();
    if (const Json* jm = comp.find("matExtraTex");
        jm && jm->type == Json::Type::Array) {
        for (const Json& it : jm->items) {
            if (it.type == Json::Type::String) {
                mr->matExtraTex.push_back(it.string);
            }
        }
        mr->matExtraTex.resize(mr->matExtraTex.size() -
                               mr->matExtraTex.size() %
                                   MeshRenderer::kMeshTexSlots);
    }
    // 0.7.0 — cor por TIC: "tint":[r,g,b] opcional (ausente = branco)
    mr->tint[0] = mr->tint[1] = mr->tint[2] = 1.0f;
    if (const Json* jt = comp.find("tint");
        jt && jt->type == Json::Type::Array && jt->items.size() == 3) {
        for (int i = 0; i < 3; ++i) {
            mr->tint[i] = static_cast<f32>(jt->items[static_cast<size_t>(i)].number);
        }
    }
    // 0.11-T (FASE 3.5a): "lodDist" opcional (ausente = 0 = OFF — o
    // default jogável; negativo/zero não liga nunca)
    mr->lodDistance = 0.0f;
    if (const Json* jl = comp.find("lodDist");
        jl && jl->type == Json::Type::Number) {
        const f32 v = static_cast<f32>(jl->number);
        if (v > 0.0f) {
            mr->lodDistance = v;
        }
    }
    // 0.11-W (3.5b): "instancing" opcional (ausente = ON — o default;
    // false é o único valor escrito)
    mr->instancing = true;
    if (const Json* ji = comp.find("instancing");
        ji && ji->type == Json::Type::Bool) {
        mr->instancing = ji->boolean;
    }
    // 0.11-W (3.75a): "streaming" opcional (o MESMO contrato — ausente =
    // ON; false é o único valor escrito)
    mr->streaming = true;
    if (const Json* js = comp.find("streaming");
        js && js->type == Json::Type::Bool) {
        mr->streaming = js->boolean;
    }
    // 0.11-W (3.75b): "impostorDist" opcional (o MESMO contrato do
    // lodDist — ausente = 0 = OFF; negativo/zero não liga nunca)
    mr->impostorDistance = 0.0f;
    if (const Json* jimp = comp.find("impostorDist");
        jimp && jimp->type == Json::Type::Number) {
        const f32 vimp = static_cast<f32>(jimp->number);
        if (vimp > 0.0f) {
            mr->impostorDistance = vimp;
        }
    }
}

// 0.12-B — o LEITOR de UM elemento de UI SEM o "kind" (o chamador consome o
// kind primeiro — o UiCanvas ignora kinds desconhecidos; o Control mapeia
// pelo vocabulário próprio). A fonte única partilhada por fillUiCanvas e
// fillControl: os MESMOS campos do gerador appendUiElementJson.
void fillUiElementJson(UiElement& e, const Json& je) {
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
        // 0.8.9 (CRASH-PROOF): CURA de auto-parent ao CARREGAR. Ficheiros
        // gravados pelas versões 0.7.4–0.8.8 podem ter parent == o PRÓPRIO
        // nome (o seletor "colocar em" incluía o próprio) — o ciclo
        // crashava o placeAt com stack exhaustion. Ao carregar: o ciclo
        // é removido com ERRO LEGÍVEL no log (o próximo save grava são).
        // Ciclos MÚTUOS (A→B→A) não são curados aqui (custo); o guard do
        // placeAt aborta-nos sem crash — ver UiRuntime.cpp 0.8.9.
        if (!e.parent.empty() && e.parent == e.name) {
            elog::error("load: elemento '%s' era pai de SI MESMO no .goni "
                        "(ciclo da UI 0.7.4-0.8.8) — parent removido",
                        e.name.c_str());
            e.parent.clear();
        }
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
    // 0.8.6 — tipografia + rotação (ausentes = defaults; mig. amigável)
    if (const Json* j = je.find("fscale");
        j && j->type == Json::Type::Number) {
        e.fontScale = static_cast<f32>(j->number);
    }
    if (const char* ts = readStr("tstyle"); ts[0] != '\0') {
        e.textStyle = uiTextStyleFromName(ts);
    }
    if (const Json* j = je.find("rot"); j && j->type == Json::Type::Number) {
        e.rot = static_cast<f32>(j->number);
    }
}

// 0.7.0 — UiCanvas: reconstrói os elementos do .goni. Campos ausentes =
// defaults do componente (forward-compat: elementos de versões futuras com
// kinds desconhecidos são IGNORADOS — a política do serializer).
// 0.12-B: o corpo partilha fillUiElementJson com o fillControl (a árvore).
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
        fillUiElementJson(e, je);   // a fonte única (o MESMO leitor do Control)
        canvas->elements.push_back(std::move(e));
    }
}

// 0.12-B — Control: reconstrói UM elemento-TIC da árvore. O kind usa o
// vocabulário do Control (uiElementKindName + "joystick"); o payload do elem
// partilha fillUiElementJson; os campos da base são opcionais (ausentes =
// defaults — a política de compat de sempre).
void fillControl(Control* ctl, const Json& comp) {
    if (!ctl) {
        return;
    }
    if (const Json* j = comp.find("kind"); j && j->type == Json::Type::String) {
        ctl->setKind(controlKindFromName(j->string));
    }
    fillUiElementJson(ctl->elem, comp);
    if (const Json* j = comp.find("stretch"); j && j->type == Json::Type::Bool) {
        ctl->stretch = j->boolean;
    }
    auto readF = [&comp](const char* key, f32 def) -> f32 {
        const Json* j = comp.find(key);
        return (j && j->type == Json::Type::Number) ? static_cast<f32>(j->number) : def;
    };
    ctl->marginL = readF("ml", 0.0f);
    ctl->marginT = readF("mt", 0.0f);
    ctl->marginR = readF("mr", 0.0f);
    ctl->marginB = readF("mb", 0.0f);
    ctl->pivotX = readF("px", 0.5f);
    ctl->pivotY = readF("py", 0.5f);
    ctl->opacity = readF("op", 1.0f);
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
    if (const Json* j = comp.find("dead"); j && j->type == Json::Type::Number) {
        tc->dead = static_cast<f32>(j->number);   // 0.12-B: a zona morta
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
    if (const Json* j = comp.find("frustum");
        j && j->type == Json::Type::Bool) {
        cam->showFrustum = j->boolean;   // 0.7.10: toggle do gizmo
    }
}

// 0.8.11 — AudioPlayer: reconstrói os campos (defaults ausentes =
// valores do componente — forward-compat como o resto do serializer).
// 0.9.2 — Script: {"type":"Script","source":"…","auto":0|1(octal não: número)}
void fillScriptComp(ScriptComp* sc, const Json& comp) {
    if (!sc) {
        return;
    }
    const Json* src = comp.find("source");
    if (src && src->type == Json::Type::String) {
        sc->source = src->string;   // escapes resolvidos pelo parser do Json
    }
    const Json* autoJ = comp.find("auto");
    sc->autoPlay = !(autoJ && autoJ->type == Json::Type::Number &&
                     autoJ->number == 0.0);
}

void fillAudioPlayer(AudioPlayer* au, const Json& comp) {
    if (!au) {
        return;
    }
    if (const Json* j = comp.find("clip");
        j && j->type == Json::Type::String) {
        au->clipPath = j->string;
    }
    auto readFlag = [&comp](const char* key) -> bool {
        const Json* j = comp.find(key);
        return j && j->type == Json::Type::Number && j->number != 0.0;
    };
    au->autoplay = readFlag("autoplay");
    au->loop = readFlag("loop");
    auto readF = [&comp](const char* key, f32 def) -> f32 {
        const Json* j = comp.find(key);
        return (j && j->type == Json::Type::Number)
                   ? static_cast<f32>(j->number) : def;
    };
    au->volume = readF("volume", 1.0f);
    au->pitch = readF("pitch", 1.0f);
    au->posicional = readFlag("posicional");
    au->raioInterno = readF("rint", 1.0f);
    au->raioExterno = readF("rext", 8.0f);
    au->clampFields();
    // runtime reset (cena carregada nasce calada)
    au->voiceId = -1;
    au->previewing = false;
}

// 0.8.0 (F7) — AnimationPlayer: reconstrói clips/tracks/keys + playback.
// Alvos/tipos de curva desconhecidos são IGNORADOS (forward-compat, a
// política do serializer); keys ficam ORDENADAS por t (sortKeys).
void fillAnimationPlayer(AnimationPlayer* ap, const Json& comp) {
    if (!ap) {
        return;
    }
    ap->clips.clear();
    ap->playing = false;   // runtime: nasce PARADO
    ap->time = 0.0f;
    ap->speed = 1.0f;
    ap->activeClip = 0;
    ap->mode = AnimationPlayer::Mode::Loop;
    if (const Json* j = comp.find("mode");
        j && j->type == Json::Type::String) {
        if (j->string == "once")          ap->mode = AnimationPlayer::Mode::Once;
        else if (j->string == "pingpong") ap->mode = AnimationPlayer::Mode::PingPong;
        else                              ap->mode = AnimationPlayer::Mode::Loop;
    }
    if (const Json* j = comp.find("speed");
        j && j->type == Json::Type::Number) {
        ap->speed = static_cast<f32>(j->number);
        if (ap->speed < 0.05f) {
            ap->speed = 0.05f;   // defesa (speed 0 = congelado inútil)
        }
    }
    if (const Json* j = comp.find("active");
        j && j->type == Json::Type::Number) {
        ap->activeClip = static_cast<i32>(j->number);
    }
    const Json* clips = comp.find("clips");
    if (!clips || clips->type != Json::Type::Array) {
        return;
    }
    for (const Json& jc : clips->items) {
        AnimClip clip;
        if (const Json* j = jc.find("name");
            j && j->type == Json::Type::String) {
            clip.name = j->string;
        }
        if (clip.name.empty()) {
            clip.name = "edit";
        }
        const Json* tracks = jc.find("tracks");
        if (tracks && tracks->type == Json::Type::Array) {
            for (const Json& jt : tracks->items) {
                AnimTrack tr;
                const Json* jtarget = jt.find("target");
                const std::string& tg =
                    (jtarget && jtarget->type == Json::Type::String)
                        ? jtarget->string : std::string();
                if      (tg == "pos")   tr.target = AnimTarget::TicPos;
                else if (tg == "rot")   tr.target = AnimTarget::TicRot;
                else if (tg == "escala") tr.target = AnimTarget::TicScale;
                else if (tg == "uipos")  tr.target = AnimTarget::UiPos;
                else if (tg == "uicor")  tr.target = AnimTarget::UiColor;
                else if (tg == "uialpha") tr.target = AnimTarget::UiAlpha;
                else if (tg == "jpos")    tr.target = AnimTarget::JointPos;    // 0.8.2
                else if (tg == "jrot")    tr.target = AnimTarget::JointRot;    // 0.8.2
                else if (tg == "jescala") tr.target = AnimTarget::JointScale; // 0.8.2
                else {
                    continue;   // alvo desconhecido — ignora (forward-compat)
                }
                if (const Json* j = jt.find("element");
                    j && j->type == Json::Type::String) {
                    tr.element = j->string;
                }
                if (const Json* j = jt.find("curve");
                    j && j->type == Json::Type::String) {
                    // 0.11-S: as 4 curvas viajam por nome (lin/bez/step/slerp);
                    // nome desconhecido → Linear (o default honesto de sempre —
                    // ficheiros antigos sem "curve" nem entram aqui)
                    if (j->string == "bez") {
                        tr.curve = AnimCurve::Bezier;
                    } else if (j->string == "step") {
                        tr.curve = AnimCurve::Step;
                    } else if (j->string == "slerp") {
                        tr.curve = AnimCurve::Slerp;
                    }
                }
                const Json* keys = jt.find("keys");
                if (keys && keys->type == Json::Type::Array) {
                    for (const Json& jk : keys->items) {
                        AnimKey k;
                        if (const Json* j = jk.find("t");
                            j && j->type == Json::Type::Number) {
                            k.t = static_cast<f32>(j->number);
                            if (k.t < 0.0f) {
                                k.t = 0.0f;   // defesa
                            }
                        }
                        if (const Json* j = jk.find("v");
                            j && j->type == Json::Type::Array) {
                            const size_t n = j->items.size() < 4 ? j->items.size() : 4;
                            for (size_t i = 0; i < n; ++i) {
                                k.v[i] = static_cast<f32>(j->items[i].number);
                            }
                        }
                        if (const Json* j = jk.find("in");
                            j && j->type == Json::Type::Array) {
                            const size_t n = j->items.size() < 4 ? j->items.size() : 4;
                            for (size_t i = 0; i < n; ++i) {
                                k.tanIn[i] = static_cast<f32>(j->items[i].number);
                            }
                        }
                        if (const Json* j = jk.find("out");
                            j && j->type == Json::Type::Array) {
                            const size_t n = j->items.size() < 4 ? j->items.size() : 4;
                            for (size_t i = 0; i < n; ++i) {
                                k.tanOut[i] = static_cast<f32>(j->items[i].number);
                            }
                        }
                        tr.keys.push_back(k);
                    }
                }
                tr.sortKeys();
                clip.tracks.push_back(std::move(tr));
            }
        }
        ap->clips.push_back(std::move(clip));
    }
    if (ap->activeClip >= static_cast<i32>(ap->clips.size())) {
        ap->activeClip = ap->clips.empty() ? 0 : 0;   // fora do intervalo → 0
    }
}

// 0.8.2 (F7) — Skeleton: reconstrói os joints (ausentes = defaults)
void fillSkeleton(SkeletonComp* sk, const Json& comp) {
    if (!sk) {
        return;
    }
    sk->joints.clear();
    const Json* jjoints = comp.find("joints");
    if (!jjoints || jjoints->type != Json::Type::Array) {
        return;
    }
    for (const Json& jj : jjoints->items) {
        SkeletonComp::Joint j;
        if (const Json* n = jj.find("name"); n && n->type == Json::Type::String) {
            j.name = n->string;
        }
        if (const Json* p = jj.find("parent");
            p && p->type == Json::Type::Number) {
            j.parent = static_cast<i32>(p->number);
        }
        readVec3(jj.find("pos"), j.pos);
        if (const Json* q = jj.find("rot");
            q && q->type == Json::Type::Array && q->items.size() == 4) {
            j.rot = Quat{static_cast<f32>(q->items[0].number),
                         static_cast<f32>(q->items[1].number),
                         static_cast<f32>(q->items[2].number),
                         static_cast<f32>(q->items[3].number)};
        }
        readVec3(jj.find("scale"), j.scale);
        // 0.8.3: bind — ausente = o TRS carregado (bind pose de 0.8.2)
        j.bindPos = j.pos;
        j.bindRot = j.rot;
        j.bindScale = j.scale;
        readVec3(jj.find("bpos"), j.bindPos);
        if (const Json* q = jj.find("brot");
            q && q->type == Json::Type::Array && q->items.size() == 4) {
            j.bindRot = Quat{static_cast<f32>(q->items[0].number),
                             static_cast<f32>(q->items[1].number),
                             static_cast<f32>(q->items[2].number),
                             static_cast<f32>(q->items[3].number)};
        }
        readVec3(jj.find("bscale"), j.bindScale);
        if (const Json* ibm = jj.find("ibm");
            ibm && ibm->type == Json::Type::Array && ibm->items.size() == 16) {
            for (int i = 0; i < 16; ++i) {
                j.inverseBind.m[i] = static_cast<f32>(ibm->items[static_cast<size_t>(i)].number);
            }
        }
        sk->joints.push_back(std::move(j));
    }
}

} // namespace

// ---- 0.8.0 (F7): AnimationPlayer — dump (clip → tracks → keys; estrutura
// recursiva; o mesmo anon ns funde com o de cima) -------------------------
namespace {

// 0.8.2 (F7) — Skeleton: joints por valor (nome/pai/TRS/ibm). IBM identidade
// omitida (skins de bind direto gravam limpas); TRS default omitido idem.
void appendComponentJson(Json& arr, const SkeletonComp* sk) {
    if (!sk) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("Skeleton"));
    Json joints = Json::makeArray();
    for (const SkeletonComp::Joint& j : sk->joints) {
        Json jj = Json::makeObject();
        jj.addMember("name", Json::makeString(j.name));
        if (j.parent >= 0) {
            jj.addMember("parent", Json::makeNumber(static_cast<f64>(j.parent)));
        }
        jj.addMember("pos", vec3ToJson(j.pos));
        Json q = Json::makeArray();
        q.addItem(Json::makeNumber(j.rot.x));
        q.addItem(Json::makeNumber(j.rot.y));
        q.addItem(Json::makeNumber(j.rot.z));
        q.addItem(Json::makeNumber(j.rot.w));
        jj.addMember("rot", std::move(q));
        jj.addMember("scale", vec3ToJson(j.scale));
        // 0.8.3: TRS de BIND (alvo do fade do blend); gravado quando
        // DIFERE do TRS atual (bind pose = omitido — 0.8.2 abre limpo)
        const bool bindDiff = !serVecNearF(j.bindPos, j.pos) ||
                              j.bindRot.x != j.rot.x || j.bindRot.y != j.rot.y ||
                              j.bindRot.z != j.rot.z || j.bindRot.w != j.rot.w ||
                              !serVecNearF(j.bindScale, j.scale);
        if (bindDiff) {
            jj.addMember("bpos", vec3ToJson(j.bindPos));
            Json bq = Json::makeArray();
            bq.addItem(Json::makeNumber(j.bindRot.x));
            bq.addItem(Json::makeNumber(j.bindRot.y));
            bq.addItem(Json::makeNumber(j.bindRot.z));
            bq.addItem(Json::makeNumber(j.bindRot.w));
            jj.addMember("brot", std::move(bq));
            jj.addMember("bscale", vec3ToJson(j.bindScale));
        }
        // ibm ≠ identidade → gravada (a identidade é o default)
        const Mat4 id = Mat4::identity();
        bool ident = true;
        for (int i = 0; i < 16; ++i) {
            if (j.inverseBind.m[i] != id.m[i]) {
                ident = false;
            }
        }
        if (!ident) {
            Json ibm = Json::makeArray();
            for (int i = 0; i < 16; ++i) {
                ibm.addItem(Json::makeNumber(j.inverseBind.m[i]));
            }
            jj.addMember("ibm", std::move(ibm));
        }
        joints.addItem(std::move(jj));
    }
    c.addMember("joints", std::move(joints));
    arr.addItem(std::move(c));
}

// 0.8.0 (F7): AnimationPlayer — dump (clip → tracks → keys; estrutura
// recursiva; o mesmo anon ns funde com o de cima)

void appendAnimKey(Json& arr, const AnimKey& k) {
    Json jk = Json::makeObject();
    jk.addMember("t", Json::makeNumber(k.t));
    Json v = Json::makeArray();
    for (int i = 0; i < 4; ++i) {
        v.addItem(Json::makeNumber(k.v[i]));
    }
    jk.addMember("v", std::move(v));
    // tangentes só quando NÃO-zero (zero = ease suave — o default de um
    // track bez recém-criado; linear é outro enum e não as grava)
    bool inZ = true, outZ = true;
    for (int i = 0; i < 4; ++i) {
        if (k.tanIn[i] != 0.0f)  inZ = false;
        if (k.tanOut[i] != 0.0f) outZ = false;
    }
    if (!inZ) {
        Json tin = Json::makeArray();
        for (int i = 0; i < 4; ++i) {
            tin.addItem(Json::makeNumber(k.tanIn[i]));
        }
        jk.addMember("in", std::move(tin));
    }
    if (!outZ) {
        Json tout = Json::makeArray();
        for (int i = 0; i < 4; ++i) {
            tout.addItem(Json::makeNumber(k.tanOut[i]));
        }
        jk.addMember("out", std::move(tout));
    }
    arr.addItem(std::move(jk));
}

// 0.8.11 — AudioPlayer: clip/autoplay/loop/volume/pitch/posicional+raios.
// Defaults omitidos (autoplay/loop false, volume 1, pitch 1, global).
void appendComponentJson(Json& arr, const AudioPlayer* au) {
    if (!au) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("AudioPlayer"));
    if (!au->clipPath.empty()) {
        c.addMember("clip", Json::makeString(au->clipPath));
    }
    if (au->autoplay) {
        c.addMember("autoplay", Json::makeNumber(1));
    }
    if (au->loop) {
        c.addMember("loop", Json::makeNumber(1));
    }
    if (au->volume != 1.0f) {
        c.addMember("volume", Json::makeNumber(au->volume));
    }
    if (au->pitch != 1.0f) {
        c.addMember("pitch", Json::makeNumber(au->pitch));
    }
    if (au->posicional) {
        c.addMember("posicional", Json::makeNumber(1));
        c.addMember("rint", Json::makeNumber(au->raioInterno));
        c.addMember("rext", Json::makeNumber(au->raioExterno));
    }
    arr.addItem(std::move(c));
}

// 0.12-Y (3.Y-C) — Environment: sky/fog/density/ambient/hdri/active.
// Defaults omitidos (o lit de sempre: névoa morta, ambient 1, sem HDRI).
// 3.Y-D: os campos do CÉU PROCEDURAL (skyType + cores + sol + nuvens)
// entram pela MESMA lei — default omitido, o .goni antigo carrega igual.
void appendComponentJson(Json& arr, const Environment* env) {
    if (!env) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("Environment"));
    auto putCol = [&](const char* key, const f32 v[4]) {
        Json a = Json::makeArray();
        for (int i = 0; i < 3; ++i) {
            a.addItem(Json::makeNumber(v[i]));
        }
        c.addMember(key, std::move(a));
    };
    // o putCol3 (3.Y-D): o array 3 só sai quando DIVERGE do default (o
    // M-Yd2 da serialização é o if morto — o roundtrip do pin vê)
    auto putCol3 = [&](const char* key, const f32 v[3], const f32 def[3]) {
        if (v[0] != def[0] || v[1] != def[1] || v[2] != def[2]) {
            Json a = Json::makeArray();
            for (int i = 0; i < 3; ++i) {
                a.addItem(Json::makeNumber(v[i]));
            }
            c.addMember(key, std::move(a));
        }
    };
    putCol("sky", env->skyColor);
    putCol("fog", env->fogColor);
    if (env->fogDensity != 0.0f) {
        c.addMember("fogDensity", Json::makeNumber(env->fogDensity));
    }
    if (env->ambient != 1.0f) {
        c.addMember("ambient", Json::makeNumber(env->ambient));
    }
    if (!env->hdriPath.empty()) {
        c.addMember("hdri", Json::makeString(env->hdriPath));
    }
    if (!env->active) {
        c.addMember("active", Json::makeNumber(0));
    }
    // ---- 3.Y-D: o CÉU PROCEDURAL (defaults omitidos = forward-compat) ----
    if (env->skyType != Environment::kSkyTypeSolid) {
        c.addMember("skyType", Json::makeNumber(env->skyType));
    }
    putCol3("skyHor", env->skyColorHorizon, Environment::kHorizonDefault);
    putCol3("skyZen", env->skyColorZenith, Environment::kZenithDefault);
    putCol3("sunCol", env->sunColor, Environment::kSunColDefault);
    putCol3("sunDir", env->sunDirection, Environment::kSunDirDefault);
    if (env->cloudCoverage != Environment::kCoverageDefault) {
        c.addMember("cloudCov", Json::makeNumber(env->cloudCoverage));
    }
    if (env->cloudSpeed != Environment::kSpeedDefault) {
        c.addMember("cloudSpd", Json::makeNumber(env->cloudSpeed));
    }
    // ---- 0.12-Y-E: O CICLO DIA/NOITE (defaults omitidos = o meio-dia da
    // identidade — o .goni da D carrega byte a byte) -----------------------
    if (env->timeOfDay != Environment::kTodDefault) {
        c.addMember("tod", Json::makeNumber(env->timeOfDay));
    }
    if (env->dayCycleSpeed != 0.0f) {
        c.addMember("cycSpd", Json::makeNumber(env->dayCycleSpeed));
    }
    if (env->autoCycle) {
        c.addMember("cycOn", Json::makeNumber(1));
    }
    if (env->cloudHeight != Environment::kHeightDefault) {
        c.addMember("cloudH", Json::makeNumber(env->cloudHeight));
    }
    arr.addItem(std::move(c));
}

// 0.12-Y (3.Y-C): reconstrói o Environment (defaults ausentes = o
// componente — forward-compat como o resto do serializer).
void fillEnvironment(Environment* env, const Json& comp) {
    if (!env) {
        return;
    }
    auto readCol = [&comp](const char* key, f32 out[4]) {
        const Json* j = comp.find(key);
        if (j && j->type == Json::Type::Array && j->items.size() >= 3) {
            for (int i = 0; i < 3; ++i) {
                if (j->items[static_cast<size_t>(i)].type ==
                    Json::Type::Number) {
                    out[i] = static_cast<f32>(
                        j->items[static_cast<size_t>(i)].number);
                }
            }
        }
    };
    readCol("sky", env->skyColor);
    readCol("fog", env->fogColor);
    if (const Json* j = comp.find("fogDensity");
        j && j->type == Json::Type::Number) {
        env->fogDensity = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("ambient");
        j && j->type == Json::Type::Number) {
        env->ambient = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("hdri");
        j && j->type == Json::Type::String) {
        env->hdriPath = j->string;
    }
    if (const Json* j = comp.find("active");
        j && j->type == Json::Type::Number) {
        env->active = j->number != 0.0;
    }
    // ---- 3.Y-D: o CÉU PROCEDURAL (os campos ausentes = os defaults) -----
    if (const Json* j = comp.find("skyType");
        j && j->type == Json::Type::Number) {
        env->skyType = j->number != 0.0 ? Environment::kSkyTypeProcedural
                                        : Environment::kSkyTypeSolid;
    }
    auto readCol3 = [&comp](const char* key, f32 out[3]) {
        const Json* j = comp.find(key);
        if (j && j->type == Json::Type::Array && j->items.size() >= 3) {
            for (int i = 0; i < 3; ++i) {
                if (j->items[static_cast<size_t>(i)].type ==
                    Json::Type::Number) {
                    out[i] = static_cast<f32>(
                        j->items[static_cast<size_t>(i)].number);
                }
            }
        }
    };
    readCol3("skyHor", env->skyColorHorizon);
    readCol3("skyZen", env->skyColorZenith);
    readCol3("sunCol", env->sunColor);
    readCol3("sunDir", env->sunDirection);
    if (const Json* j = comp.find("cloudCov");
        j && j->type == Json::Type::Number) {
        env->cloudCoverage = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("cloudSpd");
        j && j->type == Json::Type::Number) {
        env->cloudSpeed = static_cast<f32>(j->number);
    }
    // ---- 0.12-Y-E: O CICLO DIA/NOITE (os campos ausentes = o meio-dia) ---
    if (const Json* j = comp.find("tod");
        j && j->type == Json::Type::Number) {
        env->timeOfDay = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("cycSpd");
        j && j->type == Json::Type::Number) {
        env->dayCycleSpeed = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("cycOn");
        j && j->type == Json::Type::Number) {
        env->autoCycle = j->number != 0.0;
    }
    if (const Json* j = comp.find("cloudH");
        j && j->type == Json::Type::Number) {
        env->cloudHeight = static_cast<f32>(j->number);
    }
    env->clampFields();
}

// ---- 0.12-Y-F1 — ParticleEmitter: a FÁBRICA do jorro (os defaults
// omitidos = o .goni antigo carrega byte a byte; o forward-compat da casa)
void appendComponentJson(Json& arr, const ParticleEmitter* pe) {
    if (!pe) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("ParticleEmitter"));
    if (pe->maxParticles != ParticleEmitter::kMaxParticlesDefault) {
        c.addMember("maxP", Json::makeNumber(pe->maxParticles));
    }
    if (pe->lifetime != ParticleEmitter::kLifetimeDefault) {
        c.addMember("life", Json::makeNumber(pe->lifetime));
    }
    static constexpr f32 kStartDef[4] = {1.0f, 0.72f, 0.25f, 1.0f};
    static constexpr f32 kEndDef[4]   = {0.90f, 0.15f, 0.05f, 1.0f};
    bool sDef = true, eDef = true;
    for (int i = 0; i < 4; ++i) {
        sDef = sDef && pe->startColor[i] == kStartDef[i];
        eDef = eDef && pe->endColor[i] == kEndDef[i];
    }
    if (!sDef) {
        Json col = Json::makeArray();
        for (int i = 0; i < 4; ++i) {
            col.addItem(Json::makeNumber(pe->startColor[i]));
        }
        c.addMember("colA", std::move(col));
    }
    if (!eDef) {
        Json col = Json::makeArray();
        for (int i = 0; i < 4; ++i) {
            col.addItem(Json::makeNumber(pe->endColor[i]));
        }
        c.addMember("colB", std::move(col));
    }
    if (pe->startSize != ParticleEmitter::kSizeStartDefault) {
        c.addMember("szA", Json::makeNumber(pe->startSize));
    }
    if (pe->endSize != ParticleEmitter::kSizeEndDefault) {
        c.addMember("szB", Json::makeNumber(pe->endSize));
    }
    static constexpr f32 kVelDef[3] = {0.0f, ParticleEmitter::kVelYDefault,
                                       0.0f};
    static constexpr f32 kGravDef[3] = {0.0f,
                                        ParticleEmitter::kGravYDefault,
                                        0.0f};
    bool vDef = true, gDef = true;
    for (int i = 0; i < 3; ++i) {
        vDef = vDef && pe->velocity[i] == kVelDef[i];
        gDef = gDef && pe->gravity[i] == kGravDef[i];
    }
    if (!vDef) {
        Json v = Json::makeArray();
        for (int i = 0; i < 3; ++i) {
            v.addItem(Json::makeNumber(pe->velocity[i]));
        }
        c.addMember("vel", std::move(v));
    }
    if (!gDef) {
        Json g = Json::makeArray();
        for (int i = 0; i < 3; ++i) {
            g.addItem(Json::makeNumber(pe->gravity[i]));
        }
        c.addMember("grav", std::move(g));
    }
    if (!pe->texturePath.empty()) {
        c.addMember("tex", Json::makeString(pe->texturePath));
    }
    arr.addItem(std::move(c));
}

// a reconstrução (os campos ausentes = a fábrica do jorro)
void fillParticleEmitter(ParticleEmitter* pe, const Json& comp) {
    if (const Json* j = comp.find("maxP");
        j && j->type == Json::Type::Number) {
        pe->maxParticles = static_cast<i32>(j->number);
    }
    if (const Json* j = comp.find("life");
        j && j->type == Json::Type::Number) {
        pe->lifetime = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("colA");
        j && j->type == Json::Type::Array && j->items.size() == 4) {
        for (int i = 0; i < 4; ++i) {
            pe->startColor[i] = static_cast<f32>(j->items[i].number);
        }
    }
    if (const Json* j = comp.find("colB");
        j && j->type == Json::Type::Array && j->items.size() == 4) {
        for (int i = 0; i < 4; ++i) {
            pe->endColor[i] = static_cast<f32>(j->items[i].number);
        }
    }
    if (const Json* j = comp.find("szA");
        j && j->type == Json::Type::Number) {
        pe->startSize = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("szB");
        j && j->type == Json::Type::Number) {
        pe->endSize = static_cast<f32>(j->number);
    }
    if (const Json* j = comp.find("vel");
        j && j->type == Json::Type::Array && j->items.size() == 3) {
        for (int i = 0; i < 3; ++i) {
            pe->velocity[i] = static_cast<f32>(j->items[i].number);
        }
    }
    if (const Json* j = comp.find("grav");
        j && j->type == Json::Type::Array && j->items.size() == 3) {
        for (int i = 0; i < 3; ++i) {
            pe->gravity[i] = static_cast<f32>(j->items[i].number);
        }
    }
    if (const Json* j = comp.find("tex");
        j && j->type == Json::Type::String) {
        pe->texturePath = j->string;
    }
    pe->clampFields();
}

// ---- 0.12-Y-F2 — WindTIC: a brisa da fábrica é silêncio (o default
// (1,0,0)·1.0 fica FORA do dump — o .goni antigo carrega byte a byte)
void appendComponentJson(Json& arr, const WindTIC* w) {
    if (!w) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("WindTIC"));
    static constexpr f32 kDirDef[3] = {WindTIC::kDirDefault[0],
                                       WindTIC::kDirDefault[1],
                                       WindTIC::kDirDefault[2]};
    bool dDef = true;
    for (int i = 0; i < 3; ++i) {
        dDef = dDef && w->windDirection[i] == kDirDef[i];
    }
    if (!dDef) {
        Json d = Json::makeArray();
        for (int i = 0; i < 3; ++i) {
            d.addItem(Json::makeNumber(w->windDirection[i]));
        }
        c.addMember("dir", std::move(d));
    }
    if (w->windStrength != WindTIC::kStrengthDefault) {
        c.addMember("strength", Json::makeNumber(w->windStrength));
    }
    arr.addItem(std::move(c));
}

void fillWindTIC(WindTIC* w, const Json& comp) {
    if (const Json* j = comp.find("dir");
        j && j->type == Json::Type::Array && j->items.size() == 3) {
        for (int i = 0; i < 3; ++i) {
            w->windDirection[i] = static_cast<f32>(j->items[i].number);
        }
    }
    if (const Json* j = comp.find("strength");
        j && j->type == Json::Type::Number) {
        w->windStrength = static_cast<f32>(j->number);
    }
    w->clampFields();   // a direção NORMALIZA ao carregar (o contrato)
}

// ---- 0.12-Y-F2 — AmbientEnvironment: SÓ weatherType + intensity viajam
// (o thunderFlash/thunderTimer são RUNTIME — a cena carrega SEM raio
// pendente; os defaults ficam FORA do dump)
void appendComponentJson(Json& arr, const AmbientEnvironment* ae) {
    if (!ae) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("AmbientEnvironment"));
    if (ae->weatherType != AmbientEnvironment::kWeatherNone) {
        c.addMember("weather", Json::makeNumber(ae->weatherType));
    }
    if (ae->intensity != AmbientEnvironment::kIntensityDefault) {
        c.addMember("intensity", Json::makeNumber(ae->intensity));
    }
    arr.addItem(std::move(c));
}

void fillAmbientEnvironment(AmbientEnvironment* ae, const Json& comp) {
    if (const Json* j = comp.find("weather");
        j && j->type == Json::Type::Number) {
        ae->weatherType = static_cast<u8>(j->number);
    }
    if (const Json* j = comp.find("intensity");
        j && j->type == Json::Type::Number) {
        ae->intensity = static_cast<f32>(j->number);
    }
    ae->clampFields();   // o tipo desconhecido cai no NONE (o seguro)
}

void appendComponentJson(Json& arr, const AnimationPlayer* ap) {
    if (!ap) {
        return;
    }
    Json c = Json::makeObject();
    c.addMember("type", Json::makeString("AnimationPlayer"));
    // playback: defaults omitidos (loop/1.0/clip 0)
    if (ap->mode != AnimationPlayer::Mode::Loop) {
        c.addMember("mode", Json::makeString(AnimationPlayer::modeName(ap->mode)));
    }
    if (ap->speed != 1.0f) {
        c.addMember("speed", Json::makeNumber(ap->speed));
    }
    if (ap->activeClip != 0) {
        c.addMember("active", Json::makeNumber(static_cast<f64>(ap->activeClip)));
    }
    // clips (com tracks/keys) — só quando existem; o runtime (playing/time)
    // NÃO é serializado: cena carregada nasce PARADA no t=0
    if (!ap->clips.empty()) {
        Json clips = Json::makeArray();
        for (const AnimClip& clip : ap->clips) {
            Json jc = Json::makeObject();
            jc.addMember("name", Json::makeString(clip.name.empty() ? "edit" : clip.name));
            Json tracks = Json::makeArray();
            for (const AnimTrack& tr : clip.tracks) {
                Json jt = Json::makeObject();
                jt.addMember("target", Json::makeString(animTargetName(tr.target)));
                if (!tr.element.empty()) {
                    jt.addMember("element", Json::makeString(tr.element));
                }
                if (tr.curve != AnimCurve::Linear) {
                    // 0.11-S: o default (lin) continua FORA do ficheiro —
                    // os ficheiros 0.11-M e antigos ficam byte a byte
                    jt.addMember("curve", Json::makeString(animCurveName(tr.curve)));
                }
                Json keys = Json::makeArray();
                for (const AnimKey& k : tr.keys) {
                    appendAnimKey(keys, k);
                }
                jt.addMember("keys", std::move(keys));
                tracks.addItem(std::move(jt));
            }
            jc.addMember("tracks", std::move(tracks));
            clips.addItem(std::move(jc));
        }
        c.addMember("clips", std::move(clips));
    }
    arr.addItem(std::move(c));
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
        appendComponentJson(comps, cs.animators().find(t.handle));     // 0.8.0
        appendComponentJson(comps, cs.skeletons().find(t.handle));      // 0.8.2
        appendComponentJson(comps, cs.audioPlayers().find(t.handle));   // 0.8.11
        appendComponentJson(comps, cs.environments().find(t.handle));   // 0.12-Y
        appendComponentJson(comps, cs.particleEmitters().find(t.handle));   // 0.12-Y-F1
        appendComponentJson(comps, cs.winds().find(t.handle));   // 0.12-Y-F2
        appendComponentJson(comps, cs.ambients().find(t.handle));   // 0.12-Y-F2
        // BISECT-2: env dump off
        appendComponentJson(comps, cs.scripts().find(t.handle));       // 0.9.2
        appendComponentJson(comps, cs.controls().find(t.handle));      // 0.12-B
        appendComponentJson(comps, cs.uiRoots().find(t.handle));       // 0.12-B
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

// 0.12-B — COMPAT (B-1): o .goni antigo com UiCanvas abre como UIRoot +
// filhos automáticos. Para cada TIC com UiCanvas NÃO-vazio (e sem UIRoot —
// nunca converter uma árvore): o TIC hospedeiro ganha o marcador UIRoot;
// cada elemento vira um TIC FILHO (nome = o do elemento, o padrão do
// duplicateTic) com um Control (o elem por valor — âncoras 3×3/rot/…);
// os elementos com `parent` (container por NOME da 0.7.4) ficam parentados
// ao TIC filho do container. O UiCanvas SAI do TIC (deprecated — os
// elementos novos nunca o usam; o componente vive no código SÓ para este
// leitor). A migração é DE MEMÓRIA: o .goni no disco não é reescrito (o
// próximo save já grava a árvore — o formato novo).
// O TouchControls do hospedeiro NÃO se mexe (o input da física nunca muda;
// joysticks antigos continuam a desenhar pelo caminho de sempre).
void migrateUiCanvasToTree(Scene& scene) {
    // 1) recolher os hospedeiros ANTES de criar (criar durante o
    //    forEachActive invalida a iteração — Scene::create move nada em
    //    slots_, mas o contrato da casa é iterar depois de criar)
    std::vector<Handle> hosts;
    scene.forEachActive([&](Tic& t) {
        if (t.getComponent<UIRoot>()) {
            return;   // já é árvore — nada a fazer
        }
        if (UiCanvas* c = t.getComponent<UiCanvas>();
            c && !c->elements.empty()) {
            hosts.push_back(t.handle);
        }
    });
    for (const Handle hHost : hosts) {
        Tic* host = scene.get(hHost);
        if (!host) {
            continue;
        }
        UiCanvas* c = host->getComponent<UiCanvas>();
        if (!c) {
            continue;
        }
        if (!host->addComponent<UIRoot>()) {
            continue;   // a cena recusou — mantém o canvas (nunca perde dados)
        }
        // 2) um TIC filho por elemento (ordem do array = a de sempre)
        std::vector<std::pair<std::string, Handle>> made;
        made.reserve(c->elements.size());
        const i32 parentSlot = static_cast<i32>(hHost.index);
        for (const UiElement& e : c->elements) {
            const std::string name =
                e.name.empty() ? "elemento" : e.name;
            const Handle ch = scene.create(name, parentSlot);
            Tic* ct = scene.get(ch);
            if (!ct) {
                continue;   // a cena recusou — o elemento morre com o canvas
            }
            if (Control* ctl = ct->addComponent<Control>()) {
                ctl->elem = e;                  // payload por valor (0.7.0)
                ctl->elem.name = ct->name;      // o nome TIC é a identidade
            }
            made.emplace_back(e.name, ch);
        }
        // 3) containers por NOME (0.7.4) → pai TIC (o filho segue o container)
        for (const auto& [elemName, hChild] : made) {
            Tic* ct = scene.get(hChild);
            Control* ctl = ct ? ct->getComponent<Control>() : nullptr;
            if (!ct || !ctl || ctl->elem.parent.empty()) {
                continue;
            }
            for (const auto& [otherName, hOther] : made) {
                if (otherName == ctl->elem.parent && !(hOther == hChild)) {
                    ct->parent = static_cast<i32>(hOther.index);
                    break;
                }
            }
        }
        // 4) o canvas sai (deprecated) — a árvore é a única fonte daqui
        host->removeComponent<UiCanvas>();
    }
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
                fillUiCanvas(store.get<UiCanvas>(h), jc);   // 0.7.0 (compat)
            } else if (jt2->string == "Control") {
                fillControl(store.get<Control>(h), jc);   // 0.12-B: a árvore
            } else if (jt2->string == "UIRoot") {
                // marcador — presença basta (o factory já criou)
            } else if (jt2->string == "TouchControls") {
                fillTouchControls(store.get<TouchControls>(h), jc);   // 0.7.3
            } else if (jt2->string == "Camera") {
                fillCameraComp(store.get<CameraComp>(h), jc);   // 0.7.7
            } else if (jt2->string == "AnimationPlayer") {
                fillAnimationPlayer(store.get<AnimationPlayer>(h), jc);   // 0.8.0
            } else if (jt2->string == "Skeleton") {
                fillSkeleton(store.get<SkeletonComp>(h), jc);   // 0.8.2
            } else if (jt2->string == "AudioPlayer") {
                fillAudioPlayer(store.get<AudioPlayer>(h), jc);   // 0.8.11
            } else if (jt2->string == "Environment") {
                fillEnvironment(store.get<Environment>(h), jc);   // 0.12-Y
            } else if (jt2->string == "ParticleEmitter") {
                fillParticleEmitter(store.get<ParticleEmitter>(h), jc);   // 0.12-Y-F1
            } else if (jt2->string == "WindTIC") {
                fillWindTIC(store.get<WindTIC>(h), jc);   // 0.12-Y-F2
            } else if (jt2->string == "AmbientEnvironment") {
                fillAmbientEnvironment(store.get<AmbientEnvironment>(h), jc);   // 0.12-Y-F2
            } else if (jt2->string == "Script") {
                fillScriptComp(store.get<ScriptComp>(h), jc);   // 0.9.2
            }
            // InputMap: sem dados — presença basta
        }
    }
    // 0.7.7 — invariante UMA câmara ativa por cena: a primeira (ordem do
    // manifesto) fica; as restantes saem (fallback orbit se nenhuma)
    enforceSingleActiveCamera(scene);
    // 0.12-B — COMPAT: o .goni antigo com UiCanvas abre como UIRoot + filhos
    // (a migração é DE MEMÓRIA — o ficheiro no disco não é reescrito)
    migrateUiCanvasToTree(scene);
    return true;
}

} // namespace SceneSerializer
} // namespace vv
