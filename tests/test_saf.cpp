// tests/test_saf.cpp — F5.1-C: máquina de estado SAF + SafStorage sobre
// backend fake + RoutingStorage (fallback). O JNI real é device-only —
// aqui valida-se TODA a lógica GL-free que o device consome.
#include "TestFramework.h"
#include <map>
#include <set>
#include <string>
#include <vector>
#include "platform/Saf.h"
#include "core/SafStorage.h"
#include "FakeStorage.h"

using namespace vv;
using saf::SafResult;
using saf::SafStateMachine;
using saf::State;

namespace {

// backend SAF fake: map rel-path → bytes, semântica igual ao FakeStorage
struct FakeSafBackend final : public SafBackend {
    std::map<std::string, std::string> files;
    std::set<std::string> dirs;
    mutable std::string lastUri;   // registrado também nos métodos const

    bool listFiles(const std::string& uri, const std::string& relDir,
                   std::vector<std::string>& out) const override {
        lastUri = uri;
        if (dirs.count(relDir) == 0) {
            return false;
        }
        out.clear();
        const std::string prefix = relDir + "/";
        for (const auto& kv : files) {
            if (kv.first.compare(0, prefix.size(), prefix) == 0 &&
                kv.first.find('/', prefix.size()) == std::string::npos) {
                out.push_back(kv.first.substr(prefix.size()));
            }
        }
        return true;
    }
    bool makeDirs(const std::string& uri, const std::string& relDir) override {
        lastUri = uri;
        if (!validRelPath(relDir)) {
            return false;
        }
        dirs.insert(relDir);
        return true;
    }
    bool exists(const std::string& uri, const std::string& rel) const override {
        lastUri = uri;
        return files.count(rel) != 0 || dirs.count(rel) != 0;
    }
    bool writeBytes(const std::string& uri, const std::string& rel,
                    const void* data, size_t n) override {
        lastUri = uri;
        if (!validRelPath(rel)) {
            return false;
        }
        files[rel].assign(static_cast<const char*>(data), n);
        return true;
    }
    bool readBytes(const std::string& uri, const std::string& rel,
                   std::vector<u8>& out) const override {
        lastUri = uri;
        const auto it = files.find(rel);
        if (it == files.end()) {
            return false;
        }
        out.assign(it->second.begin(), it->second.end());
        return true;
    }
};

} // namespace

TEST(saf_state_machine_fluxo_completo) {
    SafStateMachine sm;
    EXPECT(sm.state() == State::Idle);
    EXPECT(!sm.uriUsable());

    // begin → Pending
    sm.begin(saf::kReqPickTree);
    EXPECT(sm.state() == State::Pending);
    EXPECT(sm.pendingRequest() == saf::kReqPickTree);

    // cancelamento → Denied, URI inutilizável
    SafResult cancel;
    cancel.request = saf::kReqPickTree;
    cancel.ok = false;
    EXPECT(!sm.onResult(cancel));
    EXPECT(sm.state() == State::Denied);
    EXPECT(!sm.uriUsable());

    // novo pedido → concedido com URI persistível
    sm.begin(saf::kReqPickTree);
    SafResult ok;
    ok.request = saf::kReqPickTree;
    ok.ok = true;
    ok.uri = "content://com.android.externalstorage.documents/tree/primary%3AJogos";
    ok.flags = saf::kFlagRead | saf::kFlagWrite | saf::kFlagPersistable;
    EXPECT(sm.onResult(ok));
    EXPECT(sm.state() == State::Granted);
    EXPECT(sm.uriUsable());
    EXPECT(sm.grantedUri() == ok.uri);
    EXPECT(saf::flagsPersistable(ok.flags));
    EXPECT(saf::flagsReadWrite(ok.flags));
    EXPECT(!saf::flagsPersistable(saf::kFlagRead));
}

TEST(saf_state_machine_resultados_invalidos) {
    SafStateMachine sm;

    // resultado sem pedido aberto → Denied
    SafResult r;
    r.request = saf::kReqPickTree;
    r.ok = true;
    r.uri = "content://x";
    EXPECT(!sm.onResult(r));
    EXPECT(sm.state() == State::Denied);

    // request DIVERGENTE do pendente → Denied
    sm.begin(saf::kReqImport);
    r.request = saf::kReqExport;
    EXPECT(!sm.onResult(r));
    EXPECT(sm.state() == State::Denied);

    // OK mas URI VAZIA → Denied
    sm.begin(saf::kReqImport);
    r.request = saf::kReqImport;
    r.ok = true;
    r.uri = "";
    EXPECT(!sm.onResult(r));
    EXPECT(sm.state() == State::Denied);

    // fluxo correto termina Granted
    sm.begin(saf::kReqImport);
    r.uri = "content://y";
    EXPECT(sm.onResult(r));
    EXPECT(sm.uriUsable());
    sm.reset();
    EXPECT(sm.state() == State::Idle);
    EXPECT(!sm.uriUsable());
}

TEST(saf_storage_guardas_de_caminho_antes_do_backend) {
    FakeSafBackend backend;
    SafStorage st(&backend, "content://tree/prim%3AJogos");

    // traversal/absoluto/backslash NUNCA chegam ao backend
    EXPECT(!st.writeText("../fuga.txt", "x"));
    EXPECT(!st.writeText("a/../../b", "x"));
    EXPECT(!st.writeBytes("/absoluto.bin", "x", 1));
    std::string sink;
    EXPECT(!st.readText("a\\b.txt", sink));   // backslash recusado
    EXPECT(!st.makeDirs(""));
    EXPECT(!st.exists("../x"));
    EXPECT(backend.files.empty());
    EXPECT(backend.dirs.empty());

    // caminho válido chega
    EXPECT(st.writeText("scenes/main.goni", "{}"));
    EXPECT(backend.files.count("scenes/main.goni") == 1u);
    EXPECT(backend.lastUri == "content://tree/prim%3AJogos");
    EXPECT(st.root() == "content://tree/prim%3AJogos");
}

