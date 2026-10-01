// tests/test_skin.cpp — 0.8.2 (F7): SKINNING ESQUELÉTICO.
//
// Aferição da spec 0.8.2:
//   • import de skins de glTF: joints, inverseBindMatrices, weights por
//     vértice (parser + attach → SkeletonComp);
//   • SHADER COM BONES: as matrizes de skin chegam ao shader (uBones
//     array + uSkin) e o MESH SKINADO DESENHA (verificado no stub de
//     render — drawElements + upload do array de matrizes);
//   • POSE ALTERA VÉRTICES: computeSkinMatrices + skinVertex (a MESMA
//     conta do shader, em CPU) — bind pose = identidade, pose rodada
//     move o vértice;
//   • tracks de JOINT (jpos/jrot/jescala) no player + round-trip .goni
//     do Skeleton.
//
// Fixture: GLB com 2 joints (pai + filho), IBM explícita, 3 vértices com
// JOINTS_0/WEIGHTS_0 e uma animação que RODA o joint filho 90° em Y.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assets/GltfAnim.h"
#include "assets/GltfImporter.h"
#include "components/AnimationPlayer.h"
#include "components/MeshRenderer.h"
#include "components/SkeletonComp.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "math/Math.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "GLES3/gl3.h"   // stub: stats do skin (uSkin/uBones/drawElements)

using namespace vv;
using test::matNearF;
using test::nearEqF;
using test::vecNearF;

