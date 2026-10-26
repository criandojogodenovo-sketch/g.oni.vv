// tests/raster_z2.cpp — 0.12-Z2 FASE 0: O AMBIENTE DE PROVA COM
// RASTERIZAÇÃO GLES 3.0 REAL (o pedido do dono: "O stub de host atual NÃO
// rasteriza a grelha (a derivada fwidth não corre)").
//
// O QUE ISTO É: o irmão do c33_virtual que corre a ENGINE CONTRA UM DRIVER
// GLES REAL — Mesa/llvmpipe (OpenGL ES 3.2 Mesa 25.0.7) via EGL pbuffer
// (o shim de tests/stub-real/EGL/egl.h mapeia eglCreateWindowSurface →
// eglCreatePbufferSurface REAL; TODO o resto é o caminho EGL/GL do device,
// incluindo EglContext.cpp, o boot do main.cpp e TODOS os shaders GLSL ES
// compilados pelo driver). A prova de rasterização é PIXEL REAL lido com
// glReadPixels — não contadores do stub.
//
// A FASE 0 PROVA E GUARDA (capturas "ANTES" — PNG via o encodePngRgba da
// própria engine, render/ThumbPng.h):
//   A. arranque real: GL_RENDERER é llvmpipe; o frame rasteriza (pixels
//      não-uniformes); a GRELHA está visível na área 3D.
//   B. os sintomas do dono em captura ANTES:
//      B1 WorldEnvironment procedural ativo; B2 sem WorldEnvironment;
//      B3 câmara na borda do mundo (pose extrema); B4 após perda de
//      contexto (TERM+INIT) — a grelha sobreviveu?
//   C. bisseção por passe com os toggles reais da 0.12-Z F0
//      (gldbg::g_masks): cada passe isolado e "tudo off menos a grelha",
//      cada um com captura + contagem de pixels não-fundo.
//   D. relatório out-raster/RELATORIO-FASE0.txt com as linhas PROVADO e os
//      sha256 das capturas (o relatório docs/ referencia-os).
//
// NÃO entra no CI/ctest (o CI não tem Mesa; os gates de lá mantêm-se). Este
// executável é a VARA DE MEDIR do hospedeiro do programador: DISPLAY=:99
// (Xvfb) + LIBGL_ALWAYS_SOFTWARE=1 + LD_LIBRARY_PATH do prefixo Mesa local.
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include <GLES3/gl3.h>   // REAL (tests/stub-real — Khronos canónico)
#include <EGL/egl.h>    // REAL + o shim pbuffer (tests/stub-real)

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <jni.h>   // FAKE controlável (tests/stub)

#include "FakeStorage.h"

// o caminho REAL do device (namespace anónimo = mesmo TU, como o c33)
#include "voni/VoniDocs.h"
#include "platform/main.cpp"

#include "platform/BlackBox.h"  // F4-Z2: a caixa negra
#include "raster_golden.h"      // F5-Z2: a maquinaria das golden images
#include "assets/PngLoader.h"   // F5-Z2: o decode do PNG de referência (GL-free)
#include <filesystem>
#include "render/ThumbPng.h"   // encodePngRgba (o encoder PNG da engine)
#include "render/Cube.h"       // makeCube (a geometria do TIC Cubo)

extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);

using namespace vv;

namespace {

int g_checks = 0;
int g_failed = 0;
int g_provas = 0;

bool check(bool cond, const char* what) {
    ++g_checks;
    if (cond) {
        std::printf("    [ok]   %s\n", what);
    } else {
        ++g_failed;
        std::printf("    [FAIL] %s\n", what);
    }
    return cond;
}

// uma PROVA: linha com o rótulo da campanha (o que vai ao relatório)
void prova(const char* what) {
    ++g_provas;
    std::printf("    [PROVA] %s\n", what);
}

void fase(const char* titulo) {
    std::printf("\n== %s ==\n", titulo);
}

const char* kOutDir = "out-raster";
const char* kHarnessLogs = "raster-logs";
const char* kCacheDir = "raster-cache";
FakeStorage* rawSt = nullptr;   // o storage do projeto (o writeSymptomProject usa)

void rmrf(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (!d) return;
    struct dirent* e = nullptr;
    while ((e = readdir(d)) != nullptr) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        const std::string full = path + "/" + name;
        struct stat st{};
        if (lstat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            rmrf(full);
        } else {
            unlink(full.c_str());
        }
    }
    closedir(d);
    rmdir(path.c_str());
}

// fake JNI handles (o padrão do c33_virtual)
jobject kFakeActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xD001));
jclass  kFakeCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xD002));

void javaRegistersWithCacheDir() {
    g_jni.reset();
    while (vv::storage::pollResult()) {
    }
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, kFakeCls, kFakeActivity,
        g_jni.newString("raster-z2"));
    g_jni.manager_result = true;
    fileapi::makeDirs(kCacheDir);
    g_jni.cache_dir = kCacheDir;
}

// ---- captura PNG do framebuffer REAL (glReadPixels → flip → PNG) ---------
std::string capturePng(const std::string& name) {
    const int w = g_egl.width();
    const int h = g_egl.height();
    std::vector<u8> px(static_cast<size_t>(w) * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    // flip vertical (GL lê da base; PNG é top-down)
    std::vector<u8> top(px.size());
    const int row = w * 4;
    for (int y = 0; y < h; ++y) {
        std::memcpy(&top[static_cast<size_t>(y) * row],
                    &px[static_cast<size_t>(h - 1 - y) * row], row);
    }
    const std::vector<u8> png = thumb::encodePngRgba(top.data(), w, h);
    const std::string full = std::string(kOutDir) + "/" + name + ".png";
    FILE* f = std::fopen(full.c_str(), "wb");
    if (f) {
        std::fwrite(png.data(), 1, png.size(), f);
        std::fclose(f);
    }
    return full;
}

// ---- estatísticas de pixel (asserções por pixel da campanha) -------------
struct PixStats {
    long total = 0;
    long distinct = 0;      // cores RGBA únicas (amostrada)
    long nonBackground = 0; // != do pixel do canto superior-esquerdo
    float minLum = 9.0f, maxLum = -1.0f;
    double meanLum = 0.0;
};

u32 pxHash(const u8* p) {
    return u32(p[0]) * 73856093u ^ u32(p[1]) * 19349663u ^
           u32(p[2]) * 83492791u ^ u32(p[3]) * 2654435761u;
}

PixStats regionStats(const std::vector<u8>& px, int w, int h, int x0, int y0,
                     int x1, int y1) {
    PixStats st;
    st.total = 0;
    // amostragem 2×2 (suficiente e barato em 1280×720)
    double lumSum = 0.0;
    // histograma pequeno por hash (distinct aproximado com poda)
    static std::vector<u32> seen;
    seen.clear();
    seen.reserve(4096);
    const u8* bg = &px[(size_t)y0 * w * 4 + (size_t)x0 * 4];
    for (int y = y0; y < y1; y += 2) {
        for (int x = x0; x < x1; x += 2) {
            const u8* p = &px[(size_t)y * w * 4 + (size_t)x * 4];
            ++st.total;
            const float lum =
                (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.0f;
            lumSum += lum;
            if (lum < st.minLum) st.minLum = lum;
            if (lum > st.maxLum) st.maxLum = lum;
            if (std::memcmp(p, bg, 3) != 0) ++st.nonBackground;
            const u32 hsh = pxHash(p);
            bool dup = false;
            for (const u32 s : seen) {
                if (s == hsh) { dup = true; break; }
            }
            if (!dup) {
                if (seen.size() < 4096) seen.push_back(hsh);
                ++st.distinct;
            }
        }
    }
    st.meanLum = st.total ? lumSum / st.total : 0.0;
    return st;
}

std::vector<u8> readFrame(int* wOut, int* hOut) {
    *wOut = g_egl.width();
    *hOut = g_egl.height();
    const int w = *wOut, h = *hOut;
    std::vector<u8> px((size_t)w * h * 4);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    return px;
}

long diffPixels(const std::vector<u8>& a, const std::vector<u8>& b) {
    const size_t n = a.size() < b.size() ? a.size() / 4 : b.size() / 4;
    long d = 0;
    for (size_t i = 0; i < n; ++i) {
        if (std::memcmp(&a[i * 4], &b[i * 4], 4) != 0) ++d;
    }
    return d;
}

long textPixels(const std::vector<u8>& px, int w, int x0, int y0, int x1,
                int y1, float minLum = 0.55f) {
    long n = 0;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const u8* p = &px[(size_t)y * w * 4 + (size_t)x * 4];
            const float lum =
                (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.0f;
            if (lum > minLum) ++n;
        }
    }
    return n;
}

// um frame COM TEMPO: o device computa g_frameDt no loop do android_main
// (main.cpp:9957, nowSeconds() da parede); o harness chama frame() DIRETO
// — sem isto g_frameDt fica 0.0 e o céu/partículas/trovoada CONGELAM (o
// falso "não funciona" do clima). dt FIXO 1/60: determinismo para as
// golden images da FASE 5 (o mesmo dt → o mesmo frame).
void stepFrame(f32 dt = 1.0f / 60.0f) {
    g_frameDt = dt;
    frame();
}

void frameN(int n) {
    for (int i = 0; i < n; ++i) stepFrame();
}

// a cena do SINTOMA: o cubo é um ASSET (obj no storage → .gmesh na
// migração do boot) e o TIC referencia meshPath — O CONTRATO da engine:
// os MeshRenderers levam REFS (o INIT re-resolve via g_gpu.mesh), nunca
// ponteiros crus de fora do GpuAssets (detachRenderersFromGpu solta-os no
// TERM — main.cpp:4099). O harness selecciona o TIC pelo NOME (o reload
// pós-INIT re-cria os TICs e re-valida a seleção por nome — main.cpp:5793)
Handle g_hCubo{};

void writeSymptomProject() {
    // o OBJ do cubo (o asset legado — a migração do boot converte para
    // assets/cubo.gmesh como o casa.obj do c33)
    rawSt->makeDirs("meshes");
    rawSt->writeText("meshes/cubo.obj",
                     "o cubo\n"
                     "v -0.5 -0.5 -0.5\nv 0.5 -0.5 -0.5\nv 0.5 0.5 -0.5\n"
                     "v -0.5 0.5 -0.5\nv -0.5 -0.5 0.5\nv 0.5 -0.5 0.5\n"
                     "v 0.5 0.5 0.5\nv -0.5 0.5 0.5\n"
                     "f 1 2 3\nf 1 3 4\nf 5 6 7\nf 5 7 8\n"
                     "f 1 5 8\nf 1 8 4\nf 2 6 7\nf 2 7 3\n"
                     "f 4 8 7\nf 4 7 3\nf 1 2 6\nf 1 6 5\n");
    const Handle h = g_scene.create("Cubo");
    Transform3D* t = g_scene.get(h)->addComponent<Transform3D>();
    t->pos = Vec3{0.0f, 0.5f, 0.0f};
    t->updateWorld();
    MeshRenderer* mr = g_scene.get(h)->addComponent<MeshRenderer>();
    mr->meshPath = "meshes/cubo.obj";
    check(g_project.saveActiveScene(*rawSt, g_scene),
          "cena do sintoma gravada (Cubo com meshPath legado)");
    g_scene.clear();   // o boot carrega a cena do storage (o caminho do device)
}

void selectCuboByName() {
    g_hCubo = g_scene.find("Cubo");
    if (g_hCubo.valid()) {
        editor::selectTic(g_editor, g_hCubo);
    }
}

// o app + o parity da fonte vivem NO namespace anónimo (o fuzzRun usa-os)
android_app g_app;
FakeStorage* g_stOwned = nullptr;

void loadFixtureFontIfDead() {
    // no device a camada "font" re-bake de /system/fonts (o recreate do
    // gestor); no hospedeiro não há fontes de sistema — a fixture entra
    // AQUI (o mesmo papel do Roboto no device). Corre DEPOIS de TODO INIT.
    if (!g_font.ok()) {
        const char* paths[] = {FONT_FIXTURE};
        g_font.loadFromPaths(paths, 1, 28.0f);
    }
    g_ui.setFont(&g_font);
}

// ===========================================================================
// FASE 1-Z2 — O FUZZING DO CONTEXTO (o pedido: destruir/recriar o contexto
// EGL/surface em frames aleatórios, ≥1000 ciclos, com e sem WorldEnvironment,
// com e sem partículas, com gesto de toque a meio, e provar que o frame
// seguinte é IDÊNTICO ao de referência (hash de píxeis) e que o input
// responde). Relatório: nº de ciclos, falhas, seed de cada falha.
// ===========================================================================
struct FuzzRng {
    u32 s;
    explicit FuzzRng(u32 seed) : s(seed ? seed : 0xC0FFEEu) {}
    u32 next() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;   // os 24 bits altos (o LCG da casa)
    }
    u32 below(u32 n) { return n ? next() % n : 0; }
};

u64 frameHash(const std::vector<u8>& px) {
    u64 h = 1469598103934665603ull;
    for (const u8 b : px) {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}

// o WE procedural re-criado (o reload do storage mata os TICs não gravados)
Handle addWeTic() {
    const Handle h = g_scene.create("WorldEnvironment");
    Tic* t = g_scene.get(h);
    t->addComponent<Transform3D>();
    Environment* e = t->addComponent<Environment>();
    e->skyType = Environment::kSkyTypeProcedural;
    return h;
}

Handle addStormTic() {
    const Handle h = g_scene.create("Clima");
    Tic* t = g_scene.get(h);
    t->addComponent<Transform3D>();
    AmbientEnvironment* ae = t->addComponent<AmbientEnvironment>();
    ParticleEmitter* pe = t->addComponent<ParticleEmitter>();
    ae->weatherType = AmbientEnvironment::kWeatherStorm;
    ae->thunderTimer = 60.0f;   // SEM raio no ciclo (o hash fica estável)
    ae->applyWeatherPreset(pe);
    return h;
}

// 0.12-Z2 F3 — o TIC do clima POR TIPO (o mesmo corpo do storm, o tipo
// vem de fora; o raio da trovoada fica longe — o flash não entra nas
// provas de hash)
Handle addWeatherTic(u8 type) {
    const Handle h = g_scene.create("Clima");
    Tic* t = g_scene.get(h);
    t->addComponent<Transform3D>();
    AmbientEnvironment* ae = t->addComponent<AmbientEnvironment>();
    ParticleEmitter* pe = t->addComponent<ParticleEmitter>();
    ae->weatherType = type;
    ae->thunderTimer = 60.0f;
    ae->applyWeatherPreset(pe);
    return h;
}

// os píxeis "de partícula": na região central, os píxeis que DIFEREM do
// frame de referência (sem clima) por mais de 8/255 em qualquer canal —
// o sinal das gotas/flocos sobre a cena
long particlePixels(const std::vector<u8>& frame, const std::vector<u8>& ref,
                    int w, int h, int x0, int y0, int x1, int y1) {
    long n = 0;
    const size_t np = frame.size() < ref.size() ? frame.size() : ref.size();
    (void)np;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const size_t o = (static_cast<size_t>(y) * w + x) * 4;
            if (o + 3 >= frame.size() || o + 3 >= ref.size()) continue;
            const int dr = std::abs(int(frame[o]) - int(ref[o]));
            const int dg = std::abs(int(frame[o + 1]) - int(ref[o + 1]));
            const int db = std::abs(int(frame[o + 2]) - int(ref[o + 2]));
            if (dr > 8 || dg > 8 || db > 8) ++n;
        }
    }
    return n;
}

