// tests/test_texbind.cpp — 0.10-M HOTFIX TEXTURAS-VINCULA (+ e/f).
//
// Pins do dono:
//   · import com 2+ texturas → GTB1 no .gmesh + 1 linha de log por slot
//     vinculado (o render texturizado sem toque manual é o caminho do
//     GpuAssets/blockHull — os binds lá são lidos do MESMO bloco)
//   · modelo com 4 materiais = 4 bindings
//   · sintético com 50 texturas = 50 vinculadas (round-trip do bloco)
//   · atribuir a um slot NÃO limpa os irmãos (auditoria de estado — a
//     causa (d) do bug morre: o pick escreve O SLOT)
//   · sub-árvore REAL (e/f): parentRec + TRS local + composição no
//     TransformSystem (a dívida «editor polish» paga)
//   · retro-compat: ficheiro SEM GTB1 abre igual; CRC mau = erro legível
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>   // getpid (os dirs /tmp por teste)
#include "assets/AssetConverter.h"
#include "assets/GOwnFormats.h"
#include "assets/GltfInstantiate.h"
#include "assets/ResourceManager.h"   // o matBindsOf REAL (HOTFIX-2)
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/FsStorage.h"
#include "core/Scene.h"
#include "core/TransformSystem.h"
#include "platform/EngineLog.h"   // elog::init — o log que os pins leem
#include "render/GpuAssets.h"     // o matBindsOf REAL (HOTFIX-2)
#include "render/Mesh.h"          // o sentinela do apply (HOTFIX-2)
#include "render/Texture.h"
#include "ui/EditorLayout.h"      // MULTIMAT: o plano (o seletor + o sufixo)
#include "ui/EditorUi.h"

using namespace vv;

namespace {

std::vector<u8> readFixture(const char* name) {
    std::string path = std::string(FIXTURE_DIR) + "/" + name;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return {};
    }
    std::vector<u8> bytes;
    u8 buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        bytes.insert(bytes.end(), buf, buf + n);
    }
    std::fclose(f);
    return bytes;
}

std::string b64encode(const u8* data, size_t n) {
    static const char* t =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((n + 2) / 3) * 4);
    for (size_t i = 0; i < n; i += 3) {
        const u32 b0 = data[i];
        const u32 b1 = i + 1 < n ? data[i + 1] : 0;
        const u32 b2 = i + 2 < n ? data[i + 2] : 0;
        out += t[b0 >> 2];
        out += t[((b0 & 3) << 4) | (b1 >> 4)];
        out += i + 1 < n ? t[((b1 & 15) << 2) | (b2 >> 6)] : '=';
        out += i + 2 < n ? t[b2 & 63] : '=';
    }
    return out;
}

std::vector<u8> triBuffer() {
    std::vector<u8> b(42, 0);
    const float pos[3][3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
    std::memcpy(b.data(), pos, 36);
    const u16 idx[3] = {0, 1, 2};
    std::memcpy(b.data() + 36, idx, 6);
    return b;
}

// o GLB completo (json + bin) — o mesmo empacotamento dos testes 5A/5C
std::vector<u8> makeGlb(const std::string& json,
                        const std::vector<u8>& bin) {
    std::vector<u8> jsonBytes(json.begin(), json.end());
    while (jsonBytes.size() % 4u != 0u) {
        jsonBytes.push_back(0x20);
    }
    std::vector<u8> b = bin;
    while (b.size() % 4u != 0u) {
        b.push_back(0);
    }
    const u32 total = 12u + 8u + static_cast<u32>(jsonBytes.size()) + 8u +
                      static_cast<u32>(b.size());
    std::vector<u8> glb;
    const auto u32le = [&glb](u32 v) {
        glb.push_back(static_cast<u8>(v));
        glb.push_back(static_cast<u8>(v >> 8));
        glb.push_back(static_cast<u8>(v >> 16));
        glb.push_back(static_cast<u8>(v >> 24));
    };
    glb.insert(glb.end(), {'g', 'l', 'T', 'F'});
    u32le(2);
    u32le(total);
    u32le(static_cast<u32>(jsonBytes.size()));
    glb.insert(glb.end(), {'J', 'S', 'O', 'N'});
    glb.insert(glb.end(), jsonBytes.begin(), jsonBytes.end());
    u32le(static_cast<u32>(b.size()));
    glb.insert(glb.end(), {'B', 'I', 'N', 0});
    glb.insert(glb.end(), b.begin(), b.end());
    return glb;
}

// nMats materiais; CADA um com baseColorTexture (imagem própria i) e o
// material 0 com normalTexture também (a imagem nMats — 2 slots nele);
// 1 mesh com nMats primitivas (a mesma geometria). `comPai`: o nó 0 tem
// MESH e translation [2,0,0] com o filho (translation [0.5,0.25,0]) — a
// sub-árvore REAL do e/f (2 peças, o filho pendurado no pai).
std::string gltfBinds(u32 nMats, bool comPai) {
    std::string mats, texs, prims;
    const u32 nImgs = nMats + 1;   // + a normal do material 0
    for (u32 i = 0; i < nImgs; ++i) {
        texs += (i ? "," : "") + std::string("{\"source\":") +
                std::to_string(i) + "}";
    }
    for (u32 i = 0; i < nMats; ++i) {
        mats += (i ? "," : "") + std::string("{\"name\":\"mat") +
                std::to_string(i) +
                "\",\"pbrMetallicRoughness\":{\"baseColorTexture\":"
                "{\"index\":" +
                std::to_string(i) + "}}";
        if (i == 0) {
            mats += ",\"normalTexture\":{\"index\":" +
                    std::to_string(nMats) + "}";
        }
        mats += "}";
        prims += (i ? "," : "") + std::string("{\"attributes\":"
                                              "{\"POSITION\":0},\"indices\":1,"
                                              "\"material\":") +
                 std::to_string(i) + "}";
    }
    const std::string nodes =
        comPai ? "[{\"name\":\"pai\",\"mesh\":0,"
                 "\"translation\":[2.0,0.0,0.0],\"children\":[1]},"
                 "{\"name\":\"filho\",\"mesh\":0,"
                 "\"translation\":[0.5,0.25,0.0]}]"
               : "[{\"name\":\"raiz\",\"mesh\":0}]";
    return std::string(
               "{\"asset\":{\"version\":\"2.0\"},"
               "\"buffers\":[{\"byteLength\":42}],"
               "\"bufferViews\":["
               "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
               "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
               "\"accessors\":["
               "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
               "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
               "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
               "\"type\":\"SCALAR\"}],"
               "\"materials\":[" +
               mats +
               "],"
               "\"textures\":[" +
               texs +
               "],"
               "\"images\":[],"
               "\"meshes\":[{\"name\":\"tri\",\"primitives\":[" +
               prims +
               "]}],"
               "\"nodes\":" +
               nodes +
               ",\"scenes\":[{\"nodes\":[0]}]}");
}

// 1 material com OS CINCO slots (5 imagens distintas) — a matriz do
// HOTFIX-2 (o bug do device: o parse só trazia baseColor+normal e os
// outros 3 slots nunca chegavam ao GTB1)
std::string gltf5slots() {
    const std::string mats =
        "{\"name\":\"mat0\",\"pbrMetallicRoughness\":{"
        "\"baseColorTexture\":{\"index\":0},"
        "\"metallicRoughnessTexture\":{\"index\":2}},"
        "\"normalTexture\":{\"index\":1},"
        "\"emissiveTexture\":{\"index\":3},"
        "\"occlusionTexture\":{\"index\":4}}";
    std::string texs;
    for (u32 i = 0; i < 5; ++i) {
        texs += (i ? "," : "") + std::string("{\"source\":") +
                std::to_string(i) + "}";
    }
    return std::string(
               "{\"asset\":{\"version\":\"2.0\"},"
               "\"buffers\":[{\"byteLength\":42}],"
               "\"bufferViews\":["
               "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
               "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
               "\"accessors\":["
               "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
               "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
               "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
               "\"type\":\"SCALAR\"}],"
               "\"materials\":[" +
               mats +
               "],"
               "\"textures\":[" +
               texs +
               "],"
               "\"images\":[],"
               "\"meshes\":[{\"name\":\"tri\",\"primitives\":["
               "{\"attributes\":{\"POSITION\":0},\"indices\":1,"
               "\"material\":0}]}],"
               "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
               "\"scenes\":[{\"nodes\":[0]}]}");
}

std::string gltfComImagens(const std::string& jsonNoImgs,
                           const std::vector<u8>& png, u32 nImgs) {
    const std::string b64 = b64encode(png.data(), png.size());
    std::string imgs;
    for (u32 i = 0; i < nImgs; ++i) {
        imgs += (i ? "," : "");
        imgs += "{\"uri\":\"data:image/png;base64," + b64 + "\"}";
    }
    const size_t pos = jsonNoImgs.find("\"images\":[]");
    return jsonNoImgs.substr(0, pos) + "\"images\":[" + imgs + "]" +
           jsonNoImgs.substr(pos + std::strlen("\"images\":[]"));
}

bool writeSrc(const std::string& path, const std::vector<u8>& bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return true;
}

u32 contaSlots(const std::vector<GMeshMatBind>& binds) {
    u32 n = 0;
    for (const GMeshMatBind& b : binds) {
        for (u8 s = 0; s < kBindSlotCount; ++s) {
            if (!b.tex[s].empty()) {
                ++n;
            }
        }
    }
    return n;
}

std::string slurp(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return {};
    }
    std::string s;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        s.append(buf, n);
    }
    std::fclose(f);
    return s;
}