namespace {

void pushF32(std::vector<u8>& b, f32 v) {
    u8 tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}
void pushU32(std::vector<u8>& b, u32 v) {
    u8 tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}
void pushU16(std::vector<u8>& b, u16 v) {
    u8 tmp[2];
    std::memcpy(tmp, &v, 2);
    b.insert(b.end(), tmp, tmp + 2);
}
void pushU8(std::vector<u8>& b, u8 v) { b.push_back(v); }
void pushMat4(std::vector<u8>& b, const Mat4& m) {
    for (int i = 0; i < 16; ++i) {
        pushF32(b, m.m[i]);
    }
}

// GLB com UM mesh (3 verts, JOINTS_0/WEIGHTS_0) + skin de 2 joints +
// animação "abracar" que roda o JOINT FILHO 90° em Y (node 2).
std::vector<u8> skinnedGlb() {
    std::vector<u8> bin;
    // times [0, 1]
    const u32 timesOff = 0;
    pushF32(bin, 0.0f);
    pushF32(bin, 1.0f);                                        // 8
    // quats: identidade → 90° Y
    const u32 quatOff = 8;
    pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 1);
    pushF32(bin, 0); pushF32(bin, 0.70710678f); pushF32(bin, 0);
    pushF32(bin, 0.70710678f);                                 // 40
    // POSITION: 3 verts (o vert2 vive no espaço do joint FILHO)
    const u32 posOff = 40;
    pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0);         // v0
    pushF32(bin, 1); pushF32(bin, 0); pushF32(bin, 0);         // v1
    pushF32(bin, 1); pushF32(bin, 0); pushF32(bin, 0);         // v2
    // NORMAL
    const u32 norOff = 76;
    for (int i = 0; i < 3; ++i) { pushF32(bin, 0); pushF32(bin, 1); pushF32(bin, 0); }
    // JOINTS_0 (U8 cru — componentType 5121): v0→joint0, v1/v2→joint1
    const u32 jntOff = 112;
    pushU8(bin, 0); pushU8(bin, 0); pushU8(bin, 0); pushU8(bin, 0);
    pushU8(bin, 1); pushU8(bin, 0); pushU8(bin, 0); pushU8(bin, 0);
    pushU8(bin, 1); pushU8(bin, 0); pushU8(bin, 0); pushU8(bin, 0);
    // WEIGHTS_0 (f32): v0→(1,0,0,0), v1/v2→(1,0,0,0)
    const u32 wgtOff = static_cast<u32>(bin.size());
    pushF32(bin, 1); pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0);
    pushF32(bin, 1); pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0);
    pushF32(bin, 1); pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0);
    // índices
    const u32 idxOff = static_cast<u32>(bin.size());
    pushU16(bin, 0); pushU16(bin, 1); pushU16(bin, 2);
    // IBM ×2: "pai" identidade + "filho" T(−1,0,0) (bind do filho em x=+1)
    const u32 ibmOff = static_cast<u32>(bin.size());
    pushMat4(bin, Mat4::identity());
    Mat4 ibmChild = Mat4::identity();
    ibmChild.m[12] = -1.0f;   // T(−1,0,0): world(1,0,0) → bind(0,0,0)
    pushMat4(bin, ibmChild);
    const u32 binLen = static_cast<u32>(bin.size());

    char j[2600];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":8},"     // 0 times
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":32},"    // 1 quat
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":36},"    // 2 pos
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":36},"    // 3 nor
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":12},"    // 4 jnt u8
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":48},"    // 5 wgt f32
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":6},"     // 6 idx
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":128}],\"" // 7 ibm
        "accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":2,\"type\":\"VEC4\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":3,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":4,\"componentType\":5121,\"count\":3,\"type\":\"VEC4\"},"
        "{\"bufferView\":5,\"componentType\":5126,\"count\":3,\"type\":\"VEC4\"},"
        "{\"bufferView\":6,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},"
        "{\"bufferView\":7,\"componentType\":5126,\"count\":2,\"type\":\"MAT4\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{"
        "\"POSITION\":2,\"NORMAL\":3,\"JOINTS_0\":4,\"WEIGHTS_0\":5},"
        "\"indices\":6}]}],"
        "\"nodes\":["
        "{\"mesh\":0,\"name\":\"raiz\",\"translation\":[0,0,0]},"
        "{\"name\":\"pai\",\"translation\":[0,0,0],\"children\":[2]},"
        "{\"name\":\"filho\",\"translation\":[1,0,0]}],"
        "\"scenes\":[{\"nodes\":[0]}],"
        "\"skins\":[{\"joints\":[1,2],\"inverseBindMatrices\":7}],"
        "\"animations\":[{\"name\":\"abracar\",\"channels\":["
        "{\"sampler\":0,\"target\":{\"node\":2,\"path\":\"rotation\"}}],"
        "\"samplers\":[{\"input\":0,\"output\":1}]}]}",
        binLen, timesOff, quatOff, posOff, norOff, jntOff, wgtOff, idxOff,
        ibmOff);
    const std::string json(j);

    std::string jsonP = json;
    while (jsonP.size() % 4 != 0) jsonP += ' ';
    std::vector<u8> binP = bin;
    while (binP.size() % 4 != 0) binP.push_back(0);
    std::vector<u8> glb;
    pushU32(glb, 0x46546C67);
    pushU32(glb, 2);
    pushU32(glb, 12 + 8 + static_cast<u32>(jsonP.size()) + 8 +
                    static_cast<u32>(binP.size()));
    pushU32(glb, static_cast<u32>(jsonP.size()));
    pushU32(glb, 0x4E4F534A);
    glb.insert(glb.end(), jsonP.begin(), jsonP.end());
    pushU32(glb, static_cast<u32>(binP.size()));
    pushU32(glb, 0x004E4942);
    glb.insert(glb.end(), binP.begin(), binP.end());
    return glb;
}

} // namespace

// ---- 1. parser: joints + IBM + JOINTS_0/WEIGHTS_0 -----------------------------

TEST(skin_parser_le_joints_ibm_e_pesos) {
    const std::vector<u8> glb = skinnedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    ASSERT(model.skins.size() == 1);
    const GltfSkin& sk = model.skins[0];
    EXPECT(sk.joints.size() == 2);
    // joints PAIS PRIMEIRO: "pai" (node 1) antes de "filho" (node 2)
    EXPECT(sk.joints[0].name == "pai");
    EXPECT(sk.joints[1].name == "filho");
    EXPECT(sk.joints[1].parent == 0);
    // bind TRS do filho: translate (1,0,0)
    EXPECT(vecNearF(sk.joints[1].pos, Vec3{1.0f, 0.0f, 0.0f}));
    // IBM do filho: T(−1,0,0)
    EXPECT(nearEqF(sk.joints[1].inverseBind.m[12], -1.0f));
    EXPECT(nearEqF(sk.joints[0].inverseBind.m[12], 0.0f));   // identidade
    // nodeToJoint
    EXPECT(sk.nodeToJoint.size() == 3);
    EXPECT(sk.nodeToJoint[1] == 0);
    EXPECT(sk.nodeToJoint[2] == 1);
    // mesh com skin: 3 verts × 4
    ASSERT(model.meshes.size() == 1);
    const MeshData& md = model.meshes[0];
    EXPECT(md.skinned());
    EXPECT(md.skinJoints.size() == 12);
    EXPECT(md.skinJoints[4] == 1);   // v1 → joint 1
    EXPECT(md.skinWeights[0] == 1.0f);
}

