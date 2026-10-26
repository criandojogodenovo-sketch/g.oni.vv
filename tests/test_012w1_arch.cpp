// tests/test_012w1_arch.cpp — 0.12-W-1: A ARQUITETURA DO CÉU E DO CLIMA.
//
// Os pins do prompt do dono (a DECISÃO DE ARQUITETURA OBRIGATÓRIA):
//   world_env_optional        — o céu procedural EXISTE por defeito: sem
//                               WorldEnvironment o Params::fromEnvironment
//                               usa OS DEFAULTS (tod 12.0, cobertura 0.3)
//   world_env_override        — COM WorldEnvironment o céu lê AS PROPRIEDADES
//                               dele (a cobertura, o relógio, as estrelas/lua)
//   ambient_env_finds_emitter — o AmbientEnvironment ENCONTRA o
//                               ParticleEmitter pelo NOME (targetEmitter) e
//                               configura-o (o fluxo dos DOIS TICs)
//   ambient_env_logs_warning  — sem emissor: o LOG WARN 1× edge-triggered
//                               (o elog::lineCount sobe UMA vez por ausência)
//   grid_no_context_corruption— a grelha NÃO corrompe o estado GL: a janela
//                               EXATA da W-1 (depth/cull OFF + blend durante;
//                               TUDO reposto depois) e a ordem céu→grelha→TICs
//   stars_visible_at_night    — as estrelas aparecem às 22:00 (o gate da
//                               grade + o limiar do starDensity do utilizador)
//   moon_visible_at_night     — a lua aparece às 22:00 (disco oposto ao sol,
//                               as FASES pelo terminador, o glow subtil)
//   particle_emitter_has_transform — o ParticleEmitter TEM Transform
//                               obrigatório (o preset traz; o draw exige; o
//                               gizmo arrasta O TRANSFORM — 0.12-V)
//   w1_goni_compat            — os campos novos só viajam ≠ default (o
//                               .goni da Y-E/Y-F2 carrega byte a byte)
//
// O mapeamento WorldEnvironment→Params é a FONTE ÚNICA do SkyRenderer.h
// (Params::fromEnvironment) — o main e ESTES pins partilham a MESMA conta.
#include "TestFramework.h"
#include "components/AmbientEnvironment.h"   // 0.12-W-1: o controlador
#include "components/Environment.h"          // o WorldEnvironment
#include "components/ParticleEmitter.h"
#include "components/Transform3D.h"
#include "core/Presets.h"                    // findWeatherTarget (a fonte)
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/EngineLog.h"              // lineCount (o pin do warn)
#include "render/SkyRenderer.h"              // as puras sky:: + o Params
#include "render/Grid.h"                     // 0.12-W-1: os degraus do zoom
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
bool readFile(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    std::fclose(f);
    return true;
}
u32 countSub(const std::string& s, const char* needle) {
    u32 n = 0;
    size_t p = 0;
    while ((p = s.find(needle, p)) != std::string::npos) {
        ++n;
        p += 1;
    }
    return n;
}
} // namespace

// ---- (1) O CÉU POR DEFEITO — sem WorldEnvironment os DEFAULTS da casa ----
TEST(world_env_optional) {
    // os DEFAULTS do dono (o prompt W-1): tod 12.0, cobertura 0.3
    EXPECT_MSG(nearEq(sky::kNoEnvTod, 12.0f),
               "world_env_optional: o tod default sem WE não é 12.0");
    EXPECT_MSG(nearEq(sky::kNoEnvCoverage, 0.3f),
               "world_env_optional: a cobertura default sem WE não é 0.3");
    // a grade do meio-dia sobre as cores da identidade Y-E (o w=0 devolve
    // as cores do utilizador EXATAS — a identidade da D byte a byte)
    sky::SkyGrade g{};
    sky::dayNightGrade(sky::kNoEnvTod, Environment::kHorizonDefault,
                       Environment::kZenithDefault, Environment::kSunColDefault,
                       g);
    for (int i = 0; i < 3; ++i) {
        EXPECT_MSG(nearEq(g.hor[i], Environment::kHorizonDefault[i], 1e-4f) &&
                       nearEq(g.zen[i], Environment::kZenithDefault[i], 1e-4f),
                   "world_env_optional: a grade do meio-dia não devolve as "
                   "cores da identidade");
    }
    EXPECT_MSG(nearEq(g.stars, 0.0f),
               "world_env_optional: o gate das estrelas ao meio-dia não é 0");
    // o mapeamento nullptr → os defaults TODOS (a decisão 1 do dono)
    SkyRenderer::Params sp;
    sp.fromEnvironment(nullptr, g, 0.0f, 12.34f);
    EXPECT_MSG(nearEq(sp.coverage, sky::kNoEnvCoverage),
               "world_env_optional: sem WE a cobertura não é o default 0.3");
    EXPECT_MSG(nearEq(sp.time, 12.34f),
               "world_env_optional: o relógio das nuvens não viaja");
    EXPECT_MSG(nearEq(sp.speed, Environment::kSpeedDefault) &&
                   nearEq(sp.height, Environment::kHeightDefault),
               "world_env_optional: sem WE o vento/altura das nuvens não "
               "são os defaults");
    EXPECT_MSG(nearEq(sp.stars, 0.0f),
               "world_env_optional: sem WE (meio-dia) as estrelas não "
               "podiam estar acesas");
    EXPECT_MSG(nearEq(sp.starThresh, 184.0f, 1e-3f),
               "world_env_optional: sem WE o limiar das estrelas não é o "
               "184 da Y-E (starDensity 0.5)");
    EXPECT_MSG(nearEq(sp.starBright, Environment::kStarBrightDefault),
               "world_env_optional: sem WE o brilho das estrelas não é o "
               "default do dono");
    EXPECT_MSG(nearEq(sp.moonPhase, Environment::kMoonPhaseDefault) &&
                   nearEq(sp.moonBright, Environment::kMoonBrightDefault),
               "world_env_optional: sem WE a lua não está nos defaults");
    // a LUA sem WE: oposta ao sol do meio-dia — ABAIXO do horizonte
    // (invisível de dia, a posição honesta)
    EXPECT_MSG(sp.moonDir[1] < 0.0f,
               "world_env_optional: a lua do meio-dia devia estar sob o "
               "horizonte (oposta ao sol)");
    f32 sun12[3];
    sky::sunDirFromTime(sky::kNoEnvTod, sun12);
    EXPECT_MSG(nearEq(sp.sunDir[0], sun12[0], 1e-4f) &&
                   nearEq(sp.sunDir[1], sun12[1], 1e-4f),
               "world_env_optional: sem WE o sol não é o do relógio default");
    // o SOL do meio-dia está ALTO (o zénite da identidade)
    EXPECT_MSG(sun12[1] > 0.9f,
               "world_env_optional: o sol do meio-dia não está no zénite");
}

