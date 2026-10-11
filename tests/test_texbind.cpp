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
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/FsStorage.h"
#include "core/Scene.h"
#include "core/TransformSystem.h"
#include "platform/EngineLog.h"   // elog::init — o log que os pins leem
#include "render/Texture.h"
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
