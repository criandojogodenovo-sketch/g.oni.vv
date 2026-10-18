// tests/test_storage.cpp — F5-A: FsStorage (POSIX) contra um diretório real
// em /tmp — a mesma implementação que o device usa com a raiz
// getExternalFilesDir(). Cobre: mkdir -p, write/read text+bytes, listDir
// (só ficheiros, ordenado), guarda de traversal e o e2e de REABRIR o projeto
// (manifesto + cena ativa) do disco.
#include "TestFramework.h"
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/FsStorage.h"
#include "core/Presets.h"
#include "core/Project.h"
#include "core/Scene.h"

using namespace vv;
using ::test::nearEqF;

namespace {
const char* kRoot = "/tmp/vv_fs_storage_test";

void wipeRoot() {
    // limpa o diretório de teste entre casos (só existe dentro de /tmp/vv_…)
    std::string cmd = std::string("rm -rf ") + kRoot;
    std::system(cmd.c_str());
}
} // namespace

TEST(fs_cria_dirs_e_ficheiros_roundtrip) {
    wipeRoot();
    FsStorage st(kRoot);
    EXPECT(st.root() == kRoot);
    EXPECT(!st.exists("scenes/main.goni"));
    EXPECT(st.makeDirs("scenes"));
    EXPECT(st.makeDirs("scenes"));            // idempotente
    EXPECT(st.makeDirs("meshes"));
    EXPECT(st.makeDirs("textures"));
    EXPECT(st.writeText("scenes/main.goni", "{\"version\":1,\"tics\":[]}"));
    std::string back;
    EXPECT(st.readText("scenes/main.goni", back));
    EXPECT(back == "{\"version\":1,\"tics\":[]}");
    EXPECT(st.exists("scenes/main.goni"));
    EXPECT(!st.exists("scenes/nao_existe.goni"));
    // writeText cria o diretório do ficheiro sem makeDirs explícito
    EXPECT(st.writeText("scenes/nivel2.goni", "{}"));
    wipeRoot();
}

TEST(fs_bytes_roundtrip_binario) {
    wipeRoot();
    FsStorage st(kRoot);
    const u8 blob[] = {0x00, 0x01, 0xFF, 0x80, 0x7F, 0x00, 0x42};
    std::vector<u8> out;
    EXPECT(!st.readBytes("meshes/faltando.bin", out));
    EXPECT(st.writeBytes("meshes/blob.bin", blob, sizeof(blob)));
    EXPECT(st.readBytes("meshes/blob.bin", out));
    EXPECT(out.size() == sizeof(blob));
    EXPECT(out[0] == 0x00 && out[2] == 0xFF && out[6] == 0x42);
    // writeBytes vazio (0 bytes) é válido
    EXPECT(st.writeBytes("meshes/vazio.bin", nullptr, 0));
    out.clear();
    EXPECT(st.readBytes("meshes/vazio.bin", out));
    EXPECT(out.empty());
    wipeRoot();
}

TEST(fs_list_dir_so_ficheiros_ordenado) {
    wipeRoot();
    FsStorage st(kRoot);
    EXPECT(st.makeDirs("meshes"));
    EXPECT(st.writeText("meshes/b.obj", "b"));
    EXPECT(st.writeText("meshes/a.obj", "a"));
    EXPECT(st.makeDirs("meshes/subdir"));     // diretórios NÃO aparecem
    EXPECT(st.writeText("meshes/subdir/c.obj", "c"));
    std::vector<std::string> files;
    EXPECT(st.listDir("meshes", files));
    EXPECT(files.size() == 2u);               // subdir ficaria como 3.º nome
    EXPECT(files[0] == "a.obj" && files[1] == "b.obj");
    // diretório inexistente → false (UI distingue "vazio" de "não existe")
    std::vector<std::string> none;
    EXPECT(!st.listDir("textures", none));
    wipeRoot();
}

TEST(fs_recusa_traversal_e_absoluto) {
    wipeRoot();
    FsStorage st(kRoot);
    EXPECT(!st.makeDirs("../fora"));
    EXPECT(!st.writeText("../fuga.goni", "x"));
    EXPECT(!st.writeText("/etc/passwd_vv", "x"));
    EXPECT(!st.exists("../fuga.goni"));
    std::string out;
    EXPECT(!st.readText("../fuga.goni", out));
    std::vector<u8> bytes;
    EXPECT(!st.readBytes("..", bytes));
    std::vector<std::string> files;
    EXPECT(!st.listDir("..", files));
    // nada vazou para fora da raiz
    struct stat probe{};
    EXPECT(::stat("/tmp/fuga.goni", &probe) != 0);
    wipeRoot();
}

TEST(fs_reabrir_projeto_e2e_do_disco) {
    // O fluxo do boot no device: abrir OU criar na raiz; editar; salvar;
    // "reboot" (novos objetos) → reabrir carrega manifesto + cena com refs.
    wipeRoot();
    FsStorage st(kRoot);
    Project p;
    EXPECT(Project::createNew(st, "e2e", p));

    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::RigidBody3D, nullptr, nullptr);
    s.get(h)->getComponent<Transform3D>()->pos = Vec3{-2.0f, 0.75f, 4.0f};
    s.get(h)->getComponent<Transform3D>()->updateWorld();
    EXPECT(p.saveActiveScene(st, s));
    EXPECT(p.saveManifest(st));

    // ---- "reboot" ----
    FsStorage st2(kRoot);
    Project q;
    EXPECT(Project::open(st2, q));
    Scene s2;
    SceneSerializer::LoadCtx ctx{};
    EXPECT(q.loadActiveScene(st2, s2, ctx));
    EXPECT(s2.count() == 1u);
    const Handle h2 = s2.find("RigidBody3D");
    EXPECT(h2.valid());
    const Transform3D* tr2 = s2.get(h2)->getComponent<Transform3D>();
    EXPECT(tr2 && nearEqF(tr2->pos.x, -2.0f) && nearEqF(tr2->pos.z, 4.0f));
    // e o round-trip do manifesto sobrevive no disco real
    EXPECT(st2.exists("project.goni"));
    EXPECT(st2.exists("scenes/main.goni"));
    EXPECT(st2.exists("meshes") && st2.exists("textures"));
    wipeRoot();
}