// ---- (2) O OVERRIDE — com WorldEnvironment o céu lê AS PROPRIEDADES ------
TEST(world_env_override) {
    Environment env;
    env.skyType = Environment::kSkyTypeProcedural;
    env.timeOfDay = 22.0f;         // a noite do dono
    env.cloudCoverage = 0.7f;
    env.cloudSpeed = 0.11f;
    env.cloudHeight = 2.5f;
    env.starDensity = 1.0f;        // o campo CHEIO (limiar 112)
    env.starBrightness = 0.3f;
    env.moonPhase = 0.25f;         // o quarto crescente
    env.moonBrightness = 0.9f;
    env.clampFields();
    sky::SkyGrade g{};
    sky::dayNightGrade(env.timeOfDay, env.skyColorHorizon,
                       env.skyColorZenith, env.sunColor, g);
    SkyRenderer::Params sp;
    sp.fromEnvironment(&env, g, 0.5f, 7.77f);
    EXPECT_MSG(nearEq(sp.coverage, 0.7f),
               "world_env_override: o céu não lê a cobertura do WE");
    EXPECT_MSG(nearEq(sp.speed, 0.11f) && nearEq(sp.height, 2.5f),
               "world_env_override: o céu não lê o vento/altura do WE");
    EXPECT_MSG(nearEq(sp.thunder, 0.5f),
               "world_env_override: o flash da trovoada não viaja");
    EXPECT_MSG(nearEq(sp.time, 7.77f),
               "world_env_override: o relógio das nuvens não viaja");
    EXPECT_MSG(g.stars > 0.99f && nearEq(sp.stars, g.stars),
               "world_env_override: às 22:00 o gate das estrelas não é ~1");
    EXPECT_MSG(nearEq(sp.starThresh, sky::starThreshFromDensity(1.0f)) &&
                   nearEq(sp.starThresh, 112.0f, 1e-3f),
               "world_env_override: o limiar não nasce do starDensity do WE");
    EXPECT_MSG(nearEq(sp.starBright, 0.3f),
               "world_env_override: o brilho não nasce do starBrightness");
    EXPECT_MSG(nearEq(sp.moonPhase, 0.25f) &&
                   nearEq(sp.moonBright, 0.9f),
               "world_env_override: a fase/brilho da lua não são do WE");
    f32 m22[3];
    sky::moonDirFromTime(22.0f, m22);
    EXPECT_MSG(nearEq(sp.moonDir[0], m22[0], 1e-4f) &&
                   nearEq(sp.moonDir[1], m22[1], 1e-4f),
               "world_env_override: a lua não é a oposta ao sol do relógio");
    EXPECT_MSG(m22[1] > 0.5f,
               "world_env_override: às 22:00 a lua não está alta");
    f32 s22[3];
    sky::sunDirFromTime(22.0f, s22);
    EXPECT_MSG(nearEq(sp.moonDir[0], -s22[0], 1e-4f) &&
                   nearEq(sp.moonDir[1], -s22[1], 1e-4f),
               "world_env_override: a lua não é EXATAMENTE oposta ao sol");
    // as cores graduadas viajam (a grade 22h = a paleta da noite)
    for (int i = 0; i < 3; ++i) {
        EXPECT_MSG(nearEq(sp.horizon[i], g.hor[i]) &&
                       nearEq(sp.zenith[i], g.zen[i]) &&
                       nearEq(sp.sunCol[i], g.sun[i]),
                   "world_env_override: as cores graduadas não viajam");
    }
}