MeshData boxMesh() {
    MeshData m;
    const float v[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1},
                           {-1, 1, -1},  {-1, -1, 1}, {1, -1, 1},
                           {1, 1, 1},    {-1, 1, 1}};
    for (int i = 0; i < 8; ++i) {
        Vertex vt;
        vt.pos = Vec3(v[i][0], v[i][1], v[i][2]);
        m.vertices.push_back(vt);
    }
    const u16 q[36] = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6,
                       0, 4, 5, 0, 5, 1, 2, 6, 7, 2, 7, 3,
                       0, 3, 7, 0, 7, 4, 1, 5, 6, 1, 6, 2};
    m.indices.assign(q, q + 36);
    return m;
}

} // namespace

// ---- o PIN do import: 2 slots (baseColor + normal) vinculados no GTB1 ------
TEST(bind_import_2_slots_e_log_por_slot) {
    const std::string base = std::string("/tmp/goni_tb1_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);
    const std::vector<u8> glb = makeGlb(
        gltfComImagens(gltfBinds(1, false), png, 2u), triBuffer());
    const std::string src = base + "/modelo.glb";
    ASSERT(writeSrc(src, glb));
    elog::init((base + "/logs").c_str());   // o engine.log que o pin lê
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::importFile(src, "modelo.glb", st, nullptr, out, stats,
                               err));
    elog::shutdown();
    EXPECT(stats.textures == 2u);   // baseColor + normal (a cura da causa (c))
    ASSERT(out.meshes.size() == 1u);
    // o GTB1 no ficheiro (o leitor dos materiais com binds)
    std::vector<u8> bytes;
    ASSERT(st.readBytes(out.meshes[0], bytes));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> names;
    std::vector<GMeshMatBind> binds;
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err, &binds));
    ASSERT(binds.size() == 1u);
    EXPECT(binds[0].materialIndex == 0u);
    EXPECT(binds[0].tex[kBindBaseColor] == "assets/modelo_0.gtext");
    EXPECT(binds[0].tex[kBindNormal] == "assets/modelo_1.gtext");
    EXPECT(contaSlots(binds) == 2u);
    // os ficheiros EXISTEM (o passe de texturas escreveu-os)
    std::vector<u8> t0;
    EXPECT(st.readBytes("assets/modelo_0.gtext", t0) && !t0.empty());
    // O PIN: UMA LINHA DE LOG POR SLOT VINCULADO
    const std::string log = slurp(base + "/logs/engine.log");
    EXPECT(log.find("textura: vinculada material 'mat0' slot baseColor -> "
                    "assets/modelo_0.gtext") != std::string::npos);
    EXPECT(log.find("slot normal") != std::string::npos);
}