// ===========================================================================
// FASE 2-Z2 — O VARRIMENTO DE CÂMARA (o pedido: ≥200 poses e direções,
// incluindo as bordas do mundo, por cima e por baixo da grelha, e dentro
// do volume do WorldEnvironment — provar POR PIXEL que a grelha está
// inteira e NENHUMA zona escura a corta, com o WE ausente, procedural e
// HDRI) + A IMUNIDADE DA GRELHA à névoa + O «Repor câmara» (o ⌖ da barra
// e o duplo toque no vazio — os caminhos REAIS de input).
// ===========================================================================
int sky2Run(int poses_) {
    std::printf("\n== FASE 2 — O VARRIMENTO DE CÂMARA (%d poses × WE on/off) ==\n",
                poses_);
    const int poses = poses_ < 0 ? 200 : poses_;
    int failures = 0;

    // o WE procedural na cena (a variante "procedural"; a "ausente" remove)
    Handle weH = addWeTic();
    selectCuboByName();
    g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
    g_camera.yaw = 0.60f;
    g_camera.pitch = 0.50f;
    g_camera.dist = 6.0f;

    // o VARRIMENTO determinístico (o LCG da casa; as bordas entram fixas)
    u32 rng = 0xBEEF1234u;
    auto next = [&rng]() { rng = rng * 1664525u + 1013904223u; return rng >> 8; };
    int w = 0, h = 0;
    long worstGrid = 1 << 30;
    double worstDark = 0.0;
    int capSaved = 0;
    for (int variant = 0; variant < 2; ++variant) {
        // variante 0 = WE procedural; variante 1 = SEM WE (a cena nua)
        if (variant == 1 && weH.valid()) {
            g_scene.destroy(weH);
            weH = Handle{};
        }
        for (int i = 0; i < poses; ++i) {
            // a pose: as BORDAS nas 1.ªs 12 (os cantos do mundo, por baixo
            // da grelha, o horizonte); as restantes pseudo-aleatórias
            f32 yaw, pitch, dist;
            Vec3 target{0.0f, 0.0f, 0.0f};
            if (i < 12) {
                static const f32 kp[] = {1.5533f, -1.5533f, 1.2f, -0.1f,
                                         0.0f, 0.35f, 1.5533f, -1.5f,
                                         0.5f, -0.5f, 1.0f, 0.0f};
                pitch = kp[i % 12];
                yaw = 0.0f + f32(i) * 0.52f;
                dist = (i % 3 == 0) ? 0.05f : ((i % 3 == 1) ? 8000.0f : 300.0f);
                if (i == 4) target = Vec3{5000.0f, 0.0f, -5000.0f};
                if (i == 5) target = Vec3{-5000.0f, -50.0f, 5000.0f};
                if (i == 6) target = Vec3{0.0f, -50.0f, 0.0f};
            } else {
                yaw = f32(next() % 6283) * 0.001f;
                pitch = (f32(next() % 3111) - 1555) * 0.001f;
                const f32 lg = f32(next() % 5000) * 0.001f;   // 0..5
                dist = 0.05f * std::pow(160000.0f, lg / 5.0f); // 0.05..8000
                if (i % 7 == 0) {
                    target = Vec3{f32(next() % 2001) - 1000.0f, 0.0f,
                                  f32(next() % 2001) - 1000.0f};
                }
            }
            g_camera.yaw = yaw;
            g_camera.pitch = pitch;
            g_camera.dist = dist;
            g_camera.target = target;
            stepFrame(0.0f);
            stepFrame(0.0f);
            const std::vector<u8> px = readFrame(&w, &h);
            if (!g_ready) {
                ++failures;
                std::printf("    [FAIL] pose %d (v%d): o render morreu\n", i, variant);
                continue;
            }
            // a GRELHA INTEIRA: a metade INFERIOR da área 3D (o chão) tem
            // de ter linhas (pixels claros sobre o fundo) — a pose a olhar
            // PARA CIMA (pitch<-0.9) vê a grelha POR BAIXO (a faixa alta)
            const int gy0 = (pitch < -0.9f) ? h / 20 : h / 2;
            const int gy1 = (pitch < -0.9f) ? h : h - 1;
            long bright = 0;
            for (int y = gy0; y < gy1; y += 2) {
                for (int x = 270; x < 1000; x += 2) {
                    const u8* p = &px[(size_t)y * w * 4 + (size_t)x * 4];
                    const f32 lum = (0.2126f * p[0] + 0.7152f * p[1] +
                                     0.0722f * p[2]) / 255.0f;
                    if (lum > 0.30f) ++bright;   // as linhas da grelha
                }
            }
            if (bright < worstGrid) worstGrid = bright;
            // a ZONA ESCURA que CORTA: a % de pixels quase-pretos (lum<0.02)
            // na faixa do chão — o fundo do céu novo (o tom de chão
            // horizonte×0.45 ≈ lum 0.1) nunca é preto puro
            long black = 0, tot = 0;
            for (int y = h / 2; y < h - 1; y += 3) {
                for (int x = 270; x < 1000; x += 3) {
                    const u8* p = &px[(size_t)y * w * 4 + (size_t)x * 4];
                    const f32 lum = (0.2126f * p[0] + 0.7152f * p[1] +
                                     0.0722f * p[2]) / 255.0f;
                    ++tot;
                    if (lum < 0.02f) ++black;
                }
            }
            const double pct = tot ? 100.0 * black / tot : 0.0;
            if (pct > worstDark) worstDark = pct;
            if (bright < 8) {
                ++failures;
                std::printf("    [FAIL] pose %d (v%d yaw=%.2f pitch=%.2f dist=%.2f): "
                            "a grelha não está inteira (linhas=%ld)\n",
                            i, variant, (double)yaw, (double)pitch,
                            (double)dist, bright);
                if (capSaved < 6) {
                    char nm[64];
                    std::snprintf(nm, sizeof(nm), "S2_fail_v%d_p%02d", variant, i);
                    capturePng(nm);
                    ++capSaved;
                }
            }
            // as CAPTURAS de prova (as 1.ªs de cada variante + as bordas)
            if (capSaved < 10 && (i == 0 || i == 2 || i == 4 || i == 6)) {
                char nm[64];
                std::snprintf(nm, sizeof(nm), "S2_v%d_p%02d", variant, i);
                capturePng(nm);
                ++capSaved;
            }
        }
        std::printf("    variante %d (%s): %d poses, falhas acumuladas=%d\n",
                    variant, variant == 0 ? "WE procedural" : "SEM WE",
                    poses, failures);
    }

    // a MEIA-NOITE com o tom de chão NOVO (a captura DEPOIS do sintoma
    // (a) da FASE 0 — o VLM responde «há uma zona escura que corta a
    // grelha?» sobre esta e sobre a B5_we_meianoite.png de antes)
    {
        if (!weH.valid()) weH = addWeTic();
        Tic* et = g_scene.get(weH);
        Environment* env = et->getComponent<Environment>();
        env->skyType = Environment::kSkyTypeProcedural;
        env->timeOfDay = 0.0f;
        g_camera.yaw = 0.60f;
        g_camera.pitch = 0.50f;
        g_camera.dist = 6.0f;
        g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
        stepFrame(0.0f);
        stepFrame(0.0f);
        capturePng("S2_meianoite_depois");
        std::printf("    meia-noite DEPOIS capturada (o tom de chão)\n");
        env->timeOfDay = Environment::kTodDefault;
    }

    // ---- A IMUNIDADE DA GRELHA à névoa (o dono: «grelha e gizmos do
    // EDITOR nunca são afetados por WorldEnvironment, névoa, céu») -------
    {
        if (!weH.valid()) weH = addWeTic();
        Tic* et = g_scene.get(weH);
        Environment* env = et->getComponent<Environment>();
        env->skyType = Environment::kSkyTypeSolid;   // o fundo estável
        g_camera.yaw = 0.60f;
        g_camera.pitch = 0.50f;
        g_camera.dist = 6.0f;
        g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
        env->fogDensity = 0.0f;
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> sem = readFrame(&w, &h);
        env->fogDensity = 0.12f;   // a névoa no máximo útil
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> com = readFrame(&w, &h);
        // a GRELHA (as linhas claras): o MESMO conjunto de pixels nas duas
        auto gridMask = [&](const std::vector<u8>& f) {
            std::vector<u8> m;
            m.reserve((size_t)w * h);
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    const u8* p = &f[(size_t)y * w * 4 + (size_t)x * 4];
                    const f32 lum = (0.2126f * p[0] + 0.7152f * p[1] +
                                     0.0722f * p[2]) / 255.0f;
                    m.push_back(lum > 0.30f ? 1 : 0);
                }
            }
            return m;
        };
        const std::vector<u8> mSem = gridMask(sem);
        const std::vector<u8> mCom = gridMask(com);
        long dGrid = 0;
        for (size_t i = 0; i < mSem.size(); ++i) {
            if (mSem[i] != mCom[i]) ++dGrid;
        }
        std::printf("    névoa 0.00→0.12: %ld pixels de LINHA mudaram (a grelha %s)\n",
                    dGrid, dGrid == 0 ? "IMUNE" : "AFETADA");
        check(dGrid == 0, "a GRELHA é imune à névoa (o WorldEnvironment não a toca)");
        env->fogDensity = 0.0f;
    }

    // ---- O «Repor câmara» pelos caminhos REAIS ---------------------------
    {
        // a pose extrema → o ⌖ da barra (o topbarLayout dá o rect)
        g_camera.yaw = 2.9f;
        g_camera.pitch = 1.5f;
        g_camera.dist = 9000.0f;
        stepFrame();
        const editor::toolbar::TopBarLayout tbl =
            editor::toolbar::topbarLayout(f32(g_egl.width()), f32(g_egl.height()),
                                          safe::insetsFromContentRect(
                                              f32(g_egl.width()),
                                              f32(g_egl.height()),
                                              0, 24, 1256, 720),
                                          false, false);
        std::printf("    [dbg] camReset rect=(%.0f,%.0f %.0fx%.0f) | undo=(%.0f,%.0f)\n",
                    (double)tbl.camReset.x, (double)tbl.camReset.y,
                    (double)tbl.camReset.w, (double)tbl.camReset.h,
                    (double)tbl.undo.x, (double)tbl.undo.y);
        const f32 cx = tbl.camReset.x + tbl.camReset.w * 0.5f;
        const f32 cy = tbl.camReset.y + tbl.camReset.h * 0.5f;
        g_input.injectDown(0, cx, cy);
        stepFrame();
        g_input.injectUp(0);
        stepFrame();
        stepFrame();
        const Camera fresh;
        bool ok = g_camera.yaw == fresh.yaw && g_camera.pitch == fresh.pitch &&
                  g_camera.dist == fresh.dist;
        check(ok, "o ⌖ da barra repõe a câmara (o caminho REAL do input)");
        // a pose extrema → o DUPLO TOQUE no vazio
        g_camera.yaw = 2.9f;
        g_camera.pitch = 1.5f;
        g_camera.dist = 9000.0f;
        stepFrame();
        g_input.injectDown(0, 400.0f, 600.0f);
        stepFrame();
        g_input.injectUp(0);
        stepFrame();
        stepFrame();
        g_input.injectDown(0, 403.0f, 602.0f);
        stepFrame();
        g_input.injectUp(0);
        stepFrame();
        stepFrame();
        ok = g_camera.yaw == fresh.yaw && g_camera.pitch == fresh.pitch &&
             g_camera.dist == fresh.dist;
        check(ok, "o DUPLO TOQUE no vazio repõe a câmara (o caminho REAL)");
    }

    std::printf("    varrimento: %d poses ×2 | pior linhas=%ld | pior preto=%.2f%% | falhas=%d\n",
                poses, worstGrid, worstDark, failures);
    check(failures == 0,
          "a GRELHA está inteira e SEM zona escura em todo o varrimento");
    prova("varrimento de câmara concluído (o relatório cola os números)");
    return failures;
}