// ---- (3) O CONTROLADOR ENCONTRA O EMISSOR PELO NOME -----------------------
TEST(ambient_env_finds_emitter) {
    // o fluxo do dono: DOIS TICs separados — o "RainEmitter" (PE+Transform)
    // e o "WeatherController" (AmbientEnvironment com targetEmitter)
    Scene s;
    const Handle hpe = createTicFromPreset(s, PresetKind::ParticleEmitter,
                                           nullptr, nullptr);
    ASSERT(hpe.valid());
    s.get(hpe)->name = "RainEmitter";   // o nome do fluxo do dono
    const Handle hae = createTicFromPreset(s, PresetKind::WeatherController,
                                           nullptr, nullptr);
    ASSERT(hae.valid());
    Tic* aeTic = s.get(hae);
    AmbientEnvironment* ae = aeTic->getComponent<AmbientEnvironment>();
    ASSERT(ae != nullptr);
    EXPECT_MSG(std::string(presetName(PresetKind::WeatherController)) ==
                   "WeatherController",
               "ambient_env_finds_emitter: o nome do preset saiu do contrato");
    // o CONTROLADOR não tem partículas NO PRÓPRIO TIC (a decisão do dono)
    EXPECT_MSG(aeTic->getComponent<ParticleEmitter>() == nullptr,
               "ambient_env_finds_emitter: o WeatherController NASCEU com "
               "ParticleEmitter (é CONTROLADOR — o emissor é um TIC à parte)");
    // o DEFAULT do alvo é o nome do prompt
    EXPECT_MSG(ae->targetEmitter == "RainEmitter",
               "ambient_env_finds_emitter: o default do targetEmitter não é "
               "RainEmitter");
    // a procura PELO NOME acha o emissor do OUTRO TIC
    ParticleEmitter* found = findWeatherTarget(s, *aeTic, *ae);
    EXPECT_MSG(found == s.get(hpe)->getComponent<ParticleEmitter>(),
               "ambient_env_finds_emitter: o findWeatherTarget não acha o "
               "ParticleEmitter pelo NOME");
    // e a APLICAÇÃO configura-o com o preset da chuva (os valores do dono)
    ae->weatherType = AmbientEnvironment::kWeatherRain;
    ae->intensity = 0.5f;
    EXPECT_MSG(applyWeatherToTarget(s, *aeTic, *ae),
               "ambient_env_finds_emitter: a aplicação ao alvo por nome "
               "falhou com o alvo EXISTENTE");
    EXPECT_MSG(found != nullptr && nearEq(found->gravity[1], -12.0f) &&
                   nearEq(found->velocity[1], -18.0f) &&
                   nearEq(found->lifetime, 1.2f),
               "ambient_env_finds_emitter: o preset da chuva não chegou ao "
               "emissor do OUTRO TIC (grav/vel/vida)");
    EXPECT_MSG(found != nullptr &&
                   found->maxParticles ==
                       static_cast<i32>(1500.0f * 0.5f),
               "ambient_env_finds_emitter: o count não escala com a "
               "intensidade (1500×0.5)");
    EXPECT_MSG(found != nullptr &&
                   nearEq(found->spawnRadius, AmbientEnvironment::kRainRadius),
               "ambient_env_finds_emitter: o volume do clima não chegou ao "
               "emissor (o raio 6 da Z2-F3)");
    // o LEGADO fica intacto: o PE do MESMO TIC ganha à procura por nome
    // (os presets Rain/Snow/Storm da Y-F2 continuam byte a byte)
    Scene s2;
    const Handle hrain = createTicFromPreset(s2, PresetKind::Rain, nullptr,
                                             nullptr);
    ASSERT(hrain.valid());
    Tic* rt = s2.get(hrain);
    AmbientEnvironment* ae2 = rt->getComponent<AmbientEnvironment>();
    ParticleEmitter* own = rt->getComponent<ParticleEmitter>();
    ASSERT(ae2 != nullptr && own != nullptr);
    EXPECT_MSG(findWeatherTarget(s2, *rt, *ae2) == own,
               "ambient_env_finds_emitter: o PE do MESMO TIC deixou de ter "
               "prioridade (o legado da Y-F2)");
    // um TIC com o NOME do alvo mas SEM ParticleEmitter não é alvo
    Scene s3;
    const Handle hbare = s3.create("RainEmitter");
    s3.get(hbare)->addComponent<Transform3D>();
    const Handle haect = createTicFromPreset(s3, PresetKind::WeatherController,
                                             nullptr, nullptr);
    (void)haect;
    AmbientEnvironment* ae3 =
        s3.get(haect)->getComponent<AmbientEnvironment>();
    ASSERT(ae3 != nullptr);
    EXPECT_MSG(findWeatherTarget(s3, *s3.get(haect), *ae3) == nullptr,
               "ambient_env_finds_emitter: um TIC sem ParticleEmitter não é "
               "alvo válido");
}

