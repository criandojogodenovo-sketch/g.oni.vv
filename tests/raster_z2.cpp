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

int fuzzRun(int cycles, u32 seed) {
    std::printf("\n== FASE 1 — FUZZING DO CONTEXTO (%d ciclos, seed %u) ==\n",
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
        onAppCmd(&g_app, APP_CMD_TERM_WINDOW);
        onAppCmd(&g_app, APP_CMD_INIT_WINDOW);
        loadFixtureFontIfDead();       // o parity da fonte (hospedeiro)
        selectCuboByName();            // o reload re-cria os TICs
        if (gesto) {
            g_input.injectUp(0);       // o dedo solta (o device também solta)
        }
        // o cenário re-aplica (os TICs não gravados morrem no reload — o
        // contrato; o device idem: o utilizador re-põe ou grava a cena) —
        // a MESMA applyScenario do pré-loss
        applyScenario(scenario);
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

}  // namespace

int main(int argc, char** argv) {
    const bool fuzzMode = argc > 1 && std::strcmp(argv[1], "--fuzz") == 0;
    const bool sky2Mode = argc > 1 && std::strcmp(argv[1], "--sky2") == 0;
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
    if (fuzzMode) {
        // FASE 1: o fuzzing usa o MESMO boot; as fases A-D não correm
        const int f = fuzzRun(fuzzCycles, fuzzSeed);
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