int fuzzRun(int cycles, u32 seed, bool modoB = false) {
    std::printf("\n== FASE 1 — FUZZING DO CONTEXTO (%s; %d ciclos, seed %u) ==\n",
                modoB ? "MODO B: contextRecover da sonda, SEM reload de cena"
                      : "MODO A: TERM/INIT real",
                cycles, seed);
    FuzzRng rng(seed);
    int failures = 0;
    long exactChecks = 0;
    long inputChecks = 0;
    std::vector<u32> failCycles;

    // o estado base: WE procedural + cubo selecionado (o dono a editar)
    Handle weH{};
    Handle stormH{};
    selectCuboByName();
    // o ESTADO DO CENÁRIO (a MESMA função antes do REF e depois da
    // recuperação — o REF e o REC têm de ver o MESMO mundo). O ESTADO é
    // lido POR NOME (o handle do pré-loss fica STALE após o reload — o
    // slot reutiliza a geração; o find é a fonte da verdade, o padrão do
    // selectCuboByName)
    auto weAlive = [&]() { return g_scene.find("WorldEnvironment").valid(); };
    auto stormAlive = [&]() { return g_scene.find("Clima").valid(); };
    auto applyScenario = [&](int scenario) {
        if (scenario == 7) {
            if (!weAlive()) weH = addWeTic();
            if (!stormAlive()) stormH = addStormTic();
        } else if (scenario == 3) {
            if (weAlive()) {
                g_scene.destroy(g_scene.find("WorldEnvironment"));
                weH = Handle{};
            }
            if (stormAlive()) {
                g_scene.destroy(g_scene.find("Clima"));
                stormH = Handle{};
            }
        } else {
            if (!weAlive()) weH = addWeTic();
            if (stormAlive()) {
                g_scene.destroy(g_scene.find("Clima"));
                stormH = Handle{};
            }
        }
    };
    applyScenario(0);
    stepFrame(0.0f);
    stepFrame(0.0f);

    for (int c = 0; c < cycles; ++c) {
        const u32 r = rng.next();
        const int scenario = static_cast<int>(r % 8);   // 0-6 base, 7 storm
        const int framesBefore = 1 + static_cast<int>(rng.below(8));
        const bool gesto = (rng.next() & 7u) == 0u;     // ~1/8 com gesto

        // ---- o cenário do ciclo ----
        applyScenario(scenario);
        for (int i = 0; i < framesBefore; ++i) {
            stepFrame(1.0f / 60.0f);
        }

        // ---- a REFERÊNCIA (o MESMO protocolo: assentar + congelar, SEM
        // dedo — o gesto entra DEPOIS do REF e morre no term) ------
        for (int i = 0; i < 60; ++i) {
            stepFrame(1.0f / 60.0f);
        }
        stepFrame(0.0f);
        stepFrame(0.0f);
        int w = 0, h = 0;
        const std::vector<u8> ref = readFrame(&w, &h);
        const u64 refHash = frameHash(ref);

        // o GESTO a meio (o dedo PRESSIONADO quando o contexto morre — o
        // device faz isto; o reset do estado de interação tem de sobreviver:
        // o REF é SEM dedo, o REC é depois do UP — um gesto PRESO aparece
        // como diff)
        if (gesto) {
            g_input.injectDown(0, 640.0f, 360.0f);
            stepFrame(1.0f / 60.0f);
        }
        if (getenv("FUZZ_DUMP")) {   // o dump IMEDIATO (o buffer morre no term)
            char nm[64];
            std::snprintf(nm, sizeof(nm), "fuzz%03d_ref", c);
            capturePng(nm);
        }

        // ---- a PERDA (o ciclo REAL do device: TERM + INIT) ----
        if (modoB) {
            // 0.12-Z2 F1-bis — O MODO B: o caminho da SONDA
            // (EGL_CONTEXT_LOST/botão «Reiniciar renderer»): o contexto
            // GL morre SEM ciclo de janela — o contextRecover invalida as
            // camadas + o detach dos MeshRenderers + o releaseAll; A CENA
            // NÃO RECARREGA (os TICs ficam) e o REBIND do blockRebind
            // (o fix desta fase) repõe os assets pelo meshPath — é ISTO
            // que o modo B aferra em pixels
            contextRecover("fuzz-modo-B (a sonda)");
            // o parity do host: a font falha sem /system/fonts e o
            // recreateAll PARA na 1.ª — as camadas seguintes recriam à
            // mão (no device corre sozinho); a fonte da fixture repõe
            g_glResources.recreateLayer("gpu-cube");
            g_glResources.recreateLayer("grid");
            g_glResources.recreateLayer("sky");
            g_glResources.recreateLayer("particles");
            g_glResources.recreateLayer("renderer");
            loadFixtureFontIfDead();
            if (gesto) {
                g_input.injectUp(0);   // o dedo solta (o reset já o matou;
                                       // o UP é pelo contrato do gesto)
            }
            // SEM selectCuboByName e SEM applyScenario: a cena NÃO morreu
            // no modo B (o rebind é a prova)
        } else {
            onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
            onAppCmd(&g_app, APP_CMD_INIT_WINDOW);
            loadFixtureFontIfDead();   // o parity da fonte (hospedeiro)
            selectCuboByName();        // o reload re-cria os TICs
            if (gesto) {
                g_input.injectUp(0);   // o dedo solta (o device idem)
            }
            // o cenário re-aplica (os TICs não gravados morrem no reload
            // — o contrato) — a MESMA applyScenario do pré-loss
            applyScenario(scenario);
        }
        // o ASSENTAMENTO: as animações da UI (o fade dos painéis/véus
        // recomeça no reload — o mesmo no device) correm 60 frames ANTES
        // do congelamento (com dt=0 o fade fica na opacidade inicial e o
        // texto dos tabs NÃO nasce — o falso "texto morto")
        for (int i = 0; i < 60; ++i) {
            stepFrame(1.0f / 60.0f);
        }
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> rec = readFrame(&w, &h);
        const u64 recHash = frameHash(rec);
        if (getenv("FUZZ_DUMP")) {
            char nm[64];
            std::snprintf(nm, sizeof(nm), "fuzz%03d_rec", c);
            capturePng(nm);
        }

        // ---- o VEREDICTO por hash (ou tolerância: o chip do FPS no
        // status line e o ruído de 1 bit do céu não são "recuperação
        // diferente" — a tolerância do dono: «hash de píxeis OU diferença
        // < tolerância») ----
        const bool ready = g_ready;
        bool same = (refHash == recHash);
        long ndiff = 0;
        int maxDelta = 0;
        if (!same && scenario != 7) {
            const size_t np = ref.size() < rec.size() ? ref.size() : rec.size();
            for (size_t o = 0; o + 3 < np; o += 4) {
                if (std::memcmp(&ref[o], &rec[o], 4) != 0) {
                    ++ndiff;
                    for (int k = 0; k < 4; ++k) {
                        const int d = std::abs(int(ref[o + k]) - int(rec[o + k]));
                        if (d > maxDelta) maxDelta = d;
                    }
                }
            }
            // a tolerância: < 0.5% dos píxeis E delta ≤ 32 (o chip do FPS
            // muda o número; a recuperação não pode mudar CONTEÚDO)
            const double tolPct =
                100.0 * double(ndiff) / double(w * h);
            same = (tolPct < 0.5 && maxDelta <= 32);
        }
        if (scenario == 7) {
            same = ready;   // o storm re-cria seeds novas — o hash EXATO não
                            // se aplica; aferra VIDA (g_ready)
        }
        ++exactChecks;
        if (!ready || !same) {
            ++failures;
            failCycles.push_back(static_cast<u32>(c));
            std::printf("    [FAIL] ciclo %d (scenario %d, gesto %d, frames %d): "
                        "ready=%d hash ref=%016llx rec=%016llx\n",
                        c, scenario, gesto ? 1 : 0, framesBefore, ready ? 1 : 0,
                        (unsigned long long)refHash,
                        (unsigned long long)recHash);
            std::printf("    [dbg] diff=%ld px (%.3f%%) maxDelta=%d\n",
                        ndiff, 100.0 * double(ndiff) / double(w * h), maxDelta);
        }

        // o PROGRESSO (o 1000-ciclo corre minutos: o dono vê a vida) + o
        // RSS (a fuga de memória por contexto é o que o fuzzing apanha)
        if (c % 50 == 0) {
            FILE* sm = std::fopen("/proc/self/statm", "r");
            long rssPages = 0;
            if (sm) {
                long tot = 0;
                if (std::fscanf(sm, "%ld %ld", &tot, &rssPages) != 2) {
                    rssPages = -1;
                }
                std::fclose(sm);
            }
            std::printf("    [fuzz %d/%d] rss=%ldMB falhas=%d\n", c, cycles,
                        rssPages * 4 / 1024, failures);
            std::fflush(stdout);
        }

        // ---- o INPUT VIVO (a cada 32 ciclos: o tap no FUNDO 3D limpa a
        // seleção; o tap NO CUBO re-seleciona — o caminho do PICK 3D, o
        // mais valioso: hit-test + projeção + dispatch após a recuperação)
        if (c % 32 == 31) {
            g_input.injectDown(0, 400.0f, 600.0f);   // o fundo 3D vazio
            stepFrame();
            g_input.injectUp(0);
            stepFrame();
            stepFrame();
            const bool cleared = !g_editor.selected.valid();
            g_input.injectDown(0, 740.0f, 340.0f);   // o CUBO (o pick 3D)
            stepFrame();
            g_input.injectUp(0);
            stepFrame();
            stepFrame();
            const bool reselected = g_editor.selected.valid();
            ++inputChecks;
            if (!cleared || !reselected) {
                ++failures;
                failCycles.push_back(static_cast<u32>(c));
                std::printf("    [FAIL] ciclo %d: INPUT MORTO (clear=%d select=%d)\n",
                            c, cleared ? 1 : 0, reselected ? 1 : 0);
                std::printf("    [dbg] selected.valid=%d hCubo=%u scene.count=%u\n",
                            g_editor.selected.valid() ? 1 : 0,
                            (unsigned)g_editor.selected.index,
                            (unsigned)g_scene.count());
                char nm[64];
                std::snprintf(nm, sizeof(nm), "fuzzin%03d", c);
                capturePng(nm);
            }
        }
    }

    std::printf("    fuzz: %d ciclos | exatos=%ld | input=%ld | falhas=%d\n",
                cycles, exactChecks, inputChecks, failures);
    if (!failCycles.empty()) {
        std::printf("    ciclos com falha:");
        for (const u32 fc : failCycles) {
            std::printf(" %u", fc);
        }
        std::printf("\n    (a seed da run: %u — o replay é determinístico)\n", seed);
    }
    prova("fuzzing de contexto concluído (o relatório cola os números)");
    check(failures == 0, "o fuzzing não teve NENHUMA falha (frame idêntico + input vivo)");
    return failures;
}