// ---- (4) O WARN DO ALVO AUSENTE — 1× edge-triggered ------------------------
TEST(ambient_env_logs_warning) {
    Scene s;
    const Handle hae = createTicFromPreset(s, PresetKind::WeatherController,
                                           nullptr, nullptr);
    ASSERT(hae.valid());
    Tic* aeTic = s.get(hae);
    AmbientEnvironment* ae = aeTic->getComponent<AmbientEnvironment>();
    ASSERT(ae != nullptr);
    ae->weatherType = AmbientEnvironment::kWeatherSnow;   // a neve do dono
    // a CRIAÇÃO já avisou (o preset aplica e o alvo ainda não existe — o
    // fluxo honesto); o pin controla o edge: desarma o flag para aferir o
    // 1× de novo (o runtime flag NUNCA serializa)
    ae->targetWarned = false;
    // SEM alvo na cena: o WARN dispara UMA vez (o lineCount é o contador
    // MONÓTONO da casa — sobe em cada linha lógica, mesmo sem ficheiro)
    const unsigned long long before = elog::lineCount();
    EXPECT_MSG(!applyWeatherToTarget(s, *aeTic, *ae),
               "ambient_env_logs_warning: sem alvo a aplicação devia "
               "devolver false");
    EXPECT_MSG(elog::lineCount() == before + 1,
               "ambient_env_logs_warning: o warn do alvo ausente não "
               "escreveu UMA linha (lineCount %llu → %llu)",
               before, elog::lineCount());
    // a SEGUNDA tentativa CALA (o edge-triggered — o spam morre)
    EXPECT_MSG(!applyWeatherToTarget(s, *aeTic, *ae),
               "ambient_env_logs_warning: a segunda ausência aplicou?");
    EXPECT_MSG(elog::lineCount() == before + 1,
               "ambient_env_logs_warning: o warn repetiu (não é "
               "edge-triggered)");
    // o alvo APARECE: aplica, o flag desarma
    const Handle hpe = createTicFromPreset(s, PresetKind::ParticleEmitter,
                                           nullptr, nullptr);
    s.get(hpe)->name = "RainEmitter";
    ParticleEmitter* pe = s.get(hpe)->getComponent<ParticleEmitter>();
    ASSERT(pe != nullptr);
    EXPECT_MSG(applyWeatherToTarget(s, *aeTic, *ae),
               "ambient_env_logs_warning: com o alvo criado a aplicação "
               "falhou");
    EXPECT_MSG(nearEq(pe->gravity[1], -1.2f) && nearEq(pe->lifetime, 4.0f),
               "ambient_env_logs_warning: o preset da NEVE não chegou ao "
               "alvo adotado");
    EXPECT_MSG(!ae->targetWarned,
               "ambient_env_logs_warning: o flag do warn não desarmou com o "
               "alvo de volta");
    // o alvo MORRE: a próxima ausência volta a falar (1× de novo)
    s.destroy(hpe);
    EXPECT_MSG(!applyWeatherToTarget(s, *aeTic, *ae),
               "ambient_env_logs_warning: a ausência rearmada aplicou?");
    EXPECT_MSG(elog::lineCount() == before + 2,
               "ambient_env_logs_warning: a SEGUNDA ausência não avisou "
               "(o edge rearmou)");
}

// ---- (5) A GRELHA NÃO CORROMPE O CONTEXTO GL -------------------------------
TEST(grid_no_context_corruption) {
    // o FONTE é o contrato (o teste de estado do device é do raster W-1):
    // a janela EXATA da W-1 no Grid.cpp, na ORDEM certa (desligar antes do
    // draw, repor depois — o M-W3 mata a reposição e este pin fica VERMELHO)
    std::string g;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/render/Grid.cpp", g));
    const std::string code = test::stripCppComments(g);
    const size_t iDraw = code.find("glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);");
    ASSERT(iDraw != std::string::npos);
    const size_t iDepOff = code.find("glDisable(GL_DEPTH_TEST);");
    const size_t iCullOff = code.find("glDisable(GL_CULL_FACE);");
    const size_t iBlendOn = code.find("glEnable(GL_BLEND);");
    const size_t iBlendFn =
        code.find("glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);");
    const size_t iDepOn = code.find("glEnable(GL_DEPTH_TEST);");
    const size_t iCullOn = code.find("glEnable(GL_CULL_FACE);");
    const size_t iBlendOff = code.find("glDisable(GL_BLEND);");
    EXPECT_MSG(iDepOff != std::string::npos && iCullOff != std::string::npos &&
                   iBlendOn != std::string::npos &&
                   iBlendFn != std::string::npos,
               "grid_no_context_corruption: a ABERTURA da janela W-1 "
               "(depth/cull OFF + blend src-alpha) está incompleta");
    EXPECT_MSG(iDepOn != std::string::npos && iCullOn != std::string::npos &&
                   iBlendOff != std::string::npos,
               "grid_no_context_corruption: o FECHO da janela W-1 (depth/cull "
               "ON + blend OFF) está incompleto — o M-W3 ao vivo");
    EXPECT_MSG(iDepOff < iDraw && iCullOff < iDraw && iBlendOn < iDraw &&
                   iBlendFn < iDraw,
               "grid_no_context_corruption: a grelha desenha SEM a janela "
               "aberta (a abertura tem de vir ANTES do draw)");
    EXPECT_MSG(iDepOn > iDraw && iCullOn > iDraw && iBlendOff > iDraw,
               "grid_no_context_corruption: a reposição tem de vir DEPOIS "
               "do draw (o estado nunca vaza para o passe seguinte)");
    // A ORDEM NO FRAME: céu → GRELHA → geometria (o main é o contrato —
    // «grelha antes do céu» é IMPOSSÍVEL com o passe fullscreen depth-OFF
    // do céu: o quad apagava a grelha; a prova de píxel vive no raster W-1)
    std::string m;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/platform/main.cpp", m));
    const std::string mc = test::stripCppComments(m);
    const size_t iSky = mc.find("g_sky.draw(sp);");
    const size_t iGrid = mc.find("g_grid.draw(vp, camEye, camFocus)");
    const size_t iMeshes = mc.find("comps.meshRenderers()");
    ASSERT(iSky != std::string::npos && iGrid != std::string::npos &&
           iMeshes != std::string::npos);
    EXPECT_MSG(iSky < iGrid && iGrid < iMeshes,
               "grid_no_context_corruption: a ordem do frame saiu do "
               "contrato (céu → grelha → geometria)");
    // A PROJEÇÃO é a da câmara do frame (o vp NUNCA hardcoded)
    EXPECT_MSG(mc.find("g_grid.draw(vp, camEye, camFocus)") !=
                   std::string::npos,
               "grid_no_context_corruption: a grelha deixou de usar a "
               "projeção da câmara (o vp do frame)");
    // O ZOOM: o passo adaptativo pela FONTE ÚNICA (Grid::gridStepForDist —
    // o zoom da câmara manda no passo/fade/extent)
    EXPECT_MSG(g.find("gridStepForDist(focus)") != std::string::npos,
               "grid_no_context_corruption: o passo adaptativo do zoom "
               "sumiu do draw da grelha");
    EXPECT_MSG(nearEq(Grid::gridStepForDist(6.0f), 1.0f) &&
                   nearEq(Grid::gridStepForDist(60.0f), 10.0f, 1e-4f) &&
                   nearEq(Grid::gridStepForDist(600.0f), 100.0f, 1e-3f),
               "grid_no_context_corruption: os degraus do zoom regressaram "
               "(6→1 · 60→10 · 600→100)");
}