// ---- o PIN dos 4 materiais: 4 bindings (nunca um material global) ----------
TEST(bind_4_materiais_4_bindings) {
    const std::string base = std::string("/tmp/goni_tb2_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);
    const std::vector<u8> glb = makeGlb(
        gltfComImagens(gltfBinds(4, false), png, 5u), triBuffer());
    const std::string src = base + "/modelo.glb";
    ASSERT(writeSrc(src, glb));
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::importFile(src, "modelo.glb", st, nullptr, out, stats,
                               err));
    ASSERT(out.meshes.size() == 1u);
    std::vector<u8> bytes;
    ASSERT(st.readBytes(out.meshes[0], bytes));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> names;
    std::vector<GMeshMatBind> binds;
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err, &binds));
    EXPECT(meta.materialCount == 4u);
    ASSERT(binds.size() == 4u);   // UM BINDING POR MATERIAL (o pin)
    for (u32 i = 0; i < 4; ++i) {
        EXPECT(binds[i].materialIndex == i);
        EXPECT(binds[i].tex[kBindBaseColor] ==
               "assets/modelo_" + std::to_string(i) + ".gtext");
    }
}

// ---- o PIN sintético: 50 texturas = 50 vinculadas (round-trip do bloco) ----
TEST(bind_50_sinteticas_roundtrip) {
    std::vector<GMeshMatBind> binds;
    for (u32 m = 0; m < 10; ++m) {
        GMeshMatBind b;
        b.materialIndex = m;
        for (u8 s = 0; s < kBindSlotCount; ++s) {
            b.tex[s] = "assets/sintetico_" + std::to_string(m) + "_" +
                       std::to_string(s) + ".gtext";
        }
        binds.push_back(b);
    }
    std::vector<u8> section;
    gmeshV3AppendBinds(section, 0, binds);
    EXPECT(section.size() > 12u);   // magic + count + entradas + CRC
    const u8 magic[4] = {'G', 'T', 'B', '1'};
    EXPECT(std::memcmp(section.data(), magic, 4) == 0);
    std::vector<GMeshMatBind> back;
    std::string err;
    ASSERT(gmeshV3ReadBinds(section.data(), section.size(), back, err));
    ASSERT(back.size() == 10u);
    EXPECT(contaSlots(back) == 50u);   // O PIN: 50 vinculadas
    for (u32 m = 0; m < 10; ++m) {
        for (u8 s = 0; s < kBindSlotCount; ++s) {
            EXPECT(back[m].tex[s] == binds[m].tex[s]);
        }
    }
}

// ---- retro-compat: SEM GTB1 abre igual (o leitor de sempre para nos nomes)
TEST(bind_retrocompativel_sem_gtb1) {
    MeshData m = boxMesh();
    m.groups.push_back(MeshData::Group{"g0", "matA", 0,
                                       static_cast<u32>(m.indices.size())});
    std::vector<u8> bytes;
    std::string err;
    ASSERT(writeGMesh(m, bytes, err));   // SEM binds — o pré-hotfix
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> names;
    std::vector<GMeshMatBind> binds;
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err, &binds));
    EXPECT(binds.empty());   // sem bloco = vazio, NÃO é erro
    ASSERT(names.size() == 1u);
    EXPECT(names[0] == "matA");
    // e com binds o MESMO mesh leva o bloco e abre
    std::vector<GMeshMatBind> want(1);
    want[0].materialIndex = 0;
    want[0].tex[kBindBaseColor] = "assets/a.gtext";
    ASSERT(writeGMesh(m, bytes, err, &want));
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err, &binds));
    ASSERT(binds.size() == 1u);
    EXPECT(binds[0].tex[kBindBaseColor] == "assets/a.gtext");
    // o CRC mau = false com err LEGÍVEL (a quarentena do chamador decide)
    bytes[bytes.size() - 2] ^= 0xFF;
    EXPECT(!readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                            err, &binds));
    EXPECT(err.find("CHECKSUM CORROMPIDO") != std::string::npos);
}

// ---- o PIN da auditoria: atribuir a um slot NÃO limpa os irmãos ------------
TEST(audit_pick_escreve_o_slot_e_irmaos_intactos) {
    Scene scene;
    const Handle h = scene.create("tic");
    MeshRenderer* mr = scene.get(h)->addComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    mr->texPath = "assets/antiga.gtext";
    editor::AssetCatalog cat;
    cat.textures.push_back("assets/nova.gtext");
    editor::AssetResolvers res;
    static Texture dummy;
    res.texture = [](const std::string&, std::string*) -> const Texture* {
        return &dummy;
    };
    // o slot 1 (normal): o texPath (baseColor) fica INTACTO
    editor::AssetPickOutcome o1 =
        editor::applyAssetPick(scene, h, 2, 2, cat, res, 1);
    ASSERT(o1.applied);
    EXPECT(mr->texNormal == "assets/nova.gtext");
    EXPECT(mr->texPath == "assets/antiga.gtext");
    // o slot 2: o 1 e o 0 ficam intactos
    editor::AssetPickOutcome o2 =
        editor::applyAssetPick(scene, h, 2, 2, cat, res, 2);
    ASSERT(o2.applied);
    EXPECT(mr->texMetalRough == "assets/nova.gtext");
    EXPECT(mr->texNormal == "assets/nova.gtext");
    EXPECT(mr->texPath == "assets/antiga.gtext");
    // o slot 0: o baseColor troca (o caminho de sempre) — os irmãos ficam
    editor::AssetPickOutcome o0 =
        editor::applyAssetPick(scene, h, 2, 2, cat, res, 0);
    ASSERT(o0.applied);
    EXPECT(mr->texPath == "assets/nova.gtext");
    EXPECT(mr->texNormal == "assets/nova.gtext");
    // o none no slot 1 limpa SÓ o slot 1
    editor::AssetPickOutcome o3 =
        editor::applyAssetPick(scene, h, 2, 1, cat, res, 1);
    ASSERT(o3.applied);
    EXPECT(mr->texNormal.empty());
    EXPECT(mr->texPath == "assets/nova.gtext");
    EXPECT(mr->texMetalRough == "assets/nova.gtext");
}