// ---- 2. attach: SkeletonComp + clips com track de JOINT -----------------------

TEST(skin_attach_esqueleto_e_clips_de_joint) {
    const std::vector<u8> glb = skinnedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));

    Scene s;
    const Handle h = s.create("Boneco");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    // skin ANTES dos clips (o fluxo do main)
    EXPECT(gltfAttachSkin(s, h, model) == 2);
    EXPECT(gltfAttachClips(s, h, model) == 1);

    const SkeletonComp* sk = tic->getComponent<SkeletonComp>();
    ASSERT(sk != nullptr);
    ASSERT(sk->joints.size() == 2);
    EXPECT(sk->joints[1].name == "filho");
    EXPECT(sk->joints[1].parent == 0);
    EXPECT(nearEqF(sk->joints[1].inverseBind.m[12], -1.0f));

    const AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    ASSERT(pl->clips.size() == 1);
    ASSERT(pl->clips[0].tracks.size() == 1);
    // track de JOINT: jrot por NOME do joint
    EXPECT(pl->clips[0].tracks[0].target == AnimTarget::JointRot);
    EXPECT(pl->clips[0].tracks[0].element == "filho");
    ASSERT(pl->clips[0].tracks[0].keys.size() == 2);
    EXPECT(nearEqF(pl->clips[0].tracks[0].keys[1].v[1], 90.0f, 1e-2f));   // 90° Y
}

TEST(skin_attach_skin_sem_duplicar) {
    const std::vector<u8> glb = skinnedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    Scene s;
    const Handle h = s.create("X");
    s.get(h)->addComponent<Transform3D>();
    EXPECT(gltfAttachSkin(s, h, model) == 2);
    EXPECT(gltfAttachSkin(s, h, model) == 0);   // reimport não duplica
    // TIC morto → 0, sem crash
    const Handle dead = s.create("d");
    s.destroy(dead);
    EXPECT(gltfAttachSkin(s, dead, model) == 0);
}

// ---- 3. POSE ALTERA VÉRTICES (CPU — a mesma conta do shader) ------------------