// ---- (6) AS ESTRELAS À NOITE (22:00) ---------------------------------------
TEST(stars_visible_at_night) {
    // o GATE da grade: 22:00 = a noite plena (o plateau 21..24)
    sky::SkyGrade g22{};
    sky::dayNightGrade(22.0f, Environment::kHorizonDefault,
                       Environment::kZenithDefault, Environment::kSunColDefault,
                       g22);
    EXPECT_MSG(g22.stars > 0.99f,
               "stars_visible_at_night: o gate às 22:00 não é ~1 (%.3f)",
               (double)g22.stars);
    sky::SkyGrade g12{};
    sky::dayNightGrade(12.0f, Environment::kHorizonDefault,
                       Environment::kZenithDefault, Environment::kSunColDefault,
                       g12);
    EXPECT_MSG(nearEq(g12.stars, 0.0f),
               "stars_visible_at_night: ao meio-dia as estrelas não podem "
               "existir");
    // a JANELA do dono (19:00..05:00): às 19:00 o gate já acende (o ramp
    // suave 18→21 da casa CONTÉM a janela do dono); às 17:00 só a CAUDA
    // ténue do crepúsculo da Y-E (kDuskStars 0.15 interpolado no segmento
    // 12→18 — a rampa suave é a decisão fechada da Y-E, o meio-dia é 0
    // EXATO e a noite plena chega às 21)
    sky::SkyGrade g19{}, g17{};
    sky::dayNightGrade(19.0f, Environment::kHorizonDefault,
                       Environment::kZenithDefault, Environment::kSunColDefault,
                       g19);
    sky::dayNightGrade(17.0f, Environment::kHorizonDefault,
                       Environment::kZenithDefault, Environment::kSunColDefault,
                       g17);
    EXPECT_MSG(g19.stars > 0.15f,
               "stars_visible_at_night: às 19:00 as estrelas deviam começar "
               "(%.3f)", (double)g19.stars);
    EXPECT_MSG(g17.stars < 0.16f,
               "stars_visible_at_night: às 17:00 a cauda do crepúsculo passa "
               "o ténue da Y-E (%.3f)", (double)g17.stars);
    // o LIMAR do starDensity: 0.5 = o campo da Y-E (184) byte a byte;
    // 0 = morto; 1 = denso — a CONTA da casa (256−144·d)
    EXPECT_MSG(nearEq(sky::starThreshFromDensity(0.5f), 184.0f, 1e-3f),
               "stars_visible_at_night: density 0.5 não reproduz o limiar "
               "184 da Y-E");
    EXPECT_MSG(nearEq(sky::starThreshFromDensity(0.0f), 256.0f, 1e-3f),
               "stars_visible_at_night: density 0 não mata o campo");
    EXPECT_MSG(nearEq(sky::starThreshFromDensity(1.0f), 112.0f, 1e-3f),
               "stars_visible_at_night: density 1 não dá o limiar 112");
    // o CAMPO tem estrelas de verdade: procura uma CÉLULA com estrela (o
    // hash acima do limiar) e sonda o CENTRO da estrela (o offset
    // 0.2..0.8 da célula — o ponto mais BRILHANTE do campo)
    bool anyStar = false;
    f32 starDir[3] = {0.0f, 0.0f, 0.0f};
    for (i32 cx = -20; cx <= 20 && !anyStar; ++cx) {
        for (i32 cy = 30; cy <= 44 && !anyStar; ++cy) {
            for (i32 cz = -20; cz <= 20 && !anyStar; ++cz) {
                const u32 h = sky::hash3u(cx, cy, cz);
                if (static_cast<f32>(h & 0xFFu) < 184.0f) {
                    continue;   // célula vazia
                }
                const f32 ox =
                    0.2f + 0.6f * static_cast<f32>((h >> 8u) & 0xFFu) / 255.0f;
                const f32 oy =
                    0.2f + 0.6f * static_cast<f32>((h >> 16u) & 0xFFu) / 255.0f;
                const f32 oz =
                    0.2f + 0.6f * static_cast<f32>((h >> 24u) & 0xFFu) / 255.0f;
                f32 dx = (static_cast<f32>(cx) + ox) / sky::kStarDensity;
                f32 dy = (static_cast<f32>(cy) + oy) / sky::kStarDensity;
                f32 dz = (static_cast<f32>(cz) + oz) / sky::kStarDensity;
                const f32 len =
                    std::sqrt(dx * dx + dy * dy + dz * dz);
                dx /= len; dy /= len; dz /= len;
                if (sky::starFieldThresh(dx, dy, dz, 184.0f) > 0.9f) {
                    anyStar = true;
                    starDir[0] = dx;
                    starDir[1] = dy;
                    starDir[2] = dz;
                }
            }
        }
    }
    EXPECT_MSG(anyStar,
               "stars_visible_at_night: nenhuma estrela acesa no céu alto "
               "com o limiar default (o campo morreu?)");
    // o CENTRO de uma estrela existente APAGA com density 0 (limiar 256:
    // nenhuma célula passa) — a densidade do utilizador MANDA no campo
    if (anyStar) {
        EXPECT_MSG(nearEq(sky::starFieldThresh(starDir[0], starDir[1],
                                               starDir[2], 256.0f),
                          0.0f),
                   "stars_visible_at_night: density 0 devia apagar TODAS as "
                   "estrelas");
        // e o brilho da MESMA estrela escala com o starBrightness no FS
        // (o campo puro é o AMPLITUDE — o ganho viaja no uniform)
        EXPECT_MSG(sky::starFieldThresh(starDir[0], starDir[1], starDir[2],
                                        112.0f) > 0.9f,
                   "stars_visible_at_night: density 1 (limiar 112) devia "
                   "ACENDER ainda mais células — a estrela já existente "
                   "continua");
    }
    // o FS consome o limiar/brilho do utilizador (a fonte é o contrato)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/render/SkyRenderer.cpp",
                    src));
    const std::string code = test::stripCppComments(src);
    EXPECT_MSG(countSub(code, "uniform float uStarThresh;") == 1 &&
                   countSub(code, "uniform float uStarBright;") == 1,
               "stars_visible_at_night: o FS perdeu os uniforms "
               "uStarThresh/uStarBright");
    EXPECT_MSG(code.find("starField(dir, uStarThresh)") != std::string::npos,
               "stars_visible_at_night: o campo do FS não lê o limiar do "
               "utilizador");
    EXPECT_MSG(code.find("vec3(s * uStarBright)") != std::string::npos,
               "stars_visible_at_night: o brilho do utilizador não multiplica "
               "os pontos");
}

