// tests/test_gltfanim.cpp — 0.8.1 (F7): IMPORT DE CLIPS glTF.
//
// Aferição da spec 0.8.1:
//   • parser de animações de glTF (channels/samplers) → GltfAnimation;
//   • importar .glb com animação → clips NOMEADOS no AnimationPlayer;
//   • lista de clips + escolher clip ativo (o seletor da timeline);
//   • playback do clip importado (o TIC move-se);
//   • round-trip .goni dos clips importados;
//   • canais de OUTROS nós são saltados (0.8.2); CUBICSPLINE→meio;
//     STEP tolerada como linear (dívidas documentadas).
//
// Fixtures construídas em código (o padrão do test_import_gltf): buffer
// binário com times/values, JSON com offsets reais, container GLB com
// chunks JSON+BIN.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assets/GltfAnim.h"
#include "assets/GltfImporter.h"
#include "components/AnimationPlayer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"

using namespace vv;
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

// GLB mínimo com UM triângulo + UMA animação "girar" que anima o NÓ 0:
//   translation: (0,0,0) @0s → (2,0,0) @2s          (VEC3)
//   rotation:    identidade @0s → 90° Y @2s          (VEC4 quat)
//   scale:       (1,1,1) @0s → (2,1,1) @2s           (VEC3)
// samplers extras: um CUBICSPLINE (3×values) e um canal do NÓ 1 (ignorado).
std::vector<u8> animatedGlb() {
    // ---- buffer binário -------------------------------------------------------
    std::vector<u8> bin;
    const u32 timesOff = 0;
    pushF32(bin, 0.0f);
    pushF32(bin, 2.0f);                                   // 8 bytes
    const u32 posOff = static_cast<u32>(bin.size());
    pushF32(bin, 0.0f); pushF32(bin, 0.0f); pushF32(bin, 0.0f);
    pushF32(bin, 2.0f); pushF32(bin, 0.0f); pushF32(bin, 0.0f);   // 24 bytes
    const u32 quatOff = static_cast<u32>(bin.size());
    pushF32(bin, 0.0f); pushF32(bin, 0.0f); pushF32(bin, 0.0f); pushF32(bin, 1.0f);
    const f32 halfSqrt2 = 0.70710678f;
    pushF32(bin, 0.0f); pushF32(bin, halfSqrt2); pushF32(bin, 0.0f); pushF32(bin, halfSqrt2);
    const u32 sclOff = static_cast<u32>(bin.size());
    pushF32(bin, 1.0f); pushF32(bin, 1.0f); pushF32(bin, 1.0f);
    pushF32(bin, 2.0f); pushF32(bin, 1.0f); pushF32(bin, 1.0f);   // 24 bytes
    // triângulo (POSITION 3×VEC3 + índices u16)
    const u32 triPosOff = static_cast<u32>(bin.size());
    pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0);
    pushF32(bin, 1); pushF32(bin, 0); pushF32(bin, 0);
    pushF32(bin, 0); pushF32(bin, 1); pushF32(bin, 0);
    const u32 triIdxOff = static_cast<u32>(bin.size());
    pushU16(bin, 0); pushU16(bin, 1); pushU16(bin, 2);
    // CUBICSPLINE: 2 keys × 3 blocos × VEC3 = 18 floats (in/val/out)
    const u32 cubOff = static_cast<u32>(bin.size());
    for (int k = 0; k < 18; ++k) {
        pushF32(bin, 9.0f);   // tudo 9 — só o MEIO (valor) interessa: 9
    }
    const u32 binLen = static_cast<u32>(bin.size());

    // ---- JSON ---------------------------------------------------------------
    char j[2200];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":8},"        // 0 times
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":24},"       // 1 pos
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":32},"       // 2 quat
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":24},"       // 3 scale
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":36},"       // 4 tri pos
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":6},"        // 5 tri idx
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":72}],\""    // 6 cubicspline
        "accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":2,\"type\":\"VEC4\"},"
        "{\"bufferView\":3,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"},"
        "{\"bufferView\":4,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":5,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"},"
        "{\"bufferView\":6,\"componentType\":5126,\"count\":6,\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":4},"
        "\"indices\":5}]}],"
        "\"nodes\":[{\"mesh\":0,\"name\":\"raiz\"},"
        "{\"name\":\"outro\"}],"
        "\"scenes\":[{\"nodes\":[0]}],"
        "\"animations\":[{\"name\":\"girar\","
          "\"channels\":["
            "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}},"
            "{\"sampler\":1,\"target\":{\"node\":0,\"path\":\"rotation\"}},"
            "{\"sampler\":2,\"target\":{\"node\":0,\"path\":\"scale\"}},"
            "{\"sampler\":4,\"target\":{\"node\":1,\"path\":\"translation\"}}"
          "],"
          "\"samplers\":["
            "{\"input\":0,\"output\":1},"
            "{\"input\":0,\"output\":2},"
            "{\"input\":0,\"output\":3,\"interpolation\":\"STEP\"},"
            "{\"input\":0,\"output\":6,\"interpolation\":\"CUBICSPLINE\"},"
            "{\"input\":0,\"output\":1}"
          "]}]}",
        binLen, timesOff, posOff, quatOff, sclOff, triPosOff, triIdxOff, cubOff);
    const std::string json(j);

    // ---- container GLB (chunkLength INCLUI o padding — convenção da casa,
    // ver glb_com_chunks do test_import_gltf) ----------------------------------
    std::string jsonP = json;
    while (jsonP.size() % 4 != 0) {
        jsonP += ' ';
    }
    std::vector<u8> binP = bin;
    while (binP.size() % 4 != 0) {
        binP.push_back(0);
    }
    std::vector<u8> glb;
    pushU32(glb, 0x46546C67);   // 'glTF'
    pushU32(glb, 2);
    const u32 total = 12 + 8 + static_cast<u32>(jsonP.size()) + 8 +
                      static_cast<u32>(binP.size());
    pushU32(glb, total);
    pushU32(glb, static_cast<u32>(jsonP.size()));
    pushU32(glb, 0x4E4F534A);   // 'JSON'
    glb.insert(glb.end(), jsonP.begin(), jsonP.end());
    pushU32(glb, static_cast<u32>(binP.size()));
    pushU32(glb, 0x004E4942);   // 'BIN'
    glb.insert(glb.end(), binP.begin(), binP.end());
    return glb;
}

} // namespace