TEST(skin_bind_pose_e_identidade_e_pose_rodada_move) {
    const std::vector<u8> glb = skinnedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    Scene s;
    const Handle h = s.create("B");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    gltfAttachSkin(s, h, model);
    gltfAttachClips(s, h, model);
    SkeletonComp* sk = tic->getComponent<SkeletonComp>();
    AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();

    // BIND POSE: skin = world·ibm = identidade → vértices NÃO mexem
    Mat4 bones[SkeletonComp::kMaxBones];
    const u32 n = computeSkinMatrices(*sk, bones, SkeletonComp::kMaxBones);
    EXPECT(n == 2);
    EXPECT(matNearF(bones[0], Mat4::identity(), 1e-5f));
    // filho: world=T(1,0,0) · ibm=T(−1,0,0) = I
    EXPECT(matNearF(bones[1], Mat4::identity(), 1e-5f));
    const u8 j0[4] = {0, 0, 0, 0};
    const u8 j1[4] = {1, 0, 0, 0};
    const f32 w1[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    const Vec3 v2{1.0f, 0.0f, 0.0f};
    EXPECT(vecNearF(skinVertex(v2, j1, w1, bones, n), v2, 1e-5f));

    // POSE da animação (t=1: filho RODADO 90° em Y): o vértice MEXE.
    // (Once: o advance(1) para EXATAMENTE no fim — com Loop o fmod(1,1)
    // dava a volta ao t=0 e a pose era a identidade, não o bug que parecia)
    pl->playing = true;
    pl->mode = AnimationPlayer::Mode::Once;
    pl->advance(1.0f);
    pl->apply(s, h);
    const u32 n2 = computeSkinMatrices(*sk, bones, SkeletonComp::kMaxBones);
    EXPECT(n2 == 2);
    EXPECT(!matNearF(bones[1], Mat4::identity(), 1e-3f));
    // v2=(1,0,0) no espaço do filho: 90° Y → (0,0,−1) MUNDO (após ibm: o
    // bind do filho é (1,0,0); world do filho em pose = T(1)·R90 → skin =
    // T(1)·R90·T(−1) → v2: (1−1)=0 → R90(0,0,0)=(0,0,0) → +1 → (1,0,0)???
    // NÃO: skin aplica-se ao vértice JÁ EM ESPAÇO DE BIND: v2=(1,0,0) é
    // ESPAÇO DE MUNDO no glTF (POSITION é do mesh, bind space). O peso é do
    // joint 1: skin[1]·(1,0,0) = T(1)·R90·T(−1)·(1,0,0):
    // T(−1)·(1,0,0)=(0,0,0); R90·(0,0,0)=(0,0,0); T(1)·(0,0,0)=(1,0,0).
    // Hmm — rodar À VOLTA do joint não move este vértice (ele ESTÁ no
    // eixo)... o MESMO teste com o vértice (2,0,0): T(−1)→(1,0,0);
    // R90→(0,0,−1); T(1)→(1,0,−1). Confirmemos com (2,0,0):
    const Vec3 v2x{2.0f, 0.0f, 0.0f};
    const Vec3 moved = skinVertex(v2x, j1, w1, bones, n2);
    EXPECT(vecNearF(moved, Vec3{1.0f, 0.0f, -1.0f}, 1e-3f));
    // e o vértice do joint 0 NÃO mexe (skin[0] continua I)
    EXPECT(vecNearF(skinVertex(v2, j0, w1, bones, n2), v2, 1e-5f));
}

TEST(skin_compute_cap_de_64_bones) {
    SkeletonComp sk;
    sk.joints.resize(80);
    Mat4 bones[SkeletonComp::kMaxBones];
    EXPECT(computeSkinMatrices(sk, bones, SkeletonComp::kMaxBones) == 64);
    EXPECT(computeSkinMatrices(sk, bones, 2) == 2);
    EXPECT(computeSkinMatrices(sk, nullptr, 64) == 0);
    sk.joints.clear();
    EXPECT(computeSkinMatrices(sk, bones, 64) == 0);
}

// ---- 4. MESH SKINADO DESENHA (stub de render) ----------------------------------

TEST(skin_mesh_skinado_desenha_com_bones) {
    glstub::reset();
    Renderer renderer;
    ASSERT(renderer.init());
    glstub::reset();   // o init conta os seus próprios uploads

    // mesh com skin (2 verts, joint 1, peso 1)
    Vertex v[2] = {};
    v[0].pos = Vec3{0.0f, 0.0f, 0.0f};
    v[1].pos = Vec3{2.0f, 0.0f, 0.0f};
    v[0].normal = v[1].normal = Vec3{0.0f, 1.0f, 0.0f};
    const u16 idx[3] = {0, 1, 0};
    const u8 jnt[8] = {1, 0, 0, 0, 1, 0, 0, 0};
    const f32 wgt[8] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
    Mesh mesh;
    ASSERT(mesh.createSkinned(v, 2, idx, 3, jnt, wgt));
    EXPECT(mesh.skinned());

    Mat4 bones[2];
    bones[0] = Mat4::identity();
    bones[1] = Mat4::identity();
    const Mat4 vp = Mat4::identity();
    const DrawStats st = renderer.drawMesh(mesh, Mat4::identity(), vp,
                                           nullptr, nullptr, bones, 2);
    EXPECT(st.drawCalls == 1);                      // DESENHOU
    EXPECT(glstub::stats.drawElementsCalls == 1);
    EXPECT(glstub::stats.lastUniform1i == 1);       // uSkin LIGADO
    EXPECT(glstub::stats.matrix4fvArrayCalls == 1); // uBones[2] uplodeado
    EXPECT(glstub::stats.lastMatrix4fvCount == 2);

    // MESH ESTÁTICO com bones passados: uSkin fica OFF (mesh não skinado —
    // o renderer decide pelo MESH, não pelos bones)
    Mesh staticMesh;
    ASSERT(staticMesh.create(v, 2, idx, 3));
    EXPECT(!staticMesh.skinned());
    renderer.drawMesh(staticMesh, Mat4::identity(), vp, nullptr, nullptr,
                      bones, 2);
    EXPECT(glstub::stats.lastUniform1i == 0);       // uSkin desligado
    EXPECT(glstub::stats.drawElementsCalls == 2);

    // mesh skinado SEM bones (esqueleto sumiu): desenha ESTÁTICO (sem skin)
    renderer.drawMesh(mesh, Mat4::identity(), vp, nullptr, nullptr);
    EXPECT(glstub::stats.drawElementsCalls == 3);
    EXPECT(glstub::stats.lastUniform1i == 0);
    renderer.shutdown();
}

// ---- 5. round-trip .goni do Skeleton + tracks de joint --------------------------

TEST(skin_serializacao_roundtrip_skeleton_e_joint_tracks) {
    const std::vector<u8> glb = skinnedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    Scene s;
    const Handle h = s.create("Boneco");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    tic->addComponent<MeshRenderer>();
    gltfAttachSkin(s, h, model);
    gltfAttachClips(s, h, model);

    const std::string text = SceneSerializer::dump(s);
    EXPECT(std::strstr(text.c_str(), "\"type\":\"Skeleton\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "\"jrot\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "\"filho\"") != nullptr);

    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    Tic* back = s2.get(s2.find("Boneco"));
    ASSERT(back != nullptr);
    const SkeletonComp* sk2 = back->getComponent<SkeletonComp>();
    ASSERT(sk2 != nullptr);
    ASSERT(sk2->joints.size() == 2);
    EXPECT(sk2->joints[1].name == "filho");
    EXPECT(sk2->joints[1].parent == 0);
    EXPECT(nearEqF(sk2->joints[1].inverseBind.m[12], -1.0f));
    EXPECT(vecNearF(sk2->joints[1].pos, Vec3{1.0f, 0.0f, 0.0f}));

    // o clip de joint VOLTA e a pose continua a mexer os vértices
    AnimationPlayer* pl2 = back->getComponent<AnimationPlayer>();
    ASSERT(pl2 != nullptr);
    ASSERT(pl2->clips.size() == 1);
    ASSERT(pl2->clips[0].tracks.size() == 1);
    EXPECT(pl2->clips[0].tracks[0].target == AnimTarget::JointRot);
    EXPECT(pl2->clips[0].tracks[0].element == "filho");
    pl2->playing = true;
    pl2->mode = AnimationPlayer::Mode::Once;   // para no fim (pose final)
    pl2->advance(1.0f);
    pl2->apply(s2, back->handle);
    Mat4 bones[SkeletonComp::kMaxBones];
    const u32 n = computeSkinMatrices(*sk2, bones, SkeletonComp::kMaxBones);
    const u8 j1[4] = {1, 0, 0, 0};
    const f32 w1[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    EXPECT(vecNearF(skinVertex(Vec3{2.0f, 0.0f, 0.0f}, j1, w1, bones, n),
                    Vec3{1.0f, 0.0f, -1.0f}, 1e-3f));
}

// ---- 6. glb SEM skin: nada muda ----------------------------------------------

TEST(skin_sem_skin_nao_mexe_em_nada) {
    // modelo só com mesh (a fixture da 0.8.1 tem 2 nós, sem skins)
    const std::vector<u8> glb = skinnedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    model.skins.clear();   // "sem skin"
    Scene s;
    const Handle h = s.create("A");
    s.get(h)->addComponent<Transform3D>();
    EXPECT(gltfAttachSkin(s, h, model) == 0);
    EXPECT(s.get(h)->getComponent<SkeletonComp>() == nullptr);
    // clips: o canal do nó 2 (não-raiz, sem skin) é SALTADO
    EXPECT(gltfAttachClips(s, h, model) == 0);
}
