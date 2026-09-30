// tests/test_saf_project.cpp — F5.4: Gestor de Projetos testado no
// hospedeiro. O core/SafStorage (ProjectStorage sobre SAF) é exercido
// contra um FakeSafIo — modelo in-memory de uma árvore SAF com a MESMA
// semântica dos providers reais:
//   • create com nome duplicado cria "nome (1)" (comportamento do
//     DocumentsContract) — SafStorage NUNCA pode cair nisso (resolve antes);
//   • openFd("w") devolve um fd real (memfd) cujo conteúdo só persiste no
//     documento quando o teste chama flushWrites() (no device é o provider
//     a persistir ao fecho do fd);
//   • list devolve filhos sem ordem garantida (o provider também não).
// O ProjectSlot (fila Activity→boot) é afervelado aqui também: push/wait/
// timeout/sobreposição — o mesmo contrato que o android_main consome.
#include "TestFramework.h"
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

// sys/memfd.h não existe em todos os sistemas (a glibc fornece o SÍMBOLO
// desde a 2.27) — protótipo explícito, assinatura estável da glibc
extern "C" int memfd_create(const char* name, unsigned int flags);
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#include <jni.h>   // FAKE (tests/stub é o primeiro include dir)

#include "platform/StorageBridge.h"
#include "core/Project.h"
#include "core/SafStorage.h"
#include "platform/ProjectSlot.h"
#include "platform/SafIo.h"

// métodos nativos definidos em platform/StorageBridge.cpp (compilado na
// suíte) — o papel do "stub Java" é chamá-los diretamente
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
    JNIEnv*, jclass, jobject activity, jstring origin);

using namespace vv;

// ---------------------------------------------------------------------------
// FakeSafIo — árvore SAF in-memory (docs com id fake://doc/N)
// ---------------------------------------------------------------------------
namespace {

struct Node {
    std::string name;
    std::string mime;
    std::string data;      // só ficheiros
    bool isDir = false;
    std::vector<std::string> kids;   // ids (ordem de criação — NÃO ordenada)
};

struct FakeSafIo final : public storage::SafIo {
    std::map<std::string, Node> docs;
    int nextId = 1;
    std::map<int, std::string> openWrites;   // fd → docUri (mode "w")

    FakeSafIo() {
        Node root;
        root.name = "pasta-escolhida";
        root.mime = storage::kSafDirMime;
        root.isDir = true;
        docs["fake://doc/1"] = root;
    }

    std::string newId() { return "fake://doc/" + std::to_string(++nextId); }

    std::string childNamed(const std::string& dirUri, const std::string& name) {
        const auto it = docs.find(dirUri);
        if (it == docs.end()) {
            return "";
        }
        for (const std::string& kid : it->second.kids) {
            const auto k = docs.find(kid);
            if (k != docs.end() && k->second.name == name) {
                return kid;
            }
        }
        return "";
    }

    bool rootDoc(const std::string&, std::string& outDocUri,
                 std::string&) override {
        outDocUri = "fake://doc/1";
        return true;
    }

    bool list(const std::string& dirDocUri, std::vector<storage::SafEntry>& out,
              std::string& err) override {
        const auto it = docs.find(dirDocUri);
        if (it == docs.end() || !it->second.isDir) {
            err = "não é pasta: " + dirDocUri;
            return false;
        }
        out.clear();
        for (const std::string& kid : it->second.kids) {
            const auto k = docs.find(kid);
            if (k != docs.end()) {
                storage::SafEntry e;
                e.uri = kid;
                e.name = k->second.name;
                e.mime = k->second.mime;
                out.push_back(std::move(e));
            }
        }
        return true;
    }

    bool create(const std::string& parentDocUri, const char* mime,
                const char* displayName, std::string& outDocUri,
                std::string& err) override {
        const auto it = docs.find(parentDocUri);
        if (it == docs.end() || !it->second.isDir) {
            err = "pai não é pasta: " + parentDocUri;
            return false;
        }
        // semântica do provider: duplicado NÃO falha — cria "nome (1)"
        std::string finalName = displayName;
        while (!childNamed(parentDocUri, finalName).empty()) {
            finalName += " (1)";
        }
        const std::string id = newId();
        Node n;
        n.name = finalName;
        n.mime = mime;
        n.isDir = (std::string(mime) == storage::kSafDirMime);
        docs[id] = n;
        it->second.kids.push_back(id);
        outDocUri = id;
        return true;
    }