// ---- o PIN (e/f): sub-árvore REAL — parentRec + TRS local ------------------
TEST(subarvore_real_pai_local_e_mundo_composto) {
    const std::string base = std::string("/tmp/goni_tb3_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);
    const std::vector<u8> glb = makeGlb(
        gltfComImagens(gltfBinds(1, true), png, 2u), triBuffer());
    const std::string src = base + "/modelo.glb";
    ASSERT(writeSrc(src, glb));
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::importFile(src, "modelo.glb", st, nullptr, out, stats,
                               err, nullptr, nullptr, /*expandNodes=*/true));
    ASSERT(out.expandNodes.size() == 2u);   // pai + filho (ambos com mesh)
    // o filho (registo 1) pende do pai (registo 0) com o TRS LOCAL
    EXPECT(out.expandNodes[1].parentRec == 0);
    EXPECT(out.expandNodes[1].translation.x == 0.5f);
    EXPECT(out.expandNodes[1].translation.y == 0.25f);
    // o pai (a raiz) fica com o TRS MUNDO (o flat de sempre)
    EXPECT(out.expandNodes[0].parentRec == -1);
    EXPECT(out.expandNodes[0].translation.x == 2.0f);
    // o TIC filho tem o TIC pai e o mundo compõe parent∘local
    Scene scene;
    GltfInstantiateCtx ctx;   // bindMesh null — o meshPath fica (o reload)
    const std::vector<Handle> created =
        gltfExpandInstantiate(scene, out.expandNodes, out.meshes, ctx);
    ASSERT(created.size() == 2u);
    const Tic* tp = scene.get(created[0]);
    const Tic* tf = scene.get(created[1]);
    ASSERT(tp != nullptr && tf != nullptr);
    EXPECT(tf->parent == static_cast<i32>(created[0].index));
    const Transform3D* trp = tp->getComponent<Transform3D>();
    const Transform3D* trf = tf->getComponent<Transform3D>();
    ASSERT(trp != nullptr && trf != nullptr);
    EXPECT(trf->world.m[12] == 2.5f);   // 2.0 (pai) + 0.5 (filho local)
    EXPECT(trf->world.m[13] == 0.25f);
    EXPECT(trp->world.m[12] == 2.0f);
}

// ---- HOTFIX-2 · o PIN da matriz: import com OS 5 SLOTS do glTF -----------
// (o bug do device: «4 .gtext no disco mas só o baseColor preenchido» —
// o parse só lia baseColor+normal; a cura lê os 5 e o GTB1 leva-os TODOS
// + 1 linha de log por slot + o matBindsOf REAL devolve os 5)
TEST(bind2_import_5_slots_matriz_log_e_matbindsof) {
    const std::string base = std::string("/tmp/goni_tb4_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);
    const std::vector<u8> glb = makeGlb(
        gltfComImagens(gltf5slots(), png, 5u), triBuffer());
    const std::string src = base + "/modelo.glb";
    ASSERT(writeSrc(src, glb));
    elog::init((base + "/logs").c_str());   // o engine.log que o pin lê
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::importFile(src, "modelo.glb", st, nullptr, out, stats,
                               err));
    elog::shutdown();
    EXPECT(stats.textures == 5u);   // OS CINCO .gtext (o pin do dono)
    ASSERT(out.meshes.size() == 1u);
    std::vector<u8> bytes;
    ASSERT(st.readBytes(out.meshes[0], bytes));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> names;
    std::vector<GMeshMatBind> binds;
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err, &binds));
    ASSERT(binds.size() == 1u);
    EXPECT(binds[0].materialIndex == 0u);
    // OS 5 SLOTS no GTB1 (a cura da causa (a)): cada slot com o SEU caminho
    for (u8 s = 0; s < kBindSlotCount; ++s) {
        EXPECT(binds[0].tex[s] == "assets/modelo_" +
                                      std::to_string(static_cast<int>(s)) +
                                      ".gtext");
    }
    EXPECT(contaSlots(binds) == 5u);
    // os 5 ficheiros EXISTEM (o passe de texturas escreveu-os)
    for (u8 s = 0; s < kBindSlotCount; ++s) {
        std::vector<u8> tb;
        EXPECT(st.readBytes("assets/modelo_" +
                                std::to_string(static_cast<int>(s)) +
                                ".gtext",
                            tb) && !tb.empty());
    }
    // O PIN: UMA LINHA DE LOG POR SLOT (slot+nome) — 5 linhas
    const std::string log = slurp(base + "/logs/engine.log");
    for (u8 s = 0; s < kBindSlotCount; ++s) {
        const std::string want =
            std::string("textura: vinculada material 'mat0' slot ") +
            kBindSlotName(s) + " -> assets/modelo_" +
            std::to_string(static_cast<int>(s)) + ".gtext";
        EXPECT(log.find(want) != std::string::npos);
    }
    // A CURA (b) no RUNTIME: o matBindsOf REAL sobre o storage — os 5
    // slots saem do MESMO bloco (o auto-bind do apply enche-os todos)
    ResourceManager rm;
    rm.setStorage(&st);
    GpuAssets gpu;
    gpu.init(&rm);
    std::string rb[kBindSlotCount];
    ASSERT(gpu.matBindsOf(out.meshes[0], 0, rb));
    for (u8 s = 0; s < kBindSlotCount; ++s) {
        EXPECT(rb[s] == binds[0].tex[s]);
    }
    EXPECT(!gpu.matBindsOf(out.meshes[0], 1, rb));   // material 1 não existe
    EXPECT(gpu.baseColorBindOf(out.meshes[0], 0) ==
           "assets/modelo_0.gtext");   // o wrapper de sempre (contrato)
}

