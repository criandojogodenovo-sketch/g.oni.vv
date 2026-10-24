// tests/test_012z2_f3.cpp — 0.12-Z2 FASE 3: OS PINS DO CLIMA VISÍVEL.
//
// Os achados do F0.4 fechados na FONTE (o pedido do dono: o clima tem de
// ser VISÍVEL no device, não só correto no CI):
//
//   weather_min_px_pura    — o piso em píxeis: a pura fx::minWorldPerDepth
//                            (3 px @720p = 0.004813 mundo por profundidade 1
//                            com fov 60°) + a orto + os guards + o VS replica
//                            (uMinWorldPerDepth + o max com o w do centro)
//   weather_follow_pura    — o emissor SEGUE a câmara: fx::followOrigin
//                            (eye + raio·0.5) + fx::spawnScale (o buffer
//                            unitário·0.15 escala) + o PE spawnRadius (os
//                            defaults/clamps) + os presets (rain 14 / snow
//                            12 / NONE repõe o jorro) + o ramo no main
//   weather_budget_mecan   — o ORÇAMENTO do FPS: a EMA degrada abaixo de
//                            50 fps (×0.85, piso 0.25), recupera acima de
//                            58, o dt ≤ 0 não mexe, o count nunca mata (o
//                            piso 1) nem excede o pedido
//   weather_raio_serialize — o .goni transporta o raio (SÓ ≠ default — o
//                            .goni antigo carrega byte a byte) + o
//                            roundtrip + os defaults fora do dump
//
// GLES 3.0: zero compute shaders — as puras C++ são o contrato, o GLSL
// replica LINHA A LINHA (o padrão Pbr.h/SkyRenderer.h; o R-073 aferra).
#include "TestFramework.h"
#include "components/AmbientEnvironment.h"   // os presets (o raio do clima)
#include "components/ParticleEmitter.h"      // o spawnRadius (o volume)
#include "components/WindTIC.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "render/ParticleShader.h"           // as puras fx:: + o VS
#include <cmath>
#include <cstdio>
#include <string>

using namespace vv;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

std::string repoRoot() {
    if (const char* e = std::getenv("GONI_ROOT_OVERRIDE")) return e;
#ifdef REPO_ROOT
    return REPO_ROOT;
#else
    return ".";
#endif
}

int countSub(const std::string& hay, const std::string& needle) {
    int n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos;
         p = hay.find(needle, p + needle.size())) {
        ++n;
    }
    return n;
}

bool readFile(const std::string& path, std::string& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    std::fclose(f);
    return true;
}

} // namespace