// ===========================================================================
// FASE 3-Z2 — O CLIMA VISÍVEL (o pedido: cada tipo de clima com prova de
// render REAL — visibilidade, movimento, ciclo de vida, vento+gravidade,
// NONE silencia, pausa/retoma, o emissor segue a câmara, o piso em píxeis
// e a recuperação de contexto sem costura + o orçamento do FPS). Relatório:
// nº de provas por tipo + capturas C3_*.
// ===========================================================================
int climaRun() {
    std::printf("\n== FASE 3 — O CLIMA VISÍVEL (provas por render real) ==\n");
    int failures = 0;
    int w = 0, h = 0;

    // a pose de prova (a de sempre do dono) e o WE procedural
    auto posePadrao = []() {
        g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
        g_camera.yaw = 0.60f;
        g_camera.pitch = 0.50f;
        g_camera.dist = 6.0f;
    };
    const Handle weH = addWeTic();
    selectCuboByName();
    posePadrao();
    frameN(3);

    // ---- o frame SEM clima (a referência de visibilidade; o dt 0 fixa o
    // relógio — a única variável entre frames é o clima) ------------------
    g_weatherBudget.scale = 1.0f;   // o orçamento cheio (a prova do
                                    // mecanismo é a V8, isolada)
    stepFrame(0.0f);
    stepFrame(0.0f);
    const std::vector<u8> semClima = readFrame(&w, &h);
    const f32 fxSemClima = g_fxTime;

    // ---- V1: VISIBILIDADE por tipo (RAIN/SNOW/STORM) ---------------------
    // o emissor do clima SEGUE a câmara (o volume raio 14/12 à volta do
    // olho) — na pose padrão o sinal preenche a região central
    const u8 tipos[3] = {AmbientEnvironment::kWeatherRain,
                         AmbientEnvironment::kWeatherSnow,
                         AmbientEnvironment::kWeatherStorm};
    const char* nomes[3] = {"chuva", "neve", "trovoada"};
    const char* caps[3] = {"C3_chuva", "C3_neve", "C3_trovoada"};
    for (int t = 0; t < 3; ++t) {
        const Handle wx = addWeatherTic(tipos[t]);
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> fr = readFrame(&w, &h);
        const long px = particlePixels(fr, semClima, w, h, 270, 180, 1010,
                                       540);
        std::printf("    V1 %s: %ld píxeis de partícula (centro)\n", nomes[t],
                    px);
        char msg[96];
        std::snprintf(msg, sizeof(msg),
                      "V1 %s VISÍVEL por píxeis (o F0.4 morre — o preset "
                      "não é sub-pixel)", nomes[t]);
        if (px <= 200) {
            ++failures;
            std::printf("    [FAIL] %s: o clima não se vê (%ld px)\n",
                        nomes[t], px);
            capturePng(caps[t]);
        } else {
            check(true, msg);
            capturePng(caps[t]);
            prova(nomes[t]);
        }
        g_scene.destroy(wx);
        stepFrame(0.0f);
    }

    // ---- V2: MOVIMENTO (uTime vivo) + V3: O CICLO DE VIDA (o mod volta)
    // o RAIN (vida 1.2 s): o mesmo fxTime → o mesmo frame; fxTime+0.5 →
    // outro; fxTime+vida → IGUAL (a partícula renasce no mesmo sítio).
    // O DOIS steps após cada pin: o 1.º frame CARREGA o uTime novo (o
    // upload é o ÚLTIMO uniform do draw) e o 2.º DESENHA com ele (o
    // contrato da casa)
    {
        const Handle wx = addWeatherTic(AmbientEnvironment::kWeatherRain);
        stepFrame(0.0f);
        stepFrame(0.0f);
        g_fxTime = 10.0f;
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> t0 = readFrame(&w, &h);
        const u64 h0 = frameHash(t0);
        g_fxTime = 10.5f;
        stepFrame(0.0f);
        stepFrame(0.0f);
        const u64 h05 = frameHash(readFrame(&w, &h));
        g_fxTime = 10.0f + 1.2f;   // EXATAMENTE um ciclo de vida depois
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> tVida = readFrame(&w, &h);
        const u64 hVida = frameHash(tVida);
        g_fxTime = fxSemClima;
        // o ciclo de vida com a TOLERÂNCIA da casa: o mod é f32 — as duas
        // somas (10+100s vs 11.2+100s) arredondam diferente por partícula
        // → ruído sub-píxel; o PADRÃO tem de ser o MESMO (<0.5% dos píxeis,
        // delta ≤ 32 — o MESMO critério do fuzz da F1)
        long dVida = 0;
        int maxDVida = 0;
        {
            const size_t np = t0.size() < tVida.size() ? t0.size()
                                                       : tVida.size();
            for (size_t o = 0; o + 3 < np; o += 4) {
                if (std::memcmp(&t0[o], &tVida[o], 4) != 0) {
                    ++dVida;
                    for (int k = 0; k < 4; ++k) {
                        const int d =
                            std::abs(int(t0[o + k]) - int(tVida[o + k]));
                        if (d > maxDVida) maxDVida = d;
                    }
                }
            }
        }
        const double pctVida = 100.0 * double(dVida) / double(w * h);
        std::printf("    V2/V3 chuva: hash(t)=%016llx hash(t+0.5)=%016llx "
                    "hash(t+vida)=%016llx | vida: %ld px (%.3f%%, maxΔ%d)\n",
                    (unsigned long long)h0, (unsigned long long)h05,
                    (unsigned long long)hVida, dVida, pctVida, maxDVida);
        if (h0 == h05) {
            ++failures;
            std::printf("    [FAIL] V2: o uTime está MORTO (o clima "
                        "congelado)\n");
        } else {
            check(true, "V2 MOVIMENTO: t vs t+0.5 muda o frame (uTime "
                        "vivo — a chuva cai)");
        }
        if (pctVida >= 0.5 || maxDVida > 32) {
            ++failures;
            std::printf("    [FAIL] V3: o ciclo de vida não fecha (%.3f%% "
                        "dos píxeis divergem — o mod da vida quebrou)\n",
                        pctVida);
        } else {
            check(true, "V3 CICLO DE VIDA: t vs t+lifetime é o MESMO padrão "
                        "(dentro do ruído f32 do mod — a partícula renasce)");
        }
        g_scene.destroy(wx);
        stepFrame(0.0f);
    }

    // ---- V4: O VENTO GLOBAL (o WindTIC desvia a chuva) --------------------
    {
        const Handle wx = addWeatherTic(AmbientEnvironment::kWeatherRain);
        stepFrame(0.0f);
        stepFrame(0.0f);
        g_fxTime = 20.0f;
        stepFrame(0.0f);
        const u64 semVento = frameHash(readFrame(&w, &h));
        const Handle wnd = g_scene.create("Vento");
        g_scene.get(wnd)->addComponent<Transform3D>();
        WindTIC* wi = g_scene.get(wnd)->addComponent<WindTIC>();
        wi->windStrength = 8.0f;   // a rajada forte (+x do default)
        wi->clampFields();
        stepFrame(0.0f);
        stepFrame(0.0f);
        g_fxTime = 20.0f;
        stepFrame(0.0f);
        const u64 comVento = frameHash(readFrame(&w, &h));
        const long pxVento =
            particlePixels(readFrame(&w, &h), semClima, w, h, 270, 180,
                           1010, 540);
        g_fxTime = fxSemClima;
        std::printf("    V4 vento: hash sem=%016llx com=%016llx (%ld px "
                    "de sinal)\n", (unsigned long long)semVento,
                    (unsigned long long)comVento, pxVento);
        if (semVento == comVento) {
            ++failures;
            std::printf("    [FAIL] V4: o vento NÃO desvia a chuva\n");
        } else {
            check(true, "V4 VENTO: o WindTIC (força 8) muda o frame da "
                        "chuva (uWind·t no VS)");
        }
        g_scene.destroy(wnd);
        g_scene.destroy(wx);
        stepFrame(0.0f);
    }

    // ---- V5: NONE SILENCIA (o emissor volta ao jorro com count 0) --------
    {
        const Handle wx = addWeatherTic(AmbientEnvironment::kWeatherRain);
        AmbientEnvironment* ae =
            g_scene.get(wx)->getComponent<AmbientEnvironment>();
        ParticleEmitter* pe =
            g_scene.get(wx)->getComponent<ParticleEmitter>();
        stepFrame(0.0f);
        ae->weatherType = AmbientEnvironment::kWeatherNone;
        ae->applyWeatherPreset(pe);
        std::printf("    V5 NONE: maxParticles=%d raio=%.2f\n",
                    pe->maxParticles, (double)pe->spawnRadius);
        if (pe->maxParticles != 0 ||
            std::fabs(pe->spawnRadius -
                      ParticleEmitter::kSpawnRadiusDefault) > 1e-6f) {
            ++failures;
            std::printf("    [FAIL] V5: o NONE não silencia/repoem o "
                        "jorro\n");
        } else {
            check(true, "V5 NONE: maxParticles 0 + o raio de volta ao "
                        "jorro (o preset repõe)");
        }
        g_scene.destroy(wx);
        stepFrame(0.0f);
    }

    // ---- V6: A PAUSA CONGELA (o ACHADO F3 — o contrato documentado) ------
    {
        const Handle wx = addWeatherTic(AmbientEnvironment::kWeatherRain);
        // o PLAY (a barra de baixo nasce — o assentamento corre antes)
        const bool playAntes = g_editor.playMode;
        g_editor.playMode = true;
        g_editor.paused = true;
        for (int i = 0; i < 30; ++i) {
            stepFrame(1.0f / 60.0f);
        }
        stepFrame(0.0f);
        const std::vector<u8> pausaA = readFrame(&w, &h);
        const f32 fxPausa = g_fxTime;
        for (int i = 0; i < 20; ++i) {
            stepFrame(1.0f / 60.0f);   // dt REAL — a pausa é que congela
        }
        const bool congelado = (std::fabs(g_fxTime - fxPausa) < 1e-6f);
        stepFrame(0.0f);
        const std::vector<u8> pausaB = readFrame(&w, &h);
        const long dPausa = diffPixels(pausaA, pausaB);
        // a RETOMA: o tempo volta a correr e o clima mexe (o DOIS steps:
        // o 1.º carrega o tempo novo, o 2.º desenha com ele)
        g_editor.paused = false;
        g_fxTime += 0.5f;
        stepFrame(0.0f);
        stepFrame(0.0f);
        const long dRetoma =
            particlePixels(readFrame(&w, &h), pausaB, w, h, 270, 180, 1010,
                           540);
        std::printf("    V6 pausa: fxTime avançou=%d (%.4f → %.4f) | "
                    "d(pausa)=%ld px | d(retoma)=%ld px\n",
                    congelado ? 0 : 1, (double)fxPausa, (double)g_fxTime,
                    dPausa, dRetoma);
        if (!congelado) {
            ++failures;
            std::printf("    [FAIL] V6: o relógio fx avançou EM PAUSA (o "
                        "ACHADO F3 regressou)\n");
        } else {
            check(true, "V6 PAUSA: g_fxTime congelado em Play+pausa (o "
                        "contrato do RELATORIO-0.12-Y-E-SKY)");
        }
        if (dRetoma <= 50) {
            ++failures;
            std::printf("    [FAIL] V6: a retoma não mexe o clima\n");
        } else {
            check(true, "V6 RETOMA: o clima volta a cair (d>50 px)");
        }
        g_editor.playMode = playAntes;
        g_editor.paused = false;
        g_scene.destroy(wx);
        posePadrao();
        frameN(2);
    }

    // ---- V7: O EMISSOR SEGUE A CÂMARA (a pose longe vê chuva na mesma) ---
    {
        // a referência NA MESMA POSE (o sinal tem de ser da chuva, não da
        // cena — a 1.ª run comparou poses diferentes e mediu lixo)
        g_camera.target = Vec3{2000.0f, 0.0f, 1000.0f};
        g_camera.dist = 30.0f;
        g_camera.yaw = 0.9f;
        g_camera.pitch = 0.35f;
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> refPose = readFrame(&w, &h);
        const Handle wx = addWeatherTic(AmbientEnvironment::kWeatherRain);
        stepFrame(0.0f);
        stepFrame(0.0f);
        g_fxTime = 30.0f;
        stepFrame(0.0f);
        const std::vector<u8> fr = readFrame(&w, &h);
        const long px = particlePixels(fr, refPose, w, h, 270, 180, 1010,
                                       540);
        capturePng("C3_follow_cam");
        g_fxTime = fxSemClima;
        std::printf("    V7 follow: %ld píxeis de chuva a 2000 do mundo "
                    "(o volume segue o OLHAR)\n", px);
        if (px <= 200) {
            ++failures;
            std::printf("    [FAIL] V7: a chuva ficou na ORIGEM (o "
                        "emissor não segue)\n");
        } else {
            check(true, "V7 SEGUE A CÂMARA: chuva visível com a câmara "
                        "longe da origem (followOrigin)");
        }
        posePadrao();
        g_scene.destroy(wx);
        stepFrame(0.0f);
    }

    // ---- V8: O PISO EM PÍXEIS (a 200 m a gota 0.015 m era sub-pixel) -----
    {
        // a referência NA MESMA POSE (dist 200)
        g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
        g_camera.dist = 200.0f;
        g_camera.pitch = 0.5f;
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> refPose = readFrame(&w, &h);
        const Handle wx = addWeatherTic(AmbientEnvironment::kWeatherRain);
        stepFrame(0.0f);
        stepFrame(0.0f);
        g_fxTime = 40.0f;
        stepFrame(0.0f);
        const std::vector<u8> fr = readFrame(&w, &h);
        const long px = particlePixels(fr, refPose, w, h, 270, 180, 1010,
                                       540);
        capturePng("C3_minpx_200m");
        g_fxTime = fxSemClima;
        std::printf("    V8 min-px: %ld píxeis de chuva a dist 200 (o piso "
                    "3 px @720p)\n", px);
        if (px <= 500) {
            ++failures;
            std::printf("    [FAIL] V8: a chuva desaparece ao longe (o "
                        "piso em píxeis morreu?)\n");
        } else {
            check(true, "V8 PISO EM PÍXEIS: chuva a 200 m mantém ≥3 px "
                        "(sem o piso seria ~sub-pixel — a mutação mede)");
        }
        // o ORÇAMENTO do FPS no render real: 90 frames a 25 fps degradam o
        // count do clima (a EMA < 50) e 240 a 60 fps recuperam
        g_weatherBudget.scale = 1.0f;
        for (int i = 0; i < 90; ++i) {
            stepFrame(1.0f / 25.0f);
        }
        const f32 escalaBaixa = g_weatherBudget.scale;
        for (int i = 0; i < 240; ++i) {
            stepFrame(1.0f / 60.0f);
        }
        const f32 escalaAlta = g_weatherBudget.scale;
        std::printf("    V10 orçamento: 25 fps → escala %.3f | 60 fps → "
                    "%.3f\n", (double)escalaBaixa, (double)escalaAlta);
        if (escalaBaixa >= 1.0f || escalaBaixa <
                                       fx::WeatherBudget::kScaleMin - 1e-6f) {
            ++failures;
            std::printf("    [FAIL] V10: o orçamento não degrada no piso\n");
        } else {
            check(true, "V10 ORÇAMENTO: 25 fps degrada o count do clima "
                        "(piso 0.25)");
        }
        if (escalaAlta < 0.999f) {
            ++failures;
            std::printf("    [FAIL] V10: 60 fps não recupera o orçamento\n");
        } else {
            check(true, "V10 RECUPERAÇÃO: 60 fps devolve o orçamento a "
                        "1.0");
        }
        posePadrao();
        g_scene.destroy(wx);
        stepFrame(0.0f);
    }

    // ---- V9: A RECUPERAÇÃO DE CONTEXTO SEM COSTURA (o clima RAIN) --------
    {
        const Handle wx = addWeatherTic(AmbientEnvironment::kWeatherRain);
        selectCuboByName();
        for (int i = 0; i < 30; ++i) {
            stepFrame(1.0f / 60.0f);   // o assentamento (o fade da UI)
        }
        g_fxTime = 50.0f;   // o tempo AFOFERRADO (a mesma fase nas duas
                            // pontas — o mod do VS é puro determinismo)
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> ref = readFrame(&w, &h);
        const u64 refHash = frameHash(ref);
        capturePng("C3_ctx_ref");
        // a PERDA REAL (TERM + INIT — o ciclo do device)
        onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
        onAppCmd(&g_app, APP_CMD_INIT_WINDOW);
        loadFixtureFontIfDead();
        selectCuboByName();
        const Handle we2 = addWeTic();   // o WE também morreu no reload
        (void)we2;
        const Handle wx2 = addWeatherTic(AmbientEnvironment::kWeatherRain);
        (void)wx2;   // o reload matou os TICs não gravados — re-criados
        for (int i = 0; i < 30; ++i) {
            stepFrame(1.0f / 60.0f);
        }
        g_fxTime = 50.0f;   // a MESMA fase (o seeds buffer re-nasce
                            // determinístico no INIT — o mesmo mod)
        stepFrame(0.0f);
        stepFrame(0.0f);
        const std::vector<u8> rec = readFrame(&w, &h);
        const u64 recHash = frameHash(rec);
        capturePng("C3_ctx_rec");
        g_fxTime = fxSemClima;
        std::printf("    V9 ctx: ref=%016llx rec=%016llx (%s)\n",
                    (unsigned long long)refHash,
                    (unsigned long long)recHash,
                    refHash == recHash ? "EXATO" : "diferente");
        if (!g_ready) {
            ++failures;
            std::printf("    [FAIL] V9: o render não voltou\n");
        } else if (refHash != recHash) {
            // a tolerância da casa (o chip do FPS muda o número; o
            // CONTEÚDO não)
            long ndiff = 0;
            int maxDelta = 0;
            const size_t np = ref.size() < rec.size() ? ref.size()
                                                      : rec.size();
            for (size_t o = 0; o + 3 < np; o += 4) {
                if (std::memcmp(&ref[o], &rec[o], 4) != 0) {
                    ++ndiff;
                    for (int k = 0; k < 4; ++k) {
                        const int d =
                            std::abs(int(ref[o + k]) - int(rec[o + k]));
                        if (d > maxDelta) maxDelta = d;
                    }
                }
            }
            const double tolPct =
                100.0 * double(ndiff) / double(w * h);
            std::printf("    [dbg] diff=%ld px (%.3f%%) maxDelta=%d\n",
                        ndiff, tolPct, maxDelta);
            if (tolPct < 0.5 && maxDelta <= 32) {
                check(true, "V9 CONTEXTO: o clima RAIN volta sem costura "
                            "(dentro da tolerância do dono)");
            } else {
                ++failures;
                std::printf("    [FAIL] V9: o frame pós-perda diverge do "
                            "REF\n");
            }
        } else {
            check(true, "V9 CONTEXTO: o clima RAIN volta EXATO após "
                        "TERM/INIT (o hash de píxeis igual)");
        }
        g_scene.destroy(wx2);
    }

    // o relatório da fase
    std::printf("    clima: falhas=%d\n", failures);
    prova("clima concluído (as capturas C3_* + os números acima)");
    check(failures == 0, "o clima passou TODAS as provas por render");
    return failures;
}