// ---- HOTFIX-2 · o PIN do auto-bind: o apply enche OS 5 SLOTS do TIC ------
// (a cura (b): o baseColorBindOf só devolvia o baseColor — os outros 4
// slots do Inspector ficavam vazios mesmo com o GTB1 completo). E a troca
// de mesh leva os slots ATRÁS (o vestígio do mesh anterior morre).
TEST(bind2_autobind_5_slots_no_apply_e_troca_de_mesh) {
    Scene scene;
    const Handle h = scene.create("tic");
    MeshRenderer* mr = scene.get(h)->addComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    mr->texPath = "assets/velha.gtext";       // do mesh anterior
    mr->texNormal = "assets/vestigio.gtext";  // idem
    editor::AssetCatalog cat;
    cat.meshes = {"assets/m/m.gmesh", "assets/o/o.gmesh", "assets/s/s.gmesh"};
    editor::AssetResolvers res;
    static Texture dummy;
    static Mesh* const kMeshSent = reinterpret_cast<Mesh*>(0x21);
    res.mesh = [](const std::string&) -> Mesh* { return kMeshSent; };
    res.texture = [](const std::string&, std::string*) -> const Texture* {
        return &dummy;
    };
    res.matBinds = [](const std::string& ref, u32,
                      std::string (&out)[MeshRenderer::kMeshTexSlots]) -> bool {
        for (u32 s = 0; s < MeshRenderer::kMeshTexSlots; ++s) {
            out[s].clear();
        }
        if (ref == "assets/m/m.gmesh") {
            out[0] = "assets/m_0.gtext"; out[1] = "assets/m_1.gtext";
            out[2] = "assets/m_2.gtext"; out[3] = "assets/m_3.gtext";
            out[4] = "assets/m_4.gtext";
            return true;
        }
        if (ref == "assets/o/o.gmesh") {
            out[0] = "assets/o_0.gtext"; out[1] = "assets/o_1.gtext";
            return true;
        }
        return false;   // o s.gmesh não tem GTB1
    };
    // o apply do m.gmesh (pick 3 = ficheiro 0): OS 5 SLOTS preenchidos
    const editor::AssetPickOutcome o1 =
        editor::applyAssetPick(scene, h, 1, 3, cat, res, 0);
    ASSERT(o1.applied);
    EXPECT(mr->meshPath == "assets/m/m.gmesh");
    EXPECT(mr->texPath == "assets/m_0.gtext");   // slot 0 + GL carregado
    EXPECT(mr->texture == &dummy);
    EXPECT(mr->texNormal == "assets/m_1.gtext");
    EXPECT(mr->texMetalRough == "assets/m_2.gtext");
    EXPECT(mr->texEmissive == "assets/m_3.gtext");
    EXPECT(mr->texOcclusion == "assets/m_4.gtext");
    // a troca para o o.gmesh: os slots SEGUEM O MESH (o vestígio morre)
    const editor::AssetPickOutcome o2 =
        editor::applyAssetPick(scene, h, 1, 4, cat, res, 0);
    ASSERT(o2.applied);
    EXPECT(mr->texPath == "assets/o_0.gtext");
    EXPECT(mr->texNormal == "assets/o_1.gtext");
    EXPECT(mr->texMetalRough.empty());   // o m_2 NÃO sobrevive à troca
    EXPECT(mr->texEmissive.empty());
    EXPECT(mr->texOcclusion.empty());
    // mesh SEM GTB1: os slots ficam LIMPOS (nenhum vestígio falso)
    const editor::AssetPickOutcome o3 =
        editor::applyAssetPick(scene, h, 1, 5, cat, res, 0);
    ASSERT(o3.applied);
    EXPECT(mr->texPath.empty());
    EXPECT(mr->texNormal.empty());
    EXPECT(mr->texEmissive.empty());
}

// ---- MULTIMAT · nMats materiais, CADA um com OS CINCO slots ---------------
// (a matriz do dono: 3 materiais × 5 slots = 15 texturas; material i →
// imagens i*5+0 .. i*5+4 — o naming do convert é assets/<stem>_<img>.gtext)
std::string gltfMultimat(u32 nMats) {
    std::string mats, texs, prims;
    const u32 nImgs = nMats * 5;
    for (u32 i = 0; i < nImgs; ++i) {
        texs += (i ? "," : "") + std::string("{\"source\":") +
                std::to_string(i) + "}";
    }
    for (u32 m = 0; m < nMats; ++m) {
        mats += (m ? "," : "") + std::string("{\"name\":\"mat") +
                std::to_string(m) + "\",\"pbrMetallicRoughness\":{"
                "\"baseColorTexture\":{\"index\":" +
                std::to_string(m * 5 + 0) +
                "},\"metallicRoughnessTexture\":{\"index\":" +
                std::to_string(m * 5 + 2) +
                "}},\"normalTexture\":{\"index\":" +
                std::to_string(m * 5 + 1) +
                "},\"emissiveTexture\":{\"index\":" +
                std::to_string(m * 5 + 3) +
                "},\"occlusionTexture\":{\"index\":" +
                std::to_string(m * 5 + 4) + "}}";
        prims += (m ? "," : "") + std::string("{\"attributes\":"
                                              "{\"POSITION\":0},\"indices\":1,"
                                              "\"material\":") +
                 std::to_string(m) + "}";
    }
    return std::string(
               "{\"asset\":{\"version\":\"2.0\"},"
               "\"buffers\":[{\"byteLength\":42}],"
               "\"bufferViews\":["
               "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
               "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
               "\"accessors\":["
               "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
               "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
               "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
               "\"type\":\"SCALAR\"}],"
               "\"materials\":[" +
               mats +
               "],"
               "\"textures\":[" +
               texs +
               "],"
               "\"images\":[],"
               "\"meshes\":[{\"name\":\"tri\",\"primitives\":[" +
               prims +
               "]}],"
               "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
               "\"scenes\":[{\"nodes\":[0]}]}");
}