// ---- (7) A LUA À NOITE (22:00) — disco, fases, glow ------------------------
TEST(moon_visible_at_night) {
    // a POSIÇÃO: oposta ao sol — às 22:00 a lua está ALTA
    f32 m22[3];
    sky::moonDirFromTime(22.0f, m22);
    EXPECT_MSG(m22[1] > 0.5f,
               "moon_visible_at_night: a lua às 22:00 não está alta (%.3f)",
               (double)m22[1]);
    f32 s22[3];
    sky::sunDirFromTime(22.0f, s22);
    EXPECT_MSG(nearEq(m22[0], -s22[0], 1e-4f) &&
                   nearEq(m22[1], -s22[1], 1e-4f) &&
                   nearEq(m22[2], -s22[2], 1e-4f),
               "moon_visible_at_night: a lua não é a posição OPSTA ao sol");
    // ao MEIO-DIA a lua está SOB o mundo (invisível — a lua é da noite)
    f32 m12[3];
    sky::moonDirFromTime(12.0f, m12);
    EXPECT_MSG(m12[1] < -0.9f,
               "moon_visible_at_night: a lua do meio-dia devia estar no "
               "nadir (%.3f)", (double)m12[1]);
    // o DISCO: o centro acende, longe apaga
    EXPECT_MSG(nearEq(sky::moonDisk(1.0f), 1.0f) &&
                   nearEq(sky::moonDisk(0.0f), 0.0f),
               "moon_visible_at_night: o disco da lua não acende no centro");
    // A FASE — as 4 fases-chave exatas:
    //   cheia (0.5): o CENTRO iluminado
    EXPECT_MSG(nearEq(sky::moonPhaseLit(0.0f, 0.5f), 1.0f),
               "moon_visible_at_night: a lua CHEIA não está iluminada ao "
               "centro");
    //   nova (0.0): NADA iluminado
    EXPECT_MSG(nearEq(sky::moonPhaseLit(0.0f, 0.0f), 0.0f, 1e-3f),
               "moon_visible_at_night: a lua NOVA não está escura");
    EXPECT_MSG(nearEq(sky::moonPhaseLit(0.0f, 1.0f), 0.0f, 1e-3f),
               "moon_visible_at_night: a lua nova do 1.0 não está escura");
    //   quarto (0.25): UM lado iluminado, o outro escuro
    EXPECT_MSG(sky::moonPhaseLit(-0.5f, 0.25f) > 0.9f &&
                   sky::moonPhaseLit(0.5f, 0.25f) < 0.1f,
               "moon_visible_at_night: o quarto crescente não parte o disco "
               "(esq acesa, dir apagada)");
    //   minguante (0.75): o lado OPUESTO (o espelho)
    EXPECT_MSG(sky::moonPhaseLit(0.5f, 0.75f) > 0.9f &&
                   sky::moonPhaseLit(-0.5f, 0.75f) < 0.1f,
               "moon_visible_at_night: o quarto minguante não espelha o "
               "disco");
    // o GLOW: subtil, maior que zero perto do disco, zero no oposto
    EXPECT_MSG(sky::moonGlow(0.999f) > 0.0f &&
                   sky::moonGlow(0.999f) < 0.2f,
               "moon_visible_at_night: o glow da lua não é subtil");
    EXPECT_MSG(nearEq(sky::moonGlow(-1.0f), 0.0f),
               "moon_visible_at_night: o glow existe do lado errado");
    // o U do disco: a coordenada horizontal vive em [-1,1] dentro do disco
    const f32 u = sky::moonDiscU(m22[0] * 0.999f, m22[1] * 0.999f,
                                 m22[2] * 0.999f, m22);
    (void)u;   // o contrato do domínio é do raster (a sonda de píxel W-1)
    // A INVERSA da fórmula da direção (a sonda do raster): viewDir→dirToNdc
    // devolve o NDC de origem (a base ortonormal desfaz a conta)
    const Vec3 right{1.0f, 0.0f, 0.0f};
    const Vec3 up{0.0f, 1.0f, 0.0f};
    const Vec3 fwd{0.0f, 0.0f, -1.0f};
    const f32 aspect = 1.6f;
    const f32 tanF = 0.5774f;
    Vec3 dir{};
    sky::viewDir(0.25f, -0.4f, right, up, fwd, aspect, tanF, dir);
    const f32 len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    dir.x /= len; dir.y /= len; dir.z /= len;
    f32 ndc[2];
    sky::dirToNdc(dir, right, up, fwd, aspect, tanF, ndc);
    EXPECT_MSG(nearEq(ndc[0], 0.25f, 1e-3f) &&
                   nearEq(ndc[1], -0.4f, 1e-3f),
               "moon_visible_at_night: a INVERSA da direção não devolve o "
               "NDC (a sonda da lua do raster depende dela)");
    // o FS desenha a lua com a fase + o glow + o uniform da direção
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/render/SkyRenderer.cpp",
                    src));
    const std::string code = test::stripCppComments(src);
    EXPECT_MSG(countSub(code, "uniform vec3 uMoonDir;") == 1 &&
                   countSub(code, "uniform float uMoonPhase;") == 1 &&
                   countSub(code, "uniform float uMoonBright;") == 1,
               "moon_visible_at_night: o FS perdeu os uniforms da lua");
    EXPECT_MSG(code.find("moonPhaseLit(u, uMoonPhase)") != std::string::npos,
               "moon_visible_at_night: o disco do FS não usa a fase");
    EXPECT_MSG(code.find("moonGlow(dm)") != std::string::npos,
               "moon_visible_at_night: o glow sumiu do FS");
    EXPECT_MSG(code.find("moonDisk(dm)") != std::string::npos,
               "moon_visible_at_night: o disco sumiu do FS");
}