// ---- 1. o parser lê channels/samplers ---------------------------------------

TEST(gltfanim_parser_le_clips_channels_samplers) {
    const std::vector<u8> glb = animatedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    ASSERT(model.animations.size() == 1);
    const GltfAnimation& a = model.animations[0];
    EXPECT(a.name == "girar");
    EXPECT(a.samplers.size() == 5);
    EXPECT(a.channels.size() == 4);   // 3 do nó 0 + 1 do nó 1

    // sampler 0: times [0,2] + values VEC3
    EXPECT(a.samplers[0].times.size() == 2);
    EXPECT(nearEqF(a.samplers[0].times[0], 0.0f));
    EXPECT(nearEqF(a.samplers[0].times[1], 2.0f));
    EXPECT(a.samplers[0].components == 3);
    EXPECT(nearEqF(a.samplers[0].values[0], 0.0f));
    EXPECT(nearEqF(a.samplers[0].values[3], 2.0f));   // x da 2ª key

    // sampler 1: VEC4 (quat)
    EXPECT(a.samplers[1].components == 4);
    EXPECT(nearEqF(a.samplers[1].values[4], 0.0f));
    EXPECT(nearEqF(a.samplers[1].values[5], 0.70710678f));   // y do 90° Y

    // CUBICSPLINE: extraído o MEIO (6 keys VEC3→2 keys de valor 9)
    EXPECT(a.samplers[3].values.size() == 6);
    EXPECT(nearEqF(a.samplers[3].values[0], 9.0f));

    // channels: nó 0 com os 3 paths + nó 1 (ignorado no attach)
    EXPECT(a.channels[0].node == 0);
    EXPECT(a.channels[0].path == GltfAnimChannel::Path::Translation);
    EXPECT(a.channels[1].path == GltfAnimChannel::Path::Rotation);
    EXPECT(a.channels[2].path == GltfAnimChannel::Path::Scale);
    EXPECT(a.channels[3].node == 1);       // canal do nó 1 (ignorado no attach)
    EXPECT(a.channels[3].sampler == 4);
}

// ---- 2. attach: clips nomeados no AnimationPlayer -----------------------------

