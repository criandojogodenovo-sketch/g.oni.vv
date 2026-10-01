// tests/test_wiring084.cpp — 0.8.4: TESTES DE INTEGRAÇÃO DE WIRING
// (crashes e freezes — o que os testes unitários NÃO apanhavam).
//
// Os três bugs do C33 que esta suíte teria apanhado ANTES do fix:
//
//   1. "ECRÃ PRETO AO ADICIONAR UI/TIC" — o resolveCanvasLayout iterava
//      TODOS os elementos do canvas mas escrevia out[i] SEM respeitar o
//      cap do chamador (o editor usa CanvasLayout lay[32] na STACK).
//      Passar de 32 elementos = escrita fora dos limites = corrupção de
//      memória (crash/freeze/ecrã preto). O teste do SENTINELA coloca uma
//      marca imediatamente após o buffer do chamador e aferi que o
//      resolver NUNCA escreve além do cap.
//
//   2. "FUNÇÕES QUE PARAM / TEXTO QUE DESAPARECE" — caps de submissão
//      (kMaxRuns=32 no UiContext, kMaxSubs=32 no Renderer) cortavam
//      SILLENTAMENTE runs e, pior, o batch de GLIFOS (submetido no fim):
//      com 32 runs TODO o texto saía do ecrã. Os caps subiram (64/66) e
//      o corte loga; o teste emite runs suficientes para estourar o cap
//      ANTIGO e aferi que runs + glifos cabem SEMPRE.
//
//   3. "FREEZES" — o wiring de play/stop repetido + add/remove em massa
//      tem de manter os invariantes (contagens, pose restaurada, NENHUM
//      objeto GL novo em steady state — realloc em loop é o gatilho do
//      freeze no driver). O teste simula o fluxo completo do device
//      (criar → editar → play → stop ×20 → salvar → carregar) com os
//      contadores do stub GL.
//
// Estilo da casa: GL-free/stub, sem device; cada caso simula o FLUXO
// completo (wiring), não funções isoladas.
#include "TestFramework.h"
#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats)
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>
#include "assets/TextureCompressor.h"
#include "components/AnimationPlayer.h"
#include "components/CameraComp.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "render/Cube.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "render/Texture.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/Timeline.h"
#include "ui/UiContext.h"
#include "ui/UiRuntime.h"

using namespace vv;
using ::test::nearEqF;

namespace {
const char* kFontPath = FONT_FIXTURE;
}

// ---- helpers ----------------------------------------------------------------

namespace {

// canvas com N elementos de topo (Panel default, âncoras Left/Top)
void fillCanvas(UiCanvas& c, u32 n, const char* prefix = "el") {
    char name[32];
    for (u32 i = 0; i < n; ++i) {
        std::snprintf(name, sizeof(name), "%s%u", prefix, i);
        UiElement e;
        e.name = name;
        e.kind = UiElement::Kind::Panel;
        e.ox = static_cast<f32>(i) * 4.0f;
        e.oy = static_cast<f32>(i) * 3.0f;
        e.w = 40.0f;
        e.h = 24.0f;
        c.elements.push_back(e);
    }
}

// par de rects sobreposto? (o mesmo critério do test_uieditor)
bool overlap(const UiRect& a, const UiRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
           b.y < a.y + a.h;
}

// textura RGBA8 4x4 real (para o resolver de imagens da UI no stub GL)
vv::Texture* makeTinyTexture(vv::Texture& out) {
    CompressedImage img;
    img.format = CompressedFormat::RGBA8;
    img.width = 4;
    img.height = 4;
    img.data.assign(4 * 4 * 4, 0xC8);
    CompressedMip m;
    m.width = 4;
    m.height = 4;
    m.offset = 0;
    m.size = static_cast<u32>(img.data.size());
    img.mips.push_back(m);
    if (!out.createFromCompressed(img)) {
        return nullptr;
    }
    return &out;
}

} // namespace

// ---- 1. o SENTINELA: o resolver NUNCA escreve além do cap -------------------