// ===========================================================================
// FASE 5-Z2 — GOLDEN IMAGES (a regressão POR IMAGEM, o pedido do dono da
// FASE 5): as cenas DOURADAS determinísticas (dt fixo 1/60 no assentamento,
// os relógios g_fxTime/g_skyTime AFOFERRADOS com stepFrame(0.0f) antes da
// leitura, o thunderTimer longe do flash) → o hash FNV-1a 64 da
// MAQUINARIA (raster_golden.h — o MESMO algoritmo do frameHash/V9) + a
// assinatura estrutural da região central → a lista viva em
// tests/golden/golden_z2.txt + os PNGs de referência em
// tests/golden/png/.
//
//   --golden-write  REGENERA (o protocolo no RELATORIO-0.12-Z2-F5.md
//                   §F5.3: o Mesa novo → o diff revisto → o ficheiro novo
//                   no MESMO commit que o documenta);
//   --golden        COMPARA: o PNG de referência decodifica (loadPng — o
//                   mesmo decodificador do pipeline de texturas) e o
//                   veredicto é o da casa: EXATO (byte a byte),
//                   TOLERANCIA (o V9: diff <0.5% E maxDelta ≤32 — o chip
//                   do FPS muda o número, o CONTEÚDO não) ou FALHA (o
//                   diff PNG vermelho sai em out-raster/ e o exit é 1).
// ===========================================================================
int goldenRun(bool writeMode) {
    std::printf("\n== FASE 5 — GOLDEN IMAGES (%s) ==\n",
                writeMode ? "WRITE — regeneração" : "VERIFY — a comparação");
    int failures = 0;
    const std::string gpath =
        std::string(REPO_ROOT) + "/" + gogolden::goldenRelPath();
    const std::string pdir =
        std::string(REPO_ROOT) + "/tests/golden/png";

    // a pose de sempre (a mesma da FASE B) e o assentamento+aferramento
    // (o padrão do V9: 30 frames a dt 1/60, depois os relógios FIXOS e
    // dois passos a dt 0 — o frame LIDO é o mesmo em TODAS as corridas)
    auto poseSempre = []() {
        g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
        g_camera.yaw = 0.60f;
        g_camera.pitch = 0.50f;
        g_camera.dist = 6.0f;
    };
    auto aferrar = []() {
        for (int i = 0; i < 30; ++i) stepFrame(1.0f / 60.0f);
        g_fxTime = 50.0f;
        g_skyTime = 100.0f;
        stepFrame(0.0f);
        stepFrame(0.0f);
    };

    // o WE e o clima criados inline (os ponteiros da cena na mão — o mesmo
    // corpo da FASE B e do addWeatherTic, com o raio longe do flash)
    Handle envH{};
    Environment* env = nullptr;
    Handle wxH{};
    auto criarWe = [&]() {
        envH = g_scene.create("WorldEnvironment");
        Tic* t = g_scene.get(envH);
        t->addComponent<Transform3D>();
        env = t->addComponent<Environment>();
        env->skyType = Environment::kSkyTypeProcedural;
    };
    auto criarClima = [&](u8 tipo) {
        wxH = g_scene.create("Clima");
        Tic* t = g_scene.get(wxH);
        t->addComponent<Transform3D>();
        AmbientEnvironment* ae = t->addComponent<AmbientEnvironment>();
        ParticleEmitter* pe = t->addComponent<ParticleEmitter>();
        ae->weatherType = tipo;
        ae->thunderTimer = 60.0f;   // o raio longe (o hash estável)
        ae->applyWeatherPreset(pe);
    };

    // o capturado (nome + frame + dims + a entrada da lista)
    struct Got {
        std::string nome;
        std::vector<u8> frame;
        int w = 0, h = 0;
        gogolden::GoldenEntry e;
    };
    std::vector<Got> got;
    auto capturar = [&](const char* nome) {
        aferrar();
        int w = 0, h = 0;
        const std::vector<u8> px = readFrame(&w, &h);
        const PixStats st =
            regionStats(px, w, h, w / 4, h / 2, 3 * w / 4, 3 * h / 4);
        Got g;
        g.nome = nome;
        g.frame = px;
        g.w = w;
        g.h = h;
        g.e.name = nome;
        g.e.hash = gogolden::fnv1a64(px.data(), px.size());
        g.e.nonBg = st.nonBackground;
        g.e.distinct = st.distinct;
        g.e.meanLum = st.meanLum;
        got.push_back(g);
        capturePng(std::string("golden_") + nome);
        std::printf("    %s: hash=%016llx nonBg=%ld distinct=%ld meanLum=%.4f\n",
                    nome, static_cast<unsigned long long>(g.e.hash),
                    st.nonBackground, st.distinct, st.meanLum);
    };
    auto writeDiffPng = [&](const Got& g, const std::vector<u8>& ref) {
        std::vector<u8> mask(g.frame.size());
        gogolden::diffMaskRgba(g.frame.data(), ref.data(), g.frame.size(),
                               mask.data());
        // o flip (GL lê da base; PNG é top-down — o mesmo do capturePng)
        std::vector<u8> top(mask.size());
        const int row = g.w * 4;
        for (int y = 0; y < g.h; ++y) {
            std::memcpy(&top[static_cast<size_t>(y) * row],
                        &mask[static_cast<size_t>(g.h - 1 - y) * row], row);
        }
        const std::vector<u8> png = thumb::encodePngRgba(top.data(), g.w, g.h);
        const std::string full =
            std::string(kOutDir) + "/golden_diff_" + g.nome + ".png";
        FILE* f = std::fopen(full.c_str(), "wb");
        if (f) {
            std::fwrite(png.data(), 1, png.size(), f);
            std::fclose(f);
        }
        std::printf("      diff PNG: %s\n", full.c_str());
    };

    poseSempre();

    // ---- AS CENAS DOURADAS (a ORDEM é o contrato da lista) --------------
    // (1) default_we — o boot + o WE procedural (o frame de sempre)
    criarWe();
    capturar("default_we");

    // (2) grid_solo — «tudo off menos a grelha» (o pedido do dono da F0)
    {
        gldbg::g_masks.master = true;
        gldbg::g_masks.sky = false;
        gldbg::g_masks.worldEnv = false;
        gldbg::g_masks.geometry = false;
        gldbg::g_masks.gizmos = false;
        gldbg::g_masks.ui = false;
        gldbg::g_masks.uiGlass = false;
        gldbg::g_masks.grid = true;
        capturar("grid_solo");
        gldbg::g_masks.master = false;
        gldbg::g_masks.sky = true;
        gldbg::g_masks.worldEnv = true;
        gldbg::g_masks.geometry = true;
        gldbg::g_masks.gizmos = true;
        gldbg::g_masks.ui = true;
        gldbg::g_masks.uiGlass = true;
    }

    // (3) we_midnight — o tom de chão do céu (a cura da F2: a «zona azul»)
    env->timeOfDay = 0.0f;
    capturar("we_midnight");
    env->timeOfDay = Environment::kTodDefault;

    // (4) fog_dense — a névoa densa (a grelha imune — a F2)
    env->fogDensity = 0.12f;
    capturar("fog_dense");
    env->fogDensity = 0.0f;
    g_scene.destroy(envH);
    envH = Handle{};
    env = nullptr;

    // (5..7) o CLIMA por tipo (o raio longe — o flash não entra no hash)
    criarClima(AmbientEnvironment::kWeatherRain);
    capturar("chuva");
    g_scene.destroy(wxH);
    wxH = Handle{};
    criarClima(AmbientEnvironment::kWeatherSnow);
    capturar("neve");
    g_scene.destroy(wxH);
    wxH = Handle{};
    criarClima(AmbientEnvironment::kWeatherStorm);
    capturar("trovoada");
    g_scene.destroy(wxH);
    wxH = Handle{};

    // (8) cam_borda — a pose extrema (o horizonte da grelha a dist 2500)
    g_camera.yaw = 2.35f;
    g_camera.pitch = 0.9f;
    g_camera.dist = 2500.0f;
    capturar("cam_borda");
    poseSempre();

    // (9) pos_recuperacao — o ciclo REAL do device (TERM/INIT) com a cena
    // reaberta: A MESMA prova da F1-R agora no nível da IMAGEM
    onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
    onAppCmd(&g_app, APP_CMD_INIT_WINDOW);
    loadFixtureFontIfDead();
    selectCuboByName();
    criarWe();
    criarClima(AmbientEnvironment::kWeatherRain);
    capturar("pos_recuperacao");

    // ---- WRITE: a lista + os PNGs de referência -------------------------
    if (writeMode) {
        FILE* f = std::fopen(gpath.c_str(), "w");
        if (!f) {
            std::printf("    [FAIL] o ficheiro %s não abre\n", gpath.c_str());
            return 1;
        }
        std::fprintf(f, "# golden_z2 — AS GOLDEN IMAGES da 0.12-Z2 F5 (a "
                        "regressão por imagem)\n");
        std::fprintf(f, "# formato: nome hash nonBg distinct meanLum — a "
                        "maquinaria vive em tests/raster_golden.h\n");
        std::fprintf(f, "# Mesa: %s | %s\n", glGetString(GL_RENDERER),
                     glGetString(GL_VERSION));
        std::fprintf(f, "# regeneração: raster_z2 --golden-write (o "
                        "protocolo no RELATORIO-0.12-Z2-F5.md §F5.3)\n");
        for (const Got& g : got) {
            std::fprintf(f, "%s\n", gogolden::goldenLine(g.e).c_str());
        }
        std::fclose(f);
        for (const Got& g : got) {
            // o flip (GL lê da base; o PNG de referência é top-down)
            std::vector<u8> top(g.frame.size());
            const int row = g.w * 4;
            for (int y = 0; y < g.h; ++y) {
                std::memcpy(&top[static_cast<size_t>(y) * row],
                            &g.frame[static_cast<size_t>(g.h - 1 - y) * row],
                            row);
            }
            const std::vector<u8> png =
                thumb::encodePngRgba(top.data(), g.w, g.h);
            const std::string full = pdir + "/golden_" + g.nome + ".png";
            FILE* pf = std::fopen(full.c_str(), "wb");
            if (pf) {
                std::fwrite(png.data(), 1, png.size(), pf);
                std::fclose(pf);
            }
        }
        std::printf("    WRITE OK: %zu cenas → %s (+ png/)\n", got.size(),
                    gpath.c_str());
        return 0;
    }

    // ---- VERIFY: a lista + os PNGs decodificados → o veredicto ---------
    std::vector<gogolden::GoldenEntry> ref;
    {
        FILE* f = std::fopen(gpath.c_str(), "rb");
        if (!f) {
            std::printf("    [FAIL] a lista %s não abre (corre "
                        "--golden-write no Mesa de referência)\n",
                        gpath.c_str());
            return 1;
        }
        std::string txt;
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) txt.append(buf, n);
        std::fclose(f);
        if (!gogolden::parseGoldenList(txt, ref)) {
            std::printf("    [FAIL] a lista tem uma linha de dados "
                        "malformada (o ficheiro podre morre cedo)\n");
            return 1;
        }
    }
    if (ref.size() != got.size()) {
        ++failures;
        std::printf("    [FAIL] a lista tem %zu goldens e a cena produziu "
                    "%zu — a lista apodreceu (regenera ou corrige)\n",
                    ref.size(), got.size());
    }
    for (const gogolden::GoldenEntry& e : ref) {
        const Got* g = nullptr;
        for (const Got& cand : got) {
            if (cand.nome == e.name) {
                g = &cand;
                break;
            }
        }
        if (!g) {
            ++failures;
            std::printf("    [FAIL] %s: a cena não correu (a lista e o "
                        "harness divergiram)\n",
                        e.name.c_str());
            continue;
        }
        if (g->e.hash == e.hash) {
            std::printf("    [ok]   %-16s EXATO (hash %016llx)\n",
                        e.name.c_str(),
                        static_cast<unsigned long long>(e.hash));
            continue;
        }
        // o hash divergiu — O PNG de referência decide (o diff por píxel
        // com a política do V9)
        const std::string pngPath = pdir + "/golden_" + e.name + ".png";
        FILE* pf = std::fopen(pngPath.c_str(), "rb");
        if (!pf) {
            ++failures;
            std::printf("    [FAIL] %s: hash divergiu e o PNG de referência "
                        "não existe (%s)\n",
                        e.name.c_str(), pngPath.c_str());
            continue;
        }
        std::vector<u8> bytes;
        char buf[8192];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), pf)) > 0)
            bytes.insert(bytes.end(), buf, buf + n);
        std::fclose(pf);
        RawImage img;
        std::string err;
        if (!vv::loadPng(bytes.data(), bytes.size(), img, err) || !img.ok()) {
            ++failures;
            std::printf("    [FAIL] %s: o PNG de referência não decodifica "
                        "(%s)\n",
                        e.name.c_str(), err.c_str());
            continue;
        }
        if (img.width != static_cast<u32>(g->w) ||
            img.height != static_cast<u32>(g->h)) {
            ++failures;
            std::printf("    [FAIL] %s: o PNG é %ux%u e o frame é %dx%d\n",
                        e.name.c_str(), img.width, img.height, g->w, g->h);
            continue;
        }
        // o PNG de referência é top-down (capturePng); o frame é GL
        // bottom-up — o flip do PNG para a ordem GL antes do diff
        std::vector<u8> refGl(img.rgba.size());
        const int row = g->w * 4;
        for (int y = 0; y < g->h; ++y) {
            std::memcpy(&refGl[static_cast<size_t>(y) * row],
                        &img.rgba[static_cast<size_t>(g->h - 1 - y) * row],
                        row);
        }
        const gogolden::DiffReport d =
            gogolden::diffRgba(g->frame.data(), refGl.data(), refGl.size());
        const gogolden::Verdict v = gogolden::verdict(d);
        if (v == gogolden::Verdict::FALHA) {
            ++failures;
            std::printf("    [FAIL] %s: FALHA — diff=%ld px (%.3f%%) "
                        "maxDelta=%d (a regressão SALTA no diff PNG)\n",
                        e.name.c_str(), d.diff, d.pct, d.maxDelta);
            writeDiffPng(*g, refGl);
        } else if (v == gogolden::Verdict::TOLERANCIA) {
            std::printf("    [nota] %-16s TOLERANCIA — diff=%ld px (%.3f%%) "
                        "maxDelta=%d (a política do V9; a Lista e o "
                        "relatório ficam com a contagem)\n",
                        e.name.c_str(), d.diff, d.pct, d.maxDelta);
        }
    }
    // a cena EXTRA (o harness a produzir o que a lista não conhece) = a
    // lista podre — nunca uma golden silenciosamente ignorada
    for (const Got& g : got) {
        bool known = false;
        for (const gogolden::GoldenEntry& e : ref) {
            if (e.name == g.nome) {
                known = true;
                break;
            }
        }
        if (!known) {
            ++failures;
            std::printf("    [FAIL] %s: a cena capturada NÃO está na lista "
                        "(o ficheiro apodreceu — regenera)\n",
                        g.nome.c_str());
        }
    }
    std::printf("    golden: falhas=%d (de %zu cenas)\n", failures,
                got.size());
    prova("golden images concluídas (a lista tests/golden/golden_z2.txt)");
    check(failures == 0, "as golden images passaram (a regressão por "
                        "imagem está verde)");
    return failures;
}