    bool remove(const std::string& docUri, std::string& err) override {
        const auto it = docs.find(docUri);
        if (it == docs.end()) {
            err = "doc ausente: " + docUri;
            return false;
        }
        docs.erase(it);
        return true;
    }

    bool openFd(const std::string& docUri, const char* mode, int* outFd,
                std::string& err) override {
        const auto it = docs.find(docUri);
        if (it == docs.end()) {
            err = "doc ausente: " + docUri;
            return false;
        }
        if (it->second.isDir) {
            err = "é pasta: " + docUri;
            return false;
        }
        const int fd = ::memfd_create("vv-fake-saf", 0);
        if (fd < 0) {
            err = "memfd_create falhou";
            return false;
        }
        if (std::string(mode) == "r") {
            if (!it->second.data.empty()) {
                ::write(fd, it->second.data.data(), it->second.data.size());
            }
            ::lseek(fd, 0, SEEK_SET);
            *outFd = fd;
            return true;
        }
        // "w": o modelo guarda um DUP do memfd aberto — o fd devolvido ao
        // SafStorage é fechado por ele (no device é o COMMIT no provider);
        // o original sobrevive para o flushWrites() ler o que foi escrito.
        // Sem o dup, o kernel reutilizava o nº do fd fechado e a 2ª escrita
        // sobrepunha a 1ª no mapa (fd=3 para sempre).
        openWrites[fd] = docUri;
        *outFd = ::dup(fd);
        return true;
    }