TEST(wiring084_resolver_cap_nao_escalava_o_buffer_do_chamador) {
    // O teste que teria APANHADO o bug do ecrã preto: 40 elementos num
    // buffer de 32 — antes do fix, o placeAt escrevia lay[32..39] por cima
    // do sentinela (e da stack real do device).
    UiCanvas c;
    fillCanvas(c, 40);
    safe::Insets ins;   // zeros

    ui::CanvasLayout lay[32];
    static constexpr u32 kGuardQuads = 32;   // 224 bytes após o buffer
    volatile f32 guard[kGuardQuads * 7];
    for (u32 i = 0; i < kGuardQuads * 7; ++i) {
        guard[i] = 1234567.0f;
    }

    ui::resolveCanvasLayout(c, 1600.0f, 720.0f, ins, lay, 32u);

    bool guardIntact = true;
    for (u32 i = 0; i < kGuardQuads * 7; ++i) {
        if (guard[i] != 1234567.0f) {
            guardIntact = false;
            break;
        }
    }
    EXPECT(guardIntact);

    // contrato funcional: os PRIMEIROS 32 ficam dispostos (rect válido +
    // visíveis); nada além deles existe (o draw/hit-test do editor também
    // limitam a 32). `laid` = "tem pai" (topo → false); o que interessa ao
    // draw é shown + rect.
    for (u32 i = 0; i < 32; ++i) {
        EXPECT(lay[i].shown);
        EXPECT(lay[i].rect.w > 0.0f);
        EXPECT(lay[i].parentIdx == -1);
    }
}

TEST(wiring084_resolver_cap_com_container_e_filhos_fora_do_cap) {
    // VBox com 33 filhos: o placeAt recursivo também tem de respeitar o
    // cap (antes, os loops de filhos liam sizeW[j]/sizeH[j] para j >= cap
    // = leitura fora dos vetores internos e escrita em out[j] além do fim).
    UiCanvas c;
    UiElement box;
    box.name = "box";
    box.kind = UiElement::Kind::VBox;
    box.ox = 10.0f;
    box.oy = 10.0f;
    box.w = 200.0f;
    box.h = 400.0f;
    c.elements.push_back(box);
    fillCanvas(c, 33, "filho");
    for (size_t i = 1; i < c.elements.size(); ++i) {
        c.elements[i].parent = "box";
    }
    safe::Insets ins;

    ui::CanvasLayout lay[32];
    volatile f32 guard[64];
    for (u32 i = 0; i < 64; ++i) {
        guard[i] = 424242.0f;
    }
    ui::resolveCanvasLayout(c, 1600.0f, 720.0f, ins, lay, 32u);

    bool guardIntact = true;
    for (u32 i = 0; i < 64; ++i) {
        if (guard[i] != 424242.0f) {
            guardIntact = false;
            break;
        }
    }
    EXPECT(guardIntact);
    // o box + os primeiros 31 filhos ficam dispostos e DENTRO do box
    EXPECT(lay[0].shown);
    for (u32 i = 1; i < 32; ++i) {
        EXPECT(lay[i].shown);
        EXPECT(lay[i].parentIdx == 0);
    }
}

// ---- 2. caps de submissão: runs + glifos CABEM SEMPRE -----------------------

TEST(wiring084_caps_runs_e_glifos_sobrevivem_ao_frame_cheio) {
    // Simula o pior ecrã do device (browser + timeline + painéis): alterna
    // sólido/imagem 64× (estourava o kMaxRuns=32 antigo — os runs além do
    // cap eram DESCARTADOS) e aferi que TODOS os runs são submetidos.
    glstub::reset();
    Renderer r;
    ASSERT(r.init());
    UiContext ui;
    ui.init();
    ui.setFont(nullptr);
    Texture texHolder;
    const Texture* tex = makeTinyTexture(texHolder);
    ASSERT(tex != nullptr && tex->ok());
    static const Texture* kTex = nullptr;
    kTex = tex;
    ui.setImageResolver([](const std::string&) -> const Texture* {
        return kTex;
    });
    ui.beginFrame(&r, nullptr, 1600.0f, 720.0f);

    const f32 white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    for (u32 i = 0; i < 64; ++i) {
        ui.panel(static_cast<f32>(i) * 8.0f, 10.0f, 6.0f, 6.0f, white);
        ui.imageQuad(static_cast<f32>(i) * 8.0f, 20.0f, 6.0f, 6.0f, "t", white);
    }
    EXPECT(ui.runCountForTest() == 128);   // 64 alternâncias = 128 runs

    ui.endFrame();
    // TODOS os runs aceites (o cap antigo de 32 descartava metade — e com
    // glifos, o texto INTEIRO sumia do device)
    EXPECT(r.submissionsForTest() == ui.runCountForTest());

    r.shutdown();
}