// ---- PIN weather_min_px_pura (0.12-Z2 F3) ---------------------------------
TEST(weather_min_px_pura) {
    // (1) A CONTA: 3 px @720p com fov 60° → tanHalf 0.57735 — o
    // tamanho-mundo por profundidade 1 é 3·2·0.57735/720 = 0.004811
    {
        const f32 tanHalf = std::tan(1.0472f * 0.5f);   // a orbit (rad)
        const f32 mw = fx::minWorldPerDepth(fx::kMinParticlePx, tanHalf,
                                            720.0f);
        EXPECT_MSG(nearEq(mw, 3.0f * 2.0f * tanHalf / 720.0f, 1e-7f),
                   "weather_min_px_pura: a pura não é minPx·2·tanHalf/viewH");
        // a 720p/dist 7 a gota do preset (0.015 m) media ~1.3 px (o F0.4
        // PROVADO: invisível) — o piso·profundidade EXCEDE o tamanho do
        // preset (o max() do VS é o que sobe o tamanho: 0.004811·7 =
        // 0.0337 m > 0.015 m)
        EXPECT_MSG(mw * 7.0f > 0.015f,
                   "weather_min_px_pura: o piso·7 não excede a gota do "
                   "preset (0.015 m) — o max() do VS nada faria");
        EXPECT_MSG(nearEq(fx::kMinParticlePx, 3.0f),
                   "weather_min_px_pura: o piso do dono (2–3 px @720p) "
                   "saiu de 3.0");
        // os guards: viewH ≤ 0 ou tanHalf ≤ 0 = PISO MORTO (o caminho de
        // sempre byte a byte)
        EXPECT_MSG(fx::minWorldPerDepth(3.0f, tanHalf, 0.0f) == 0.0f &&
                       fx::minWorldPerDepth(3.0f, 0.0f, 720.0f) == 0.0f &&
                       fx::minWorldPerDepth(3.0f, -1.0f, 720.0f) == 0.0f,
                   "weather_min_px_pura: os guards do piso (0.0 = morto) "
                   "saíram");
        // a ORTO: o w do clip fica 1.0 — o tamanho é CONSTANTE
        // minPx·2·meiaAltura/viewH (o orthoSize 5 @720p)
        const f32 mo = fx::minWorldOrtho(3.0f, 5.0f, 720.0f);
        EXPECT_MSG(nearEq(mo, 3.0f * 2.0f * 5.0f / 720.0f, 1e-7f),
                   "weather_min_px_pura: a orto não é minPx·2·meiaAltura/"
                   "viewH");
        EXPECT_MSG(fx::minWorldOrtho(3.0f, 0.0f, 720.0f) == 0.0f,
                   "weather_min_px_pura: o guard da orto saiu");
        // a conversão graus do CameraComp (o deg2rad da casa)
        EXPECT_MSG(nearEq(fx::tanHalfFovDeg(60.0f), std::tan(0.5235988f),
                          1e-4f),
                   "weather_min_px_pura: o tanHalfFovDeg divergiu do "
                   "deg2rad da casa");
    }
    // (2) O VS replica: o uniform + o max com o w do CENTRO (a fonte SEM
    // comentários — o padrão M-Yd1/R-061)
    {
        std::string src;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/render/"
                        "ParticleShader.h",
                        src));
        std::string code = test::stripCppComments(src);
        EXPECT_MSG(countSub(code, "uniform float uMinWorldPerDepth;") == 1,
                   "weather_min_px_pura: o uniform uMinWorldPerDepth saiu "
                   "do VS");
        EXPECT_MSG(countSub(code,
                            "max(sz, uMinWorldPerDepth * center.w)") == 1,
                   "weather_min_px_pura: o MAX com o w do centro saiu do "
                   "VS (o piso morto — o F0.4 volta)");
        EXPECT_MSG(countSub(code, "vec4 center = uVP * vec4(p, 1.0);") == 1,
                   "weather_min_px_pura: a profundidade do centro saiu do "
                   "VS (o w é o que escala píxeis)");
        // (3) O CAMINHO NO MAIN — a pura alimenta o Params com a câmara
        // do frame e a ALTURA do viewport real
        std::string mainSrc;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/platform/main.cpp",
                        mainSrc));
        EXPECT_MSG(countSub(mainSrc,
                            "fx::minWorldPerDepth(fx::kMinParticlePx, "
                            "tanHalf,") == 1,
                   "weather_min_px_pura: o main deixou de calcular o piso "
                   "pela FONTE ÚNICA");
        EXPECT_MSG(countSub(mainSrc, "pp.minWorldPerDepth = partMinW;") == 1,
                   "weather_min_px_pura: o main deixou de passar o piso ao "
                   "draw");
    }
}