// ---- MULTIMAT · o PIN do dono: 3 materiais → o seletor mostra 3, cada um
// com os seus 5 slots (o import fundido + o auto-bind da grade INTEIRA) ----
TEST(multimat_3_materiais_import_seletor_e_slots) {
    const std::string base = std::string("/tmp/goni_tb5_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);
    const std::vector<u8> glb = makeGlb(
        gltfComImagens(gltfMultimat(3), png, 15u), triBuffer());
    const std::string src = base + "/modelo.glb";
    ASSERT(writeSrc(src, glb));
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::importFile(src, "modelo.glb", st, nullptr, out, stats,
                               err));
    EXPECT(stats.textures == 15u);   // 3 materiais × 5 slots (o pin do dono)
    ASSERT(out.meshes.size() == 1u);
    // o GTB1 no ficheiro: 3 bindings, 15 slots
    std::vector<u8> bytes;
    ASSERT(st.readBytes(out.meshes[0], bytes));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> names;
    std::vector<GMeshMatBind> binds;
    ASSERT(readGMeshV3Meta(bytes.data(), bytes.size(), meta, blocks, names,
                           err, &binds));
    EXPECT(meta.materialCount == 3u);
    ASSERT(binds.size() == 3u);
    EXPECT(contaSlots(binds) == 15u);
    // o apply do mesh com o matBindsOf REAL (o auto-bind da grade inteira)
    ResourceManager rm;
    rm.setStorage(&st);
    GpuAssets gpu;
    gpu.init(&rm);
    static GpuAssets* const sGpu = &gpu;   // o REAL (a lambda é sem captura)
    Scene scene;
    const Handle h = scene.create("tic");
    MeshRenderer* mr = scene.get(h)->addComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    editor::AssetCatalog cat;
    cat.meshes = {out.meshes[0]};
    editor::AssetResolvers res;
    static Texture dummy;
    static Mesh* const kMeshSent = reinterpret_cast<Mesh*>(0x41);
    res.mesh = [](const std::string&) -> Mesh* { return kMeshSent; };
    res.texture = [](const std::string&, std::string*) -> const Texture* {
        return &dummy;
    };
    res.matBinds = [](const std::string& ref, u32 mi,
                      std::string (&o)[MeshRenderer::kMeshTexSlots]) {
        return sGpu->matBindsOf(ref, mi, o);
    };
    const editor::AssetPickOutcome o1 =
        editor::applyAssetPick(scene, h, 1, 3, cat, res, 0);
    ASSERT(o1.applied);
    EXPECT(mr->meshPath == out.meshes[0]);
    // O PIN: 3 MATERIAIS na grade, cada um com os seus 5 slots do GTB1
    EXPECT(mr->matCount() == 3u);
    for (u32 m = 0; m < 3; ++m) {
        for (u32 s = 0; s < MeshRenderer::kMeshTexSlots; ++s) {
            const std::string want =
                "assets/modelo_" + std::to_string(m * 5 + static_cast<int>(s)) +
                ".gtext";
            EXPECT(mr->matTex(m, s) == want);
        }
    }
    // O SELETOR: o perfil diz 3 e o plano ganha a linha «material: i/N»
    const Tic* tic = scene.get(h);
    const TextMetrics tm{20.0f, 6.0f};
    const editor::InspProfile prof = editor::inspectorProfile(*tic);
    EXPECT(prof.matCount == 3u);
    editor::InspRow plan[64];
    const u32 n = editor::inspectorPlan(prof, tm, true, 0u, plan);
    u32 nSel = 0, nSlots = 0;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == editor::InspRow::Kind::MatSelRow) {
            ++nSel;
        }
        if (plan[i].kind == editor::InspRow::Kind::TexSlotRow) {
            ++nSlots;
        }
    }
    EXPECT(nSel == 1u);    // O SELETOR existe (N>1)
    EXPECT(nSlots == 5u);  // as 5 linhas de slot continuam 5
    // mesh de 1 material: SEM a linha do seletor (o comportamento de sempre)
    editor::AssetCatalog cat2;
    cat2.meshes = {"assets/o/o.gmesh"};
    editor::AssetResolvers res2;
    res2.mesh = res.mesh;
    res2.texture = res.texture;
    res2.matBinds = [](const std::string&, u32,
                       std::string (&o)[MeshRenderer::kMeshTexSlots]) {
        for (u32 s = 0; s < MeshRenderer::kMeshTexSlots; ++s) {
            o[s].clear();
        }
        return false;   // sem GTB1 — 1 material
    };
    const editor::AssetPickOutcome o2 =
        editor::applyAssetPick(scene, h, 1, 3, cat2, res2, 0);
    ASSERT(o2.applied);
    EXPECT(mr->matCount() == 1u);   // a grade morre com o mesh (o vestígio)
    const editor::InspProfile prof2 = editor::inspectorProfile(*scene.get(h));
    EXPECT(prof2.matCount == 1u);
    editor::InspRow plan2[64];
    const u32 n2 = editor::inspectorPlan(prof2, tm, true, 0u, plan2);
    u32 nSel2 = 0;
    for (u32 i = 0; i < n2; ++i) {
        if (plan2[i].kind == editor::InspRow::Kind::MatSelRow) {
            ++nSel2;
        }
    }
    EXPECT(nSel2 == 0u);   // N=1 → sem seletor
}

// ---- MULTIMAT · o PIN do dono: o pick no MATERIAL 2 não toca no 0 ---------
TEST(multimat_pick_no_material_2_nao_toca_no_0) {
    Scene scene;
    const Handle h = scene.create("tic");
    MeshRenderer* mr = scene.get(h)->addComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    mr->meshPath = "assets/m/m.gmesh";
    mr->texPath = "assets/m0_0.gtext";
    mr->texNormal = "assets/m0_1.gtext";
    mr->texMetalRough = "assets/m0_2.gtext";
    mr->texEmissive = "assets/m0_3.gtext";
    mr->texOcclusion = "assets/m0_4.gtext";
    mr->setMatCount(3);
    for (u32 m = 1; m < 3; ++m) {
        for (u32 s = 0; s < MeshRenderer::kMeshTexSlots; ++s) {
            *mr->matTexRef(m, s) = "assets/m" + std::to_string(m) + "_" +
                                   std::to_string(s) + ".gtext";
        }
    }
    editor::AssetCatalog cat;
    cat.textures = {"assets/nova.gtext"};
    editor::AssetResolvers res;
    static Texture dummy;
    res.texture = [](const std::string&, std::string*) -> const Texture* {
        return &dummy;
    };
    // o pick no MATERIAL 2, slot emissive (3): o 2 muda, o 0 INTACTO
    const editor::AssetPickOutcome o1 =
        editor::applyAssetPick(scene, h, 2, 2, cat, res, 3, 2);
    ASSERT(o1.applied);
    EXPECT(mr->matTex(2, 3) == "assets/nova.gtext");
    EXPECT(mr->matTex(2, 0) == "assets/m2_0.gtext");   // o base do 2 intacto
    EXPECT(mr->texPath == "assets/m0_0.gtext");        // o 0 INTACTO (o pin)
    EXPECT(mr->texNormal == "assets/m0_1.gtext");
    EXPECT(mr->texEmissive == "assets/m0_3.gtext");
    // o GL pointer NÃO é tocado num material >0 (o baseColor do 0 é o de
    // sempre — o lit de hoje amostra-o; o pick multimat não mente)
    EXPECT(mr->texture == nullptr);
    // a linha de log diz O MATERIAL (o dono lê: material 2 slot emissive)
    EXPECT(std::strstr(o1.log, "material 2 slot emissive") != nullptr);
    // o none no MATERIAL 1 slot normal: limpa SÓ esse (os outros 14 ficam)
    const editor::AssetPickOutcome o2 =
        editor::applyAssetPick(scene, h, 2, 1, cat, res, 1, 1);
    ASSERT(o2.applied);
    EXPECT(mr->matTex(1, 1).empty());
    EXPECT(mr->matTex(1, 0) == "assets/m1_0.gtext");
    EXPECT(mr->matTex(0, 1) == "assets/m0_1.gtext");
    EXPECT(mr->matTex(2, 3) == "assets/nova.gtext");
    // o clamp honesto: materialIndex 7 numa grade de 3 → escreve no ÚLTIMO
    const editor::AssetPickOutcome o3 =
        editor::applyAssetPick(scene, h, 2, 2, cat, res, 4, 7);
    ASSERT(o3.applied);
    EXPECT(mr->matTex(2, 4) == "assets/nova.gtext");
    // mesh de 1 material: o materialIndex é clamp a 0 (o caminho de sempre)
    mr->setMatCount(1);
    const editor::AssetPickOutcome o4 =
        editor::applyAssetPick(scene, h, 2, 2, cat, res, 1, 2);
    ASSERT(o4.applied);
    EXPECT(mr->texNormal == "assets/nova.gtext");   // slot 1 DO material 0
}