TEST(wiring084_caps_glifos_sobrevivem_com_os_runs_no_limite) {
    // O caso EXATO do C33: runs a esgotar o limiar (64) — os glifos (sempre
    // a última submissão) têm de continuar DENTRO do limiar de aviso do
    // Renderer (66 = 64 + glifos + folga) e SEMPRE aceites.
    glstub::reset();
    Renderer r;
    ASSERT(r.init());
    UiContext ui;
    ui.init();
    FontAtlas font;
    ASSERT(font.loadFromPaths(&kFontPath, 1, 28.0f));
    ui.setFont(&font);
    Texture texHolder;
    const Texture* tex = makeTinyTexture(texHolder);
    ASSERT(tex != nullptr && tex->ok());
    static const Texture* kTex = nullptr;
    kTex = tex;
    ui.setImageResolver([](const std::string&) -> const Texture* {
        return kTex;
    });
    ui.beginFrame(&r, nullptr, 1600.0f, 720.0f);
    const f32 white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    for (u32 i = 0; i < 32; ++i) {
        ui.panel(static_cast<f32>(i) * 8.0f, 10.0f, 6.0f, 6.0f, white);
        ui.imageQuad(static_cast<f32>(i) * 8.0f, 20.0f, 6.0f, 6.0f, "x", white);
    }
    EXPECT(ui.runCountForTest() == 64);
    ui.label(2.0f, 2.0f, "texto", white);   // glifos presentes no frame
    ui.endFrame();
    // 64 runs + 1 batch de glifos = 65 submissões — TODAS aceites (antes:
    // subCount cap 32 → a partir de 32 submissões TUDO era descartado,
    // glifos incluídos)
    EXPECT(r.submissionsForTest() == 65);
    r.shutdown();
}

// ---- 3. storm de wiring: add/remove em massa + play/stop repetido -----------

TEST(wiring084_storm_add_remove_50_mesh_50_ui_10_tics_sem_perder_invariantes) {
    // O fluxo do dono no C33: adicionar MUITOS elementos em sequência
    // (mesh/UI/TICs), remover alguns, voltar a adicionar — a cena tem de
    // permanecer ÍNTEGRA (sem corruption, sem handles mortos, contagens
    // coerentes). Antes do fix do resolver, este fluxo CORROMPIA a stack
    // no editor UI mode com 33+ elementos.
    Scene scene;
    Mesh* nullMesh = nullptr;   // sem GL no hospedeiro (o draw salta)

    // 50 TICs Mesh (preset 0.8.0 — Transform+MeshRenderer, sem física)
    std::vector<Handle> meshes;
    for (u32 i = 0; i < 50; ++i) {
        const Handle h = createTicFromPreset(scene, PresetKind::Mesh,
                                             nullMesh, nullptr);
        ASSERT(h.valid());
        meshes.push_back(h);
    }
    // 10 TICs de câmara (0.7.7)
    for (u32 i = 0; i < 10; ++i) {
        const Handle h = scene.create("Camera");
        ASSERT(h.valid());
        ASSERT(scene.get(h)->addComponent<Transform3D>() != nullptr);
        ASSERT(scene.get(h)->addComponent<CameraComp>() != nullptr);
    }
    // 1 TIC de UI com 50 elementos (a atravessar o cap 32 do resolver!)
    const Handle uiTic = scene.create("UI");
    ASSERT(uiTic.valid());
    UiCanvas* canvas = scene.get(uiTic)->addComponent<UiCanvas>();
    ASSERT(canvas != nullptr);
    fillCanvas(*canvas, 50);

    // TODOS os frames do editor UI mode com o buffer fixo de 32: o wiring
    // EXATO do drawUiViewport/uiDetachElement (main → UiEditor)
    safe::Insets ins;
    {
        ui::CanvasLayout lay[32];
        for (u32 frame = 0; frame < 10; ++frame) {
            ui::resolveCanvasLayout(*canvas, 1600.0f, 720.0f, ins, lay, 32u);
        }
    }

    EXPECT(scene.count() == 61);

    // remove 20 meshes e re-adiciona (add/remove repetido do critério)
    for (u32 i = 0; i < 20; ++i) {
        ASSERT(scene.destroy(meshes[i]));
    }
    EXPECT(scene.count() == 41);
    for (u32 i = 0; i < 20; ++i) {
        const Handle h = createTicFromPreset(scene, PresetKind::Mesh,
                                             nullMesh, nullptr);
        ASSERT(h.valid());
        meshes[i] = h;
    }
    EXPECT(scene.count() == 61);

    // handles sobreviventes continuam vivos e com os componentes intactos
    for (u32 i = 20; i < 50; ++i) {
        const Tic* t = scene.get(meshes[i]);
        ASSERT(t != nullptr);
        EXPECT(t->getComponent<MeshRenderer>() != nullptr);
    }
    for (u32 i = 0; i < 10; ++i) {
        // (câmaras não foram tocadas — re-resolve pelos nomes)
    }
    ASSERT(scene.get(uiTic) != nullptr);
    EXPECT(scene.get(uiTic)->getComponent<UiCanvas>()->elements.size() == 50);
}