// ---- (8) O ParticleEmitter TEM TRANSFORM OBRIGATÓRIO -----------------------
TEST(particle_emitter_has_transform) {
    // o PRESET traz o Transform (a ESTRUTURA da Y-F1)
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::ParticleEmitter,
                                         nullptr, nullptr);
    ASSERT(h.valid());
    Tic* t = s.get(h);
    EXPECT_MSG(t->getComponent<Transform3D>() != nullptr,
               "particle_emitter_has_transform: o preset ParticleEmitter "
               "nasceu SEM Transform3D");
    // o gizmo arrasta O TRANSFORM (0.12-V: a fonte única composeWorld +
    // o gizmoAnchor no tr->pos — o main é o contrato)
    std::string m;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/platform/main.cpp", m));
    const std::string mc = test::stripCppComments(m);
    // o draw do PE lê a origem DO TRANSFORM (a coluna da translação)
    EXPECT_MSG(mc.find("pp.pos[0] = tr->world.m[12];") != std::string::npos,
               "particle_emitter_has_transform: o ParticleRenderer deixou "
               "de aplicar o Transform às posições (a coluna 12 do world)");
    // sem Transform não há draw (a exigência: o guard do caminho)
    EXPECT_MSG(mc.find("if (!tr && !isWeather)") != std::string::npos,
               "particle_emitter_has_transform: o guard do Transform sumiu "
               "do draw das partículas");
    // o GIZMO existe para QUALQUER TIC selecionado com Transform (o
    // gizmoTr do main — o PE incluso)
    EXPECT_MSG(mc.find("gizmoTic->getComponent<Transform3D>()") !=
                   std::string::npos,
               "particle_emitter_has_transform: o gizmo deixou de ancorar "
               "no Transform do TIC selecionado");
    // o drag escreve o TRANSFORM (não as partículas): o feedGizmo compõe
    // pelo pai e escreve pos/rot/scale (a fonte 0.12-V)
    std::string ts;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/core/TransformSystem.h",
                    ts));
    EXPECT_MSG(ts.find("composeWorld") != std::string::npos &&
                   ts.find("worldToLocalPos") != std::string::npos,
               "particle_emitter_has_transform: a fonte única da composição "
               "sumiu (o gizmo depende dela)");
    // o round-trip .goni MANTÉM o Transform do emissor
    const std::string dump = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, dump, ctx));
    const Handle hf = s2.find(t->name);
    ASSERT(hf.valid());
    EXPECT_MSG(s2.get(hf)->getComponent<Transform3D>() != nullptr &&
                   s2.get(hf)->getComponent<ParticleEmitter>() != nullptr,
               "particle_emitter_has_transform: o round-trip .goni perdeu o "
               "Transform ou o emissor");
}

