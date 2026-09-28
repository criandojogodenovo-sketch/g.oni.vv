#include "core/SceneSerializer.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/ComponentStore.h"
#include "core/Scene.h"
#include <cstdio>
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
    // F3: único mesh do engine — tag fixa; F5 (assets) estende para nomes.
    c.addMember("mesh", Json::makeString(mr->mesh ? "cube" : "none"));
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
    const bool wantsCube = m && m->type == Json::Type::String && m->string == "cube";
    mr->mesh = wantsCube ? ctx.cubeMesh : nullptr;
    mr->material = wantsCube ? ctx.material : nullptr;
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
        jt.addMember("components", std::move(comps));

        tics.addItem(std::move(jt));
    });

    root.addMember("tics", std::move(tics));

    const std::string text = root.dump();
    FILE* f = std::fopen(path, "wb");
    if (!f) {
        return false;
    }
    const size_t n = text.size();
    const bool ok = std::fwrite(text.data(), 1, n, f) == n && std::fclose(f) == 0;
    return ok;
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
        if (const Json* ja = jt.find("active"); ja && ja->type == Json::Type::Bool) {
            if (Tic* t = scene.get(created.back())) {
                t->active = ja->boolean;
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
            }
            // InputMap: sem dados na F3 — presença basta
        }
    }
    return true;
}

} // namespace SceneSerializer
} // namespace vv