TEST(gltfanim_attach_clips_no_player) {
    const std::vector<u8> glb = animatedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));

    Scene s;
    const Handle h = s.create("Robo");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    const u32 n = gltfAttachClips(s, h, model);
    EXPECT(n == 1);   // "girar" (sem canal útil para o nó 1 → não entra)
    const AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    ASSERT(pl->clips.size() == 1);
    EXPECT(pl->clips[0].name == "girar");
    // os 3 tracks do nó 0: pos/rot/escala (o canal do nó 1 NÃO entrou)
    ASSERT(pl->clips[0].tracks.size() == 3);
    EXPECT(pl->clips[0].tracks[0].target == AnimTarget::TicPos);
    EXPECT(pl->clips[0].tracks[1].target == AnimTarget::TicRot);
    EXPECT(pl->clips[0].tracks[2].target == AnimTarget::TicScale);
    // keys corretas
    ASSERT(pl->clips[0].tracks[0].keys.size() == 2);
    EXPECT(nearEqF(pl->clips[0].tracks[0].keys[0].t, 0.0f));
    EXPECT(nearEqF(pl->clips[0].tracks[0].keys[1].t, 2.0f));
    EXPECT(nearEqF(pl->clips[0].tracks[0].keys[1].v[0], 2.0f));
    // rot: quat 90° Y → euler (0, 90, 0) GRAUS
    EXPECT(nearEqF(pl->clips[0].tracks[1].keys[1].v[1], 90.0f, 1e-2f));
    EXPECT(nearEqF(pl->clips[0].tracks[1].keys[1].v[0], 0.0f, 1e-2f));
    // escala
    EXPECT(nearEqF(pl->clips[0].tracks[2].keys[1].v[0], 2.0f));
}

TEST(gltfanim_attach_cria_player_e_transform) {
    // TIC sem nada: o attach cria Player E Transform3D (defesa)
    const std::vector<u8> glb = animatedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    Scene s;
    const Handle h = s.create("X");
    EXPECT(gltfAttachClips(s, h, model) == 1);
    EXPECT(s.get(h)->getComponent<AnimationPlayer>() != nullptr);
    EXPECT(s.get(h)->getComponent<Transform3D>() != nullptr);
    // TIC morto/inválido → 0 clips, sem crash
    EXPECT(gltfAttachClips(s, Handle::invalid(), model) == 0);
    const Handle hDead = s.create("morto");
    s.destroy(hDead);
    EXPECT(gltfAttachClips(s, hDead, model) == 0);
}

TEST(gltfanim_attach_clips_acrescenta_ao_edit) {
    // player com clip "edit" PRÉVIO: os importados ACRESCENTAM e o 1º
    // importado fica ATIVO
    const std::vector<u8> glb = animatedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    Scene s;
    const Handle h = s.create("Y");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    pl->editClip();   // cria "edit"
    ASSERT(pl->clips.size() == 1);
    EXPECT(gltfAttachClips(s, h, model) == 1);
    ASSERT(pl->clips.size() == 2);
    EXPECT(pl->clips[0].name == "edit");
    EXPECT(pl->clips[1].name == "girar");
    EXPECT(pl->activeClip == 1);   // o importado fica ativo
}

// ---- 3. playback do clip importado --------------------------------------------

TEST(gltfanim_playback_o_tic_move_se) {
    const std::vector<u8> glb = animatedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    Scene s;
    const Handle h = s.create("Robo");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    gltfAttachClips(s, h, model);
    AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);

    // reproduz: 1 s (meio do clip de 2 s)
    pl->playing = true;
    pl->advance(1.0f);
    pl->apply(s, h);
    Transform3D* tr = tic->getComponent<Transform3D>();
    EXPECT(nearEqF(tr->pos.x, 1.0f));                       // translation lerp
    EXPECT(nearEqF(tr->scale.x, 1.5f));                     // scale lerp
    // rot: quat 90°Y no fim → euler yaw 45° no meio
    EXPECT(nearEqF(tr->rot.y, 0.38268f, 1e-3f));            // sin(22.5°)
    // loop: 5 s → 5 mod 2 = 1 (o default é loop)
    pl->advance(4.0f);
    EXPECT(nearEqF(pl->time, 1.0f));
}

// ---- 4. round-trip .goni dos clips importados ---------------------------------

TEST(gltfanim_serializacao_roundtrip_do_importado) {
    const std::vector<u8> glb = animatedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    Scene s;
    const Handle h = s.create("Robo");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    tic->addComponent<MeshRenderer>();
    gltfAttachClips(s, h, model);

    const std::string text = SceneSerializer::dump(s);
    EXPECT(std::strstr(text.c_str(), "\"name\":\"girar\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "\"clips\"") != nullptr);

    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    Tic* back = s2.get(s2.find("Robo"));
    ASSERT(back != nullptr);
    const AnimationPlayer* pl = back->getComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    ASSERT(pl->clips.size() == 1);
    EXPECT(pl->clips[0].name == "girar");
    ASSERT(pl->clips[0].tracks.size() == 3);
    EXPECT(pl->clips[0].tracks[1].target == AnimTarget::TicRot);
    EXPECT(nearEqF(pl->clips[0].tracks[1].keys[1].v[1], 90.0f, 1e-2f));
    // e REPRODUZ depois do round-trip (o critério da spec)
    AnimationPlayer* plm = back->getComponent<AnimationPlayer>();
    plm->playing = true;
    plm->advance(1.0f);
    plm->apply(s2, back->handle);
    EXPECT(nearEqF(back->getComponent<Transform3D>()->pos.x, 1.0f));
}