// ===========================================================================
// FASE W-1 (0.12-W-1) — A ARQUITETURA DO CÉU E DO CLIMA EM PÍXEIS:
//   1. O CÉU DEFAULT SEM WorldEnvironment (a decisão 1 do dono: o quad
//      procedural desenha com tod 12 / cobertura 0.3 — o velho fundo era o
//      clear escuro de fábrica);
//   2. AS ESTRELAS à noite (22:00) + a DENSIDADE do utilizador (1.0 acende
//      o campo, 0 apaga);
//   3. A LUA em píxeis: o disco ACENDE, a FASE 0 (nova) apaga-o, o
//      brilho 0 desliga-o — a pose CENTRA a lua pela INVERSA sky::dirToNdc;
//   4. A GRELHA SOBREVIVE AO CÉU (a refutação da ordem «grelha antes do
//      céu»: com a grelha MASCARADA o frame muda; se o passe do céu a
//      apagasse, o diff seria ZERO);
//   5. O CLIMA PELO NOME (o fluxo dos DOIS TICs do dono): o "RainEmitter"
//      (ParticleEmitter+Transform) + o "WeatherController"
//      (AmbientEnvironment) — a chuva desenha SEM ninguém tocar no emissor.
// ===========================================================================
int w1Run() {
    fase("FASE W-1 — a arquitetura em píxeis (o céu default, as estrelas, "
         "a lua, a grelha, o alvo do clima)");
    // o assentamento da casa (o padrão do golden: 30 frames + relógios
    // FIXOS + 2 passos a dt 0 — o frame LIDO é o mesmo em TODAS as corridas)
    auto aferrar = []() {
        for (int i = 0; i < 30; ++i) stepFrame(1.0f / 60.0f);
        g_fxTime = 50.0f;
        g_skyTime = 100.0f;
        stepFrame(0.0f);
        stepFrame(0.0f);
    };
    auto poseSempre = []() {
        g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
        g_camera.yaw = 0.60f;
        g_camera.pitch = 0.50f;
        g_camera.dist = 6.0f;
    };
    // varre a cena de Environment/Ambient/ParticleEmitter/Wind (o boot pode
    // ter deixado herança — a FASE W-1 controla a cena COMPLETA)
    auto varrerCena = []() {
        std::vector<Handle> alvo;
        g_scene.forEachActive([&](Tic& t) {
            if (t.getComponent<Environment>() ||
                t.getComponent<AmbientEnvironment>() ||
                t.getComponent<ParticleEmitter>() ||
                t.getComponent<WindTIC>()) {
                alvo.push_back(t.handle);
            }
        });
        for (const Handle h : alvo) {
            g_scene.destroy(h);
        }
    };
    // a luminância de um pixel (o buffer do glReadPixels: linha 0 = FUNDO)
    auto lumPx = [](const std::vector<u8>& px, int w, int h, f32 ndcX,
                    f32 ndcY, int* rOut = nullptr, int* bOut = nullptr) {
        const int x = static_cast<int>((ndcX * 0.5f + 0.5f) * (w - 1));
        const int y = static_cast<int>((ndcY * 0.5f + 0.5f) * (h - 1));
        const size_t o = (static_cast<size_t>(y) * w + x) * 4;
        if (o + 2 >= px.size()) {
            return -1.0f;
        }
        if (rOut) *rOut = px[o];
        if (bOut) *bOut = px[o + 2];
        return (0.2126f * px[o] + 0.7152f * px[o + 1] +
                0.0722f * px[o + 2]) / 255.0f;
    };

    // ---- (1) O CÉU DEFAULT SEM WorldEnvironment ------------------------
    varrerCena();
    poseSempre();
    g_camera.pitch = -0.35f;   // a olhar PARA CIMA: o céu enche o frame
    aferrar();
    {
        int w = 0, h = 0;
        const std::vector<u8> px = readFrame(&w, &h);
        int r = 0, b = 0;
        const f32 lumTop = lumPx(px, w, h, 0.0f, 0.75f, &r, &b);
        std::printf("    sem WE: pixel do topo r=%d b=%d lum=%.3f\n", r, b,
                    lumTop);
        check(lumTop > 0.2f && b > r + 20,
              "sem WorldEnvironment o céu procedural AZUL desenha (o "
              "default W-1: tod 12 · cobertura 0.3 — o velho era o clear "
              "escuro de fábrica)");
        const f32 lumGround = lumPx(px, w, h, 0.0f, -0.75f);
        check(lumGround > 0.05f && lumGround < 0.5f,
              "sem WE o abaixo-do-horizonte é o TOM DE CHÃO (o gradiente da "
              "Z2-F2 — nem céu azul nem o escuro de fábrica)");
        prova("o céu DEFAULT sem WorldEnvironment (a decisão 1 do dono)");
    }

    // ---- (2) AS ESTRELAS à noite + a densidade do utilizador ------------
    Handle envH = g_scene.create("WorldEnvironment");
    Environment* env = nullptr;
    {
        Tic* t = g_scene.get(envH);
        t->addComponent<Transform3D>();
        env = t->addComponent<Environment>();
        env->skyType = Environment::kSkyTypeProcedural;
        env->timeOfDay = 22.0f;        // a noite do dono
        env->starDensity = 1.0f;       // o campo CHEIO (o limiar 112)
        env->starBrightness = 0.95f;
        env->moonBrightness = 0.0f;    // a lua FORA da prova das estrelas
        env->cloudCoverage = 0.0f;     // céu LIMPO: as nuvens não comem a
                                       // contagem (a cobertura 0 mata o
                                       // limiar — n ≤ 1 < t=1)
        env->clampFields();
    }
    // A UI FORA DA MEDIDA (o isolamento da F0): os botões do chrome do
    // viewport são BRANCOS — sem as máscaras a sonda conta ÍCONES, não
    // estrelas; com ui/gizmos OFF o frame é o 3D PURO
    gldbg::g_masks.master = true;
    gldbg::g_masks.ui = false;
    gldbg::g_masks.uiGlass = false;
    gldbg::g_masks.gizmos = false;
    aferrar();
    long estrelasCheias = 0;
    {
        int w = 0, h = 0;
        const std::vector<u8> px = readFrame(&w, &h);
        // a FAIXA CENTRAL do viewport (x 270..1010 — sem a hierarquia/à
        // esquerda nem o Inspector/à direita, o padrão das sondas da casa)
        // na METADE DE CIMA (o buffer é bottom-up: as linhas h/2..h−120)
        estrelasCheias = textPixels(px, w, 270, h / 2, 1010, h - 120, 0.55f);
        const f32 lumTop = lumPx(px, w, h, 0.0f, 0.75f);
        capturePng("w1_estrelas_dense");
        std::printf("    22:00 density 1.0: %ld px de estrelas (metade de "
                    "cima), fundo lum=%.3f\n", estrelasCheias, lumTop);
        check(estrelasCheias > 300,
              "as ESTRELAS aparecem às 22:00 com density 1.0 (o campo em "
              "píxeis)");
        check(lumTop < 0.15f,
              "às 22:00 o céu é ESCURO (o override do WE — a paleta da "
              "noite da Y-E)");
    }
    env->starDensity = 0.0f;    // o campo MORTO
    env->clampFields();
    aferrar();
    {
        int w = 0, h = 0;
        const std::vector<u8> px = readFrame(&w, &h);
        const long estrelasZero = textPixels(px, w, 270, h / 2, 1010,
                                            h - 120, 0.55f);
        std::printf("    22:00 density 0.0: %ld px de estrelas\n",
                    estrelasZero);
        check(estrelasZero < estrelasCheias / 10,
              "density 0 APAGA o campo (o starDensity do utilizador manda)");
        prova("as estrelas às 22:00 + a densidade do utilizador em píxeis");
    }

    // ---- (3) A LUA: o disco, as fases, o brilho --------------------------
    env->starDensity = Environment::kStarDensityDefault;
    env->starBrightness = Environment::kStarBrightDefault;
    env->moonPhase = 0.5f;          // a CHEIA
    env->moonBrightness = 0.9f;
    env->cloudCoverage = 0.0f;      // o céu continua LIMPO: a nuvem não
                                    // pode tapar o disco da prova
    env->clampFields();
    // a pose que CENTRA a lua: varre yaw/pitch e mede o NDC da lua pela
    // INVERSA sky::dirToNdc (determinístico — sem constantes mágicas). O
    // guard da INVERSA devolve (0,0) para a lua ATRÁS da câmara — a busca
    // EXIGE dot(lua, fwd) > 0.35 (a lua À FRENTE, bem dentro do cone)
    f32 moonDir[3];
    sky::moonDirFromTime(22.0f, moonDir);
    f32 bestYaw = 0.6f, bestPitch = -0.6f, bestN2 = 9.0f;
    for (int yi = 0; yi < 63; ++yi) {
        for (int pi = 0; pi < 25; ++pi) {
            g_camera.yaw = static_cast<f32>(yi) * 0.1f;
            g_camera.pitch = -0.1f - static_cast<f32>(pi) * 0.05f;
            const Mat4 view = g_camera.view();
            Vec3 right, up, fwd;
            if (!sky::viewBasis(view, right, up, fwd)) {
                continue;
            }
            const f32 facing = moonDir[0] * fwd.x + moonDir[1] * fwd.y +
                               moonDir[2] * fwd.z;
            if (facing < 0.35f) {
                continue;   // a lua atrás/de lado — o guard mentiria (0,0)
            }
            f32 ndc[2];
            sky::dirToNdc(Vec3{moonDir[0], moonDir[1], moonDir[2]}, right,
                          up, fwd, 1280.0f / 720.0f,
                          std::tan(g_camera.fovY * 0.5f), ndc);
            const f32 n2 = ndc[0] * ndc[0] + ndc[1] * ndc[1];
            if (n2 < bestN2) {
                bestN2 = n2;
                bestYaw = g_camera.yaw;
                bestPitch = g_camera.pitch;
            }
        }
    }
    g_camera.yaw = bestYaw;
    g_camera.pitch = bestPitch;
    // O ALVO SOBE ao longo da direção da lua: com o pitch negativo a olhar
    // para cima, o OLHO da orbit caía 5 unidades DEBAIXO do chão e o CUBO
    // da cena silhuetava EXATAMENTE à frente da lua (o "polígono" da
    // sonda). O target a 40 unidades na direção da lua põe o OLHO no céu —
    // a cena fica PARA TRÁS e o disco limpo (a direção da vista não muda:
    // é do yaw/pitch)
    g_camera.target = Vec3{moonDir[0] * 40.0f, moonDir[1] * 40.0f,
                           moonDir[2] * 40.0f};
    aferrar();
    // A SONDA: o pixel MAIS BRILHANTE da faixa central do viewport — o
    // DISCO da lua cheia (com o glow por baixo) é o ponto mais claro do
    // céu noturno; a busca NDC garante o ENQUADRAMENTO, o píxel mais claro
    // AFERRE o disco (o layout do viewport é do editor — o NDC→píxel exato
    // não é contrato da sonda)
    int luaX = -1, luaY = -1;
    f32 lumCheia = -1.0f;
    {
        int w = 0, h = 0;
        const std::vector<u8> px = readFrame(&w, &h);
        for (int y = 60; y < 650; ++y) {
            for (int x = 270; x < 1010; ++x) {
                const size_t o = (static_cast<size_t>(y) * w + x) * 4;
                const f32 lum = (0.2126f * px[o] + 0.7152f * px[o + 1] +
                                 0.0722f * px[o + 2]) / 255.0f;
                if (lum > lumCheia) {
                    lumCheia = lum;
                    luaX = x;
                    luaY = y;
                }
            }
        }
        std::printf("    lua: pose yaw=%.2f pitch=%.2f ndc=(%.2f,%.2f) "
                    "píxel=(%d,%d) lum=%.3f\n", bestYaw, bestPitch,
                    0.0f, 0.0f, luaX, luaY, lumCheia);
        capturePng("w1_lua_cheia");
        check(bestN2 < 0.25f,
              "a lua às 22:00 está no ENQUADRAMENTO (a pose centralizada "
              "pela INVERSA — a sonda depende dela)");
        check(lumCheia > 0.5f,
              "a LUA CHEIA acende (o píxel mais claro do viewport é o "
              "disco: phase 0.5 · brightness 0.9)");
    }
    // A FASE: a lua NOVA apaga o MESMO pixel
    env->moonPhase = 0.0f;
    env->clampFields();
    aferrar();
    {
        int w = 0, h = 0;
        const std::vector<u8> px = readFrame(&w, &h);
        const size_t o = (static_cast<size_t>(luaY) * w + luaX) * 4;
        const f32 lumNova = (0.2126f * px[o] + 0.7152f * px[o + 1] +
                             0.0722f * px[o + 2]) / 255.0f;
        std::printf("    lua NOVA (phase 0): lum=%.3f (era %.3f)\n", lumNova,
                    lumCheia);
        check(lumNova < lumCheia - 0.15f,
              "a FASE controla o disco: a lua NOVA (phase 0) apaga o centro");
    }
    // O BRILHO: 0 desliga a lua inteira (disco + glow)
    env->moonPhase = 0.5f;
    env->moonBrightness = 0.0f;
    env->clampFields();
    aferrar();
    {
        int w = 0, h = 0;
        const std::vector<u8> px = readFrame(&w, &h);
        const size_t o = (static_cast<size_t>(luaY) * w + luaX) * 4;
        const f32 lumOff = (0.2126f * px[o] + 0.7152f * px[o + 1] +
                            0.0722f * px[o + 2]) / 255.0f;
        std::printf("    lua brightness 0: lum=%.3f (era %.3f)\n", lumOff,
                    lumCheia);
        check(lumOff < lumCheia - 0.1f,
              "moonBrightness 0 DESLIGA a lua (o disco e o glow morrem)");
        prova("a lua em píxeis: o disco acende, a fase apaga, o brilho "
              "desliga");
    }
    env->moonBrightness = Environment::kMoonBrightDefault;
    env->cloudCoverage = Environment::kCoverageDefault;
    env->timeOfDay = Environment::kTodDefault;
    env->clampFields();
    // a UI VOLTA (as máscaras ao estado de fábrica)
    gldbg::g_masks.master = false;
    gldbg::g_masks.ui = true;
    gldbg::g_masks.uiGlass = true;
    gldbg::g_masks.gizmos = true;

    // ---- (4) A GRELHA SOBREVIVE AO CÉU (a refutação da ordem) -----------
    poseSempre();
    aferrar();
    {
        int w = 0, h = 0;
        const std::vector<u8> comGrelha = readFrame(&w, &h);
        gldbg::g_masks.master = true;
        gldbg::g_masks.grid = false;   // TUDO on MENOS a grelha
        aferrar();
        const std::vector<u8> semGrelha = readFrame(&w, &h);
        gldbg::g_masks.master = false;
        gldbg::g_masks.grid = true;
        const long d = diffPixels(comGrelha, semGrelha);
        std::printf("    grelha on/off: diff=%ld px\n", d);
        check(d > 2000,
              "a grelha desenha POR CIMA do céu (a ordem céu→grelha→TICs — "
              "se a grelha fosse ANTES do céu, o passe fullscreen "
              "apagava-a e este diff seria ~0)");
        prova("a grelha sobrevive ao passe do céu (a «grelha antes do céu» "
              "refutada por construção)");
    }

    // ---- (5) O CLIMA PELO NOME (o fluxo dos DOIS TICs do dono) ----------
    poseSempre();
    aferrar();
    {
        int w = 0, h = 0;
        const std::vector<u8> antes = readFrame(&w, &h);
        // o "RainEmitter" (ParticleEmitter + Transform) PRIMEIRO, o
        // "WeatherController" (AmbientEnvironment) DEPOIS — a criação do
        // controlador LIGA os dois pelo NOME (ninguém toca no emissor)
        const Handle hpe = createTicFromPreset(
            g_scene, PresetKind::ParticleEmitter, nullptr, nullptr);
        g_scene.get(hpe)->name = "RainEmitter";
        const Handle hae = createTicFromPreset(
            g_scene, PresetKind::WeatherController, nullptr, nullptr);
        (void)hae;
        aferrar();
        const std::vector<u8> depois = readFrame(&w, &h);
        const long gotas = particlePixels(depois, antes, w, h, 270, 180,
                                          1010, 640);
        std::printf("    DOIS TICs (RainEmitter+WeatherController): %ld px "
                    "de chuva na faixa central\n", gotas);
        check(gotas > 150,
              "a chuva do 'RainEmitter' controlado pelo 'WeatherController' "
              "desenha (o fluxo dos DOIS TICs — o alvo por NOME no render "
              "real)");
        // a mortes: a cena volta ao estado do boot
        g_scene.destroy(hpe);
        g_scene.destroy(hae);
        prova("o clima por NOME em píxeis (dois TICs, um controlador)");
    }
    varrerCena();
    return g_failed;
}

}  // namespace

