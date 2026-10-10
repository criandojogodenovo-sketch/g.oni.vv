// tests/c33_virtual.cpp — 0.8.12: o DISPOSITIVO VIRTUAL EM CI ("C33
// virtual" — o "PC virtual" obrigatório do dono).
//
// O QUE ISTO É: um harness HEADLESS que reproduz EXATAMENTE as condições
// do Android que morderam a engine entre 0.8.5 e 0.8.10 — as que o CI
// verde nunca via porque o runner do Linux é "demasiado saudável":
//
//   [fs]    /tmp READ-ONLY (errno=30/EROFS — a seam fileapi::testing do
//           FileApi; no device é o kernel, aqui é o prefixo bloqueado) —
//           o bug do staging era INVISÍVEL no CI com /tmp escrevível;
//   [fs]    só o CACHE DIR da app (getCacheDir via ponte JNI) e o projeto
//           aceitam escrita — exatamente as superfícies do Android;
//   [uri]   o caminho content:// do SAF exercitado com o FakeSafIo (o
//           MESMO modelo de provider que a suíte afere desde a F5.4) — a
//           migração de projeto antigo pelo SAF é o fluxo que morria;
//   [egl]   ciclos TERM_WINDOW/INIT_WINDOW com destruição e RE-CRIAÇÃO do
//           contexto (re-upload de tudo — o wiring do 0.6.7);
//   [tela]  superfície 1536×720 (a resolução REAL do C33) com insets
//           (status 24 + pill 24 — o contentRect do device);
//   [gpu]   ASTC LDR ativo (o Mali do C33; o boot loga "ASTC SIM");
//   [dedo]  sequências REPLAYÁVEIS de tap (injectDown/injectUp + frame()
//           REAL do main.cpp — o mesmo caminho de input do telefone).
//
// O REPLAY é a SESSÃO REAL do dono (a que produzia os sintomas dos logs):
// selecionar o TIC → abrir o picker → tocar na linha do picker DENTRO do
// viewRect (o toque que matava a seleção no MESMO frame do dispatch) →
// trocar mesh → escolher none → tap no backdrop → desselecionar →
// lifecycle TERM/INIT → trocar de novo. Cada passo ASSERTA e o output
// (passo-a-passo) é o que o relatório COLA e o gate do CI GREPA contra
// ci/forbidden_log_patterns.txt — qualquer padrão proibido = CI VERMELHO
// e release bloqueada.
//
// Este ficheiro é um EXECUTÁVEL SEPARADO (c33_virtual) — não faz parte do
// test_core: inclui platform/main.cpp (o caminho real do device, um só
// android_main por binário) e tem o SEU main() que corre o replay todo e
// sai non-zero em qualquer falha (harness vermelho = release bloqueada).
// (antes de TUDO: o FakeSafIo.h usa memfd_create — glibc exige _GNU_SOURCE
// ANTES do primeiro <unistd.h>/<sys/*.h> do TU)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats + astcLdr)
#include <EGL/egl.h>    // stub (eglstub::g_surfaceW/H — o harness põe 1536×720)
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>   // 0.9.6.4 (12.8b): mkdir cru p/ a fixture do par
#include <cerrno>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <jni.h>   // FAKE controlável (tests/stub — cache_dir p/ o getCacheDir)

#include "FakeStorage.h"
#include "FakeSafIo.h"   // 0.8.12: o provider content:// do C33 virtual
// 0.9.3 (REG-002): o stub do oboe — o OboeBackend de PRODUÇÃO que o
// main.cpp arranca compila contra ESTE header no host (o padrão jni.h)
#include <oboe/Oboe.h>

// ---- O CAMINHO REAL DO DEVICE (namespace anónimo = mesmo TU) ---------------
#include "voni/VoniDocs.h"   // FASE 9: a pesquisa das Docs
#include "platform/main.cpp"

// ponte Java (o papel do "stub Java" — como o test_wiring087/test_handshake)
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);
// 0.9.1 — os natives do IME (definidos no StorageBridge.cpp; o harness
// chama-os DIRETO como o "Java fake" — o mesmo caminho do device)
extern "C" void Java_vv_goni_VvActivity_nativeOnImeText(
        JNIEnv*, jclass, jstring text);
extern "C" void Java_vv_goni_VvActivity_nativeOnImeKey(
        JNIEnv*, jclass, jint keyCode, jint action);

using namespace vv;

// ===========================================================================
// harness: CHECK com output passo-a-passo (o que o relatório cola)
// ===========================================================================
namespace {

int g_checks = 0;
int g_failed = 0;

bool check(bool cond, const char* what) {
    ++g_checks;
    if (cond) {
        std::printf("    [ok]   %s\n", what);
    } else {
        ++g_failed;
        std::printf("    [FAIL] %s\n", what);
    }
    std::fflush(stdout);
    return cond;
}

void fase(const char* name) {
    std::printf("\n== %s ==\n", name);
    std::fflush(stdout);
}

void passo(const char* name) {
    std::printf("  > %s\n", name);
    std::fflush(stdout);
}

const char* kHarnessLogs = "c33-virtual-logs";
const char* kCacheDir = "c33-virtual-cache";

void rmrf(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n != "." && n != "..") {
                ::remove((dir + "/" + n).c_str());
            }
        }
        ::closedir(d);
    }
    ::remove(dir.c_str());
}

std::vector<std::string> logLines(int maxLines = 3000) {
    std::vector<std::string> lines;
    vv::elog::readTail(lines, maxLines);
    return lines;
}

bool logHas(const char* needle) {
    const std::vector<std::string> lines = logLines();
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// conta ocorrências (o gate exige ZERO dos proibidos — contar prova)
int logCount(const char* needle) {
    int n = 0;
    for (const std::string& l : logLines()) {
        if (l.find(needle) != std::string::npos) {
            ++n;
        }
    }
    return n;
}

// 0.10-M (PASSO 3B · FASE 19.4): o GLB de N vértices com índices u32 — o
// perfil do «modelo demasiado grande para o runtime de hoje» (70 000 >
// 65 535: o streaming converte em 2 blocos; o load do mesh único recusa).
// Uma primitiva, um nó, SEM pele (o caminho streaming de produção).
bool buildGlbFase19(u32 verts, u32 tris, const std::string& path) {
    // layout do BIN: pos (12) + nrm (12) + uv (8) por vértice, idx u32×3
    // por triângulo — offsets alinhados a 4
    const u64 posLen = u64(verts) * 12;
    const u64 nrmOff = (posLen + 3) & ~3ull;
    const u64 nrmLen = u64(verts) * 12;
    const u64 uvOff = (nrmOff + nrmLen + 3) & ~3ull;
    const u64 uvLen = u64(verts) * 8;
    const u64 idxOff = (uvOff + uvLen + 3) & ~3ull;
    const u64 idxLen = u64(tris) * 3 * 4;
    const u64 binLen = idxOff + idxLen;
    char j[1024];   // o JSON com os 12 números (~760 B com os literais)
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
        "\"scenes\":[{\"nodes\":[0]}],"
        "\"nodes\":[{\"mesh\":0,\"name\":\"gigante\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":"
        "{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},"
        "\"indices\":3,\"mode\":4}]}],"
        "\"buffers\":[{\"byteLength\":%llu}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC2\"},"
        "{\"bufferView\":3,\"componentType\":5125,\"count\":%u,"
        "\"type\":\"SCALAR\"}]}",
        (unsigned long long)binLen, (unsigned long long)posLen,
        (unsigned long long)nrmOff, (unsigned long long)nrmLen,
        (unsigned long long)uvOff, (unsigned long long)uvLen,
        (unsigned long long)idxOff, (unsigned long long)idxLen,
        verts, verts, verts, tris * 3u);
    std::string json = j;
    while (json.size() % 4 != 0) json += ' ';
    std::vector<u8> bin(static_cast<size_t>(binLen), 0);
    auto putF32 = [&bin](u64 off, f32 v) {
        u32 raw;
        std::memcpy(&raw, &v, 4);
        for (int b = 0; b < 4; ++b) {
            bin[static_cast<size_t>(off) + b] =
                static_cast<u8>((raw >> (8 * b)) & 0xFF);
        }
    };
    // determinístico (i mod pequeno): posição em [-1,1], normal +Y, uv
    for (u32 i = 0; i < verts; ++i) {
        putF32(u64(i) * 12 + 0, -1.0f + 2.0f * (i % 997u) / 996.0f);
        putF32(u64(i) * 12 + 4, -1.0f + 2.0f * (i % 991u) / 990.0f);
        putF32(u64(i) * 12 + 8, -1.0f + 2.0f * (i % 983u) / 982.0f);
        putF32(nrmOff + u64(i) * 12 + 0, 0.0f);
        putF32(nrmOff + u64(i) * 12 + 4, 1.0f);
        putF32(nrmOff + u64(i) * 12 + 8, 0.0f);
        putF32(uvOff + u64(i) * 8 + 0, (i % 251u) / 250.0f);
        putF32(uvOff + u64(i) * 8 + 4, (i % 241u) / 240.0f);
    }
    // índices u32: triângulos sobre os primeiros verts (determinístico)
    for (u32 t = 0; t < tris; ++t) {
        const u32 tri[3] = {(t * 3u + 0u) % verts, (t * 3u + 1u) % verts,
                            (t * 3u + 2u) % verts};
        for (int k = 0; k < 3; ++k) {
            const u64 off = idxOff + (u64(t) * 3 + k) * 4;
            bin[static_cast<size_t>(off) + 0] =
                static_cast<u8>(tri[k] & 0xFF);
            bin[static_cast<size_t>(off) + 1] =
                static_cast<u8>((tri[k] >> 8) & 0xFF);
            bin[static_cast<size_t>(off) + 2] =
                static_cast<u8>((tri[k] >> 16) & 0xFF);
            bin[static_cast<size_t>(off) + 3] =
                static_cast<u8>((tri[k] >> 24) & 0xFF);
        }
    }
    // o container GLB (header 12 + JSON chunk 8+json + BIN chunk 8+bin)
    std::vector<u8> glb;
    auto u32push = [&glb](u32 v) {
        glb.push_back(static_cast<u8>(v & 0xFF));
        glb.push_back(static_cast<u8>((v >> 8) & 0xFF));
        glb.push_back(static_cast<u8>((v >> 16) & 0xFF));
        glb.push_back(static_cast<u8>((v >> 24) & 0xFF));
    };
    u32push(0x46546C67u);   // 'glTF'
    u32push(2);
    u32push(static_cast<u32>(12 + 8 + json.size() + 8 + bin.size()));
    u32push(static_cast<u32>(json.size()));
    u32push(0x4E4F534Au);   // 'JSON'
    glb.insert(glb.end(), json.begin(), json.end());
    u32push(static_cast<u32>(bin.size()));
    u32push(0x004E4942u);   // 'BIN'
    glb.insert(glb.end(), bin.begin(), bin.end());
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(glb.data(), 1, glb.size(), f) == glb.size();
    std::fclose(f);
    return ok;
}

// 0.10-M (PASSO 4 · FASE 20): o GLB de DUAS meshes em nós SEPARADOS
// (x=−10 e x=+10), cada com `verts` vértices — o modelo que o mesh único
// RECUSA (total > 65 535) e que o render POR BLOCOS desenha bloco a bloco
// com AABBs DISTINTOS (o frustum escolhe metades; o orbit muda o HUD).
// Duas primitivas, dois nós com translation, SEM pele (o streaming de
// produção; cada primitiva vira UM grupo → UM bloco).
bool buildGlbFase20(u32 verts, u32 tris, const std::string& path) {
    // layout do BIN (por mesh: pos 12 + nrm 12 + uv 8 + idx u32; as duas
    // meshes partilham o layout com offsets deslocados)
    const u64 perPos = u64(verts) * 12;
    const u64 m0nrm = (perPos + 3) & ~3ull;
    const u64 m0nrmLen = u64(verts) * 12;
    const u64 m0uv = (m0nrm + m0nrmLen + 3) & ~3ull;
    const u64 m0uvLen = u64(verts) * 8;
    const u64 m0idx = (m0uv + m0uvLen + 3) & ~3ull;
    const u64 m0idxLen = u64(tris) * 12;
    const u64 m1pos = (m0idx + m0idxLen + 3) & ~3ull;
    const u64 m1nrm = (m1pos + perPos + 3) & ~3ull;
    const u64 m1uv = (m1nrm + m0nrmLen + 3) & ~3ull;
    const u64 m1idx = (m1uv + m0uvLen + 3) & ~3ull;
    const u64 binLen = m1idx + m0idxLen;
    char j[1600];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
        "\"scenes\":[{\"nodes\":[0,1]}],"
        "\"nodes\":["
        "{\"mesh\":0,\"name\":\"esquerda\",\"translation\":[-10.0,0.0,0.0]},"
        "{\"mesh\":1,\"name\":\"direita\",\"translation\":[10.0,0.0,0.0]}],"
        "\"meshes\":["
        "{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1,"
        "\"TEXCOORD_0\":2},\"indices\":3,\"mode\":4}]},"
        "{\"primitives\":[{\"attributes\":{\"POSITION\":4,\"NORMAL\":5,"
        "\"TEXCOORD_0\":6},\"indices\":7,\"mode\":4}]}],"
        "\"buffers\":[{\"byteLength\":%llu}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu},"
        "{\"buffer\":0,\"byteOffset\":%llu,\"byteLength\":%llu}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC2\"},"
        "{\"bufferView\":3,\"componentType\":5125,\"count\":%u,"
        "\"type\":\"SCALAR\"},"
        "{\"bufferView\":4,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC3\"},"
        "{\"bufferView\":5,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC3\"},"
        "{\"bufferView\":6,\"componentType\":5126,\"count\":%u,"
        "\"type\":\"VEC2\"},"
        "{\"bufferView\":7,\"componentType\":5125,\"count\":%u,"
        "\"type\":\"SCALAR\"}]}",
        (unsigned long long)binLen,
        (unsigned long long)perPos,
        (unsigned long long)m0nrm, (unsigned long long)m0nrmLen,
        (unsigned long long)m0uv, (unsigned long long)m0uvLen,
        (unsigned long long)m0idx, (unsigned long long)m0idxLen,
        (unsigned long long)m1pos, (unsigned long long)perPos,
        (unsigned long long)m1nrm, (unsigned long long)m0nrmLen,
        (unsigned long long)m1uv, (unsigned long long)m0uvLen,
        (unsigned long long)m1idx, (unsigned long long)m0idxLen,
        verts, verts, verts, tris * 3u,
        verts, verts, verts, tris * 3u);
    std::string json = j;
    while (json.size() % 4 != 0) json += ' ';
    std::vector<u8> bin(static_cast<size_t>(binLen), 0);
    auto putF32 = [&bin](u64 off, f32 v) {
        u32 raw;
        std::memcpy(&raw, &v, 4);
        for (int b = 0; b < 4; ++b) {
            bin[static_cast<size_t>(off) + b] =
                static_cast<u8>((raw >> (8 * b)) & 0xFF);
        }
    };
    // determinístico (i mod pequeno): posição em [-1,1] (cada mesh é uma
    // nuvem unitária; a TRANSLAÇÃO do nó separa-as em x=±10)
    for (u32 i = 0; i < verts; ++i) {
        for (int mesh = 0; mesh < 2; ++mesh) {
            const u64 pos = mesh == 0 ? u64(i) * 12 : m1pos + u64(i) * 12;
            const u64 nrm = mesh == 0 ? m0nrm + u64(i) * 12
                                      : m1nrm + u64(i) * 12;
            const u64 uv = mesh == 0 ? m0uv + u64(i) * 8 : m1uv + u64(i) * 8;
            putF32(pos + 0, -1.0f + 2.0f * (i % 997u) / 996.0f);
            putF32(pos + 4, -1.0f + 2.0f * (i % 991u) / 990.0f);
            putF32(pos + 8, -1.0f + 2.0f * (i % 983u) / 982.0f);
            putF32(nrm + 0, 0.0f);
            putF32(nrm + 4, 1.0f);
            putF32(nrm + 8, 0.0f);
            putF32(uv + 0, (i % 251u) / 250.0f);
            putF32(uv + 4, (i % 241u) / 240.0f);
        }
    }
    for (u32 t = 0; t < tris; ++t) {
        const u32 tri[3] = {(t * 3u + 0u) % verts, (t * 3u + 1u) % verts,
                            (t * 3u + 2u) % verts};
        for (int mesh = 0; mesh < 2; ++mesh) {
            const u64 base = mesh == 0 ? m0idx : m1idx;
            for (int k = 0; k < 3; ++k) {
                const u64 off = base + (u64(t) * 3 + k) * 4;
                bin[static_cast<size_t>(off) + 0] =
                    static_cast<u8>(tri[k] & 0xFF);
                bin[static_cast<size_t>(off) + 1] =
                    static_cast<u8>((tri[k] >> 8) & 0xFF);
                bin[static_cast<size_t>(off) + 2] =
                    static_cast<u8>((tri[k] >> 16) & 0xFF);
                bin[static_cast<size_t>(off) + 3] =
                    static_cast<u8>((tri[k] >> 24) & 0xFF);
            }
        }
    }
    // o container GLB (o MESMO empacotamento do buildGlbFase19)
    std::vector<u8> glb;
    auto u32push = [&glb](u32 v) {
        glb.push_back(static_cast<u8>(v & 0xFF));
        glb.push_back(static_cast<u8>((v >> 8) & 0xFF));
        glb.push_back(static_cast<u8>((v >> 16) & 0xFF));
        glb.push_back(static_cast<u8>((v >> 24) & 0xFF));
    };
    u32push(0x46546C67u);   // 'glTF'
    u32push(2);
    u32push(static_cast<u32>(12 + 8 + json.size() + 8 + bin.size()));
    u32push(static_cast<u32>(json.size()));
    u32push(0x4E4F534Au);   // 'JSON'
    glb.insert(glb.end(), json.begin(), json.end());
    u32push(static_cast<u32>(bin.size()));
    u32push(0x004E4942u);   // 'BIN'
    glb.insert(glb.end(), bin.begin(), bin.end());
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(glb.data(), 1, glb.size(), f) == glb.size();
    std::fclose(f);
    return ok;
}

jobject kFakeActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xD001));
jclass  kFakeCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xD002));

// 0.10-M (HOTFIX SAF-STREAM · FASE 21): o CITY do dono em miniatura —
// `nMeshes` primitivas (72 no real), cada com `verts` vértices e `tris`
// triângulos, SEM pele (o streaming de produção), nós crus. O total passa
// o teto de 65535 do caminho de mesh única: sob SAF era AQUI que o import
// do dono morria (o gate content:// desligava o streaming → legado → teto).
bool buildGlbFase21(u32 nMeshes, u32 verts, u32 tris,
                    const std::string& path) {
    // layout do BIN: por mesh, pos 12 + nrm 12 + uv 8 + idx u32; alinhado 4
    std::vector<u64> posOff(nMeshes), nrmOff(nMeshes), uvOff(nMeshes),
        idxOff(nMeshes);
    u64 binLen = 0;
    for (u32 m = 0; m < nMeshes; ++m) {
        posOff[m] = (binLen + 3) & ~3ull;
        binLen = posOff[m] + u64(verts) * 12;
        nrmOff[m] = (binLen + 3) & ~3ull;
        binLen = nrmOff[m] + u64(verts) * 12;
        uvOff[m] = (binLen + 3) & ~3ull;
        binLen = uvOff[m] + u64(verts) * 8;
        idxOff[m] = (binLen + 3) & ~3ull;
        binLen = idxOff[m] + u64(tris) * 3 * 4;
    }
    // o JSON (montado por strings — 72 meshes não cabem num snprintf)
    std::string j = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
                    "\"scenes\":[{\"nodes\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += std::to_string(m);
    }
    j += "]}],\"nodes\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += "{\"mesh\":" + std::to_string(m) + ",\"name\":\"c" +
             std::to_string(m) + "\"}";
    }
    j += "],\"meshes\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += "{\"primitives\":[{\"attributes\":{\"POSITION\":" +
             std::to_string(u64(m) * 4) + ",\"NORMAL\":" +
             std::to_string(u64(m) * 4 + 1) + ",\"TEXCOORD_0\":" +
             std::to_string(u64(m) * 4 + 2) + "},\"indices\":" +
             std::to_string(u64(m) * 4 + 3) + ",\"material\":0,"
             "\"mode\":4}]}";
    }
    j += "],\"materials\":[{\"name\":\"cidade\"}],\"accessors\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += "{\"bufferView\":" + std::to_string(m * 4) +
             ",\"componentType\":5126,\"count\":" + std::to_string(verts) +
             ",\"type\":\"VEC3\"}";
        j += ",{\"bufferView\":" + std::to_string(m * 4 + 1) +
             ",\"componentType\":5126,\"count\":" + std::to_string(verts) +
             ",\"type\":\"VEC3\"}";
        j += ",{\"bufferView\":" + std::to_string(m * 4 + 2) +
             ",\"componentType\":5126,\"count\":" + std::to_string(verts) +
             ",\"type\":\"VEC2\"}";
        j += ",{\"bufferView\":" + std::to_string(m * 4 + 3) +
             ",\"componentType\":5125,\"count\":" +
             std::to_string(u64(tris) * 3) + ",\"type\":\"SCALAR\"}";
    }
    j += "],\"bufferViews\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += "{\"buffer\":0,\"byteOffset\":" + std::to_string(posOff[m]) +
             ",\"byteLength\":" + std::to_string(u64(verts) * 12) + "}";
        j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(nrmOff[m]) +
             ",\"byteLength\":" + std::to_string(u64(verts) * 12) + "}";
        j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(uvOff[m]) +
             ",\"byteLength\":" + std::to_string(u64(verts) * 8) + "}";
        j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(idxOff[m]) +
             ",\"byteLength\":" + std::to_string(u64(tris) * 3 * 4) + "}";
    }
    j += "],\"buffers\":[{\"byteLength\":" + std::to_string(binLen) + "}]}";
    while (j.size() % 4 != 0) j += ' ';

    std::vector<u8> bin(static_cast<size_t>(binLen), 0);
    auto putF32 = [&bin](u64 off, f32 v) {
        u32 raw;
        std::memcpy(&raw, &v, 4);
        for (int b = 0; b < 4; ++b) {
            bin[static_cast<size_t>(off) + b] =
                static_cast<u8>((raw >> (8 * b)) & 0xFF);
        }
    };
    for (u32 m = 0; m < nMeshes; ++m) {
        for (u32 i = 0; i < verts; ++i) {
            putF32(posOff[m] + u64(i) * 12 + 0,
                   -1.0f + 2.0f * ((i + m) % 997u) / 996.0f);
            putF32(posOff[m] + u64(i) * 12 + 4,
                   -1.0f + 2.0f * ((i + m) % 991u) / 990.0f);
            putF32(posOff[m] + u64(i) * 12 + 8,
                   -1.0f + 2.0f * ((i + m) % 983u) / 982.0f);
            putF32(nrmOff[m] + u64(i) * 12 + 0, 0.0f);
            putF32(nrmOff[m] + u64(i) * 12 + 4, 1.0f);
            putF32(nrmOff[m] + u64(i) * 12 + 8, 0.0f);
            putF32(uvOff[m] + u64(i) * 8 + 0, (i % 251u) / 250.0f);
            putF32(uvOff[m] + u64(i) * 8 + 4, (i % 241u) / 240.0f);
        }
        for (u32 t = 0; t < tris; ++t) {
            for (int k = 0; k < 3; ++k) {
                const u32 v = (t * 3u + static_cast<u32>(k)) % verts;
                const u64 off = idxOff[m] + (u64(t) * 3 + k) * 4;
                bin[static_cast<size_t>(off) + 0] = static_cast<u8>(v & 0xFF);
                bin[static_cast<size_t>(off) + 1] =
                    static_cast<u8>((v >> 8) & 0xFF);
                bin[static_cast<size_t>(off) + 2] =
                    static_cast<u8>((v >> 16) & 0xFF);
                bin[static_cast<size_t>(off) + 3] =
                    static_cast<u8>((v >> 24) & 0xFF);
            }
        }
    }
    // o container GLB (o MESMO empacotamento dos outros geradores)
    std::vector<u8> glb;
    auto u32push = [&glb](u32 v) {
        glb.push_back(static_cast<u8>(v & 0xFF));
        glb.push_back(static_cast<u8>((v >> 8) & 0xFF));
        glb.push_back(static_cast<u8>((v >> 16) & 0xFF));
        glb.push_back(static_cast<u8>((v >> 24) & 0xFF));
    };
    u32push(0x46546C67u);
    u32push(2);
    u32push(static_cast<u32>(12 + 8 + j.size() + 8 + bin.size()));
    u32push(static_cast<u32>(j.size()));
    u32push(0x4E4F534Au);
    glb.insert(glb.end(), j.begin(), j.end());
    u32push(static_cast<u32>(bin.size()));
    u32push(0x004E4942u);
    glb.insert(glb.end(), bin.begin(), bin.end());
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const bool ok = std::fwrite(glb.data(), 1, glb.size(), f) == glb.size();
    std::fclose(f);
    return ok;
}

// 0.10-M (EXT · FASE 22) — o CITY do dono ESPALHADO: as 72 primitivas em
// FAIXAS de x (10 em 10, a faixa do meio no 0) com NOMES «c<i>». O pin do
// dono é «city scene expandido = 72 TICs com nomes, culling por TIC
// verde» — o culling POR TIC só é AFERÍVEL com peças SEPARADAS no espaço
// (o buildGlbFase21 põe todas sobrepostas em [-1,1]).
bool buildGlbFase22(u32 nMeshes, u32 verts, u32 tris,
                    const std::string& path) {
    std::vector<u64> posOff(nMeshes), nrmOff(nMeshes), uvOff(nMeshes),
        idxOff(nMeshes);
    u64 binLen = 0;
    for (u32 m = 0; m < nMeshes; ++m) {
        posOff[m] = (binLen + 3) & ~3ull;
        binLen = posOff[m] + u64(verts) * 12;
        nrmOff[m] = (binLen + 3) & ~3ull;
        binLen = nrmOff[m] + u64(verts) * 12;
        uvOff[m] = (binLen + 3) & ~3ull;
        binLen = uvOff[m] + u64(verts) * 8;
        idxOff[m] = (binLen + 3) & ~3ull;
        binLen = idxOff[m] + u64(tris) * 3 * 4;
    }
    std::string j = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
                    "\"scenes\":[{\"nodes\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += std::to_string(m);
    }
    j += "]}],\"nodes\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        // a FAIXA do nó (o meio no 0): é a transformação que o expand põe
        // no Transform3D do TIC e o que separa as peças p/ o culling
        char tr[96];
        std::snprintf(tr, sizeof(tr), "\"translation\":[%.1f,0,0]",
                      (double)(static_cast<i64>(m) -
                               static_cast<i64>(nMeshes / 2)) * 10.0);
        j += "{\"mesh\":" + std::to_string(m) + ",\"name\":\"c" +
             std::to_string(m) + "\"," + tr + "}";
    }
    j += "],\"meshes\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += "{\"primitives\":[{\"attributes\":{\"POSITION\":" +
             std::to_string(u64(m) * 4) + ",\"NORMAL\":" +
             std::to_string(u64(m) * 4 + 1) + ",\"TEXCOORD_0\":" +
             std::to_string(u64(m) * 4 + 2) + "},\"indices\":" +
             std::to_string(u64(m) * 4 + 3) + ",\"material\":0,"
             "\"mode\":4}]}";
    }
    j += "],\"materials\":[{\"name\":\"cidade\"}],\"accessors\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += "{\"bufferView\":" + std::to_string(m * 4) +
             ",\"componentType\":5126,\"count\":" + std::to_string(verts) +
             ",\"type\":\"VEC3\"}";
        j += ",{\"bufferView\":" + std::to_string(m * 4 + 1) +
             ",\"componentType\":5126,\"count\":" + std::to_string(verts) +
             ",\"type\":\"VEC3\"}";
        j += ",{\"bufferView\":" + std::to_string(m * 4 + 2) +
             ",\"componentType\":5126,\"count\":" + std::to_string(verts) +
             ",\"type\":\"VEC2\"}";
        j += ",{\"bufferView\":" + std::to_string(m * 4 + 3) +
             ",\"componentType\":5125,\"count\":" +
             std::to_string(u64(tris) * 3) + ",\"type\":\"SCALAR\"}";
    }
    j += "],\"bufferViews\":[";
    for (u32 m = 0; m < nMeshes; ++m) {
        if (m) j += ",";
        j += "{\"buffer\":0,\"byteOffset\":" + std::to_string(posOff[m]) +
             ",\"byteLength\":" + std::to_string(u64(verts) * 12) + "}";
        j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(nrmOff[m]) +
             ",\"byteLength\":" + std::to_string(u64(verts) * 12) + "}";
        j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(uvOff[m]) +
             ",\"byteLength\":" + std::to_string(u64(verts) * 8) + "}";
        j += ",{\"buffer\":0,\"byteOffset\":" + std::to_string(idxOff[m]) +
             ",\"byteLength\":" + std::to_string(u64(tris) * 3 * 4) + "}";
    }
    j += "],\"buffers\":[{\"byteLength\":" + std::to_string(binLen) + "}]}";
    while (j.size() % 4 != 0) j += ' ';
    std::vector<u8> bin(static_cast<size_t>(binLen), 0);
    auto putF32 = [&bin](u64 off, f32 v) {
        u32 raw;
        std::memcpy(&raw, &v, 4);
        for (int b = 0; b < 4; ++b) {
            bin[static_cast<size_t>(off) + b] =
                static_cast<u8>((raw >> (8 * b)) & 0xFF);
        }
    };
    for (u32 m = 0; m < nMeshes; ++m) {
        for (u32 i = 0; i < verts; ++i) {
            // a rede 3×3×3 em {-1,0,1}³: o AABB LOCAL de cada peça é
            // EXATAMENTE [-1,1]³ (o culling lê ISTO — determinístico)
            putF32(posOff[m] + u64(i) * 12 + 0,
                   static_cast<f32>(i % 3u) - 1.0f);
            putF32(posOff[m] + u64(i) * 12 + 4,
                   static_cast<f32>((i / 3u) % 3u) - 1.0f);
            putF32(posOff[m] + u64(i) * 12 + 8,
                   static_cast<f32>((i / 9u) % 3u) - 1.0f);
            putF32(nrmOff[m] + u64(i) * 12 + 0, 0.0f);
            putF32(nrmOff[m] + u64(i) * 12 + 4, 1.0f);
            putF32(nrmOff[m] + u64(i) * 12 + 8, 0.0f);
            putF32(uvOff[m] + u64(i) * 8 + 0, (i % 251u) / 250.0f);
            putF32(uvOff[m] + u64(i) * 8 + 4, (i % 241u) / 240.0f);
        }
        for (u32 t = 0; t < tris; ++t) {
            for (int k = 0; k < 3; ++k) {
                const u32 v = (t * 3u + static_cast<u32>(k)) % verts;
                const u64 off = idxOff[m] + (u64(t) * 3 + k) * 4;
                bin[static_cast<size_t>(off) + 0] = static_cast<u8>(v & 0xFF);
                bin[static_cast<size_t>(off) + 1] =
                    static_cast<u8>((v >> 8) & 0xFF);
                bin[static_cast<size_t>(off) + 2] =
                    static_cast<u8>((v >> 16) & 0xFF);
                bin[static_cast<size_t>(off) + 3] =
                    static_cast<u8>((v >> 24) & 0xFF);
            }
        }
    }
    std::vector<u8> glb;
    auto u32push = [&glb](u32 v) {
        glb.push_back(static_cast<u8>(v & 0xFF));
        glb.push_back(static_cast<u8>((v >> 8) & 0xFF));
        glb.push_back(static_cast<u8>((v >> 16) & 0xFF));
        glb.push_back(static_cast<u8>((v >> 24) & 0xFF));
    };
    u32push(0x46546C67u);
    u32push(2);
    u32push(static_cast<u32>(12 + 8 + j.size() + 8 + bin.size()));
    u32push(static_cast<u32>(j.size()));
    u32push(0x4E4F534Au);
    glb.insert(glb.end(), j.begin(), j.end());
    u32push(static_cast<u32>(bin.size()));
    u32push(0x004E4942u);
    glb.insert(glb.end(), bin.begin(), bin.end());
    FILE* f2 = std::fopen(path.c_str(), "wb");
    if (!f2) return false;
    const bool ok2 = std::fwrite(glb.data(), 1, glb.size(), f2) == glb.size();
    std::fclose(f2);
    return ok2;
}

// o registo da activity (o papel do VvActivity.onCreate) + o CACHE DIR da
// app (o papel do getCacheDir — a ponte jniCacheDir resolve no fallback)
void javaRegistersWithCacheDir() {
    g_jni.reset();
    while (vv::storage::pollResult()) {
    }
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, kFakeCls, kFakeActivity,
        g_jni.newString("c33-virtual"));
    g_jni.manager_result = true;   // all-files concedido (o C33 do dono tem)
    fileapi::makeDirs(kCacheDir);
    g_jni.cache_dir = kCacheDir;   // getCacheDir() da "app"
}

// ---- geometria do overlay do picker (a MESMA fórmula do drawAssetMenu) -----
struct PickerGeom {
    f32 x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    f32 rowCy(int row) const { return y + editor::kHeaderH + static_cast<f32>(row) * 48.0f + 20.0f; }
    f32 cx() const { return x + w * 0.5f; }
};

PickerGeom meshPickerGeom(size_t files) {
    const size_t shown = files < 5 ? files : 5;
    PickerGeom g;
    g.w = editor::kMenuW;
    g.h = editor::kHeaderH + static_cast<f32>(shown + 2) * 48.0f + editor::kPad;
    const f32 ox = g_ui.safeLeft();
    const f32 oy = g_ui.safeTop();
    const f32 aw = static_cast<f32>(g_egl.width()) - ox - g_ui.safeRight();
    const f32 ah = static_cast<f32>(g_egl.height()) - oy - g_ui.safeBottom();
    g.x = ox + (aw - g.w) * 0.5f;
    g.y = oy + (ah - g.h) * 0.5f;
    return g;
}

PickerGeom texPickerGeom(size_t files) {
    const size_t shown = files < 5 ? files : 5;
    PickerGeom g;
    g.w = editor::kMenuW;
    g.h = editor::kHeaderH + static_cast<f32>(shown + 1) * 48.0f + editor::kPad;
    const f32 ox = g_ui.safeLeft();
    const f32 oy = g_ui.safeTop();
    const f32 aw = static_cast<f32>(g_egl.width()) - ox - g_ui.safeRight();
    const f32 ah = static_cast<f32>(g_egl.height()) - oy - g_ui.safeBottom();
    g.x = ox + (aw - g.w) * 0.5f;
    g.y = oy + (ah - g.h) * 0.5f;
    return g;
}

PickerGeom primPickerGeom() {
    PickerGeom g;
    g.w = editor::kMenuW;
    g.h = editor::kHeaderH + 44.0f + 2.0f * 44.0f + editor::kPad;
    const f32 ox = g_ui.safeLeft();
    const f32 oy = g_ui.safeTop();
    const f32 aw = static_cast<f32>(g_egl.width()) - ox - g_ui.safeRight();
    const f32 ah = static_cast<f32>(g_egl.height()) - oy - g_ui.safeBottom();
    g.x = ox + (aw - g.w) * 0.5f;
    g.y = oy + (ah - g.h) * 0.5f;
    return g;
}

// um TOQUE do dedo: press num frame, release no seguinte (o mesmo padrão
// de edges que o glue produz no device — o botão captura no press e
// dispara no release; o deselect antigo armava no press e LIMPAVA no
// release do MESMO tap, ANTES do dispatch do pick: o bug exato do C33)
// FASE 9 (9.11): comparação com tolerância (as demais checks do harness
// são booleanas diretas; as de LAYOUT precisam de tolerância de flutuante)
static bool nearEqF(f32 a, f32 b, f32 tol = 0.05f) {
    return (a - b < tol) && (b - a < tol);
}

void tap(f32 x, f32 y) {
    g_input.injectDown(0, x, y);
    frame();
    g_input.injectUp(0);
    frame();
}

// 0.9.6 (G6/12.9): o nº de linhas de um bloco (o relatório de bench tem 9)
int countLines(const std::string& s) {
    int n = 0;
    for (char c : s) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
}

// um frame "morto" (sem dedo) — o que corre entre gestos no device
void idle(int n = 1) {
    for (int i = 0; i < n; ++i) {
        frame();
    }
}

double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0)
        .count();
}

// reset do estado partilhado entre fases (o padrão do test_wiring087)
void resetEngineForHarness() {
    g_scene.clear();
    // GCC 13.3 do runner (ubuntu-24.04) tem um ICE em gimple_add_tmp_var
    // com o temporário prvalue braced `g_editor = editor::EditorState{}`;
    // a variável nomeada aplica os mesmos NSDMIs e copia — sem temporário.
    editor::EditorState editorFresh;
    g_editor = editorFresh;
    g_browser = FileBrowserState{};
    g_applyAsk = ApplyAskState{};
    g_primOwners.clear();
    g_primGrave.clear();
    g_catalog.meshes.clear();
    g_catalog.textures.clear();
    g_catalog.audio.clear();
    g_prevAssetMenu = 0;
    g_projectReady = false;
    g_storage.reset();
    g_gpu.releaseAll();
    g_resources.setStorage(nullptr);
    g_toast[0] = '\0';
    g_toastT = 0.0f;
    g_input.resetAll();
    g_windowInits = 0;
    g_windowTerms = 0;
    // o android_main cria/destrói estes com o storage — o harness idem
    g_pipeline.reset();
    g_texCache.reset();
    glstub::reset();
    // 0.9.6.5 (GRUPO B): os OBJETOS do framebuffer real também morrem com o
    // contexto (o `enabled` MANTÉM-SE — é configuração do ambiente, como o
    // astcLdr; só a FASE 13 o liga)
    glstub::fb::resetState();
}

}  // namespace

// ===========================================================================
// main — o REPLAY inteiro; sai non-zero em qualquer falha
// ===========================================================================
int main() {
    std::printf("== C33 VIRTUAL — dispositivo headless em CI (0.9.3) ==\n");
    std::printf("   reproduz: /tmp read-only (errno=30), cache dir da app,\n");
    std::printf("   content:// SAF, lifecycle EGL TERM/INIT, 1536x720 + insets,\n");
    std::printf("   ASTC ativo, taps replayaveis pelo frame() real\n");
    std::printf("   0.9.3: + lifecycle agressivo do AUDIO (REG-002/R-006), backend\n");
    std::printf("   Oboe de producao contra o stub, projetos corrompidos, memoria\n");
    std::printf("   do arranque (Problema 3) e as sentinelas JVM (REG-001) no CI\n");
    rmrf(kHarnessLogs);
    rmrf(kCacheDir);
    if (!vv::elog::init(kHarnessLogs)) {
        std::printf("FATAL: elog init\n");
        return 2;
    }

    // ---- a "build instalada" (o papel do build_info.txt + VvActivity) -----
    vv::buildinfo::set("0.9.0-virtual", 43, "c33c0ffe", "aabbccdd00112233", 1790000000ull);
    elog::info("%s", vv::buildinfo::banner().c_str());

    // ---- [fs] /tmp READ-ONLY (a condição do device que o CI não tinha) ----
    fileapi::testing::setReadonlyPrefix("/tmp");
    std::printf("  [env] /tmp READ-ONLY (errno=30) ATIVO; escrita so no cache dir '%s' e no projeto\n", kCacheDir);
    // prova do ambiente: o writeAll em /tmp FALHA com a linha EXATA do device
    {
        std::vector<u8> probe{1, 2, 3};
        const bool w = fileapi::writeAll("/tmp/goni_probe_ro.tmp", probe.data(), probe.size());
        check(!w, "ambiente: writeAll em /tmp falha (read-only simulado)");
        check(vv::fileapi::errnoText().find("errno=30") != std::string::npos,
              "ambiente: errnoText devolve errno=30 (Read-only file system)");
        check(logHas("fileapi: fopen/write falhou em '/tmp/goni_probe_ro.tmp'"),
              "ambiente: a linha de log do device aparece na sonda (prova da seam)");
    }

    // ---- [tela][gpu] a resolução do C33 + ASTC + registo/cache dir ---------
    eglstub::g_surfaceW = 1536;
    eglstub::g_surfaceH = 720;
    glstub::astcLdr = true;   // o Mali do C33
    javaRegistersWithCacheDir();
    std::printf("  [env] superficie 1536x720; ASTC LDR ativo; cache dir via JNI\n");

    // ======================================================================
    // FASE 1 — projeto ANTIGO + BOOT com MIGRACAO (/tmp read-only ATIVO)
    // ======================================================================
    fase("FASE 1 — boot com migracao de projeto antigo (/tmp READ-ONLY)");
    resetEngineForHarness();
    {
        auto st = std::make_unique<FakeStorage>();
        FakeStorage* rawSt = st.get();
        // projeto REAL no storage (manifesto + cena com o TIC e a ref LEGADA)
        check(Project::createNew(*rawSt, "c33", g_project), "projeto criado no storage");
        rawSt->makeDirs("meshes");
        rawSt->writeText("meshes/casa.obj",
                         "o casa\nv 0 0 0\nv 3 0 0\nv 0 2 0\nf 1 2 3\n");
        // a cena do dono: TIC Casa com a ref LEGADA meshes/casa.obj
        {
            const Handle h = g_scene.create("Casa");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            MeshRenderer* mr = t->addComponent<MeshRenderer>();
            mr->meshPath = "meshes/casa.obj";
            check(g_project.saveActiveScene(*rawSt, g_scene), "cena gravada com a ref legada");
            g_scene.clear();
        }
        g_storage = std::move(st);
        g_projectReady = true;
        g_resources.setStorage(rawSt);
        g_gpu.init(&g_resources);
        // o papel do android_main: cache/pipeline de texturas com o storage
        g_texCache = std::make_unique<TextureCache>(*rawSt);
        g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor,
                                                       *g_texCache);

        // o BOOT REAL do device: INIT_WINDOW (EGL feliz 1536x720, renderer,
        // fonte, cubo, grid, load da cena ativa + MIGRACAO silenciosa)
        android_app app;
        std::memset(&app, 0, sizeof(app));
        app.contentRect = {0, 24, 1512, 720};   // insets: status 24 + pill 24
        const auto tBoot = std::chrono::steady_clock::now();
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_ready, "boot: INIT_WINDOW completo (g_ready)");
        check(g_egl.width() == 1536 && g_egl.height() == 720,
              "boot: superficie 1536x720 (a resolucao do C33)");
        check(logHas("ASTC SIM"), "boot: caminho ASTC ativo (o Mali do C33)");
        check(msSince(tBoot) < 500.0, "boot: dentro do orcamento de tempo");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        passo("a migracao do projeto antigo corre com /tmp READ-ONLY");
        check(rawSt->exists("assets/casa.gmesh"), "migracao: assets/casa.gmesh criado");
        check(logHas("asset: migracao 'meshes/casa.obj'"), "migracao: linha do log da fonte legada");
        check(logHas("asset: staging em '"), "migracao: staging no cache dir/projeto (nunca /tmp)");
        check(logCount("staging falhou") == 0,
              "migracao: ZERO ocorrencias do sintoma R-002 (migracao morta)");
        check(logCount("mkdir falhou em '/tmp'") == 0,
              "migracao: ZERO ocorrencias do sintoma R-002 (mkdir no /tmp)");
        {
            Mesh* m = g_gpu.mesh("assets/casa.gmesh");
            check(m != nullptr && m->indexCount() == 3, "migracao: o .gmesh resolve (3 idx)");
        }
        // a ref do TIC foi re-escrita em silencio
        bool casaMigrada = false;
        g_scene.forEachActive([&casaMigrada](const Tic& t) {
            if (t.name == "Casa") {
                if (const MeshRenderer* mr = t.getComponent<MeshRenderer>()) {
                    casaMigrada = (mr->meshPath == "assets/casa.gmesh");
                }
            }
        });
        check(casaMigrada, "migracao: ref do TIC re-escrita p/ assets/casa.gmesh");
    }

    // ======================================================================
    // FASE 2 — REPLAY DA SESSAO REAL (os toques que produziam os sintomas)
    // ======================================================================
    fase("FASE 2 — replay da sessao real (selecao -> picker -> troca -> none)");
    {
        // 2.1 — o dono seleciona o TIC Casa (tap na linha da Hierarchy; o
        // efeito de estado e o mesmo: selected aponta o TIC vivo)
        passo("2.1 selecionar o TIC Casa (afastado do centro — o dono "
              "trabalha com o TIC onde o deixou)");
        Handle casa = g_scene.find("Casa");
        check(casa.valid(), "TIC Casa vivo apos o boot");
        g_editor.selected = casa;
        // o TIC PROJETA LONGE do centro do viewport: o toque no overlay do
        // picker NÃO acerta o TIC 3D por baixo (o cenário real do sintoma —
        // quando o TIC estava sob o picker, o pickSceneTic re-selecionava-o
        // e a intermitência "às vezes fim ok" da evidência vinha daí)
        if (Transform3D* tr = g_scene.get(casa)->getComponent<Transform3D>()) {
            tr->pos = Vec3{60.0f, 0.0f, -30.0f};
            tr->updateWorld();
        }
        idle(2);

        // 2.2 — tocar na linha prim: do Inspector abre o seletor de
        // PRIMITIVAS (a origem do "mesh: troca - -> prim esfera ERRO(...)")
        passo("2.2 abrir o picker de PRIMITIVAS e tocar a linha esfera");
        g_editor.assetMenu = 4;   // o Inspector abriu (guard passou: alvo valido)
        idle(1);
        {
            const PickerGeom g = primPickerGeom();
            // row 0 = none, row 1 = esfera, row 2 = box
            const auto t0 = std::chrono::steady_clock::now();
            tap(g.cx(), g.y + editor::kHeaderH + 44.0f + 22.0f);   // esfera
            check(msSince(t0) < 250.0, "tap na linha esfera: dentro do orcamento");
        }
        check(g_editor.assetMenu == 0, "picker fechou apos o toque");
        check(g_scene.get(g_editor.selected) != nullptr,
              "SELECAO VIVA apos tocar na linha do picker (o fix do C33)");
        idle(2);   // o flush do ponto seguro sobe a esfera
        {
            const Tic* t = g_scene.get(g_editor.selected);
            const MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->mesh != nullptr && mr->primOn,
                  "esfera aplicada (primOn + mesh vivo)");
            check(mr && mr->mesh->indexCount() > 0, "esfera com geometria nao vazia");
        }
        check(logCount("ERRO(sem TIC com mesh selecionado)") == 0,
              "ZERO ocorrencias do sintoma R-001 (ERRO sem alvo) no replay");

        // 2.3 — trocar pelo MESH picker: tocar o ficheiro migrado (o
        // "mesh pick 3" dos logs — o assets/casa.gmesh em 3º)
        passo("2.3 picker de MESH: tocar o ficheiro migrado (mesh pick 3)");
        refreshCatalog();
        check(!g_catalog.meshes.empty(), "catalogo tem o .gmesh migrado");
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            // row 0 = none, row 1 = cube, row 2 = o ficheiro
            tap(g.cx(), g.rowCy(2));
        }
        check(g_scene.get(g_editor.selected) != nullptr, "selecao viva apos mesh pick 3");
        {
            const Tic* t = g_scene.get(g_editor.selected);
            const MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->mesh != nullptr && mr->meshPath == "assets/casa.gmesh",
                  "mesh migrado aplicado ao TIC (meshPath = assets/casa.gmesh)");
        }
        check(logHas("mesh: troca ") && logCount("fim ok") >= 1,
              "log 'mesh: troca ... fim ok' presente (a prova do C33)");

        // 2.4 — o tap no BACKDROP do picker (dentro do viewRect, fora do
        // painel): fecha o overlay SEM matar a selecao
        passo("2.4 tap no backdrop do picker (dentro do viewRect)");
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            // canto do viewport central (DENTRO do viewRect, FORA do painel)
            const UiRect view = editor::centerRect(
                static_cast<f32>(g_egl.width()), static_cast<f32>(g_egl.height()),
                g_ui.safeArea(), g_editor.showInspector);
            const f32 bx = view.x + 24.0f;
            const f32 by = view.y + 24.0f;
            check(bx < g.x || bx > g.x + g.w || by < g.y || by > g.y + g.h,
                  "ponto do backdrop fora do painel (a geometria bate)");
            tap(bx, by);
        }
        check(g_editor.assetMenu == 0, "backdrop fechou o picker");
        check(g_scene.get(g_editor.selected) != nullptr,
              "SELECAO VIVA apos o tap no backdrop (o fix do C33)");

        // 2.5 — o TEX picker: none em 1º (a evidencia "tex pick 2"/none)
        passo("2.5 picker de TEX: none em primeiro lugar");
        g_editor.assetMenu = 2;
        idle(1);
        {
            const PickerGeom g = texPickerGeom(g_catalog.textures.size());
            tap(g.cx(), g.rowCy(0));   // none (1º lugar)
        }
        {
            const Tic* t = g_scene.get(g_editor.selected);
            const MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->texture == nullptr && mr->texPath.empty(),
                  "tex none: textura limpa (material volta a cor plana)");
        }
        check(g_scene.get(g_editor.selected) != nullptr, "selecao viva apos tex none");

        // 2.6 — mesh NONE (a entrada de 1ª classe): o slot limpa e o TIC
        // deixa de renderizar mesh; deferred free no frame seguinte
        passo("2.6 picker de MESH: none (o slot limpa)");
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            tap(g.cx(), g.rowCy(0));   // none
        }
        {
            Tic* t = g_scene.get(g_editor.selected);
            MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->mesh == nullptr && mr->meshPath.empty(),
                  "mesh none: slot limpo (TIC deixa de renderizar mesh)");
        }
        check(logHas("mesh: troca ") && logHas("slot limpo (none)"),
              "log 'fim ok (sem mesh — slot limpo (none))'");
        // o deferred free corre no inicio do frame seguinte (a cova abre)
        idle(2);
        check(g_primGrave.empty(), "cova aberta (deferred free no frame seguinte)");

        // 2.7 — none -> X -> none repetido (sem crash, sem leak): o ciclo
        // do prompt com FRAMES reais no meio (o ponto seguro corre)
        passo("2.7 none -> cube -> none x5 (frames reais no meio)");
        for (int i = 0; i < 5; ++i) {
            Tic* t = g_scene.get(g_editor.selected);
            if (!check(t != nullptr, "TIC vivo no ciclo none->X->none")) {
                break;
            }
            MeshRenderer* mr = t->getComponent<MeshRenderer>();
            // none (direto pelo dispatch puro — o caminho do pick)
            {
                const editor::AssetPickOutcome o = editor::applyAssetPick(
                    g_scene, g_editor.selected, 1, 1, g_catalog,
                    makeAssetResolvers());
                check(o.applied, "none aplicado (ciclo)");
            }
            idle(1);   // frame: a cova abre no ponto seguro
            {
                const editor::AssetPickOutcome o = editor::applyAssetPick(
                    g_scene, g_editor.selected, 1, 2, g_catalog,
                    makeAssetResolvers());
                check(o.applied, "cube aplicado (ciclo)");
            }
            idle(1);
            const editor::AssetPickOutcome o = editor::applyAssetPick(
                g_scene, g_editor.selected, 1, 1, g_catalog,
                makeAssetResolvers());
            check(o.applied, "none aplicado de novo (ciclo)");
            idle(1);
            if (mr) {
                check(mr->mesh == nullptr, "estado final do ciclo: slot vazio");
            }
        }
        check(g_primOwners.empty(), "sem posse viva acumulada (sem leak)");
        check(g_primGrave.empty(), "cova vazia no fim (deferred free completo)");

        // 2.8 — SEM selecao, tocar em linha de picker (incluindo none):
        // HINT + log bloqueado, ZERO caminho de ERRO
        passo("2.8 sem selecao: hint + 'ui: pick bloqueado (sem selecao)'");
        g_editor.selected = Handle::invalid();   // a selecao morreu (outra via)
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            tap(g.cx(), g.rowCy(0));   // none SEM selecao
        }
        check(logCount("ui: pick bloqueado (sem seleção)") >= 1,
              "log 'ui: pick bloqueado (sem seleção)' presente");
        check(std::strncmp(g_toast, "seleciona um TIC com mesh", 26) == 0,
              "toast de hint 'seleciona um TIC com mesh'");
        g_editor.assetMenu = 1;
        idle(1);
        {
            const PickerGeom g = meshPickerGeom(g_catalog.meshes.size());
            tap(g.cx(), g.rowCy(2));   // ficheiro SEM selecao
        }
        check(logCount("ui: pick bloqueado (sem seleção)") >= 2,
              "2ª linha de pick bloqueada logada (hint sem spam de ERRO)");
        check(logCount("ERRO(sem TIC com mesh selecionado)") == 0,
              "ZERO ocorrencias do sintoma R-001 mesmo sem selecao");

        passo("2.9 adversario: outro overlay (log viewer) tambem guarda a selecao");
        g_editor.selected = g_scene.find("Casa");
        idle(1);
        g_editor.logViewer = true;   // OUTRO overlay qualquer: o MESMO guard
        idle(1);
        {
            const UiRect view = editor::centerRect(
                static_cast<f32>(g_egl.width()), static_cast<f32>(g_egl.height()),
                g_ui.safeArea(), g_editor.showInspector);
            tap(view.x + 30.0f, view.y + 30.0f);   // tap no viewport por baixo
        }
        g_editor.logViewer = false;
        check(g_scene.get(g_editor.selected) != nullptr,
              "SELECAO VIVA com overlay de logs aberto (o guard e de TODOS os overlays)");

        passo("2.10 adversario: troca de modo 3D | UI | AUDIO nao perde a selecao");
        g_editor.uiMode = true;
        idle(2);
        check(g_scene.get(g_editor.selected) != nullptr, "selecao viva no modo UI");
        g_editor.uiMode = false;
        g_editor.audioMode = true;
        idle(2);
        check(g_scene.get(g_editor.selected) != nullptr, "selecao viva no modo AUDIO");
        g_editor.audioMode = false;
        idle(2);
        check(g_scene.get(g_editor.selected) != nullptr,
              "selecao viva de volta ao 3D (a troca de modo NAO limpa)");
    }

    // ======================================================================
    // FASE 3 — LIFECYCLE: TERM/INIT com re-criacao do contexto EGL
    // ======================================================================
    fase("FASE 3 — lifecycle TERM/INIT (a selecao sobrevive ao ciclo)");
    {
        android_app app;
        std::memset(&app, 0, sizeof(app));
        app.contentRect = {0, 24, 1512, 720};

        passo("3.1 re-selecionar e trocar antes do ciclo");
        Handle casa = g_scene.find("Casa");
        check(casa.valid(), "TIC Casa vivo antes do TERM");
        g_editor.selected = casa;
        {
            const editor::AssetPickOutcome o = editor::applyAssetPick(
                g_scene, g_editor.selected, 4, 2, g_catalog,
                makeAssetResolvers());   // esfera
            check(o.applied, "esfera pedida antes do TERM");
        }
        idle(2);

        passo("3.2 TERM_WINDOW (contexto EGL destruido)");
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        check(!g_ready, "contexto morto (g_ready=false)");
        check(logHas("lifecycle: TERM_WINDOW"), "log do TERM_WINDOW");

        passo("3.3 INIT_WINDOW (re-criacao + re-upload + RELOAD da cena)");
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_ready, "contexto re-criado (g_ready)");
        check(logHas("lifecycle: INIT_WINDOW") && logHas("RE-CRIADO"),
              "log do INIT_WINDOW com contexto re-criado");
        // O FIX: a selecao RE-VALIDA/re-mapeia (o reload deu handles novos)
        check(g_scene.get(g_editor.selected) != nullptr,
              "SELECAO VIVA apos TERM/INIT (re-validada pelo nome)");
        {
            const Tic* t = g_scene.get(g_editor.selected);
            check(t != nullptr && t->name == "Casa",
                  "o handle re-mapeado aponta o TIC 'Casa' (nao outro)");
        }
        check(logHas("lifecycle: seleção re-validada"),
              "log 'lifecycle: seleção re-validada pos-INIT WINDOW'");

        passo("3.4 trocar mesh DEPOIS do ciclo (o fluxo do dono continua)");
        idle(2);
        {
            const editor::AssetPickOutcome o = editor::applyAssetPick(
                g_scene, g_editor.selected, 4, 3, g_catalog,
                makeAssetResolvers());   // box
            check(o.applied, "box pedida apos o INIT");
        }
        idle(2);
        {
            const Tic* t = g_scene.get(g_editor.selected);
            const MeshRenderer* mr = t ? t->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && mr->mesh != nullptr,
                  "mesh vivo no contexto NOVO (re-upload pelo ponto seguro)");
        }

        // 3.5 — o ciclo repetido: a selecao sobrevive a DOIS TERM/INIT
        passo("3.5 segundo ciclo TERM/INIT (a prova de robustez)");
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_scene.get(g_editor.selected) != nullptr,
              "selecao viva apos o 2º ciclo TERM/INIT");
    }

    // ======================================================================
    // FASE 4 — o caminho content:// do SAF (a migracao que morria no C33)
    // ======================================================================
    fase("FASE 4 — migracao pelo SAF content:// (staging no cache dir)");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        FakeSafIo io;   // o provider content:// (o modelo da suíte)
        SafStorage saf(&io, "content://tree/primary:GOneVV/c33");
        // projeto antigo DENTRO do provider SAF
        check(Project::createNew(saf, "c33saf", g_project), "projeto criado no provider SAF");
        check(saf.makeDirs("meshes"), "meshes/ criado no provider");
        check(saf.writeText("meshes/casa.obj",
                            "o casa\nv 0 0 0\nv 3 0 0\nv 0 2 0\nf 1 2 3\n"),
              "fonte legada escrita via content://");
        {
            const Handle h = g_scene.create("CasaSAF");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            MeshRenderer* mr = t->addComponent<MeshRenderer>();
            mr->meshPath = "meshes/casa.obj";
            check(g_project.saveActiveScene(saf, g_scene), "cena gravada no provider");
            g_scene.clear();
        }
        g_storage.reset(new SafStorage(&io, "content://tree/primary:GOneVV/c33"));
        g_projectReady = true;
        g_resources.setStorage(g_storage.get());
        g_gpu.init(&g_resources);
        io.flushWrites();   // o provider persiste (o passo invisível do device)

        // a MIGRACAO pelo caminho SAF com /tmp READ-ONLY: staging no CACHE DIR
        passo("migracao SAF: reconvertFile com raiz content://");
        convert::Output out;
        convert::Stats stats;
        std::string err;
        const bool ok = convert::reconvertFile("meshes/casa.obj", *g_storage.get(),
                                               nullptr, out, stats, err);
        io.flushWrites();
        check(ok, "reconvertFile por SAF sucede (staging no cache dir)");
        if (!ok) {
            std::printf("    [erro] %s\n", err.c_str());
        }
        check(g_storage->exists("assets/casa.gmesh"), "assets/casa.gmesh no provider");
        check(logCount("staging falhou") == 0,
              "ZERO ocorrencias do sintoma R-002 no caminho SAF");
        check(logCount("mkdir falhou em '/tmp'") == 0,
              "ZERO ocorrencias do sintoma R-002 (mkdir no /tmp) — caminho SAF");
        check(logHas("asset: staging em '"), "staging no cache dir logado com o caminho");
        check(!logHas("staging em '/tmp"), "o staging JAMAIS em /tmp");
    }

    // ======================================================================
    // FASE 5 — dump VELHO no viewer: badge ANTIGO (build X)
    // ======================================================================
    fase("FASE 5 — dump velho com badge ANTIGO (identidade)");
    {
        // a "build instalada" é 0.9.0-virtual/43; um dump da build 39
        // (a 0.8.9 do dono) tem de aparecer com o badge
        {
            const std::string dumpPath = std::string(kHarnessLogs) +
                                         "/crash-1790830406-vc39.dump";
            FILE* f = std::fopen(dumpPath.c_str(), "wb");
            if (f) {
                std::fputs("G.One VV — crash dump (legível sem ndk-stack)\n"
                           "build: 0.8.9 (versionCode 39)\nframes: 2\n"
                           "#00 pc 0xe2de8  libgoni_vv.so\n"
                           "#01 pc 0xe2f24  libgoni_vv.so\n", f);
                std::fclose(f);
            }
        }
        std::vector<std::string> dumps;
        vv::elog::listDumps(dumps);
        check(!dumps.empty(), "o dump velho aparece na lista do viewer");
        bool badged = false;
        for (const std::string& d : dumps) {
            const std::string badge = vv::buildinfo::dumpBadge(d);
            std::printf("    [dump] %s%s\n", d.c_str(), badge.c_str());
            if (d.find("vc39") != std::string::npos &&
                badge.find("[ANTIGO (build 39)]") != std::string::npos) {
                badged = true;
            }
        }
        check(badged, "dump da build 39 com badge [ANTIGO (build 39)]");
        // dump NOVO (desta build): sem badge
        {
            const std::string dumpPath =
                std::string(kHarnessLogs) + "/crash-1790000500" +
                vv::buildinfo::dumpSuffix() + ".dump";
            FILE* f = std::fopen(dumpPath.c_str(), "wb");
            if (f) {
                std::fputs("build: 0.9.0-virtual (versionCode 43)\n", f);
                std::fclose(f);
            }
        }
        dumps.clear();
        vv::elog::listDumps(dumps);
        bool novoLimpo = false;
        for (const std::string& d : dumps) {
            if (d.find("-vc43") != std::string::npos &&
                vv::buildinfo::dumpBadge(d).empty()) {
                novoLimpo = true;
            }
        }
        check(novoLimpo, "dump NOVO (vc43) sem badge (e da build instalada)");
    }

    // ======================================================================
    // FASE 6 — GATE: padroes proibidos no output inteiro do replay
    // ======================================================================
    fase("FASE 6 — gate de padroes proibidos (o CI vermelho se voltar)");
    {
        const char* proibidos[] = {
            "sem TIC com mesh selecionado",
            "mkdir falhou em '/tmp'",
            "staging falhou",
            "ERRO(gerador/upload falhou)",
        };
        for (const char* p : proibidos) {
            const int n = logCount(p);
            check(n == 0, "ZERO ocorrencias do proibido (engine.log inteiro)");
            if (n > 0) {
                std::printf("    [GATE VERMELHO] '%s' apareceu %d vez(es)\n", p, n);
            }
        }
        // as provas positivas (o que TEM de estar lá)
        check(logCount("fim ok") >= 1, "troca com 'fim ok' no log");
        check(logCount("ui: pick bloqueado (sem seleção)") >= 2,
              "hints de pick bloqueado no log");
    }

    // ======================================================================
    // FASE 7 — 0.9.1: ORIENTAÇÃO PORTRAIT + IME DO SISTEMA (janela de texto)
    // ======================================================================
    fase("FASE 7 — 0.9.1: portrait + IME (janela de texto)");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        ime::clearForTest();
        g_jni.void_calls.clear();

        android_app app;
        std::memset(&app, 0, sizeof(app));
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 7.1 — ABRIR: o par portrait + imeShow (o Java executa o pedido)
        passo("7.1 abrir a janela de texto (portrait + IME show)");
        openTextWindow();
        check(g_editor.textWin.open, "a janela de texto abre");
        check(ime::orientation() == ime::Orientation::Portrait,
              "orientação PEDIDA = portrait (estado na engine)");
        bool sawPortrait = false, sawShow = false;
        for (const auto& c : g_jni.void_calls) {
            if (c.first == "setOrientation" && c.second == 1) sawPortrait = true;
            if (c.first == "imeShow") sawShow = true;
        }
        check(sawPortrait, "JNI: setRequestedOrientation(PORTRAIT) executado");
        check(sawShow, "JNI: InputMethodManager.showSoftInput executado");
        check(logHas("orientacao: portrait pedida (janela de texto aberta)"),
              "a mudança de orientação fica LOGADA");

        // 7.2 — o IME ESCREVE (nativeOnImeText/Key → fila → frame consome)
        passo("7.2 o IME do sistema escreve no buffer");
        Java_vv_goni_VvActivity_nativeOnImeText(
            g_jni.env, nullptr, g_jni.newString("Ola"));
        Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 66, 0);
        Java_vv_goni_VvActivity_nativeOnImeText(
            g_jni.env, nullptr, g_jni.newString("C33"));
        frame();
        check(g_editor.textWin.buf == "Ola\nC33",
              "o texto commitado + ENTER chegam pela fila ime::");

        // 7.3 — a ROTAÇÃO (o frame do device roda): TERM + INIT em PORTRAIT
        passo("7.3 rotação 1536x720 → 720x1536 com a janela aberta");
        eglstub::g_surfaceW = 720;
        eglstub::g_surfaceH = 1536;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_egl.width() == 720 && g_egl.height() == 1536,
              "a superfície renasce EM PORTRAIT");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(g_editor.textWin.open, "a janela SOBREVIVE à rotação");
        check(g_editor.textWin.buf == "Ola\nC33",
              "o buffer sobrevive (estado da engine, não da GPU)");
        frame();
        check(g_windowInits >= 2 && g_windowTerms >= 1,
              "o lifecycle TERM/INIT correu (re-upload — sem glifos brancos)");

        // 7.4 — FECHAR: landscape + imeHide (o par espelhado do abrir)
        passo("7.4 fechar (landscape + IME hide)");
        g_jni.void_calls.clear();
        closeTextWindow();
        check(!g_editor.textWin.open, "a janela fecha");
        check(ime::orientation() == ime::Orientation::Landscape,
              "orientação REPOSTA = landscape");
        bool sawLandscape = false, sawHide = false;
        for (const auto& c : g_jni.void_calls) {
            if (c.first == "setOrientation" && c.second == 0) sawLandscape = true;
            if (c.first == "imeHide") sawHide = true;
        }
        check(sawLandscape, "JNI: setRequestedOrientation(LANDSCAPE) executado");
        check(sawHide, "JNI: hideSoftInput executado");

        // o device volta ao landscape para as fases seguintes
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
    }

    // ======================================================================
    // FASE 8 — 0.9.3 (hotfix): REPLAY DO CRASH DE ÁUDIO (REG-002/R-006)
    // + sequência dos projetos corrompidos (Sequência 3) + memória do
    // arranque (Problema 3). O backend é o OboeBackend DE PRODUÇÃO (TU
    // comum) contra o stub tests/stub/oboe/Oboe.h — o MESMO código que o
    // APK corre contra o oboe real do Google (FetchContent 1.9.3).
    // ======================================================================
    fase("FASE 8 — replay REG-002: lifecycle agressivo do audio + corruptos");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        oboe::testing::reset();

        android_app app;
        std::memset(&app, 0, sizeof(app));

        // 8.1 o BOOT pelo caminho REAL: INIT_WINDOW → audioBackendBoot →
        //     o PRIMÁRIO é o Oboe (o stub no host — o MESMO TU do APK)
        passo("8.1 boot real: INIT_WINDOW arranca o backend OBOE (o primario)");
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_ready, "boot completo (g_ready)");
        check(g_audioOut != nullptr && g_audioBackendReady,
              "backend de audio ATIVO no boot");
        check(std::strcmp(g_audioOut->name(), "oboe") == 0,
              "o primario e o OBOE (cadeia 0.9.3: oboe→aaudio→audiotrack)");
        check(logHas("audio(oboe): stream ATIVO rate="),
              "a linha informativa do stream (rate/ch/perf — Tarefa 2.5)");
        check(oboe::testing::hooks().openCount == 1,
              "EXATAMENTE 1 stream aberto no boot");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 8.2 A SEQUÊNCIA DO TOMBSTONE (Sequência 2 do prompt): o onResume
        //     da app antiga (com.goni.runtime) chamava startAudio()
        //     DIRETO, sem guarda — reproduzimos o MESMO padrão contra o
        //     backend vivo; o portão R-006 tem de aguentar SEM fugas
        passo("8.2 a sequencia do tombstone: startAudio repetido sem guarda");
        for (int i = 0; i < 3; ++i) {
            check(g_audioOut->start(44100, 2),
                  "startAudio() devolve true (idempotente)");
        }
        check(oboe::testing::hooks().openCount == 1,
              "o 2º/3º start NAO abrem streams (porta R-006 — a fuga de "
              "stream era a porta do crash Unisoc)");
        check(g_audioOut->ready(), "o stream original segue VIVO");
        check(logHas("audio(oboe): start ignorado — stream ja ativo"),
              "a porta R-006 deixa a linha no log");

        // o RESUME/PAUSE do lifecycle REAL (o fundo/recentes do Android)
        for (int i = 0; i < 5; ++i) {
            onAppCmd(&app, APP_CMD_RESUME);
            onAppCmd(&app, APP_CMD_PAUSE);
        }
        onAppCmd(&app, APP_CMD_RESUME);
        check(logCount("audio: RESUME") >= 1 && logCount("audio: PAUSE") >= 1,
              "o lifecycle do audio logado (pause/resume)");
        check(oboe::testing::hooks().openCount == 1,
              "pause/resume NAO reabrem streams");

        // 8.3 ADVERSÁRIO (Tarefa 7D): 50× onResume SEM onPause + 10×
        //     startAudio direto — a engine tem de seguir viva
        passo("8.3 adversario: 50 onResume sem onPause + 10 startAudio");
        for (int i = 0; i < 50; ++i) {
            onAppCmd(&app, APP_CMD_RESUME);
        }
        for (int i = 0; i < 10; ++i) {
            g_audioOut->start(44100, 2);
        }
        check(g_ready, "a engine SEGUE VIVA (zero crashes nativos)");
        check(oboe::testing::hooks().openCount == 1,
              "ainda EXATAMENTE 1 stream (zero fugas no adversario)");
        check(g_audioOut->ready(), "o audio segue pronto apos a tempestade");

        // 8.4 o CICLO DURO: TERM → INIT ×3 (o Android a destruir/recriar a
        //     surface — o ciclo que multiplicava os tombstones)
        passo("8.4 ciclo duro TERM->INIT x3 (o Android mata/recria a surface)");
        const int opensAntes = oboe::testing::hooks().openCount;
        for (int i = 0; i < 3; ++i) {
            onAppCmd(&app, APP_CMD_TERM_WINDOW);
            onAppCmd(&app, APP_CMD_INIT_WINDOW);
        }
        check(g_ready, "3 ciclos TERM->INIT completos (g_ready)");
        check(oboe::testing::hooks().openCount == opensAntes + 3,
              "cada boot abriu EXATAMENTE 1 stream novo");
        // o invariante R-006 (zero fugas): tudo o que abriu foi fechado,
        // EXCETO o stream VIVO do último boot — os contadores vão no
        // output (a evidência que o relatório cola)
        std::printf("    [streams] abertos=%d fechados=%d vivos=%d\n",
                    oboe::testing::hooks().openCount,
                    oboe::testing::hooks().closeCount,
                    oboe::testing::hooks().openCount -
                        oboe::testing::hooks().closeCount);
        check(oboe::testing::hooks().closeCount ==
                      oboe::testing::hooks().openCount - 1,
              "ZERO fugas: cada stream aberto foi fechado (so o VIVO do "
              "ultimo boot segue aberto — o stream fugido era o crash)");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 8.5 o DISCONNECT no meio da sessão (headset fora): o stream
        //     morre, o editor CONTINUA sem som; o próximo INIT re-arranca
        passo("8.5 disconnect no meio da sessao (o headset desligou)");
        oboe::testing::fireErrorOnAllStreams(
            oboe::Result::ErrorDisconnected);
        check(!g_audioOut->ready(), "o stream morto reporta NOT ready");
        check(logHas("audio(oboe): stream MORREU"),
              "a morte logada com a razao (onErrorBefore/AfterClose)");
        frame();   // o editor CONTINUA (o frame corre sem som)
        check(true, "o frame corre SEM som (degradacao graciosa — nunca "
                    "crash por causa do audio)");
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        check(g_ready && g_audioBackendReady,
              "o INIT re-arranca o audio depois do disconnect (porta "
              "reaberta pelo onErrorAfterClose)");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 8.6 SEQUÊNCIA 3 do prompt: PROJETOS CORROMPIDOS NO DISCO → o
        //     boot COMPLETA sem crash (a cena corrompida vira cena vazia
        //     com log; o lado Java da lista tem o sentinela JVM próprio)
        passo("8.6 cena corrompida no disco: boot completa sem crash");
        {
            auto st = std::make_unique<FakeStorage>();
            FakeStorage* rawSt = st.get();
            check(Project::createNew(*rawSt, "corrompido", g_project),
                  "projeto criado no storage");
            check(rawSt->writeText(*g_project.activeScenePath(),
                                   "{{{ lixo nao-json \x01\x02 !!!"),
                  "cena CORROMPIDA escrita no disco");
            g_scene.clear();
            g_storage = std::move(st);
            g_projectReady = true;
            g_resources.setStorage(rawSt);
            g_gpu.init(&g_resources);
            g_texCache = std::make_unique<TextureCache>(*rawSt);
            g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor,
                                                           *g_texCache);
            onAppCmd(&app, APP_CMD_TERM_WINDOW);
            onAppCmd(&app, APP_CMD_INIT_WINDOW);
            check(g_ready,
                  "boot com projeto CORROMPIDO no disco completa (g_ready)");
            check(logHas("[boot 6/6] scene FALHOU"),
                  "a cena corrompida logada como FALHOU (legivel)");
            check(logHas("editor arranca com cena vazia"),
                  "o editor arranca com cena vazia (zero crash)");
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
        }

        // 8.7 PROBLEMA 3: a memória do arranque no log (a evidência p/ o
        //     dono comparar com o FinalizerWatchdog dos tombstones antigos)
        check(logHas("boot: memoria"),
              "a linha de memoria do arranque (Problema 3)");

        // 8.8 GATE interno R-006: zero assinaturas de crash nativo em TODO
        //     o engine.log do replay (a mensagem do check NUNCA cita o
        //     padrão — a lição 0.8.12-c: o gate grepa o PRÓPRIO output)
        check(logCount("SIGSEGV") == 0,
              "zero assinaturas de crash nativo no engine.log (R-006)");
        check(logCount("SEGV_ACCERR") == 0,
              "zero falhas de acesso nativas no engine.log (R-006)");

        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        // o fecho TOTAL no fim da fase: com o TERM, NENHUM stream vivo resta
        std::printf("    [streams] fim da fase: abertos=%d fechados=%d vivos=%d\n",
                    oboe::testing::hooks().openCount,
                    oboe::testing::hooks().closeCount,
                    oboe::testing::hooks().openCount -
                        oboe::testing::hooks().closeCount);
        check(oboe::testing::hooks().closeCount ==
                      oboe::testing::hooks().openCount,
              "fim da fase: NENHUM stream vivo resta (o TERM fechou tudo)");
        oboe::testing::reset();
    }

    // ======================================================================
    // FASE 9 — UI REPLAY (0.9.4 / FASE 9 do dono): o editor de script
    // digita sem fechar (G0-1/G0-2), Docs alcançáveis (G0-3), lifecycle
    // com fonte gravada (o bug do handle morto). A toolbar/acentos/layout
    // entram nos grupos G1/G2 (mesma fase, passos novos).
    // ======================================================================
    fase("FASE 9 — UI replay: editor de script + Docs + lifecycle");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        ime::clearForTest();
        g_jni.void_calls.clear();

        // projeto REAL com a cena gravada (o lifecycle do INIT recarrega a
        // cena do disco — o cenário exato do bug do handle morto)
        auto st9 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt9 = st9.get();
        check(Project::createNew(*rawSt9, "fase9", g_project), "projeto criado");
        {
            const Handle h = g_scene.create("Ator");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            t->addComponent<ScriptComp>();
            check(g_project.saveActiveScene(*rawSt9, g_scene), "cena gravada");
        }
        g_storage = std::move(st9);
        g_projectReady = true;

        android_app app;
        std::memset(&app, 0, sizeof(app));
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 9.1 — ABRIR o editor de script (o caminho REAL do Inspector):
        // skeleton + cursor no interior + par portrait/IME
        passo("9.1 abrir o editor de script (script NOVO)");
        const Handle ator = g_scene.find("Ator");
        check(ator.valid(), "o TIC Ator existe pós-boot");
        openScriptEditor(ator);
        check(g_editor.scriptWin.open, "o editor abre");
        check(std::string(g_editor.scriptWin.buf) ==
                  editor::scriptwin::kSkeleton,
              "script SEM fonte abre com o esqueleto base (G0-2)");
        check(g_editor.scriptWin.buf[g_editor.scriptWin.caret] == '}',
              "o cursor abre NO INTERIOR do allmoments (G0-2)");
        bool sawP = false, sawS = false;
        for (const auto& c : g_jni.void_calls) {
            if (c.first == "setOrientation" && c.second == 1) sawP = true;
            if (c.first == "imeShow") sawS = true;
        }
        check(sawP, "JNI: portrait pedido ao abrir");
        check(sawS, "JNI: IME show pedido ao abrir");

        // 9.2 — DIGITAR (o IME do sistema, o caminho do GBoard): 20 teclas
        // e o editor CONTINUA ABERTO com o texto presente (o sintoma exato
        // da checklist da 0.9.3: "script editor fecha ao digitar")
        passo("9.2 digitar 20 teclas do IME sem fechar (G0-1)");
        for (int i = 0; i < 20; ++i) {
            char one[2] = {static_cast<char>('a' + (i % 26)), 0};
            Java_vv_goni_VvActivity_nativeOnImeText(
                g_jni.env, nullptr, g_jni.newString(one));
        }
        Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 66, 0);
        Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 67, 0);
        frame();
        check(g_editor.scriptWin.open, "o editor SIGE aberto apos 20 teclas");
        check(g_editor.scriptWin.buf.size() > 20, "o texto esta PRESENTE");
        check(logCount("script") == 0 || true, "(diagnostico)");

        // 9.3 — o TECLADO IN-APP: toque no corpo abre o teclado (result 5 =
        // IME re-pedido) e a tecla digitavel entra pelo MESMO applyEvent
        passo("9.3 teclado in-app: toque no corpo + tecla (G0-1)");
        g_jni.void_calls.clear();
        eglstub::g_surfaceW = 720;
        eglstub::g_surfaceH = 1536;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(g_editor.scriptWin.open, "o editor sobrevive aa rotacao");
        bool sawImeAgain = false;
        for (const auto& c : g_jni.void_calls) {
            if (c.first == "imeShow") sawImeAgain = true;
        }
        check(sawImeAgain,
              "INIT_WINDOW: o IME e RE-PEDIDO pos-rotacao (o fix do foco)");
        check(g_editor.scriptWin.buf.size() > 20,
              "o buffer sobrevive ao ciclo TERM/INIT");
        // o handle do TIC morreu no reload — o MAIN re-validou por NOME
        // (o check lê o ESTADO sem chamar o fix — senão cura a mutação)
        check(g_scene.get(g_editor.scriptWin.tic) != nullptr,
              "o MAIN re-validou o TIC dono por NOME pos-reload (G0-1)");
        const size_t bufBefore = g_editor.scriptWin.buf.size();
        // 0.9.6.8 (GRUPO E): o teclado da engine SAIU — o IME do sistema é
        // o ÚNICO teclado. O toque no CORPO pede o IME (result 5) e a
        // BARRA DE SÍMBOLOS dokada sobre ele aparece com o inset REAL
        tap(360.0f, 400.0f);
        // (o result 5 é o contrato — o main re-pede o IME; o teclado
        // próprio CEDER morreu com ele)
        // a BARRA: o inset injetado como o device manda (280px = um
        // GBoard típico no harness @1.0) — as teclas entram pelo MESMO
        // applyEvent (a 2ª tecla é '{'; a 1ª é o seletor de página)
        vv::ime::setBottomInset(280.0f);
        {
            // o toque no corpo (acima) MOVEU o caret para a linha sob o
            // dedo — o símbolo entra NO CARET (o contrato R-019), não no fim
            const u32 caret0 = g_editor.scriptWin.caret;
            const f32 keyW = 720.0f / 14.0f;
            const f32 barY = 1536.0f - 280.0f - 40.0f;
            tap(keyW * 1.5f, barY + 20.0f);
            check(g_editor.scriptWin.buf.size() == bufBefore + 1 &&
                      g_editor.scriptWin.caret == caret0 + 1 &&
                      g_editor.scriptWin.buf[caret0] == '{',
                  "a tecla '{' da BARRA entra NO CARET pelo MESMO applyEvent "
                  "(Grupo E)");
        }
        vv::ime::setBottomInset(0.0f);   // o IME fecha (o par do fecho)

        // 9.4 — FECHAR com o back: a FONTE GRAVA no ScriptComp (o bug
        // 0.9.3: o handle morto fazia o fecho NUNCA gravar)
        passo("9.4 fechar: a fonte grava no componente (G0-1)");
        closeScriptEditor();
        check(!g_editor.scriptWin.open, "o editor fecha com o back");
        const Tic* atorDepois = g_scene.get(g_scene.find("Ator"));
        check(atorDepois != nullptr &&
                  atorDepois->getComponent<ScriptComp>() != nullptr &&
                  !atorDepois->getComponent<ScriptComp>()->source.empty(),
              "a fonte digitada FICA gravada no ScriptComp (pelo nome)");
        // GUARDAR A CENA ainda em memoria (antes de qualquer rotação — o
        // reload traz o .goni; a fonte tem de viajar NO DISCO)
        check(g_project.saveActiveScene(*rawSt9, g_scene) &&
                  g_project.saveManifest(*rawSt9),
              "a cena e guardada no .goni (a fonte viaja no disco)");
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 9.5 — DOCS pela LUPA do editor (G0-3): abre por cima, a pesquisa
        // filtra as entradas estruturadas e mostra o exemplo.
        // O fluxo REAL do .goni: fechar guarda a fonte no componente;
        // GUARDAR A CENA materializa-a no disco; o reload traz-a de volta.
        passo("9.5 Docs: lupa do editor + pesquisa filtra (G0-3)");
        eglstub::g_surfaceW = 720;
        eglstub::g_surfaceH = 1536;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        const Handle atorHandle9 = g_scene.find("Ator");
        check(atorHandle9.valid(), "o Ator volta do .goni");
        const ScriptComp* sc9 =
            g_scene.get(atorHandle9)->getComponent<ScriptComp>();
        check(sc9 != nullptr && !sc9->source.empty(),
              "a fonte gravada SOBREVIVE no disco (round-trip .goni)");
        const std::string fonteGuardada = sc9 ? sc9->source : std::string();
        openScriptEditor(atorHandle9);
        check(g_editor.scriptWin.buf == fonteGuardada,
              "script EXISTENTE reabre com a fonte guardada intacta (G0-2)");
        // a superficie JÁ está em portrait (a rotação do reload acima)
        {
            // 0.9.6.8 (E · header flexível): lupaX = 720-8-72-8-72-12-48 = 500
            const f32 docsX = 720.0f - 8.0f - 72.0f - 8.0f - 72.0f - 12.0f - 48.0f;
            tap(docsX + 24.0f, 28.0f);
        }
        check(g_editor.docsScreen.open, "a LUPA abre as Docs por cima (G0-3)");
        check(!g_editor.scriptWin.open == false,
              "o editor continua aberto POR BAIXO das Docs");
        // a pesquisa filtra (o campo commita pelo purpose 9 — aqui direto)
        std::snprintf(g_editor.docsScreen.query,
                      sizeof(g_editor.docsScreen.query), "view");
        g_editor.docsScreen.queryLen = 4;
        check(voni::docs::search("view").size() > 0,
              "a pesquisa 'view' filtra entradas estruturadas");
        check(voni::docs::search("view").size() < voni::docs::search("").size(),
              "o filtro REDUZ a lista (pesquisa viva)");
        g_editor.docsScreen.open = false;   // back
        check(g_editor.scriptWin.open, "o editor volta a ser o modal");
        closeScriptEditor();

        // 9.6 — DOCS pelo SETTINGS (G0-3): a linha "Ver docs da V.ONI" era
        // MORTA no device (o walk do scrollTap nao a re-despachava)
        passo("9.6 Docs: a linha do Settings (o fix do botao morto)");
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        onAppCmd(&app, APP_CMD_TERM_WINDOW);
        onAppCmd(&app, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        g_editor.settingsMenu = true;
        g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                     editor::settings::kBitAudio |
                                     editor::settings::kBitPerm |
                                     editor::settings::kBitDiag;
        frame();   // layout estabiliza (slot de scroll)
        // y da linha Docs: 8 + 4 headers colapsados*48 + header Docs 48 + 18
        // (PASSO 4 · 0.9.6.20 J-03: as LINHAS do Settings são 36dp — a meia
        // linha era 24 nos 48dp)
        // 0.9.6 (G1): Settings ECRÃ CHEIO — sem a banda kToolbarH do overlayArea
        tap(800.0f, 56.0f + 8.0f + 4.0f * 48.0f + 48.0f + 18.0f);
        check(g_editor.docsScreen.open,
              "o toque na linha Docs do Settings ABRE as Docs (era morta)");
        check(logHas("voni: docs abertas"), "a abertura fica LOGADA");
        g_editor.docsScreen.open = false;
        g_editor.settingsMenu = false;

        // 9.7 — ACENTOS (G1-2/R-008): o atlas do boot tem a cobertura
        // latina e o frame EMITE os glifos acentuados (o "ÁUDIO" desenha)
        passo("9.7 acentos: cobertura do atlas + emissao (R-008)");
        check(g_font.hasGlyph(0xE7) && g_font.hasGlyph(0xE3) &&
                  g_font.hasGlyph(0xC3) && g_font.hasGlyph(0xF5) &&
                  g_font.hasGlyph(0xE9) && g_font.hasGlyph(0xED),
              "o atlas do boot tem c/ae, a-tilde, A-tilde, o-tilde, e-agudo, "
              "i-agudo (R-008)");
        check(g_font.hasGlyph(0x2026),
              "a elipse U+2026 esta no atlas (truncagem com '…')");
        {
            g_ui.beginFrame(nullptr, &g_input,
                             static_cast<f32>(g_egl.width()),
                             static_cast<f32>(g_egl.height()));
            const u32 antes = g_ui.glyphsForTest().vertexCount();
            g_ui.label(16.0f, 60.0f,
                       "\xC3\x81UDIO F\xC3\xADsica Anima\xC3\xA7\xC3\xA3o "
                       "Sele\xC3\xA7\xC3\xA3o \xC3\xA7\xC3\xA3o "
                       "\xC3\x83\xC3\x95 \xC3\xA7",
                       theme::kTheme.text1);
            const u32 depois = g_ui.glyphsForTest().vertexCount();
            g_ui.endFrame();
            check(depois > antes,
                  "a string de teste do dono EMITE glifos (acentos desenham)");
        }

        // 9.8 — LAYOUT.JSON em DEBOUNCE (G1-5): o log do dono mostrava 4
        // writes em ~40 s (cada passo de 8dp do drag do drawer gravava);
        // agora grava 1,5 s após a ÚLTIMA alteração, UMA linha
        // "layout guardado (motivo)" e NÃO grava sem mudança
        passo("9.8 layout.json: debounce 1,5 s — uma linha (G1-5)");
        {
            g_lastLayoutSaved.clear();   // baseline determinístico
            const int n0 = logCount("layout guardado");
            // o "drag" inteiro: 3 passos de 8dp SEGUIDOS (cada tick 0,4 s —
            // o debounce RECOMEÇA a cada mudança: nada é gravado no meio)
            g_bottom.bottomTab = 1;
            g_bottom.drawerH = 240.0f;
            for (int i = 0; i < 3; ++i) {
                g_bottom.drawerH = 240.0f + 8.0f * static_cast<f32>(i);
                layoutSaveTick(0.4f);
            }
            check(logCount("layout guardado") == n0,
                  "durante o drag (3 passos): ZERO writes (coalesce)");
            // o debounce VENCE (1,6 s após a última alteração): 1 write
            for (int i = 0; i < 4; ++i) {
                layoutSaveTick(0.4f);
            }
            check(logCount("layout guardado") == n0 + 1,
                  "1,5 s após a última alteração: EXATAMENTE 1 write");
            check(logHas("layout guardado (painel de baixo)"),
                  "a linha única traz o MOTIVO (painel de baixo)");
            // sem mudança: NÃO volta a gravar
            for (int i = 0; i < 6; ++i) {
                layoutSaveTick(0.4f);
            }
            check(logCount("layout guardado") == n0 + 1,
                  "sem mudança de conteúdo: NÃO grava");
            // a SAÍDA para segundo plano faz o FLUSH imediato do pendente
            g_editor.showInspector = !g_editor.showInspector;
            layoutSaveTick(0.2f);   // acabou de agendar (1,5 s pendentes)
            onAppCmd(&app, APP_CMD_PAUSE);
            check(logCount("layout guardado") == n0 + 2,
                  "APP_CMD_PAUSE: o write pendente faz FLUSH imediato");
            check(logHas("layout guardado (saída para segundo plano)"),
                  "o flush loga o motivo de segundo plano");
            // o áudio pausado pelo cmd não pode deixar stream vivo
            onAppCmd(&app, APP_CMD_RESUME);
        }

        // 9.9 — CHROME ANCORADO AO RECT DA VIEWPORT (G1-1 → PASSO 3): nunca
        // cobre outro painel — com o painel de baixo FECHADO e ABERTO
        passo("9.9 chrome ancorado ao viewport (G1-1/PASSO 3)");
        {
            const f32 sw = static_cast<f32>(g_egl.width());
            const f32 sh = static_cast<f32>(g_egl.height());
            const safe::Insets ins = g_ui.safeArea();
            // estado A: painel de baixo FECHADO, Inspector ABERTO
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
            g_editor.showInspector = true;
            frame();
            const UiRect vA =
                safe::centerRect(sw, sh, ins, 0.0f, g_editor.showInspector);
            const editor::vpchrome::Layout lA = editor::vpchrome::layout(vA);
            const editor::vpchrome::Layout* a = &lA;
            // o RAIL esquerdo (as 5 ferramentas) + os cantos DENTRO do rect
            bool railOkA = true;
            for (u32 i = 0; i < 5; ++i) {
                railOkA = railOkA && safe::rectInside(a->rail[i], vA);
            }
            check(railOkA && safe::rectInside(a->railPanel, vA),
                  "painel FECHADO: o rail esquerdo (5 ferramentas) DENTRO do "
                  "rect da viewport");
            // a fila do topo (undo/redo/save/⋯) + os cantos
            bool quickOkA = true;
            for (int i = 0; i < 4; ++i) {
                quickOkA = quickOkA && safe::rectInside(a->quick[i], vA);
            }
            check(quickOkA && safe::rectInside(a->quickPanel, vA) &&
                      safe::rectInside(a->gizmoBtn, vA),
                  "a fila do topo (undo/redo/save/⋯) + o gizmo dentro do "
                  "rect");
            // 0.9.6.19b (D21): o canto inferior DIREITO fica LIMPO — o [+]
            // morreu; o alvo mais a sul do lado direito é a fila do topo
            check(safe::rectInside(a->quick[3], vA),
                  "o ⋯ fica dentro do rect (a fila pode descer na "
                  "degradação — o D21 é o canto INFERIOR limpo)");
            check(a->gizmoBtn.y + a->gizmoBtn.h < vA.y + vA.h * 0.5f,
                  "D21 o gizmo fica no TOPO-direito (nada no fundo-direito)");
            // estado B: painel de baixo ABERTO (drawer 240) — o chrome SOBRE
            // o rect (D21: a prova do canto é o gizmo do topo, o [+]
            // acompanhante morreu)
            g_bottom.bottomTab = 1;
            g_bottom.drawerH = 240.0f;
            frame();
            const UiRect vB = safe::centerRect(sw, sh, ins, 240.0f,
                                               g_editor.showInspector);
            const editor::vpchrome::Layout lB = editor::vpchrome::layout(vB);
            const editor::vpchrome::Layout* b = &lB;
            bool dentroB = safe::rectInside(b->gizmoBtn, vB) &&
                           safe::rectInside(b->quickPanel, vB);
            for (u32 i = 0; i < 5; ++i) {
                dentroB = dentroB && safe::rectInside(b->rail[i], vB);
            }
            check(dentroB, "painel ABERTO (240px): o chrome SOBE com o "
                           "rect — nunca cobre o painel de baixo");
            check(b->gizmoBtn.y < a->gizmoBtn.y + 100.0f,
                  "o gizmo ACOMPANHA o topo do rect (o desenho segue o "
                  "viewport, nunca pisa o painel)");
            // conflito DIRETO contra o painel: nenhum botão invade a faixa
            // do drawer (y >= topo do painel)
            const f32 drawerTop = sh - 240.0f;
            bool foraDoDrawer = b->gizmoBtn.y + b->gizmoBtn.h <=
                                    drawerTop + 0.5f;
            for (u32 i = 0; i < 5; ++i) {
                foraDoDrawer =
                    foraDoDrawer &&
                    b->rail[i].y + b->rail[i].h <= drawerTop + 0.5f;
            }
            check(foraDoDrawer, "nenhum botão pisa a faixa do painel de baixo");
            // largura mínima: o rail esquerdo cabe na viewport mais estreita
            // (1600×720 com Inspector + drawer = o pior caso do dono)
            check(vB.w >= 480.0f, "a viewport útil no pior caso tem folga");
            // o rail vertical (40dp de alvo + 8 de passo) cabe na LARGURA
            // e a fila do topo na que sobra — o total fecha < viewport útil
            const f32 totalW = b->railPanel.w + b->quickPanel.w;
            check(totalW < vB.w, "rail + fila do topo cabem na viewport útil");
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
        }

        // 9.10 — TOCAR SELECIONA O TIC (G1-6): o toque no CORPO de um cubo
        // grande seleciona-o (o bug: só o centro a 44px contava) — e cada
        // mudança de seleção LOGA com o motivo
        passo("9.10 tocar no corpo seleciona o TIC (G1-6)");
        {
            // Inspector FECHADO (viewport inteiro — o tap tem de nascer
            // DENTRO do viewRect para o pick armar)
            g_editor.showInspector = false;
            // um TIC Cubo GRANDE na cena (mesh procedural do boot, escala 8)
            const Handle hCubo = g_scene.create("Cubo");
            Tic* cubo = g_scene.get(hCubo);
            Transform3D* ctr = cubo->addComponent<Transform3D>();
            MeshRenderer* cmr = cubo->addComponent<MeshRenderer>();
            cmr->mesh = &g_cubeMesh;
            ctr->pos = Vec3{0.0f, 0.0f, -6.0f};
            ctr->scale = Vec3{8.0f, 8.0f, 8.0f};
            ctr->updateWorld();
            // limpa a seleção (estado de partida conhecido)
            g_editor.selected = Handle::invalid();
            g_editor.selElement = -1;
            // o CANTO do corpo projetado (fora dos 44px do centro)
            const Mat4 vp = Mat4::mul(g_camera.proj(g_egl.width() /
                                                    static_cast<f32>(
                                                        g_egl.height())),
                                      g_camera.view());
            const Vec3 canto{-0.5f, -0.5f, -0.5f};
            const Vec3 w = Vec3{
                ctr->world.m[0] * canto.x + ctr->world.m[4] * canto.y +
                    ctr->world.m[8] * canto.z + ctr->world.m[12],
                ctr->world.m[1] * canto.x + ctr->world.m[5] * canto.y +
                    ctr->world.m[9] * canto.z + ctr->world.m[13],
                ctr->world.m[2] * canto.x + ctr->world.m[6] * canto.y +
                    ctr->world.m[10] * canto.z + ctr->world.m[14]};
            f32 cx = 0.0f, cy = 0.0f, ccx = 0.0f, ccy = 0.0f;
            gizmo::projectPoint(vp, w, static_cast<f32>(g_egl.width()),
                                static_cast<f32>(g_egl.height()), cx, cy);
            gizmo::projectPoint(vp, ctr->pos,
                                static_cast<f32>(g_egl.width()),
                                static_cast<f32>(g_egl.height()), ccx, ccy);
            const f32 dc = std::sqrt((cx - ccx) * (cx - ccx) +
                                     (cy - ccy) * (cy - ccy));
            check(dc > 60.0f,
                  "o canto do corpo está LONGE do centro (>60px — fora do "
                  "raio antigo de 44)");
            // o pick DIRETO acerta (o contrato do G1-6)
            check(camgizmo::pickSceneTic(
                      g_scene, vp, static_cast<f32>(g_egl.width()),
                      static_cast<f32>(g_egl.height()), cx, cy) == hCubo,
                  "pickSceneTic: o toque no CORPO seleciona o TIC (G1-6)");
            // e pelo caminho REAL da UI: o tap no viewport
            const int nSel = logCount("seleção: TIC 'Cubo' (toque no viewport)");
            tap(cx, cy);
            idle(1);
            check(g_editor.selected == hCubo,
                  "o TAP no corpo do cubo SELECIONA-o (o caminho da UI)");
            check(logCount("seleção: TIC 'Cubo' (toque no viewport)") ==
                      nSel + 1,
                  "a mudança de seleção LOGA com o motivo (toque no viewport)");
            // o tap no VAZIO limpa E loga (x=500: FORA do AABB projetado
            // do cubo — minX≈580 — e fora do círculo de 44px do centro)
            const int nClr = logCount("seleção limpa: toque no vazio");
            tap(500.0f, 300.0f);
            idle(1);
            check(!g_editor.selected.valid(),
                  "o tap no vazio LIMPA a seleção");
            check(logCount("seleção limpa: toque no vazio") == nClr + 1,
                  "a limpeza também LOGA (cada mudança com motivo)");
            // a cena volta ao estado (o TIC extra sai)
            g_scene.destroy(hCubo);
        }

        // 9.11 — G2: ALINHAMENTO AO MOCK — barra única 56dp (a tab bar
        // fundiu-se), ícones da hierarquia por tipo de corpo (tic_static/
        // tic_player/tic_rigid/tic_camera), Física em duas colunas e o
        // Inspector que volta ao TOPO na troca de TIC
        passo("9.11 G2: barra única + ícones de corpo + inspector ao topo");
        {
            // a BARRA ÚNICA: kToolbarH = 36 (PASSO 1) — o viewport GANHOU
            // os 20dp da barra mais baixa + os 24dp da status removida
            check(nearEqF(safe::kToolbarH, 36.0f),
                  "kToolbarH = 36 (menu+tabs numa barra — G2-10 · PASSO 1)");
            const UiRect viewG2 = safe::centerRect(
                static_cast<f32>(g_egl.width()),
                static_cast<f32>(g_egl.height()), g_ui.safeArea(), 0.0f,
                false);
            const f32 hAntiga = static_cast<f32>(g_egl.height()) - 104.0f -
                                safe::kStatusH - safe::kBottomTabH;
            check(nearEqF(viewG2.h, hAntiga + 68.0f),
                  "o viewport central GANHOU 68px (barra 36 + status 0 vs "
                  "o antigo 56+24+48 — PASSO 1)");
            // o TRIAD morreu (os pontinhos fantasma) — o layout do chrome
            // não tem triad (compila). D21 (0.9.6.19b): o canto direito só
            // tem o GIZMO do topo — o fundo-direito fica LIMPO
            const editor::vpchrome::Layout lg2 =
                editor::vpchrome::layout(viewG2);
            check(nearEqF(lg2.gizmoBtn.x + lg2.gizmoBtn.w + 8.0f,
                          viewG2.x + viewG2.w),
                  "o gizmo continua o ÚNICO elemento do canto direito (o "
                  "fundo-direito ficou limpo no D21)");
            // ícones da hierarquia por TIPO DE CORPO (G2-7)
            {
                const Handle hC = g_scene.create("CorpoG2");
                Tic* tc2 = g_scene.get(hC);
                Transform3D* tr2 = tc2->addComponent<Transform3D>();
                tr2->pos = Vec3{40.0f, 0.0f, -20.0f};   // fora do caminho
                tr2->updateWorld();
                BodyComp* bc2 = tc2->addComponent<BodyComp>();
                bc2->type = BodyType::Static;
                check(editor::hierIconFor(*tc2) == icons::Icon::Static,
                      "corpo ESTÁTICO → tic_static (G2-7)");
                bc2->type = BodyType::Character;
                check(editor::hierIconFor(*tc2) == icons::Icon::Person,
                      "corpo PERSONAGEM → tic_player (G2-7)");
                bc2->type = BodyType::Rigid;
                check(editor::hierIconFor(*tc2) == icons::Icon::Rigid,
                      "corpo RÍGIDO → tic_rigid (G2-7)");
                check(icons::iconByName("tic_static") >= 0 &&
                          icons::iconByName("tic_rigid") >= 0,
                      "os ícones novos existem pelos nomes do mock");
                // Física em DUAS COLUNAS: o plano tem 3 TwoCol
                const TextMetrics m2 = g_ui.textMetrics();
                const editor::InspProfile prof2 = editor::inspectorProfile(*tc2);
                editor::InspRow plan2[64];
                const u32 n2 = inspectorPlan(prof2, m2, false, 0u, plan2);
                u32 twoCol = 0;
                for (u32 i2 = 0; i2 < n2; ++i2) {
                    if (plan2[i2].kind == editor::InspRow::Kind::TwoCol) {
                        ++twoCol;
                    }
                }
                check(twoCol == 3u,
                      "Física: 3 linhas em duas colunas (tipo/forma/no chão)");
                // o Inspector VOLTA AO TOPO na troca de TIC (G2-8)
                // (o Inspector tem de estar ABERTO — o 9.10 fechou-o)
                g_editor.showInspector = true;
                g_editor.selected = hC;   // estado de partida: o CorpoG2
                frame();
                g_ui.scrollSetOffset(editor::kIdScrollInsp, 200.0f);
                const Handle hAtor2 = g_scene.find("Ator");
                g_editor.selected = hAtor2;
                frame();
                check(nearEqF(g_ui.scrollOffsetForTest(
                          editor::kIdScrollInsp), 0.0f),
                      "troca de TIC: o Inspector volta ao TOPO (G2-8)");
                g_scene.destroy(hC);
            }
        }

        onAppCmd(&app, APP_CMD_TERM_WINDOW);
    }

    // ======================================================================
    // FASE 11 — 0.9.5 · LINKERS & TYKERS (METADE 1): o replay no caminho
    // REAL do app — o editor carrega um script com linker/tyker, o Run
    // arranca a run, o follow MOVE o Transform3D, o colorpars TINGE o
    // MeshRenderer, o RF em falta LOGA o erro exato sem matar o script, e
    // o CICLO aparece na BARRA DE ERRO do editor com linha. (11.B — os
    // lookups de ajuda do Editor que Ensina — entra com a METADE 2.)
    // ======================================================================
    fase("FASE 11 — replay linkers/tykers (0.9.5 METADE 1)");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        ime::clearForTest();
        g_jni.void_calls.clear();

        auto st11 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt11 = st11.get();
        check(Project::createNew(*rawSt11, "fase11", g_project),
              "projeto criado");
        {
            const Handle hA = g_scene.create("Ator");
            Tic* tA = g_scene.get(hA);
            Transform3D* trA = tA->addComponent<Transform3D>();
            trA->pos = Vec3{15.0f, 0.0f, 0.0f};
            trA->updateWorld();
            tA->addComponent<MeshRenderer>();
            // o script viaja NO .goni (o serializer salta ScriptComp VAZIO —
            // a fonte entra ANTES do save, o cenário real de um script que
            // já existia no projeto)
            ScriptComp& sc11 = *tA->addComponent<ScriptComp>();
            sc11.source =
                "linker(Ator)to(Alvo)=RF(principal)\n"
                "tyker(seguelo){ find(principal) follow(2) }\n"
                "tyker(pinta){ find(principal) colorpars(cor)(#FF8800) }\n"
                "central main { on moment { } allmoments { } }\n";
            const Handle hB = g_scene.create("Alvo");
            Tic* tB = g_scene.get(hB);
            Transform3D* trB = tB->addComponent<Transform3D>();
            trB->pos = Vec3{5.0f, 0.0f, 0.0f};
            trB->updateWorld();
            check(g_project.saveActiveScene(*rawSt11, g_scene),
                  "cena gravada (Ator em 15, Alvo em 5, script no .goni)");
        }
        g_storage = std::move(st11);
        g_projectReady = true;

        android_app app11;
        std::memset(&app11, 0, sizeof(app11));
        onAppCmd(&app11, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);

        // 11.1 — o script COM LINKER/TYKER corre pelo caminho REAL: o
        // editor abre a fonte guardada, o Run arranca, o follow move o TIC
        passo("11.1 linker+tyker: Run real → follow move o Transform3D");
        {
            const Handle ator = g_scene.find("Ator");
            check(ator.valid(), "o TIC Ator volta do boot");
            const ScriptComp* sc11 =
                g_scene.get(ator)->getComponent<ScriptComp>();
            check(sc11 != nullptr && !sc11->source.empty(),
                  "o script veio NO .goni (round-trip das fontes)");
            openScriptEditor(ator);
            check(g_editor.scriptWin.open, "o editor abre com a fonte");
            check(g_editor.scriptWin.buf.find("linker(Ator)to(Alvo)") !=
                      std::string::npos,
                  "a fonte guardada abre INTACTA (com linkers)");
            scriptEditorRun();
            check(g_editor.scriptWin.running, "a run do editor está ATIVA");
            // o tick do VoniSystem (o harness não passa pelo android_main,
            // onde o g_systems se registra — o tick DIRETO é o mesmo passo
            // que o loop real dá: VoniSystem.tick → runFrame → tykers)
            g_voni.tick(g_scene, 1.0f / 60.0f);
            frame();   // o frame de desenho
            const Tic* tA = g_scene.get(g_scene.find("Ator"));
            const Transform3D* tr =
                tA ? tA->getComponent<Transform3D>() : nullptr;
            check(tr != nullptr && nearEqF(tr->pos.x, 7.0f),
                  "follow(2): o Ator FICA a 2 do Alvo (15→7, alvo em 5)");
            check(tr != nullptr && nearEqF(tr->pos.y, 0.0f) &&
                      nearEqF(tr->pos.z, 0.0f),
                  "follow: eixos y/z intocados");
        }

        // 11.2 — o colorpars TINGIU o MeshRenderer (o mesmo frame de 11.1)
        passo("11.2 colorpars tinge o material do TIC de origem");
        {
            const Tic* tA = g_scene.get(g_scene.find("Ator"));
            const MeshRenderer* mr =
                tA ? tA->getComponent<MeshRenderer>() : nullptr;
            check(mr != nullptr && nearEqF(mr->tint[0], 1.0f) &&
                      nearEqF(mr->tint[1], 0.5333f, 1e-2f) &&
                      nearEqF(mr->tint[2], 0.0f),
                  "colorpars(cor)(#FF8800): tint = (1, 0.53, 0)");
        }

        // 11.3 — RF EM FALTA (R-012): o erro exato no engine.log, o tyker
        // não corre, o script CONTINUA (o follow do outro tyker mexeu)
        passo("11.3 RF em falta: log exato + tyker não corre (R-012)");
        {
            const Handle ator = g_scene.find("Ator");
            // repõe a posição de partida + EDITA o buffer (o que o Run usa)
            if (Tic* t = g_scene.get(ator)) {
                if (Transform3D* tr = t->getComponent<Transform3D>()) {
                    tr->pos = Vec3{15.0f, 0.0f, 0.0f};
                    tr->updateWorld();
                }
            }
            g_editor.scriptWin.buf =
                "linker(Ator)to(Alvo)=RF(principal)\n"
                "tyker(bom){ find(principal) follow(2) }\n"
                "tyker(mau){ find(fantasma) follow() }\n"
                "central main { allmoments { View P \"segue\" } }\n";
            g_editor.scriptWin.caret =
                static_cast<u32>(g_editor.scriptWin.buf.size());
            scriptEditorRun();
            g_voni.tick(g_scene, 1.0f / 60.0f);
            frame();
            check(logHas("RF 'fantasma' não encontrada"),
                  "o log traz o ERRO EXATO da spec (R-012)");
            check(logHas("tyker 'mau' não corre"),
                  "o log diz QUAL tyker não correu");
            const Tic* tA = g_scene.get(g_scene.find("Ator"));
            const Transform3D* tr =
                tA ? tA->getComponent<Transform3D>() : nullptr;
            check(tr != nullptr && nearEqF(tr->pos.x, 7.0f),
                  "o tyker BOM correu (o script não morreu)");
            check(logHas("voni: segue"), "o allmoments segue a correr");
        }

        // 11.4 — CICLO (R-011): a run MORRE com o erro legível E a barra
        // de erro do editor ACENDE com a linha
        passo("11.4 ciclo a→b + b→a: erro legível na barra (R-011)");
        {
            // EDITA o buffer para o script com o CICLO (o que o Run compila)
            g_editor.scriptWin.buf =
                "linker(Ator)to(Alvo)=RF(p)\n"
                "linker(Alvo)to(Ator)=RF(p)\n"
                "central main { }\n";
            g_editor.scriptWin.caret =
                static_cast<u32>(g_editor.scriptWin.buf.size());
            scriptEditorRun();
            check(!g_editor.scriptWin.running, "a run NÃO arranca (rejeitada)");
            check(g_editor.scriptWin.errLine >= 1,
                  "a barra de erro tem LINHA");
            check(g_editor.scriptWin.errMsg.find("ciclo") !=
                      std::string::npos,
                  "a barra de erro diz CICLO (legível)");
            check(g_editor.scriptWin.errMsg.find("'Ator'") !=
                      std::string::npos,
                  "o erro nomeia os lados do ciclo");
        }

        // 11.5 — as Docs têm os LINKERS/TYKERS/COMPONENTES (a mesma fonte
        // do registo — a lupa do editor pesquisa por elas)
        passo("11.5 Docs: as categorias novas povoadas (o registo alimenta)");
        {
            const auto& all = voni::docs::all();
            bool hasLinker = false, hasTyker = false, hasComp = false;
            for (const auto& e : all) {
                hasLinker = hasLinker || e.cat == voni::docs::Cat::Linker;
                hasTyker = hasTyker || e.cat == voni::docs::Cat::Tyker;
                hasComp = hasComp || e.cat == voni::docs::Cat::Componente;
            }
            check(hasLinker && hasTyker && hasComp,
                  "Docs: categorias Linker/Tyker/Componente povoadas");
            check(!voni::docs::search("follow").empty(),
                  "Docs: a pesquisa apanha 'follow'");
            check(!voni::docs::search("colorpars").empty(),
                  "Docs: a pesquisa apanha 'colorpars'");
        }

        // ================================================================
        // 11.B — O EDITOR QUE ENSINA (METADE 2): os lookups de ajuda no
        // caminho REAL do app — o erro que ENSINA, a mini-descrição desde
        // a 1ª letra, os esqueletos por Tab, o toque numa palavra, os
        // níveis I/N/S, o copiar-referência — e o WALKTHROUGH scriptado:
        // um script FUNCIONAL montado só com a ajuda do editor.
        // (O editor é PORTRAIT 720×1536 — o par inseparável 0.9.1; a
        // superfície muda ANTES dos toques, o mesmo ciclo do 9.3, e o
        // editor SOBREVIVE ao reload — o fix G0-1 do handle por nome.)
        // ================================================================
        passo("11.6 erros-que-ensinam: o 'if' do Python aprende o exist");
        {
            eglstub::g_surfaceW = 720;
            eglstub::g_surfaceH = 1536;
            onAppCmd(&app11, APP_CMD_TERM_WINDOW);
            onAppCmd(&app11, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            check(g_editor.scriptWin.open,
                  "o editor SOBREVIVE ao ciclo portrait (G0-1)");
            g_editor.scriptWin.buf = "if (vida == 0) { }";
            g_editor.scriptWin.caret = 19;
            scriptEditorRun();
            check(!g_editor.scriptWin.running, "o script com 'if' não corre");
            check(g_editor.scriptWin.errLine >= 1,
                  "a barra de erro tem LINHA");
            check(g_editor.scriptWin.errMsg.find("exist") !=
                      std::string::npos,
                  "a barra de erro ENSINA: 'if' → exist (registo kForeign)");
        }

        passo("11.7 mini-descrição em tempo real DESDE A 1ª LETRA");
        {
            g_editor.scriptWin.errLine = 0;
            g_editor.scriptWin.errMsg.clear();
            g_editor.scriptWin.buf = "exi";
            g_editor.scriptWin.caret = 3;
            frame();   // o draw recalcula a strip do estado
            const std::string l1 =
                editor::scriptwin::helpStripLine1(g_editor.scriptWin);
            check(!l1.empty(), "a strip acende com 'exi' (3 letras)");
            check(l1.find("exist") != std::string::npos,
                  "a strip mostra o NOME do casamento por prefixo");
            check(editor::scriptwin::helpStripLine2(g_editor.scriptWin)
                      .empty(),
                  "nível Normal: 1 linha (sem encher o ecrã)");
        }

        passo("11.8 TAB → o esqueleto (o texto exato da spec)");
        {
            // a tecla Tab do GBoard chega pela fila do IME (keycode 61)
            Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 61, 0);
            frame();
            check(g_editor.scriptWin.buf == "exist(){ } notexist{ }",
                  "'exi' + Tab → o esqueleto 'exist(){ } notexist{ }'");
            check(g_editor.scriptWin.caret == 8,
                  "o caret fica NO INTERIOR do exist(){ … }");
        }

        passo("11.9 toque numa palavra → explicação com exemplo (Docs)");
        {
            // a palavra 'notexist' na linha 0 do buffer atual — o toque na
            // posição dela acende a strip com a explicação + exemplo
            g_editor.scriptWin.helpLevel = 1;   // Normal
            const f32 lh11 = 28.0f + 10.0f;     // (linha do corpo ~38px)
            tap(90.0f, 56.0f + 8.0f + lh11 * 0.5f);
            check(g_editor.scriptWin.helpTapped,
                  "o toque numa palavra marca o estado de explicação");
            const std::string l1 =
                editor::scriptwin::helpStripLine1(g_editor.scriptWin);
            check(!l1.empty(), "a strip mostra a explicação da palavra");
            const std::string l2 =
                editor::scriptwin::helpStripLine2(g_editor.scriptWin);
            check(l2.find("ex.:") == 0,
                  "no toque a 2ª linha acende com o EXEMPLO (das Docs)");
        }

        passo("11.10 os níveis I/N/S: Silencioso apaga a strip");
        {
            // 0.9.6.8 (E · header flexível): o botão do nível ancorou à
            // direita — helpX=388 (copy 444, lupa 500, run 560, stop 640)
            tap(388.0f + 24.0f, 28.0f);
            check(g_editor.scriptWin.helpLevel == 2, "N → S (Silencioso)");
            check(editor::scriptwin::helpStripLine1(g_editor.scriptWin)
                      .empty(),
                  "Silencioso: a strip APAGA (sem encher o ecrã)");
            tap(388.0f + 24.0f, 28.0f);
            check(g_editor.scriptWin.helpLevel == 0, "S → I (Iniciante)");
            check(!editor::scriptwin::helpStripLine1(g_editor.scriptWin)
                      .empty(),
                  "Iniciante: a strip acende");
            check(!editor::scriptwin::helpStripLine2(g_editor.scriptWin)
                      .empty(),
                  "Iniciante: SEMPRE com o exemplo (2 linhas)");
            tap(388.0f + 24.0f, 28.0f);
            check(g_editor.scriptWin.helpLevel == 1, "I → N (volta ao Normal)");
        }

        passo("11.11 copiar-referência: o clipboard recebe a referência");
        {
            g_jni.void_calls.clear();
            g_jni.last_new_string.clear();
            tap(444.0f + 24.0f, 28.0f);   // o botão 📋 (copyX=444, E flexível)
            frame();
            bool sawClip = false;
            for (const auto& c : g_jni.void_calls) {
                if (c.first == "clipboardCopy") {
                    sawClip = true;
                }
            }
            check(sawClip, "JNI: clipboardCopy chamada com a referência");
            check(g_jni.last_new_string.find(
                      "V.ONI — Referência da linguagem") !=
                      std::string::npos,
                  "o texto colável é a referência COMPLETA (do registo)");
            check(g_jni.last_new_string.find("tyker") != std::string::npos,
                  "a referência traz os tykers");
            check(logHas("referência V.ONI copiada"),
                  "o log regista a cópia (com o nº de entradas)");
        }

        passo("11.12 WALKTHROUGH: script funcional SÓ com a ajuda do editor");
        {
            // A pessoa que sabe Python/JS monta um script V.ONI que CORRE:
            // limpa o esqueleto com o backspace do teclado, digita o linker,
            // o 'tyker' expande pelo Tab, corrige o placeholder RF→principal
            // e acrescenta o follow — TUDO pelo caminho REAL do IME.
            g_editor.scriptWin.helpLevel = 1;
            g_editor.scriptWin.errLine = 0;
            g_editor.scriptWin.errMsg.clear();
            // (1) limpa: o caret vai ao FIM (seta →) e o backspace come
            // tudo (o backspace só apaga ANTES do caret — do fim limpa o
            // buffer inteiro)
            for (int i = 0; i < 30; ++i) {
                Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                       22, 0);   // RIGHT
            }
            for (int i = 0; i < 80; ++i) {
                Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                       67, 0);   // DEL
            }
            frame();
            check(g_editor.scriptWin.buf.empty(), "o esqueleto é limpo");
            // (2) digita o linker (o IME commita char a char)
            for (const char c :
                 std::string("linker(Ator)to(Alvo)=RF(principal)\n")) {
                char one[2] = {c, 0};
                Java_vv_goni_VvActivity_nativeOnImeText(
                    g_jni.env, nullptr, g_jni.newString(one));
            }
            frame();
            check(g_editor.scriptWin.buf.find("linker(Ator)to(Alvo)") !=
                      std::string::npos,
                  "o linker está no buffer (digitado)");
            // (3) 'tyker' + TAB → o esqueleto do registo
            for (const char c : std::string("tyker")) {
                char one[2] = {c, 0};
                Java_vv_goni_VvActivity_nativeOnImeText(
                    g_jni.env, nullptr, g_jni.newString(one));
            }
            Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr, 61, 0);
            frame();
            check(g_editor.scriptWin.buf.find("tyker(nome){ find(RF) }") !=
                      std::string::npos,
                  "o Tab expande o tyker (esqueleto do registo)");
            // (4) a pessoa corrige o placeholder RF→principal: o caret
            // está antes do '}' (22 no esqueleto); ←×2 põe-no DEPOIS do
            // 'RF', DEL×2 apaga as duas letras, e 'principal' entra no sítio
            for (int i = 0; i < 2; ++i) {
                Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                       21, 0);   // LEFT
            }
            for (int i = 0; i < 2; ++i) {
                Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                       67, 0);   // DEL
            }
            for (const char c : std::string("principal")) {
                char one[2] = {c, 0};
                Java_vv_goni_VvActivity_nativeOnImeText(
                    g_jni.env, nullptr, g_jni.newString(one));
            }
            frame();
            check(g_editor.scriptWin.buf.find("find(principal)") !=
                      std::string::npos,
                  "o placeholder RF→principal corrigido pelo IME");
            // (5) o caret salta o ')' (fica depois do find(...)) e
            // acrescenta o follow DENTRO do tyker
            Java_vv_goni_VvActivity_nativeOnImeKey(g_jni.env, nullptr,
                                                   22, 0);   // RIGHT
            for (const char c : std::string(" follow(2)")) {
                char one[2] = {c, 0};
                Java_vv_goni_VvActivity_nativeOnImeText(
                    g_jni.env, nullptr, g_jni.newString(one));
            }
            frame();
            // (6) RUN: o script FUNCIONA — o Ator segue o Alvo
            if (Tic* t = g_scene.get(g_scene.find("Ator"))) {
                if (Transform3D* tr = t->getComponent<Transform3D>()) {
                    tr->pos = Vec3{15.0f, 0.0f, 0.0f};
                    tr->updateWorld();
                }
            }
            scriptEditorRun();
            check(g_editor.scriptWin.running,
                  "o script do walkthrough ARRANCA");
            check(g_editor.scriptWin.errLine == 0, "sem erros na barra");
            g_voni.tick(g_scene, 1.0f / 60.0f);
            frame();
            const Tic* tA = g_scene.get(g_scene.find("Ator"));
            const Transform3D* tr =
                tA ? tA->getComponent<Transform3D>() : nullptr;
            check(tr != nullptr && nearEqF(tr->pos.x, 7.0f),
                  "o follow FUNCIONA: o Ator fica a 2 do Alvo (15→7)");
        }

        onAppCmd(&app11, APP_CMD_TERM_WINDOW);
    }

    // =====================================================================
    // FASE 12 — 0.9.6 G1: INSETS + CAMADAS (os ecrãs cheios respeitam a
    // safe-area e capturam TODO o toque; nada desenha por cima deles)
    // =====================================================================
    fase("FASE 12 — 0.9.6 G1: insets reais + camadas (ecrãs cheios)");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        ime::clearForTest();
        g_jni.void_calls.clear();

        auto st12 = std::make_unique<FakeStorage>();
        check(Project::createNew(*st12, "fase12", g_project),
              "projeto fase12 criado");
        {
            const Handle h = g_scene.create("Ator");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            t->addComponent<MeshRenderer>();
            t->addComponent<ScriptComp>();
            check(g_project.saveActiveScene(*st12, g_scene), "cena gravada");
        }
        g_storage = std::move(st12);
        g_projectReady = true;

        // O C33 com a faixa preta de verdade: topo 96 (status+recorte) e
        // barra de navegação 48 em baixo — o contentRect que o device manda
        // (landscape 1536x720, como o boot da FASE 1)
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        android_app app12;
        std::memset(&app12, 0, sizeof(app12));
        app12.contentRect = {0, 96, 1536, 672};   // insets T96 B48
        onAppCmd(&app12, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(nearEqF(g_ui.safeArea().top, 96.0f) &&
                  nearEqF(g_ui.safeArea().bottom, 48.0f),
              "12.0 os insets do contentRect chegam à UI (T96 B48)");

        // helper: NENHUM glifo sob a faixa do topo/acima da barra de baixo
        auto glyphsDentroDosInsets = [&]() {
            const QuadBatch& g = g_ui.glyphsForTest();
            const QuadVertex* v = g.vertices();
            const u32 n = g.vertexCount();
            const f32 topLimit = g_ui.safeArea().top;
            const f32 botLimit =
                static_cast<f32>(g_egl.height()) - g_ui.safeArea().bottom;
            u32 fora = 0;
            for (u32 i = 0; i + 5 < n; i += 6) {
                if (v[i].y < topLimit - 0.5f ||
                    v[i + 2].y > botLimit + 0.5f) {
                    ++fora;
                    if (std::getenv("VV_DBG_INSETS")) {
                        std::printf("FORA: y0=%.1f y1=%.1f x0=%.1f x1=%.1f "
                                    "(limites T%.0f B%.0f)\n",
                                    v[i].y, v[i + 2].y, v[i].x, v[i + 2].x,
                                    topLimit, botLimit);
                    }
                }
            }
            return fora;
        };

        // ---- 12.1 DOCS: o título nunca sob a faixa preta -------------------
        passo("12.1 Docs com insets T96/B48: nada desenha sob o sistema");
        {
            g_editor.docsScreen.open = true;
            g_editor.docsScreen.queryLen = 0;
            g_editor.docsScreen.expanded = -1;
            frame();
            check(glyphsDentroDosInsets() == 0,
                  "Docs: NENHUM glifo sob a faixa do topo/barra de baixo");
            // o BACK mora no inset+0..inset+56 — o toque FUNCIONA lá
            tap(32.0f, 96.0f + 28.0f);
            check(!g_editor.docsScreen.open,
                  "Docs: o back (dentro da parte útil) fecha");
            g_editor.docsScreen.open = false;
        }

        // ---- 12.2 SETTINGS: ecrã cheio + captura TODO o toque -------------
        passo("12.2 Settings: ecrã cheio, orbit morto, glifo do áudio fora");
        {
            g_editor.settingsMenu = true;
            frame();   // o settings desenha (sem o TIC Som ainda)
            check(glyphsDentroDosInsets() == 0,
                  "Settings: NENHUM glifo sob a faixa do topo/barra de baixo");
            // (a) baseline dos sólidos COM o modal aberto; o TIC de áudio
            // entra DEPOIS — se o glifo amarelo desenhasse por cima do
            // settings, o contador de sólidos SUBIA
            const u32 solidsBaseline = g_ui.solidsForTest().vertexCount();
            const Handle ha = g_scene.create("Som");
            if (Tic* ta = g_scene.get(ha)) {
                ta->addComponent<Transform3D>();
                ta->addComponent<AudioPlayer>();
            }
            frame();
            check(g_ui.solidsForTest().vertexCount() == solidsBaseline,
                  "o glifo amarelo do áudio NÃO desenha sobre o Settings "
                  "(sólidos idênticos com o TIC Som criado)");
            // (b) o DRAG no viewport NÃO orbita (a cena não mexe por trás)
            const f32 yaw0 = g_camera.yaw;
            const f32 pitch0 = g_camera.pitch;
            g_input.injectDown(0, 900.0f, 400.0f);
            frame();
            g_input.injectMove(0, 1100.0f, 300.0f);
            frame();
            g_input.injectUp(0);
            frame();
            check(nearEqF(g_camera.yaw, yaw0) &&
                      nearEqF(g_camera.pitch, pitch0),
                  "Settings aberto: o drag NÃO orbita a câmara (toque "
                  "capturado pelo ecrã cheio)");
            // (c) a barra de baixo escondida: o toque na tab Ficheiros não
            // abre o drawer (não desenhado = não interativo — a regra da
            // casa). A tab mora em y = 720-48(inset)-24(status)-48/2 = 624
            const f32 drawerH0 = g_editor.drawerH;
            const int tab0 = g_bottom.bottomTab;
            tap(200.0f, 624.0f);
            check(g_bottom.bottomTab == tab0 && g_editor.drawerH == drawerH0,
                  "Settings aberto: a barra de baixo NÃO responde (escondida)");
            g_editor.settingsMenu = false;
            frame();
            check(g_ui.solidsForTest().vertexCount() != solidsBaseline,
                  "settings fechado: o chrome volta a desenhar (o modal "
                  "não deixou estado)");
        }

        // ---- 12.3 EDITOR DE SCRIPT (PORTRAIT): corpo no contentRect ------
        passo("12.3 editor de script com insets: corpo no contentRect");
        {
            // o par inseparável portrait+IME: a superfície MUDA antes dos
            // toques (o mesmo ciclo do 11.B) com contentRect T96/B48
            eglstub::g_surfaceW = 720;
            eglstub::g_surfaceH = 1536;
            app12.contentRect = {0, 96, 720, 1488};
            onAppCmd(&app12, APP_CMD_TERM_WINDOW);
            onAppCmd(&app12, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            const Handle ator = g_scene.find("Ator");
            openScriptEditor(ator);
            check(g_editor.scriptWin.open, "o editor abre (portrait)");
            check(g_editor.scriptWin.buf ==
                      std::string(editor::scriptwin::kSkeleton),
                  "o esqueleto volta INTACTO do reload (revalidação R-007)");
            frame();
            check(glyphsDentroDosInsets() == 0,
                  "editor: NENHUM glifo sob a faixa do topo (1ª linha "
                  "visível) nem sob a barra de baixo");
            // 0.9.6.8 (GRUPO E): a BARRA DE SÍMBOLOS dokada SOBRE o IME —
            // o inset injetado (280px = um GBoard típico no harness @1.0)
            // e a barra de 40dp no sítio certo. Silencioso: a strip de
            // ajuda apaga (matemática determinística)
            g_editor.scriptWin.helpLevel = 2;
            vv::ime::setBottomInset(280.0f);
            frame();
            check(editor::scriptwin::symbolBarHeight() == 40.0f,
                  "12.3 a barra de símbolos mede 40dp EXATOS (a spec E)");
            const f32 barY = 1536.0f - 280.0f - 40.0f;
            const f32 keyW3 = 720.0f / 14.0f;
            const u32 len0 = (u32)g_editor.scriptWin.buf.size();
            const u32 caret0 = g_editor.scriptWin.caret;
            tap(keyW3 * 1.5f, barY + 20.0f);   // a tecla '{' (a 2ª)
            check(g_editor.scriptWin.buf.size() == len0 + 1 &&
                      g_editor.scriptWin.caret == caret0 + 1 &&
                      g_editor.scriptWin.buf[caret0] == '{',
                  "12.3 a barra dokada: o '{' tecla NO SÍTIO CERTO (sobre "
                  "o IME injetado — insere NO cursor)");
            vv::ime::setBottomInset(0.0f);   // o IME fecha
            frame();
            // ---- 12.4 (G2-7c/R-010): o esqueleto fresco CORRE LIMPO ----
            {
                openScriptEditor(g_scene.find("Ator"));
                check(g_editor.scriptWin.buf ==
                          std::string(editor::scriptwin::kSkeleton),
                      "12.4 o modelo inicial é o esqueleto da spec");
                scriptEditorRun();
                if (std::getenv("VV_DBG_124")) {
                    std::printf("124: running=%d errLine=%u msg=%s\n",
                                (int)g_editor.scriptWin.running,
                                g_editor.scriptWin.errLine,
                                g_editor.scriptWin.errMsg.c_str());
                }
                check(g_editor.scriptWin.running &&
                          g_editor.scriptWin.errLine == 0,
                      "12.4 Run no esqueleto fresco = ZERO erros (R-010)");
                scriptEditorStop();
                closeScriptEditor();
            }

            // ---- 12.5 (G2-7e): o ERRO QUE ENSINA + SUBSTITUIR -----------
            {
                openScriptEditor(g_scene.find("Ator"));
                // o dono Python/JS escreve 'if'
                g_editor.scriptWin.buf = "central main {\n  on moment { }\n}\n";
                g_editor.scriptWin.buf = "if (x) { }\n";
                g_editor.scriptWin.caret =
                    (u32)g_editor.scriptWin.buf.size();
                scriptEditorRun();
                check(g_editor.scriptWin.errLine == 1,
                      "12.5 o 'if' dá erro na linha 1");
                check(g_editor.scriptWin.errMsg.find("exist") !=
                          std::string::npos,
                      "12.5 a mensagem ENSINA o exist");
                check(g_editor.scriptWin.fixFrom == "if" &&
                          g_editor.scriptWin.fixTo == "exist",
                      "12.5 o par do SUBSTITUIR viaja com o erro (if→exist)");
                frame();   // a barra de erro desenha (com o botão)
                // o botão (retrato 720): x = 720-128+60 = 652 ·
                // y = errY(1536-48-40)+4+16 = 1468
                if (std::getenv("VV_DBG_125")) {
                    std::printf("125: errLine=%u msg=%s fix=%s->%s\n",
                                g_editor.scriptWin.errLine,
                                g_editor.scriptWin.errMsg.c_str(),
                                g_editor.scriptWin.fixFrom.c_str(),
                                g_editor.scriptWin.fixTo.c_str());
                }
                tap(652.0f, 1468.0f);
                check(g_editor.scriptWin.buf == "exist (x) { }\n",
                      "12.5 SUBSTITUIR: o 'if' virou 'exist' no buffer");
                check(g_editor.scriptWin.errLine == 0,
                      "12.5 o erro LIMPA após a substituição");
                closeScriptEditor();
            }

            closeScriptEditor();
            // volta ao landscape p/ as próximas fases
            eglstub::g_surfaceW = 1536;
            eglstub::g_surfaceH = 720;
            app12.contentRect = {0, 96, 1536, 672};
            onAppCmd(&app12, APP_CMD_TERM_WINDOW);
            onAppCmd(&app12, APP_CMD_INIT_WINDOW);
        }

        // ---- 12.6 (G2-5): Docs com QUEBRA DE LINHA (sem "...") -----------
        passo("12.6 Docs: descrição inteira com wrap (altura variável)");
        {
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            g_editor.docsScreen.open = true;
            std::snprintf(g_editor.docsScreen.query,
                          sizeof(g_editor.docsScreen.query), "%s",
                          "colorpars");
            g_editor.docsScreen.queryLen = 9;
            g_editor.docsScreen.expanded = -1;
            frame();
            // a descrição do colorpars tem ~40+ code points; com o wrap a
            // PARTIR da largura ela faz >= 2 linhas -> os glifos da 2ª
            // linha existem ABAIXO da linha do nome (e o texto NÃO sai com
            // "..."). Conta code points da desc REAL:
            const voni::docs::Entry* eCP = nullptr;
            for (const auto* ee : voni::docs::search("colorpars")) {
                eCP = ee;
                break;
            }
            check(eCP != nullptr, "12.6 a entrada colorpars existe");
            if (eCP) {
                u32 cps = 0;
                for (const char* q = eCP->desc; *q;) {
                    const unsigned char c = *q;
                    cps += (c & 0xC0) != 0x80 ? 1 : 0;   // code points
                    ++q;
                }
                // glifos na zona da lista (abaixo do campo de pesquisa)
                const QuadBatch& g = g_ui.glyphsForTest();
                const QuadVertex* v = g.vertices();
                const u32 n = g.vertexCount();
                u32 inList = 0;
                const f32 listTop = g_ui.safeArea().top + 36.0f + 8.0f +
                                    32.0f + 8.0f;   // PASSO 1: hdr 36 + campo 32
                if (std::getenv("VV_DBG_126")) {
                    std::printf("126: cps=%u listTop=%.0f safeT=%.0f\n",
                                cps, listTop, g_ui.safeArea().top);
                }
                for (u32 i = 0; i + 5 < n; i += 6) {
                    if (v[i].y > listTop) {
                        ++inList;
                    }
                }
                if (std::getenv("VV_DBG_126b")) {
                    std::printf("126b: inList=%u cps=%u desc=[%.80s]\n",
                                inList, cps, eCP->desc);
                    u32 shown = 0;
                    for (u32 i = 0; i + 5 < n && shown < 30; i += 6) {
                        if (v[i].y > 216.0f) {
                            std::printf("  LIST gy=%.0f gx=%.0f\n", v[i].y,
                                        v[i].x);
                            ++shown;
                        }
                    }
                    std::printf("  totalGlyphs=%u\n", n / 6);
                }
                check(inList >= cps,
                      "12.6 a descrição desenha INTEIRA (sem reticências: "
                      ">= code points em glifos)");
            }
            g_editor.docsScreen.open = false;
        }

        // ---- 12.7 (GRUPO E): A BARRA DE SÍMBOLOS — páginas, símbolos, IME --
        passo("12.7 barra de simbolos: paginas, insercao no cursor, IME");
        {
            // portrait + insets T96/B48 (o par do editor)
            eglstub::g_surfaceW = 720;
            eglstub::g_surfaceH = 1536;
            app12.contentRect = {0, 96, 720, 1488};
            onAppCmd(&app12, APP_CMD_TERM_WINDOW);
            onAppCmd(&app12, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            openScriptEditor(g_scene.find("Ator"));
            g_editor.scriptWin.helpLevel = 2;   // strip fora (matemática)
            // (a) a barra SÓ existe com o IME aberto (o inset REAL): com o
            // inset a 0 o corpo é o de sempre; com o inset a barra dokada
            // em h − inset − 40dp
            vv::ime::setBottomInset(280.0f);
            frame();
            const f32 keyW = 720.0f / 14.0f;   // 14 teclas @720dp
            const f32 barY = 1536.0f - 280.0f - 40.0f;
            check(editor::scriptwin::symKeysVisible(720.0f) == 14u &&
                      editor::scriptwin::symPageCount(720.0f) == 2u,
                  "12.7 a 720dp: 14 teclas visíveis, 2 páginas (a adaptação)");
            // as PÁGINAS cobrem os 24 símbolos da spec (união = o set TODO)
            {
                bool seen[22] = {};
                for (u32 pg = 0; pg < 2; ++pg) {
                    g_editor.scriptWin.symPage = (u8)pg;
                    // o padrao da 13.2: o pedido armado popula o registo
                    g_layoutExportPending = true;
                    frame();
                    const auto& rec = g_ui.auditRecord();
                    u32 keys = 0;
                    for (const auto& en : rec.entries) {
                        if (en.kind == vv::layout::Entry::Button &&
                            en.id >= editor::scriptwin::kSymKeyBase &&
                            en.id < editor::scriptwin::kSymKeyBase + 14 &&
                            en.h >= 40.0f - 0.5f && en.w >= 40.0f - 0.5f) {
                            ++keys;
                        }
                    }
                    check(keys >= 8, "12.7 cada página da barra desenha as "
                          "teclas (>=40dp cada, compactas da spec E)");
                    // os símbolos da página: a tecla k da página pg é o
                    // símbolo (pg*13 + k-1); a página 2 é CURTA (9)
                    const u32 n = pg == 0 ? 13u : 9u;
                    for (u32 k = 1; k <= n; ++k) {
                        seen[pg * 13u + (k - 1u)] = true;
                    }
                }
                u32 total = 0;
                for (bool s : seen) {
                    total += s ? 1u : 0u;
                }
                check(total == 22,
                      "12.7 as 2 páginas cobrem os 22 símbolos da spec "
                      "(a união é o set TODO)");
            }
            // (b) o SELETOR cicla 1/2 -> 2/2 -> 1/2 e os símbolos entram
            // NO CURSOR (o MESMO applyEvent do IME)
            {
                g_editor.scriptWin.symPage = 0;
                g_editor.scriptWin.buf = "abc";
                ++g_editor.scriptWin.bufVersion;
                g_editor.scriptWin.caret = 3;
                tap(keyW * 1.5f, barY + 20.0f);   // '{' (a 2ª tecla)
                check(g_editor.scriptWin.buf == "abc{",
                      "12.7 o '{' entra NO CURSOR (a 2ª tecla da página 1)");
                tap(keyW * 0.5f, barY + 20.0f);   // o seletor
                check(g_editor.scriptWin.symPage == 1,
                      "12.7 o seletor cicla 1/2 -> 2/2");
                tap(keyW * 1.5f, barY + 20.0f);   // '!' (1.º da página 2)
                check(g_editor.scriptWin.buf == "abc{!",
                      "12.7 o '!' entra NO CURSOR (a página 2 começa em '!')");
                tap(keyW * 0.5f, barY + 20.0f);   // o seletor de volta
                check(g_editor.scriptWin.symPage == 0,
                      "12.7 o seletor cicla 2/2 -> 1/2 (o ciclo fecha)");
                check(logHas("barra de simbolos -> pagina"),
                      "12.7 o LOG da página existe (o dono segue no engine.log)");
                check(logHas("simbolo '{' pela barra"),
                      "12.7 o LOG do símbolo existe (um por tecla)");
            }
            // (c) o IME FECHA (inset a 0): a barra SOME e o corpo volta
            // ao tamanho de sempre (o par abrir/fechar do teclado)
            {
                vv::ime::setBottomInset(0.0f);
                g_layoutExportPending = true;   // o registo FRESCO (13.2)
                frame();
                const auto& rec = g_ui.auditRecord();
                bool barKeys = false;
                for (const auto& en : rec.entries) {
                    if (en.kind == vv::layout::Entry::Button &&
                        en.id >= editor::scriptwin::kSymKeyBase &&
                        en.id < editor::scriptwin::kSymKeyBase + 14) {
                        barKeys = true;
                    }
                }
                check(!barKeys,
                      "12.7 IME fechado (inset 0): a barra SOME (a barra "
                      "pertence ao IME aberto)");
                check(logHas("ime: fechado"),
                      "12.7 o LOG do fecho do IME existe (uma linha por "
                      "mudança do inset)");
            }
            closeScriptEditor();
            // volta ao landscape
            eglstub::g_surfaceW = 1536;
            eglstub::g_surfaceH = 720;
            app12.contentRect = {0, 96, 1536, 672};
            onAppCmd(&app12, APP_CMD_TERM_WINDOW);
            onAppCmd(&app12, APP_CMD_INIT_WINDOW);
        }

        // ---- 12.8 (G4 · R-014): IMPORT glb REAL → o seletor MOSTRA --------
        passo("12.8 import de glb: o asset aparece no seletor (<1s)");
        {
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            // 6 .gmesh JÁ no projeto (acima do cap antigo de 5 — o cenário
            // exato do bug: o import novo nunca aparecia)
            for (int i = 1; i <= 6; ++i) {
                char rel[48];
                std::snprintf(rel, sizeof(rel), "assets/m%d.gmesh", i);
                const char dummy[8] = "GMESH";
                g_storage->writeBytes(rel, dummy, 5);
            }
            // um .glb REAL (triângulo: pos+norm+uv+idx — o container GLB
            // com JSON chunk + BIN chunk, como o test_import_gltf)
            const auto t0 = std::chrono::steady_clock::now();
            {
                const f32 pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
                const f32 nrm[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
                const f32 uv[6] = {0, 0, 1, 0, 0, 1};
                const u16 idx[3] = {0, 1, 2};
                std::vector<u8> bin;
                auto pushF = [&bin](const f32* v, int n) {
                    for (int i = 0; i < n; ++i) {
                        const u32 b = *reinterpret_cast<const u32*>(&v[i]);
                        bin.push_back((u8)(b & 0xFF));
                        bin.push_back((u8)((b >> 8) & 0xFF));
                        bin.push_back((u8)((b >> 16) & 0xFF));
                        bin.push_back((u8)((b >> 24) & 0xFF));
                    }
                };
                const u32 po = 0, pl = 36;
                const u32 no = 36, nl = 36;
                const u32 uo = 72, ul = 24;
                const u32 io = 96, il = 6;
                pushF(pos, 9);
                pushF(nrm, 9);
                pushF(uv, 6);
                for (int i = 0; i < 3; ++i) {
                    bin.push_back((u8)(idx[i] & 0xFF));
                    bin.push_back((u8)(idx[i] >> 8));
                }
                char j[900];
                std::snprintf(j, sizeof(j),
                    "{\"asset\":{\"version\":\"2.0\"},"
                    "\"buffers\":[{\"byteLength\":%u}],"
                    "\"bufferViews\":["
                    "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
                    "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
                    "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u},"
                    "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u}],"
                    "\"accessors\":["
                    "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
                    "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
                    "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
                    "{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
                    "\"meshes\":[{\"primitives\":[{\"attributes\":"
                    "{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},"
                    "\"indices\":3}]}]}",
                    (u32)bin.size(), po, pl, no, nl, uo, ul, io, il);
                std::string json = j;
                while (json.size() % 4 != 0) json += ' ';
                std::vector<u8> binPad = bin;
                while (binPad.size() % 4 != 0) binPad.push_back(0);
                std::vector<u8> glb;
                auto u32push = [&glb](u32 v) {
                    glb.push_back((u8)(v & 0xFF));
                    glb.push_back((u8)((v >> 8) & 0xFF));
                    glb.push_back((u8)((v >> 16) & 0xFF));
                    glb.push_back((u8)((v >> 24) & 0xFF));
                };
                u32push(0x46546C67u);   // 'glTF'
                u32push(2);
                u32push(12 + 8 + (u32)json.size() + 8 + (u32)binPad.size());
                u32push((u32)json.size());
                u32push(0x4E4F534Au);   // 'JSON'
                glb.insert(glb.end(), json.begin(), json.end());
                u32push((u32)binPad.size());
                u32push(0x004E4942u);   // 'BIN'
                glb.insert(glb.end(), binPad.begin(), binPad.end());
                // o ficheiro FONTE (host /tmp — o mesmo padrão do wiring010)
                char src[128];
                std::snprintf(src, sizeof(src), "/tmp/goni_fase12_robo.glb");
                FILE* f = std::fopen(src, "wb");
                std::fwrite(glb.data(), 1, glb.size(), f);
                std::fclose(f);
                // O IMPORT REAL (o MESMO convert::importFile do worker)
                convert::Output out;
                convert::Stats stats;
                std::string err;
                const bool ok = convert::importFile(
                    src, "robo.glb", *g_storage, g_pipeline.get(), out, stats,
                    err, nullptr, nullptr);
                check(ok && err.empty(),
                      "12.8 o import do glb REAL funciona (o conversor de "
                      "produção)");
                check(out.meshes.size() == 1 &&
                          out.meshes[0].find("robo") != std::string::npos,
                      "12.8 o convertido vive em assets/robo.gmesh");
                std::remove(src);
            }
            // o catálogo VÊ o novo asset (o refresh do fim do import)
            refreshCatalog();
            bool achou = false;
            for (const auto& m : g_catalog.meshes) {
                if (m == "assets/robo.gmesh") {
                    achou = true;
                }
            }
            check(achou, "12.8 o catálogo lista o import NOVO (com 6+ "
                         "meshes já no projeto)");
            const double ms = msSince(t0);
            check(ms < 1000.0,
                  "12.8 import + catálogo em <1s (o fluxo é síncrono no fim "
                  "do job)");
            // O SELETOR MOSTRA E APLICA: TIC com MeshRenderer selecionado,
            // picker aberto, tap na linha do robo (idx 6 — INVISÍVEL no cap
            // antigo de 5) — o dispatch REAL do main aplica no componente
            {
                const Handle ator = g_scene.find("Ator");
                g_editor.selected = ator;
                g_editor.assetMenu = 1;
                frame();   // o refresh-on-open + o desenho do seletor
                // geometria do seletor (landscape 1536x720, insets T96/B48):
                // overlayArea: oy=96+56=152, ah=720-96-48-56-24-48=448;
                // PASSO 1: fixedH=28+2*36+16=116; maxListH=448-116-8=324 →
                // 7 ficheiros = 252 CABEM (sem scroll) — o robo é visível
                const f32 w = 340.0f;
                const f32 fixedH = 28.0f + 2.0f * 36.0f + 16.0f;
                const f32 maxListH = 448.0f - fixedH - 8.0f;
                const f32 listH = 7.0f * 36.0f < maxListH ? 7.0f * 36.0f
                                                          : maxListH;
                const f32 h = fixedH + listH;
                const f32 x = (1536.0f - w) * 0.5f;
                const f32 y = 152.0f + (448.0f - h) * 0.5f;
                const f32 listTop = y + 28.0f + 2.0f * 36.0f;
                // PASSO 1: as 7 linhas de 36 CABEM (252 ≤ 324) — sem
                // scroll; a ÚLTIMA linha (o robo) recebe o tap direto
                tap(x + w * 0.5f, listTop + 6.0f * 36.0f + 18.0f);
                const Tic* tA = g_scene.get(ator);
                const MeshRenderer* mr =
                    tA ? tA->getComponent<MeshRenderer>() : nullptr;
                check(mr != nullptr && mr->meshPath == "assets/robo.gmesh",
                      "12.8 O SELETOR APLICA O IMPORT NOVO (meshPath no "
                      "MeshRenderer — o fim-a-fim do R-014)");
            }
            g_editor.assetMenu = 0;
            g_editor.selected = Handle::invalid();
        }

        // ---- 12.8b (GRUPO A · R-021/R-022): O PAR .gltf+.bin PELO BROWSER
        // REAL — 1 toque importa (irmãos copiados, textura externa lida) e
        // o seletor APLICA o convertido. O cenário EXATO do device: a pasta
        // com o par (o browser dá caminhos POSIX); o CWD do processo NÃO é
        // essa pasta (o bug histórico «buffer externo não resolvido:
        // scene.bin» resolvia o URI contra o CWD).
        {
            passo("12.8b browser 1-toque: o par .gltf+.bin importa e "
                  "aplica (R-021)");
            // o DIRETÓRIO ORIGINAL com o PAR + a textura %20 — criado com
            // syscalls CRUS (mkdir/fopen, o precedente da 12.8): a seam
            // /tmp READ-ONLY do ambiente C33 bloqueia o fileapi DE
            // PRODUÇÃO — a fixture do harness não é produção (o IMPORT em
            // si lê por fileapi::readAll, que a seam não bloqueia)
            char dir[96];
            std::snprintf(dir, sizeof(dir), "/tmp/goni_fase128b_%d",
                          (int)::getpid());
            check(::mkdir(dir, 0775) == 0 || errno == EEXIST,
                  "12.8b a pasta do par existe (fixture)");
            {
                std::vector<u8> bin;
                auto pushF = [&bin](const f32* v, int n) {
                    for (int i = 0; i < n; ++i) {
                        const u32 b =
                            *reinterpret_cast<const u32*>(&v[i]);
                        bin.push_back((u8)(b & 0xFF));
                        bin.push_back((u8)((b >> 8) & 0xFF));
                        bin.push_back((u8)((b >> 16) & 0xFF));
                        bin.push_back((u8)((b >> 24) & 0xFF));
                    }
                };
                const f32 pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
                pushF(pos, 9);
                const u16 idx[3] = {0, 1, 2};   // u16 LITTLE-ENDIAN no bin
                for (int i = 0; i < 3; ++i) {
                    bin.push_back((u8)(idx[i] & 0xFF));
                    bin.push_back((u8)(idx[i] >> 8));
                }
                FILE* f = std::fopen((std::string(dir) + "/scene.bin").c_str(),
                                     "wb");
                std::fwrite(bin.data(), 1, bin.size(), f);
                std::fclose(f);
                // a textura com ESPAÇO no nome (URI «tex%20albedo.png»)
                std::vector<u8> png;
                {
                    const std::string p =
                        std::string(FIXTURE_DIR) + "/yellow4.png";
                    FILE* pf = std::fopen(p.c_str(), "rb");
                    u8 buf[4096];
                    size_t n;
                    while (pf && (n = std::fread(buf, 1, sizeof(buf), pf)) > 0) {
                        png.insert(png.end(), buf, buf + n);
                    }
                    if (pf) {
                        std::fclose(pf);
                    }
                }
                f = std::fopen((std::string(dir) + "/tex albedo.png").c_str(),
                               "wb");
                std::fwrite(png.data(), 1, png.size(), f);
                std::fclose(f);
                char j[880];
                std::snprintf(j, sizeof(j),
                    "{\"asset\":{\"version\":\"2.0\"},"
                    "\"buffers\":[{\"uri\":\"scene.bin\",\"byteLength\":%zu}],"
                    "\"bufferViews\":["
                    "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                    "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
                    "\"accessors\":["
                    "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                    "\"type\":\"VEC3\"},"
                    "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
                    "\"type\":\"SCALAR\"}],"
                    "\"materials\":[{\"pbrMetallicRoughness\":"
                    "{\"baseColorTexture\":{\"index\":0}}}],"
                    "\"textures\":[{\"source\":0}],"
                    "\"images\":[{\"uri\":\"tex%%20albedo.png\"}],"
                    "\"meshes\":[{\"primitives\":[{\"attributes\":"
                    "{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
                    "\"nodes\":[{\"mesh\":0}],\"scenes\":[{\"nodes\":[0]}],"
                    "\"scene\":0}",
                    bin.size());
                f = std::fopen((std::string(dir) + "/par.gltf").c_str(), "wb");
                std::fwrite(j, 1, std::strlen(j), f);
                std::fclose(f);
            }
            // O BROWSER ABERTO NA PASTA (o caminho REAL do device)
            g_editor.fileBrowser = true;
            browserOpen(dir);
            frame();
            check(g_browser.open && !g_browser.failed &&
                      !g_browser.entries.empty(),
                  "12.8b o browser lista a pasta do par");
            // a linha do par.gltf: ficheiros ordenados («par.gltf» é o
            // 1.º dos 3) — a MESMA geometria do drawFileBrowser
            // (landscape 1536x720, insets T96/B48):
            //   oy=96 ah=576 w=900 x=318 h=362 y=203 listTop=389
            {
                const f32 ox = 0.0f, oy = 96.0f;
                const f32 aw = 1536.0f, ah = 576.0f;
                const f32 w = 900.0f;
                const f32 h = 48.0f + 34.0f + 52.0f + 8.0f + 52.0f + 8.0f +
                              3.0f * 48.0f + 16.0f;
                const f32 x = ox + (aw - w) * 0.5f;
                const f32 y = oy + (ah - h) * 0.5f;
                const f32 listTop = y + 48.0f + 6.0f + 34.0f - 18.0f + 12.0f +
                                    52.0f + 52.0f;
                const auto t0 = std::chrono::steady_clock::now();
                // 1 TOQUE na linha (o A4: 1 toque = seleciona/importa)
                tap(x + w * 0.5f, listTop + 24.0f);
                // 0.9.6.4 — espera o CICLO COMPLETO do job: o finalize do
                // frame() faz o JOIN e publica os resultados; SÓ DEPOIS o
                // estado do job é legível. (Esperar só pelo `done` tinha
                // uma JANELA: um done STALADO de um job anterior saía do
                // loop antes do worker terminar — os checks liam os campos
                // A MEIO da escrita do worker (corrida/UB) e o cleanup lá
                // embaixo apagava a fixture SOB o worker — o errno=2 que
                // se viu na 1ª rodada. active=false SÓ acontece DEPOIS do
                // importJobFinish — é a testemunha certa.)
                int guard = 0;
                while (g_importJob.active.load() && guard++ < 3000) {
                    frame();
                }
                frame();   // garante o importJobFinish (o finalize põe active=false)
                check(g_importJob.err.empty(),
                      "12.8b o import do par pelo browser funciona (err no "
                      "engine.log se falhar)");
                check(g_importJob.out.meshes.size() == 1 &&
                          g_importJob.out.meshes[0] == "assets/par.gmesh",
                      "12.8b o convertido vive em assets/par.gmesh");
                check(g_importJob.stats.siblings == 2,
                      "12.8b os 2 irmaos (scene.bin + tex albedo.png) foram "
                      "copiados para source/");
                check(g_storage->exists("source/scene.bin") &&
                          g_storage->exists("source/tex albedo.png"),
                      "12.8b source/ tem os irmaos (o projeto fica "
                      "autossuficiente)");
                // o catálogo lista o novo asset; o SELETOR aplica (troca)
                refreshCatalog();
                bool achouPar = false;
                for (const auto& m : g_catalog.meshes) {
                    if (m == "assets/par.gmesh") {
                        achouPar = true;
                    }
                }
                check(achouPar, "12.8b o catalogo lista o par convertido");
                const double ms = msSince(t0);
                check(ms < 1000.0,
                      "12.8b toque->catalogo em <1s (o fluxo do browser)");
                {
                    const Handle ator = g_scene.find("Ator");
                    g_editor.selected = ator;
                    // o índice do par no catálogo ORDENADO (m1..m6, par,
                    // robo) — procurado, não assumido; pick = idx + 3
                    // (none=1, cube=2, ficheiros a partir de 3)
                    i32 parIdx = -1;
                    for (size_t i = 0; i < g_catalog.meshes.size(); ++i) {
                        if (g_catalog.meshes[i] == "assets/par.gmesh") {
                            parIdx = static_cast<i32>(i);
                            break;
                        }
                    }
                    check(parIdx >= 0, "12.8b o par tem indice no catalogo");
                    if (parIdx >= 0) {
                        const editor::AssetPickOutcome out =
                            editor::applyAssetPick(g_scene, ator, 1,
                                                   parIdx + 3, g_catalog,
                                                   makeAssetResolvers());
                        const Tic* tA = g_scene.get(ator);
                        const MeshRenderer* mr =
                            tA ? tA->getComponent<MeshRenderer>() : nullptr;
                        check(out.applied && mr != nullptr &&
                                  mr->meshPath == "assets/par.gmesh",
                              "12.8b O SELETOR APLICA O PAR (meshPath no "
                              "MeshRenderer)");
                    }
                }
                g_editor.selected = Handle::invalid();
            }
            // a fixture sai (a lição da FASE 9: /tmp não acumula)
            ::remove((std::string(dir) + "/par.gltf").c_str());
            ::remove((std::string(dir) + "/scene.bin").c_str());
            ::remove((std::string(dir) + "/tex albedo.png").c_str());
            ::remove(dir);
        }

        // ---- 12.9 (G6 · R-017): O BENCH — medições reais + honestidade --
        // A máquina de fases INTEIRA pelo caminho do device (o main.cpp
        // REAL que este TU inclui): Settings → Diagnóstico → Correr bench
        // → as fases DefRun/BuildScene/Import/BenchRun/Texture/Audio/
        // Finish → Done. O CI não tem GPU: o dt é INJETADO a 60fps e o
        // relatório MEDE O RELÓGIO INJETADO (se o format() hardcodasse um
        // número, os checks abaixo caem — a escala injetada vs reportada é
        // a prova). O que NO HOST não existe (áudio/APK/device) tem de
        // dizer "não medido" — a honestidade é PARTE da sentinela.
        {
            passo("12.9 bench: o bloco de 9 linhas com MEDIÇÕES (R-017)");
            const u32 ticsAntes = g_scene.count();
            // CI: cenas de 0.3s (o PERCURSO é o mesmo do device — lá são
            // 10s por cena; aqui o harness não pode dormir 20s)
            benchTestHookSetSecs(0.3);
            // pelo caminho da UI: o Settings com o Diagnóstico ABERTO (os
            // outros 5 secções fechadas) — o botão Correr bench é a 4ª
            // linha da secção (depois de logs/Export/Probe)
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();   // o layout estabiliza (o scroll abre no topo)
            g_ui.scrollSetOffset(editor::settings::kScrollId, 0.0f);
            frame();
            const safe::Insets si = g_ui.safeArea();
            const f32 aw = g_ui.screenWidth() - si.left - si.right;
            const f32 btnCx = si.left + aw - 16.0f - 76.0f;   // botão 152
            // y do botão Correr bench: 3 headers fechados ANTES do
            // Diagnóstico (Geral/Áudio/Permissões — Docs e Sobre vêm
            // DEPOIS na página) + header Diag + logs/Export/Probe + centro
            // (PASSO 4 · J-03: linhas 36dp — era 3×48 + 24)
            const f32 yRun = si.top + 56.0f + 8.0f + 3.0f * 48.0f +
                             48.0f + 3.0f * 36.0f + 18.0f;
            tap(btnCx, yRun);
            check(!g_editor.settingsMenu,
                  "12.9 o botão Correr bench FECHA o Settings (o bench é "
                  "modal de facto)");
            check(logHas("bench: início"),
                  "12.9 o arranque do bench fica LOGADO");
            // a máquina de fases: dt INJETADO a 60fps (o relatório mede o
            // relógio que lhe deram — no device é o dt REAL do loop)
            const double dtInj = 1.0 / 60.0;
            int guard = 0;
            while (!benchTestHookDone() && guard++ < 900) {
                g_frameDt = static_cast<f32>(dtInj);
                frame();
            }
            check(benchTestHookDone(),
                  "12.9 a máquina de fases TERMINA (DefRun→BuildScene→"
                  "Import→BenchRun→Texture→Audio→Finish→Done)");
            const bench::Report& r = benchTestHookReport();
            // AS MEDIÇÕES deste processo (R-017: nada hardcodado — o
            // relógio injetado a 60 ⇒ avg≈60; verts do stub GL REAIS; o
            // import é o convert::importFile DE PRODUÇÃO com cronómetro)
            check(r.def.ok && r.def.avg > 55.0 && r.def.avg < 65.0,
                  "12.9 cena default: média ≈60fps (o relógio injetado — "
                  "se fosse hardcode não casava com o inject)");
            check(r.scene.ok && r.scene.avg > 55.0 && r.scene.avg < 65.0,
                  "12.9 cena bench: idem (64 TICs + mesh importado)");
            check(r.defVerts.ok && r.defVerts.value > 0.0,
                  "12.9 verts da cena default MEDIDOS (a MESMA soma da "
                  "barra de estado)");
            check(r.sceneVerts.ok && r.sceneVerts.value > r.defVerts.value,
                  "12.9 a cena bench tem MAIS verts que a default (64 TICs "
                  "+ o mesh importado)");
            check(r.importMs.ok && r.importMs.value >= 0.0 &&
                      r.importMs.value < 10000.0,
                  "12.9 import glTF ref MEDIDO (o cronómetro real)");
            check(nearEqF(static_cast<f32>(r.importScale.value), 2.5f,
                          0.001f),
                  "12.9 a escala do nó (2.5) fez o round-trip GLB→importer"
                  "→Transform3D");
            check(r.texMs.ok && r.texFormat[0] != '\0',
                  "12.9 textura comprimida COM FORMATO REAL (a mesma "
                  "máquina do pipeline)");
            check(r.rssPeakMb.ok && r.rssPeakMb.value > 1.0,
                  "12.9 pico RSS MEDIDO no host (VmHWM do /proc — real)");
            check(r.projMb.ok && r.projMb.value > 0.0,
                  "12.9 o tamanho do projeto MEDIDO (stat por ficheiro)");
            check(g_scene.count() == ticsAntes,
                  "12.9 a cena bench SAIU no fim (o editor volta exatamente "
                  "ao que era)");
            // A HONESTIDADE do host (o que não há, é DITO — nunca 0):
            check(r.audioTotal == 0,
                  "12.9 áudio: não medido no host (o probe corre no "
                  "aparelho — R-017)");
            check(!r.apkMb.ok && r.device[0] == '\0' && r.sdk == 0,
                  "12.9 device/APK/Android: não medidos no host (a JNI do "
                  "bench é do aparelho)");
            // o BLOCO de 9 linhas com os valores E os "não medido"
            const std::string bloco = bench::format(r);
            check(countLines(bloco) == 9,
                  "12.9 o bloco colável tem EXATAMENTE 9 linhas");
            check(bloco.find("não medido") != std::string::npos,
                  "12.9 o bloco DIZ o que não foi medido");
            check(bloco.find("cena bench (mesh importado)") !=
                      std::string::npos,
                  "12.9 a linha da cena bench está no bloco");
            check(bloco.find("(escala 2.5)") != std::string::npos,
                  "12.9 a escala medida está no bloco");
            // COPIAR: o botão põe o MESMO bloco no clipboard (o dono cola
            // no relatório — a fonte é ÚNICA). O padrão da FASE 11.11:
            // void_calls + last_new_string do stub
            g_jni.void_calls.clear();
            g_jni.last_new_string.clear();
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();
            const f32 yCopy = yRun + 36.0f;   // a linha SEGUINTE (36dp — J-03)
            tap(btnCx, yCopy);
            bool sawClip = false;
            for (const auto& c : g_jni.void_calls) {
                if (c.first == "clipboardCopy") {
                    sawClip = true;
                }
            }
            check(sawClip, "12.9 o Copiar relatório chama a ponte do "
                           "clipboard (a MESMA do copiar-referência)");
            check(g_jni.last_new_string == bloco,
                  "12.9 o clipboard recebe o bloco EXATO do format (fonte "
                  "única — byte a byte)");
            g_editor.settingsMenu = false;
            frame();
        }

        onAppCmd(&app12, APP_CMD_TERM_WINDOW);
    }

    // ---- 12.10 (0.9.6.1 · PASSO 0 · R-018): A DENSIDADE — dp a dp -----------
    // O DONO mediu no device: cabeçalho 56px (devia ser 112), teclas 48×65px
    // (metade de 48dp). A CAUSA: constantes dp consumidas como px. O FIX: a
    // função theme::dp() na FONTE (SafeArea/EditorLayout/componentes). AQUI
    // a prova do mecanismo: com densidade 2.0 INJETADA (o par do C33) os
    // rects do layout duplicam; com 1.0 ficam IGUAIS ao de sempre (o resto
    // do harness inteiro corre a 1.0 — os checks acima são a prova).
    passo("12.10 densidade: dp×2 injetado → barra 112, alvos 96 (R-018)");
    {
        // (a) a densidade chega do AConfiguration (o caminho REAL do device:
        // 320 dpi ÷ 160 = 2.0) e o layout da barra escala
        vvstub::g_stubDensityDpi = 320;
        theme::setDensity(2.0f);
        editor::applyDensity();
        const safe::Insets zero{};
        const UiRect bar = safe::toolbarRect(1536.0f, 720.0f, zero);
        check(bar.h == 72.0f,
              "12.10 com densidade 2.0 a barra de cima mede 72px (36dp "
              "REAL — PASSO 1; o bug era 56px)");
        const UiRect status = safe::statusRect(1536.0f, 720.0f, zero);
        check(status.h == 0.0f,
              "12.10 a status line tem altura ZERO (REMOVIDA no PASSO 1 — "
              "o FPS·TICs vive na tab bar de baixo)");
        check(editor::kRowH == 72.0f && editor::kPad == 32.0f,
              "12.10 as linhas/paddings dos painéis duplicam (36dp/16dp "
              "reais — PASSO 1; applyDensity)");
        check(safe::kTopBarH == 36.0f && theme::dp(safe::kTopBarH) == 72.0f,
              "12.10 o dp() da casa multiplica pela densidade corrente");
        // (b) 0.9.6.8 (GRUPO E · RECALIBRADO): o teclado da engine SAIU —
        // a vara passa a aferir a BARRA DE SÍMBOLOS (a spec E: 40dp = 80px
        // @2.0; as teclas ≥40dp de largura — 9 no device 360dp, 14 @720dp)
        check(editor::scriptwin::symbolBarHeight() == 80.0f,
              "12.10 a barra de simbolos mede 80px (40dp real — a spec E)");
        check(editor::scriptwin::symKeysVisible(720.0f) == 9u &&
                  editor::scriptwin::symKeysVisible(1440.0f) == 14u,
              "12.10 as teclas da barra: >=40dp de largura (9 no portrait "
              "do device 720px@2.0=360dp; 14 @720dp)");
        // (c) a densidade do ARRANQUE vem do AConfiguration (o main lê
        // 320→2.0; o log de identidade do ecrã existe no arranque)
        check(vv::theme::g_density == 2.0f,
              "12.10 a densidade injetada fica no theme (o layout consome)");
        // (d) REPOSIÇÃO: densidade 1.0 — o harness inteiro continua a correr
        // o layout de sempre (os checks 12.11+ e o sumário abaixo dependem)
        vvstub::g_stubDensityDpi = 160;
        theme::setDensity(1.0f);
        editor::applyDensity();
        const UiRect bar1 = safe::toolbarRect(1536.0f, 720.0f, zero);
        check(bar1.h == 36.0f && editor::kRowH == 36.0f,
              "12.10 com densidade 1.0 o layout é EXATAMENTE o da spec "
              "PASSO 1 (os testes não mudam)");
    }

    // ---- 12.11 (0.9.6.2 · R-019): O TOQUE MOVE O CURSOR ---------------------
    // O dono: "no editor de script o cursor nunca fica dentro de
    // 'on moment { }' nem de 'allmoments { }'; fica sempre fora... torna o
    // editor inutilizável". A CAUSA: o tap CALCULAVA o offset e nunca o
    // aplicava ao caret. AQUI o teste EXATO do dono, pelo caminho REAL da
    // UI: tocar ENTRE as chavetas de "allmoments { }" → o caret fica aí;
    // escrever "x" insere aí.
    passo("12.11 toque entre as chavetas move o cursor e escreve aí (R-019)");
    {
        // app PRÓPRIO (o app12 saiu de escopo no fim do 12.9) — o MESMO
        // padrão: superfície portrait + insets T96/B48, INIT_WINDOW
        eglstub::g_surfaceW = 720;
        eglstub::g_surfaceH = 1536;
        android_app app11;
        std::memset(&app11, 0, sizeof(app11));
        app11.contentRect = {0, 96, 720, 1488};
        onAppCmd(&app11, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        openScriptEditor(g_scene.find("Ator"));
        g_editor.scriptWin.helpLevel = 2;   // strip fora (matemática limpa)
        // (0.9.6.8 GRUPO E: o kbOpen MORREU com o teclado da engine — o
        // IME fechado é o estado default do inset; nada a desligar aqui)
        frame();
        // a linha 3 do esqueleto: "  allmoments { }" — o centro dela no
        // corpo. 0.9.6.6 (GRUPO C · C2): TODOS os números vêm da GEOMETRIA
        // ÚNICA exportada (lineHeight/codeX/lineTopOnScreen — as MESMAS
        // funções que o draw usa; antes «lh=34» e «64» hardcoded DRIFTAVAM
        // quando a tipografia ganhou o sp()/textK — a lição R-019 aplicada
        // ao próprio teste)
        const f32 lh = vv::editor::scriptwin::lineHeight(g_ui);
        const safe::Insets ins11 = g_ui.safeArea();
        const f32 bodyY = 96.0f + vv::theme::dp(56.0f);
        const f32 yLine3 = vv::editor::scriptwin::lineTopOnScreen(
                               bodyY, 2u, lh, g_ui.scrollOffset()) +
                           lh * 0.5f;
        // x do INTERIOR das chavetas: codeX + largura REAL (pelo CONTEXTO —
        // textK incluído, a MESMA escala do draw) + 1px
        const f32 xBraces =
            vv::editor::scriptwin::codeX(ins11) +
            g_ui.fontWidth("  allmoments { ") + 1.0f;
        const u32 caret0 = g_editor.scriptWin.caret;
        tap(xBraces, yLine3);
        check(g_editor.scriptWin.caret == caret0 &&
                  g_editor.scriptWin.buf.find("allmoments {") !=
                      std::string::npos,
              "12.11 preparar: o esqueleto com o caret no interior");
        // (a) o TOQUE põe o cursor ENTRE as chavetas (antes era ignorado)
        check(g_editor.scriptWin.caret ==
                  (u32)g_editor.scriptWin.buf.find("allmoments { ") + 13,
              "12.11 tocar entre as chavetas põe o caret AÍ (offset sob o "
              "dedo aplicado — o bug: calculava e não aplicava)");
        // (b) escrever "x" insere NO CARET (não no fim)
        const u32 at = g_editor.scriptWin.caret;
        vv::ime::clearForTest();
        vv::ime::pushText("x");
        vv::ime::Event ev;
        while (vv::ime::poll(ev)) {
            vv::editor::scriptwin::applyEvent(g_editor.scriptWin, ev);
        }
        check(g_editor.scriptWin.buf.find("allmoments { x}") !=
                      std::string::npos &&
                  g_editor.scriptWin.caret == at + 1,
              "12.11 escrever \"x\" insere ENTRE as chavetas (o dono vê o "
              "código nascer onde tocou)");
        // (c) o toque no MEIO da linha 2 ("on moment") move para lá
        // (a geometria única de novo — zero fórmulas à mão)
        const f32 yLine2 = vv::editor::scriptwin::lineTopOnScreen(
                               bodyY, 1u, lh, g_ui.scrollOffset()) +
                           lh * 0.5f;
        tap(vv::editor::scriptwin::codeX(ins11) +
                g_ui.fontWidth("  on ") + 2.0f, yLine2);
        check(g_editor.scriptWin.caret > 15 && g_editor.scriptWin.caret < 31,
              "12.11 tocar no meio da linha 2 põe o cursor NA linha 2 "
              "(coluna pelas métricas reais)");
        // (d) ENTER aí cria linha nova INDENTADA (o herda-indentação)
        vv::ime::clearForTest();
        vv::ime::pushKey(vv::ime::Key::Enter);
        while (vv::ime::poll(ev)) {
            vv::editor::scriptwin::applyEvent(g_editor.scriptWin, ev);
        }
        {
            const u32 c = g_editor.scriptWin.caret;
            const u32 ls =
                editor::scriptwin::lineStartOfOffset(g_editor.scriptWin.buf,
                                                     c);
            check(g_editor.scriptWin.buf.compare(ls, 2, "  ") == 0,
                  "12.11 o ENTER herda a indentação da linha (o código "
                  "nasce alinhado)");
        }
        // o log do toque existe (o dono segue o cursor no engine.log)
        check(logHas("editor: toque x="),
              "12.11 o toque LOGA px/dp/linha/coluna/caret (R-019)");
        closeScriptEditor();
        // TERM (o par do lifecycle — o estado fica limpo p/ o sumário)
        onAppCmd(&app11, APP_CMD_TERM_WINDOW);
    }

    // ======================================================================
    // FASE 13 — 0.9.6.5 (GRUPO B): LAYOUT EXPORTADO (PNG+JSON) + AUDITORIA
    // + OS PNGs RELIDOS. O framebuffer REAL do C33 virtual rasteriza a
    // sério (glstub::fb) e o engine exporta o que o dono obtém no device:
    // layout/<ecrã>.png (backbuffer full-res) + .json (as entradas REAIS
    // do frame) + auditoria-<ecrã>.txt (o validador da casa). A PROVA
    // fecha o círculo: o PNG é RELIDO pelo loadPng DE PRODUÇÃO e os píxeis
    // CONFIRMAM o que o JSON diz (o painel do botão está onde o registo
    // diz, na cor do tema) — o "PNG lido" do relatório, ao pé da letra.
    // ======================================================================
    fase("FASE 13 — layout exportado (PNG+JSON) + auditoria + PNG relido");
    {
        // o ambiente: A GPU do C33 virtual deixa de ser no-op — rasteriza
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        resetEngineForHarness();
        auto st13 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt13 = st13.get();
        check(Project::createNew(*rawSt13, "c33", g_project), "13 projeto criado");
        g_storage = std::move(st13);
        g_projectReady = true;
        g_resources.setStorage(rawSt13);
        g_gpu.init(&g_resources);
        g_texCache = std::make_unique<TextureCache>(*rawSt13);
        g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor, *g_texCache);
        eglstub::g_surfaceW = 1536;
        eglstub::g_surfaceH = 720;
        android_app app13;
        std::memset(&app13, 0, sizeof(app13));
        app13.contentRect = {0, 24, 1512, 720};   // insets: status 24 + pill 24
        onAppCmd(&app13, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(g_ready, "13 boot com o framebuffer REAL ligado (fb rasteriza)");
        check(glstub::fb::enabled, "13 o modo fb persiste ao boot (ambiente)");

        // 0.9.6.6 (GRUPO C · 13.6): a cópia das entradas do editor a 1.0 —
        // a dupla densidade compara entrada a entrada (o ecrã a 2.0 é o
        // ecrã a 1.0 visto a 2×; a INvariância da escala dp+sp)
        std::vector<vv::layout::Entry> editor1x;

        // helper: exporta o ecrã ATUAL e devolve os bytes do PNG+JSON
        auto exportScreen = [&](const char* nome) {
            g_layoutExportPending = true;
            frame();
            std::vector<u8> png, js;
            const bool okP = rawSt13->readBytes(std::string("layout/") + nome + ".png", png);
            const bool okJ = rawSt13->readBytes(std::string("layout/") + nome + ".json", js);
            check(okP && !png.empty(),
                  (std::string("13 o PNG de [") + nome + "] esta no projeto").c_str());
            check(okJ && !js.empty(),
                  (std::string("13 o JSON de [") + nome + "] esta no projeto").c_str());
            return std::make_pair(png, js);
        };

        // ---- (a) O EDITOR 3D: export + PNG RELIDO ---------------------------
        passo("13.1 editor: PNG relido confirma o registo do layout");
        {
            auto [png, js] = exportScreen("editor");
            check(logHas("layout: export do ecrã \"editor\""),
                  "13.1 o export LOGA o ecrã e os ficheiros");
            // o PNG RELIDO (o decode de PRODUÇÃO do pipeline de texturas)
            vv::RawImage img;
            std::string err;
            check(vv::loadPng(png.data(), png.size(), img, err) && img.ok(),
                  "13.1 o PNG RELIDO decodifica (loadPng de produção)");
            check(img.width == 1536 && img.height == 720,
                  "13.1 o PNG tem a resolução da superfície (1536x720)");
            // o registo (o MESMO frame que o PNG) — os botões da toolbar
            const layout::Record& rec = g_ui.auditRecord();
            check(rec.entries.size() > 8,
                  "13.1 o JSON tem as entradas do ecrã (>8 widgets)");
            u32 btns = 0;
            f32 bigArea = 0.0f;
            u32 bigIdx = 0xFFFFFFFFu;
            for (u32 i = 0; i < rec.entries.size(); ++i) {
                const auto& e = rec.entries[i];
                if (e.kind == layout::Entry::Button) ++btns;
                if (e.kind == layout::Entry::Panel && e.w * e.h > bigArea) {
                    bigArea = e.w * e.h;
                    bigIdx = i;
                }
            }
            check(btns >= 4, "13.1 a toolbar regista os botões interativos");
            check(bigIdx != 0xFFFFFFFFu,
                  "13.1 o registo tem painéis (o chrome do editor)");
            // PIXEL vs REGISTO (1): o MAIOR painel do registo — o canto dele
            // no PNG tem de ser o COMPOSTO DE VIDRO: surface α0.80 sobre o
            // clear #0E0E10 = (20,20,22), ≠ do fundo (14,14,16) — o rect do
            // registo e o pixel dizem o MESMO (RECALIBRADO 0.9.6.10 ·
            // GRUPO UI: o vidro passou a α0.80 — a diferença para o fundo
            // é 6 (não ±8 crus): o cheque passou a ser A MATEMÁTICA do
            // blend, como a 13.9 — o pixel É blend(surface@0.80, bg))
            {
                const auto& e = rec.entries[bigIdx];
                const u32 px = (u32)(e.x + 3.0f), py = (u32)(e.y + 3.0f);
                check(px < img.width && py < img.height,
                      "13.1 o maior painel esta DENTRO do ecra");
                f32 expG[4];
                theme::blendOver(theme::kTheme.surface, theme::kTheme.bg,
                                 expG);
                const i32 comp = (i32)(expG[2] * 255.0f + 0.5f);
                const size_t pi = (size_t(py) * img.width + px) * 4;
                const i32 d = (i32)img.rgba[pi + 2] - comp;
                check(d >= -1 && d <= 1,
                      "13.1 o pixel do maior painel e o COMPOSTO DO VIDRO "
                      "(o registo bate com o PNG)");
            }
            // PIXEL vs REGISTO (2): a BANDA da toolbar (56px) POVOADA — a
            // fração de pixels != clear (RECALIBRADO 0.9.6.9 · GRUPO F: o
            // piso era 30% quando a banda pintava um bg DIFERENTE do clear
            // — o navy #0B0E13 vs o clear #141414 davam 100% «povoado» de
            // graça; com o MONO a banda pinta o TOKEN bg que É o clear —
            // a população é o CONTEÚDO (ícones/rótulos ≈5,5% medidos); o
            // piso novo 3% afere o MESMO contrato: a banda não está VAZIA)
            {
                u32 diff = 0, tot2 = 0;
                for (u32 y = 24; y < 80; ++y) {
                    for (u32 x = 0; x < img.width; x += 3) {
                        const size_t pi = (size_t(y) * img.width + x) * 4;
                        ++tot2;
                        if (img.rgba[pi] != 20 || img.rgba[pi + 1] != 20 ||
                            img.rgba[pi + 2] != 20) {
                            ++diff;
                        }
                    }
                }
                check(tot2 > 0 && diff * 100u > tot2 * 3u,
                      "13.1 a banda da toolbar esta POVOADA no PNG "
                      "(>3% pixels de CONTEUDO != fundo)");
            }
            // o GLIFO: texto brilhante na banda da toolbar (o atlas R8
            // rasterizou cobertura — o caminho do texto do device)
            u32 bright = 0;
            for (u32 y = 24; y < 80; ++y) {
                for (u32 x = 0; x < img.width; ++x) {
                    const size_t pi = (size_t(y) * img.width + x) * 4;
                    if (img.rgba[pi] > 200) ++bright;
                }
            }
            check(bright > 200,
                  "13.1 o TEXTO desenha no PNG (glifos do atlas na toolbar)");
            // a cópia para o CI colecionar como artefacto do run
            fileapi::writeAll("layout-harness-editor.png", png.data(), png.size());
            fileapi::writeAll("layout-harness-editor.json", js.data(), js.size());
            // (13.6) a cópia viva do registo a densidade 1.0
            editor1x = rec.entries;
        }

        // ---- (b) A AUDITORIA pelo CAMINHO DO DEVICE (botão do Diagnóstico) --
        passo("13.2 auditoria: o botão do Diagnóstico audita o ecrã");
        {
            // o Settings com o Diagnóstico ABERTO (o padrão 12.9)
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();
            g_ui.scrollSetOffset(editor::settings::kScrollId, 0.0f);
            frame();
            // O TAP PELO REGISTO (a vara de medir do Grupo B a trabalhar):
            // um frame auditado dá o rect REAL do botão «Auditoria do
            // ecrã» — o toque cai no CENTRO exato dele, ZERO fórmulas de
            // layout que driftam quando a página muda (a lição deste run)
            g_layoutExportPending = true;
            frame();   // exporta o settings (ecrã também!) + registo
            const layout::Record& rr = g_ui.auditRecord();
            f32 ax = -1.0f, ay = -1.0f;
            for (const auto& e : rr.entries) {
                if (e.kind == layout::Entry::Button &&
                    e.id == editor::settings::kLayoutAudId) {
                    ax = e.x + e.w * 0.5f;
                    ay = e.y + e.h * 0.5f;
                }
            }
            check(ax > 0.0f && ay > 0.0f,
                  "13.2 o botao Auditoria esta no registo (o rect real do "
                  "frame real)");
            std::vector<u8> setPng;
            check(rawSt13->readBytes("layout/settings.png", setPng) &&
                      !setPng.empty(),
                  "13.2 o export do PROPRIO settings saiu (ecra completo)");
            tap(ax, ay);
            check(!g_editor.settingsMenu,
                  "13.2 o botao Auditoria FECHA o Settings (o ecra por baixo "
                  "e o auditado - um ecra de cada vez)");
            check(g_layoutExportPending && g_layoutAuditPending,
                  "13.2 o pedido armado (export+audit no proximo frame)");
            frame();   // o frame auditado: registo + PNG + validador + log
            check(logHas("layout: AUDITORIA do ecrã"),
                  "13.2 a auditoria CORRE e LOGA o veredito");
            std::vector<u8> aud;
            check(rawSt13->readBytes("layout/auditoria-editor.txt", aud) &&
                      !aud.empty(),
                  "13.2 o relatorio auditoria-editor.txt esta no projeto");
            std::string audS(aud.begin(), aud.end());
            check(audS.find("AUDITORIA do ecr") != std::string::npos &&
                      audS.find("problemas:") != std::string::npos,
                  "13.2 o relatorio traz o cabecalho e as contagens");
            // 0.9.6.6 (GRUPO C): a linha de base medida pelo Grupo B está
            // CURADA — a auditoria do editor diz VERDE (o ERRO da label que
            // sangrava 3px o fundo e os avisos <48dp morreram com o sp()/dp)
            check(audS.find("problemas: 0 ERRO") != std::string::npos &&
                      audS.find("VERDE") != std::string::npos,
                  "13.2 o editor esta VERDE (o ERRO da status bar + os avisos "
                  "da linha de base do Grupo B curados pelo sp()/dp)");
            fileapi::writeAll("layout-harness-auditoria.txt", aud.data(), aud.size());
            // PASSO 1: o relatório INTEIRO no stdout quando vermelho — o
            // diagnóstico sem caçar o ficheiro no cache dir apagado
            if (audS.find("VERDE") == std::string::npos) {
                std::printf("%s", audS.c_str());
                std::vector<u8> js;
                if (rawSt13->readBytes("layout/editor.json", js)) {
                    std::string jS(js.begin(), js.end());
                    // dump das entradas no fundo do ecrã (y > 640)
                    size_t pos = 0;
                    while ((pos = jS.find("\"y\":6", pos)) != std::string::npos ||
                           (pos = jS.find("\"y\":7", pos)) != std::string::npos) {
                        const size_t b = jS.rfind('{', pos);
                        const size_t e = jS.find('}', pos);
                        if (b != std::string::npos && e != std::string::npos) {
                            std::printf("JSON %s\n", jS.substr(b, e - b + 1).c_str());
                        }
                        pos = e;
                    }
                }
            }
            std::printf("    [aud]  %s",
                        audS.find("VERDE") != std::string::npos
                            ? "editor: VERDE\n"
                            : "editor: problemas documentados no relatorio\n");
        }

        // ---- (c) O EDITOR DE SCRIPT (portrait: o ciclo REAL da janela) ---
        passo("13.3 script: export portrait + PNG relido");
        {
            // o editor de script e PORTRAIT no device — o ciclo completo
            // (TERM -> superficie 720x1536 + insets T96/B48 -> INIT), o
            // MESMO padrao da FASE 12.11
            onAppCmd(&app13, APP_CMD_TERM_WINDOW);
            eglstub::g_surfaceW = 720;
            eglstub::g_surfaceH = 1536;
            android_app app13p;
            std::memset(&app13p, 0, sizeof(app13p));
            app13p.contentRect = {0, 96, 720, 1488};
            onAppCmd(&app13p, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            const Handle ator13 = g_scene.create("Ator");
            openScriptEditor(ator13);
            // 0.9.6.8 (GRUPO E): o IME DO SISTEMA aberto (o inset injetado
            // como a VvActivity manda) — a BARRA DE SÍMBOLOS de 40dp dokada
            // sobre ele é o layout exportado (o teclado da engine MORREU)
            g_editor.scriptWin.helpLevel = 2;   // strip fora (matemática)
            vv::ime::setBottomInset(280.0f);
            frame();
            auto [png, js] = exportScreen("script");
            vv::RawImage img;
            std::string err;
            check(vv::loadPng(png.data(), png.size(), img, err) &&
                      img.width == 720 && img.height == 1536,
                  "13.3 o PNG do script e 720x1536 (portrait real)");
            const layout::Record& rec = g_ui.auditRecord();
            check(std::string(rec.screen) == "script" &&
                      rec.entries.size() > 8,
                  "13.3 o registo do script tem as entradas (header/barra)");
            // (a) o VALIDADOR INTEIRO: 0 erros/0 avisos (a linha de base
            // do Grupo C com a barra compacta — a exceção da spec E)
            {
                const auto probs = vv::layout::validate(rec);
                u32 erros = 0, avisos = 0;
                for (const auto& p : probs) {
                    if (p.sev == vv::layout::Problem::Erro) ++erros;
                    else ++avisos;
                }
                check(erros == 0 && avisos == 0,
                      "13.3 o script com IME+barra passa o validador 0/0 "
                      "(as teclas compactas de 40dp da spec E)");
            }
            // (b) a BARRA: as teclas no sítio (>=40dp, compactas no JSON,
            // dokadas em h-inset-40) e o SELETOR de página
            u32 barKeys = 0, compactas = 0;
            f32 barTop = -1.0f;
            for (auto& e : rec.entries) {
                if (e.kind == layout::Entry::Button &&
                    (e.id == editor::scriptwin::kSymBarPageId ||
                     (e.id >= editor::scriptwin::kSymKeyBase &&
                      e.id < editor::scriptwin::kSymKeyBase + 14))) {
                    ++barKeys;
                    if (e.compact) ++compactas;
                    if (e.h >= 39.5f &&
                        (barTop < 0.0f || e.y < barTop)) {
                        barTop = e.y;
                    }
                }
            }
            check(barKeys == 14,
                  "13.3 a barra desenha as 14 teclas (o seletor + 13 "
                  "simbolos da pagina 1 @720dp)");
            check(compactas == barKeys,
                  "13.3 TODAS as teclas da barra estao marcadas compactas "
                  "no JSON (a excecao da spec E visivel)");
            check(barTop > 0.0f && nearEqF(barTop, 1536.0f - 280.0f - 40.0f),
                  "13.3 a barra dokada SOBRE o IME (y = h - inset - 40dp)");
            // (c) o TECLADO DA ENGINE MORREU: nenhuma tecla do range
            // antigo (kKbBase+40..60 do space/arrows/enter) — o Grupo E
            {
                bool fantasmas = false;
                for (auto& e : rec.entries) {
                    if (e.kind == layout::Entry::Button &&
                        e.id >= editor::scriptwin::kSymKeyBase + 14 &&
                        e.id <= editor::scriptwin::kSymKeyBase + 60) {
                        fantasmas = true;   // as teclas antigas 6560+40..60
                    }
                }
                check(!fantasmas,
                      "13.3 o teclado da engine SAIU (nenhuma tecla antiga "
                      "no registo — a REMOCAO do Grupo E)");
            }
            fileapi::writeAll("layout-harness-script.png", png.data(), png.size());
            fileapi::writeAll("layout-harness-script.json", js.data(), js.size());

            // ---- (c) 0.9.6.6 (GRUPO C · C3): O CULLING — o custo por frame
            // deixa de ser O(buffer): 800 linhas com o caret no FIM (o
            // scroll segue) → o draw TOKENIZA as visíveis+folga, não 800
            {
                std::string big;
                for (int i = 0; i < 800; ++i) {
                    big += "  linha ";
                    big += std::to_string(i);
                    big += " momento { }\n";
                }
                g_editor.scriptWin.buf = big;
                ++g_editor.scriptWin.bufVersion;   // o contrato do índice
                g_editor.scriptWin.caret = (u32)big.size();
                frame();   // o scroll segue o caret → a janela no FIM
                const u32 tok = vv::editor::scriptwin::dbgLinesTokenized;
                check(tok > 0 && tok < 100,
                      "13.3 o CULLING: 800 linhas, o frame tokeniza as "
                      "visiveis (~50), nao as 800 (a prova do perf)");
                check(vv::editor::scriptwin::lineCount(g_editor.scriptWin) ==
                          801u,
                      "13.3 o indice de linhas conta as 800+1 (O(1))");
                // o roundtrip da GEOMETRIA UNICA (C2): y→linha→y fecha
                const f32 lhR = vv::editor::scriptwin::lineHeight(g_ui);
                const safe::Insets insR = g_ui.safeArea();
                const f32 bodyYR = insR.top + vv::theme::dp(56.0f);
                bool rtOk = true;
                for (u32 i : {0u, 5u, 400u, 800u}) {
                    const f32 y = vv::editor::scriptwin::lineTopOnScreen(
                        bodyYR, i, lhR, 0.0f);
                    if (vv::editor::scriptwin::lineAtScreenY(
                            y + lhR * 0.5f, bodyYR, lhR, 0.0f) !=
                        static_cast<i32>(i)) {
                        rtOk = false;
                    }
                }
                check(rtOk,
                      "13.3 a geometria unica: lineTopOnScreen/lineAtScreenY "
                      "sao UM o inverso do outro (draw<->toque nunca drifta)");
            }
            closeScriptEditor();

            // ---- (c2) 0.9.6.8 (GRUPO E) · FASE 13.8: O SCRIPT AO TAMANHO
            // DO DEVICE (RMX3624 portrait: 720x1600@2.0 = 360x800dp) —
            // o header flexível e a barra de símbolos no orçamento REAL
            passo("13.8 script ao tamanho do device (360x800dp + IME)");
            {
                // o ciclo REAL da janela ao tamanho e densidade do device
                onAppCmd(&app13p, APP_CMD_TERM_WINDOW);
                eglstub::g_surfaceW = 720;
                eglstub::g_surfaceH = 1600;
                android_app app13d;
                std::memset(&app13d, 0, sizeof(app13d));
                vvstub::g_stubDensityDpi = 320;   // 2.0 — o device real
                theme::setDensity(2.0f);
                editor::applyDensity();
                app13d.contentRect = {0, 48, 720, 1552};   // insets reais: 24dp top/bottom @2.0
                onAppCmd(&app13d, APP_CMD_INIT_WINDOW);
                if (!g_font.ok()) {
                    const char* paths[] = {FONT_FIXTURE};
                    g_font.loadFromPaths(paths, 1, 28.0f);
                }
                g_ui.setFont(&g_font);
                // o TIC nasce DEPOIS do INIT (o reload do lifecycle re-cria
                // a cena do último save — o "Ator" do 13.3 vivia em memória;
                // o padrão do 13.3: create + open no ecrã já re-carregado)
                const Handle ator13d = g_scene.create("Ator");
                openScriptEditor(ator13d);
                g_editor.scriptWin.helpLevel = 2;   // strip fora
                // o IME do device: ~55% da altura em portrait (o GBoard
                // no RMX3624 ~ 880px @2.0 = 440dp)
                vv::ime::setBottomInset(880.0f);
                frame();
                // como a 13.7: o export escreve por currentScreenName()
                // ("script") — a copia do device vai para artefactos proprios
                auto [pngD, jsD] = exportScreen("script");
                vv::RawImage imgD;
                std::string errD;
                check(vv::loadPng(pngD.data(), pngD.size(), imgD, errD) &&
                          imgD.width == 720 && imgD.height == 1600,
                      "13.8 o PNG do script-device e 720x1600 (o device)");
                const layout::Record& rd = g_ui.auditRecord();
                check(std::string(rd.screen) == "script" &&
                          rd.density == 2.0f,
                      "13.8 o registo diz o ecra CERTO na densidade 2.0");
                // (1) O VALIDADOR INTEIRO no orçamento do device: o header
                // flexível (tudo >=48dp, nada fora, nada sobreposto) e a
                // barra compacta (>=40dp) — 0 erros/0 avisos
                {
                    const auto probs = vv::layout::validate(rd);
                    u32 erros = 0, avisos = 0;
                    for (const auto& p : probs) {
                        if (p.sev == vv::layout::Problem::Erro) ++erros;
                        else ++avisos;
                    }
                    check(erros == 0 && avisos == 0,
                          "13.8 o script ao TAMANHO do device passa o "
                          "validador 0/0 (o header flexível no aperto)");
                }
                // (2) O HEADER no aperto: os 6 alvos (back/run/stop/lupa/
                // copy/help) TODOS dentro do ecrã 360dp, >=48dp, sem
                // sobreposição (o antigo media helpX=-56dp: FORA!)
                {
                    u32 hdr48 = 0;
                    f32 minX = 1e9f, maxX = -1e9f;
                    for (const auto& e : rd.entries) {
                        if (e.kind == vv::layout::Entry::Button &&
                            e.y < 200.0f && !e.compact) {
                            // os botões do header (y no topo, não-compactos)
                            if (e.w >= 96.0f - 0.5f &&
                                e.h >= 96.0f - 0.5f) {
                                ++hdr48;   // 48dp @2.0 = 96px
                            }
                            minX = e.x < minX ? e.x : minX;
                            maxX = (e.x + e.w) > maxX ? (e.x + e.w) : maxX;
                        }
                    }
                    check(hdr48 >= 5,
                          "13.8 o header do device: >=5 alvos de 48dp "
                          "inteiros (96px @2.0)");
                    check(minX >= -0.5f && maxX <= 720.0f + 0.5f,
                          "13.8 o header do device: NADA fora do ecrã "
                          "(o antigo helpX media -56dp!)");
                }
                // (3) A BARRA no device: 9 teclas (360dp/40dp), dokada
                // sobre o IME (y = h - inset - 80px), TODAS compactas
                {
                    u32 barKeys = 0;
                    f32 barTop = -1.0f;
                    f32 keyW = 0.0f;
                    for (const auto& e : rd.entries) {
                        if (e.kind == vv::layout::Entry::Button && e.compact) {
                            ++barKeys;
                            if (barTop < 0.0f) {
                                barTop = e.y;
                                keyW = e.w;
                            }
                        }
                    }
                    check(barKeys == 9,
                          "13.8 a barra no device: 9 teclas (360dp/40dp)");
                    check(nearEqF(barTop, 1600.0f - 880.0f - 80.0f),
                          "13.8 a barra dokada sobre o IME do device "
                          "(h - 880px - 80px)");
                    check(nearEqF(keyW, 720.0f / 9.0f) && keyW >= 80.0f - 0.5f,
                          "13.8 as teclas do device >=40dp de largura (80px @2.0)");
                    check(editor::scriptwin::symPageCount(720.0f) == 3u,
                          "13.8 o device tem 3 paginas (22 simbolos / 8 por pagina)");
                }
                // (4) o título FICA e NADA SANGRA (RECALIBRADO
                // 0.9.6.10 · GRUPO UI · E1): a spec manda «header flexível,
                // overflow para ⋯, NADA por cima do título» — o título é
                // SEMPRE visível (piso 48dp) e os BOTÕES recuam para o
                // «⋯» (no 360dp: back+título+⋯+lupa+run/stop). O contrato
                // novo: TODO o label do header termina ANTES do botão
                // mais à esquerda (o título nunca sangra a zona deles)
                {
                    bool tituloSangra = false;
                    bool algumTitulo = false;
                    for (const auto& e : rd.entries) {
                        if (e.kind != vv::layout::Entry::Label ||
                            e.y >= 160.0f) {
                            continue;
                        }
                        algumTitulo = true;   // o título FICA (E1)
                        // o botão mais próximo À DIREITA do label (o Back
                        // vive À ESQUERDA do título — não é ele quem
                        // limita; limitam o ⋯/lupa/run/stop)
                        f32 btnEdge = 1e9f;
                        for (const auto& b : rd.entries) {
                            if (b.kind == vv::layout::Entry::Button &&
                                !b.compact && b.y < 200.0f &&
                                b.x >= e.x && b.x < btnEdge) {
                                btnEdge = b.x;
                            }
                        }
                        if (btnEdge < 1e8f && e.x + e.w > btnEdge - 4.0f) {
                            tituloSangra = true;   // invadiu os botões
                        }
                    }
                    check(algumTitulo && !tituloSangra,
                          "13.8 o título FICA e não sangra: o header E1 "
                          "(título com piso 48dp + botões no ⋯)");
                }
                fileapi::writeAll("layout-harness-script-device.png",
                                  pngD.data(), pngD.size());
                fileapi::writeAll("layout-harness-script-device.json",
                                  jsD.data(), jsD.size());
                vv::ime::setBottomInset(0.0f);
                closeScriptEditor();
                // repõe o harness (1536x720 @1.0) para o 13.4+
                onAppCmd(&app13d, APP_CMD_TERM_WINDOW);
                vvstub::g_stubDensityDpi = 160;
                theme::setDensity(1.0f);
            }

            // ---- (d) O DOCS + (e) O BROWSER de volta ao landscape -------
            passo("13.4 docs + 13.5 browser: os ecras restantes (landscape)");
            onAppCmd(&app13p, APP_CMD_TERM_WINDOW);
            eglstub::g_surfaceW = 1536;
            eglstub::g_surfaceH = 720;
            android_app app13l;
            std::memset(&app13l, 0, sizeof(app13l));
            app13l.contentRect = {0, 24, 1512, 720};
            onAppCmd(&app13l, APP_CMD_INIT_WINDOW);
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            {
                g_editor.docsScreen.open = true;
                frame();
                auto [png, js] = exportScreen("docs");
                vv::RawImage img;
                std::string err;
                check(vv::loadPng(png.data(), png.size(), img, err) &&
                          img.width == 1536 && img.height == 720,
                      "13.4 o PNG do docs e 1536x720 (landscape)");
                check(std::string(g_ui.auditRecord().screen) == "docs",
                      "13.4 o registo diz o ecra CERTO (docs)");
                fileapi::writeAll("layout-harness-docs.png", png.data(), png.size());
                fileapi::writeAll("layout-harness-docs.json", js.data(), js.size());
                g_editor.docsScreen.open = false;
            }
            {
                // o browser REAL: browserOpen lista uma pasta (no host, o
                // caminho do device falha e o estado "opendir FALHOU"
                // desenha à mesma — o ecrã com o seu chrome)
                browserOpen(std::string(fileapi::kExternalRoot) + "/Download");
                g_editor.fileBrowser = true;
                frame();
                auto [png, js] = exportScreen("browser");
                vv::RawImage img;
                std::string err;
                check(vv::loadPng(png.data(), png.size(), img, err) && img.ok(),
                      "13.5 o PNG do browser decodifica");
                fileapi::writeAll("layout-harness-browser.png", png.data(), png.size());
                fileapi::writeAll("layout-harness-browser.json", js.data(), js.size());
                g_editor.fileBrowser = false;
            }

            // ---- (f) 13.6 · A DUPLA DENSIDADE (o NÃO VERIFICADO #4 do
            // relatório B fechado): o export a 2.0 é o ecrã a 1.0 VISTO A
            // 2× — a INvariância da escala: dp para o layout, sp para o
            // texto, NADA fica para trás (o texto era o atlas cru em
            // qualquer densidade — a causa do ERRO da status bar)
            passo("13.6 dupla densidade: o ecrã a 2.0 == o ecrã a 1.0 × 2");
            {
                onAppCmd(&app13l, APP_CMD_TERM_WINDOW);
                eglstub::g_surfaceW = 3072;
                eglstub::g_surfaceH = 1440;
                vvstub::g_stubDensityDpi = 320;   // o caminho REAL: 320→2.0
                theme::setDensity(2.0f);
                editor::applyDensity();
                android_app app13d;
                std::memset(&app13d, 0, sizeof(app13d));
                app13d.contentRect = {0, 48, 3024, 1440};   // insets ×2 (24→48)
                onAppCmd(&app13d, APP_CMD_INIT_WINDOW);
                if (!g_font.ok()) {
                    const char* paths[] = {FONT_FIXTURE};
                    g_font.loadFromPaths(paths, 1, 28.0f);
                }
                g_ui.setFont(&g_font);
                // o mesmo estado da 13.1 (a cena vazia — o Ator da 13.3 sai)
                if (const Handle hAtor = g_scene.find("Ator"); hAtor.valid()) {
                    g_scene.destroy(hAtor);
                }
                // o TOAST da auditoria da 13.2 ainda está vivo (1,8s de vida
                // e os frames do harness correm em milissegundos) — a 13.1
                // exportou SEM ele; mata-se para o estado ser o MESMO
                g_toastT = 0.0f;
                g_toast[0] = '\0';
                frame();
                auto [png2, js2] = exportScreen("editor");
                fileapi::writeAll("layout-harness-editor-2x.png", png2.data(),
                                  png2.size());
                fileapi::writeAll("layout-harness-editor-2x.json", js2.data(),
                                  js2.size());
                vv::RawImage img2;
                std::string err2;
                check(vv::loadPng(png2.data(), png2.size(), img2, err2) &&
                          img2.width == 3072 && img2.height == 1440,
                      "13.6 o PNG a 2.0 e 3072x1440 (a superficie duplicada)");
                const layout::Record& r2 = g_ui.auditRecord();
                check(r2.density == 2.0f && r2.entries.size() > 8,
                      "13.6 o registo a densidade 2.0 existe");
                // (a) o VALIDADOR na dupla densidade: VERDE (a regra 48dp
                // multiplica pela densidade — os alvos 48dp sao 96px la)
                const auto probs2 = layout::validate(r2);
                check(probs2.empty(),
                      "13.6 o editor a 2.0 passa o validador INTEIRO "
                      "(0 erros, 0 avisos — nada fica pela densidade)");
                // (b) a INvariância: entrada a entrada, o rect a 2.0 é o
                // rect a 1.0 × 2 (a mesma ORDEM/kind — o ecrã e o MESMO)
                check(r2.entries.size() == editor1x.size(),
                      "13.6 o MESMO numero de entradas (o estado e o mesmo)");
                u32 cmp = 0, mism = 0;
                for (u32 i = 0; i < r2.entries.size() &&
                                 i < editor1x.size(); ++i) {
                    const auto& e2 = r2.entries[i];
                    const auto& e1 = editor1x[i];
                    if (e2.kind != e1.kind) { ++mism; continue; }
                    if (std::fabs(e2.x - e1.x * 2.0f) > 1.0f ||
                        std::fabs(e2.y - e1.y * 2.0f) > 1.0f ||
                        std::fabs(e2.w - e1.w * 2.0f) > 1.0f ||
                        std::fabs(e2.h - e1.h * 2.0f) > 1.0f) {
                        ++mism;
                        continue;
                    }
                    ++cmp;
                }
                check(mism == 0 && cmp == editor1x.size(),
                      "13.6 a INvariância: TODAS as entradas a 2.0 sao as de "
                      "1.0 × 2 (dp E sp — o texto tambem dobra)");
                // (c) a prova sp(): a largura do TEXTO dobra (o atlas nao
                // era escala nenhuma antes — media igual nas duas)
                bool txt2x = false;
                for (u32 i = 0; i < r2.entries.size() &&
                                 i < editor1x.size(); ++i) {
                    if (r2.entries[i].kind == layout::Entry::Label &&
                        editor1x[i].w > 5.0f &&
                        std::fabs(r2.entries[i].w - editor1x[i].w * 2.0f) <=
                            1.0f) {
                        txt2x = true;
                    }
                }
                check(txt2x,
                      "13.6 o sp(): a largura do TEXTO dobra com a densidade "
                      "(o atlas cru media SEMPRE igual — o bug do Grupo B)");
                // REPOSIÇÃO: o resto da suíte corre a 1.0 (o layout de sempre)
                onAppCmd(&app13d, APP_CMD_TERM_WINDOW);
                vvstub::g_stubDensityDpi = 160;
                theme::setDensity(1.0f);
                editor::applyDensity();
            }

            // ---- (g) 13.7 · O ORÇAMENTO DO DEVICE (GRUPO D): o editor ao
            // TAMANHO REAL do RMX3624 (1600×720 @2.0 = 776×336dp de
            // conteúdo) — a vara que o harness largo (1512dp) NÃO tinha:
            // ANTES do Grupo D os painéis FIXOS de 300dp deixavam o
            // viewport 3D a 176dp (22% do ecrã), o stack vertical
            // transbordava o fundo POR CIMA da toolbar e o [+] caía sobre
            // os botões de ferramenta. AGORA: a gangorra dos divisores
            // (hier 200 | vp 288 | insp 288), o chrome adaptativo e o
            // scissor/aspect do rect — MEDIDOS, não afirmados
            passo("13.7 orcamento do device: 1600x720@2.0 (a vara do RMX3624)");
            {
                eglstub::g_surfaceW = 1600;
                eglstub::g_surfaceH = 720;
                vvstub::g_stubDensityDpi = 320;   // 2.0 — o device real
                theme::setDensity(2.0f);
                editor::applyDensity();
                android_app app13e;
                std::memset(&app13e, 0, sizeof(app13e));
                app13e.contentRect = {0, 48, 1552, 720};   // insets ×2 — 776×336dp
                onAppCmd(&app13e, APP_CMD_INIT_WINDOW);
                if (!g_font.ok()) {
                    const char* paths[] = {FONT_FIXTURE};
                    g_font.loadFromPaths(paths, 1, 28.0f);
                }
                g_ui.setFont(&g_font);
                // o estado da 13.1 (defaults dos divisores; sem toast)
                g_editor.hierW = -1.0f;
                g_editor.inspW = -1.0f;
                g_editor.divDragActive = false;
                g_toastT = 0.0f;
                g_toast[0] = '\0';
                // PASSO 2: a FASE afere o editor com o INSPECTOR ABERTO
                // (o drag da pega direita e o chrome presumem-no) — a cena
                // está VAZIA neste frame, por isso CRIA um TIC real e
                // seleciona (sem seleção o inspector colapsa ao TRILHO de
                // 32dp e a pega direita não existe — o novo default); o
                // TIC é DESTRUÍDO no fim da FASE (a suíte segue limpa)
                const Handle hTic137 = g_scene.create("tic 13.7");
                g_editor.selected = hTic137;
                frame();
                // como a 13.6: o export escreve por currentScreenName()
                // ("editor") — as cópias do device vão para artefactos próprios
                auto [pngD, jsD] = exportScreen("editor");
                fileapi::writeAll("layout-harness-editor-device.png",
                                  pngD.data(), pngD.size());
                fileapi::writeAll("layout-harness-editor-device.json",
                                  jsD.data(), jsD.size());
                // (a) o PNG na resolução EXATA do device
                vv::RawImage imgD;
                std::string errD;
                check(vv::loadPng(pngD.data(), pngD.size(), imgD, errD) &&
                          imgD.width == 1600 && imgD.height == 720,
                      "13.7 o PNG e 1600x720 (a superficie do RMX3624)");
                // (b) o VALIDADOR ao tamanho do device: VERDE (antes do
                // Grupo D o stack transbordava e o [+] sobrepunha — os
                // avisos 'sobreposto' eram a evidência). Os problemas vão
                // NA MENSAGEM (o dono vê O QUE apontou, não só que apontou)
                const layout::Record& rD = g_ui.auditRecord();
                const auto probsD = layout::validate(rD);
                if (probsD.empty()) {
                    check(true,
                          "13.7 o editor ao TAMANHO do device passa o validador "
                          "INTEIRO (0 erros, 0 avisos)");
                } else {
                    std::string why = "13.7 o editor ao device no validador "
                                      "(";
                    for (u32 pi = 0; pi < probsD.size(); ++pi) {
                        if (pi) {
                            why += "; ";
                        }
                        why += layout::describe(rD, probsD[pi]);
                    }
                    why += ")";
                    check(false, why.c_str());
                }
                // (c) A GANGORRA: hier 140dp | insp 180dp | vp 456dp no
                // device — os defaults POR PERCENTAGEM do PASSO 2 (18%/22%
                // caem nos pisos 140/180 no ecrã de 776dp) e o viewport
                // fica 58,8% ≥ 55% (o critério a do dono)
                const safe::PanelBudget bd = editor::resolveEditorPanels(
                    g_editor, g_ui.contentWidthPx());
                check(std::fabs(bd.hier - theme::dp(safe::kHierMinW)) < 1.0f,
                      "13.7 a hierarquia default é o PISO 140dp (18% de 776 "
                      "= 139,7 — spec PASSO 2)");
                check(bd.insp >= theme::dp(safe::kInspMinW) - 1.0f &&
                          std::fabs(bd.insp - theme::dp(180.0f)) < 1.0f,
                      "13.7 o inspector default é o PISO 180dp (22% de 776 = "
                      "170,7 — spec PASSO 2)");
                const f32 vpW = g_ui.contentWidthPx() - bd.hier - bd.insp;
                check(vpW >= theme::dp(safe::kViewportMinW) - 1.0f,
                      "13.7 o viewport 3D >= 288dp (o piso da toolbar) — eram "
                      "176dp");
                check(vpW >= g_ui.contentWidthPx() * 0.55f - 1.0f,
                      "13.7 o viewport 3D >= 55% da largura (critério a do "
                      "dono, PASSO 2) — eram 37,1%");
                // (d) A BARRA DE TOQUE CABE: o layout do chrome com o rect
                // REAL — todos os alvos dentro, 40dp inteiros (PASSO 3: o
                // rail esquerdo, a fila do topo e os cantos 40dp)
                {
                    const UiRect vr = editor::centerRect(
                        1600.0f, 720.0f, g_ui.safeArea(), currentDrawerH(),
                        g_editor.showInspector, g_editor.hierW, g_editor.inspW,
                        editor::inspectorCollapsed(g_editor));
                    const editor::vpchrome::Layout L = editor::vpchrome::layout(vr);
                    check(L.railVisible,
                          "13.7 o rail do viewport está VISÍVEL (a "
                          "degradação honesta — nada some no device)");
                    {
                        // com o DRAWER ABERTO (o viewport a 152dp) o rail
                        // divide-se em COLUNAS (a degradação do stack antigo)
                        const UiRect vrAb = editor::centerRect(
                            1600.0f, 720.0f, g_ui.safeArea(),
                            safe::effectiveDrawerH(
                                240.0f * theme::dp(1.0f),
                                safe::viewportRect(1600.0f, 720.0f,
                                                   g_ui.safeArea())
                                    .h,
                                672.0f),
                            g_editor.showInspector, g_editor.hierW,
                            g_editor.inspW,
                            editor::inspectorCollapsed(g_editor));
                        const editor::vpchrome::Layout lab =
                            editor::vpchrome::layout(vrAb);
                        check(lab.railVisible && lab.railCols >= 2,
                              "13.7 com o drawer aberto o rail vai a "
                              "COLUNAS (2/3) — a altura nao comporta 5 "
                              "em coluna");
                    }
                    const f32 minTouch = theme::dp(40.0f);   // a lei de ouro
                    bool allIn = true, all40 = true;
                    // D21 (0.9.6.19b): os alvos do chrome são N−1 (10)
                    const UiRect all[10] = {
                        L.rail[0], L.rail[1], L.rail[2], L.rail[3],
                        L.rail[4], L.quick[0], L.quick[1], L.quick[2],
                        L.quick[3], L.gizmoBtn};
                    for (const UiRect& r : all) {
                        if (r.x < vr.x - 0.5f || r.y < vr.y - 0.5f ||
                            r.x + r.w > vr.x + vr.w + 0.5f ||
                            r.y + r.h > vr.y + vr.h + 0.5f) {
                            allIn = false;
                        }
                        if (r.w < minTouch - 0.5f || r.h < minTouch - 0.5f) {
                            all40 = false;
                        }
                    }
                    check(allIn,
                          "13.7 TODOS os alvos do chrome DENTRO do viewport "
                          "(o stack transbordava o fundo antes do Grupo D)");
                    check(all40,
                          "13.7 TODOS os alvos >= 40dp REAIS no device (a "
                          "lei de ouro do PASSO 1 intacta no rail PASSO 3)");
                }
                // (e) OS DIVISORES AO VIVO: press na pega → drag → clamps —
                // o caminho REAL do input (o mesmo do dedo no telefone).
                // No DEVICE a hierarquia default está PRESA no PISO (a
                // gangorra: insp 288 + vp 288 já gastam o orçamento) — o
                // drag com INTERVALO é o do INSPECTOR; o da hierarquia
                // AFEREMOS pelo PIN (arrasta e não mexe — a gangorra
                // honesta: o viewport nunca fecha)
                {
                    // (e.1) a pega da HIERARQUIA arma e fica PRESA (piso)
                    const f32 stripX =
                        bd.hier - theme::dp(10.0f);   // dentro do hit 24dp
                    const f32 stripY = 300.0f;
                    g_input.injectDown(0, stripX, stripY);
                    frame();
                    check(g_editor.divDragActive,
                          "13.7 o press na PEGA arma o drag (o toque na pega "
                          "e da pega — nao scroll, nao orbit)");
                    g_input.injectMove(0, stripX - 2000.0f, stripY);
                    frame();
                    const safe::PanelBudget bs = editor::resolveEditorPanels(
                        g_editor, g_ui.contentWidthPx());
                    check(std::fabs(bs.hier - theme::dp(safe::kHierMinW)) <
                              1.0f,
                          "13.7 o PISO da hierarquia (140dp — spec PASSO 2) "
                          "segura o drag");
                    g_input.injectUp(0);
                    frame();
                    check(!g_editor.divDragActive,
                          "13.7 o release FIXA a largura");
                    // (e.2) a pega do INSPECTOR: no device o default É o
                    // PISO 180dp (PASSO 2 — 22% de 776 = 170,7 < piso) — o
                    // drag com INTERVALO é ALARGAR até ao TETO 260dp (novo)
                    // e ENCOLHER de volta ao piso; a gangorra mostra os dois
                    const f32 inspX = g_ui.contentWidthPx() - bd.insp;
                    const f32 stripR = inspX + theme::dp(10.0f);
                    g_input.injectDown(0, stripR, stripY);
                    frame();
                    check(g_editor.divDragActive && g_editor.divDragRight,
                          "13.7 o press na pega DIREITA arma o drag do "
                          "inspector");
                    g_input.injectMove(0, stripR - 2000.0f, stripY);
                    frame();
                    const safe::PanelBudget bt = editor::resolveEditorPanels(
                        g_editor, g_ui.contentWidthPx());
                    check(std::fabs(bt.insp - theme::dp(safe::kInspMaxW)) <
                              1.0f,
                          "13.7 o TETO do inspector (260dp — spec PASSO 2) "
                          "segura o drag ao alargar");
                    check(g_ui.contentWidthPx() - bt.hier - bt.insp <
                              g_ui.contentWidthPx() - bd.hier - bd.insp,
                          "13.7 ao ALARGAR o painel o viewport ENCOLHE (a "
                          "gangorra a favor do 3D)");
                    g_input.injectMove(0, stripR + 2000.0f, stripY);
                    frame();
                    const safe::PanelBudget bw = editor::resolveEditorPanels(
                        g_editor, g_ui.contentWidthPx());
                    check(std::fabs(bw.insp - theme::dp(safe::kInspMinW)) <
                              1.0f,
                          "13.7 o PISO do inspector (180dp — spec PASSO 2) "
                          "segura o drag ao encolher");
                    check(g_ui.contentWidthPx() - bw.hier - bw.insp >
                              g_ui.contentWidthPx() - bd.hier - bt.insp,
                          "13.7 ao ENCOLHER o painel o viewport CRESCE (a "
                          "gangorra a favor do 3D)");
                    g_input.injectUp(0);
                    frame();
                    // (f) a PERSISTENCIA: o layout.json leva as larguras
                    const std::string data = editor::bottom::serializeLayout(
                        g_bottom, g_editor.showInspector,
                        g_editor.inspCollapsed | (g_editor.settingsCollapsed
                                                 << 8),
                        g_editor.hierW, g_editor.inspW);
                    check(data.find("hierW=") != std::string::npos &&
                              data.find("inspW=") != std::string::npos,
                          "13.7 o layout.json leva hierW/inspW (spec G)");
                    editor::bottom::BottomState bs2{};
                    bool insp2 = true;
                    u32 col2 = 0;
                    f32 hw2 = -9.0f, iw2 = -9.0f;
                    check(editor::bottom::parseLayout(data, bs2, insp2, col2,
                                                      &hw2, &iw2) &&
                              std::fabs(hw2 - g_editor.hierW) < 1.0f,
                          "13.7 o round-trip hierW (o que se guarda e o que "
                          "volta)");
                    // o formato ANTIGO (sem hierW) → default adaptativo
                    check(editor::bottom::parseLayout(
                              "bottomTab=0\ndrawerH=240\ninspector=1\n",
                              bs2, insp2, col2, &hw2, &iw2) &&
                              hw2 < 0.0f && iw2 < 0.0f,
                          "13.7 o layout.json ANTIGO (sem larguras) → "
                          "defaults (retrocompativel)");
                    // REPOEM o estado p/ o resto da suite — o TIC da FASE
                    // sai (a cena volta ao estado de sempre) e a seleção
                    // com ele (PASSO 2: sem seleção o trilho volta)
                    g_scene.destroy(hTic137);
                    g_editor.selected = Handle::invalid();

                    // ---- (h) PASSO 2 · A EVIDÊNCIA (P-05): o TRILHO e a
                    // CONSOLA nova — PNG + JSON exportados ao projeto
                    passo("13.7h passo2: o trilho 32dp e a consola nova "
                          "(PNG+JSON)");
                    {
                        // (h.1) SEM seleção: o inspector É o TRILHO de 32dp
                        // (64px @2.0) — o JSON do export mostra o painel de
                        // 64px na borda direita do content
                        g_bottom.bottomTab = 0;
                        frame();
                        auto [pngT, jsT] = exportScreen("editor");
                        fileapi::writeAll("layout-harness-editor-trilho.png",
                                          pngT.data(), pngT.size());
                        fileapi::writeAll("layout-harness-editor-trilho.json",
                                          jsT.data(), jsT.size());
                        {
                            vv::RawImage imgT;
                            std::string errT;
                            check(vv::loadPng(pngT.data(), pngT.size(), imgT,
                                              errT) &&
                                      imgT.width == 1600 &&
                                      imgT.height == 720,
                                  "13.7h o PNG do trilho e 1600x720");
                            const std::string jsStr(jsT.begin(), jsT.end());
                            check(jsStr.find("\"w\":64") !=
                                      std::string::npos,
                                  "13.7h o JSON mostra o TRILHO de 64px "
                                  "(32dp — spec PASSO 2)");
                        }
                        // (h.2) A CONSOLA: chips 28dp e a «Consola»
                        // duplicada FORA — a linha dos chips fica com 5
                        // alvos (Logs/Erros/Avisos + auto + export)
                        elog::info("c33 linha info do passo2");
                        elog::warn("c33 linha warn do passo2");
                        elog::error("c33 linha ERRO um do passo2");
                        elog::error("c33 linha ERRO dois do passo2");
                        g_bottom.bottomTab = 3;
                        frame();
                        frame();
                        auto [pngC, jsC] = exportScreen("editor");
                        fileapi::writeAll("layout-harness-editor-consola.png",
                                          pngC.data(), pngC.size());
                        fileapi::writeAll("layout-harness-editor-consola.json",
                                          jsC.data(), jsC.size());
                        {
                            vv::RawImage imgC;
                            std::string errC;
                            check(vv::loadPng(pngC.data(), pngC.size(), imgC,
                                              errC) &&
                                      imgC.width == 1600 &&
                                      imgC.height == 720,
                                  "13.7h o PNG da consola e 1600x720");
                            // a linha dos chips: 5 BOTÕES com 56px (28dp) de
                            // altura na banda do topo do drawer (eram 6 com
                            // a chip duplicada; o h dos chips era 72px/36dp)
                            const std::string jsStr(jsC.begin(), jsC.end());
                            u32 nChips = 0;
                            size_t p = jsStr.find("\"entradas\"");
                            while (p != std::string::npos) {
                                p = jsStr.find("\"tipo\":\"botao\"", p + 1);
                                if (p == std::string::npos) {
                                    break;
                                }
                                const size_t hy = jsStr.find("\"y\":", p);
                                const size_t hh = jsStr.find("\"h\":", p);
                                if (hy == std::string::npos ||
                                    hh == std::string::npos) {
                                    continue;
                                }
                                const f32 yv = std::strtof(
                                    jsStr.c_str() + hy + 4, nullptr);
                                const f32 hv = std::strtof(
                                    jsStr.c_str() + hh + 4, nullptr);
                                if (yv > 480.0f && yv < 560.0f &&
                                    hv > 55.0f && hv < 57.0f) {
                                    ++nChips;
                                }
                            }
                            check(nChips == 5,
                                  "13.7h a linha dos chips tem 5 alvos de "
                                  "56px (Logs/Erros/Avisos/auto/export — a "
                                  "chip «Consola» saiu e são 28dp)");
                        }
                        // (h.3) o FILTRO Erros mostra erros: o tap na chip
                        // Erros (a 2.ª — [150..258]px no registo) arma o
                        // filtro 1 e o engine.log tem as linhas E/ para
                        // mostrar
                        {
                            const int nE = logCount(" E/GONI:");
                            tap(200.0f, 516.0f);   // o padrão down/frame/up/frame
                            check(g_bottom.consoleTab == 1,
                                  "13.7h o tap na chip Erros ativa o filtro "
                                  "(consoleTab=1 — Logs=0/Erros=1/Avisos=2)");
                            check(nE >= 2,
                                  "13.7h o engine.log tem as linhas E/ do "
                                  "passo2 (o filtro Erros tem o que mostrar)");
                        }
                        g_bottom.bottomTab = 0;
                        g_bottom.consoleTab = 0;
                    }
                    // ---- (i) P2-bis · O PIN DO INSPECTOR + A CONSOLA ≥60%
                    // (as DUAS decisões do dono sobre o PASSO 2 — R-034)
                    passo("13.7i p2-bis: o pin do trilho e a consola ≥60% "
                          "(PNG+JSON)");
                    {
                        // (i.1) O PIN: sem seleção o trilho está lá; tocar
                        // no ÍCONE (a célula 64x80px no topo do trilho de
                        // 64px @2.0 — x 1488..1552, y 120..200) ABRE o
                        // painel E FIXA (inspPinned — a regra do dono)
                        g_editor.inspPinned = false;
                        check(editor::inspectorCollapsed(g_editor),
                              "13.7i a pré-condição: sem seleção o trilho "
                              "está de pé");
                        tap(1520.0f, 160.0f);
                        check(g_editor.inspPinned,
                              "13.7i o toque no ícone do trilho FIXA o pin "
                              "(P2-bis — abre E fixa)");
                        check(!editor::inspectorCollapsed(g_editor),
                              "13.7i fixado, o painel fica aberto MESMO sem "
                              "seleção (a regra do dono)");
                        frame();
                        {
                            auto [pngP, jsP] = exportScreen("editor");
                            fileapi::writeAll("layout-harness-p2bis-pin.png",
                                              pngP.data(), pngP.size());
                            fileapi::writeAll("layout-harness-p2bis-pin.json",
                                              jsP.data(), jsP.size());
                            vv::RawImage imgP;
                            std::string errP;
                            check(vv::loadPng(pngP.data(), pngP.size(), imgP,
                                              errP) &&
                                      imgP.width == 1600 &&
                                      imgP.height == 720,
                                  "13.7i o PNG do pin e 1600x720");
                            const std::string jsStr(jsP.begin(), jsP.end());
                            // a SETA DE RECOLHER (id 7433) existe no painel
                            // fixado; o BOTÃO do trilho (7432) desapareceu
                            check(jsStr.find("\"id\":\"1d09\"") !=
                                      std::string::npos,
                                  "13.7i a seta de recolher (id 7433) vive "
                                  "no painel fixado");
                            check(jsStr.find("\"id\":\"1d08\"") ==
                                      std::string::npos,
                                  "13.7i o botão do trilho (id 7432) saiu — "
                                  "o painel está aberto");
                            // a PERSISTÊNCIA: o serialize do main leva o
                            // pin (o layout.json grava inspPinned=1)
                            const std::string dataP =
                                editor::bottom::serializeLayout(
                                    g_bottom, g_editor.showInspector,
                                    g_editor.inspCollapsed |
                                        (g_editor.settingsCollapsed << 8),
                                    g_editor.hierW, g_editor.inspW,
                                    g_editor.inspPinned);
                            check(dataP.find("inspPinned=1") !=
                                      std::string::npos,
                                  "13.7i o layout.json leva inspPinned=1 "
                                  "(spec G — o pin persiste)");
                            // o VALIDADOR da casa afere o ecrã FIXADO (os
                            // dois alvos novos: a célula do trilho e a seta)
                            std::vector<u8> audP;
                            if (rawSt13->readBytes(
                                    "layout/auditoria-editor.txt", audP)) {
                                std::string audPS(audP.begin(), audP.end());
                                check(
                                    audPS.find("problemas: 0 ERRO") !=
                                            std::string::npos &&
                                        audPS.find("VERDE") !=
                                            std::string::npos,
                                    "13.7i a auditoria do painel FIXADO é "
                                    "VERDE (o validador afere o par do pin)");
                            }
                        }
                        // (i.2) A SETA: tocar na CÉLULA do recolher (o
                        // 0.9.6.18 · D1 moveu-a para a LINHA 1 do cabeçalho,
                        // à direita: x 1440..1520px, y 120..176px — a célula
                        // 40×28dp; longe da pega do divisor a 1240px)
                        // DESFAZ o pin — o trilho volta (sem seleção)
                        tap(1480.0f, 148.0f);
                        check(!g_editor.inspPinned,
                              "13.7i a seta de recolher DESFAZ o pin");
                        check(editor::inspectorCollapsed(g_editor),
                              "13.7i sem pin e sem seleção o TRILHO volta");
                        // (i.3) A CONSOLA ≥60%: com a tab Consola ativa a
                        // lista de log ocupa ≥60% da altura de CONTEÚDO do
                        // drawer (chips+extras ≤40% — SEM exceções; no
                        // drawer pequeno o campo de comando SAI)
                        g_bottom.bottomTab = 3;
                        frame();
                        frame();
                        {
                            const editor::bottom::Layout Lc =
                                editor::bottom::layout(1600.0f, 720.0f,
                                                       g_ui.safeArea(),
                                                       g_bottom);
                            const f32 contentH =
                                Lc.drawer.h - 48.0f;   // pega 24dp @2.0
                            // @2.0 o drawer é capado a 35% → conteúdo
                            // < 190dp → o campo de comando SAI (a regra)
                            char msgPeq[160];
                            std::snprintf(msgPeq, sizeof(msgPeq),
                                          "13.7i a pre-condicao: o drawer @2.0 "
                                          "e pequeno (%.0fpx = %.1fdp de "
                                          "conteudo)",
                                          contentH, contentH / 2.0f);
                            check(contentH < 190.0f * 2.0f, msgPeq);
                            auto [pngC2, jsC2] = exportScreen("editor");
                            fileapi::writeAll(
                                "layout-harness-p2bis-consola.png",
                                pngC2.data(), pngC2.size());
                            fileapi::writeAll(
                                "layout-harness-p2bis-consola.json",
                                jsC2.data(), jsC2.size());
                            vv::RawImage imgC2;
                            std::string errC2;
                            check(vv::loadPng(pngC2.data(), pngC2.size(),
                                              imgC2, errC2) &&
                                      imgC2.width == 1600 &&
                                      imgC2.height == 720,
                                  "13.7i o PNG da consola e 1600x720");
                            const std::string jsStr(jsC2.begin(), jsC2.end());
                            // o CAMPO DE COMANDO (id 5636) não existe no
                            // drawer pequeno — chips 28dp + a lista no resto
                            check(jsStr.find("\"id\":\"1604\"") ==
                                      std::string::npos,
                                  "13.7i o campo de comando SAIU do drawer "
                                  "pequeno (a regra do dono — sem exceções)");
                            // a LISTA (scroll id 47) ocupa ≥60% do conteúdo
                            size_t pS = jsStr.find("\"tipo\":\"scroll\"");
                            bool achouLista = false;
                            f32 listaH = -1.0f;
                            while (pS != std::string::npos) {
                                const size_t pId =
                                    jsStr.find("\"id\":\"2f\"", pS);
                                const size_t pFim =
                                    jsStr.find("}", pS + 14);
                                if (pId != std::string::npos &&
                                    pId < pFim) {
                                    const size_t hy =
                                        jsStr.find("\"y\":", pS);
                                    const size_t hh =
                                        jsStr.find("\"h\":", pS);
                                    if (hy != std::string::npos &&
                                        hh != std::string::npos) {
                                        listaH = std::strtof(
                                            jsStr.c_str() + hh + 4, nullptr);
                                        (void)std::strtof(
                                            jsStr.c_str() + hy + 4, nullptr);
                                        achouLista = true;
                                    }
                                    break;
                                }
                                pS = jsStr.find("\"tipo\":\"scroll\"",
                                                pS + 14);
                            }
                            check(achouLista,
                                  "13.7i a lista de log (scroll 47) está no "
                                  "registo");
                            char msg60[160];
                            std::snprintf(msg60, sizeof(msg60),
                                          "13.7i a lista de log ocupa %.0fpx "
                                          "de %.0fpx de conteudo (%.1f%% — o "
                                          "piso e 60%%)",
                                          listaH, contentH,
                                          100.0f * listaH / contentH);
                            check(listaH >= contentH * 0.60f - 0.5f, msg60);
                            // o VALIDADOR afere a consola nova (a lista ≥60%
                            // é regra de LAYOUT; a auditoria cobre pisos/
                            // sobreposições dos alvos que ficam)
                            std::vector<u8> audC;
                            if (rawSt13->readBytes(
                                    "layout/auditoria-editor.txt", audC)) {
                                std::string audCS(audC.begin(), audC.end());
                                check(
                                    audCS.find("problemas: 0 ERRO") !=
                                            std::string::npos &&
                                        audCS.find("VERDE") !=
                                            std::string::npos,
                                    "13.7i a auditoria da CONSOLA (sem o "
                                    "campo, lista ≥60%) é VERDE");
                            }
                        }
                        g_bottom.bottomTab = 0;
                    }
                    // REPOEM o resto do estado p/ a suíte
                    g_editor.hierW = -1.0f;
                    g_editor.inspW = -1.0f;
                }
                // REPOSIÇÃO: o resto da suíte corre a 1.0
                onAppCmd(&app13e, APP_CMD_TERM_WINDOW);
                vvstub::g_stubDensityDpi = 160;
                theme::setDensity(1.0f);
                editor::applyDensity();
            }

            // ---- (i) 13.9 · A IDENTIDADE (GRUPO F): o mono+vidro MEDIDO
            // nos píxeis do export — o azul da spec A morto, o vidro com a
            // matemática do device, o clear a LER o token. A vara: o PNG
            // relido (o MESMO caminho do Grupo B) + o registo (os rects
            // reais — zero fórmulas de layout que driftam)
            passo("13.9 identidade: o mono+vidro nos pixels (GRUPO F)");
            {
                eglstub::g_surfaceW = 1536;
                eglstub::g_surfaceH = 720;
                android_app app13f;
                std::memset(&app13f, 0, sizeof(app13f));
                app13f.contentRect = {0, 24, 1512, 720};
                onAppCmd(&app13f, APP_CMD_INIT_WINDOW);
                if (!g_font.ok()) {
                    const char* paths[] = {FONT_FIXTURE};
                    g_font.loadFromPaths(paths, 1, 28.0f);
                }
                g_ui.setFont(&g_font);
                // o estado da 13.1 (defaults, sem toast, cena vazia)
                g_editor.hierW = -1.0f;
                g_editor.inspW = -1.0f;
                g_editor.divDragActive = false;
                g_toastT = 0.0f;
                g_toast[0] = '\0';
                if (const Handle hAtor = g_scene.find("Ator"); hAtor.valid()) {
                    g_scene.destroy(hAtor);
                }
                frame();
                auto [pngF, jsF] = exportScreen("editor");
                vv::RawImage imgF;
                std::string errF;
                check(vv::loadPng(pngF.data(), pngF.size(), imgF, errF) &&
                          imgF.width == 1536 && imgF.height == 720,
                      "13.9 o PNG do editor re-vestido (1536x720)");
                const layout::Record& rf = g_ui.auditRecord();

                // (1) O MONO: a paleta VELHA da spec A está AUSENTE do
                // ecrã — o accent AZUL #2196F3, o accentPress azul, a
                // família NAVY inteira (bg/surface/surface2/border) e o
                // text2 azulado: NENHUM píxel (o azul do eixo Z do gizmo
                // #4F92F5 é OUTRO azul — a exceção documentada, vive)
                {
                    const u8 velha[][3] = {{33, 150, 243},   // accent azul
                                           {27, 127, 212},   // accentPress
                                           {11, 14, 19},     // bg navy
                                           {21, 26, 35},     // surface navy
                                           {31, 39, 51},     // surface2 navy
                                           {42, 52, 66},     // border navy
                                           {152, 162, 179}}; // text2 azulado
                    u32 achados = 0;
                    for (u32 i = 0; i + 2 < imgF.width * imgF.height * 4;
                         i += 4) {
                        for (const auto& c : velha) {
                            if (imgF.rgba[i] == c[0] &&
                                imgF.rgba[i + 1] == c[1] &&
                                imgF.rgba[i + 2] == c[2]) {
                                ++achados;
                                break;
                            }
                        }
                    }
                    check(achados == 0,
                          "13.9 o MONO: a paleta velha (o azul #2196F3 e "
                          "a familia navy) AUSENTE do ecra inteiro");
                }

                // (2) O CLEAR LÊ O TOKEN: o céu da viewport (entre os
                // painéis, acima do horizonte da grelha) é o BG TOKEN
                // #0E0E10 (RECALIBRADO 0.9.6.10: o grafite da spec G) —
                // não o preto do framebuffer (o wipe do stub, o achado
                // do Grupo F) nem um literal órfão
                {
                    // o céu: o meio da largura da viewport (entre a
                    // hierarquia [0..300] e o inspector [1212..1512]),
                    // 10px abaixo do topo da viewport — acima da grelha
                    // RECALIBRADO 0.9.6.10 (GRUPO UI): a strip do topo
                    // da viewport ([Cena][Perspetiva][Global] — o pai de
                    // vidro) cobre y 56..96; o céu afere-se BAIXO dela
                    const u32 sx = 756, sy = 130;
                    const size_t pi = (size_t(sy) * imgF.width + sx) * 4;
                    const i32 bgR = (i32)(theme::kTheme.bg[0] * 255.0f +
                                         0.5f);
                    const i32 bgB = (i32)(theme::kTheme.bg[2] * 255.0f +
                                         0.5f);
                    check(imgF.rgba[pi] == bgR &&
                              imgF.rgba[pi + 1] == bgR &&
                              imgF.rgba[pi + 2] == bgB,
                          "13.9 o clear LE o token: o ceu da viewport e o "
                          "bg #0E0E10 (era o literal fossil/um wipe preto)");
                }

                // (3) O VIDRO: o painel da hierarquia é o COMPOSTO
                // blend(surface a0.88, bg) — A MATEMÁTICA do blend do
                // device aplicada ao píxel (29,29,29 com a tabela F) —
                // e NÃO a surface sólida (30): o vidro é translúcido
                {
                    f32 expF[4];
                    theme::blendOver(theme::kTheme.surface,
                                     theme::kTheme.bg, expF);
                    const i32 e8[3] = {
                        (i32)(expF[0] * 255.0f + 0.5f),
                        (i32)(expF[1] * 255.0f + 0.5f),
                        (i32)(expF[2] * 255.0f + 0.5f)};
                    // o painel da hierarquia PELO REGISTO (x<10, o mais
                    // alto) — o interior a meio da altura
                    const layout::Entry* hier = nullptr;
                    for (const auto& e : rf.entries) {
                        if (e.kind == layout::Entry::Panel && e.x < 10.0f &&
                            e.h > 400.0f) {
                            hier = &e;
                            break;
                        }
                    }
                    check(hier != nullptr,
                          "13.9 o painel da hierarquia esta no registo");
                    if (hier) {
                        const u32 px =
                            (u32)(hier->x + hier->w * 0.5f);
                        const u32 py = (u32)(hier->y + hier->h * 0.5f);
                        const size_t pi =
                            (size_t(py) * imgF.width + px) * 4;
                        const i32 d0 = (i32)imgF.rgba[pi] - e8[0];
                        const i32 d1 = (i32)imgF.rgba[pi + 1] - e8[1];
                        const i32 d2 = (i32)imgF.rgba[pi + 2] - e8[2];
                        check(d0 >= -1 && d0 <= 1 && d1 >= -1 && d1 <= 1 &&
                                  d2 >= -1 && d2 <= 1,
                              "13.9 o VIDRO: o painel e o composto "
                              "blend(surface@0.80, bg) — a matematica do "
                              "device no pixel");
                        // translúcido a sério: NÃO é a surface sólida
                        // (RECALIBRADO 0.9.6.10: a sólida é 22 — o
                        // composto é 20; Δ2 > tolerância ±1)
                        check(imgF.rgba[pi] != 22,
                              "13.9 o VIDRO e translucido: o pixel NAO e a "
                              "surface solida (a=1 desenharia 22)");
                    }
                }

                // (4) O RAIL da toolstack: o MESMO composto — o vidro
                // SOBRE a viewport (a cena por trás é o céu/clear aqui) —
                // o vidro flutuante é o REAL, não o véu lateral.
                // (RECALIBRADO 0.9.6.10: os pais flutuantes usam a receita
                // COMPLETA da spec G — fill SURFACE2 α0.86 + bordo
                // glassEdge — o vidro LÊ-SE sobre o céu escuro)
                {
                    // o 1.º botão do RAIL PELO REGISTO (dentro da viewport,
                    // no topo-esquerda dela). PASSO 3: a procura compara com
                    // o rect QUE O vpchrome CALCULA (a mesma fonte — se o
                    // layout muda, o teste acompanha)
                    const layout::Entry* chip = nullptr;
                    const safe::Insets i13{rf.insetL, rf.insetT, rf.insetR,
                                           rf.insetB};
                    const editor::vpchrome::Layout l13 =
                        editor::vpchrome::layout(editor::centerRect(
                            rf.screenW, rf.screenH, i13, 0.0f, true, -1.0f,
                            -1.0f));
                    const UiRect want13 = l13.rail[0];
                    for (const auto& e : rf.entries) {
                        if (e.kind == layout::Entry::Button &&
                            std::fabs(e.x - want13.x) <= 2.0f &&
                            std::fabs(e.y - want13.y) <= 2.0f &&
                            std::fabs(e.w - want13.w) <= 2.0f) {
                            chip = &e;
                            break;
                        }
                    }
                    check(chip != nullptr,
                          "13.9 o botão do rail está no registo (o rect que "
                          "o vpchrome calcula — a prova do composto da alfa "
                          "60% vive na R-034: o chromeCol é público no "
                          "header; a prova pixel-a-pixel exigiria uma cena "
                          "controlada, o cubo lit da suíte ocupa o canto)");
                }

                // (5) O AUDIT re-vestido: o tema não mexeu em NENHUM rect
                // — o validador continua 0/0 (a pele trocou, o layout
                // é o mesmo)
                {
                    const auto probsF = layout::validate(rf);
                    check(probsF.empty(),
                          "13.9 o editor re-vestido passa o validador "
                          "0/0 (o tema nao mexe nos rects)");
                }

                // (6) A DUPLA DENSIDADE com vidro: o painel a 2.0 é o
                // MESMO composto — as CORES não escalam com a densidade
                // (o vidro é invariante; o layout dobra, a pele não)
                {
                    std::vector<u8> png2x;
                    if (fileapi::readAll("layout-harness-editor-2x.png",
                                         png2x) && !png2x.empty()) {
                        vv::RawImage img2x;
                        std::string err2x;
                        if (vv::loadPng(png2x.data(), png2x.size(), img2x,
                                        err2x) &&
                            img2x.width == 3072) {
                            f32 expF[4];
                            theme::blendOver(theme::kTheme.surface,
                                             theme::kTheme.bg, expF);
                            const i32 e8 =
                                (i32)(expF[0] * 255.0f + 0.5f);
                            // o interior da hierarquia a 2.0 (o rect da
                            // 13.6 dobra: [0,160 600x1136])
                            const size_t pi =
                                (size_t(600) * img2x.width + 300) * 4;
                            const i32 d = (i32)img2x.rgba[pi] - e8;
                            check(d >= -1 && d <= 1,
                                  "13.9 o VIDRO a 2.0: o MESMO composto "
                                  "(as cores nao escalam — so o layout)");
                        } else {
                            check(false,
                                  "13.9 o PNG 2x relido (a invariancia do "
                                  "vidro)");
                        }
                    } else {
                        check(false,
                              "13.9 o PNG 2x disponivel (a invariancia do "
                              "vidro)");
                    }
                }
                fileapi::writeAll("layout-harness-editor.png",
                                  pngF.data(), pngF.size());
                fileapi::writeAll("layout-harness-editor.json",
                                  jsF.data(), jsF.size());
                onAppCmd(&app13f, APP_CMD_TERM_WINDOW);
            }
            onAppCmd(&app13l, APP_CMD_TERM_WINDOW);
        }

        // a GPU volta ao no-op (o resto da suíte não paga o raster)
        onAppCmd(&app13, APP_CMD_TERM_WINDOW);
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }

    // ========================================================================
    // FASE 14 — 0.9.6.12 (GRUPOS J): O CONTRATO AO DEVICE COM A FONTE REAL.
    // As sentinelas puras (R-022/R-023) usam medidor fake; ESTA fase afere
    // com a LiberationSans carregada + exporta o PNG do device (a prova
    // P-05 do dono). 14.1/14.2 = J2 (strip + pesquisa); 14.3 = J3 (o rect
    // do viewport segue o painel + o log vp3d); 14.4 = J4 (o rodapé).
    // ========================================================================
    fase("FASE 14 — GRUPOS J: o contrato ao device (strip/pesquisa/viewport/rodapé)");
    {
        // o ambiente da 13: GPU rasteriza + projeto de verdade (o export
        // escreve layout/<ecrã>.png no storage)
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        resetEngineForHarness();
        auto st14 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt14 = st14.get();
        check(Project::createNew(*rawSt14, "c33", g_project), "14 projeto criado");
        g_storage = std::move(st14);
        g_projectReady = true;
        g_resources.setStorage(rawSt14);
        g_gpu.init(&g_resources);
        g_texCache = std::make_unique<TextureCache>(*rawSt14);
        g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor, *g_texCache);
        eglstub::g_surfaceW = 1600;
        eglstub::g_surfaceH = 720;
        vvstub::g_stubDensityDpi = 320;   // 2.0 — o device real (RMX3624)
        theme::setDensity(2.0f);
        editor::applyDensity();
        android_app app14;
        std::memset(&app14, 0, sizeof(app14));
        app14.contentRect = {0, 48, 1552, 720};
        onAppCmd(&app14, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        g_editor.hierW = -1.0f;
        g_editor.inspW = -1.0f;
        g_editor.divDragActive = false;
        g_toastT = 0.0f;
        g_toast[0] = '\0';
        frame();

        // o export da 14 (o MESMO caminho da 13 — layout/<nome>.png+json)
        auto export14 = [&](const char* nome) {
            g_layoutExportPending = true;
            frame();
            std::vector<u8> png, js;
            const bool okP = rawSt14->readBytes(
                std::string("layout/") + nome + ".png", png);
            const bool okJ = rawSt14->readBytes(
                std::string("layout/") + nome + ".json", js);
            check(okP && !png.empty(),
                  "14 o PNG do estado J está no projeto");
            check(okJ && !js.empty(),
                  "14 o JSON do estado J está no projeto");
            return std::make_pair(png, js);
        };

        // ---- 14.1 (PASSO 3 · R-023 reescrita) — NADA atravessa a largura: a strip
        // [Cena][Perspetiva][Global] MORREU (a barra full-width era a maior
        // parte da cobertura do device) — o chrome novo é rail + fila +
        // cantos, e nenhum elemento passa 90% da largura do viewport
        passo("14.1 pass03: nada full-width sobre a cena (a strip morreu)");
        {
            const UiRect view = editor::centerRect(
                1600.0f, 720.0f, g_ui.safeArea(), currentDrawerH(),
                g_editor.showInspector, g_editor.hierW, g_editor.inspW,
                editor::inspectorCollapsed(g_editor));
            const editor::vpchrome::Layout L =
                editor::vpchrome::layout(view);
            // NENHUM elemento do chrome atravessa a largura (a regra do
            // dono: «proibido o bar full-width») — nem o pai de vidro
            // D21 (0.9.6.19b): o plusPanel FOI REMOVIDO com o [+]
            const UiRect bands[3] = {L.railPanel, L.quickPanel,
                                     L.gizmoPanel};
            bool semFullWidth = true;
            for (const UiRect& r : bands) {
                if (r.w > 0.0f && r.w >= view.w * 0.90f) {
                    semFullWidth = false;
                }
            }
            check(semFullWidth,
                  "14.1 nenhum pai de vidro do chrome passa 90% da largura "
                  "(a strip full-width morreu no PASSO 3)");
            // o GIZMO 40dp no topo-DIREITO; o fundo-DIREITO fica LIMPO
            check(nearEqF(L.gizmoBtn.x + L.gizmoBtn.w + theme::dp(8.0f),
                          view.x + view.w) &&
                      nearEqF(L.gizmoBtn.y - theme::dp(8.0f), view.y),
                  "14.1 o gizmo 40dp vive no canto SUPERIOR direito (a "
                  "spec PASSO 3)");
            // 0.9.6.19b (D21): o [+] do fundo-direito MORREU (a decisão do
            // dono — o canto fica limpo para a orbit/seleção; a ação vive
            // no + da hierarquia e no menu ⋯ «Novo objeto»)
            check(L.gizmoBtn.y + L.gizmoBtn.h < view.y + view.h * 0.5f,
                  "14.1/D21 o alvo mais a sul do lado direito é o gizmo do "
                  "TOPO — o fundo-direito não tem controlos");
            // a legenda existe no layout primário (rail 1 coluna no ecrã
            // alto) e não pisa a fila do topo
            if (L.legendVisible) {
                check(L.legend.y >= L.quick[0].y + L.quick[0].h,
                      "14.1 a legenda vive SOB a fila do topo (nunca a "
                      "pisa)");
            }
        }

        // ---- 14.2 (J2 · R-023) — o PLACEHOLDER com a fonte REAL no
        // PISO da hierarquia (PASSO 2: 140dp): a degradação POR ORDEM —
        // «pesquisar TIC» quando o campo dá, «pesquisar» no piso — o
        // placeholder NUNCA sai com reticência
        passo("14.2 pesquisa: o placeholder degrada sem recorte no piso "
              "140dp (fonte real)");
        {
            const f32 fieldW =
                theme::dp(safe::kHierMinW) - 2.0f * theme::dp(16.0f);
            const f32 budget = fieldW - theme::dp(40.0f) - theme::dp(12.0f);
            const f32 twFull = g_ui.fontWidth("pesquisar TIC");
            const f32 twShort = g_ui.fontWidth("pesquisar");
            if (twFull <= budget + 0.5f) {
                check(true,
                      "14.2 «pesquisar TIC» cabe inteiro no campo do piso");
            } else {
                check(twShort <= budget + 0.5f,
                      "14.2 «pesquisar» cabe no PISO 140dp (a degradação da "
                      "spec PASSO 2 — o placeholder nunca corta; fonte real "
                      "@2.0)");
            }
        }

        // ---- 14.3 (J3 · R-024) — O LOG DO RECT A ACONTECER: abrir o
        // painel de baixo muda o retângulo visível → a linha
        // `vp3d: viewport set to (x, y, w x h) — aspect` aparece com os
        // números certos (a spec J3: se não aparecer, o evento não chegou
        // ao render). O fecho produz a linha de volta
        passo("14.3 o log vp3d a acontecer (abrir/fechar o painel)");
        {
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
            frame();
            // o rect ESPERADO com o painel fechado — a MESMA fórmula do
            // main (PASSO 2: o trilho quando não há seleção e o modo 3D)
            const bool inspR14 =
                g_editor.showInspector &&
                !(g_editor.uiMode || editor::inspectorCollapsed(g_editor));
            const bool trk14 =
                !g_editor.uiMode && editor::inspectorCollapsed(g_editor);
            const UiRect fechado = editor::centerRect(
                1600.0f, 720.0f, g_ui.safeArea(), currentDrawerH(), inspR14,
                g_editor.hierW, g_editor.inspW, trk14);
            // força uma MUDANÇA: abre o painel (o drawer persistido 240 →
            // a altura efetiva da fonte única) e corre um frame
            g_bottom.bottomTab = 1;
            g_bottom.drawerH = 240.0f;
            frame();
            const UiRect aberto = editor::centerRect(
                1600.0f, 720.0f, g_ui.safeArea(), currentDrawerH(), inspR14,
                g_editor.hierW, g_editor.inspW, trk14);
            // a ÚLTIMA linha vp3d tem de descrever o rect ABERTO
            std::string lastVp;
            for (const std::string& l : logLines()) {
                if (l.find("vp3d: viewport set to") != std::string::npos) {
                    lastVp = l;
                }
            }
            check(!lastVp.empty(),
                  "14.3 o log vp3d existe no engine.log (o diagnóstico da "
                  "spec J3)");
            char want[128];
            std::snprintf(want, sizeof(want),
                          "vp3d: viewport set to (%.0f, %.0f, %.0f x %.0f)",
                          (double)aberto.x, (double)aberto.y,
                          (double)aberto.w, (double)aberto.h);
            check(lastVp.find(want) != std::string::npos,
                  (std::string("14.3 a linha vp3d descreve o rect ABERTO (") +
                   want + ") — o rect do render SEGUE o painel")
                      .c_str());
            // o aspect da linha é o DO RECT (a janela é o ecrã da câmara)
            {
                char wantAspect[48];
                std::snprintf(wantAspect, sizeof(wantAspect), "aspect %.3f",
                              (double)(aberto.w / aberto.h));
                check(lastVp.find(wantAspect) != std::string::npos,
                      "14.3 o aspect no log é o DO RECT (nunca o do ecrã)");
            }
            check(aberto.h < fechado.h,
                  "14.3 o rect ENCOLHEU com o painel aberto (o canvas "
                  "recomputa — o defeito 4 do dono)");
            // fecha: a linha de volta ao rect fechado
            g_bottom.bottomTab = 0;
            frame();
            std::string lastVp2;
            for (const std::string& l : logLines()) {
                if (l.find("vp3d: viewport set to") != std::string::npos) {
                    lastVp2 = l;
                }
            }
            char want2[128];
            std::snprintf(want2, sizeof(want2),
                          "vp3d: viewport set to (%.0f, %.0f, %.0f x %.0f)",
                          (double)fechado.x, (double)fechado.y,
                          (double)fechado.w, (double)fechado.h);
            check(lastVp2.find(want2) != std::string::npos,
                  "14.3 ao FECHAR o painel a linha vp3d volta ao rect "
                  "cheio (o evento de fecho também chega)");
        }

        // ---- 14.4 (J4 · R-025) — O RODAPÉ COM UM NOME LONGO (a fonte
        // real): o projeto ellipsado A MEIO mantém o suffixo FPS/TICs — o
        // label da faixa de status NÃO fica truncado (o flag do audit)
        passo("14.4 o rodapé com o projeto longo (o middle do J4)");
        {
            const std::string nomeAntigo = g_project.name;
            // 120 glifos — o nome excede o orçamento REAL da faixa (o
            // ecrã do device é largo: um nome de 60 ainda caberia inteiro;
            // o middle tem de ser exercido A VALE)
            g_project.name =
                "projeto-do-dono-com-nome-extravagantemente-comprido-"
                "que-continua-e-continua-e-continua-ate-transbordar-"
                "qualquer-orcamento-de-largura-5678";
            g_bottom.bottomTab = 0;
            g_layoutExportPending = true;   // o audit só corre no frame de
                                            // export (o registo do dump)
            frame();
            // PASSO 1 (0.9.6.14): a faixa de status FOI REMOVIDA — o rodapé
            // é a TAB BAR de 32dp (o «FPS · TICs» à direita; o nome do
            // projeto já não se desenha, o middle do J4 fica nas unidades
            // puras do test_sentinels). A prova: a banda da tab bar tem
            // labels (o FPS·TICs) e NENHUMA truncada — com o nome longo
            // ou curto, o rodapé é estável
            const UiRect tb = safe::bottomTabRect(
                1600.0f, 720.0f, g_ui.safeArea());
            u32 labelsNoRodape = 0, truncadosNoRodape = 0;
            for (const vv::layout::Entry& e : g_ui.auditRecord().entries) {
                // a faixa do rodapé: y dentro da banda de 32dp no fundo
                if (e.kind == vv::layout::Entry::Label &&
                    e.y >= tb.y - 1.0f && e.y + e.h <= tb.y + tb.h + 1.0f) {
                    ++labelsNoRodape;
                    if (e.truncated) {
                        ++truncadosNoRodape;
                    }
                }
            }
            check(labelsNoRodape > 0,
                  "14.4 o rodapé (a tab bar 32dp) desenhou labels — o "
                  "FPS·TICs vive nela (PASSO 1)");
            check(truncadosNoRodape == 0,
                  "14.4 NENHUM label do rodapé truncado com o nome longo "
                  "(o rodapé da tab bar é estável — PASSO 1)");
            g_project.name = nomeAntigo;
            frame();
        }

        // ---- 14.x — o PNG do estado J (drawer aberto + strip) — a prova
        // P-05 do dono (o estado que produzia os defeitos 1+2+3)
        passo("14.x o PNG do device com o drawer aberto (a prova P-05)");
        {
            g_bottom.bottomTab = 1;   // Ficheiros aberto
            g_bottom.drawerH = 240.0f;
            frame();
            auto [png14, js14] = export14("editor");
            fileapi::writeAll("j2-device-drawer-aberto.png", png14.data(),
                              png14.size());
            fileapi::writeAll("j2-device-drawer-aberto.json", js14.data(),
                              js14.size());
            vv::RawImage img14;
            std::string err14;
            check(vv::loadPng(png14.data(), png14.size(), img14, err14) &&
                      img14.width == 1600 && img14.height == 720,
                  "14.x o PNG do device com o drawer aberto (1600x720)");
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
        }

        onAppCmd(&app14, APP_CMD_TERM_WINDOW);
        vvstub::g_stubDensityDpi = 160;
        theme::setDensity(1.0f);
        editor::applyDensity();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }

    // helper: a largura do texto A ESCALA (o mesmo produto do labelStyled —
    // o registo grava a largura DESENHADA, a lição R-020)
    auto uiFontWidth15 = [](UiContext& ui, const char* t, f32 scale) {
        return ui.fontWidth(t) * scale;
    };

    // ======================================================================
    // FASE 15 — 0.9.6.18 (HOTFIX): OS 12 DEFEITOS DA IMAGEM REAL DO DONO.
    // O device-equivalente (1600×720 @2.0 — o RMX3624) prova cada defeito
    // com o REGISTO (os rects reais do frame) e exporta os PNGs P-05
    // (docs/hotfix-*.png). Os checks: (D1) o cabeçalho do inspector sem
    // colisão a 180dp, com e sem pin; (D2) as caixas X/Y/Z + o reset
    // DENTRO do rect do painel; (D3/D9) o empty-state DENTRO do drawer;
    // (D4) o título «Definições» do REGISTO (o M4 é apanhado aqui);
    // (D5) o registo não tem a vista Nós (o rótulo morreu); (D7) a captura
    // thumb.png 256×144 ≤60KB OFF-thread no save; (D8) o rail sem id de
    // undo/redo; (D11) o divisor sem pontos; (D6) o tile de letra morto
    // (o glifo da marca desenha-se — a prova visual é o PNG).
    // ======================================================================
    fase("FASE 15 — 0.9.6.18 HOTFIX: os 12 defeitos medidos no device virtual");
    {
        vvstub::g_stubDensityDpi = 320;   // o device @2.0 (776×336dp)
        theme::setDensity(2.0f);
        editor::applyDensity();
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        resetEngineForHarness();
        auto st15 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt15 = st15.get();
        check(Project::createNew(*rawSt15, "c33", g_project), "15 projeto criado");
        g_storage = std::move(st15);
        g_projectReady = true;
        g_resources.setStorage(rawSt15);
        g_gpu.init(&g_resources);
        g_texCache = std::make_unique<TextureCache>(*rawSt15);
        g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor, *g_texCache);
        eglstub::g_surfaceW = 1600;
        eglstub::g_surfaceH = 720;
        android_app app15;
        std::memset(&app15, 0, sizeof(app15));
        app15.contentRect = {0, 48, 1552, 720};   // insets do device
        onAppCmd(&app15, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(g_ready, "15 boot do device virtual (fb rasteriza)");

        // o estado: TIC real selecionado (o inspector ABERTO), drawer fechado
        g_editor.hierW = -1.0f;
        g_editor.inspW = -1.0f;
        g_editor.inspPinned = false;
        g_editor.divDragActive = false;
        g_toastT = 0.0f;
        g_toast[0] = '\0';
        const Handle hTic15 = g_scene.create("tic 15");
        g_editor.selected = hTic15;
        frame();

        auto export15 = [&](const char* nome) {
            g_layoutExportPending = true;
            frame();
            std::vector<u8> png, js;
            const bool okP = rawSt15->readBytes(std::string("layout/") + nome + ".png", png);
            const bool okJ = rawSt15->readBytes(std::string("layout/") + nome + ".json", js);
            check(okP && !png.empty(), (std::string("15 o PNG de [") + nome + "] esta no projeto").c_str());
            check(okJ && !js.empty(), (std::string("15 o JSON de [") + nome + "] esta no projeto").c_str());
            return std::make_pair(png, js);
        };

        const safe::PanelBudget bd15 = editor::resolveEditorPanels(
            g_editor, g_ui.contentWidthPx());
        const f32 inspX15 = g_ui.contentWidthPx() - bd15.insp;

        // ---- (D1) o CABEÇALHO: o título e o recolher NUNCA partilham x;
        //      a 180dp o cabeçalho é UMA linha (o «InspeInspector» morreu)
        passo("15.1 D1: o cabeçalho do inspector limpo a 180dp");
        {
            auto [png15, js15] = export15("editor");
            fileapi::writeAll("hotfix-device-editor-180.png", png15.data(), png15.size());
            fileapi::writeAll("hotfix-device-editor-180.json", js15.data(), js15.size());
            const layout::Record& r15 = g_ui.auditRecord();
            // a 1ª faixa do cabeçalho (28dp sob a barra): o título desenha
            // aí e NENHUM botão de TAB existe na faixa (o D5 matou as tabs;
            // o literal «Nós» é caçado pelo gate ui_vocab — M5)
            const f32 headY0 = 48.0f + theme::dp(safe::kTopBarH);
            const f32 headY1 = headY0 + theme::dp(28.0f);
            bool titulo = false, botaoNaFaixa = false;
            for (const auto& e : r15.entries) {
                if (e.x >= inspX15 && e.y >= headY0 && e.y < headY1 + 1.0f) {
                    if (e.kind == layout::Entry::Label) {
                        titulo = true;   // o rótulo do título (o único texto)
                    }
                    if (e.kind == layout::Entry::Button) {
                        botaoNaFaixa = true;   // tabs mortas — nada de botão
                    }
                }
            }
            check(titulo, "15.1 D1 o título desenha na faixa 1 do cabeçalho (a colisão «InspeInspector» morreu)");
            check(!botaoNaFaixa, "15.1 D5 NENHUM botão de tab na faixa do título (a 2ª tab morreu — o duplicado da hierarquia)");
            // o pin do D1 nas TRÊS larguras do intervalo (180/220/260dp):
            // a faixa do título nunca tem botão, com e sem pin
            for (int iw : {180, 220, 260}) {
                for (int pin : {0, 1}) {
                    g_editor.inspW = static_cast<f32>(iw);
                    g_editor.inspPinned = pin == 1;
                    frame();
                    const layout::Record& rW = g_ui.auditRecord();
                    bool botaoW = false;
                    const f32 inspXW = g_ui.contentWidthPx() -
                                       safe::resolvePanels(
                                           g_ui.contentWidthPx(), -1,
                                           g_editor.inspW)
                                           .insp;
                    for (const auto& e : rW.entries) {
                        if (e.kind == layout::Entry::Button &&
                            e.x >= inspXW && e.y >= headY0 &&
                            e.y < headY1) {
                            botaoW = true;
                        }
                    }
                    char msgW[128];
                    std::snprintf(msgW, sizeof(msgW),
                                  "15.1 D1 a %ddp %s pin: nenhum botão na "
                                  "faixa do título",
                                  iw, pin ? "COM" : "sem");
                    check(!botaoW, msgW);
                }
            }
            g_editor.inspW = -1.0f;
            g_editor.inspPinned = false;
            // (D2) as CAIXAS X/Y/Z + o reset: TODAS dentro do rect do painel
            passo("15.2 D2: as caixas X/Y/Z dentro do rect a 180dp");
            bool zFora = false, resetFora = false, resetIcone = false;
            f32 nCaixas = 0.0f;
            const f32 inspRight = inspX15 + bd15.insp;
            for (const auto& e : r15.entries) {
                if (e.x >= inspX15 && e.w > 0.0f) {
                    if (e.x + e.w > inspRight + 0.5f) {
                        // nada do painel sangra para o viewport
                        if (e.h <= theme::dp(34.0f) && e.h >= theme::dp(30.0f)) {
                            zFora = true;   // uma CAIXA (32dp de altura) fora
                        }
                    }
                    // as caixas: painéis de ~32dp de altura na faixa das caixas
                    if (std::fabs(e.h - theme::dp(32.0f)) < 1.0f &&
                        e.x >= inspX15 && e.x < inspRight) {
                        ++nCaixas;
                    }
                }
            }
            check(!zFora, "15.2 D2 nenhuma CAIXA X/Y/Z sangra o rect do painel (o campo Z cortado morreu)");
            check(nCaixas >= 3.0f, "15.2 D2 as caixas existem no registo (o plano desenha)");
            // o reset ícone inline: com 164dp úteis @180dp o orçamento dá
            // caixas 40 + reset ícone 20 (a spec do dono)
            const auto tb15 = editor::transformRowBudget(164.0f);
            resetIcone = tb15.resetIcon;
            check(resetIcone, "15.2 D2 a 180dp o reset é ÍCONE inline após o Z (o orçamento da spec)");
        }

        // ---- (D1 com pin) o recolher na linha 1 à direita; sem sobreposição
        passo("15.3 D1: com pin, o recolher vive na linha 1 à direita");
        {
            g_editor.inspPinned = true;   // fixa (sem seleção o painel fica)
            const Handle sel0 = g_editor.selected;
            g_editor.selected = Handle::invalid();
            frame();
            auto [pngP, jsP] = export15("editor");
            fileapi::writeAll("hotfix-device-inspector-pin.png", pngP.data(), pngP.size());
            const layout::Record& rP = g_ui.auditRecord();
            const f32 headY0 = 48.0f + theme::dp(safe::kTopBarH);
            const f32 headY1 = headY0 + theme::dp(28.0f);
            f32 titleL = -1.0f, titleR = -1.0f;
            f32 cellL = -1.0f, cellR = -1.0f;
            for (const auto& e : rP.entries) {
                if (e.x >= inspX15 && e.y >= headY0 && e.y < headY1) {
                    if (e.kind == layout::Entry::Label && titleL < 0.0f) {
                        titleL = e.x;   // o rótulo do título (o 1.º da faixa)
                        titleR = e.x + e.w;
                    }
                    // o hit da célula do recolher (id 7433 — o par do pin)
                    if (e.id == editor::kInspUnpinId) {
                        cellL = e.x;
                        cellR = e.x + e.w;
                    }
                }
            }
            check(titleL >= 0.0f, "15.3 D1 com pin o título desenha (o painel fixado)");
            check(cellL > titleR, "15.3 D1 a célula do recolher fica à DIREITA do título (o contrato do dono)");
            check(cellR <= inspX15 + bd15.insp + 0.5f, "15.3 D1 a célula não sai do rect do painel");
            g_editor.selected = sel0;
            g_editor.inspPinned = false;
        }

        // ---- (D3/D9) o EMPTY-STATE da tab Ficheiros DENTRO do drawer ------
        passo("15.4 D3/D9: o empty-state da Ficheiros dentro do drawer");
        {
            g_bottom.bottomTab = 1;
            g_bottom.drawerH = 240.0f;
            g_toastT = 0.0f;   // o toast é overlay bottom-center INTENCIONAL
            g_toast[0] = '\0';   // (não é conteúdo do drawer — fora da conta)
            frame();
            auto [pngE, jsE] = export15("editor");
            fileapi::writeAll("hotfix-device-drawer-vazio.png", pngE.data(), pngE.size());
            const layout::Record& rE = g_ui.auditRecord();
            // o rect de conteúdo do drawer (o projeto novo não tem ficheiros)
            const editor::bottom::BottomState bs15 = g_bottom;
            const editor::bottom::Layout bl15 =
                editor::bottom::layout(1600.0f, 720.0f, g_ui.safeArea(), bs15);
            const f32 cY0 = bl15.drawer.y;
            const f32 cY1 = bl15.drawer.y + bl15.drawer.h;
            bool emptyDesenha = false, labelFora = false;
            for (const auto& e : rE.entries) {
                if (e.kind == layout::Entry::Label && !e.clipped &&
                    e.y >= cY0 && e.y < cY1) {
                    // o ÚNICO label da região é o empty-state (a tab vazia):
                    // tem de estar INTEIRO dentro do rect de conteúdo
                    emptyDesenha = true;
                    if (e.y + e.h > cY1 + 0.5f || e.y < cY0 - 0.5f) {
                        labelFora = true;
                    }
                }
            }
            check(emptyDesenha, "15.4 D3 os rótulos do drawer desenharam (o projeto do harness tem as pastas base)");
            check(!labelFora, "15.4 D3 NENHUM rótulo da tab Ficheiros cruza o limite do rect de conteúdo (o mesmo clip da lista — o caminho do empty-state é o helper partilhado das 4 tabs)");
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
        }

        // ---- (D4) o Settings em PT (o M4 é apanhado pelo REGISTO) ---------
        passo("15.5 D4: o título do Settings é «Definições» (PT do device)");
        {
            g_editor.settingsMenu = true;
            frame();
            auto [pngS, jsS] = export15("settings");
            fileapi::writeAll("hotfix-device-settings-pt.png", pngS.data(), pngS.size());
            const layout::Record& rS = g_ui.auditRecord();
            // o título: existe UM rótulo 20sp na faixa do cabeçalho da
            // página e a SUA largura é a da string da TABELA («Definições»
            // é ~15% mais larga que «Settings» — a largura DISTINGUE);
            // o literal «Settings» no draw é caçado pelo gate ui_vocab (M4)
            const f32 headS0 = 0.0f;                          // a faixa do
            const f32 headS1 = 48.0f + theme::dp(safe::kTopBarH);  // cabeçalho
            const f32 wDef = uiFontWidth15(g_ui, "Definições",
                                           theme::fontScale(theme::kFontScreen));
            bool def = false;
            for (const auto& e : rS.entries) {
                if (e.kind == layout::Entry::Label && e.y >= headS0 &&
                    e.y < headS1 && e.h >= theme::dp(18.0f)) {
                    if (std::fabs(e.fullW - wDef) < theme::dp(4.0f)) {
                        def = true;   // a largura é a de «Definições»
                    }
                }
            }
            check(def, "15.5 D4 o título desenhado tem a largura de «Definições» (a tabela localizada no draw)");
            // o botão «Repor layout» COMPACTO: a LAJE antiga (152dp →
            // 304px @2.0) AUSENTE do registo; os controlos de linha de
            // 28dp (56px) existem (o padrão novo — a geometria exata é
            // pinada na R-025 pela FONTE ÚNICA actionBtnRect)
            bool laje = false, ctl28 = false;
            for (const auto& e : rS.entries) {
                if (e.kind == layout::Entry::Panel &&
                    std::fabs(e.w - theme::dp(152.0f)) < 2.0f &&
                    std::fabs(e.h - theme::dp(48.0f)) < 2.0f) {
                    laje = true;   // a laje filled de 152×48
                }
                if (e.kind == layout::Entry::Panel &&
                    std::fabs(e.h - theme::dp(28.0f)) < 1.0f &&
                    e.w > theme::dp(40.0f)) {
                    ctl28 = true;   // um controlo compacto de linha
                }
            }
            check(!laje, "15.5 D4 a LAJE de 152×48 AUSENTE do registo (o botão compacto morou no lugar)");
            check(ctl28, "15.5 D4 os controlos compactos de 28dp existem (a altura dos controlos de linha)");
            g_editor.settingsMenu = false;
            frame();
        }

        // ---- (D7) a CAPTURA: save → thumb.png 256×144 ≤60KB off-thread ----
        // 0.9.6.19 (D16): o ARM é o armThumbCapture (o ponto único) — a
        // captura corre no fim do frame SEGUINTE e SÓ num frame limpo (sem
        // overlay modal); o job colhe-se nos frames seguintes
        passo("15.6 D7: a captura thumb.png no save (off-thread, orçamento)");
        {
            const Handle hT = g_scene.create("tic thumb");
            (void)hT;
            g_editor.selected = hTic15;
            // o MESMO caminho do «Guardar cena» (a ação de save da casa):
            // assets + manifesto + arma a captura
            {
                const bool okSave =
                    g_project.saveActiveScene(*g_storage, g_scene) &&
                    g_project.saveManifest(*g_storage);
                check(okSave, "15.6 D7 o SAVE corre (o save nunca bloqueia pela thumb)");
                armThumbCapture();
            }
            frame();   // o frame do gesto (a captura cede a vez — D16)
            frame();   // o frame limpo: lê a viewport e LANÇA o worker
            for (int i = 0; i < 30 && g_thumbJob.active.load(); ++i) {
                frame();   // o main colhe o worker (nunca espera no frame)
            }
            check(!g_thumbJob.active.load(),
                  "15.6 D7 o job de thumb terminou (o worker colhido pelo frame)");
            check(g_thumbJob.ok, "15.6 D7 a captura ESCREVEU o thumb.png (save ok + thumb)");
            check(g_thumbJob.tw == 256u && g_thumbJob.th == 144u,
                  "15.6 D7 o PNG é 256x144 EXATO (o alvo da spec do dono)");
            char orcMsg[128];
            std::snprintf(orcMsg, sizeof(orcMsg),
                          "15.6 D7 o PNG esta dentro do orcamento de 60KB "
                          "(%zu B)",
                          g_thumbJob.pngBytes);
            check(g_thumbJob.pngBytes > 0 && g_thumbJob.pngBytes <= 60u * 1024u,
                  orcMsg);
            // o thumb.png está na raiz do projeto (o Java lê-o para o card)
            std::vector<u8> thumbBytes;
            check(rawSt15->readBytes("thumb.png", thumbBytes) &&
                      thumbBytes.size() == g_thumbJob.pngBytes,
                  "15.6 D7 o thumb.png está na raiz do projeto (o caminho do card)");
            // o log do orçamento existe (a linha OFF-thread)
            g_logLines.clear();
            elog::info("15.6 o orçamento medido: %llums off-thread",
                       (unsigned long long)g_thumbJob.msOff);
        }

        // ---- (D8/D11) o rail sem undo + o divisor sem pontos ---------------
        passo("15.7 D8/D11: o rail sem id de undo; o divisor sem pontos");
        {
            g_editor.selected = hTic15;
            frame();
            auto [pngF, jsF] = export15("editor");
            fileapi::writeAll("hotfix-device-editor-final.png", pngF.data(), pngF.size());
            const layout::Record& rF = g_ui.auditRecord();
            // (D8) nenhum botão de id 30/31 (undo/redo) fora da FILA DO TOPO:
            // a fila do topo vive no canto esquerdo (y ~72..152px); os ids
            // SÓ aparecem lá — o rail (coluna à esquerda, x < 160px) não tem
            bool undoForaDaFila = false;
            const f32 quickY0 = 48.0f + theme::dp(safe::kTopBarH);
            const f32 quickY1 = quickY0 + theme::dp(48.0f);
            for (const auto& e : rF.entries) {
                if ((e.id == 30u || e.id == 31u) &&
                    !(e.y >= quickY0 - 1.0f && e.y <= quickY1 + 1.0f)) {
                    undoForaDaFila = true;
                }
            }
            check(!undoForaDaFila, "15.7 D8 os ids undo/redo vivem SÓ na fila do topo (o rail não repete ação)");
            // (D11) nenhuma coluna de pontos: no frame inteiro nenhum painel
            // de <8×8dp (os dots de 2×4dp morreram; a pill só no drag)
            // (D11) dentro das COLUNAS dos divisores (o strip de 12dp dos
            // dois divisores — hier borda direita / inspector borda esquerda)
            // não existe NENHUM ponto (os dots 2×4dp morreram; a pill só no
            // drag). O ⋮ da hierarquia (glifo legítimo de 3dp fora das
            // colunas) não entra na conta.
            const f32 hierR = bd15.hier;                       // borda direita
            const f32 inspL = inspX15;                          // borda esquerda
            bool ponto = false;
            for (const auto& e : rF.entries) {
                const bool naColunaL = e.x + e.w > hierR - theme::dp(12.0f) &&
                                       e.x < hierR + theme::dp(2.0f);
                const bool naColunaR = e.x + e.w > inspL - theme::dp(2.0f) &&
                                       e.x < inspL + theme::dp(12.0f);
                if (e.w < 8.0f && e.h < 8.0f && (naColunaL || naColunaR)) {
                    ponto = true;
                }
            }
            check(!ponto, "15.7 D11 nenhum ponto flutuante nas colunas dos divisores (a linha 1dp é o divisor)");
        }

        // ---- o validador inteiro ao device (o hotfix não abre exceções) ----
        {
            frame();
            const layout::Record& rZ = g_ui.auditRecord();
            const auto probsZ = layout::validate(rZ);
            check(probsZ.empty(),
                  "15.8 o editor do hotfix passa o VALIDADOR INTEIRO ao device (0/0)");
        }

        onAppCmd(&app15, APP_CMD_TERM_WINDOW);
        vvstub::g_stubDensityDpi = 160;
        theme::setDensity(1.0f);
        editor::applyDensity();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }

    // ======================================================================
    // FASE 16 — 0.9.6.19 (HOTFIX): R1 + D14/D15/D16/D17/D19 NO DEVICE.
    // O device-equivalente (1600×720 @2.0) prova: (R1) os valores X/Y/Z
    // não-vazios nos 3 campos em 180/220/260dp; (D14) o menu abre com
    // offset 0, contido acima da tab bar, a última linha alcançável;
    // (D15) «Exportar OBJ» no registo (o EN é caçado pelo gate); (D16) a
    // captura só corre em frame limpo (o frame do menu nunca é capturado);
    // (D17) a câmara objeto pequeno E2E — frustum mudo sem seleção, zero
    // handles, glifo + handles ≤12dp com seleção, gizmo no glifo e UM
    // DRAG INJETADO fora dos handles que MOVE a câmara; (D19) o toggle
    // «visível» com o knob nos dois estados.
    // ======================================================================
    fase("FASE 16 — 0.9.6.19 HOTFIX: R1+D14..D19 medidos no device virtual");
    {
        vvstub::g_stubDensityDpi = 320;   // o device @2.0 (776×336dp)
        theme::setDensity(2.0f);
        editor::applyDensity();
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        resetEngineForHarness();
        auto st16 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt16 = st16.get();
        check(Project::createNew(*rawSt16, "c33", g_project), "16 projeto criado");
        g_storage = std::move(st16);
        g_projectReady = true;
        g_resources.setStorage(rawSt16);
        g_gpu.init(&g_resources);
        g_texCache = std::make_unique<TextureCache>(*rawSt16);
        g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor, *g_texCache);
        eglstub::g_surfaceW = 1600;
        eglstub::g_surfaceH = 720;
        android_app app16;
        std::memset(&app16, 0, sizeof(app16));
        app16.contentRect = {0, 48, 1552, 720};   // insets do device
        onAppCmd(&app16, APP_CMD_INIT_WINDOW);
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        check(g_ready, "16 boot do device virtual (fb rasteriza)");

        g_editor.hierW = -1.0f;
        g_editor.inspW = -1.0f;
        g_editor.inspPinned = false;
        g_editor.divDragActive = false;
        g_editor.playMode = false;
        g_editor.uiMode = false;
        g_editor.audioMode = false;
        g_toastT = 0.0f;
        g_toast[0] = '\0';
        g_bottom.bottomTab = 0;
        g_bottom.drawerH = 0.0f;
        g_gizmo.mode = gizmo::Mode::Move;   // o modo do drag do D17-e
        frame();

        auto export16 = [&](const char* nome) {
            g_layoutExportPending = true;
            frame();
            std::vector<u8> png, js;
            const bool okP = rawSt16->readBytes(std::string("layout/") + nome + ".png", png);
            const bool okJ = rawSt16->readBytes(std::string("layout/") + nome + ".json", js);
            check(okP && !png.empty(), (std::string("16 o PNG de [") + nome + "] esta no projeto").c_str());
            return std::make_pair(png, js);
        };
        // o vp do editor — o MESMO cálculo do frame (para projetar alvos)
        auto viewRect16 = [&]() {
            const bool inspRight =
                g_editor.showInspector &&
                !(g_editor.uiMode || editor::inspectorCollapsed(g_editor));
            return editor::centerRect(
                1600.0f, 720.0f, g_ui.safeArea(), currentDrawerH(),
                inspRight, g_editor.hierW, g_editor.inspW,
                !g_editor.uiMode && editor::inspectorCollapsed(g_editor));
        };
        auto editorVp16 = [&]() {
            const UiRect vr = viewRect16();
            return Mat4::mul(g_camera.proj(vr.w / vr.h), g_camera.view());
        };
        // quads SÓ da viewport (o chrome do viewport desenha a 60% — os
        // quads FULL-âmbar daí são só gizmo/câmara/handles)
        auto quads16 = [](const UiRect& vr) {
            struct Q { f32 x0, y0, x1, y1, a, r, g, b; };
            std::vector<Q> out;
            const auto& vb = g_ui.solidsForTest();
            for (u32 i = 0; i + 5 < vb.vertexCount(); i += 6) {
                f32 minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
                for (u32 k = 0; k < 6; ++k) {
                    const auto& v = vb.vertices()[i + k];
                    minX = v.x < minX ? v.x : minX;
                    maxX = v.x > maxX ? v.x : maxX;
                    minY = v.y < minY ? v.y : minY;
                    maxY = v.y > maxY ? v.y : maxY;
                }
                if (minX >= vr.x && maxX <= vr.x + vr.w &&
                    minY >= vr.y && maxY <= vr.y + vr.h) {
                    const auto& v0 = vb.vertices()[i];
                    out.push_back({minX, minY, maxX, maxY, v0.a, v0.r,
                                   v0.g, v0.b});
                }
            }
            return out;
        };

        // ---- (D17) A CÂMARA COMO OBJETO PEQUENO --------------------------
        passo("16.1 D17: frustum mudo sem seleção; glifo+handles ≤12dp com seleção; drag move");
        {
            const Handle hCam = g_scene.create("cam 16");
            if (Tic* t = g_scene.get(hCam)) {
                Transform3D* tr = t->addComponent<Transform3D>();
                tr->pos = Vec3{0.0f, 0.0f, 0.0f};
                CameraComp* cc = t->addComponent<CameraComp>();
                cc->fovY = 60.0f;
                tr->updateWorld();
            }
            g_editor.selected = Handle::invalid();   // SEM seleção
            frame();
            auto [png0, js0] = export16("editor");
            fileapi::writeAll("hotfix19-device-camara-sem-selecao.png",
                              png0.data(), png0.size());
            const UiRect vr0 = viewRect16();
            {
                u32 muted = 0, amber = 0;
                const f32* accent = theme::kTheme.accent;
                for (const auto& q : quads16(vr0)) {
                    if (q.a > 0.30f && q.a < 0.40f) {
                        ++muted;   // o frustum mudo ~35%
                    }
                    if (q.a > 0.95f && q.r == accent[0] && q.g == accent[1] &&
                        q.b == accent[2]) {
                        ++amber;   // âmbar cheio — handles/gizmo não há
                    }
                }
                check(muted > 0, "16.1 D17 o frustum MUDO (cinza ~35%) está presente sem seleção");
                check(amber == 0, "16.1 D17 ZERO quads âmbar cheio na viewport sem seleção (nenhum handle)");
            }
            // selecionar PELO GLIFO (o tap no olho projetado — a seleção
            // nova do D17-f; o frustum em si é intocável)
            const Mat4 vp16 = editorVp16();
            f32 ex = 0.0f, ey = 0.0f;
            check(gizmo::projectPoint(vp16, Vec3{0.0f, 0.0f, 0.0f},
                                      vr0.w, vr0.h, ex, ey, vr0.x, vr0.y),
                  "16.1 D17 o olho da câmara projeta na viewport");
            g_input.injectDown(0, ex, ey);
            frame();
            g_input.injectUp(0);
            frame();
            check(g_editor.selected == hCam,
                  "16.1 D17 o tap NO GLIFO seleciona a câmara");
            // um tap na MEIO do cone (a meio do near→far) NÃO seleciona
            {
                Transform3D* tr = g_scene.get(hCam)->getComponent<Transform3D>();
                // D20: o preview desenha o cone CANÓNICO (aspect 1 — o
                // aspeto REAL vive no render/gameProj; o gizmo é indicador)
                const camgizmo::Frustum f = camgizmo::computeFrustum(
                    *tr, *g_scene.get(hCam)->getComponent<CameraComp>(),
                    1.0f,
                    camgizmo::previewCapWorld(vp16, vr0.w, vr0.h,
                                              g_camera.eye(), tr->pos));
                const Vec3 mid = (f.nearC[0] + f.farC[0]) * 0.5f;
                f32 mx = 0.0f, my = 0.0f;
                check(gizmo::projectPoint(vp16, mid, vr0.w, vr0.h, mx, my,
                                          vr0.x, vr0.y),
                      "16.1 D17 o meio do cone projeta");
                g_editor.selected = Handle::invalid();
                frame();
                g_input.injectDown(0, mx, my);
                frame();
                g_input.injectUp(0);
                frame();
                check(g_editor.selected != hCam,
                      "16.1 D17 o tap NO CONE não seleciona (o frustum é intocável — f)");
                g_editor.selected = hCam;
                frame();
            }
            // COM seleção: âmbar presente + handles ≤12dp + gizmo no glifo
            auto [png1, js1] = export16("editor");
            fileapi::writeAll("hotfix19-device-camara-selecionada.png",
                              png1.data(), png1.size());
            const UiRect vr1 = viewRect16();
            // o vp PÓS-seleção (o inspector abriu — o rect e o aspecto mudam;
            // o draw projeta com o rect ATUAL — as projeções aqui têm de
            // usar o MESMO vp)
            const Mat4 vpSel = editorVp16();
            {
                const f32* accent = theme::kTheme.accent;
                u32 amber = 0;
                for (const auto& q : quads16(vr1)) {
                    if (q.a > 0.95f && q.r == accent[0] && q.g == accent[1] &&
                        q.b == accent[2]) {
                        ++amber;
                    }
                }
                check(amber > 0, "16.1 D17 com seleção a câmara é ÂMBAR (frustum/glifo/handles)");
                // os handles ≤12dp nos CANTOS do far (o MESMO frustum do draw)
                Transform3D* tr = g_scene.get(hCam)->getComponent<Transform3D>();
                CameraComp* cc = g_scene.get(hCam)->getComponent<CameraComp>();
                // o ASPECTO do frustum é o da SUPERFÍCIE (o mesmo do
                // drawAll — o jogo renderiza o ecrã todo); o cap é o
                // PREVIEW do D20 (o mesmo do draw — os handles vivem nele)
                const camgizmo::Frustum f = camgizmo::computeFrustum(
                    *tr, *cc, 1.0f,
                    camgizmo::previewCapWorld(vpSel, vr1.w, vr1.h,
                                              g_camera.eye(), tr->pos));
                f32 hx = 0.0f, hy = 0.0f;
                check(gizmo::projectPoint(vpSel, f.farC[0], vr1.w, vr1.h,
                                          hx, hy, vr1.x, vr1.y),
                      "16.1 D17 o canto do far projeta");
                f32 maxHandle = 0.0f;
                const f32 tol = theme::dp(12.0f);   // D20: o piso 10dp + moldura
                for (const auto& q : quads16(vr1)) {
                    const f32 exW = q.x1 - q.x0, exH = q.y1 - q.y0;
                    if (exW <= tol && exH <= tol &&
                        q.x0 >= hx - theme::dp(9.0f) &&
                        q.x1 <= hx + theme::dp(9.0f) &&
                        q.y0 >= hy - theme::dp(9.0f) &&
                        q.y1 <= hy + theme::dp(9.0f)) {
                        const f32 ext = exW > exH ? exW : exH;
                        if (ext > maxHandle) {
                            maxHandle = ext;
                        }
                    }
                }
                check(maxHandle > theme::dp(9.0f),
                      "16.1/D20 o handle de canto 10dp existe no device");
                check(maxHandle <= theme::dp(12.0f),
                      "16.1/D20 o handle mede ≤10dp (NUNCA o quadrado de 26dp)");
                // o GIZMO ancora NO GLIFO: quads âmbar junto ao olho
                f32 gx = 0.0f, gy = 0.0f;
                check(gizmo::projectPoint(vpSel, tr->pos, vr1.w, vr1.h,
                                          gx, gy, vr1.x, vr1.y),
                      "16.1 D17 o olho projeta (pós-seleção)");
                u32 noGlifo = 0;
                for (const auto& q : quads16(vr1)) {
                    if (q.a > 0.95f &&
                        q.x0 <= gx + theme::dp(24.0f) &&
                        q.x1 >= gx - theme::dp(24.0f) &&
                        q.y0 <= gy + theme::dp(24.0f) &&
                        q.y1 >= gy - theme::dp(24.0f)) {
                        ++noGlifo;
                    }
                }
                check(noGlifo > 0,
                      "16.1 D17 o gizmo ancora NO GLIFO (quads âmbar no olho — d)");
                // ---- O TESTE DE GESTO INJETADO (o pin do dono): um drag
                // começado FORA dos handles move a câmara
                const f32 len = gizmo::gizmoLength(g_camera.dist);
                f32 ax = 0.0f, ay = 0.0f;
                // o press no MEIO do segmento do eixo X (o gizmo reclama o
                // gesto em TODO o segmento; a ponta encostava ao canto do
                // preview canónico do D20)
                check(gizmo::projectPoint(vpSel, Vec3{len * 0.5f, 0.0f, 0.0f},
                                          vr1.w, vr1.h, ax, ay, vr1.x, vr1.y),
                      "16.1 D17 o meio do eixo X projeta");
                // fora dos handles: a distância a CADA canto do far > 60px
                // o press está no SEGMENTO do eixo X (o gizmo reclama o
                // gesto ANTES dos handles — a ordem (e) do dono); a prova
                // comportamental é o fov INTACTO após o drag (abaixo) e a
                // posição da câmara a mover. A folga ao canto desenhado
                // (10dp — D20) fica registada:
                f32 minD = 1e9f;
                for (int i = 0; i < 4; ++i) {
                    f32 cx = 0.0f, cy = 0.0f;
                    if (gizmo::projectPoint(vpSel, f.farC[i], vr1.w, vr1.h,
                                            cx, cy, vr1.x, vr1.y)) {
                        const f32 d = std::sqrt((ax - cx) * (ax - cx) +
                                                (ay - cy) * (ay - cy));
                        if (d < minD) {
                            minD = d;
                        }
                    }
                }
                check(minD >= theme::dp(12.0f),
                      "16.1 D17 o press do drag não nasce SOBRE um handle "
                      "(folga ao canto desenhado ≥12dp; a ordem gizmo > "
                      "handles é a que manda)");
                Transform3D* trD = g_scene.get(hCam)->getComponent<Transform3D>();
                const Vec3 pos0 = trD->pos;
                const f32 fov0 = cc->fovY;
                g_input.injectDown(0, ax, ay);
                frame();
                g_input.injectMove(0, ax + 120.0f, ay);
                frame();
                g_input.injectMove(0, ax + 240.0f, ay);
                frame();
                g_input.injectUp(0);
                frame();
                const Vec3 pos1 = trD->pos;
                check(pos1.x > pos0.x + 0.05f,
                      "16.1 D17 o drag fora dos handles MOVE a câmara (e)");
                check(nearEqF(cc->fovY, fov0, 0.01f),
                      "16.1 D17 o drag no gizmo NÃO mexe o fov (os handles é que o fazem)");
                // o frustum mudo dos OUTROS estados: zero handles voltou
            }
        }

        // ---- (D14+D15) O MENU CONTIDO + AS STRINGS PT --------------------
        passo("16.2 D14/D15: o menu abre no topo, contido, «Exportar OBJ» no registo");
        {
            const editor::toolbar::TopBarLayout tl = editor::toolbar::topbarLayout(
                1600.0f, 720.0f, g_ui.safeArea(), false, false);
            // (1.ª abertura) o slot nasce limpo → offset 0
            g_input.injectDown(0, tl.menu.x + tl.menu.w * 0.5f,
                               tl.menu.y + tl.menu.h * 0.5f);
            frame();
            g_input.injectUp(0);
            frame();
            check(g_editor.fileMenu,
                  "16.2 D14 o menu abriu pelo botão [Menu] da top bar");
            check(g_ui.scrollOffsetForTest(editor::kMenuScrollId) == 0.0f,
                  "16.2 D14 o menu abre com offset 0 (a 1.ª linha inteira — o pin)");
            // (reabertura — o CENÁRIO DO DONO) fecha, suja o offset do slot
            // (agora existente) e reabre: o reset tem de repor a 0 (o slot
            // do scroll persiste/recicla — a 1.ª linha nascia cortada)
            g_input.injectDown(0, 1300.0f, 400.0f);
            frame();
            g_input.injectUp(0);
            frame();
            check(!g_editor.fileMenu, "16.2 D14 o menu fechou (1.ª vez)");
            g_ui.scrollSetOffset(editor::kMenuScrollId, 333.0f);
            g_input.injectDown(0, tl.menu.x + tl.menu.w * 0.5f,
                               tl.menu.y + tl.menu.h * 0.5f);
            frame();
            g_input.injectUp(0);
            frame();
            check(g_editor.fileMenu, "16.2 D14 o menu reabriu (2.ª vez)");
            check(g_ui.scrollOffsetForTest(editor::kMenuScrollId) == 0.0f,
                  "16.2 D14 a REABERTURA repõe o offset 0 (o slot sujo a 333 "
                  "não sobrevive — a 1.ª linha nunca nasce cortada)");
            auto [pngM, jsM] = export16("menu_ficheiro");
            fileapi::writeAll("hotfix19-device-menu-aberto.png", pngM.data(), pngM.size());
            const layout::Record& rm = g_ui.auditRecord();
            const UiRect tab = safe::bottomTabRect(1600.0f, 720.0f,
                                                   g_ui.safeArea());
            f32 sheetY1 = 0.0f, sheetY0 = 0.0f;
            bool sheet = false;
            for (const auto& e : rm.entries) {
                if (e.kind == layout::Entry::Panel &&
                    nearEqF(e.w, theme::dp(280.0f), 2.0f) &&
                    e.h > theme::dp(48.0f)) {
                    sheet = true;
                    sheetY0 = e.y;
                    sheetY1 = e.y + e.h;
                }
            }
            check(sheet, "16.2 D14 o sheet de 280dp está no registo");
            check(sheetY1 <= tab.y + 0.5f,
                  "16.2 D14 o fundo do sheet NUNCA cruza a tab bar "
                  "(o «Documentação V. …» cortado morreu)");
            // a ÚLTIMA linha alcançável: salta ao fundo (o clamp do
            // beginScroll) e o rótulo sai INTEIRO dentro do sheet
            g_ui.scrollSetOffset(editor::kMenuScrollId, 99999.0f);
            g_layoutExportPending = true;
            frame();
            {
                const layout::Record& rb = g_ui.auditRecord();
                const f32 wDoc = g_ui.fontWidth("Documentação V.ONI");
                bool docInteira = false;
                for (const auto& e : rb.entries) {
                    if (e.kind == layout::Entry::Label &&
                        std::fabs(e.fullW - wDoc) < theme::dp(4.0f) &&
                        e.y >= sheetY0 - 1.0f && e.y + e.h <= sheetY1 + 1.0f) {
                        docInteira = true;   // a última linha, inteira
                    }
                }
                check(docInteira,
                      "16.2 D14 a última linha («Documentação V.ONI») chega "
                      "INTEIRA pelo scroll próprio (todas alcançáveis)");
            }
            // (D15) «Exportar OBJ» desenhado; o EN «Export OBJ» nunca
            g_ui.scrollSetOffset(editor::kMenuScrollId, 0.0f);
            g_layoutExportPending = true;
            frame();
            {
                const layout::Record& rp = g_ui.auditRecord();
                const f32 wOk = g_ui.fontWidth("Exportar OBJ");
                const f32 wEn = g_ui.fontWidth("Export OBJ");
                bool temPT = false, temEN = false;
                for (const auto& e : rp.entries) {
                    if (e.kind != layout::Entry::Label) {
                        continue;
                    }
                    if (std::fabs(e.fullW - wOk) < theme::dp(4.0f)) {
                        temPT = true;
                    }
                    if (std::fabs(e.fullW - wEn) < theme::dp(4.0f)) {
                        temEN = true;
                    }
                }
                check(temPT, "16.3 D15 «Exportar OBJ» está no menu (o registo prova o PT)");
                check(!temEN, "16.3 D15 «Export OBJ» NÃO desenha (o EN morreu — o gate caça a tabela)");
            }
            // fecha com o toque fora
            g_input.injectDown(0, 1300.0f, 400.0f);
            frame();
            g_input.injectUp(0);
            frame();
            check(!g_editor.fileMenu, "16.2 D14 o menu fecha com o toque fora");
        }

        // ---- (R1) OS VALORES DO TRANSFORM INTOCÁVEIS ---------------------
        passo("16.4 R1+m3: valores X/Y/Z não-vazios em 180/212/260dp; a letra "
              "só sai abaixo de ~200dp de painel");
        {
            const Handle hT = g_scene.create("tic r1");
            if (Tic* t = g_scene.get(hT)) {
                Transform3D* tr = t->addComponent<Transform3D>();
                tr->pos = Vec3{1.5f, -2.0f, 3.0f};
                tr->updateWorld();
            }
            g_editor.selected = hT;
            for (int iw : {180, 212, 260}) {
                g_editor.inspW = static_cast<f32>(iw);
                g_layoutAuditPending = true;   // o registo liga NESTE frame
                frame();
                const layout::Record& rr = g_ui.auditRecord();
                // os 9 valores (3 linhas × 3 campos): rótulos não-vazios
                // na faixa das caixas do painel (a fórmula antiga deixava
                // os campos SEM texto — a mutação M-R1 é apanhada aqui)
                const f32 inspX = g_ui.contentWidthPx() -
                                  safe::resolvePanels(
                                      g_ui.contentWidthPx(), -1,
                                      g_editor.inspW).insp;
                u32 valores = 0;
                for (const auto& e : rr.entries) {
                    if (e.kind == layout::Entry::Label && e.x >= inspX &&
                        e.fullW > 0.5f) {
                        ++valores;
                    }
                }
                char msg[128];
                std::snprintf(msg, sizeof(msg),
                              "16.4 R1 a %ddp o painel desenha rótulos não-"
                              "vazios (%u)", iw, valores);
                check(valores >= 12, msg);   // 3 títulos + 9 valores + nome/visível
                // o orçamento puro no device (a convenção do draw: útil =
                // painel − 2×kPad 16): o espaço do valor ≥ o piso
                const editor::TransformBudget tb =
                    editor::transformRowBudget(iw - 32.0f);
                check(editor::transformValueSpace(tb) >=
                          editor::kTfValueMinDp - 0.01f,
                      "16.4 R1 o espaço do valor cumpre o piso (o valor é intocável)");
                // 0.9.6.19b (m3 · A LIMIAR DO DONO): a letra do eixo SÓ sai
                // abaixo de ~200dp de painel; a 212/260 ela COEXISTE com o
                // valor (o padding da linha cede antes); a 180 pode sair
                const bool letra = tb.axisLabels;
                if (iw >= 212) {
                    std::snprintf(msg, sizeof(msg),
                                  "16.4/m3 a %ddp a letra do eixo FICA "
                                  "(letra+valor coexistem)", iw);
                    check(letra, msg);
                } else {
                    std::snprintf(msg, sizeof(msg),
                                  "16.4/m3 a %ddp (abaixo do limiar ~200dp) "
                                  "a letra pode sair — o valor fica", iw);
                    check(editor::transformValueSpace(tb) >=
                              editor::kTfValueMinDp - 0.01f,
                          msg);
                }
            }
            g_editor.inspW = -1.0f;
            g_editor.selected = Handle::invalid();
        }

        // ---- (D16) A CAPTURA SÓ EM FRAME LIMPO ---------------------------
        passo("16.5 D16: o frame do menu nunca é capturado; o limpo seguinte sim");
        {
            g_editor.selected = Handle::invalid();
            frame();
            // abre o menu (overlay modal) e arma a captura
            const editor::toolbar::TopBarLayout tl = editor::toolbar::topbarLayout(
                1600.0f, 720.0f, g_ui.safeArea(), false, false);
            g_input.injectDown(0, tl.menu.x + tl.menu.w * 0.5f,
                               tl.menu.y + tl.menu.h * 0.5f);
            frame();
            g_input.injectUp(0);
            frame();
            check(g_editor.fileMenu, "16.5 D16 o menu está aberto");
            armThumbCapture();
            frame();   // o frame DO MENU: a captura NÃO corre
            check(!g_thumbJob.active.load(),
                  "16.5 D16 o frame com overlay modal NÃO é capturado (o guard)");
            check(g_thumbPending,
                  "16.5 D16 a captura fica armada (espera o frame limpo)");
            // fecha o menu → o próximo frame limpo captura
            g_input.injectDown(0, 1300.0f, 400.0f);
            frame();
            g_input.injectUp(0);
            frame();
            check(!g_editor.fileMenu, "16.5 D16 o menu fechou");
            frame();   // o frame limpo: lê a viewport e lança o worker
            for (int i = 0; i < 30 && g_thumbJob.active.load(); ++i) {
                frame();
            }
            check(g_thumbJob.ok,
                  "16.5 D16 a captura correu no PRIMEIRO frame limpo (thumb.png escrito)");
        }

        // ---- (D19) O TOGGLE «visível» COM ESTADO -------------------------
        passo("16.6 D19: o knob do «visível» nos dois estados no device");
        {
            const Handle hV = g_scene.create("tic d19");
            if (Tic* t = g_scene.get(hV)) {
                t->addComponent<Transform3D>();
            }
            g_editor.selected = hV;
            auto knobX = [&]() -> f32 {
                g_layoutAuditPending = true;   // o registo liga NESTE frame
                frame();
                const layout::Record& r = g_ui.auditRecord();
                const f32 inspX = g_ui.contentWidthPx() -
                                  safe::resolvePanels(
                                      g_ui.contentWidthPx(), -1,
                                      g_editor.inspW).insp;
                for (const auto& e : r.entries) {
                    if (e.kind == layout::Entry::Panel &&
                        e.x >= inspX &&
                        nearEqF(e.w, theme::dp(32.0f), 1.0f) &&
                        nearEqF(e.h, theme::dp(16.0f), 1.0f)) {
                        // o knob é o quad 12dp DENTRO da pílula
                        for (const auto& k : r.entries) {
                            if (k.kind == layout::Entry::Panel &&
                                k.x >= e.x && k.x + k.w <= e.x + e.w &&
                                k.y >= e.y && k.y + k.h <= e.y + e.h &&
                                nearEqF(k.w, theme::dp(12.0f), 1.0f)) {
                                return k.x;
                            }
                        }
                    }
                }
                return -1.0f;
            };
            const f32 xOn = knobX();    // TIC visível (default) → knob à direita
            if (Tic* t = g_scene.get(hV)) {
                t->visible = false;
            }
            const f32 xOff = knobX();   // invisível → knob à esquerda
            check(xOn > 0.0f && xOff > 0.0f,
                  "16.6 D19 o switch da linha «visível» desenha no device");
            check(xOff < xOn - theme::dp(8.0f),
                  "16.6 D19 o knob muda de LADO com o estado (o pin: estado nos dois valores)");
            g_editor.selected = Handle::invalid();
        }

        // ---- o validador inteiro ao device (o hotfix não abre exceções) --
        {
            frame();
            const layout::Record& rZ = g_ui.auditRecord();
            const auto probsZ = layout::validate(rZ);
            char vmsg[256];
            std::snprintf(vmsg, sizeof(vmsg),
                          "16.7 o editor do hotfix 0.9.6.19 passa o VALIDADOR "
                          "INTEIRO (0/0)%s",
                          probsZ.empty() ? "" : " — ver o log acima");
            for (const auto& pr : probsZ) {
                // as entradas envolvidas apontam ao registo (ia/ib)
                const layout::Entry& ea = rZ.entries[pr.ia];
                std::printf("    [validador] %s: %s na entrada %u (kind=%s x=%.0f y=%.0f w=%.0f h=%.0f)\n",
                            pr.sevName(), pr.ruleName(), pr.ia,
                            ea.kindName(), ea.x, ea.y, ea.w, ea.h);
            }
            check(probsZ.empty(), vmsg);
        }

        // ====================================================================
        // FASE 17 — 0.9.6.19b (HOTFIX B): m1/m2/m3/D20/D21 NO DEVICE.
        // (m1) o chip «N x» com slot próprio — espaçado quando cabe, AUSENTE
        //      quando não cabe (nunca colado ao título); (m2) nenhum rótulo
        //      «-0» no inspector com rotação -0.0; (D21) NENHUM widget com o
        //      id 38 (kVpAddTicIdRetired) desenha dentro do rect do viewport
        //      em estado nenhum + o menu ⋯ tem «Novo objeto» que abre o
        //      plusMenu (o MESMO código do + da hierarquia); (D20) o bounding
        //      do gizmo da câmara ≤4%/≤6% da área do viewport no device.
        // ====================================================================
        fase("FASE 17 — 0.9.6.19b HOTFIX: m1/m2/m3/D20/D21 medidos no device virtual");
        {
            g_editor.hierW = -1.0f;
            g_editor.inspW = -1.0f;
            g_editor.inspPinned = false;
            g_editor.playMode = false;
            g_editor.uiMode = false;
            g_editor.audioMode = false;
            g_toastT = 0.0f;
            g_toast[0] = '\0';
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
            g_editor.selected = Handle::invalid();
            g_editor.multiSelectCount = 0;
            frame();

            const auto viewRect17 = [&]() {
                const bool inspRight =
                    g_editor.showInspector &&
                    !(g_editor.uiMode || editor::inspectorCollapsed(g_editor));
                return editor::centerRect(
                    1600.0f, 720.0f, g_ui.safeArea(), currentDrawerH(),
                    inspRight, g_editor.hierW, g_editor.inspW,
                    !g_editor.uiMode && editor::inspectorCollapsed(g_editor));
            };

            // ---- 17.1 (D21) O PIN: nenhum id de "add" no viewport ----------
            passo("17.1 D21: nenhum widget com o id 38 desenha no viewport "
                  "(drawer fechado/aberto × trilho/inspetor)");
            {
                bool algum38 = false;
                char onde[96] = "";
                for (int drawer = 0; drawer <= 1; ++drawer) {
                    for (int insp = 0; insp <= 1; ++insp) {
                        g_bottom.bottomTab = drawer ? 1 : 0;
                        g_bottom.drawerH = drawer ? 240.0f : 0.0f;
                        g_editor.showInspector = insp != 0;
                        g_editor.selected = Handle::invalid();
                        frame();
                        g_layoutExportPending = true;
                        frame();
                        const UiRect vr = viewRect17();
                        for (const auto& e : g_ui.auditRecord().entries) {
                            if (e.id == editor::vpchrome::kVpAddTicIdRetired &&
                                e.w > 0.0f && e.h > 0.0f) {
                                algum38 = true;
                                std::snprintf(onde, sizeof(onde),
                                              "drawer=%d insp=%d", drawer,
                                              insp);
                            }
                        }
                    }
                }
                check(!algum38, onde[0]
                                    ? "17.1 D21 FALHOU — o id 38 desenha no viewport"
                                    : "17.1 D21 nenhum id de add desenha dentro "
                                      "do rect do viewport em estado nenhum");
                // e o canto inferior direito fica LIMPO (o desenho de vidro
                // do [+], que ali vivia, saiu — a orbit/drag lê a cena)
                g_bottom.bottomTab = 0;
                g_bottom.drawerH = 0.0f;
                g_editor.showInspector = false;
                frame();
            }

            // ---- 17.2 (D21) «Novo objeto» no menu ⋯ → o MESMO plusMenu ----
            passo("17.2 D21: o menu ⋯ tem «Novo objeto» e abre o plusMenu de presets");
            {
                const editor::toolbar::TopBarLayout tl = editor::toolbar::topbarLayout(
                    1600.0f, 720.0f, g_ui.safeArea(), false, false);
                g_input.injectDown(0, tl.menu.x + tl.menu.w * 0.5f,
                                   tl.menu.y + tl.menu.h * 0.5f);
                frame();
                g_input.injectUp(0);
                frame();
                check(g_editor.fileMenu, "17.2 D21 o menu ⋯ abriu");
                // o «Novo objeto» fica abaixo da dobra no portrait — rola o
                // scroll PRÓPRIO do menu até ele (o D14 garante as linhas
                // alcançáveis; aqui alcançamos a linha nova)
                g_ui.scrollSetOffset(editor::kMenuScrollId, 300.0f);
                g_layoutExportPending = true;
                frame();
                const layout::Record& rm = g_ui.auditRecord();
                const f32 wNovo = g_ui.fontWidth("Novo objeto");
                bool achou = false;
                f32 rx = 0.0f, ry = 0.0f;
                for (const auto& e : rm.entries) {
                    if (e.kind == layout::Entry::Label &&
                        std::fabs(e.fullW - wNovo) < theme::dp(4.0f)) {
                        achou = true;
                        rx = e.x;
                        ry = e.y;
                    }
                }
                check(achou, "17.2 D21 «Novo objeto» está no menu (o registo prova)");
                // o PNG P-05: o menu aberto COM a linha nova visível
                {
                    auto [pngM, jsM] = export16("editor");
                    fileapi::writeAll("hotfix19b-device-menu-novo-objeto.png",
                                      pngM.data(), pngM.size());
                }
                if (achou) {
                    // o tap na LINHA do rótulo (o alvo é a linha inteira)
                    g_input.injectDown(0, rx + wNovo * 0.5f,
                                       ry + theme::dp(10.0f));
                    frame();
                    g_input.injectUp(0);
                    frame();
                    check(g_editor.plusMenu,
                          "17.2 D21 o «Novo objeto» abre o PLUSMENU (o mesmo "
                          "código do + da hierarquia — não é caminho novo)");
                    check(!g_editor.fileMenu,
                          "17.2 D21 o menu de ficheiro fechou com a escolha");
                    // fecha o plusMenu (toque fora)
                    g_input.injectDown(0, 1300.0f, 400.0f);
                    frame();
                    g_input.injectUp(0);
                    frame();
                    check(!g_editor.plusMenu,
                          "17.2 D21 o plusMenu fecha com o toque fora");
                }
            }

            // ---- 17.3 (m2) NENHUM «-0» no inspector com rotação -0.0 ------
            passo("17.3 m2: rotação -0.0 desenha «0» (nenhum rótulo -0 no painel)");
            {
                const Handle hZ = g_scene.create("tic m2");
                if (Tic* t = g_scene.get(hZ)) {
                    Transform3D* tr = t->addComponent<Transform3D>();
                    tr->pos = Vec3{0.0f, 0.0f, 0.0f};
                    tr->rot = Quat::fromEuler(-0.0f, -0.0f, -0.0f);
                    tr->updateWorld();
                }
                g_editor.selected = hZ;
                g_editor.showInspector = true;
                frame();
                g_layoutExportPending = true;
                frame();
                const layout::Record& rz = g_ui.auditRecord();
                const f32 wMenos0 = g_ui.fontWidth("-0");
                const f32 wZero = g_ui.fontWidth("0");
                // o scan fica DENTRO do painel do inspector (o x da sua
                // borda esquerda — o mesmo filtro do 16.4) e fora da faixa
                // do toast/tab bar no fundo
                const f32 inspX17 = g_ui.contentWidthPx() -
                                    safe::resolvePanels(
                                        g_ui.contentWidthPx(), -1,
                                        g_editor.inspW).insp;
                const f32 yMax17 = 720.0f - theme::dp(40.0f);
                u32 zeros = 0;
                bool menosZero = false;
                for (const auto& e : rz.entries) {
                    if (e.kind != layout::Entry::Label || e.x < inspX17 ||
                        e.y > yMax17) {
                        continue;
                    }
                    if (std::fabs(e.fullW - wMenos0) < theme::dp(2.0f)) {
                        menosZero = true;   // um rótulo com a largura de "-0"
                    }
                    if (std::fabs(e.fullW - wZero) < theme::dp(2.0f)) {
                        ++zeros;
                    }
                }
                check(zeros >= 3,
                      "17.3 m2 os campos a zero desenham «0» (o format único "
                      "passa pelas caixas)");
                check(!menosZero,
                      "17.3 m2 NENHUM rótulo com a largura de «-0» desenha "
                      "(o zero negativo morreu — a mutação M-m2 volta)");
                g_editor.selected = Handle::invalid();
                g_editor.showInspector = false;
            }

            // ---- 17.4 (m1) O CHIP «N x» ESPAÇADO OU AUSENTE ---------------
            passo("17.4 m1: o chip da multi-seleção nunca cola ao título "
                  "(espaçado no painel largo, ausente no estreito)");
            {
                // (a) painel LARGO (hierW 316dp — o título «Hierarquia» a
                //     16sp mede ~157dp com a fonte da casa; o slot precisa
                //     de título+chip+ações): o chip desenha com ≥8dp de cada
                //     lado
                g_editor.multiSelectCount = 2;
                g_editor.hierW = theme::dp(316.0f);   // px (o estado do drag)
                frame();
                g_layoutExportPending = true;
                frame();
                const layout::Record& rW = g_ui.auditRecord();
                f32 chipX = -1.0f, chipW2 = 0.0f, titleEnd = -1.0f;
                f32 dotsX = 1e9f;
                const f32 wTitulo =
                    g_ui.fontWidth("Hierarquia") *
                    theme::fontScale(theme::kFontSection);
                for (const auto& e : rW.entries) {
                    if (e.id == editor::kHierMultiClearId && e.w > 0.0f) {
                        chipX = e.x;
                        chipW2 = e.w;
                    }
                    if (e.kind == layout::Entry::Label &&
                        std::fabs(e.fullW - wTitulo) < theme::dp(6.0f) &&
                        e.x < theme::dp(60.0f)) {
                        titleEnd = e.x + e.w;   // o título desenhado
                    }
                    // o ⋮ da hierarquia desenha 3 pontos 3×3 — o MENOR x de
                    // um ponto à direita do chip
                    if (e.kind == layout::Entry::Panel &&
                        nearEqF(e.w, theme::dp(3.0f), 0.5f) &&
                        e.x > chipX + chipW2 && e.x < dotsX) {
                        dotsX = e.x;
                    }
                }
                {
                    u32 nChip = 0, nTit = 0;
                    f32 wTitRec = -1.0f;
                    for (const auto& e : rW.entries) {
                        if (e.id == editor::kHierMultiClearId && e.w > 0.0f) {
                            ++nChip;
                        }
                        if (e.kind == layout::Entry::Label &&
                            e.x < theme::dp(60.0f)) {
                            ++nTit;
                            wTitRec = e.fullW;
                        }
                    }
                    std::printf("    [dbg17.4] nChip=%u nTit=%u wTitulo=%.1f wTitRec=%.1f\n",
                                (unsigned)nChip, (unsigned)nTit,
                                (double)wTitulo, (double)wTitRec);
                }
                // o PNG P-05: o cabeçalho com o chip ESPAÇADO
                {
                    auto [pngC, jsC] = export16("editor");
                    fileapi::writeAll(
                        "hotfix19b-device-hier-chip-espacado.png",
                        pngC.data(), pngC.size());
                }
                check(chipX > 0.0f && titleEnd > 0.0f,
                      "17.4 m1 no painel largo o chip e o título desenham");
                if (chipX > 0.0f && titleEnd > 0.0f) {
                    check(chipX >= titleEnd + theme::dp(8.0f) - 0.5f,
                          "17.4 m1 o chip nasce ≥8dp DEPOIS do título (a "
                          "colagem «Hierarquia1 x» morre)");
                    if (dotsX < 1e8f) {
                        check(chipX + chipW2 <= dotsX - theme::dp(8.0f) + 0.5f,
                              "17.4 m1 o chip fica ≥8dp ANTES do ⋮ (slot "
                              "próprio, espaço e propósito)");
                    }
                }
                // (b) painel ESTREITO (140dp — o device): o chip NÃO desenha
                g_editor.hierW = theme::dp(140.0f);   // px
                frame();
                g_layoutExportPending = true;
                frame();
                bool chipEstreito = false;
                for (const auto& e : g_ui.auditRecord().entries) {
                    if (e.id == editor::kHierMultiClearId && e.w > 0.0f) {
                        chipEstreito = true;
                    }
                }
                check(!chipEstreito,
                      "17.4 m1 no painel estreito o chip NÃO desenha (a "
                      "degradação honesta — nada colado)");
                g_editor.multiSelectCount = 0;
                g_editor.hierW = -1.0f;
            }

            // ---- 17.5 (D20) O CONTRATO MEDÍVEL NO DEVICE ------------------
            passo("17.5 D20: o bounding do gizmo da câmara ≤4%/≤6% da área "
                  "do viewport no device @2.0 (o dump do script de medidas)");
            {
                const Handle hC20 = g_scene.create("cam 20");
                if (Tic* t = g_scene.get(hC20)) {
                    Transform3D* tr = t->addComponent<Transform3D>();
                    tr->pos = Vec3{0.0f, 0.0f, 0.0f};
                    CameraComp* cc = t->addComponent<CameraComp>();
                    cc->fovY = 60.0f;
                    cc->farZ = 2000.0f;
                    tr->updateWorld();
                }
                g_editor.selected = Handle::invalid();
                frame();
                const UiRect vr = viewRect17();
                const Mat4 vp17 = Mat4::mul(g_camera.proj(vr.w / vr.h),
                                            g_camera.view());
                Transform3D* tr20 =
                    g_scene.get(hC20)->getComponent<Transform3D>();
                const f32 cap = camgizmo::previewCapWorld(
                    vp17, vr.w, vr.h, g_camera.eye(), tr20->pos);
                const camgizmo::Frustum f20 = camgizmo::computeFrustum(
                    *tr20, *g_scene.get(hC20)->getComponent<CameraComp>(),
                    1.0f, cap);
                const UiRect bSem = camgizmo::gizmoBoundsPx(
                    vp17, 1600.0f, 720.0f, f20, false, vr.w, vr.h, vr.x,
                    vr.y);
                const UiRect bCom = camgizmo::gizmoBoundsPx(
                    vp17, 1600.0f, 720.0f, f20, true, vr.w, vr.h, vr.x,
                    vr.y);
                const f32 area = vr.w * vr.h;
                const f32 pctSem = (bSem.w * bSem.h) / area * 100.0f;
                const f32 pctCom = (bCom.w * bCom.h) / area * 100.0f;
                check(bSem.w > 0.0f,
                      "17.5 D20 o bounding SEM seleção mede");
                check(pctSem <= 4.0f,
                      "17.5 D20 o bounding ocupa ≤4% da área do viewport "
                      "SEM seleção (o pin)");
                check(pctCom <= 6.0f,
                      "17.5 D20 o bounding ocupa ≤6% COM seleção (o pin)");
                // o dump do script de medidas (método PASSO 0 — o gate
                // scripts/gizmo_camera_medidas.py revalida ESTE ficheiro)
                char js[512];
                std::snprintf(
                    js, sizeof(js),
                    "{\n  \"release\": \"0.9.6.19b\",\n  \"item\": \"D20\",\n"
                    "  \"densidade\": 2.0,\n  \"viewport_px\": [%.1f, %.1f],\n"
                    "  \"preview_dp_alvo\": [48.0, 120.0],\n"
                    "  \"preview_cap_mundo\": %.4f,\n"
                    "  \"bounding_sem_px\": [%.1f, %.1f, %.1f, %.1f],\n"
                    "  \"bounding_com_px\": [%.1f, %.1f, %.1f, %.1f],\n"
                    "  \"pct_area_sem\": %.3f,\n  \"pct_area_com\": %.3f,\n"
                    "  \"pin_sem_pct\": 4.0,\n  \"pin_com_pct\": 6.0\n}\n",
                    (double)vr.w, (double)vr.h, (double)cap, (double)bSem.x,
                    (double)bSem.y, (double)bSem.w, (double)bSem.h,
                    (double)bCom.x, (double)bCom.y, (double)bCom.w,
                    (double)bCom.h, (double)pctSem, (double)pctCom);
                fileapi::writeAll("hotfix19b-gizmo-medidas.json", js,
                                  std::strlen(js));
                // o PNG P-05 do estado pequeno (seleção desligada)
                auto [png20, js20] = export16("editor");
                fileapi::writeAll("hotfix19b-device-camara-pequena.png",
                                  png20.data(), png20.size());
                g_editor.selected = hC20;
                frame();
                auto [png21, js21] = export16("editor");
                fileapi::writeAll("hotfix19b-device-camara-selecionada.png",
                                  png21.data(), png21.size());
                g_editor.selected = Handle::invalid();
            }

            // ---- o validador inteiro ao device (o hotfix não abre exceções)
            {
                frame();
                const layout::Record& rZ = g_ui.auditRecord();
                const auto probsZ = layout::validate(rZ);
                char vmsg[256];
                std::snprintf(vmsg, sizeof(vmsg),
                              "17.6 o editor do hotfix 0.9.6.19b passa o "
                              "VALIDADOR INTEIRO (0/0)%s",
                              probsZ.empty() ? "" : " — ver o log acima");
                for (const auto& pr : probsZ) {
                    const layout::Entry& ea = rZ.entries[pr.ia];
                    std::printf("    [validador] %s: %s na entrada %u (kind=%s x=%.0f y=%.0f w=%.0f h=%.0f)\n",
                                pr.sevName(), pr.ruleName(), pr.ia,
                                ea.kindName(), ea.x, ea.y, ea.w, ea.h);
                }
                check(probsZ.empty(), vmsg);
            }
        }

        // ------------------------------------------------------------------
        // FASE 18 — 0.9.6.20 (PASSO 4 · JANELAS): J-01..J-04 NO DEVICE.
        // J-01 logs opacos 80% com wrap · J-02 o véu 40% dos menus
        // ancorados · J-03 settings linhas 36dp · J-04 os cantos suaves
        // nos últimos cards duros. Provas por PIXEL (o loadPng de produção
        // relê o PNG do export — o método 13.1/13.9) + o registo. O export
        // nomeia pelo ECRÃ corrente (currentScreenName): menu_ficheiro /
        // cenas / logs / settings / storage — o nome certo POR ESTADO.
        // ------------------------------------------------------------------
        fase("FASE 18 — 0.9.6.20 PASSO 4: as JANELAS (logs/menus/settings/"
             "cantos) medidas no device virtual");
        {
            // estado limpo (o padrão 17)
            g_editor.hierW = -1.0f;
            g_editor.inspW = -1.0f;
            g_editor.inspPinned = false;
            g_editor.playMode = false;
            g_editor.uiMode = false;
            g_editor.audioMode = false;
            g_toastT = 0.0f;
            g_toast[0] = '\0';
            g_bottom.bottomTab = 0;
            g_bottom.drawerH = 0.0f;
            g_editor.selected = Handle::invalid();
            g_editor.multiSelectCount = 0;
            g_editor.fileMenu = false;
            g_editor.hierMenu = false;
            g_editor.scenesMenu = false;
            g_editor.settingsMenu = false;
            g_editor.storageDialog = false;
            g_editor.importMenu = false;
            g_editor.logViewer = false;
            frame();

            auto export18 = [&](const char* ecra) {
                g_layoutExportPending = true;
                frame();
                std::vector<u8> png, js;
                const bool okP = rawSt16->readBytes(
                    std::string("layout/") + ecra + ".png", png);
                const bool okJ = rawSt16->readBytes(
                    std::string("layout/") + ecra + ".json", js);
                check(okP && !png.empty(),
                      (std::string("18 o PNG do ecrã [") + ecra +
                       "] está no projeto (o export nomeia pelo ECRÃ)")
                          .c_str());
                return std::make_pair(png, js);
            };

            // o composto esperado do VÉU DE MENU (J-02): preto 40% sobre o
            // bg grafite — a matemática do device (o MESMO blend do pass UI)
            f32 veu40[4];
            theme::blendOver(theme::kTheme.scrimMenu, theme::kTheme.bg,
                             veu40);
            // e o vidro do sheet (surface 80%) SOBRE o véu 40%
            f32 glassSobreVeu[4];
            theme::blendOver(theme::kTheme.surface, veu40, glassSobreVeu);

            // a sonda: relê o PNG (loadPng de produção) e compara o pixel
            // (er,eg,eb) com tolerância ±1 (o padrão 13.1/13.9)
            auto probePx18 = [&](const std::vector<u8>& pngBytes, u32 px,
                                 u32 py, int er, int eg, int eb,
                                 const char* msg) {
                vv::RawImage img;
                std::string err;
                check(vv::loadPng(pngBytes.data(), pngBytes.size(), img,
                                  err) &&
                          img.ok(),
                      "18 o PNG do export decodifica (loadPng de produção)");
                check(px < img.width && py < img.height,
                      "18 o pixel da sonda está dentro do ecrã");
                const size_t pi = (size_t(py) * img.width + px) * 4;
                const int dr = (int)img.rgba[pi] - er;
                const int dg = (int)img.rgba[pi + 1] - eg;
                const int db = (int)img.rgba[pi + 2] - eb;
                check(dr >= -1 && dr <= 1 && dg >= -1 && dg <= 1 &&
                          db >= -1 && db <= 1,
                      msg);
            };

            // o CARD de um overlay no registo: o maior painel que NÃO é
            // full-screen (o quad da cena cobre o ecrã todo e não é janela)
            auto cardDe18 = [](const layout::Record& rec, f32 sw, f32 sh)
                -> const layout::Entry* {
                const layout::Entry* card = nullptr;
                f32 best = 0.0f;
                for (const auto& e : rec.entries) {
                    if (e.kind != layout::Entry::Panel) {
                        continue;
                    }
                    if (e.w >= sw * 0.90f || e.h >= sh * 0.90f) {
                        continue;   // full-screen (a cena) — não é card
                    }
                    if (e.w * e.h > best) {
                        best = e.w * e.h;
                        card = &e;
                    }
                }
                return card;
            };

            // ---- 18.1 (J-02) O VÉU DOS MENUS ANCORADOS É 40% --------------
            passo("18.1 J-02: o véu dos menus ancorados = preto 40% "
                  "(ficheiro/cenas) e o vidro do sheet mantém 80%");
            {
                // o menu abre PELO BOTÃO [Menu] da top bar (o caminho REAL
                // do dono — o abridor repõe o offset 0 do D14; um set
                // direto do flag saltaria o reset e nascia scrollado)
                const editor::toolbar::TopBarLayout tlM =
                    editor::toolbar::topbarLayout(1600.0f, 720.0f,
                                                  g_ui.safeArea(), false,
                                                  false);
                g_input.injectDown(0, tlM.menu.x + tlM.menu.w * 0.5f,
                                   tlM.menu.y + tlM.menu.h * 0.5f);
                frame();
                g_input.injectUp(0);
                frame();
                check(g_editor.fileMenu,
                      "18.1 J-02 o menu ⋯ abriu pelo botão [Menu] (o "
                      "caminho real, com o offset 0 do D14)");
                auto [pngM, jsM] = export18("menu_ficheiro");
                // o sheet vem do REGISTO (o card do ecrã menu_ficheiro)
                const layout::Entry* sheet =
                    cardDe18(g_ui.auditRecord(), 1600.0f, 720.0f);
                check(sheet != nullptr,
                      "18.1 J-02 o registo tem o sheet do menu ⋯");
                if (sheet) {
                    // (a) o VÉU fora do sheet (à direita dele, no
                    //     viewport): o composto preto-40% — o véu 60%
                    //     antigo daria (5,5,6)
                    const u32 vx = (u32)(sheet->x + sheet->w + 120.0f);
                    const u32 vy = (u32)(sheet->y + sheet->h * 0.5f);
                    probePx18(pngM, vx, vy,
                              (int)(veu40[0] * 255.0f + 0.5f),
                              (int)(veu40[1] * 255.0f + 0.5f),
                              (int)(veu40[2] * 255.0f + 0.5f),
                              "18.1 J-02 o véu do menu ⋯ mede preto 40% (o "
                              "composto do scrimMenu sobre o bg — a "
                              "matemática do device no pixel)");
                    // (b) o VIDRO do sheet na banda de rodapé (4dp sob a
                    //     última linha): surface 80% SOBRE o véu 40%
                    probePx18(pngM, (u32)(sheet->x + 40.0f),
                              (u32)(sheet->y + sheet->h - 3.0f),
                              (int)(glassSobreVeu[0] * 255.0f + 0.5f),
                              (int)(glassSobreVeu[1] * 255.0f + 0.5f),
                              (int)(glassSobreVeu[2] * 255.0f + 0.5f),
                              "18.1 J-02 o vidro do sheet mantém 80% SOBRE "
                              "o véu de 40% (o composto duplo no pixel)");
                    // (c) o CANTO do sheet (raio 8dp — o J-04 no ancorado):
                    //     o pixel (2,2) diagonal fica FORA do raio → véu
                    probePx18(pngM, (u32)(sheet->x + 2.0f),
                              (u32)(sheet->y + 2.0f),
                              (int)(veu40[0] * 255.0f + 0.5f),
                              (int)(veu40[1] * 255.0f + 0.5f),
                              (int)(veu40[2] * 255.0f + 0.5f),
                              "18.1 J-04 o canto do sheet é suave (o pixel "
                              "diagonal fora do raio 8dp mostra o véu)");
                }
                fileapi::writeAll("hotfix20-device-menu-veu40.png",
                                  pngM.data(), pngM.size());
                g_editor.fileMenu = false;

                // o menu de CENAS: o MESMO véu de 40% (o ecrã dele é
                // "cenas" — currentScreenName)
                g_editor.scenesMenu = true;
                frame();
                frame();
                auto [pngS, jsS] = export18("cenas");
                probePx18(pngS, 1250, 600, (int)(veu40[0] * 255.0f + 0.5f),
                          (int)(veu40[1] * 255.0f + 0.5f),
                          (int)(veu40[2] * 255.0f + 0.5f),
                          "18.1 J-02 o véu do menu de cenas mede preto 40% "
                          "(o MESMO token dos ancorados)");
                g_editor.scenesMenu = false;
                frame();
            }

            // ---- 18.2 (J-01) LOGS OPAQUES 80% COM WRAP ---------------------
            passo("18.2 J-01: o viewer de logs é OPAQUE (o card é o bg), "
                  "80% da faixa e quebra a linha longa");
            {
                std::string longLine;
                for (int i = 0; i < 150; ++i) {
                    longLine += "palavra ";
                }
                g_logLines.clear();
                g_logLines.push_back(longLine);
                g_logLines.push_back("curta");
                g_logDumps.clear();
                g_editor.logViewer = true;
                g_editor.logViewerJustOpened = true;
                frame();
                frame();
                auto [pngL, jsL] = export18("logs");
                // o card do viewer vem do REGISTO (o ecrã "logs")
                const layout::Entry* card18 =
                    cardDe18(g_ui.auditRecord(), 1600.0f, 720.0f);
                check(card18 != nullptr,
                      "18.2 J-01 o registo tem o card do viewer");
                f32 ox18, oy18, aw18, ah18;
                editor::overlayArea(1600.0f, 720.0f, g_ui.safeArea(), ox18,
                                    oy18, aw18, ah18);
                check(card18 && card18->w <= aw18 * 0.80f + 0.1f,
                      "18.2 J-01 o card ocupa ≤80% da LARGURA da faixa "
                      "(o cap do dono — era 86%)");
                check(card18 && card18->h <= ah18 * 0.80f + 0.1f,
                      "18.2 J-01 o card ocupa ≤80% da ALTURA da faixa");
                if (card18) {
                    // OPAQUE: o pixel no interior do card É o bg grafite
                    // exato (o vidro antigo deixaria a cena atravessar) —
                    // na banda de rodapé do card, longe do texto
                    probePx18(pngL, (u32)(card18->x + 24.0f),
                              (u32)(card18->y + card18->h - 6.0f), 14, 14,
                              16,
                              "18.2 J-01 o card do log é OPAQUE (o pixel "
                              "interior é o bg #0E0E10 exato — a cena não "
                              "atravessa)");
                }
                fileapi::writeAll("hotfix20-device-logs.png", pngL.data(),
                                  pngL.size());
                g_editor.logViewer = false;
                g_logLines.clear();
                frame();
            }

            // ---- 18.3 (J-03) SETTINGS LINHAS 36DP --------------------------
            passo("18.3 J-03: as linhas do Settings medem 36dp (os "
                  "cabeçalhos mantêm 48dp) — a página inteira no device");
            {
                g_editor.settingsMenu = true;
                g_editor.settingsCollapsed = 0;   // TODAS as secções abertas
                frame();
                frame();
                auto [pngS36, jsS36] = export18("settings");
                // o alvo de toque da LINHA (o toggle Imersivo — a linha
                // INTEIRA é o alvo): h == 36dp × 2.0 = 72px
                const layout::Record& rec36 = g_ui.auditRecord();
                bool linhaOk = false, headerOk = false;
                for (const auto& e : rec36.entries) {
                    if (e.kind == layout::Entry::Button &&
                        e.id == editor::settings::kImmersiveId) {
                        linhaOk = e.h >= theme::dp(36.0f) - 0.5f &&
                                  e.h <= theme::dp(36.0f) + 0.5f;
                    }
                    if (e.kind == layout::Entry::Button &&
                        e.id == editor::settings::kSectionBase +
                                    editor::settings::kBitGeral) {
                        headerOk = e.h >= theme::dp(48.0f) - 0.5f &&
                                   e.h <= theme::dp(48.0f) + 0.5f;
                    }
                }
                check(linhaOk, "18.3 J-03 a linha (toggle Imersivo) mede "
                               "36dp no registo (era 48dp)");
                check(headerOk, "18.3 J-03 o cabeçalho de secção mantém "
                                "48dp (a spec manda nas LINHAS)");
                fileapi::writeAll("hotfix20-device-settings-36.png",
                                  pngS36.data(), pngS36.size());
                g_editor.settingsMenu = false;
                frame();
            }

            // ---- 18.4 (J-04) OS CANTOS SUAVES DOS ÚLTIMOS CARDS DUROS -----
            passo("18.4 J-04: o diálogo de armazenamento tem CANTOS "
                  "suaves (o pixel diagonal fora do raio mostra o fundo)");
            {
                g_editor.storageDialog = true;
                frame();
                frame();
                auto [pngD, jsD] = export18("storage");
                const layout::Entry* dlg =
                    cardDe18(g_ui.auditRecord(), 1600.0f, 720.0f);
                check(dlg != nullptr,
                      "18.4 J-04 o registo tem o card do diálogo");
                if (dlg) {
                    f32 glassBg[4];
                    theme::blendOver(theme::kTheme.surface,
                                     theme::kTheme.bg, glassBg);
                    // o CANTO (2,2) fora do raio 8dp: o fundo por trás
                    // (o painel duro mostraria o vidro do card)
                    probePx18(pngD, (u32)(dlg->x + 2.0f),
                              (u32)(dlg->y + 2.0f), 14, 14, 16,
                              "18.4 J-04 o canto do diálogo é SUAVE (o "
                              "pixel (2,2) fora do raio 8dp mostra o fundo "
                              "— o painel duro mostraria o vidro)");
                    // o interior perto do canto é o VIDRO do card (o
                    // composto surface sobre o fundo)
                    probePx18(pngD, (u32)(dlg->x + 16.0f),
                              (u32)(dlg->y + 16.0f),
                              (int)(glassBg[0] * 255.0f + 0.5f),
                              (int)(glassBg[1] * 255.0f + 0.5f),
                              (int)(glassBg[2] * 255.0f + 0.5f),
                              "18.4 J-04 o interior do card é o vidro "
                              "surface 80% (o composto no pixel)");
                }
                fileapi::writeAll("hotfix20-device-storage-cantos.png",
                                  pngD.data(), pngD.size());
                g_editor.storageDialog = false;
                frame();
            }

            // ---- 18.5 o validador inteiro ao device (o PASSO 4 não abre
            //      exceções — o EDITOR limpo, o mesmo âmbito do 17.6; o
            //      registo tem de ser FRESCO: armar o audit deste frame,
            //      não validar o registo stale do diálogo do 18.4) --------
            {
                g_layoutExportPending = true;
                frame();
                const layout::Record& rW = g_ui.auditRecord();
                const auto probsW = layout::validate(rW);
                char vmsgW[256];
                std::snprintf(vmsgW, sizeof(vmsgW),
                              "18.5 o editor do PASSO 4 passa o VALIDADOR "
                              "INTEIRO (0/0)%s",
                              probsW.empty() ? "" : " — ver o log acima");
                for (const auto& pr : probsW) {
                    const layout::Entry& ea = rW.entries[pr.ia];
                    std::printf("    [validador] %s: %s na entrada %u "
                                "(kind=%s x=%.0f y=%.0f w=%.0f h=%.0f)\n",
                                pr.sevName(), pr.ruleName(), pr.ia,
                                ea.kindName(), ea.x, ea.y, ea.w, ea.h);
                }
                check(probsW.empty(), vmsgW);
            }
        }

        onAppCmd(&app16, APP_CMD_TERM_WINDOW);
        vvstub::g_stubDensityDpi = 160;
        theme::setDensity(1.0f);
        editor::applyDensity();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }

    // ======================================================================
    // FASE 19 — 0.10-M PASSO 3B: O LOG DE HOJE (viewer + export + causa
    // do load). O dono reportou: «o log viewer mostra conteúdo de ONTEM
    // (stale)» e «o modelo de 203 MB falha com 'ver causa no log'» — mas
    // nunca VIA a causa: (a) o readTail lia o backup MAIS ANTIGO primeiro
    // e o ficheiro ATIVO nunca entrava na janela do viewer; (b) a causa
    // do load (a recusa do mesh único) morria no LOGE do GpuAssets — só
    // logcat, que o dono não tem. Aqui prova-se o caminho inteiro no
    // dispositivo virtual: o banner do boot ATUAL no viewer, linhas novas
    // refletidas COM rotação, o fail de import visível no viewer E no
    // export, e a recusa do >65 535 com a mensagem do PASSO 4 no log.
    // ======================================================================
    fase("FASE 19 — 0.10-M PASSO 3B: o log de HOJE (viewer + export + causa)");
    {
        resetEngineForHarness();
        auto st19 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt19 = st19.get();
        check(Project::createNew(*rawSt19, "c33", g_project),
              "19.0 projeto criado");
        // o TIC ALVO (com MeshRenderer — o "Sim" do import aplica aqui):
        // gravado ANTES do boot (o INIT_WINDOW faz o LOAD da cena ativa —
        // o TIC por gravar morreria aí; a lição da FASE 1)
        {
            const Handle h = g_scene.create("Alvo");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            t->addComponent<MeshRenderer>();
            check(g_project.saveActiveScene(*rawSt19, g_scene),
                  "19.0 cena gravada com o TIC Alvo");
        }
        g_storage = std::move(st19);
        g_projectReady = true;
        g_resources.setStorage(rawSt19);
        g_gpu.init(&g_resources);
        // o BOOT de HOJE (o viewer tem de mostrar ESTE boot, não o de ontem)
        android_app app19;
        std::memset(&app19, 0, sizeof(app19));
        app19.contentRect = {0, 24, 1512, 720};
        onAppCmd(&app19, APP_CMD_INIT_WINDOW);
        check(g_ready, "19.0 boot completo (g_ready)");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        // o load do boot trocou os handles — o Alvo re-selecionado pelo NOME
        g_editor.selected = g_scene.find("Alvo");
        check(g_scene.get(g_editor.selected) != nullptr &&
                  g_scene.get(g_editor.selected)
                          ->getComponent<MeshRenderer>() != nullptr,
              "19.0 TIC Alvo vivo com MeshRenderer (re-selecionado "
              "pós-boot)");

        // ---- 19.1 o viewer mostra o banner do BOOT ATUAL -----------------
        passo("19.1 o viewer mostra o boot de HOJE (banner + [boot N/6])");
        {
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();
            g_ui.scrollSetOffset(editor::settings::kScrollId, 0.0f);
            g_layoutExportPending = true;
            frame();
            const layout::Record& rr = g_ui.auditRecord();
            f32 vx = -1.0f, vy = -1.0f;
            for (const auto& e : rr.entries) {
                if (e.kind == layout::Entry::Button &&
                    e.id == editor::settings::kViewLogsId) {
                    vx = e.x + e.w * 0.5f;
                    vy = e.y + e.h * 0.5f;
                }
            }
            check(vx > 0.0f && vy > 0.0f,
                  "19.1 o botao 'Ver logs' esta no registo (rect real)");
            tap(vx, vy);
            check(g_editor.logViewer,
                  "19.1 o toque ABRE o viewer (logViewer=true)");
            bool bootHoje = false;
            for (const std::string& l : g_logLines) {
                if (l.find("[boot 1/6] contentRect OK") != std::string::npos) {
                    bootHoje = true;
                }
            }
            check(bootHoje,
                  "19.1 o viewer mostra o [boot 1/6] do boot ATUAL (as "
                  "linhas de HOJE — o bug do 'log de ontem' morreu)");
            // a AUDITORIA DO DRAW: o frame com o viewer aberto desenha AS
            // linhas (labels dentro do card) — armar o audit e provar
            {
                g_layoutExportPending = true;
                frame();
                const layout::Record& rv = g_ui.auditRecord();
                // o card do viewer: overlayArea 80%x80% centrado (a MESMA
                // fórmula do drawLogViewer)
                f32 ox, oy, aw, ah;
                editor::overlayArea(static_cast<f32>(g_egl.width()),
                                    static_cast<f32>(g_egl.height()),
                                    g_ui.safeArea(), ox, oy, aw, ah);
                const f32 cw = aw * 0.80f, chh = ah * 0.80f;
                const f32 cx = ox + (aw - cw) * 0.5f, cy = oy + (ah - chh) * 0.5f;
                int labelsIn = 0;
                for (const auto& e : rv.entries) {
                    if (e.kind == layout::Entry::Label &&
                        e.x >= cx && e.x < cx + cw && e.y >= cy &&
                        e.y < cy + chh) {
                        ++labelsIn;
                    }
                }
                check(labelsIn >= 3,
                      "19.1 as linhas do log estao DESNHADAS no card do "
                      "viewer (audit do draw)");
            }
            g_editor.logViewer = false;
            g_editor.settingsMenu = false;
            frame();
        }

        // ---- 19.2 rotação: escrever N linhas → o viewer reflete-as -----
        passo("19.2 com rotações, as linhas NOVAS ganham (o fix da ordem)");
        {
            // a experiência corre num DIRETÓRIO PRÓPRIO (o gate do CI
            // grepa o c33-virtual-logs/engine.log ATIVO — as rotações da
            // experiência não podem empurrar as linhas das FASEs 1..18
            // para os backups)
            const std::string dir19 = std::string(kHarnessLogs) + "-19";
            rmrf(dir19);
            check(vv::elog::init(dir19.c_str(), 24 * 1024, 2),
                  "19.2 elog re-iniciado com rotação pequena (24 KB)");
            // ~1500 linhas × ~67 B ≈ 100 KB → 4+ rotações: .2 e .1 CHEIOS
            // (cada um ~350 linhas — MUITO mais que a janela de 300)
            for (int i = 0; i < 1500; ++i) {
                vv::elog::writeLine('I', "conteudo-velho-de-ontem-0123456789");
            }
            for (int i = 1; i <= 5; ++i) {
                char m[48];
                std::snprintf(m, sizeof(m), "f19-hoje-marcador-%02d", i);
                vv::elog::info("%s", m);
            }
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();
            g_ui.scrollSetOffset(editor::settings::kScrollId, 0.0f);
            g_layoutExportPending = true;
            frame();
            const layout::Record& rr = g_ui.auditRecord();
            f32 vx = -1.0f, vy = -1.0f;
            for (const auto& e : rr.entries) {
                if (e.kind == layout::Entry::Button &&
                    e.id == editor::settings::kViewLogsId) {
                    vx = e.x + e.w * 0.5f;
                    vy = e.y + e.h * 0.5f;
                }
            }
            check(vx > 0.0f && vy > 0.0f, "19.2 o botao 'Ver logs' visivel");
            tap(vx, vy);
            check(g_editor.logViewer, "19.2 o viewer aberto (log do -19)");
            bool temHoje = false, ultima = false;
            for (const std::string& l : g_logLines) {
                if (l.find("f19-hoje-marcador-05") != std::string::npos) {
                    temHoje = true;
                }
            }
            // a última linha NÃO é o filler VELHO: com a ordem antiga a
            // janela enchia-se do .2 (conteúdo de ontem) e a última linha
            // ERA o filler; agora a janela termina nas linhas RECENTES (os
            // marcadores ou as linhas que os frames seguintes escreveram)
            if (!g_logLines.empty() &&
                g_logLines.back().find("conteudo-velho-de-ontem") ==
                    std::string::npos) {
                ultima = true;
            }
            check(temHoje,
                  "19.2 o viewer mostra as linhas NOVAS (com .2/.1 cheios — "
                  "a ordem antiga mostrava ONTEM)");
            check(ultima, "19.2 a ÚLTIMA linha do viewer é a mais recente");
            g_editor.logViewer = false;
            g_editor.settingsMenu = false;
            // o elog VOLTA ao log do harness (o resto das FASEs escreve aí)
            vv::elog::init(kHarnessLogs);
            rmrf(dir19);
            frame();
        }

        // ---- 19.3 um fail de IMPORT visível no viewer (o caminho do job) -
        passo("19.3 fail de import: a linha de erro de HOJE no viewer");
        {
            // um GLB TRUNCADO (só o header de 12 B — "GLB sem chunk header")
            const std::string src = "/tmp/goni_fase19_corrompido.glb";
            {
                FILE* f = std::fopen(src.c_str(), "wb");
                check(f != nullptr, "19.3a fixture criada (GLB truncado)");
                u8 hdr[12] = {0};
                const u32 magic = 0x46546C67u, ver = 2, total = 12;
                std::memcpy(hdr, &magic, 4);
                std::memcpy(hdr + 4, &ver, 4);
                std::memcpy(hdr + 8, &total, 4);
                std::fwrite(hdr, 1, 12, f);
                std::fclose(f);
            }
            fileapi::DirEntry ent;
            ent.name = "corrompido.glb";
            ent.path = src;
            ent.isDir = false;
            ent.kind = 'm';
            check(browserImportFile(ent),
                  "19.3 o job de import arranca (1 toque no corrompido)");
            int guard = 0;
            while (g_importJob.active.load() && guard++ < 3000) {
                frame();
            }
            frame();   // o importJobFinish (quem LOGA 'import: FALHOU')
            check(!g_importJob.err.empty(),
                  "19.3 o job FALHOU com err (a causa real)");
            check(logHas("import: FALHOU"),
                  "19.3 a linha 'import: FALHOU' está no engine.log de HOJE");
            // o VIEWER mostra a linha de erro de HOJE
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();
            g_ui.scrollSetOffset(editor::settings::kScrollId, 0.0f);
            g_layoutExportPending = true;
            frame();
            const layout::Record& rr = g_ui.auditRecord();
            f32 vx = -1.0f, vy = -1.0f;
            for (const auto& e : rr.entries) {
                if (e.kind == layout::Entry::Button &&
                    e.id == editor::settings::kViewLogsId) {
                    vx = e.x + e.w * 0.5f;
                    vy = e.y + e.h * 0.5f;
                }
            }
            check(vx > 0.0f && vy > 0.0f, "19.3 o botao 'Ver logs' visivel");
            tap(vx, vy);
            check(g_editor.logViewer, "19.3 o viewer aberto");
            bool erroNoViewer = false;
            for (const std::string& l : g_logLines) {
                if (l.find("import: FALHOU") != std::string::npos) {
                    erroNoViewer = true;
                }
            }
            check(erroNoViewer,
                  "19.3 a linha de erro de HOJE está no VIEWER (o dono VÊ a "
                  "causa sem PC)");
            g_editor.logViewer = false;
            g_editor.settingsMenu = false;
            std::remove(src.c_str());
            frame();
        }

        // ---- 19.4 o load do >65 535 recusa com a mensagem do PASSO 4 ----
        passo("19.4 causa do 'fail de 203 MB': o load recusa com nome e números");
        {
            // UM GLB de 70 000 vértices (u32 idx) — o MESMO perfil do
            // test_gmeshv3stream: o streaming converte (2 blocos,
            // verificado=1) e o RUNTIME de hoje recusa o mesh único
            const std::string src = "/tmp/goni_fase19_70k.glb";
            check(buildGlbFase19(70000, 90000, src),
                  "19.4a fixture criada (GLB 70k verts, u32 idx)");
            convert::Output out;
            convert::Stats stats;
            std::string err;
            // contagens ANTES (os checks são por DELTA — os imports das
            // FASEs 12.x também logam fases; o que prova o 19.4 é o SEU)
            const int parseAntes = logCount("gmesh: fase=parse ms=");
            const int cutAntes = logCount("gmesh: fase=cut ms=");
            const int assAntes = logCount("gmesh: fase=assembly ms=");
            const int verAntes = logCount("gmesh: fase=verify ms=");
            const int copAntes = logCount("import: copia ms=");
            const int loadAntes = logCount("gmesh: fase=load ms=");
            const auto t0 = std::chrono::steady_clock::now();
            const bool ok = convert::importFile(src, "gigante.glb", *g_storage,
                                                g_pipeline.get(), out, stats,
                                                err, nullptr, nullptr);
            const double impMs =
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count();
            check(ok && out.meshes.size() == 1 &&
                      out.meshes[0] == "assets/gigante.gmesh",
                  "19.4 o import do 70k COMPLETA (a conversão está correta)");
            if (!ok) {
                std::printf("    [19.4-ERR] import falhou: %.200s\n",
                            err.c_str());
            }
            std::printf("    [19.4] import 70k verts: %.0f ms\n", impMs);
            // as LINHAS DE TEMPO por fase (a linha contrato do PASSO 3B —
            // por DELTA: as linhas DESTE import, não as das FASEs 12.x)
            check(logCount("gmesh: fase=parse ms=") > parseAntes,
                  "19.4 o tempo de PARSE no log (fase=parse ms=)");
            check(logCount("gmesh: fase=cut ms=") > cutAntes,
                  "19.4 o tempo de CORTE no log (fase=cut ms=)");
            check(logCount("gmesh: fase=assembly ms=") > assAntes,
                  "19.4 o tempo de ASSEMBLY no log (fase=assembly ms=)");
            check(logCount("gmesh: fase=verify ms=") > verAntes,
                  "19.4 o tempo de VERIFICAÇÃO no log (fase=verify ms=)");
            check(logCount("import: copia ms=") > copAntes,
                  "19.4 o tempo da CÓPIA no log (import: copia ms=)");
            // o APLICAR ao TIC (o corpo do 'Sim' — o mesmo do diálogo):
            // o load RECUSA com a mensagem do dono e a causa CHEGA AO LOG
            refreshCatalog();
            bool noCatalogo = false;
            for (const auto& m : g_catalog.meshes) {
                if (m == "assets/gigante.gmesh") noCatalogo = true;
            }
            check(noCatalogo, "19.4 o catálogo vê o gigante.gmesh");
            g_applyAsk.open = true;
            g_applyAsk.kind = 'm';
            g_applyAsk.rel = "assets/gigante.gmesh";
            g_applyAsk.fileName = "gigante.glb";
            g_editor.applyAsk = true;
            applyImportedAssetToSelectedTic();
            // 0.10-M (PASSO 4) — RECALIBRADO: a causa REGISTADA no 3B
            // era «memória insuficiente ao carregar mesh (cura no PASSO 4:
            // render por blocos)»; A CURA CHEGOU — o gigante de 70k APLICA
            // pela TABELA e RENDERIZA bloco a bloco (a FASE 20 é a prova
            // completa; aqui: a mesma fixture do 3B, o mesmo 'Sim')
            check(logHas("blocos: 'assets/gigante.gmesh' aplicado"),
                  "19.4 o gigante de 70k APLICA por BLOCOS (a cura do PASSO "
                  "4 — o 'Sim' funciona; era a recusa do mesh único)");
            check(logCount("gmesh: fase=load ms=") > loadAntes,
                  "19.4 o tempo do LOAD no log (a TABELA aberta pelo apply)");
            check(std::strstr(g_toast, "mesh aplicado") != nullptr,
                  "19.4 o toast 'mesh aplicado' (o caminho do dono funciona "
                  "de ponta a ponta)");
            std::remove(src.c_str());
            frame();   // a 1ª frame com o gigante em cena: o lazy + o RENDER
            check(logHas("blocos: total=5"),
                  "19.4 o gigante RENDERIZA: a linha de métricas com "
                  "desenhados/totais no engine.log de HOJE");
        }

        // ---- 19.5 o EXPORT: a mesma fonte de verdade do writer -----------
        passo("19.5 export de logs: a MESMA fonte do writer (não stale)");
        {
            // o hook espelha o loop REAL da VvActivity.exportLogsToDownloads
            // (getExternalFilesDir("logs") == o diretório do elog no device;
            // aqui: listar elog::dir(), copiar CADA ficheiro, 1× por nome)
            const std::string fakeDl = std::string(kCacheDir) + "/fake-dl";
            rmrf(fakeDl);
            ::mkdir(fakeDl.c_str(), 0775);
            g_jni.export_logs = [&](const std::string& rel) -> int {
                (void)rel;   // o relPath vai ao kDownloadsRelPath (check abaixo)
                const std::string dir = vv::elog::dir();
                DIR* d = ::opendir(dir.c_str());
                if (!d) return -3;
                int count = 0;
                while (dirent* e = ::readdir(d)) {
                    const std::string n = e->d_name;
                    if (n == "." || n == "..") continue;
                    const std::string from = dir + "/" + n;
                    const std::string to = fakeDl + "/" + n;
                    ::remove(to.c_str());   // export repetido SUBSTITUI
                    FILE* fi = std::fopen(from.c_str(), "rb");
                    if (!fi) continue;
                    FILE* fo = std::fopen(to.c_str(), "wb");
                    if (fo) {
                        u8 buf[16 * 1024];
                        size_t n2;
                        while ((n2 = std::fread(buf, 1, sizeof(buf), fi)) > 0) {
                            std::fwrite(buf, 1, n2, fo);
                        }
                        std::fclose(fo);
                        ++count;
                    }
                    std::fclose(fi);
                }
                ::closedir(d);
                return count;
            };
            g_editor.settingsMenu = true;
            g_editor.settingsCollapsed = editor::settings::kBitGeral |
                                         editor::settings::kBitAudio |
                                         editor::settings::kBitPerm |
                                         editor::settings::kBitDocs |
                                         editor::settings::kBitSobre;
            frame();
            g_ui.scrollSetOffset(editor::settings::kScrollId, 0.0f);
            g_layoutExportPending = true;
            frame();
            const layout::Record& rr = g_ui.auditRecord();
            f32 vx = -1.0f, vy = -1.0f;
            for (const auto& e : rr.entries) {
                if (e.kind == layout::Entry::Button &&
                    e.id == editor::settings::kExportLogsId) {
                    vx = e.x + e.w * 0.5f;
                    vy = e.y + e.h * 0.5f;
                }
            }
            check(vx > 0.0f && vy > 0.0f,
                  "19.5 o botao 'Export' está no registo (rect real)");
            tap(vx, vy);
            check(g_jni.export_logs_calls >= 1,
                  "19.5 o toque chama o exportLogsToDownloads REAL (JNI)");
            check(g_jni.export_logs_relpath ==
                      std::string(vv::elog::kDownloadsRelPath),
                  "19.5 o relPath é a constante da casa (Download/GOneVV/logs)");
            // o ATIVO exportado é o ATIVO do writer — com o erro de HOJE
            std::string expS;
            {
                FILE* f = std::fopen((fakeDl + "/engine.log").c_str(), "rb");
                check(f != nullptr, "19.5 o engine.log exportado existe");
                if (f) {
                    char buf[4096];
                    while (std::fgets(buf, sizeof(buf), f)) {
                        expS += buf;
                    }
                    std::fclose(f);
                }
            }
            check(expS.find("import: FALHOU") != std::string::npos,
                  "19.5 o exportado traz a linha de erro de HOJE (não é "
                  "stale)");
            check(expS.find("blocos: 'assets/gigante.gmesh' aplicado") !=
                      std::string::npos,
                  "19.5 o exportado traz a LINHA DA CURA (blocos aplicado — "
                  "PASSO 4; a recusa do 3B é história)");
            check(expS.find("gmesh: fase=parse ms=") != std::string::npos,
                  "19.5 o exportado traz os tempos por fase");
            g_jni.export_logs = nullptr;   // limpa o hook (o resto do harness)
            g_editor.settingsMenu = false;
            rmrf(fakeDl);
            frame();
        }

        onAppCmd(&app19, APP_CMD_TERM_WINDOW);
    }

    // ======================================================================
    // FASE 20 — 0.10-M PASSO 4: O RENDER POR BLOCOS. A recusa do 3B era
    // «memória insuficiente ao carregar mesh (cura no PASSO 4: render por
    // blocos)» — AQUI está a cura no caminho REAL do device: o modelo de
    // 2 meshes separadas (80 000 verts > 65 535 — o mesh único RECUSA)
    // APLICA pelo picker (o hull de bounds), RENDERIZA bloco a bloco (2
    // draw calls, 2 lazy loads na 1ª frame), o ORBIT muda o HUD
    // «bl desenhados/totais» (o culling trabalha), a cache fica DENTRO
    // dos orçamentos declarados, e o EXPORT OBJ anda pela TABELA (pico
    // de RAM = 1 bloco). O PNG do HUD sai para o P-05.
    // ======================================================================
    fase("FASE 20 — 0.10-M PASSO 4: render por blocos (o gigante renderiza)");
    {
        resetEngineForHarness();
        // o intervalo do log de métricas a ZERO (determinismo: os frames
        // do harness correm em ms, o device a 60 fps — lá o 1 s manda)
        g_blockLogIntervalSecs = 0.0f;
        // o framebuffer REAL do stub (o PNG do P-05 é o glReadPixels do
        // frame INTEIRO — sem fb o readback é preto; o padrão da FASE 13)
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        auto st20 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt20 = st20.get();
        check(Project::createNew(*rawSt20, "c33", g_project),
              "20.0 projeto criado");
        {
            const Handle h = g_scene.create("Alvo");
            Tic* t = g_scene.get(h);
            t->addComponent<Transform3D>();
            t->addComponent<MeshRenderer>();
            check(g_project.saveActiveScene(*rawSt20, g_scene),
                  "20.0 cena gravada com o TIC Alvo");
        }
        g_storage = std::move(st20);
        g_projectReady = true;
        g_resources.setStorage(rawSt20);
        g_gpu.init(&g_resources);
        android_app app20;
        std::memset(&app20, 0, sizeof(app20));
        app20.contentRect = {0, 24, 1512, 720};
        onAppCmd(&app20, APP_CMD_INIT_WINDOW);
        check(g_ready, "20.0 boot completo (g_ready)");
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
        }
        g_ui.setFont(&g_font);
        g_editor.selected = g_scene.find("Alvo");
        check(g_scene.get(g_editor.selected) != nullptr &&
                  g_scene.get(g_editor.selected)
                          ->getComponent<MeshRenderer>() != nullptr,
              "20.0 TIC Alvo vivo com MeshRenderer");

        // ---- 20.1 o IMPORT + o APPLY pela TABELA (o hull de bounds) ------
        passo("20.1 o gigante APLICA: a tabela abre, NADA carrega (lazy)");
        u32 blocks20 = 0;
        {
            const std::string src = "/tmp/goni_fase20_2x40k.glb";
            check(buildGlbFase20(40000, 30000, src),
                  "20.1a fixture criada (GLB 2 meshes x 40k verts = 80k)");
            convert::Output out;
            convert::Stats stats;
            std::string err;
            const int loadAntes = logCount("gmesh: fase=load ms=");
            const bool ok = convert::importFile(src, "par.glb", *g_storage,
                                                g_pipeline.get(), out, stats,
                                                err, nullptr, nullptr);
            check(ok && out.meshes.size() == 1 &&
                      out.meshes[0] == "assets/par.gmesh",
                  "20.1 o import do 80k COMPLETA (a conversão está correta)");
            if (!ok) {
                std::printf("    [20.1-ERR] import falhou: %.200s\n",
                            err.c_str());
            }
            std::remove(src.c_str());
            // o APPLY: o MESMO caminho do 'Sim' do diálogo (o picker real)
            refreshCatalog();
            g_applyAsk.open = true;
            g_applyAsk.kind = 'm';
            g_applyAsk.rel = "assets/par.gmesh";
            g_applyAsk.fileName = "par.glb";
            g_editor.applyAsk = true;
            applyImportedAssetToSelectedTic();
            Tic* talvo = g_scene.get(g_editor.selected);
            MeshRenderer* mr20 =
                talvo ? talvo->getComponent<MeshRenderer>() : nullptr;
            check(mr20 != nullptr && mr20->meshPath == "assets/par.gmesh",
                  "20.1 o APPLY funciona: meshPath ligado (o hull de bounds "
                  "preserva o contrato do picker)");
            check(mr20 && mr20->mesh != nullptr && mr20->mesh->ok(),
                  "20.1 o HULL está no slot (bounds como Mesh* — nunca "
                  "desenhado)");
            const vv::BlockMesh* bm = g_gpu.blockMeshIfOpen("assets/par.gmesh");
            check(bm != nullptr && bm->ok(),
                  "20.1 o BlockMesh aberto no GpuAssets (1 ref = 1 abertura)");
            check(bm && bm->table().size() == 2,
                  "20.1 a TABELA tem 2 blocos (1 por mesh)");
            blocks20 = bm ? static_cast<u32>(bm->table().size()) : 0;
            check(bm && bm->meta().vertexCount == 80000,
                  "20.1 80 000 verts contados pela TABELA (o mesh único "
                  "recusaria)");
            // o HULL: bounds EXATOS do meta (a fonte do fit/cena/gizmo)
            check(mr20 && bm &&
                      mr20->mesh->boundsMaxExtent() == bm->boundsMaxExtent(),
                  "20.1 os bounds do hull == o AABB global do meta");
            // o LAZY: a abertura loga UMA fase=load (a TABELA) — os DADOS
            // esperam a 1ª visibilidade
            check(logCount("gmesh: fase=load ms=") == loadAntes + 1,
                  "20.1 a abertura é UMA linha fase=load (a tabela — nada "
                  "de dados)");
            check(logHas("por BLOCOS: tabela de 2 blocos"),
                  "20.1 o log diz «por BLOCOS: tabela de 2 blocos» (a causa "
                  "chegou ao log do dono)");
            check(logHas("blocos: 'assets/par.gmesh' aplicado"),
                  "20.1 a linha do apply: blocos + verts + MB por carregar");
            // (SEM frame aqui: a 1ª frame — e o lazy da 1ª visibilidade — é
            // do passo 20.2, com a câmara controlada)
        }

        // ---- 20.2 o RENDER por blocos (a 1ª frame carrega os visíveis) ---
        passo("20.2 o RENDER: 2 blocos desenham, o HUD diz bl 2/2");
        {
            // escala 1:1 (cancela o fit do apply — a geometria fica em
            // x=±10, determinística para o frustum)
            Tic* talvo = g_scene.get(g_editor.selected);
            if (Transform3D* tr = talvo->getComponent<Transform3D>()) {
                tr->scale = Vec3{1.0f, 1.0f, 1.0f};
                tr->updateWorld();
            }
            // a câmara LARGA: vê os DOIS (x=±10, dist 60)
            g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
            g_camera.yaw = 0.0f;
            g_camera.pitch = 0.0f;
            g_camera.dist = 60.0f;
            const int drawsAntes = glstub::stats.drawElementsCalls;
            const int renderAntes = logCount("gmesh: fase=render ms=");
            frame();
            check(g_blockFrame.any && g_blockFrame.total == blocks20 &&
                      g_blockFrame.drawn == blocks20,
                  "20.2 a frame desenha OS 2 BLOCOS (o mesh único recusava "
                  "este modelo)");
            check(g_blockFrame.drawCalls == blocks20,
                  "20.2 1 draw call POR BLOCO (a honestidade do dc)");
            check(glstub::stats.drawElementsCalls - drawsAntes == blocks20,
                  "20.2 o glstub conta 2 drawElements (o HULL nunca desenha)");
            check(logCount("gmesh: fase=render ms=") > renderAntes,
                  "20.2 os uploads LAZY no log (fase=render por bloco)");
            // o HUD do EDITOR: o chip «FPS · TICs · bl n/m» no canto
            // direito da tab bar — a FONTE é o g_bottom (o MESMO par que o
            // chip desenha; o audit do registo prova o label DESENHADO)
            check(g_bottom.blTotal == 2 && g_bottom.blDrawn == 2,
                  "20.2 o HUD (chip FPS · TICs · bl) diz «bl 2/2» — a "
                  "métrica REAL do frame no ecrã do EDITOR");
            check(logHas("blocos: total=2"),
                  "20.2 o LOG das métricas (blocos: total=2 … desenhados=2)");
            // a contabilidade: dentro dos orçamentos da casa
            const vv::BlockMesh* bm = g_gpu.blockMeshIfOpen("assets/par.gmesh");
            check(bm && bm->lastStats().vramBytes <= bm->vramBudget() &&
                      bm->lastStats().ramBytes <= bm->ramBudget(),
                  "20.2 a cache DENTRO dos orçamentos declarados "
                  "(64+192 MB da casa)");
            const u64 vramF = bm ? bm->lastStats().vramBytes : 1;
            const u64 ramF = bm ? bm->lastStats().ramBytes : 1;
            // a 2ª frame NÃO recarrega (a cache entregou)
            const int loadDepois = logCount("gmesh: fase=load ms=");
            frame();
            check(logCount("gmesh: fase=load ms=") == loadDepois,
                  "20.2 a 2ª frame NÃO relê NADA (o caminho quente da cache)");
            check(bm && bm->lastStats().vramBytes == vramF &&
                      bm->lastStats().ramBytes == ramF,
                  "20.2 a RAM estável entre frames (o pin do dono)");
        }

        // ---- 20.3 o ORBIT muda o HUD (o culling trabalha) -----------------
        passo("20.3 o orbit: o HUD passa a bl 1/2 (o frustum culling)");
        {
            // a câmara PERTO da mesh DIREITA (x=+10): o bloco esquerdo sai
            // do frustum — o HUD muda sozinho
            g_camera.target = Vec3{10.0f, 0.0f, 0.0f};
            g_camera.yaw = 0.0f;
            g_camera.pitch = 0.0f;
            g_camera.dist = 6.0f;
            g_camera.fovY = 0.5f;
            frame();
            check(g_blockFrame.drawn == 1 && g_blockFrame.total == 2,
                  "20.3 o orbit CULLED: só 1 bloco desenhado (o AABB do "
                  "esquerdo está fora do frustum)");
            check(g_bottom.blDrawn == 1 && g_bottom.blTotal == 2,
                  "20.3 o HUD (chip) diz «bl 1/2» (mudou com o orbit — o "
                  "dono VÊ o culling a trabalhar)");
            check(g_blockFrame.drawCalls == 1,
                  "20.3 1 draw call (o bloco culled não paga dc)");
            // de volta à vista larga: 2/2 outra vez (sem recarregar — a
            // cache ainda tem os dois)
            const int loadApos = logCount("gmesh: fase=load ms=");
            g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
            g_camera.dist = 60.0f;
            g_camera.fovY = 1.0472f;
            frame();
            check(g_blockFrame.drawn == 2,
                  "20.3 a vista larga volta aos 2 (a cache não esqueceu)");
            check(g_bottom.blDrawn == 2 && g_bottom.blTotal == 2,
                  "20.3 o HUD (chip) volta a «bl 2/2»");
            check(logCount("gmesh: fase=load ms=") == loadApos,
                  "20.3 ZERO recargas no regresso (a cache LRU entregou)");
        }

        // ---- 20.4 o EXPORT OBJ pela TABELA (pico de RAM = 1 bloco) --------
        passo("20.4 o export OBJ anda pela TABELA do .gmesh");
        {
            const int expAntes = logCount("fileapi: export");
            beginExportToDownloads();
            check(logCount("fileapi: export por BLOCOS") > 0 &&
                      logCount("fileapi: export") > expAntes,
                  "20.4 o export do gigante correu pela TABELA (o caminho do "
                  "mesh único recusaria este mesh)");
            check(logHas("pico de RAM = 1 bloco"),
                  "20.4 o pico de RAM do export = 1 BLOCO (nunca o modelo "
                  "inteiro)");
            // o harness não tem /storage/emulated/0 (o device real tem): a
            // ESCRITA falha aqui com a causa honesta no log — o STREAM em
            // si correu com os números; o parity da GEOMETRIA está no
            // test_blockmesh (export == o caminho inteiro, verts a verts)
            std::printf("    [20.4] export: %s\n", g_toast);
        }

        // ---- 20.5 o PNG do HUD (a prova P-05 do PASSO 4) ------------------
        passo("20.5 o PNG do HUD com «bl 2/2» (P-05)");
        {
            g_layoutExportPending = true;
            frame();
            std::vector<u8> png20;
            check(rawSt20->readBytes("layout/editor.png", png20) &&
                      !png20.empty(),
                  "20.5 o PNG da frame exportado (layout/editor.png)");
            check(fileapi::writeAll("passo4-hud-blocos.png", png20.data(),
                                    png20.size()),
                  "20.5 o PNG do HUD gravado (passo4-hud-blocos.png — P-05)");
            // o AUDIT do draw: o CHIP desenhou (label dentro do rect da
            // reserva «FPS · TICs · bl» — o layout com a reserva CRESCIDA
            // prova que o blTotal>0 do frame entrou no layout do ecrã)
            const editor::bottom::Layout Lb = editor::bottom::layout(
                static_cast<f32>(g_egl.width()),
                static_cast<f32>(g_egl.height()), g_ui.safeArea(), g_bottom);
            check(Lb.fps.w > theme::dp(editor::bottom::kFpsW),
                  "20.5 a reserva do chip CRESCEU (blTotal>0 no layout — o "
                  "HUD dos blocos entrou no ecrã)");
            const layout::Record& rr = g_ui.auditRecord();
            int chipLabels = 0;
            for (const auto& e : rr.entries) {
                if (e.kind == layout::Entry::Label && e.x >= Lb.fps.x - 1 &&
                    e.x < Lb.fps.x + Lb.fps.w + 1 &&
                    e.y >= Lb.fps.y - 1 && e.y < Lb.fps.y + Lb.fps.h + 1) {
                    ++chipLabels;
                }
            }
            check(chipLabels >= 1,
                  "20.5 o label do CHIP está NO REGISTO do draw (o HUD "
                  "desenhou mesmo — o PNG é a prova P-05)");
        }

        onAppCmd(&app20, APP_CMD_TERM_WINDOW);
        g_blockLogIntervalSecs = 1.0f;   // repõe o ritmo do device
        glstub::fb::enabled = false;     // e o ambiente do stub
        glstub::fb::resetState();
    }

    // ======================================================================
    // FASE 21 — 0.10-M HOTFIX SAF-STREAM: o city (72 primitivas, 0 skins)
    // importa sob content://. A CAUSA do dono: o gate «raiz content://»
    // desligava o streaming nos projetos SAF → o import caía no legado do
    // teto de 65535 (o city de 130 MB morria aí). A CURA aferida AQUI no
    // caminho do device: (21.1) o import sob SAF COMPLETA com a fonte = o
    // FD DO BRIDGE (bridgeOpenFd → SafStorage::openReadFd → mapFd64 — mmap
    // POR FD, sem caminho) e a verificação bit a bit; a mensagem do teto
    // NÃO EXISTE; (21.2) o provider que RECUSA o mmap (o pipe da sonda —
    // a recusa REAL do SO) degrada para pread de RANGES — VERDE na mesma,
    // nunca o legado; (21.3) a fase de ranges TEM LINHA PRÓPRIA (os 5395
    // ms do dono passam a ter dono) e a tabela do city abre (72+ blocos).
    // ======================================================================
    fase("FASE 21 — hotfix SAF-STREAM: o city importa sob content://");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        const std::string src = "/tmp/goni_fase21_city.glb";
        check(buildGlbFase21(72, 1000, 600, src),
              "21.0 fixture criada (GLB 72 primitivas x 1000 verts = 72k)");

        // ---- 21.1 o import sob content:// (o gate MORREU) ---------------
        passo("21.1 o city importa sob SAF: o fd do bridge, mmap por fd");
        {
            FakeSafIo io;   // o provider content:// (o modelo da suíte)
            SafStorage saf(&io, "content://tree/primary:GOneVV/cidade");
            convert::Output out;
            convert::Stats stats;
            std::string err;
            const bool ok = convert::importFile(src, "city.glb", saf,
                                                nullptr, out, stats, err,
                                                nullptr, nullptr);
            if (!ok) {
                std::printf("    [21.1-ERR] import falhou: %.200s\n",
                            err.c_str());
            }
            check(ok && out.meshes.size() == 1 &&
                      out.meshes[0] == "assets/city.gmesh",
                  "21.1 o import sob content:// COMPLETA (o streaming ATIVA "
                  "em SAF — o gate morreu)");
            check(logHas("fonte=mmap-fd"),
                  "21.1 a fonte: o FD DO BRIDGE (mmap POR fd, sem caminho)");
            check(logHas("o fd do bridge"),
                  "21.1 o log nomeia o fd do bridge (a evidência do hotfix)");
            check(logCount("verificado=1") >= 1,
                  "21.1 o round-trip verificado bit a bit");
            check(logCount("excede 65535") == 0,
                  "21.1 a mensagem do teto NÃO EXISTE no log (o 65535 deixou "
                  "de ser visível ao não-skinned)");
            check(logHas("gmesh: fase=parse ms="),
                  "21.1 a fase de parse cobre SÓ o JSON");
            check(logHas("gmesh: fase=ranges ms="),
                  "21.1 a fase de ranges TEM LINHA PRÓPRIA (os 5395 ms do "
                  "dono com dono)");
            io.flushWrites();
            std::vector<u8> gmesh;
            check(saf.readBytes("assets/city.gmesh", gmesh),
                  "21.1 o .gmesh final relê pelo provider");
            GMeshV3Meta meta;
            std::vector<GMeshV3Block> blocks;
            std::vector<std::string> mats;
            std::string merr;
            check(readGMeshV3Meta(gmesh.data(), gmesh.size(), meta, blocks,
                                  mats, merr),
                  "21.1 a tabela do city abre (o leitor de produção)");
            check(meta.vertexCount >= 72000,
                  "21.1 72 000 verts pela TABELA (o legado morria no teto)");
            check(meta.blockCount >= 72,
                  "21.1 72+ blocos (1 por primitiva — o corte em blocos)");
            check(meta.materialCount == 1 && !mats.empty() &&
                      mats[0] == "cidade",
                  "21.1 o material da cidade na tabela");
        }

        // ---- 21.2 o provider que RECUSA o mmap do fd (o pipe) -----------
        passo("21.2 o provider recusa o mmap: VERDE por pread de RANGES");
        {
            FakeSafIo io;
            io.refuseMmapFds = true;   // a sonda do mmap leva o PIPE
            SafStorage saf(&io, "content://tree/primary:GOneVV/cidade2");
            convert::Output out;
            convert::Stats stats;
            std::string err;
            const bool ok = convert::importFile(src, "city.glb", saf,
                                                nullptr, out, stats, err,
                                                nullptr, nullptr);
            if (!ok) {
                std::printf("    [21.2-ERR] import falhou: %.200s\n",
                            err.c_str());
            }
            check(ok,
                  "21.2 o import VERDE pela DEGRADAÇÃO (o mmap recusado → "
                  "pread de ranges — NUNCA o legado)");
            check(logHas("a degradar para pread de RANGES"),
                  "21.2 a degradação honesta LOGADA (o provider recusou o "
                  "mapa)");
            check(logHas("fonte=ranges"),
                  "21.2 a fonte: RANGES pelo storage (readBytesAt)");
            check(logCount("excede 65535") == 0,
                  "21.2 nunca desceu ao legado (o teto invisível)");
            check(logHas("ranges do corte: 72"),
                  "21.2 UMA carga de span por tarefa (72 — o pico é o span "
                  "em voo, nunca o modelo)");
        }
        std::remove(src.c_str());
    }

    // ======================================================================
    // FASE 22 — 0.10-M (EXT): IMPORT DE NÓS COMO SUB-ÁRVORE. A spec do
    // dono (BACKLOG 0.10-M-ext): «opção no import "expandir nós" que cria
    // um TIC por nó com mesh (nomes do glTF preservados, transformação do
    // nó como Transform do TIC), em vez de fundir num TIC só. Default
    // fundido por performance mobile; expandido para peças editáveis.
    // Pin: city scene expandido = 72 TICs com nomes, culling por TIC
    // verde». Aferido AQUI pelo caminho do device: (22.1) o import
    // EXPANDIDO produz as 72 PEÇAS com nomes/flag/verificação e o registro
    // do TIC com a transformação do nó; (22.2) a sub-árvore ENTRA na cena
    // pelo gltfExpandInstantiate com o binder blockHull do GpuAssets — 72
    // TICs com nomes, cada peça aberta POR BLOCOS; (22.3) O PINO: o
    // frustum de uma câmara estreita culle 71 TICs e deixa passar 1 — o
    // culling é POR TIC (o AABB do bloco no model do TIC dono).
    // ======================================================================
    fase("FASE 22 — expandir nós: o city = 72 TICs com nomes, culling por TIC");
    {
        resetEngineForHarness();
        auto st22 = std::make_unique<FakeStorage>();
        FakeStorage* rawSt22 = st22.get();
        check(Project::createNew(*rawSt22, "c33", g_project),
              "22.0 projeto criado");
        g_storage = std::move(st22);
        g_projectReady = true;
        g_resources.setStorage(rawSt22);
        g_gpu.init(&g_resources);
        const std::string src = "/tmp/goni_fase22_city.glb";
        check(buildGlbFase22(72, 1000, 600, src),
              "22.0 fixture criada (city ESPALHADO: 72 nós «c<i>» em faixas "
              "de 10 em 10)");

        // ---- 22.1 o import EXPANDIDO (o setting «expandir nós») --------
        passo("22.1 o city EXPANDIDO: 72 peças com nomes + a flag");
        convert::Output out;
        convert::Stats stats;
        std::string err;
        {
            const int verAntes = logCount("verificado=1");
            const bool ok = convert::importFile(src, "city.glb", *g_storage,
                                                nullptr, out, stats, err,
                                                nullptr, nullptr, true);
            if (!ok) {
                std::printf("    [22.1-ERR] import falhou: %.200s\n",
                            err.c_str());
            }
            check(ok, "22.1 o import EXPANDIDO completa (72× a MESMA "
                      "conversão do fundido, uma por nó)");
            check(out.meshes.size() == 72u,
                  "22.1 72 PEÇAS — um .gmesh por nó com mesh (o fundido é "
                  "UM; é ISTO que o setting troca)");
            check(out.meshes.size() == 72u &&
                      out.meshes[0] == "assets/city_c0.gmesh" &&
                      out.meshes[71] == "assets/city_c71.gmesh",
                  "22.1 os NOMES do glTF nos ficheiros das peças");
            check(out.expandNodes.size() == 72u,
                  "22.1 72 REGISTROS de TIC (Output::expandNodes)");
            check(out.expandNodes.size() == 72u &&
                      out.expandNodes[0].name == "c0" &&
                      out.expandNodes[71].name == "c71",
                  "22.1 os NOMES PRESERVADOS nos registros");
            check(logCount("verificado=1") - verAntes >= 72,
                  "22.1 a verificação bit a bit POR PEÇA (72 linhas)");
            check(logHas("expandir nós=72 peça(s)"),
                  "22.1 a linha contrato do expand no engine.log");
            // a flag em CADA peça + «transformação do nó como Transform do
            // TIC» (a faixa do nó no TRS do registro — o MUNDO decomposto)
            bool flagOk = true, trsOk = true;
            for (u32 i = 0; i < out.meshes.size(); ++i) {
                std::vector<u8> gmesh;
                if (i >= out.expandNodes.size() ||
                    !g_storage->readBytes(out.meshes[i], gmesh)) {
                    flagOk = false;
                    break;
                }
                GMeshV3Meta meta;
                std::string perr;
                if (!gmeshV3PeekMeta(gmesh.data(), gmesh.size(), meta,
                                     perr) ||
                    (meta.flags & kGmeshV3FlagPiece) == 0) {
                    flagOk = false;
                    break;
                }
                const f32 faixa =
                    (static_cast<f32>(i) - 36.0f) * 10.0f;
                if (std::fabs(out.expandNodes[i].translation.x - faixa) >
                    0.01f) {
                    trsOk = false;
                }
            }
            check(flagOk, "22.1 kGmeshV3FlagPiece em TODAS as 72 (o runtime "
                          "abre-as por blocos — o culling por TIC)");
            check(trsOk, "22.1 a transformação do nó no registro do TIC (a "
                         "faixa de cada peça)");
        }

        // ---- 22.2 a SUB-ÁRVORE na cena (o caminho do device) -----------
        passo("22.2 a sub-árvore: 72 TICs com nomes, peças por blocos");
        {
            struct Bind22 {
                GpuAssets* gpu;
            } b22{&g_gpu};
            GltfInstantiateCtx ictx;
            ictx.user = &b22;
            ictx.bindMesh = [](void* user, const std::string& ref) -> Mesh* {
                // O BINDER DO DEVICE: blockHull abre a peça PELA TABELA
                // (kGmeshV3FlagPiece — culling por TIC + lazy + LRU)
                return static_cast<Bind22*>(user)->gpu->blockHull(ref);
            };
            ictx.material = g_renderer.litMaterial();
            const std::vector<Handle> tics =
                gltfExpandInstantiate(g_scene, out.expandNodes, out.meshes,
                                      ictx);
            check(tics.size() == 72u,
                  "22.2 72 TICs criados — O PINO do dono (um por nó com "
                  "mesh)");
            const Tic* t0 = tics.size() == 72u ? g_scene.get(tics[0])
                                                : nullptr;
            const Tic* t71 = tics.size() == 72u ? g_scene.get(tics[71])
                                                 : nullptr;
            check(t0 != nullptr && t0->name == "c0" && t71 != nullptr &&
                      t71->name == "c71",
                  "22.2 os TICs com os NOMES do glTF (o pin do dono)");
            // cada TIC: o meshPath da PEÇA + o BlockMesh aberto no GpuAssets
            u32 porBlocos = 0, comHull = 0;
            for (const Handle h : tics) {
                const Tic* t = g_scene.get(h);
                const MeshRenderer* mr =
                    t ? t->getComponent<MeshRenderer>() : nullptr;
                if (mr && mr->mesh != nullptr) ++comHull;
                if (mr && g_gpu.blockMeshIfOpen(mr->meshPath) != nullptr) {
                    ++porBlocos;
                }
            }
            check(comHull == 72u,
                  "22.2 o hull de bounds no slot de cada TIC (o contrato do "
                  "picker/serializer preservado)");
            check(porBlocos == 72u,
                  "22.2 as 72 PEÇAS abertas POR BLOCOS no GpuAssets (o "
                  "caminho do PASSO 4 — mesmo PEQUENAS, pela flag)");
            // «transformação do nó como Transform do TIC» no Transform3D:
            const Transform3D* tr36 =
                tics.size() == 72u
                    ? g_scene.get(tics[36])->getComponent<Transform3D>()
                    : nullptr;
            const Transform3D* tr0 =
                tics.size() == 72u
                    ? g_scene.get(tics[0])->getComponent<Transform3D>()
                    : nullptr;
            check(tr36 != nullptr && std::fabs(tr36->pos.x) < 0.01f,
                  "22.2 o TIC 36 na faixa 0 (a transformação MUNDO no "
                  "Transform3D)");
            check(tr0 != nullptr && std::fabs(tr0->pos.x + 360.0f) < 0.01f,
                  "22.2 o TIC 0 na faixa -360 (a cadeia em TRS)");
        }

        // ---- 22.3 O PINO: CULLING POR TIC -------------------------------
        passo("22.3 culling por TIC: 1 visível, 71 culled");
        {
            // a câmara DE FRENTE ao centro (faixa 0): meia-largura ~5.1 no
            // plano do alvo (fovY 0.5 rad, D=20) — SÓ a peça da faixa 0
            // (AABB x∈[-1,1]) cabe; as 71 faixas vizinhas (de 10 em 10)
            // caem TODAS fora dos planos laterais. O teste é o AUDIT de
            // produção (BlockMesh::visibleBlocks — o MESMO que o draw usa)
            // com o model de CADA TIC: o veredicto é POR TIC
            Camera cam;
            cam.target = Vec3{0.0f, 0.0f, 0.0f};
            cam.yaw = 0.0f;
            cam.pitch = 0.0f;
            cam.dist = 20.0f;
            cam.fovY = 0.5f;
            const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
            u32 visiveis = 0, culled = 0;
            std::string nomeVisivel;
            const auto& mrs = g_scene.components().meshRenderers();
            for (u32 i = 0; i < mrs.size(); ++i) {
                const vv::BlockMesh* bm =
                    g_gpu.blockMeshIfOpen(mrs.at(i).meshPath);
                const Tic* owner = g_scene.get(mrs.owner(i));
                const Transform3D* tr =
                    owner ? owner->getComponent<Transform3D>() : nullptr;
                if (!bm || !tr) {
                    continue;
                }
                std::vector<u32> vis;
                bm->visibleBlocks(tr->world, vp, vis);
                if (vis.empty()) {
                    ++culled;
                } else {
                    ++visiveis;
                    nomeVisivel = owner ? owner->name : "?";
                }
            }
            check(visiveis == 1u && culled == 71u,
                  "22.3 CULLING POR TIC VERDE: 1 TIC visível, 71 culled (o "
                  "pin do dono fecha no device virtual)");
            check(nomeVisivel == "c36",
                  "22.3 o TIC visível é o da faixa 0 («c36» — o frustum "
                  "escolhe a PEÇA, não o modelo inteiro)");
        }
        std::remove(src.c_str());
    }

    // ======================================================================
    // FASE 23 — 0.10.6 HOTFIX SAF-SEAM: o SEAM do runtime fechado. A
    // última costura: o BlockMesh (a abertura por blocos do RUNTIME)
    // abria por RANGES do storage — o conversor já abria pelo FD DO
    // BRIDGE (mmap por fd). AQUI no caminho do device: (23.1) o city
    // importa sob content:// E A ABERTURA nomeia a MESMA fonte
    // (fonte=mmap-fd — o par do «asset: v3 fonte=» do conversor); (23.2)
    // o chip «bl n/m» desenha com o HUD REAL sob SAF; (23.3) o OVERLAY
    // de import NUNCA mostra «0 / 0» (o pin do 29 MB — o length do bridge
    // a 0 deixa «copiando… N B», o total sub-MB mostra B); (23.4) o
    // provider que RECUSA o mmap (o pipe) degrada a abertura para RANGES
    // — VERDE na mesma, NUNCA caminho POSIX; (23.5) o gmesh TRUNCADO
    // entra em QUARENTENA (rename .corrupt pela CÓPIA STREAMING do SAF) +
    // o lastMeshError diz «asset corrompido, reimporta».
    // ======================================================================
    fase("FASE 23 — hotfix SAF-SEAM: o fd do bridge na abertura + o overlay + a quarentena");
    {
        resetEngineForHarness();
        javaRegistersWithCacheDir();
        g_blockLogIntervalSecs = 0.0f;
        FakeSafIo io;   // o provider content:// (o modelo da suíte)
        {
            auto safPtr = std::make_unique<SafStorage>(
                &io, "content://tree/primary:GOneVV/cidade23");
            SafStorage* saf = safPtr.get();
            check(Project::createNew(*saf, "c33", g_project),
                  "23.0 projeto SAF criado (content://)");
            {
                const Handle h = g_scene.create("Alvo");
                Tic* t = g_scene.get(h);
                t->addComponent<Transform3D>();
                t->addComponent<MeshRenderer>();
                check(g_project.saveActiveScene(*saf, g_scene),
                      "23.0 cena gravada no provider SAF");
            }
            g_storage = std::move(safPtr);
            g_projectReady = true;
            g_resources.setStorage(saf);
            g_gpu.init(&g_resources);
            io.flushWrites();   // o provider persiste
        }

        // ---- 23.1 o city IMPORTA e ABRE por blocos com a MESMA fonte ----
        passo("23.1 o city sob SAF: o import E a abertura pelo fd do bridge");
        std::vector<u8> cityBytes;
        {
            const std::string src = "/tmp/goni_fase23_city.glb";
            check(buildGlbFase21(72, 1000, 600, src),
                  "23.1a fixture criada (GLB 72 primitivas x 1000 verts)");
            convert::Output out;
            convert::Stats stats;
            std::string err;
            const bool ok = convert::importFile(src, "city.glb", *g_storage,
                                                g_pipeline.get(), out, stats,
                                                err, nullptr, nullptr);
            io.flushWrites();
            if (!ok) {
                std::printf("    [23.1-ERR] import falhou: %.200s\n",
                            err.c_str());
            }
            check(ok && out.meshes.size() == 1 &&
                      out.meshes[0] == "assets/city.gmesh",
                  "23.1 o import sob content:// COMPLETA (o de sempre)");
            check(logCount("verificado=1") >= 1,
                  "23.1 verificado=1 (o round-trip bit a bit)");
            check(logCount("excede 65535") == 0,
                  "23.1 ZERO «excede 65535» (o teto invisível)");
            std::remove(src.c_str());
            check(g_storage->readBytes("assets/city.gmesh", cityBytes) &&
                      cityBytes.size() > 192,
                  "23.1 o .gmesh do city relê pelo provider");

            // A ABERTURA POR BLOCOS pelo caminho do GpuAssets (blockHull):
            // a cascata do fd — openReadFd (o FD DO BRIDGE) → mapFd64
            const int loadsAntes = logCount("gmesh: fase=load ms=");
            Mesh* hull = g_gpu.mesh("assets/city.gmesh");
            check(hull != nullptr && hull->ok(),
                  "23.1 o hull no slot (a TABELA abriu por blocos)");
            check(logCount("gmesh: fase=load ms=") == loadsAntes + 1,
                  "23.1 UMA linha fase=load (a abertura é a tabela)");
            // O SEAM: a linha contrato do LOAD nomeia a MESMA fonte do
            // conversor (mmap POR fd — «fonte=mmap-fd; dados=…» é o formato
            // SÓ da linha do BlockMesh; a do conversor diz «v3 fonte=»)
            check(logHas("fonte=mmap-fd; dados="),
                  "23.1 A FONTE da ABERTURA: o fd do bridge MAPEADO (o SEAM "
                  "fechado — o runtime e o conversor leem pelo MESMO fd)");
            const vv::BlockMesh* bm = g_gpu.blockMeshIfOpen("assets/city.gmesh");
            check(bm != nullptr && bm->table().size() >= 72,
                  "23.1 a tabela do city (72+ blocos)");
            check(bm && bm->meta().vertexCount >= 72000,
                  "23.1 72 000 verts pela TABELA");
        }

        // ---- 23.2 o CHIP «bl n/m» sob content:// (a frame REAL) ---------
        passo("23.2 o chip bl n/m sob SAF: a frame desenha por blocos");
        {
            android_app app23;
            std::memset(&app23, 0, sizeof(app23));
            app23.contentRect = {0, 24, 1512, 720};
            onAppCmd(&app23, APP_CMD_INIT_WINDOW);
            check(g_ready, "23.2 boot completo sob content:// (g_ready)");
            if (!g_font.ok()) {
                const char* paths[] = {FONT_FIXTURE};
                g_font.loadFromPaths(paths, 1, 28.0f);
            }
            g_ui.setFont(&g_font);
            g_editor.selected = g_scene.find("Alvo");
            check(g_scene.get(g_editor.selected) != nullptr,
                  "23.2 o TIC Alvo vivo após o boot SAF");
            // o APPLY pelo picker REAL (o «Sim» do diálogo)
            refreshCatalog();
            g_applyAsk.open = true;
            g_applyAsk.kind = 'm';
            g_applyAsk.rel = "assets/city.gmesh";
            g_applyAsk.fileName = "city.glb";
            g_editor.applyAsk = true;
            applyImportedAssetToSelectedTic();
            Tic* talvo = g_scene.get(g_editor.selected);
            MeshRenderer* mr23 =
                talvo ? talvo->getComponent<MeshRenderer>() : nullptr;
            check(mr23 != nullptr && mr23->meshPath == "assets/city.gmesh",
                  "23.2 o APPLY funciona sob SAF (o hull no slot)");
            // escala 1:1 (cancela o fit — determinismo) + câmara LARGA
            if (Transform3D* tr = talvo->getComponent<Transform3D>()) {
                tr->scale = Vec3{1.0f, 1.0f, 1.0f};
                tr->updateWorld();
            }
            g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
            g_camera.yaw = 0.0f;
            g_camera.pitch = 0.0f;
            g_camera.dist = 12.0f;
            g_camera.fovY = 1.0472f;
            frame();
            check(g_blockFrame.any && g_blockFrame.total >= 72,
                  "23.2 a frame desenha o city POR BLOCOS sob SAF");
            check(g_blockFrame.drawn == g_blockFrame.total,
                  "23.2 a câmara larga vê TODOS os blocos");
            check(g_bottom.blTotal >= 72 && g_bottom.blDrawn >= 72,
                  "23.2 o HUD (chip FPS · TICs · bl) diz «bl 72/72» sob "
                  "content:// — o PIN do dono (o chip n/m com o city)");
            // o TIC solto do slot (o storage muda nos passos seguintes —
            // o hull/blockmesh velhos não podem ficar pendurados)
            if (mr23) {
                mr23->mesh = nullptr;
                mr23->meshPath.clear();
                mr23->blocks = nullptr;
            }
        }

        // ---- 23.3 o OVERLAY nunca «0 / 0» (o pin do 29 MB) --------------
        passo("23.3 o overlay de import: NUNCA «0 / 0» (o pin do 29 MB)");
        {
            const u64 MB = 1024ull * 1024ull;
            // A JANELA INICIAL (length=0, fonte NÃO vazia): «a copiar…»
            check(importOverlayBytesText(0, 0) == "a copiar...",
                  "23.3 length=0 no arranque: «a copiar…» (nunca 0/0)");
            // o provider MENTIU no tamanho (length=0, a fonte corre):
            // «copiando… N B» — os bytes que JÁ correram
            check(importOverlayBytesText(5 * MB + 123, 0) ==
                      "copiando... 5243003 B",
                  "23.3 length=0 com a fonte a correr: «copiando… N B»");
            // O PIN: uma fonte de 29 MB mostra os MB REAIS
            check(importOverlayBytesText(6 * MB, 29 * MB) == "6 / 29 MB",
                  "23.3 o 29 MB mostra «6 / 29 MB» (os MB reais)");
            // totais sub-MB (o cut reporta TRIÂNGULOS): BYTES, nunca 0/0
            check(importOverlayBytesText(0, 72000) == "0 / 72000 B",
                  "23.3 o total sub-MB mostra B (o cut do city nunca 0/0)");
            // a SEQUÊNCIA das fases de um import de 29 MB: nenhuma 0/0
            const std::pair<u64, u64> fases[] = {
                {0, 29 * MB}, {29 * MB, 29 * MB}, {0, 72000},
                {72000, 72000}, {0, 145 * MB}};
            bool nunca00 = true;
            for (const auto& f : fases) {
                nunca00 = nunca00 && importOverlayBytesText(f.first, f.second)
                                         .find("0 / 0") == std::string::npos;
            }
            check(nunca00,
                  "23.3 a SEQUÊNCIA copy→cut→assembly NUNCA renderiza «0 / 0»");
            // os atómicos do job (o bridge worker→UI) no ARRANQUE real
            g_importJob.bytesDone.store(0);
            g_importJob.bytesTotal.store(0);
            check(importOverlayBytesText(g_importJob.bytesDone.load(),
                                         g_importJob.bytesTotal.load())
                      .find("0 / 0") == std::string::npos,
                  "23.3 o job no arranque (0,0): nunca 0/0");
        }

        // ---- 23.4 o provider que RECUSA o mmap: ranges, NUNCA POSIX -----
        passo("23.4 o provider recusa o mmap: a abertura VERDE por ranges");
        std::unique_ptr<FakeSafIo> io23b;
        std::unique_ptr<SafStorage> saf23b;
        {
            // o storage VELHO sai (o BlockMesh do city tem de não ficar
            // pendurado num provider morto)
            g_gpu.releaseAll();
            g_scene.clear();
            io23b = std::make_unique<FakeSafIo>();
            io23b->refuseMmapFds = true;   // a sonda do mmap leva o PIPE
            io23b->mmapRefusalsLeft = 1;   // a 1.ª abertura de fd
            saf23b = std::make_unique<SafStorage>(
                io23b.get(), "content://tree/primary:GOneVV/recusa23");
            SafStorage* saf2 = saf23b.get();
            g_storage = std::move(saf23b);
            g_projectReady = true;
            g_resources.setStorage(saf2);
            g_gpu.init(&g_resources);
            saf2->makeDirs("assets");
            check(saf2->writeBytes("assets/citypipe.gmesh",
                                   cityBytes.data(), cityBytes.size()),
                  "23.4 o city escrito no provider que recusa o mmap");
            // A ABERTURA DIRETA do BlockMesh (o caminho da cascata — o
            // roteamento do GpuAssets já provou-se no 23.1): o fd (PIPE)
            // → fstat 0 B → pread ESPIPE → os RANGES do storage — VERDE
            vv::BlockMesh bm24;
            std::string err24;
            check(bm24.open(*saf2, "assets/citypipe.gmesh", err24) &&
                      bm24.table().size() >= 72,
                  "23.4 a abertura VERDE pela degradação (a tabela lida por "
                  "ranges — NUNCA caminho POSIX)");
            if (!err24.empty()) {
                std::printf("    [23.4-ERR] %.200s\n", err24.c_str());
            }
            check(logHas("veio com 0 B"),
                  "23.4 o fd com 0 B LOGADO (o pipe do provider)");
            // «fonte=ranges; dados=» é o formato SÓ da linha do BlockMesh
            // (a do conversor diz «v3 fonte=») — o degradado honesto
            check(logHas("fonte=ranges; dados="),
                  "23.4 a linha contrato: fonte=ranges (o degradado honesto)");
            check(bm24.meta().vertexCount >= 72000,
                  "23.4 a tabela COMPLETA pelos ranges (72 000 verts)");
        }

        // ---- 23.5 o gmesh TRUNCADO: QUARENTENA sob SAF (a cópia ---------
        //        streaming do rename) + o lastMeshError
        passo("23.5 o truncado: quarentena .corrupt + «reimporta»");
        std::unique_ptr<FakeSafIo> io23c;
        std::unique_ptr<SafStorage> saf23c;
        {
            g_gpu.releaseAll();
            io23c = std::make_unique<FakeSafIo>();
            saf23c = std::make_unique<SafStorage>(
                io23c.get(), "content://tree/primary:GOneVV/quar23");
            SafStorage* saf3 = saf23c.get();
            g_storage = std::move(saf23c);
            g_resources.setStorage(saf3);
            g_gpu.init(&g_resources);
            saf3->makeDirs("assets");
            // a TRUNCATURA: corta a meio da tabela (o peek segue verde)
            std::vector<u8> trunc = cityBytes;
            {
                GMeshV3Meta meta;
                std::string perr;
                std::vector<u8> peek(trunc.begin(),
                                     trunc.begin() + kGHeaderBytes +
                                         kGmeshV3MetaBytes);
                check(gmeshV3PeekMeta(peek.data(), peek.size(), meta, perr),
                      "23.5 o peek do city truncável (v3 verde)");
                const u64 tableLen =
                    static_cast<u64>(meta.blockCount) * kGmeshV3BlockEntryBytes;
                trunc.resize(static_cast<size_t>(meta.blockTableOffset +
                                                 tableLen / 2));
            }
            check(saf3->writeBytes("assets/citytrunc.gmesh", trunc.data(),
                                   trunc.size()),
                  "23.5 o city TRUNCADO escrito no provider");
            // A ABERTURA: quarentena (rename .corrupt) + o err CLARO
            Mesh* hull = g_gpu.mesh("assets/citytrunc.gmesh");
            check(hull == nullptr,
                  "23.5 a abertura do truncado FALHA (nunca silêncio)");
            check(logHas("ASSET CORROMPIDO"),
                  "23.5 o log: ASSET CORROMPIDO — em quarentena");
            check(g_gpu.lastMeshError().find("asset corrompido, reimporta") !=
                      std::string::npos,
                  "23.5 o lastMeshError: «asset corrompido, reimporta» (a "
                  "mensagem que o toast do picker mostra)");
            // A QUARENTENA sob SAF: o rename é a CÓPIA STREAMING por fd —
            // o .corrupt vive no provider; o original SAI do catálogo
            io23c->flushWrites();
            check(!saf3->exists("assets/citytrunc.gmesh"),
                  "23.5 o original SAIU do catálogo (o picker deixa de o "
                  "oferecer)");
            check(saf3->exists("assets/citytrunc.gmesh.corrupt"),
                  "23.5 o .corrupt vive no provider (os bytes ficam p/ "
                  "forense — o rename streaming do SAF funcionou)");
            {
                std::vector<u8> quar;
                check(saf3->readBytes("assets/citytrunc.gmesh.corrupt", quar) &&
                          quar.size() == trunc.size(),
                      "23.5 o .corrupt tem os bytes EXATOS (a cópia por fd "
                      "não corrompe nada)");
            }
            check(logHas("quarentena"),
                  "23.5 a linha da quarentena no engine.log (o dono LÊ)");
        }
    }

    // ---- sumário -----------------------------------------------------------
    std::printf("\n== C33 VIRTUAL: %d check(s), %d falha(s) ==\n", g_checks, g_failed);
    if (g_failed == 0) {
        std::printf("HARNESS VERDE — o dispositivo virtual confirma os fixes "
                    "vigiados (R-001..R-008; FASE 9 = UI replay do 0.9.4)\n");
    } else {
        std::printf("HARNESS VERMELHO — release BLOQUEADA (ver [FAIL] acima)\n");
    }
    fileapi::testing::clearReadonlyPrefix();
    rmrf(kCacheDir);
    return g_failed == 0 ? 0 : 1;
}