// ---- 5. o seletor de clips da timeline (teste completo no fim, com ------
//        UiContext + toques injetados) ------------------------------------

// ---- 6. glb SEM animações → nada acontece ---------------------------------------

TEST(gltfanim_sem_animacoes_nada_muda) {
    // o triGltf clássico (sem "animations"): attach devolve 0 e o TIC
    // fica sem player
    Scene s;
    const Handle h = s.create("A");
    s.get(h)->addComponent<Transform3D>();
    GltfModel model;   // vazio de animações
    EXPECT(gltfAttachClips(s, h, model) == 0);
    EXPECT(s.get(h)->getComponent<AnimationPlayer>() == nullptr);
}

// ---- Helper + teste REAL do seletor (UiContext + toques) -----------------------

#include "platform/InputState.h"
#include "render/Renderer.h"
#include "ui/Timeline.h"
#include "ui/UiContext.h"

namespace {

struct TlEnv {
    FontAtlas   font;
    UiContext   ui;
    InputState  input;
    Scene       scene;
    editor::EditorState st;
    timeline::State tl;
    Handle      tic{};

    TlEnv() {
        const char* fontPath = FONT_FIXTURE;
        font.loadFromPaths(&fontPath, 1, 28.0f);
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        tic = scene.create("Robo");
        Tic* t = scene.get(tic);
        t->addComponent<Transform3D>();
        t->addComponent<AnimationPlayer>();
        st.selected = tic;
    }

    void frame() {
        ui.beginFrame(nullptr, &input, 1600.0f, 720.0f);
        timeline::drawTimeline(ui, input, scene, st, tl, 1.0f / 60.0f);
        ui.endFrame();
        input.clearEdges();
    }

    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame();
        input.injectUp(0);
        frame();
    }
};

} // namespace

TEST(gltfanim_timeline_troca_de_clip_ativo) {
    const std::vector<u8> glb = animatedGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));

    TlEnv e;
    AnimationPlayer* pl = e.scene.get(e.tic)->getComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    // 1 clip "edit" default + 1 importado
    pl->editClip();
    const u32 n = gltfAttachClips(e.scene, e.tic, model);
    ASSERT(n == 1);
    ASSERT(pl->clips.size() == 2);
    EXPECT(pl->activeClip == 1);   // importado ativo (o attach decide)

    // abre o seletor de clips (botão "clip:" no header da strip)
    const UiRect r = timeline::timelineRect(1600.0f, 720.0f, safe::Insets{}, true);
    e.tap(r.x + 464.0f, r.y + 24.0f);   // centro do botão clip (r.x+376+88)
    EXPECT(e.tl.clipMenu);
    // escolhe o clip 0 ("edit"): 1º item do overlay centrado
    const f32 w = 300.0f;
    const f32 h = timeline::kHeaderH + 3.0f * 44.0f + 12.0f;   // 2 clips + novo
    const f32 x = (1600.0f - w) * 0.5f;
    const f32 y = (720.0f - h) * 0.5f;
    e.tap(x + 150.0f, y + timeline::kHeaderH + 18.0f);
    EXPECT(!e.tl.clipMenu);
    EXPECT(pl->activeClip == 0);
    EXPECT(nearEqF(pl->time, 0.0f));   // trocou → recomeça limpo

    // "novo (edit)" com "edit" existente: REUTILIZA (não duplica)
    e.tap(r.x + 464.0f, r.y + 24.0f);
    EXPECT(e.tl.clipMenu);
    e.tap(x + 150.0f, y + timeline::kHeaderH + 2.0f * 44.0f + 18.0f);   // item "novo"
    EXPECT(!e.tl.clipMenu);
    EXPECT(pl->clips.size() == 2);
    EXPECT(pl->activeClip == 0);   // o "edit" reutilizado

    // a timeline EDITA o clip ativo: +track no clip 0
    e.tap(r.x + r.w - 36.0f, r.y + 24.0f);   // "+"
    EXPECT(e.tl.addTrackMenu);
    e.tap((1600.0f - 300.0f) * 0.5f + 150.0f,
          (720.0f - (timeline::kHeaderH + 6.0f * 44.0f + 12.0f)) * 0.5f +
              timeline::kHeaderH + 18.0f);
    EXPECT(pl->clips[0].tracks.size() == 1);
    EXPECT(pl->clips[1].tracks.size() == 3);   // o importado intacto
}