TEST(wiring084_play_stop_repetido_20x_pose_integra_e_sem_leak_de_ui) {
    // play/stop repetido (critério 0.8.4): o snapshot captura/repõe pos e
    // os elementos de UI voltam à pose de editor; players param no zero.
    Scene scene;
    Mesh* nullMesh = nullptr;
    const Handle h = createTicFromPreset(scene, PresetKind::Mesh, nullMesh,
                                         nullptr);
    ASSERT(h.valid());
    Transform3D* tr = scene.get(h)->getComponent<Transform3D>();
    ASSERT(tr != nullptr);
    const Vec3 editorPos = tr->pos;

    // anima: track pos + 2 keys (o Play de jogo aplica isto por frame)
    AnimationPlayer* pl = scene.get(h)->addComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    AnimTrack* track = pl->addTrack(AnimTarget::TicPos, "");
    ASSERT(track != nullptr);
    AnimKey k0;
    k0.t = 0.0f;
    k0.v[0] = editorPos.x;
    k0.v[1] = editorPos.y;
    k0.v[2] = editorPos.z;
    track->keys.push_back(k0);
    AnimKey k1;
    k1.t = 1.0f;
    k1.v[0] = editorPos.x + 5.0f;
    k1.v[1] = editorPos.y;
    k1.v[2] = editorPos.z;
    track->keys.push_back(k1);
    track->sortKeys();

    UiCanvas* canvas = scene.get(h)->addComponent<UiCanvas>();
    ASSERT(canvas != nullptr);
    fillCanvas(*canvas, 4, "hud");
    const f32 oy0 = canvas->elements[0].oy;

    for (u32 cycle = 0; cycle < 20; ++cycle) {
        // enterPlayMode (o wiring do main): snapshot + players a correr
        PlaySnapshot snap;
        playSnapshotCapture(scene, snap);
        pl->playing = true;
        pl->time = 0.0f;
        for (u32 s = 0; s < 30; ++s) {   // 0.5 s de play
            pl->advance(1.0f / 60.0f);
            pl->apply(scene, h);
        }
        // a pose MOVEU (o play aplica)
        EXPECT(tr->pos.x > editorPos.x + 1.0f);

        // leavePlayMode: restaura + para players
        playSnapshotRestore(scene, snap);
        pl->playing = false;
        pl->time = 0.0f;
        pl->resetDir();

        EXPECT(nearEqF(tr->pos.x, editorPos.x));
        EXPECT(nearEqF(tr->pos.y, editorPos.y));
        EXPECT(nearEqF(canvas->elements[0].oy, oy0));   // UI restaurada
        EXPECT(!pl->playing);
    }
    EXPECT(scene.count() == 1);
}