// ---- (9) O COMPAT do .goni — os campos novos só viajam ≠ default -----------
TEST(w1_goni_compat) {
    // o .goni da Y-E/Y-F2 (SEM os campos da W-1) carrega byte a byte: o
    // dump NÃO escreve nenhum campo novo quando os valores são os defaults
    Scene a;
    const Handle hw = a.create("WE");
    a.get(hw)->addComponent<Transform3D>();
    Environment* env = a.get(hw)->addComponent<Environment>();
    ASSERT(env != nullptr);
    env->skyType = Environment::kSkyTypeProcedural;
    env->timeOfDay = 21.5f;   // um valor VELHO que viaja (o tod da Y-E)
    const Handle ha = a.create("Clima");
    a.get(ha)->addComponent<Transform3D>();
    AmbientEnvironment* ae = a.get(ha)->addComponent<AmbientEnvironment>();
    ASSERT(ae != nullptr);
    ae->weatherType = AmbientEnvironment::kWeatherStorm;
    const std::string dump = SceneSerializer::dump(a);
    EXPECT_MSG(countSub(dump, "\"starD\"") == 0 &&
                   countSub(dump, "\"starB\"") == 0 &&
                   countSub(dump, "\"moonP\"") == 0 &&
                   countSub(dump, "\"moonB\"") == 0 &&
                   countSub(dump, "\"target\"") == 0,
               "w1_goni_compat: os defaults da W-1 INVADIRAM o .goni (só "
               "viaja o ≠ default — o byte a byte da Y-E morre)");
    // o round-trip não cria «edit» fantasma
    Scene b;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(b, dump, ctx));
    EXPECT_MSG(SceneSerializer::dump(b) == dump,
               "w1_goni_compat: o round-trip criou um edit fantasma");
    // os campos AUSENTES carregam com os DEFAULTS do dono
    const Handle hw2 = b.find("WE");
    ASSERT(hw2.valid());
    const Environment* env2 = b.get(hw2)->getComponent<Environment>();
    ASSERT(env2 != nullptr);
    EXPECT_MSG(nearEq(env2->starDensity, 0.5f) &&
                   nearEq(env2->starBrightness, 0.8f) &&
                   nearEq(env2->moonPhase, 0.5f) &&
                   nearEq(env2->moonBrightness, 0.6f),
               "w1_goni_compat: o .goni velho não carregou com os defaults "
               "da W-1");
    EXPECT_MSG(nearEq(env2->timeOfDay, 21.5f),
               "w1_goni_compat: o tod VELHO não sobreviveu ao load");
    const Handle ha2 = b.find("Clima");
    ASSERT(ha2.valid());
    const AmbientEnvironment* ae2 =
        b.get(ha2)->getComponent<AmbientEnvironment>();
    ASSERT(ae2 != nullptr);
    EXPECT_MSG(ae2->targetEmitter == "RainEmitter",
               "w1_goni_compat: o alvo ausente não caiu no default");
    // COM valores novos: viajam E voltam
    env->starDensity = 0.9f;
    env->starBrightness = 0.4f;
    env->moonPhase = 0.125f;
    env->moonBrightness = 0.75f;
    ae->targetEmitter = "SnowEmitter";
    const std::string dump2 = SceneSerializer::dump(a);
    EXPECT_MSG(countSub(dump2, "\"starD\"") == 1 &&
                   countSub(dump2, "\"starB\"") == 1 &&
                   countSub(dump2, "\"moonP\"") == 1 &&
                   countSub(dump2, "\"moonB\"") == 1 &&
                   countSub(dump2, "\"target\"") == 1,
               "w1_goni_compat: os campos NOVOS não viajam no .goni");
    Scene c;
    SceneSerializer::LoadCtx ctx2;
    ASSERT(SceneSerializer::loadText(c, dump2, ctx2));
    const Environment* env3 =
        c.get(c.find("WE"))->getComponent<Environment>();
    const AmbientEnvironment* ae3 =
        c.get(c.find("Clima"))->getComponent<AmbientEnvironment>();
    ASSERT(env3 != nullptr && ae3 != nullptr);
    EXPECT_MSG(nearEq(env3->starDensity, 0.9f) &&
                   nearEq(env3->starBrightness, 0.4f) &&
                   nearEq(env3->moonPhase, 0.125f) &&
                   nearEq(env3->moonBrightness, 0.75f),
               "w1_goni_compat: os campos da W-1 não voltam do .goni");
    EXPECT_MSG(ae3->targetEmitter == "SnowEmitter",
               "w1_goni_compat: o alvo não volta do .goni");
}
