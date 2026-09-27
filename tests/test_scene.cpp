// tests/test_scene.cpp — Handle/free-list/generation + Scene create/destroy/find.
#include "TestFramework.h"
#include "core/Handle.h"
#include "core/Scene.h"

using vv::Handle;
using vv::Scene;

TEST(handle_invalido_por_definicao) {
    const Handle h = Handle::invalid();
    EXPECT(!h.valid());
    EXPECT(h.index == Handle::kInvalidIndex);
    EXPECT(h.generation == 0u);
}

TEST(scene_create_handles_distintos) {
    Scene s;
    const Handle a = s.create("a");
    const Handle b = s.create("b");
    EXPECT(a.valid() && b.valid());
    EXPECT(a != b);
    EXPECT(s.count() == 2u);
    EXPECT(s.alive(a) && s.alive(b));
}

TEST(scene_find_por_nome) {
    Scene s;
    const Handle a = s.create("root");
    s.create("child");
    EXPECT(s.find("root") == a);
    EXPECT(s.find("child").valid());
    EXPECT(!s.find("nao-existe").valid());
}

TEST(scene_destroy_e_find) {
    Scene s;
    const Handle a = s.create("a");
    s.create("b");
    EXPECT(s.destroy(a));
    EXPECT(!s.alive(a));
    EXPECT(!s.find("a").valid());
    EXPECT(s.count() == 1u);
    EXPECT(!s.destroy(a));   // destruir duas vezes é no-op seguro
}

TEST(destroy_bumpa_generation_reuso_seguro) {
    Scene s;
    const Handle a = s.create("a");
    EXPECT(s.destroy(a));

    const Handle b = s.create("b");   // reusa o slot de "a"
    EXPECT(b.index == a.index);                  // mesmo slot…
    EXPECT(b.generation == a.generation + 1u);   // …generation avançada
    EXPECT(!s.alive(a));              // handle antigo: inválido, sem reuso silencioso
    EXPECT(s.alive(b));
    EXPECT(!s.destroy(a));            // destroy com handle obsoleto falha
}

TEST(free_list_reaproveita_indices) {
    Scene s;
    const Handle x = s.create("x");
    const Handle y = s.create("y");
    const Handle z = s.create("z");
    EXPECT(s.capacity() == 3u);
    EXPECT(s.destroy(y));
    const Handle w = s.create("w");
    EXPECT(w.index == y.index);                  // free-list devolveu o índice do meio
    EXPECT(w.generation == y.generation + 1u);   // generation bumped
    EXPECT(s.alive(x) && s.alive(z) && s.alive(w));
    EXPECT(s.count() == 3u);
    EXPECT(!s.alive(y));
}

TEST(parent_registrado) {
    Scene s;
    const Handle p = s.create("pai");
    const Handle f = s.create("filho", static_cast<vv::i32>(p.index));
    EXPECT(s.get(f) != nullptr);
    EXPECT(s.get(f)->parent == static_cast<vv::i32>(p.index));
    EXPECT(s.get(p)->parent == -1);
}

TEST(get_com_handle_obsoleto_retorna_nullptr) {
    Scene s;
    const Handle a = s.create("a");
    EXPECT(s.get(a) != nullptr);
    s.destroy(a);
    EXPECT(s.get(a) == nullptr);
}

TEST(destroy_handle_invalido_ou_estranho) {
    Scene s;
    EXPECT(!s.destroy(Handle::invalid()));
    EXPECT(!s.destroy(Handle{999u, 1u}));    // fora da capacidade
    const Handle a = s.create("a");
    EXPECT(!s.destroy(Handle{a.index, 7u})); // generation errada
    EXPECT(s.alive(a));
}