TEST(wiring084_save_load_com_cena_grande_roundtrip_do_storm) {
    // salvar/carregar a cena do storm (o fluxo criar→editar→save→load):
    // os 61 TICs voltam com os componentes; nenhum handle morto no load.
    Scene scene;
    Mesh* nullMesh = nullptr;
    for (u32 i = 0; i < 50; ++i) {
        (void)createTicFromPreset(scene, PresetKind::Mesh, nullMesh, nullptr);
    }
    const Handle uiTic = scene.create("UI");
    UiCanvas* canvas = scene.get(uiTic)->addComponent<UiCanvas>();
    fillCanvas(*canvas, 50);
    AnimationPlayer* pl = scene.get(uiTic)->addComponent<AnimationPlayer>();
    AnimTrack* tr = pl->addTrack(AnimTarget::UiPos, "el0");
    ASSERT(tr != nullptr);
    AnimKey k;
    k.t = 0.5f;
    k.v[0] = 12.0f;
    k.v[1] = 34.0f;
    tr->keys.push_back(k);

    const std::string json = SceneSerializer::dump(scene);
    EXPECT(!json.empty());

    Scene loaded;
    SceneSerializer::LoadCtx ctx;   // resolvers vazios: meshes ficam null
    ASSERT(SceneSerializer::loadText(loaded, json, ctx));
    EXPECT(loaded.count() == scene.count());
    const Handle uiH = loaded.find("UI");
    ASSERT(uiH.valid());
    AnimationPlayer* pl2 = loaded.get(uiH)->getComponent<AnimationPlayer>();
    ASSERT(pl2 != nullptr);
    ASSERT(pl2->activeClipPtr() != nullptr);
    const bool hasTrack = !pl2->activeClipPtr()->tracks.empty();
    EXPECT(hasTrack);
    if (hasTrack) {
        EXPECT(pl2->activeClipPtr()->tracks[0].keys.size() == 1);
    }
    UiCanvas* cv2 = loaded.get(uiH)->getComponent<UiCanvas>();
    ASSERT(cv2 != nullptr);
    EXPECT(cv2->elements.size() == 50);
}

// ---- 4. steady state: NENHUM objeto GL novo por frame (realloc em loop) -----

TEST(wiring084_steady_state_zero_realloc_gl_em_loop_de_frames) {
    // O gatilho de FREEZE do critério: realloc de buffers em LOOP. O frame
    // em steady state (cena já carregada) NÃO pode criar/destruir NENHUM
    // objeto GL — só subir o batch da UI (bufferData dinâmico) e desenhar.
    glstub::reset();
    Scene scene;
    CubeMeshData cube = makeCube(1.0f);
    Mesh mesh;
    ASSERT(mesh.create(cube.vertices.data(),
                       static_cast<u32>(cube.vertices.size()),
                       cube.indices.data(),
                       static_cast<u32>(cube.indices.size())));
    const Handle h = scene.create("cubo");
    Transform3D* tr = scene.get(h)->addComponent<Transform3D>();
    MeshRenderer* mr = scene.get(h)->addComponent<MeshRenderer>();
    ASSERT(tr != nullptr && mr != nullptr);
    mr->mesh = &mesh;

    Renderer r;
    ASSERT(r.init());
    UiContext ui;
    ui.init();

    // frame de WARM-UP (a UI sobe o VBO 1× — bufferData conta a partir daqui)
    {
        ui.beginFrame(&r, nullptr, 1600.0f, 720.0f);
        const f32 white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        ui.panel(0.0f, 0.0f, 40.0f, 40.0f, white);
        ui.endFrame();
        r.endFrame();
    }
    const int genVa = glstub::stats.genVertexArrays;
    const int genBuf = glstub::stats.genBuffers;
    const int genTex = glstub::stats.genTextures;

    // 30 frames em steady state: drawTics-equivalente + pass UI
    for (u32 f = 0; f < 30; ++f) {
        r.beginFrame();
        const Mat4 vp = Mat4::identity();
        (void)r.drawMesh(*mr->mesh, tr->world, vp);
        ui.beginFrame(&r, nullptr, 1600.0f, 720.0f);
        const f32 white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        ui.panel(0.0f, 0.0f, 40.0f, 40.0f, white);
        ui.endFrame();
        r.endFrame();
    }
    // ZERO objetos GL novos (o realloc-in-loop do C33 criava/destruía aqui)
    EXPECT(glstub::stats.genVertexArrays == genVa);
    EXPECT(glstub::stats.genBuffers == genBuf);
    EXPECT(glstub::stats.genTextures == genTex);
    EXPECT(glstub::stats.deleteVertexArrays == 0);
    EXPECT(glstub::stats.deleteBuffers == 0);
    EXPECT(glstub::stats.drawElementsCalls == 30);   // 1 mesh × 30 frames
    r.shutdown();
}

