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
#include "core/Scene.h"
#include "components/MeshRenderer.h"
#include "platform/ProjectSlot.h"
#include "platform/SafIo.h"
#include "FakeSafIo.h"   // 0.8.12: extraído p/ partilhar com o C33 virtual

// métodos nativos definidos em platform/StorageBridge.cpp (compilado na
// suíte) — o papel do "stub Java" é chamá-los diretamente
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
    JNIEnv*, jclass, jobject activity, jstring origin);

using namespace vv;

// FakeSafIo — agora em tests/FakeSafIo.h (partilhado com o C33 virtual)


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
// F5.4-hotfix — ANTI-DUPLICAÇÃO. O bug do device ("main.goni (1).json",
// "project.goni (2)" em TODO boot): o provider renomeava o displayName no
// createDocument (mime json + ext desconhecida) e o nome no disco divergia
// do nome procurado → verificação falhava → createDocument de novo. O fake
// agora MODELA o rename + a colisão; estes testes falhariam com o código
// da 0.6.4.
// ---------------------------------------------------------------------------

namespace {

// contagem de documentos por nome + detetor de sufixo anticolisão
int countNamed(const FakeSafIo& io, const std::string& name) {
    int n = 0;
    for (const auto& kv : io.docs) {
        if (kv.second.name == name) {
            ++n;
        }
    }
    return n;
}
int countCollisions(const FakeSafIo& io) {
    int n = 0;
    for (const auto& kv : io.docs) {
        if (kv.second.name.find(" (1)") != std::string::npos ||
            kv.second.name.find(" (2)") != std::string::npos) {
            ++n;
        }
    }
    return n;
}

} // namespace

// O TESTE pedido pelo dono: criar projeto → FECHAR → reabrir → editar →
// guardar de novo — NENHUM sufixo (1)/(2) pode aparecer.
TEST(saf_no_duplication_across_reopen) {
    FakeSafIo io;
    {
        // "boot 1" — criar
        SafStorage st(&io, "content://tree/x");
        Project p;
        EXPECT(Project::openOrCreate(st, "meu jogo", p));
        EXPECT(io.flushWrites() >= 2);
    }
    {
        // "boot 2" — reabrir (storage NOVO = caches limpas, como no reboot),
        // editar e guardar de novo
        SafStorage st(&io, "content://tree/x");
        Project p;
        EXPECT(Project::openOrCreate(st, "meu jogo", p));   // ABRE (não cria)
        EXPECT(p.name == "meu jogo");
        EXPECT(p.scenes.size() == 1);
        Scene edit;
        const Handle h = edit.create("caixa");
        EXPECT(h.valid());
        MeshRenderer* mr = edit.get(h)->addComponent<MeshRenderer>();
        mr->meshPath = "";   // procedural
        EXPECT(p.saveActiveScene(st, edit));
        EXPECT(p.saveManifest(st));
        EXPECT(io.flushWrites() >= 2);
    }
    {
        // "boot 3" — reabrir outra vez e regravar o manifesto
        SafStorage st(&io, "content://tree/x");
        Project p;
        EXPECT(Project::open(st, p));
        EXPECT(p.saveManifest(st));
        EXPECT(io.flushWrites() >= 1);
    }
    // EXATAMENTE um manifesto e uma cena — zero duplicados, zero renames
    EXPECT(countNamed(io, "project.goni") == 1);
    EXPECT(countNamed(io, "main.goni") == 1);
    EXPECT(countCollisions(io) == 0);
    for (const auto& kv : io.docs) {
        // o provider NUNCA recebeu json-mime para .goni → sem ".goni.json"
        EXPECT(kv.second.name.find(".goni.json") == std::string::npos);
    }
    // e o projeto continua a abrir com o conteúdo intacto
    SafStorage st(&io, "content://tree/x");
    Project p;
    EXPECT(Project::open(st, p));
    EXPECT(p.name == "meu jogo");
    Scene cena;
    SceneSerializer::LoadCtx ctx;
    EXPECT(p.loadActiveScene(st, cena, ctx));
    EXPECT(cena.find("caixa").valid());
}

// CURA dos projetos criados pela 0.6.4 (ficheiros já renomeados pelo
// provider): reabre os ".goni.json" e reutiliza-os — nunca cria por cima.
TEST(saf_heal_provider_renamed_files) {
    FakeSafIo io;
    {
        // legado 0.6.4: o provider guardou com ".json" acrescentado
        SafStorage st(&io, "content://tree/x");
        Project p;
        EXPECT(Project::openOrCreate(st, "legado", p));
        EXPECT(io.flushWrites() >= 2);
        // simula o rename do provider 0.6.4 nos documentos já gravados
        // (o fake da 0.6.4 gravava verbatim; o device guardava "x.goni.json")
    }
    // força os nomes que o provider real produziu na 0.6.4
    for (auto& kv : io.docs) {
        if (kv.second.name == "project.goni") {
            kv.second.name = "project.goni.json";
        } else if (kv.second.name == "main.goni") {
            kv.second.name = "main.goni.json";
        }
    }
    {
        // reabrir: o resolveChild (contrato bridgeFindFile) cura via
        // compat "nome + .json" — abre o legado SEM criar novos ficheiros
        SafStorage st(&io, "content://tree/x");
        Project p;
        EXPECT(Project::openOrCreate(st, "legado", p));
        EXPECT(p.name == "legado");
        EXPECT(p.scenes.size() == 1);
        EXPECT(p.saveManifest(st));
        EXPECT(io.flushWrites() >= 1);
    }
    // o manifesto continua ÚNICO (agora com o nome legado) — nada cresceu
    EXPECT(countNamed(io, "project.goni.json") == 1);
    EXPECT(countNamed(io, "main.goni.json") == 1);
    EXPECT(countNamed(io, "project.goni") == 0);
    EXPECT(countCollisions(io) == 0);
}