// ---- PIN weather_follow_pura (0.12-Z2 F3) ----------------------------------
TEST(weather_follow_pura) {
    // (1) A PURA followOrigin: ALVO + lift = raio·0.5 (o volume ACIMA do
    // ponto que a câmara OLHA — a chuva nasce a cair)
    {
        const f32 aim[3] = {10.0f, 20.0f, 30.0f};
        f32 out[3];
        fx::followOrigin(aim, 14.0f, out);
        EXPECT_MSG(nearEq(out[0], 10.0f) && nearEq(out[1], 27.0f) &&
                       nearEq(out[2], 30.0f),
                   "weather_follow_pura: o followOrigin não é alvo + "
                   "(0, raio·0.5, 0) — (10,20,30) r14 → (10,27,30)");
        // o raio de fábrica (0.15) levanta 0.075 — o jorro da F1 fica
        // quase na origem (a diferença é o volume, não a origem)
        fx::followOrigin(aim, 0.15f, out);
        EXPECT_MSG(nearEq(out[1], 20.075f),
                   "weather_follow_pura: o lift do jorro (raio·0.5) "
                   "divergiu");
        // a ESCALA do volume: o buffer nasce unitário ×0.15 — o raio
        // escala o offset; o default devolve 1.0 (a F1 byte a byte)
        EXPECT_MSG(nearEq(fx::spawnScale(0.15f, 0.15f), 1.0f),
                   "weather_follow_pura: a escala do jorro default não é "
                   "1.0 (a F1 tem de ficar byte a byte)");
        EXPECT_MSG(nearEq(fx::spawnScale(1.5f, 0.15f), 10.0f),
                   "weather_follow_pura: a escala do volume divergiu (o "
                   "raio 1.5 sobre o seed 0.15 é 10×)");
    }
    // (2) O COMPONENTE: o default É o jorro da F1 + a clamp de defesa
    {
        ParticleEmitter pe;
        EXPECT_MSG(nearEq(pe.spawnRadius,
                          ParticleEmitter::kSpawnRadiusDefault),
                   "weather_follow_pura: o default do raio saiu do jorro "
                   "da F1 (0.15)");
        pe.spawnRadius = 100.0f;
        pe.clampFields();
        EXPECT_MSG(nearEq(pe.spawnRadius,
                          ParticleEmitter::kSpawnRadiusMax),
                   "weather_follow_pura: a clamp do teto do raio (60) "
                   "morreu");
        pe.spawnRadius = 0.01f;
        pe.clampFields();
        EXPECT_MSG(nearEq(pe.spawnRadius,
                          ParticleEmitter::kSpawnRadiusMin),
                   "weather_follow_pura: a clamp do piso do raio (0.15) "
                   "morreu (o volume não degenera em ponto)");
    }
    // (3) OS PRESETS transportam o volume (o clima vive À VOLTA da
    // câmara) e o NONE repõe o JORRO (o emissor fica como antes)
    {
        AmbientEnvironment ae;
        ParticleEmitter pe;
        ae.intensity = 1.0f;
        ae.weatherType = AmbientEnvironment::kWeatherRain;
        ae.applyWeatherPreset(&pe);
        EXPECT_MSG(nearEq(pe.spawnRadius, AmbientEnvironment::kRainRadius),
                   "weather_follow_pura: o preset RAIN não transporta o "
                   "raio do volume (kRainRadius)");
        ae.weatherType = AmbientEnvironment::kWeatherSnow;
        ae.applyWeatherPreset(&pe);
        EXPECT_MSG(nearEq(pe.spawnRadius, AmbientEnvironment::kSnowRadius),
                   "weather_follow_pura: o preset SNOW não transporta o "
                   "raio do volume (kSnowRadius)");
        ae.weatherType = AmbientEnvironment::kWeatherStorm;
        ae.applyWeatherPreset(&pe);
        EXPECT_MSG(nearEq(pe.spawnRadius, AmbientEnvironment::kRainRadius),
                   "weather_follow_pura: o preset STORM não transporta o "
                   "raio do volume");
        ae.weatherType = AmbientEnvironment::kWeatherNone;
        ae.applyWeatherPreset(&pe);
        EXPECT_MSG(nearEq(pe.spawnRadius,
                          ParticleEmitter::kSpawnRadiusDefault),
                   "weather_follow_pura: o NONE não repõe o JORRO (o raio "
                   "de fábrica 0.15)");
    }
    // (4) O RAMO NO MAIN: o clima segue (followOrigin + uSpawnScale) e o
    // jorro fica na origem do TIC (byte a byte)
    {
        std::string mainSrc;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/platform/main.cpp",
                        mainSrc));
        EXPECT_MSG(countSub(mainSrc, "fx::followOrigin(aim, "
                                     "pes.at(i).spawnRadius, origem);") == 1,
                   "weather_follow_pura: o main deixou de seguir a câmara "
                   "pela FONTE ÚNICA followOrigin");
        EXPECT_MSG(countSub(mainSrc,
                            "pp.spawnScale = fx::spawnScale(") == 1,
                   "weather_follow_pura: o main deixou de escalar o "
                   "volume (uSpawnScale)");
        EXPECT_MSG(countSub(mainSrc,
                            "ae->weatherType != "
                            "AmbientEnvironment::kWeatherNone") == 1,
                   "weather_follow_pura: o teste do CLIMA (AE ativo com "
                   "tipo ≠ NONE) saiu do ramo de partículas");
    }
}