TEST(saf_storage_roundtrip_completo_via_backend) {
    FakeSafBackend backend;
    SafStorage st(&backend, "content://tree/uri");

    // texto
    EXPECT(st.writeText("scenes/main.goni", "conteudo da cena"));
    std::string text;
    EXPECT(st.readText("scenes/main.goni", text));
    EXPECT(text == "conteudo da cena");

    // bytes binários (PNG do cache, p.ex.)
    std::vector<u8> bin = {0x89, 'P', 'N', 'G', 0, 1, 2, 255};
    EXPECT(st.writeBytes("textures/cache/cache_ab.gtc", bin.data(), bin.size()));
    std::vector<u8> out;
    EXPECT(st.readBytes("textures/cache/cache_ab.gtc", out));
    EXPECT(out == bin);

    // exists + listDir (só ficheiros, ordenado)
    EXPECT(st.exists("scenes/main.goni"));
    EXPECT(!st.exists("scenes/outra.goni"));
    EXPECT(st.writeText("scenes/z.goni", "z"));
    EXPECT(st.writeText("scenes/a.goni", "a"));
    std::vector<std::string> files;
    EXPECT(st.listDir("scenes", files));
    EXPECT(files.size() == 3u);
    EXPECT(files[0] == "a.goni" && files[2] == "z.goni");
    // listDir de pasta inexistente falha
    EXPECT(!st.listDir("meshes", files));
}

TEST(routing_fallback_cancelou_vai_para_primario) {
    FakeStorage prim;
    prim.rootPath = "/external";
    FakeSafBackend saf;
    SafStorage sec(&saf, "content://tree/x");
    RoutingStorage router(&prim, &sec);

    // default = primário (getExternalFilesDir)
    EXPECT(!router.usingSecondary());
    EXPECT(router.root() == "/external");

    // SAF concedido → secundário
    router.useSecondary(true);
    EXPECT(router.usingSecondary());
    EXPECT(router.writeText("scenes/s.goni", "via saf"));
    EXPECT(saf.files.count("scenes/s.goni") == 1u);
    EXPECT(prim.files.empty());   // primário intocado
    std::string t;
    EXPECT(router.readText("scenes/s.goni", t));
    EXPECT(t == "via saf");
    std::vector<std::string> files;
    EXPECT(router.listDir("scenes", files));
    EXPECT(files.size() == 1u);

    // fallback (cancelou/URI inválida) → primário de novo
    router.useSecondary(false);
    EXPECT(router.writeText("scenes/local.goni", "via files"));
    EXPECT(prim.files.count("scenes/local.goni") == 1u);
    EXPECT(!router.exists("scenes/s.goni"));   // dados do SAF não vazam
}

TEST(routing_storage_router_rejeita_ops_de_recursos_nulos) {
    // router com secundário SAF cujo backend recusa tudo (URI inválida)
    FakeStorage prim;
    FakeSafBackend saf;
    SafStorage sec(&saf, "content://tree/x");
    RoutingStorage router(&prim, &sec);

    // escrita no secundário com dir não criado: backend fake aceita writes
    // diretos (igual ao FakeStorage); aqui valida a DELEGAÇÃO pura:
    router.useSecondary(true);
    EXPECT(router.makeDirs("textures"));
    EXPECT(saf.dirs.count("textures") == 1u);
    router.useSecondary(false);
    EXPECT(router.makeDirs("textures"));
    EXPECT(prim.dirs.count("textures") == 1u);
    EXPECT(saf.dirs.size() == 1u);   // só o secundário criou
}

// ---------------------------------------------------------------------------
// F5.1-hotfix (auditoria JNI): fila UI-thread → engine-thread
// ---------------------------------------------------------------------------

TEST(pending_result_push_poll_roundtrip) {
    saf::PendingResult q;
    saf::SafResult out;
    EXPECT(!q.poll(&out));   // vazio → false

    saf::SafResult r;
    r.request = saf::kReqPickTree;
    r.ok = true;
    r.uri = "content://tree/pasta";
    r.flags = saf::kFlagRead | saf::kFlagWrite;
    q.push(r);

    EXPECT(q.poll(&out));
    EXPECT(out.request == saf::kReqPickTree);
    EXPECT(out.ok);
    EXPECT(out.uri == "content://tree/pasta");
    EXPECT(out.flags == (saf::kFlagRead | saf::kFlagWrite));
    EXPECT(!q.poll(&out));   // consumido
    EXPECT(!q.droppedAny()); // nada descartado
}

TEST(pending_result_sobrepoe_e_marca_dropped) {
    saf::PendingResult q;
    saf::SafResult a;
    a.request = saf::kReqImport;
    a.uri = "primeiro";
    saf::SafResult b;
    b.request = saf::kReqExport;
    b.uri = "segundo";
    q.push(a);
    q.push(b);   // sobreposição (resultado anterior não consumido)

    saf::SafResult out;
    EXPECT(q.poll(&out));
    EXPECT(out.uri == "segundo");   // só o ÚLTIMO interessa
    EXPECT(q.droppedAny());         // e o descarte fica registrado
}