// probe TRI-ESTADO + recusa de criação com provider em falha (regra do
// dono: createDocument SÓ depois de ausência CONFIRMADA — "não sei" nunca cria)
TEST(saf_probe_tri_state_and_create_refusal) {
    FakeSafIo io;
    SafStorage st(&io, "content://tree/x");

    // Absent CONFIRMADO (pai existe, nome não está lá)
    EXPECT(st.makeDirs("scenes"));
    EXPECT(st.probe("scenes/nope.goni") == Presence::Absent);
    // Present
    EXPECT(st.writeText("scenes/nope.goni", "{}"));
    EXPECT(io.flushWrites() >= 1);
    EXPECT(st.probe("scenes/nope.goni") == Presence::Present);
    EXPECT(st.exists("scenes/nope.goni"));
    // Absent herdado (pai confirmado ausente)
    EXPECT(st.probe("nao-existe/f.x") == Presence::Absent);
    // pai inválido/absoluto → Unknown (indecidido, honesto)
    EXPECT(st.probe("/absoluto") == Presence::Unknown);

    // provider em falha → Unknown E a escrita falha SEM criar nada
    io.failQueries = true;
    EXPECT(st.probe("scenes/outro.goni") == Presence::Unknown);
    EXPECT(st.probe("project.goni") == Presence::Unknown);
    EXPECT(!st.writeText("scenes/outro.goni", "{}"));   // recusa (não decide)
    EXPECT(!st.exists("scenes/outro.goni"));
    io.failQueries = false;
    std::vector<std::string> files;
    EXPECT(st.listDir("scenes", files));
    EXPECT(files.size() == 1);          // só nope.goni — nada foi criado
    EXPECT(files[0] == "nope.goni");

    // e createNew sobre provider em falha NÃO cria o projeto (nem duplica)
    FakeSafIo io2;
    io2.failQueries = true;
    SafStorage st2(&io2, "content://tree/x");
    Project p;
    EXPECT(!Project::openOrCreate(st2, "x", p));   // probe Unknown → recusa
    EXPECT(io2.docs.size() == 1);                  // só a raiz — nada criado
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

// F5.4-hotfix — bridgeFindFile (verificação DEDICADA de existência) pela
// JniSafIo: contrato TRI-ESTADO (URI / "" confirmado / null indecidido) +
// repetição da query em falha. É esta verificação que o native exige antes
// de QUALQUER bridgeCreate (a regra do dono contra os " (1)").
TEST(saf_jni_find_file_tri_state_and_retry) {
    g_jni.reset();
    fakeRegisters("onCreate");
    EXPECT(vv::storage::handshakeOk());   // bridgeFindFile é CRÍTICA — presente

    bool found = false;
    std::string uri, err;

    // 1) exato → found + uri
    g_jni.bridge_find_file = [](const std::string&, const std::string& n) {
        return n == "project.goni" ? std::string("content://doc/pj") : std::string("");
    };
    EXPECT(vv::storage::jniSafIo()->resolveChild("content://doc/raiz",
                                                 "project.goni", found, uri, err));
    EXPECT(found && uri == "content://doc/pj");
    EXPECT(err.empty());

    // 2) "" → ausência CONFIRMADA (found=false SEM erro — só aqui se cria)
    EXPECT(vv::storage::jniSafIo()->resolveChild("content://doc/raiz",
                                                 "main.goni", found, uri, err));
    EXPECT(!found && uri.empty());
    EXPECT(err.empty());   // NÃO é erro — é ausência confirmada

    // 3) null SEMPRE (query falhou) → erro; a query repetiu UMA vez antes
    //    de desistir (não confiar num único insucesso p/ decidir criação)
    g_jni.find_file_calls = 0;
    g_jni.find_file_null = 99;          // todas as chamadas falham
    EXPECT(!vv::storage::jniSafIo()->resolveChild("content://doc/raiz",
                                                  "x.goni", found, uri, err));
    EXPECT(!found);
    EXPECT(err.find("null") != std::string::npos);
    EXPECT(g_jni.find_file_calls == 2);   // 1ª tentativa + repetição fresca

    // 4) null na 1ª chamada, query fresca ACHA na 2ª → recupera (found)
    g_jni.find_file_calls = 0;
    g_jni.find_file_null = 1;           // só a 1ª falha
    g_jni.bridge_find_file = [](const std::string&, const std::string& n) {
        return n == "flaky.goni" ? std::string("content://doc/f")
                                 : std::string("");
    };
    EXPECT(vv::storage::jniSafIo()->resolveChild("content://doc/raiz",
                                                 "flaky.goni", found, uri, err));
    EXPECT(found && uri == "content://doc/f");
    EXPECT(g_jni.find_file_calls == 2);

    // 5) bridgeFindFile AUSENTE no dex → registo PARCIAL: handshake fica em
    //    BAIXO (é CRÍTICA — sem ela o createDocument duplicaria) e toda a
    //    ponte responde a causa honesta
    g_jni.reset();
    g_jni.fail_methods["bridgeFindFile"] = true;
    fakeRegisters("onCreate");
    EXPECT(!vv::storage::handshakeOk());
    EXPECT(!vv::storage::jniSafIo()->resolveChild("content://doc/raiz",
                                                  "y.goni", found, uri, err));
    EXPECT(err == "ponte Java indisponível (handshake)");
}