// ---- PIN weather_budget_mecan (0.12-Z2 F3) ---------------------------------
TEST(weather_budget_mecan) {
    // (1) A DEGRADAÇÃO: 40 fps (dt 25 ms) em série — a EMA passa o piso
    // (50) e o scale desce ×0.85 até ao piso 0.25 (nunca abaixo)
    {
        fx::WeatherBudget b;
        EXPECT_MSG(nearEq(b.scale, 1.0f),
                   "weather_budget_mecan: o orçamento não nasce cheio");
        for (int i = 0; i < 120; ++i) {
            b.step(1.0f / 40.0f);
        }
        EXPECT_MSG(b.scale > fx::WeatherBudget::kScaleMin - 1e-6f &&
                       b.scale < 1.0f,
                   "weather_budget_mecan: 40 fps não degrada o orçamento "
                   "(o scale ficou 1.0)");
        EXPECT_MSG(nearEq(b.scale, fx::WeatherBudget::kScaleMin),
                   "weather_budget_mecan: a degradação passou do PISO "
                   "(0.25 — o count mínimo do clima)");
        // (2) O COUNT: o teto do clima escala com o piso (1500×0.25=375)
        EXPECT_MSG(b.count(1500) == 375,
                   "weather_budget_mecan: o count não escala (1500 com o "
                   "piso é 375)");
        EXPECT_MSG(b.count(0) == 0 && b.count(-5) == 0,
                   "weather_budget_mecan: o count de um emissor morto "
                   "deixou de ser 0");
        EXPECT_MSG(b.count(1) == 1,
                   "weather_budget_mecan: o piso do count não é 1 (a "
                   "escala degrada o COUNT, não mata)");
    }
    // (3) A RECUPERAÇÃO: 60 fps (dt 16.67 ms) em série — o scale volta a
    // 1.0 (o histerético 58 evita o dente-de-serra)
    {
        fx::WeatherBudget b;
        for (int i = 0; i < 120; ++i) {
            b.step(1.0f / 40.0f);
        }
        ASSERT(b.scale < 1.0f);
        for (int i = 0; i < 240; ++i) {
            b.step(1.0f / 60.0f);
        }
        EXPECT_MSG(nearEq(b.scale, 1.0f),
                   "weather_budget_mecan: 60 fps não recupera o orçamento "
                   "(o scale ficou degradado)");
        EXPECT_MSG(b.count(1500) == 1500,
                   "weather_budget_mecan: o count não volta ao teto do "
                   "componente");
    }
    // (4) O dt CONGELADO não mexe (a pausa/harness não inventa orçamento)
    {
        fx::WeatherBudget b;
        for (int i = 0; i < 60; ++i) {
            b.step(0.0f);
        }
        EXPECT_MSG(nearEq(b.scale, 1.0f) &&
                       nearEq(b.emaDt, 1.0f / 60.0f),
                   "weather_budget_mecan: o dt 0 mexeu no orçamento");
        // (5) o histerético: 52 fps (entre 50 e 58) MANTÉM (nem degrada
        // nem recupera — a banda morta)
        fx::WeatherBudget b2;
        b2.scale = 0.5f;
        b2.emaDt = 1.0f / 52.0f;
        b2.step(1.0f / 52.0f);
        EXPECT_MSG(nearEq(b2.scale, 0.5f),
                   "weather_budget_mecan: a banda morta (50..58) deixou de "
                   "ser estável");
    }
}

// ---- PIN weather_raio_serialize (0.12-Z2 F3) --------------------------------
TEST(weather_raio_serialize) {
    // (1) O ROUNDTRIP: o raio ≠ default viaja no "rad" e volta
    {
        Scene a;
        const Handle h = a.create("Chuva");
        a.get(h)->addComponent<Transform3D>();
        ParticleEmitter* pe = a.get(h)->addComponent<ParticleEmitter>();
        pe->spawnRadius = 14.0f;
        const std::string dump = SceneSerializer::dump(a);
        EXPECT_MSG(countSub(dump, "\"ParticleEmitter\"") == 1 &&
                       countSub(dump, "\"rad\"") == 1,
                   "weather_raio_serialize: o .goni NÃO guarda o raio do "
                   "volume (rad)");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const ParticleEmitter* pe2 =
            b.get(b.find("Chuva"))->getComponent<ParticleEmitter>();
        ASSERT(pe2 != nullptr);
        EXPECT_MSG(nearEq(pe2->spawnRadius, 14.0f),
                   "weather_raio_serialize: o raio não volta do .goni");
    }
    // (2) OS DEFAULTS ficam FORA do dump (o .goni antigo carrega byte a
    // byte — o forward-compat da casa)
    {
        Scene c;
        const Handle hc = c.create("limpo");
        c.get(hc)->addComponent<Transform3D>();
        c.get(hc)->addComponent<ParticleEmitter>();
        const std::string dump2 = SceneSerializer::dump(c);
        EXPECT_MSG(countSub(dump2, "\"rad\"") == 0,
                   "weather_raio_serialize: o default do raio INVADIU o "
                   ".goni (o jorro 0.15 é o default omitido)");
    }
}
