// tests/test_components.cpp — F3: ComponentStorage SoA, ComponentRegistry,
// ComponentStore (add/get/remove + attach/detach) e integração com Scene.
#include "TestFramework.h"
#include <cstring>
#include "core/Component.h"
#include "core/ComponentRegistry.h"
#include "core/ComponentStorage.h"
#include "core/ComponentStore.h"
#include "core/Scene.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"

using namespace vv;
using ::test::nearEqF;

namespace {

// Componente de teste para contar hooks (o storage é template — aceita
// qualquer tipo derivado de Component, mesmo fora do ComponentStore).
struct CountingComp : Component {
    static int attached;
    static int detached;
    void attach() override { ++attached; }
    void detach() override { ++detached; }
};
int CountingComp::attached = 0;
int CountingComp::detached = 0;

} // namespace

TEST(storage_add_find_remove_soa) {
    Scene s;
    ComponentStorage<CountingComp> st;
    const Handle a = s.create("a");
    const Handle b = s.create("b");
    const Handle c = s.create("c");

    CountingComp::attached = 0;
    CountingComp::detached = 0;

    EXPECT(st.add(a, s.get(a)) != nullptr);
    EXPECT(st.add(b, s.get(b)) != nullptr);
    EXPECT(st.add(c, s.get(c)) != nullptr);
    EXPECT(CountingComp::attached == 3);
    EXPECT(st.size() == 3u);
    EXPECT(st.owner(0) == a && st.owner(1) == b && st.owner(2) == c);

    EXPECT(st.find(b) != nullptr);
    st.find(b)->owner = s.get(b);   // owner coerente
    EXPECT(st.find(b)->owner == s.get(b));
    EXPECT(!st.find(Handle::invalid()));

    // remove do meio → swap-remove: dono 'c' toma o índice de 'b'
    EXPECT(st.remove(b));
    EXPECT(st.size() == 2u);
    EXPECT(st.find(b) == nullptr);
    EXPECT(st.owner(1) == c);
    EXPECT(CountingComp::detached == 1);
}

TEST(storage_recusa_duplicado_e_handle_invalido) {
    Scene s;
    ComponentStorage<Transform3D> st;
    const Handle a = s.create("a");
    EXPECT(st.add(a, s.get(a)) != nullptr);
    EXPECT(st.add(a, s.get(a)) == nullptr);   // um por tipo por TIC
    EXPECT(st.size() == 1u);
    EXPECT(!st.remove(a.index == 0 ? Handle{99u, 1u} : Handle{99u, 1u}));
    EXPECT(!st.remove(Handle::invalid()));
    EXPECT(st.remove(a));                     // limpa
    EXPECT(st.size() == 0u);
}

TEST(registry_registo_find_factory) {
    Scene s;
    ComponentStore store(&s);
    const Handle h = s.create("tic");

    // ordem de registo fixa (contrato do ctor)
    EXPECT(store.registry().count() == 3u);
    EXPECT(store.registry().find("Transform3D") == 0);
    EXPECT(store.registry().find("MeshRenderer") == 1);
    EXPECT(store.registry().find("InputMap") == 2);
    EXPECT(store.registry().find("Desconhecido") == -1);

    // factory por id e por nome
    const i32 id = store.registry().find("Transform3D");
    EXPECT(store.registry().create(static_cast<u32>(id), store, h));
    EXPECT(store.get<Transform3D>(h) != nullptr);
    EXPECT(!store.registry().create(static_cast<u32>(id), store, h));   // duplicado
    EXPECT(!store.registry().create(999u, store, h));                   // id fora

    EXPECT(store.registry().create("InputMap", store, h));
    EXPECT(store.get<InputMap>(h) != nullptr);

    const ComponentTypeRecord* rec = store.registry().at(1);
    EXPECT(rec != nullptr);
    EXPECT(rec->name != nullptr && std::strcmp(rec->name, "MeshRenderer") == 0);
    EXPECT(store.registry().at(42u) == nullptr);
}

TEST(store_add_get_remove_ciclo_completo) {
    Scene s;
    ComponentStore store(&s);
    const Handle h = s.create("cubo");

    Transform3D init;
    init.pos = Vec3{1.0f, 2.0f, 3.0f};
    init.rot = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.5f);
    init.scale = Vec3{2.0f, 2.0f, 2.0f};
    init.updateWorld();

    Transform3D* tr = store.add<Transform3D>(h, init);
    EXPECT(tr != nullptr);
    EXPECT(tr->owner == s.get(h));                 // owner aponta para o Tic
    EXPECT(tr->pos.x == 1.0f && tr->pos.y == 2.0f && tr->pos.z == 3.0f);

    MeshRenderer* mr = store.add<MeshRenderer>(h);
    EXPECT(mr != nullptr && mr->mesh == nullptr && mr->material == nullptr);

    // get<const> via overload const
    const Scene& cs = s;
    const ComponentStore& cstore = store;
    EXPECT(cstore.get<Transform3D>(h) != nullptr);

    // remove: detach + swap; getAll vazio
    EXPECT(store.remove<Transform3D>(h));
    EXPECT(store.get<Transform3D>(h) == nullptr);
    EXPECT(store.remove<Transform3D>(h) == false);   // segunda vez falha

    store.removeAll(h);
    EXPECT(store.get<MeshRenderer>(h) == nullptr);
    EXPECT(!store.hasAny(h));
}

TEST(store_sem_dono_recusa_add) {
    ComponentStore store(nullptr);   // situação só de teste — fora da Scene
    EXPECT(!store.hasAny(Handle::invalid()));
    // add sem Scene não pode crashar: devolve nullptr
    EXPECT(store.registry().count() == 3u);   // registry vive mesmo sem dono
}

TEST(transform3d_trs_matrix) {
    Transform3D tr;
    tr.pos = Vec3{1.0f, 2.0f, 3.0f};
    tr.rot = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 1.5707963f);   // +90° yaw
    tr.scale = Vec3{2.0f, 1.0f, 1.0f};
    tr.updateWorld();

    // local (1,0,0) → scale (2,0,0) → rotY90 (0,0,-2) → translação (1,2,1)
    f32 out[4];
    Mat4::transformPoint4(tr.world, Vec3{1.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEqF(out[0], 1.0f));
    EXPECT(nearEqF(out[1], 2.0f));
    EXPECT(nearEqF(out[2], 1.0f));

    // identidade: pos=(0,0.5,0) default dos presets, rot e scale neutros
    Transform3D base;
    base.pos = Vec3{0.0f, 0.5f, 0.0f};
    base.updateWorld();
    f32 out2[4];
    Mat4::transformPoint4(base.world, Vec3{0.5f, -0.5f, 0.0f}, out2);
    EXPECT(nearEqF(out2[0], 0.5f));
    EXPECT(nearEqF(out2[1], 0.0f));
    EXPECT(nearEqF(out2[2], 0.0f));
}