TEST(wiring084_frame_time_do_resolver_50_elementos_limitado) {
    // "frame time estável ao adicionar elementos": o resolver do canvas de
    // 50 elementos × 600 frames (10 s @ 60 fps) tem de caber numa janela
    // generosa (50 ms) — catches O(n²) que cresceriam com a cena.
    UiCanvas c;
    fillCanvas(c, 50);
    safe::Insets ins;
    const auto t0 = std::chrono::steady_clock::now();
    ui::CanvasLayout lay[32];
    for (u32 f = 0; f < 600; ++f) {
        ui::resolveCanvasLayout(c, 1600.0f, 720.0f, ins, lay, 32u);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    EXPECT(ms < 50.0);
}

// ---- 5. header da timeline: sem sobreposição a QUALQUER largura -------------

TEST(wiring084_header_timeline_sem_sobreposicao_do_c33_a_720px) {
    // O C33 (1600×720 landscape, 2 painéis de 300) dá ~1000 px de strip;
    // com insets/nav bar pode descer. Os offsets fixos da 0.8.0 sobrepunham
    // "clip:" ↔ play quando r.w < 988 — dois widgets a lutar pelo toque.
    const f32 widths[] = {1600.0f, 1000.0f, 988.0f, 952.0f, 800.0f, 720.0f,
                          680.0f};
    for (const f32 w : widths) {
        const UiRect r{0.0f, 0.0f, w, timeline::kTimelineH};
        const timeline::HeaderLayout hl = timeline::headerLayout(r);

        // nenhum par sobreposto (o bug do C33 era play ↔ clip)
        const UiRect* all[] = {&hl.play, &hl.stop, &hl.mode, &hl.slider,
                               &hl.add, &hl.clip};
        for (u32 i = 0; i < 6; ++i) {
            for (u32 j = i + 1; j < 6; ++j) {
                EXPECT(!overlap(*all[i], *all[j]));
            }
        }
        // todos DENTRO da strip
        for (const UiRect* q : all) {
            EXPECT(q->x >= r.x);
            EXPECT(q->x + q->w <= r.x + r.w + 0.01f);
        }
        // o título nunca some (min ~120 px) e o clip continua clicável
        EXPECT(hl.title.w >= 120.0f);
        EXPECT(hl.clip.w >= 96.0f);
        // as alturas são as de sempre (header 36)
        EXPECT(nearEqF(hl.play.h, timeline::kHeaderH - 12.0f));
    }
    // largura de referência: o cluster direito fica EXATAMENTE onde estava
    const UiRect r{0.0f, 0.0f, 1600.0f, timeline::kTimelineH};
    const timeline::HeaderLayout hl = timeline::headerLayout(r);
    EXPECT(nearEqF(hl.play.x, 1600.0f - 436.0f));
    EXPECT(nearEqF(hl.stop.x, 1600.0f - 348.0f));
    EXPECT(nearEqF(hl.mode.x, 1600.0f - 280.0f));
    EXPECT(nearEqF(hl.slider.x, 1600.0f - 164.0f));
    EXPECT(nearEqF(hl.add.x, 1600.0f - 60.0f));
    EXPECT(nearEqF(hl.clip.x, 1600.0f - 436.0f - 8.0f - 176.0f));
}

TEST(wiring084_header_timeline_no_c33_os_dois_botoes_que_lutavam_separam_se) {
    // O caso CONCRETO do C33: 952 px de strip (1600 − 600 painéis − insets)
    // — antes: play em x=516 vs clip em 376..552 SOBREPUNHAM 36 px.
    const UiRect r{0.0f, 0.0f, 952.0f, timeline::kTimelineH};
    const timeline::HeaderLayout hl = timeline::headerLayout(r);
    // clip termina ANTES do play começar (folga ≥ 8)
    EXPECT(hl.clip.x + hl.clip.w <= hl.play.x);
    EXPECT(hl.play.x - (hl.clip.x + hl.clip.w) >= 8.0f);
}