int main(int argc, char** argv) {
    const bool fuzzMode = argc > 1 && std::strcmp(argv[1], "--fuzz") == 0;
    // 0.12-Z2 F1-bis: o MODO B (contextRecover da sonda, sem reload) —
    // o caminho do EGL_CONTEXT_LOST e do botão «Reiniciar renderer»;
    // aferra o REBIND do blockRebind em pixels (a cena NÃO recarrega)
    const bool fuzzBMode = argc > 1 && std::strcmp(argv[1], "--fuzzb") == 0;
    const bool sky2Mode = argc > 1 && std::strcmp(argv[1], "--sky2") == 0;
    const bool climaMode = argc > 1 && std::strcmp(argv[1], "--clima") == 0;
    // FASE 5-Z2: as golden images (a regressão POR IMAGEM) — o --golden
    // compara contra tests/golden/, o --golden-write regenera (o protocolo
    // no RELATORIO-0.12-Z2-F5.md §F5.3)
    const bool goldenMode =
        argc > 1 && std::strcmp(argv[1], "--golden") == 0;
    const bool goldenWriteMode =
        argc > 1 && std::strcmp(argv[1], "--golden-write") == 0;
    const int sky2Poses = argc > 2 ? std::atoi(argv[2]) : 200;
    const int fuzzCycles = argc > 2 ? std::atoi(argv[2]) : 1000;
    const u32 fuzzSeed = argc > 3 ? static_cast<u32>(std::strtoul(argv[3], nullptr, 10)) : 0xC0FFEEu;
    std::printf("== RASTER Z2 — GLES REAL no hospedeiro (0.12-Z2 FASE 0) ==\n");
    std::printf("   Mesa/llvmpipe via EGL pbuffer; pixels LIDOS (glReadPixels)\n");
    rmrf(kOutDir);
    rmrf(kHarnessLogs);
    rmrf(kCacheDir);
    fileapi::makeDirs(kOutDir);
    if (!vv::elog::init(kHarnessLogs)) {
        std::printf("FATAL: elog init\n");
        return 2;
    }
    vv::buildinfo::set("0.12-Z2-raster", 130, "rasterc33", "aabbccdd00112233", 1790000000ull);
    elog::info("%s", vv::buildinfo::banner().c_str());

    javaRegistersWithCacheDir();
    rasteregl::g_surfaceW = 1280;   // 720p pedido pelo dono
    rasteregl::g_surfaceH = 720;

    // ---- o projeto + a árvore de recursos (o padrão do c33) --------------
    std::printf("[mk] storage\n"); std::fflush(stdout);
    auto st = std::make_unique<FakeStorage>();
    rawSt = st.get();   // o ponteiro sobrevive ao move (o pointee é o mesmo)
    check(vv::Project::createNew(*rawSt, "raster", g_project), "projeto criado");
    g_storage = std::move(st);
    g_projectReady = true;
    g_resources.setStorage(rawSt);
    g_gpu.init(&g_resources);
    g_texCache = std::make_unique<TextureCache>(*rawSt);
    g_pipeline = std::make_unique<TexturePipeline>(g_hwCompressor, *g_texCache);

    // a cena do sintoma ANTES do boot (o caminho do device: o projeto tem
    // o OBJ legado e a cena ativa com o TIC Cubo — a migração do boot
    // converte e o [boot 6/6] carrega)
    std::printf("[mk] projeto ok, escrevendo cena do sintoma\n"); std::fflush(stdout);
    writeSymptomProject();
    std::printf("[mk] cena gravada\n"); std::fflush(stdout);

    glResourcesRegister();
    // O g_app ÚNICO (o fuzzRun e as fases partilham a MESMA instância —
    // o contentRect é o payload do sistema e tem de sobreviver aos INITs,
    // como no device: o glue mantém o último retângulo entregado)
    std::memset(&g_app, 0, sizeof(g_app));
    g_app.contentRect = {0, 24, 1256, 720};   // insets do C33 escalados a 1280
    const auto tBoot = std::chrono::steady_clock::now();
    std::printf("[mk] INIT_WINDOW...\n"); std::fflush(stdout);
    onAppCmd(&g_app, APP_CMD_INIT_WINDOW);
    std::printf("[mk] INIT done\n"); std::fflush(stdout);
    check(g_ready, "boot: INIT_WINDOW completo (g_ready) com GLES REAL");
    // 0.12-Z2 F1-R (a lição do ambiente reconstruído): se o EGL NÃO abriu
    // (w/h=0) o readFrame devolve VAZIO e a primeira regionStats com
    // coordenadas fixas faz SEGFAULT em vez de falhar — abortar JÁ com o
    // diagnóstico do protocolo (não há prova sem driver):
    //   DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 \
    //   LD_LIBRARY_PATH=/home/z/mesa-debs/root/usr/lib/x86_64-linux-gnu \
    //   __EGL_VENDOR_LIBRARY_FILENAMES=…/50_mesa.json
    if (g_egl.width() != 1280 || g_egl.height() != 720) {
        std::printf(
            "FALHA AMBIENTAL: o EGL NÃO abriu (w=%d h=%d) — sem driver não "
            "há prova.\n    Protocolo: DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 "
            "LD_LIBRARY_PATH=/home/z/mesa-debs/root/usr/lib/x86_64-linux-gnu "
            "__EGL_VENDOR_LIBRARY_FILENAMES=<mesa>/50_mesa.json\n",
            g_egl.width(), g_egl.height());
        std::fflush(stdout);
        return 1;
    }
    check(g_egl.width() == 1280 && g_egl.height() == 720,
          "boot: superficie 1280x720 (pbuffer real)");
    // o PARITY do device: no device a camada "font" re-bake de
    // /system/fonts (o recreate do gestor); no hospedeiro não há fontes de
    // sistema — a fixture entra AQUI (o mesmo papel do Roboto no device)
    if (!g_font.ok()) {
        const char* paths[] = {FONT_FIXTURE};
        g_font.loadFromPaths(paths, 1, 28.0f);
    }
    g_ui.setFont(&g_font);
    // a seleção do TIC Cubo (o gizmo vivo — o estado do dono)
    selectCuboByName();
    check(g_hCubo.valid(), "boot: o TIC Cubo carregado da cena (migration legado→gmesh)");
    if (fuzzMode || fuzzBMode) {
        // FASE 1: o fuzzing usa o MESMO boot; as fases A-D não correm
        // (o --fuzzb: o MODO B — contextRecover da sonda, sem reload)
        const int f = fuzzRun(fuzzCycles, fuzzSeed, /*modoB=*/fuzzBMode);
        onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
        std::printf("\n%s (falhas=%d)\n", f ? "FUZZ FALHOU" : "FUZZ OK", f);
        return f ? 1 : 0;
    }
    if (sky2Mode) {
        // FASE 2: o varrimento de câmara usa o MESMO boot
        const int f = sky2Run(sky2Poses);
        onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
        std::printf("\n%s (falhas=%d)\n", f ? "SKY2 FALHOU" : "SKY2 OK", f);
        return f ? 1 : 0;
    }
    if (climaMode) {
        // FASE 3: o clima visível usa o MESMO boot
        const int f = climaRun();
        onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
        std::printf("\n%s (falhas=%d)\n", f ? "CLIMA FALHOU" : "CLIMA OK", f);
        return f ? 1 : 0;
    }
    const bool w1Mode = argc > 1 && std::strcmp(argv[1], "--w1") == 0;
    if (w1Mode) {
        // 0.12-W-1: a arquitetura em píxeis (o céu default, as estrelas,
        // a lua, a grelha, o alvo do clima) — o MESMO boot
        const int f = w1Run();
        onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
        std::printf("\n%s (falhas=%d)\n", f ? "W1 FALHOU" : "W1 OK", f);
        return f ? 1 : 0;
    }
    if (goldenMode || goldenWriteMode) {
        // FASE 5: as golden images usam o MESMO boot (a mesma cena do
        // sintoma, a mesma fonte, a mesma seleção)
        const int f = goldenRun(goldenWriteMode);
        onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
        std::printf("\n%s (falhas=%d)\n", f ? "GOLDEN FALHOU" : "GOLDEN OK",
                    f);
        return f ? 1 : 0;
    }

    // ======================================================================
    // FASE A — A PROVA DE RASTERIZAÇÃO REAL
    // ======================================================================
    fase("FASE A — arranque real e prova de rasterização");
    {
        const GLubyte* rend = glGetString(GL_RENDERER);
        const GLubyte* ver = glGetString(GL_VERSION);
        std::printf("    GL_RENDERER = %s\n", rend ? (const char*)rend : "?");
        std::printf("    GL_VERSION  = %s\n", ver ? (const char*)ver : "?");
        check(rend && std::strstr((const char*)rend, "llvmpipe") != nullptr,
              "GL_RENDERER é llvmpipe (driver REAL, não stub)");
        check(ver && std::strstr((const char*)ver, "OpenGL ES") != nullptr,
              "GL_VERSION é OpenGL ES (o perfil do device)");
        prova("GL_RENDERER/GLES reais no hospedeiro (Mesa/llvmpipe)");

        frameN(3);
        int w = 0, h = 0;
        const std::vector<u8> px = readFrame(&w, &h);
        check((int)px.size() == w * h * 4 && w == 1280 && h == 720,
              "glReadPixels devolve o framebuffer 1280x720 inteiro");
        const PixStats full = regionStats(px, w, h, 0, 0, w, h);
        std::printf("    stats: total=%ld distinct=%ld meanLum=%.3f minLum=%.3f maxLum=%.3f\n",
                    full.total, full.distinct, full.meanLum, full.minLum, full.maxLum);
        check(full.distinct > 64,
              "o frame é RASTERIZADO (≥64 cores distintas — stub no-op devolveria 1)");
        check(full.maxLum - full.minLum > 0.05f,
              "o frame tem CONTRASTE (céu/grama/grelha/UI desenhados)");

        // a GRELHA: com a câmara default (origem, dist 6) a área 3D central
        // tem linhas da grelha sobre o fundo — pixels não-fundo na faixa
        // central abaixo do horizonte
        const PixStats center = regionStats(px, w, h, w / 4, h / 2, 3 * w / 4, 3 * h / 4);
        std::printf("    centro 3D: nonBackground=%ld/%ld (%.1f%%)\n",
                    center.nonBackground, center.total,
                    100.0 * center.nonBackground / (center.total ? center.total : 1));
        check(center.nonBackground > 8,
              "a GRELHA é rasterizada (pixels não-fundo na área 3D central)");
        prova("grelha rasterizada por pixels (fwidth corre no driver)");

        capturePng("A0_boot");
    }

    // ======================================================================
    // FASE B — BISSEÇÃO POR PASSE (cena saudável, ANTES de qualquer perda)
    // ======================================================================
    fase("FASE B — bisseção por passe (gldbg::g_masks, cena completa)");
    {
        // a cena do dono: cubo selecionado (gizmo vivo) + WorldEnvironment
        // PROCEDURAL (o céu do gradiente — o toggle "ceu" tem de mudar algo)
        selectCuboByName();
        const Handle envH = g_scene.create("WorldEnvironment");
        Tic* envT = g_scene.get(envH);
        envT->addComponent<Transform3D>();
        Environment* env = envT->addComponent<Environment>();
        env->skyType = Environment::kSkyTypeProcedural;
        frameN(3);
        int w = 0, h = 0;
        const std::vector<u8> b1 = readFrame(&w, &h);
        capturePng("B1_we_procedural");
        const PixStats s1 = regionStats(b1, w, h, 0, 0, w, h);

        // B2: SEM WorldEnvironment (remove o TIC)
        g_scene.destroy(envH);
        frameN(3);
        const std::vector<u8> b2 = readFrame(&w, &h);
        capturePng("B2_sem_we");
        const PixStats s2 = regionStats(b2, w, h, 0, 0, w, h);

        const long d12 = diffPixels(b1, b2);
        std::printf("    B1(WE) vs B2(sem WE): %ld pixels diferentes (meanLum %.3f → %.3f)\n",
                    d12, s1.meanLum, s2.meanLum);
        check(d12 > 0, "WorldEnvironment muda o frame (célula céu/fundo viva)");
        prova("B1/B2 capturados: WE procedural ON e OFF (célula céu/fundo)");

        // o TIC do WE VOLTA (a bisseção precisa dele: o toggle "ceu" só
        // vive com skyType procedural e o "worldEnv" com env na cena)
        const Handle envH2 = g_scene.create("WorldEnvironment");
        Tic* envT2 = g_scene.get(envH2);
        envT2->addComponent<Transform3D>();
        Environment* env2 = envT2->addComponent<Environment>();
        env2->skyType = Environment::kSkyTypeProcedural;
        frameN(3);
        const std::vector<u8> b1b = readFrame(&w, &h);
        const long dB = diffPixels(b1, b1b);
        std::printf("    WE recriado: %ld px dif vs B1 (o TIC idêntico re-renderiza igual: %s)\n",
                    dB, dB == 0 ? "IDÊNTICO" : "DIFERENTE");

        // B5: OS CANDIDATOS à "zona escura/azul que corta a grelha" (o
        // sintoma do dono) — capturas ANTES de cada configuração suspeita
        {
            env2->timeOfDay = 0.0f;   // meia-noite (o céu escuro)
            frameN(3);
            capturePng("B5_we_meianoite");
            std::printf("    B5a meia-noite capturada (timeOfDay=0)\n");

            env2->timeOfDay = Environment::kTodDefault;
            env2->fogDensity = 0.12f;   // a névoa no máximo útil
            frameN(3);
            capturePng("B5_we_nevoeiro");
            std::printf("    B5b névoa densa capturada (fogDensity=0.12)\n");
            env2->fogDensity = 0.0f;

            // o STORM (o TIC do clima + o emissor — a base da FASE 3)
            const Handle wxH = g_scene.create("Clima");
            Tic* wxT = g_scene.get(wxH);
            wxT->addComponent<Transform3D>();
            AmbientEnvironment* ae = wxT->addComponent<AmbientEnvironment>();
            ParticleEmitter* pe = wxT->addComponent<ParticleEmitter>();
            ae->weatherType = AmbientEnvironment::kWeatherStorm;
            ae->applyWeatherPreset(pe);   // os valores exatos do preset
            ae->thunderTimer = 0.3f;      // o raio CEDO (determinístico)
            // a câmara olha PARA CIMA (o céu no ecrã: as nuvens e o flash
            // vivem ACIMA do horizonte — com a pose de baixo o FS pinta
            // uHorizon no meio-plano inteiro e o decay é invisível)
            g_camera.pitch = -0.35f;
            frameN(3);
            const std::vector<u8> stFlash = readFrame(&w, &h);
            capturePng("B5_clima_storm");
            // o FLASH determinístico: avança até thunderFlash > 0.5 (o
            // pico) e depois até < 0.05 (o decaimento a 3.0/s) — o pipeline
            // ACENDE e MORRE (o prompt 3.2: nunca persistente)
            auto* aeMut = [&]() -> AmbientEnvironment* {
                auto& aes = g_scene.components().ambients();
                return aes.size() ? &aes.at(0) : nullptr;
            }();
            int fPeak = -1, fDead = -1;
            std::vector<u8> stPeak, stDead;
            for (int i = 0; i < 240; ++i) {
                stepFrame();
                if (fPeak < 0 && aeMut && aeMut->thunderFlash > 0.5f) {
                    fPeak = i;
                    stPeak = readFrame(&w, &h);
                }
                if (fPeak >= 0 && fDead < 0 && aeMut &&
                    aeMut->thunderFlash < 0.05f) {
                    fDead = i;
                    stDead = readFrame(&w, &h);
                    break;
                }
            }
            if (fPeak >= 0) {
                capturePng("B5_clima_storm_flash");
                std::printf("    B5c FLASH no pico no frame +%d (thunderFlash=%.2f)\n",
                            fPeak, aeMut ? aeMut->thunderFlash : -1.0f);
            }
            if (fDead >= 0) {
                capturePng("B5_clima_storm_t1s");
                const long dFlash = diffPixels(stPeak, stDead);
                std::printf("    B5c decaimento no frame +%d: %ld px diferentes pico→morto (%s)\n",
                            fDead, dFlash,
                            dFlash > 100 ? "o flash morre — NUNCA persistente" : "?");
                check(dFlash > 100,
                      "o FLASH do storm acende e MORRE (o pipeline do clima vivo)");
            } else {
                check(false, "o FLASH do storm acendeu no intervalo (o timer corre)");
            }
            // as PARTÍCULAS da chuva: o preset de fábrica é ~1.3 px na
            // 720p (a hipótese F0.4 da 0.12-Z) — a contagem de pixels não-
            // céu na faixa central com a pose DE BAIXO (a vista do dono)
            g_camera.pitch = 0.50f;
            frameN(3);
            const std::vector<u8> stRain = readFrame(&w, &h);
            capturePng("B5_clima_storm_chuva");
            const PixStats rainA = regionStats(stRain, w, h, w / 3, h / 3, 2 * w / 3, 2 * h / 3);
            const PixStats rainRef = regionStats(stFlash, w, h, w / 3, h / 3, 2 * w / 3, 2 * h / 3);
            std::printf("    B5c chuva (centro, pose de baixo): com clima distinct=%ld | sem=%ld\n",
                        rainA.distinct, rainRef.distinct);
            prova("B5c capturado: STORM (flash + chuva) — a base da FASE 3");
            g_scene.destroy(wxH);
            g_camera.pitch = 0.50f;   // a pose de sempre de volta
            frameN(2);
            capturePng("B5d_sem_clima");
            std::printf("    B5d sem o TIC Clima capturado (o azul sai?)\n");
        }

        // ==================================================================
        // BISSEÇÃO POR PASSE (os toggles da 0.12-Z F0) — na cena saudável
        // ==================================================================
        fase("FASE Bbis — bisseção por passe (gldbg::g_masks)");
        gldbg::g_masks.master = true;
        frameN(2);
        const std::vector<u8> base = readFrame(&w, &h);
        capturePng("C_master_todos");
        const PixStats sAll = regionStats(base, w, h, 0, 0, w, h);
        std::printf("    master ON, todos os passes: distinct=%ld nonBg=%ld\n",
                    sAll.distinct, sAll.nonBackground);
        check(sAll.distinct > 32, "master ON com todos os passes desenha o frame");

        // cada passe OFF um de cada vez — o que MUDA localiza o passe
        using GMasks = vv::gldbg::Masks;
        struct Passo { const char* nome; bool GMasks::*campo; };
        const Passo passos[] = {
            {"ceu", &GMasks::sky},         {"worldEnv", &GMasks::worldEnv},
            {"geometria", &GMasks::geometry}, {"grelha", &GMasks::grid},
            {"gizmos", &GMasks::gizmos},   {"ui", &GMasks::ui},
            {"vidro", &GMasks::uiGlass},
        };
        for (const Passo& p : passos) {
            gldbg::g_masks.*p.campo = false;
            frameN(2);
            const std::vector<u8> sem = readFrame(&w, &h);
            char nome[64];
            std::snprintf(nome, sizeof(nome), "C_sem_%s", p.nome);
            capturePng(nome);
            const long d = diffPixels(base, sem);
            std::printf("    sem %-10s: %8ld pixels diferentes (o passe desenha? %s)\n",
                        p.nome, d, d > 0 ? "SIM" : "NÃO");
            char msg[96];
            std::snprintf(msg, sizeof(msg), "toggle %s muda o frame (passe vivo)", p.nome);
            check(d > 0, msg);
            gldbg::g_masks.*p.campo = true;   // repõe
        }

        // "tudo off menos a grelha" — o pedido EXPLÍCITO do dono: a grelha
        // TEM de aparecer sozinha
        GMasks& m = gldbg::g_masks;
        m.sky = false;
        m.worldEnv = false;
        m.geometry = false;
        m.gizmos = false;
        m.ui = false;
        m.uiGlass = false;
        m.grid = true;
        frameN(2);
        const std::vector<u8> so = readFrame(&w, &h);
        capturePng("C_somente_grelha");
        const PixStats sSo = regionStats(so, w, h, w / 4, h / 2, 3 * w / 4, 3 * h / 4);
        std::printf("    somente grelha (centro 3D): nonBg=%ld/%ld\n",
                    sSo.nonBackground, sSo.total);
        check(sSo.nonBackground > 8,
              "«tudo off menos a grelha» mostra a GRELHA (o trio do dono)");
        prova("C: bisseção completa — cada passe isolado com captura");

        // repõe o render normal
        m.master = false;
        m.sky = true;
        m.worldEnv = true;
        m.geometry = true;
        m.grid = true;
        m.gizmos = true;
        m.ui = true;
        m.uiGlass = true;
        frameN(2);

        // B3: câmara na BORDA do mundo (a pose extrema do sintoma 3)
        g_camera.target = Vec3{0.0f, 0.0f, 0.0f};
        g_camera.yaw = 2.35f;
        g_camera.pitch = 0.9f;
        g_camera.dist = 2500.0f;   // longe — o horizonte da grelha aparece
        frameN(3);
        const std::vector<u8> b3 = readFrame(&w, &h);
        capturePng("B3_cam_borda");
        const PixStats s3 = regionStats(b3, w, h, 0, 0, w, h);
        std::printf("    B3 borda: meanLum=%.3f distinct=%ld\n", s3.meanLum, s3.distinct);
        check(s3.distinct > 16, "borda do mundo rasteriza (não há tela morta)");
        prova("B3 capturado: câmara na borda do mundo (dist 2500)");

        // volta à pose normal
        g_camera.yaw = 0.60f;
        g_camera.pitch = 0.50f;
        g_camera.dist = 6.0f;
        frameN(2);

        // B4: PERDA DE CONTEXTO (o ciclo do device: TERM + INIT) — A CAPTURA
        // DO SINTOMA (o dono: «sair e voltar: grelha some, zoom morto»)
        const std::vector<u8> antes = readFrame(&w, &h);
        onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
        onAppCmd(&g_app, APP_CMD_INIT_WINDOW);
        // o PARITY do device: a camada font re-bake de /system/fonts no
        // device; no hospedeiro a fixture repõe (o recreate do gestor
        // falha SEM fontes de sistema — FontAtlas.cpp:12 só tem caminhos
        // Android). O WE TIC não gravado cai (a cena recarrega do storage —
        // o contrato), o Cubo RE-RESOLVE via g_gpu.mesh (o [boot 6/6]).
        if (!g_font.ok()) {
            const char* paths[] = {FONT_FIXTURE};
            g_font.loadFromPaths(paths, 1, 28.0f);
            g_ui.setFont(&g_font);
        }
        selectCuboByName();   // a re-seleção (o reload re-cria os TICs)
        frameN(3);
        const std::vector<u8> depois = readFrame(&w, &h);
        capturePng("B4_pos_perda");
        const long dCtx = diffPixels(antes, depois);
        std::printf("    B4 perda de contexto: %ld pixels diferentes antes/depois\n", dCtx);

        // O VEREDICTO POR PIXEL da recuperação: (1) a grelha, (2) o CUBO
        // (a cena), (3) o GIZMO, (4) o TEXTO da UI — a região do Inspector
        // (x>1000) tem de ter conteúdo de texto como ANTES
        const PixStats c4 = regionStats(depois, w, h, w / 4, h / 2, 3 * w / 4, 3 * h / 4);
        check(c4.nonBackground > 8,
              "após TERM/INIT a GRELHA volta (o gestor da 0.12-Z F1 recria)");
        const PixStats inspAntes = regionStats(antes, w, h, 1010, 90, 1270, 680);
        const PixStats inspDepois = regionStats(depois, w, h, 1010, 90, 1270, 680);
        std::printf("    inspector: antes distinct=%ld | depois distinct=%ld\n",
                    inspAntes.distinct, inspDepois.distinct);

        // O VEREDICTO POR PIXEL (o relatório da FASE 0 cola estes números):
        //   texto da UI (o cabeçalho «Hieraquia» + as entradas da lista) —
        //   ATENÇÃO: readFrame é a leitura GL (origem no FUNDO); o
        //   cabeçalho no PNG está a y≈70-95 → em GL y≈625-654
        const long txtAntes = textPixels(antes, w, 4, 598, 230, 660);
        const long txtDepois = textPixels(depois, w, 4, 598, 230, 660);
        std::printf("    texto «Hieraquia»+lista: antes=%ld px | depois=%ld px\n",
                    txtAntes, txtDepois);
        check(txtAntes > 100, "o TEXTO da UI está presente antes da perda");
        if (txtDepois < txtAntes / 2) {
            prova("ACHADO F0-1: o TEXTO da UI NÃO sobrevive à perda de "
                  "contexto (o sintoma do device) — ver B4_pos_perda.png");
            check(txtDepois < txtAntes / 2,
                  "ACHADO F0-1 REPRODUZIDO: texto da UI morre no TERM/INIT");
        } else {
            check(txtDepois >= txtAntes / 2,
                  "o TEXTO da UI sobrevive à perda de contexto");
        }
        //   a CENA (o cubo desenhado — a região central do viewport)
        const long cubeAntes = textPixels(antes, w, 560, 280, 700, 420, 0.35f);
        const long cubeDepois = textPixels(depois, w, 560, 280, 700, 420, 0.35f);
        std::printf("    cubo (lum>0.35 no centro): antes=%ld px | depois=%ld px\n",
                    cubeAntes, cubeDepois);
        if (cubeDepois < cubeAntes / 2) {
            prova("ACHADO F0-2: a CENA (o cubo) NÃO sobrevive à perda de "
                  "contexto — ver B4_pos_perda.png");
            check(cubeDepois < cubeAntes / 2,
                  "ACHADO F0-2 REPRODUZIDO: a cena morre no TERM/INIT");
        } else {
            check(cubeDepois >= cubeAntes / 2,
                  "a CENA sobrevive à perda de contexto");
        }
        prova("B4 capturado: frame após perda de contexto real (EGL destruído)");
    }

    // ======================================================================
    // FASE C-BB — A CAIXA NEGRA (F4-Z2): o snapshot .txt + PNG no GLES real
    // ======================================================================
    fase("FASE C-BB — a caixa negra");
    {
        blackbox::arm("prova raster_z2");
        frame();          // o PONTO SEGURO do frame captura o backbuffer
        frame();          // o reaper junta o worker no frame seguinte
        check(blackbox::waitIdle(10000), "o job da caixa negra termina");
        // o ficheiro nasce no dir de logs (elog) ou no fallback /tmp
        std::vector<std::string> bases;
        const char* d = elog::dir();
        if (d && d[0]) bases.push_back(std::string(d));
        bases.push_back("/tmp/goni_blackbox");
        std::string txtPath, pngPath;
        for (const std::string& b : bases) {
            const std::string pat1 = b + "/blackbox-";
            for (const std::string& cand : {std::string(), std::string()}) {
                (void)cand;
            }
        }
        // procura simples: lista os 2 dirs por ls via std::filesystem
        for (const std::string& b : bases) {
            if (!txtPath.empty()) break;
            std::error_code ec;
            std::filesystem::directory_iterator it(b, ec), end;
            for (; !ec && it != end; it.increment(ec)) {
                const std::string n = it->path().filename().string();
                if (n.rfind("blackbox-", 0) == 0 &&
                    n.size() > 4 && n.substr(n.size() - 4) == ".txt" &&
                    txtPath.empty()) {
                    txtPath = it->path().string();
                    pngPath = txtPath.substr(0, txtPath.size() - 4) + ".png";
                }
            }
        }
        check(!txtPath.empty(), "o .txt da caixa negra existe");
        if (!txtPath.empty()) {
            std::string txt;
            FILE* f = std::fopen(txtPath.c_str(), "rb");
            if (f) {
                char buf[65536];
                const size_t n = std::fread(buf, 1, sizeof(buf) - 1, f);
                buf[n] = 0;
                txt = buf;
                std::fclose(f);
            }
            check(txt.find("== CAIXA NEGRA G.ONE VV ==") != std::string::npos,
                  "o .txt tem o cabeçalho");
            check(txt.find("gatilho: prova raster_z2") != std::string::npos,
                  "o .txt tem o gatilho");
            check(txt.find("GL_RENDERER: llvmpipe") != std::string::npos,
                  "o .txt tem o renderer REAL");
            check(txt.find("contexto:") != std::string::npos &&
                      txt.find("camara:") != std::string::npos &&
                      txt.find("recursos GL") != std::string::npos &&
                      txt.find("ULTIMAS 200 LINHAS") != std::string::npos,
                  "o .txt tem as secções do contrato");
        }
        std::FILE* pf = pngPath.empty()
                            ? nullptr
                            : std::fopen(pngPath.c_str(), "rb");
        check(pf != nullptr, "o .png da caixa negra existe");
        if (pf) {
            std::fseek(pf, 0, SEEK_END);
            const long sz = std::ftell(pf);
            std::fclose(pf);
            check(sz > 1024, "o PNG tem conteúdo (acima de 1KB)");
        }
        std::printf("    [BB] txt=%s png=%s\n", txtPath.c_str(),
                    pngPath.c_str());
        // F4-Z2: o join do worker (o reap corre no frame — sem ESTE frame
        // o std::thread morria joinable no fim = terminate)
        frame();   // o reap do frame junta o worker (nunca joinable ao sair)
        check(blackbox::waitIdle(1000), "o worker da caixa juntado (idle)");
    }

    // ======================================================================
    // FASE D — RELATÓRIO
    // ======================================================================
    fase("FASE D — resumo");
    {
        const std::string rep = std::string(kOutDir) + "/RELATORIO-FASE0.txt";
        FILE* f = std::fopen(rep.c_str(), "w");
        if (f) {
            std::fprintf(f, "RASTER Z2 — FASE 0 (0.12-Z2) — capturas ANTES\n");
            std::fprintf(f, "GL_RENDERER: %s\n", glGetString(GL_RENDERER));
            std::fprintf(f, "GL_VERSION: %s\n", glGetString(GL_VERSION));
            std::fprintf(f, "checks=%d falhas=%d provas=%d\n", g_checks,
                         g_failed, g_provas);
            std::fclose(f);
        }
        std::printf("    relatório: %s (checks=%d falhas=%d provas=%d)\n",
                    rep.c_str(), g_checks, g_failed, g_provas);
    }

    onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
    std::printf("\n%s (falhas=%d)\n", g_failed ? "RASTER Z2 FALHOU" : "RASTER Z2 OK",
                g_failed);
    return g_failed ? 1 : 0;
}