    // o passo que no device é invisível (o provider persiste ao fechar o fd)
    size_t flushWrites() {
        size_t n = 0;
        for (const auto& kv : openWrites) {
            auto dit = docs.find(kv.second);
            if (dit == docs.end()) {
                ::close(kv.first);
                continue;
            }
            std::string data;
            char buf[4096];
            ::lseek(kv.first, 0, SEEK_SET);
            ssize_t r;
            while ((r = ::read(kv.first, buf, sizeof(buf))) > 0) {
                data.append(buf, static_cast<size_t>(r));
            }
            dit->second.data = data;
            ::close(kv.first);
            ++n;
        }
        openWrites.clear();
        return n;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// ProjectSlot — a fila Activity→boot (push/waitFor/timeout/sobreposição)
// ---------------------------------------------------------------------------

TEST(saf_project_slot_wait_and_push) {
    storage::ProjectSlot s;
    storage::ProjectRequest r;
    EXPECT(!s.waitFor(&r, 30));            // sem push → timeout honesto

    r.treeUri = "content://tree/abc";
    r.name = "meu jogo";
    s.push(r);

    storage::ProjectRequest got;
    EXPECT(s.waitFor(&got, 100));          // chega no wait
    EXPECT(got.treeUri == "content://tree/abc");
    EXPECT(got.name == "meu jogo");
    EXPECT(!s.tryPoll(&got));              // consumido (1 slot)
}

TEST(saf_project_slot_overwrite_and_late) {
    storage::ProjectSlot s;
    storage::ProjectRequest a;
    a.treeUri = "content://tree/antigo";
    a.name = "antigo";
    s.push(a);
    storage::ProjectRequest b;
    b.treeUri = "content://tree/novo";
    b.name = "novo";
    s.push(b);                             // sobrepõe (1 slot)
    EXPECT(s.droppedAny() == 1);

    storage::ProjectRequest got;
    EXPECT(s.tryPoll(&got));
    EXPECT(got.treeUri == "content://tree/novo");

    // push APÓS o wait desistir conta como "tarde" (diagnóstico no boot)
    storage::ProjectSlot s2;
    storage::ProjectRequest late;
    EXPECT(!s2.waitFor(nullptr, 30));
    s2.push(late);
    EXPECT(s2.latePushes() == 1);
}

// ---------------------------------------------------------------------------
// SafStorage — estrutura do projeto, round-trip e resolução de caminhos
// ---------------------------------------------------------------------------

TEST(saf_storage_open_or_creates_full_structure) {
    FakeSafIo io;
    SafStorage st(&io, "content://tree/pasta-do-projeto");

    Project p;
    EXPECT(Project::openOrCreate(st, "meu jogo", p));
    EXPECT(p.name == "meu jogo");
    EXPECT(p.scenes.size() == 1);
    EXPECT(io.flushWrites() >= 2);   // project.goni + scenes/main.goni escritos

    // estrutura completa visível no modelo (storage novo = cache limpa)
    SafStorage st2(&io, "content://tree/pasta-do-projeto");
    EXPECT(st2.exists(Project::kManifestFile));
    EXPECT(st2.exists(Project::kDirScenes));
    EXPECT(st2.exists(Project::kDirMeshes));
    EXPECT(st2.exists(Project::kDirTextures));
    std::vector<std::string> files;
    EXPECT(st2.listDir(Project::kDirScenes, files));
    EXPECT(files.size() == 1);
    EXPECT(files[0] == "main.goni");
    EXPECT(st2.listDir(Project::kDirMeshes, files));
    EXPECT(files.empty());
    EXPECT(st2.listDir(Project::kDirTextures, files));
    EXPECT(files.empty());

    // REABRIR com storage novo (nova cache) — o projeto lê de volta
    Project p2;
    EXPECT(Project::open(st2, p2));
    EXPECT(p2.name == "meu jogo");
    EXPECT(p2.scenes.size() == 1);
    EXPECT(p2.scenes[0] == "scenes/main.goni");
}

TEST(saf_storage_text_and_bytes_roundtrip) {
    FakeSafIo io;
    SafStorage st(&io, "content://tree/x");

    const std::string cena = R"({"version":1,"tics":[]})";
    EXPECT(st.writeText("scenes/main.goni", cena));
    EXPECT(io.flushWrites() == 1);
    std::string back;
    EXPECT(st.readText("scenes/main.goni", back));
    EXPECT(back == cena);

    // bytes (PNG fake — binário com 0x00 e bytes altos)
    std::vector<u8> png;
    for (int i = 0; i < 300; ++i) {
        png.push_back(static_cast<u8>((i * 37) % 256));
    }
    EXPECT(st.writeBytes("textures/base.png", png.data(), png.size()));
    EXPECT(io.flushWrites() == 1);
    std::vector<u8> backB;
    EXPECT(st.readBytes("textures/base.png", backB));
    EXPECT(backB == png);

    // sobrescrever REUTILIZA o documento (create duplicado criaria " (1)")
    EXPECT(st.writeText("scenes/main.goni", "v2"));
    EXPECT(io.flushWrites() == 1);
    FakeSafIo* f = &io;
    int goniDocs = 0;
    for (const auto& kv : f->docs) {
        if (kv.second.name == "main.goni") ++goniDocs;
    }
    EXPECT(goniDocs == 1);
    std::string v2;
    EXPECT(st.readText("scenes/main.goni", v2));
    EXPECT(v2 == "v2");

    // mkdir -p: escrita em subpasta nova cria o caminho TODO
    EXPECT(st.writeText("meshes/deep/nested.obj", "v 0 0 0"));
    EXPECT(io.flushWrites() == 1);
    EXPECT(st.exists("meshes/deep"));
    std::vector<std::string> files;
    EXPECT(st.listDir("meshes/deep", files));
    EXPECT(files.size() == 1);
    EXPECT(files[0] == "nested.obj");
}

TEST(saf_storage_missing_and_invalid_paths) {
    FakeSafIo io;
    SafStorage st(&io, "content://tree/x");

    std::string out;
    EXPECT(!st.readText("scenes/nope.goni", out));   // ausente → false
    EXPECT(!st.exists("scenes/nope.goni"));
    std::vector<std::string> files;
    EXPECT(!st.listDir("no-such-dir", files));       // pasta ausente → false

    // makeDirs + exists de DIRETÓRIO (a pergunta válida não é erro)
    EXPECT(st.makeDirs("scenes/sub"));
    EXPECT(st.exists("scenes"));
    EXPECT(st.exists("scenes/sub"));
    EXPECT(st.listDir("scenes", files));
    EXPECT(files.empty());   // lista só FICHEIROS — sub não aparece

    // traversal recusado (mesma guarda das implementações reais)
    EXPECT(!st.writeText("../fora.txt", "x"));
    EXPECT(!st.writeText("a//b.txt", "x"));
    EXPECT(!st.exists("/absoluto"));
    EXPECT(!st.makeDirs("a/../b"));
}

// ---------------------------------------------------------------------------
// Ponte JNI do SAF (JniSafIo) — fumo no hospedeiro com o fake JNI.
// Determinístico quanto à ordem dos casos: o handshake começa em BAIXO
// (registo com método crítico ausente) para aferir a mensagem honesta e
// depois recupera com um registo completo.
// ---------------------------------------------------------------------------

namespace {

// identidade fake da activity (igual ao test_handshake)
constexpr intptr_t kAct = 0xA001;
constexpr intptr_t kCls = 0xA002;

void fakeRegisters(const char* origin) {
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, reinterpret_cast<jclass>(static_cast<intptr_t>(kCls)),
        reinterpret_cast<jobject>(static_cast<intptr_t>(kAct)),
        g_jni.newString(origin));
}

} // namespace

TEST(saf_jni_bridge_smoke_and_honest_errors) {
    g_jni.reset();
    g_jni.fail_methods["bridgeOpenFd"] = true;   // método CRÍTICO ausente

    fakeRegisters("onCreate");
    EXPECT(!vv::storage::handshakeOk());         // ponte em baixo

    // SEM handshake → TODA operação falha com a causa HONESTA (ponte,
    // nunca "sistema sem suporte")
    std::string err, outUri;
    EXPECT(!vv::storage::jniSafIo()->rootDoc("content://tree/x", outUri, err));
    EXPECT(err == "ponte Java indisponível (handshake)");
    int fd = -1;
    EXPECT(!vv::storage::jniSafIo()->openFd("fake://doc/1", "r", &fd, err));
    EXPECT(err == "ponte Java indisponível (handshake)");

    // recuperação: registo completo (a Java fake fornece os bridges)
    g_jni.reset();
    fakeRegisters("onCreate");
    EXPECT(vv::storage::handshakeOk());

    g_jni.bridge_root_doc =
        [](const std::string& tree) { return tree + "#raiz"; };
    g_jni.bridge_open_fd = [](const std::string&, const std::string& mode) {
        const int f = ::memfd_create("vv-bridge", 0);
        if (f >= 0 && mode == "r") {
            const std::string dados = "conteudo-do-projeto";
            ::write(f, dados.data(), dados.size());
            ::lseek(f, 0, SEEK_SET);
        }
        return f;
    };
    g_jni.bridge_list = [](const std::string&) {
        return std::vector<std::string>{
            "fake://doc/9", "main.goni", "application/json",
        };
    };
    g_jni.bridge_create = [](const std::string&, const std::string& mime,
                             const std::string& name) {
        return "fake://criado/" + name + "/" + mime;
    };
    g_jni.bridge_delete = [](const std::string& uri) {
        return uri == "fake://apagavel";
    };

    EXPECT(vv::storage::jniSafIo()->rootDoc("content://tree/x", outUri, err));
    EXPECT(outUri == "content://tree/x#raiz");

    EXPECT(vv::storage::jniSafIo()->openFd("fake://doc/2", "r", &fd, err));
    EXPECT(fd >= 0);
    char buf[64] = {0};
    const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    EXPECT(n > 0);
    EXPECT(std::string(buf) == "conteudo-do-projeto");

    std::vector<storage::SafEntry> kids;
    EXPECT(vv::storage::jniSafIo()->list("fake://doc/1", kids, err));
    EXPECT(kids.size() == 1);
    EXPECT(kids[0].name == "main.goni");
    EXPECT(kids[0].mime == "application/json");

    std::string created;
    EXPECT(vv::storage::jniSafIo()->create("fake://doc/1", storage::kSafDirMime,
                                           "scenes", created, err));
    EXPECT(created == "fake://criado/scenes/vnd.android.document/directory");

    EXPECT(vv::storage::jniSafIo()->remove("fake://apagavel", err));
    err.clear();
    EXPECT(!vv::storage::jniSafIo()->remove("fake://outro", err));
    EXPECT(err.find("false") != std::string::npos);
}