// ---- MULTIMAT · o PIN da HONESTIDADE: o sufixo «vinculado · render na
// 0.11-L» nos slots não-baseColor preenchidos; o baseColor SEM sufixo ------
TEST(multimat_sufixo_honesto_no_rotulo) {
    char b[160];
    // o baseColor (slot 0) RENDERIZA — SEM sufixo nunca
    editor::inspTexSlotLabel(b, sizeof(b), "baseColor", 0, "modelo_0.gtext");
    EXPECT(std::strcmp(b, "baseColor: modelo_0.gtext") == 0);
    // o normal preenchido NÃO renderiza hoje — o SUFIXO HONESTO (o pin)
    editor::inspTexSlotLabel(b, sizeof(b), "normal", 1, "modelo_1.gtext");
    EXPECT(std::strstr(b, "normal: modelo_1.gtext ") == b);
    EXPECT(std::strstr(b, "(vinculado \xc2\xb7 render na 0.11-L)") !=
           nullptr);
    // o emissive idem (o 3) — e o VAZIO não leva sufixo (nada a esconder)
    editor::inspTexSlotLabel(b, sizeof(b), "emissive", 3, "modelo_3.gtext");
    EXPECT(std::strstr(b, "(vinculado \xc2\xb7 render na 0.11-L)") !=
           nullptr);
    editor::inspTexSlotLabel(b, sizeof(b), "normal", 1, "");
    EXPECT(std::strcmp(b, "normal: —") == 0);
    // a fronteira é a FUNÇÃO (o lit de hoje: só o 0; a 0.11-L traz os
    // samplers 1-4 — e o sufixo morre com eles)
    EXPECT(editor::texSlotRenders(0));
    EXPECT(!editor::texSlotRenders(1));
    EXPECT(!editor::texSlotRenders(2));
    EXPECT(!editor::texSlotRenders(3));
    EXPECT(!editor::texSlotRenders(4));
}

// ---- P0 · o PIN do CICLO de pais (a cura (b) — o walk SEM visitados
// girava para sempre num glTF malformado; o mutante M-P0a HANGA aqui) ------
TEST(expand_p0_ciclo_de_pais_termina_e_diz) {
    const std::string base = std::string("/tmp/goni_tb6_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    // DOIS nós com mesh e pais em CICLO (0→1→0 pelos children): o parse
    // produz nodes[0].parent=1 e nodes[1].parent=0 — o walk do ancestral/
    // da cadeia/mundo tinha de TERMINAR (o mutante sem visitados nunca
    // volta — o timeout do ctest é o vermelho)
    const std::string json =
        std::string("{\"asset\":{\"version\":\"2.0\"},"
                    "\"buffers\":[{\"byteLength\":42}],"
                    "\"bufferViews\":["
                    "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                    "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
                    "\"accessors\":["
                    "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                    "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
                    "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
                    "\"type\":\"SCALAR\"}],"
                    "\"materials\":[{\"name\":\"m\"}],"
                    "\"meshes\":[{\"name\":\"tri\",\"primitives\":["
                    "{\"attributes\":{\"POSITION\":0},\"indices\":1,"
                    "\"material\":0}]}],"
                    "\"nodes\":["
                    "{\"name\":\"a\",\"mesh\":0,\"children\":[1]},"
                    "{\"name\":\"b\",\"mesh\":0,\"children\":[0]}],"
                    "\"scenes\":[{\"nodes\":[0]}]}");
    const std::vector<u8> glb = makeGlb(json, triBuffer());
    const std::string src = base + "/modelo.glb";
    ASSERT(writeSrc(src, glb));
    elog::init((base + "/logs").c_str());
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    const bool ok =
        convert::importFile(src, "modelo.glb", st, nullptr, out, stats,
                            err, nullptr, nullptr, /*expandNodes=*/true);
    elog::shutdown();
    EXPECT(ok);   // TERMINA (o pin é o próprio retorno — o mutante hanga)
    EXPECT(out.expandNodes.size() == 2u);   // as 2 peças saíram
    // o dono LÊ a causa (o conjunto de visitados cortou e disse)
    const std::string log = slurp(base + "/logs/engine.log");
    EXPECT(log.find("ciclo nos nós do glTF") != std::string::npos);
}

// ---- P0 · o PIN do LIMITE declarado (a cura (c): 2100 nós com mesh →
// TICs ≤ kGmeshV3ExpandMaxTics + o aviso claro dos que ficaram fora) -----
TEST(expand_p0_limite_de_pecas_e_aviso) {
    const std::string base = std::string("/tmp/goni_tb7_") +
                             std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    // 2100 nós COM mesh (todos referem o mesmo triângulo — peças de 1
    // primitiva sem texturas, o caminho mais barato do conversor)
    const u32 kNodes = 2100;
    std::string nodes;
    for (u32 i = 0; i < kNodes; ++i) {
        nodes += (i ? "," : "") + std::string("{\"name\":\"n") +
                 std::to_string(i) + "\",\"mesh\":0}";
    }
    const std::string json =
        std::string("{\"asset\":{\"version\":\"2.0\"},"
                    "\"buffers\":[{\"byteLength\":42}],"
                    "\"bufferViews\":["
                    "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
                    "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
                    "\"accessors\":["
                    "{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
                    "\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
                    "{\"bufferView\":1,\"componentType\":5123,\"count\":3,"
                    "\"type\":\"SCALAR\"}],"
                    "\"materials\":[{\"name\":\"m\"}],"
                    "\"meshes\":[{\"name\":\"tri\",\"primitives\":["
                    "{\"attributes\":{\"POSITION\":0},\"indices\":1,"
                    "\"material\":0}]}],"
                    "\"nodes\":[" +
                    nodes +
                    "],\"scenes\":[{\"nodes\":[0]}]}");
    const std::vector<u8> glb = makeGlb(json, triBuffer());
    const std::string src = base + "/modelo.glb";
    ASSERT(writeSrc(src, glb));
    elog::init((base + "/logs").c_str());
    FsStorage st((base + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    ASSERT(convert::importFile(src, "modelo.glb", st, nullptr, out, stats,
                               err, nullptr, nullptr, /*expandNodes=*/true));
    elog::shutdown();
    // O PIN: os TICs NÃO passam do limite declarado (a explosão morre)
    EXPECT(out.expandNodes.size() ==
           static_cast<size_t>(convert::kGmeshV3ExpandMaxTics));
    // o resto dos nós FICOU DE FORA com o aviso claro (o nº no stats)
    EXPECT(stats.expandLimited == kNodes - convert::kGmeshV3ExpandMaxTics);
    const std::string log = slurp(base + "/logs/engine.log");
    EXPECT(log.find("LIMITE de 2000") != std::string::npos);
}

// ---- P0 · o PIN do LOTE (a cura (a): o tick configura ≤
// kGltfExpandBatchTics peças — a expansão NÃO corre tudo na thread de UI
// num só frame; o progresso done/total avança; o cancelamento LIMPA) ----
TEST(expand_p0_lote_progresso_e_cancelamento_limpo) {
    Scene scene;
    // 100 registos COM peça (a ref é a "assets/x/x.gmesh" — o bindMesh null
    // do ctx deixa o meshPath, o reload re-liga; o pin conta o lote)
    std::vector<convert::Output::NodeOut> nodes;
    std::vector<std::string> refs = {"assets/x/x.gmesh"};
    for (u32 i = 0; i < 100; ++i) {
        convert::Output::NodeOut no;
        no.name = "p" + std::to_string(i);
        no.node = static_cast<i32>(i);
        no.mesh = 0;
        nodes.push_back(no);
    }
    GltfInstantiateCtx ctx;   // bindMesh null — o meshPath fica
    GltfExpandBatch job = GltfExpandBatch::start(scene, nodes, refs, ctx);
    EXPECT(job.total() == 100u);
    EXPECT(job.done() == 0u);
    EXPECT(job.active());
    // o gancho por peça conta (o main injeta o auto-bind; o pin prova que
    // corre DENTRO do lote)
    u32 perPieceCalls = 0;
    job.perPiece = [](Scene&, Handle, void* user) {
        ++*static_cast<u32*>(user);
    };
    job.perPieceUser = &perPieceCalls;
    // A LEI DO LOTE: 1 tick = NO MÁXIMO kGltfExpandBatchTics peças — a
    // mutação M-P0b (tudo num tick = a expansão na thread de UI) morre
    // NESTE assert
    EXPECT(!job.tick());
    EXPECT(job.done() == 32u);   // ← o mutante faz 100 aqui (vermelho)
    EXPECT(perPieceCalls == 32u);
    EXPECT(scene.count() == 100u);   // os TICs existem (a 1.ª passada)
    EXPECT(!job.tick());
    EXPECT(job.done() == 64u);
    EXPECT(!job.tick());
    EXPECT(job.done() == 96u);
    EXPECT(job.tick());   // a última: 4 peças + fim
    EXPECT(job.done() == 100u);
    EXPECT(!job.active());
    EXPECT(scene.count() == 100u);
    // o CANCELAMENTO LIMPO (o pin do dono): os TICs já nascidos MORREM —
    // a cena fica COMO ESTAVA antes do expand
    GltfExpandBatch job2 = GltfExpandBatch::start(scene, nodes, refs, ctx);
    EXPECT(!job2.tick());
    EXPECT(scene.count() == 200u);   // +100 do 2.º job (32 configurados)
    job2.cancel();
    EXPECT(job2.canceled);
    EXPECT(scene.count() == 100u);   // os 100 do 1.º job INTACTOS
    // e o one-shot é o MESMO estado final (o wrapper do lote — a fonte
    // única; a sub-árvore real dos pins de cima passou por AQUI)
    Scene scene2;
    const std::vector<Handle> created =
        gltfExpandInstantiate(scene2, nodes, refs, ctx);
    EXPECT(created.size() == 100u);
    EXPECT(scene2.count() == 100u);
    const Tic* t0 = scene2.get(created[0]);
    ASSERT(t0 != nullptr);
    EXPECT(t0->getComponent<MeshRenderer>() != nullptr);
    EXPECT(t0->getComponent<MeshRenderer>()->meshPath == "assets/x/x.gmesh");
}

// ---- a composição de pais no TransformSystem (a dívida do polish paga) -----
TEST(transform_system_compoe_pais) {
    Scene scene;
    const Handle pai = scene.create("pai");
    const Handle filho = scene.create("filho", static_cast<i32>(pai.index));
    Transform3D* tp = scene.get(pai)->addComponent<Transform3D>();
    Transform3D* tf = scene.get(filho)->addComponent<Transform3D>();
    ASSERT(tp != nullptr && tf != nullptr);
    tp->pos = Vec3{2.0f, 0.0f, 0.0f};
    tp->rot = Quat::identity();
    tf->pos = Vec3{0.5f, 0.25f, 0.0f};
    tf->rot = Quat::identity();
    TransformSystem sys;
    sys.tick(scene, 0.016f);
    // o filho: world = parentWorld ∘ local (a MESMA matemática do glTF)
    const Mat4 want = Mat4::mul(tp->computeMatrix(), tf->computeMatrix());
    for (int i = 0; i < 16; ++i) {
        EXPECT(std::fabs(tf->world.m[i] - want.m[i]) < 1e-5f);
    }
    EXPECT(tf->world.m[12] == 2.5f);
    EXPECT(tf->world.m[13] == 0.25f);
    // a raiz: o local de sempre (byte a byte)
    const Mat4 local = tp->computeMatrix();
    for (int i = 0; i < 16; ++i) {
        EXPECT(tp->world.m[i] == local.m[i]);
    }
}
