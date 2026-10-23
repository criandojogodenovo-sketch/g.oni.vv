// tests/test_012y_f2.cpp — 0.12-Y-F2: O CLIMA E O VENTO.
//
// Os pins da sub-fase (a ordem do prompt do dono):
//   wind_tic_cria      — o preset Wind cria o TIC com WindTIC (os
//                        defaults, a NORMALIZAÇÃO da direção no clamp e
//                        a fonte pura windVector)
//   ambient_rain_preset— weatherType=1 configura o ParticleEmitter com
//                        gravity=(0,-12,0) (o preset INTEIRO + a
//                        decisão de arquitetura: o clima CONFIGURA o
//                        emissor do MESMO TIC na criação)
//   ambient_snow_preset— weatherType=2 configura o ParticleEmitter com
//                        lifetime=4.0 (+ a trovoada e o NONE)
//   ambient_serialize  — o .goni guarda e carrega weatherType +
//                        intensity (o thunderFlash/Timer são RUNTIME —
//                        NUNCA viajam; os defaults ficam FORA do dump)
//   wind_uniform_passed— o ParticleRenderer passa o uWind ao shader
//                        (o draw real no stub: o ÚLTIMO 3f é o vento, o
//                        uTime continua o ÚLTIMO 1f) + o VS replica a
//                        fórmula do prompt (a linha SEM comentários)
//
// A decisão de arquitetura do dono: AmbientEnvironment NÃO renderiza
// partículas — CONFIGURA o ParticleEmitter do mesmo TIC (applyWeatherPreset);
// WindTIC fornece o vento global lido pelo ParticleRenderer; a trovoada é
// um flash de ecrã via uniform no sky shader (sem geometria de raio);
// GLES 3.0 — zero compute shaders.
#include "TestFramework.h"
#include "components/WindTIC.h"              // 0.12-Y-F2: o vento
#include "components/AmbientEnvironment.h"   // 0.12-Y-F2: o clima
#include "components/ParticleEmitter.h"      // 0.12-Y-F1: o emissor
#include "components/Transform3D.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "render/ParticleShader.h"           // as puras fx:: (o M-Yg1)
#include "render/ParticleRenderer.h"         // o draw real no stub
#include <GLES3/gl3.h>                       // o stub (stats do uniform)

#include <cmath>
#include <cstdio>
#include <string>

using namespace vv;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

std::string repoRoot() {
    // o padrão da casa (test_012y.cpp): a macro do cmake manda (o ctest
    // corre com cwd = build-tests — o "." morre no CI; a lição do job
    // vermelho da 0.12-Y-F2)
    if (const char* e = std::getenv("GONI_ROOT_OVERRIDE")) return e;
#ifdef REPO_ROOT
    return REPO_ROOT;
#else
    return ".";
#endif
}

// o contador de sub-string (o padrão dos pins de fonte da casa)
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

// ---- PIN wind_tic_cria (0.12-Y-F2) ---------------------------------------------
TEST(wind_tic_cria) {
    // (1) O COMPONENTE — os defaults da fábrica + a clamp (a direção
    // NORMALIZA, o zero cai no default; a força vive em [0,10])
    {
        WindTIC w;
        EXPECT_MSG(nearEq(w.windDirection[0], 1.0f) &&
                       nearEq(w.windDirection[1], 0.0f) &&
                       nearEq(w.windDirection[2], 0.0f) &&
                       nearEq(w.windStrength, 1.0f) && w.active,
                   "wind_tic_cria: os defaults da fábrica ((1,0,0)·1.0)");
        w.windDirection[0] = 3.0f;
        w.windDirection[1] = 4.0f;
        w.windStrength = 99.0f;
        w.clampFields();
        EXPECT_MSG(nearEq(w.windDirection[0], 0.6f, 1e-4f) &&
                       nearEq(w.windDirection[1], 0.8f, 1e-4f) &&
                       nearEq(w.windStrength, WindTIC::kStrengthMax),
                   "wind_tic_cria: a direção (3,4) NORMALIZA para (0.6,0.8) "
                   "e a força clampa a 10");
        w.windDirection[0] = 0.0f;
        w.windDirection[1] = 0.0f;
        w.windDirection[2] = 0.0f;
        w.clampFields();
        EXPECT_MSG(nearEq(w.windDirection[0], WindTIC::kDirDefault[0]) &&
                       nearEq(w.windDirection[1], WindTIC::kDirDefault[1]),
                   "wind_tic_cria: a direção ZERO cai no default (+x)");
    }
    // (2) A FONTE PURA — windVector = direção × força (o draw e os pins
    // partilham a MESMA conta — o padrão ambientFromAvg)
    {
        WindTIC w;
        w.windDirection[0] = 0.0f;
        w.windDirection[1] = 1.0f;
        w.windStrength = 2.5f;
        f32 v[3];
        windVector(w, v);
        EXPECT_MSG(nearEq(v[0], 0.0f) && nearEq(v[1], 2.5f) &&
                       nearEq(v[2], 0.0f),
                   "wind_tic_cria: o vento (0,1,0)·2.5 sai (0,2.5,0)");
    }
    // (3) O PRESET — «Wind» cria o TIC com Transform + WindTIC (SÓ isso;
    // SEM mesh, SEM física, SEM partículas) e o nome único Godot-style
    {
        Scene scene;
        const Handle h =
            createTicFromPreset(scene, PresetKind::Wind, nullptr, nullptr);
        ASSERT(h.valid());
        const Tic* t = scene.get(h);
        EXPECT_MSG(t->name.find("Wind") == 0,
                   "wind_tic_cria: o TIC nasce com o nome do preset ('%s')",
                   t->name.c_str());
        EXPECT_MSG(t->getComponent<Transform3D>() != nullptr,
                   "wind_tic_cria: o preset traz o Transform");
        const WindTIC* w = t->getComponent<WindTIC>();
        EXPECT_MSG(w != nullptr,
                   "wind_tic_cria: o preset traz o WindTIC (o pin do dono)");
        EXPECT_MSG(t->getComponent<MeshRenderer>() == nullptr &&
                       t->getComponent<BodyComp>() == nullptr &&
                       t->getComponent<ParticleEmitter>() == nullptr,
                   "wind_tic_cria: o preset NÃO traz mesh/corpo/emissor");
        const Handle h2 =
            createTicFromPreset(scene, PresetKind::Wind, nullptr, nullptr);
        ASSERT(h2.valid());
        EXPECT_MSG(scene.get(h2)->name != t->name,
                   "wind_tic_cria: o 2.º vento tem nome único");
    }
    // (4) O REGISTO — o factory cria por nome (o caminho do serializer)
    {
        Scene s2;
        const Handle h2 = s2.create("X");
        EXPECT_MSG(s2.components().registry().create("WindTIC",
                                                     s2.components(), h2),
                   "wind_tic_cria: o registo não cria 'WindTIC' por nome");
        EXPECT_MSG(s2.get(h2)->getComponent<WindTIC>() != nullptr,
                   "wind_tic_cria: o factory não ligou o componente");
        s2.components().removeAll(h2);
        EXPECT_MSG(s2.get(h2)->getComponent<WindTIC>() == nullptr,
                   "wind_tic_cria: o removeAll não largou o vento");
    }
}

// ---- PIN ambient_rain_preset (0.12-Y-F2) ----------------------------------------
// A ordem do dono: «weatherType=1 configura ParticleEmitter com
// gravity=(0,-12,0)». O preset Rain nasce COM o clima aplicado (a decisão
// de arquitetura — o preset é Transform+PE+AE no MESMO TIC).
TEST(ambient_rain_preset) {
    // (1) O PRESET — o TIC de chuva nasce configurado
    {
        Scene scene;
        const Handle h =
            createTicFromPreset(scene, PresetKind::Rain, nullptr, nullptr);
        ASSERT(h.valid());
        const Tic* t = scene.get(h);
        EXPECT_MSG(t->name.find("Rain") == 0,
                   "ambient_rain_preset: o TIC nasce 'Rain' ('%s')",
                   t->name.c_str());
        EXPECT_MSG(t->getComponent<Transform3D>() != nullptr,
                   "ambient_rain_preset: o preset traz o Transform");
        const AmbientEnvironment* ae =
            t->getComponent<AmbientEnvironment>();
        const ParticleEmitter* pe = t->getComponent<ParticleEmitter>();
        EXPECT_MSG(ae != nullptr && pe != nullptr,
                   "ambient_rain_preset: o preset traz clima+emissor NO "
                   "MESMO TIC");
        EXPECT_MSG(ae->weatherType == AmbientEnvironment::kWeatherRain &&
                       nearEq(ae->intensity, 0.5f, 1e-6f),
                   "ambient_rain_preset: o clima de fábrica é chuva 0.5");
        EXPECT_MSG(nearEq(pe->gravity[0], 0.0f) &&
                       nearEq(pe->gravity[1], -12.0f) &&
                       nearEq(pe->gravity[2], 0.0f),
                   "ambient_rain_preset: a gravidade NÃO é (0,-12,0) — o "
                   "applyWeatherPreset não correu na criação (M-Yg2)");
        EXPECT_MSG(nearEq(pe->velocity[1], -18.0f) &&
                       nearEq(pe->lifetime, 1.2f) &&
                       nearEq(pe->startSize, 0.015f) &&
                       nearEq(pe->endSize, 0.015f),
                   "ambient_rain_preset: a chuva do prompt (vel -18, vida "
                   "1.2, tam 0.015)");
        EXPECT_MSG(nearEq(pe->startColor[0], 0.6f) &&
                       nearEq(pe->startColor[1], 0.7f) &&
                       nearEq(pe->startColor[2], 0.9f) &&
                       nearEq(pe->startColor[3], 0.7f) &&
                       nearEq(pe->endColor[3], 0.0f),
                   "ambient_rain_preset: a cor da chuva (0.6,0.7,0.9) com "
                   "o fade out");
        EXPECT_MSG(pe->maxParticles ==
                       static_cast<i32>(1500.0f * 0.5f),
                   "ambient_rain_preset: o COUNT escala com a intensidade "
                   "(1500×0.5 = %d)", pe->maxParticles);
        EXPECT_MSG(t->getComponent<MeshRenderer>() == nullptr &&
                       t->getComponent<BodyComp>() == nullptr,
                   "ambient_rain_preset: SEM mesh, SEM física");
    }
    // (2) A FONTE — applyWeatherPreset num emissor sujo REESCREVE tudo
    // (o clima manda no emissor; a intensidade nova re-escala o count)
    {
        AmbientEnvironment ae;
        ae.weatherType = AmbientEnvironment::kWeatherRain;
        ae.intensity = 1.0f;
        ParticleEmitter pe;
        pe.maxParticles = 1234;
        pe.lifetime = 9.0f;
        pe.gravity[1] = 4.0f;
        ae.applyWeatherPreset(&pe);
        EXPECT_MSG(nearEq(pe.gravity[1], -12.0f) &&
                       nearEq(pe.lifetime, 1.2f) &&
                       pe.maxParticles == 1500,
                   "ambient_rain_preset: o preset REESCREVE o emissor "
                   "(intensidade 1.0 → 1500)");
        // o NONE silencia (o 0 é o contrato — a clamp do PE não levanta)
        ae.weatherType = AmbientEnvironment::kWeatherNone;
        ae.applyWeatherPreset(&pe);
        EXPECT_MSG(pe.maxParticles == 0,
                   "ambient_rain_preset: o NONE não silenciou o emissor "
                   "(maxParticles %d)", pe.maxParticles);
        // sem emissor: nada (a decisão do dono — o log é do chamador)
        ae.weatherType = AmbientEnvironment::kWeatherRain;
        ae.applyWeatherPreset(nullptr);   // não pode crashar
        EXPECT_MSG(true, "ambient_rain_preset: pe null é inócuo");
    }
    // (3) A TROVOADA — o STORM é a chuva com 2000×intensity (0.7 → 1400)
    // e o update do flash (o pseudocódigo EXATO do prompt)
    {
        Scene scene;
        const Handle h =
            createTicFromPreset(scene, PresetKind::Storm, nullptr, nullptr);
        ASSERT(h.valid());
        const AmbientEnvironment* ae =
            scene.get(h)->getComponent<AmbientEnvironment>();
        const ParticleEmitter* pe =
            scene.get(h)->getComponent<ParticleEmitter>();
        ASSERT(ae != nullptr && pe != nullptr);
        EXPECT_MSG(ae->weatherType == AmbientEnvironment::kWeatherStorm &&
                       nearEq(ae->intensity, 0.7f, 1e-6f) &&
                       pe->maxParticles == 1400,
                   "ambient_rain_preset: a trovoada é 2000×0.7 = %d "
                   "(intensity %.2f)", pe->maxParticles,
                   (double)ae->intensity);
        EXPECT_MSG(nearEq(pe->gravity[1], -12.0f),
                   "ambient_rain_preset: a trovoada partilha a balística "
                   "da chuva");
        // o UPDATE — dt 0.25 (exato em binário: o FP acumulado de 0.1
        // enganava o gatilho), o flash arma no 4.º passo (timer 1.0) e
        // morre a 3.0/s; o re-agenda é random(3,8) (rnd 0.4 → 5.0)
        AmbientEnvironment st = *ae;
        st.thunderFlash = 0.0f;
        st.thunderTimer = 1.0f;
        for (int i = 0; i < 4; ++i) {
            st.updateStorm(0.25f, 0.4f);
        }
        EXPECT_MSG(nearEq(st.thunderTimer, 5.0f, 1e-4f),
                   "ambient_rain_preset: o re-agenda não é 3+rnd·5 "
                   "(%.3f)", (double)st.thunderTimer);
        EXPECT_MSG(nearEq(st.thunderFlash, 0.25f, 1e-4f),
                   "ambient_rain_preset: o flash não morre a 3.0/s "
                   "(%.3f — 1.0−4·0.25·3)", (double)st.thunderFlash);
        st.updateStorm(0.25f, 0.4f);
        EXPECT_MSG(st.thunderFlash == 0.0f,
                   "ambient_rain_preset: o flash não morreu (piso 0 — "
                   "%.3f)", (double)st.thunderFlash);
        // a CHUVA não Flasha (o update é SÓ do STORM — o contrato)
        AmbientEnvironment rn;
        rn.weatherType = AmbientEnvironment::kWeatherRain;
        rn.thunderTimer = 0.0f;
        rn.updateStorm(0.1f, 0.4f);
        EXPECT_MSG(rn.thunderFlash == 0.0f && nearEq(rn.thunderTimer, 0.0f),
                   "ambient_rain_preset: a chuva NÃO pode flashar (o "
                   "update é SÓ do STORM)");
    }
}

// ---- PIN ambient_snow_preset (0.12-Y-F2) ----------------------------------------
TEST(ambient_snow_preset) {
    Scene scene;
    const Handle h =
        createTicFromPreset(scene, PresetKind::Snow, nullptr, nullptr);
    ASSERT(h.valid());
    const Tic* t = scene.get(h);
    EXPECT_MSG(t->name.find("Snow") == 0,
               "ambient_snow_preset: o TIC nasce 'Snow' ('%s')",
               t->name.c_str());
    const AmbientEnvironment* ae = t->getComponent<AmbientEnvironment>();
    const ParticleEmitter* pe = t->getComponent<ParticleEmitter>();
    ASSERT(ae != nullptr && pe != nullptr);
    EXPECT_MSG(ae->weatherType == AmbientEnvironment::kWeatherSnow,
               "ambient_snow_preset: weatherType não é SNOW (2)");
    EXPECT_MSG(nearEq(pe->lifetime, 4.0f),
               "ambient_snow_preset: a vida NÃO é 4.0 s (o pin do dono — "
               "%.3f)", (double)pe->lifetime);
    EXPECT_MSG(nearEq(pe->gravity[1], -1.2f) &&
                   nearEq(pe->velocity[1], -2.0f),
               "ambient_snow_preset: a neve cai LENTA (gravidade -1.2, "
               "velocidade -2)");
    EXPECT_MSG(nearEq(pe->startSize, 0.04f) && nearEq(pe->endSize, 0.02f),
               "ambient_snow_preset: o floco ENCOLHE (0.04 → 0.02)");
    EXPECT_MSG(nearEq(pe->startColor[3], 0.9f) &&
                   nearEq(pe->endColor[3], 0.0f),
               "ambient_snow_preset: o branco com o fade out");
    EXPECT_MSG(pe->maxParticles == static_cast<i32>(800.0f * 0.5f),
               "ambient_snow_preset: o COUNT 800×0.5 = %d",
               pe->maxParticles);
    // a clamp do clima: o tipo desconhecido cai no NONE (o seguro)
    {
        AmbientEnvironment bad;
        bad.weatherType = 99;
        bad.intensity = 42.0f;
        bad.clampFields();
        EXPECT_MSG(bad.weatherType == AmbientEnvironment::kWeatherNone &&
                       nearEq(bad.intensity, 1.0f),
                   "ambient_snow_preset: o tipo desconhecido não cai no "
                   "NONE e a intensidade não clampa a 1");
    }
}

// ---- PIN ambient_serialize (0.12-Y-F2) ------------------------------------------
TEST(ambient_serialize) {
    // (1) O ROUNDTRIP — weatherType + intensity viajam; o thunder é
    // RUNTIME (NUNCA serializado); o vento transporta dir/strength
    {
        Scene a;
        const Handle h = a.create("Clima");
        a.get(h)->addComponent<Transform3D>();
        AmbientEnvironment* ae = a.get(h)->addComponent<AmbientEnvironment>();
        ae->weatherType = AmbientEnvironment::kWeatherStorm;
        ae->intensity = 0.8f;
        ae->thunderFlash = 0.77f;   // runtime — NÃO pode sair no dump
        ae->thunderTimer = 1.23f;   // runtime — idem
        WindTIC* w = a.get(h)->addComponent<WindTIC>();
        w->windDirection[0] = 0.0f;
        w->windDirection[1] = 1.0f;
        w->windStrength = 6.5f;
        w->clampFields();
        const std::string dump = SceneSerializer::dump(a);
        EXPECT_MSG(countSub(dump, "\"AmbientEnvironment\"") == 1 &&
                       countSub(dump, "\"weather\"") == 1 &&
                       countSub(dump, "\"intensity\"") == 1,
                   "ambient_serialize: o .goni NÃO guarda o clima "
                   "(tipo/intensidade)");
        EXPECT_MSG(countSub(dump, "\"WindTIC\"") == 1 &&
                       countSub(dump, "\"dir\"") == 1 &&
                       countSub(dump, "\"strength\"") == 1,
                   "ambient_serialize: o .goni NÃO guarda o vento");
        EXPECT_MSG(countSub(dump, "thunder") == 0,
                   "ambient_serialize: o RUNTIME da trovoada INVADIU o "
                   ".goni (thunderFlash/Timer nunca viajam)");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const Handle hf = b.find("Clima");
        ASSERT(hf.valid());
        const AmbientEnvironment* ae2 =
            b.get(hf)->getComponent<AmbientEnvironment>();
        const WindTIC* w2 = b.get(hf)->getComponent<WindTIC>();
        ASSERT(ae2 != nullptr && w2 != nullptr);
        EXPECT_MSG(ae2->weatherType == AmbientEnvironment::kWeatherStorm &&
                       nearEq(ae2->intensity, 0.8f),
                   "ambient_serialize: o clima não volta do .goni");
        EXPECT_MSG(nearEq(ae2->thunderFlash, 0.0f) &&
                       nearEq(ae2->thunderTimer, 4.0f, 1e-3f),
                   "ambient_serialize: a cena carrega SEM raio pendente "
                   "(o runtime volta ao default)");
        EXPECT_MSG(nearEq(w2->windDirection[1], 1.0f) &&
                       nearEq(w2->windStrength, 6.5f),
                   "ambient_serialize: o vento não volta do .goni");
    }
    // (2) Os DEFAULTS ficam FORA do dump (a fábrica é silêncio — o
    // .goni antigo carrega byte a byte)
    {
        Scene c;
        const Handle hc = c.create("limpo");
        c.get(hc)->addComponent<Transform3D>();
        c.get(hc)->addComponent<AmbientEnvironment>();
        c.get(hc)->addComponent<WindTIC>();
        const std::string dump2 = SceneSerializer::dump(c);
        EXPECT_MSG(countSub(dump2, "\"weather\"") == 0 &&
                       countSub(dump2, "\"intensity\"") == 0 &&
                       countSub(dump2, "\"dir\"") == 0 &&
                       countSub(dump2, "\"strength\"") == 0,
                   "ambient_serialize: os defaults INVADIRAM o .goni");
        Scene d;
        SceneSerializer::LoadCtx ctx2;
        ASSERT(SceneSerializer::loadText(d, dump2, ctx2));
        const AmbientEnvironment* ae3 =
            d.get(d.find("limpo"))->getComponent<AmbientEnvironment>();
        const WindTIC* w3 =
            d.get(d.find("limpo"))->getComponent<WindTIC>();
        ASSERT(ae3 != nullptr && w3 != nullptr);
        EXPECT_MSG(ae3->weatherType == AmbientEnvironment::kWeatherNone &&
                       nearEq(ae3->intensity, 0.5f),
                   "ambient_serialize: os ausentes não caem na fábrica "
                   "(clima)");
        EXPECT_MSG(nearEq(w3->windStrength, 1.0f),
                   "ambient_serialize: os ausentes não caem na fábrica "
                   "(vento)");
    }
    // (3) A CORRUPÇÃO normaliza ao carregar (a direção (3,0) → (1,0); o
    // tipo 99 → NONE)
    {
        const char* sujo =
            "{\"version\":1,\"tics\":[{\"id\":0,\"name\":\"s\",\"parent\":"
            "-1,\"components\":[{\"type\":\"Transform3D\"},{\"type\":"
            "\"WindTIC\",\"dir\":[3.0,0.0,0.0],\"strength\":-5.0},{"
            "\"type\":\"AmbientEnvironment\",\"weather\":7,\"intensity\":"
            "9.0}]}]}";
        Scene e;
        SceneSerializer::LoadCtx ctx3;
        ASSERT(SceneSerializer::loadText(e, sujo, ctx3));
        const WindTIC* w4 = e.get(e.find("s"))->getComponent<WindTIC>();
        const AmbientEnvironment* ae4 =
            e.get(e.find("s"))->getComponent<AmbientEnvironment>();
        ASSERT(w4 != nullptr && ae4 != nullptr);
        EXPECT_MSG(nearEq(w4->windDirection[0], 1.0f, 1e-4f) &&
                       nearEq(w4->windStrength, 0.0f),
                   "ambient_serialize: a direção suja não normaliza e a "
                   "força negativa não clampa a 0");
        EXPECT_MSG(ae4->weatherType == AmbientEnvironment::kWeatherNone &&
                       nearEq(ae4->intensity, 1.0f),
                   "ambient_serialize: o clima sujo não cai no seguro "
                   "(NONE/1.0)");
    }
}

// ---- PIN wind_uniform_passed (0.12-Y-F2) ----------------------------------------
// A ordem do dono: «O ParticleRenderer, antes do draw call, procura o
// primeiro WindTIC ativo na cena… Passa como uniform uWind ao particle
// shader». O draw real no stub: o ÚLTIMO 3f do draw é o uWind e o uTime
// continua o ÚLTIMO 1f (o contrato da casa); o VS replica a fórmula do
// prompt (a linha SEM comentários — o M-Yg1 morre AQUI 2×).
TEST(wind_uniform_passed) {
    // (1) A FÓRMULA no VS — o termo do vento na posição final (SEM
    // comentários: o stripCppComments do R-061 mata o falso verde)
    {
        std::string src;
        ASSERT(readFile(repoRoot() +
                        "/app/src/main/cpp/render/ParticleShader.h",
                        src));
        std::string code;
        // o stripper da casa (o padrão do test_sentinels — sem ele um
        // comentário com a fórmula satisfazia o pin)
        bool inBlock = false, inLine = false;
        for (size_t i = 0; i < src.size(); ++i) {
            if (!inLine && !inBlock && i + 1 < src.size() &&
                src[i] == '/' && src[i + 1] == '*') {
                inBlock = true;
                ++i;
                continue;
            }
            if (inBlock && i + 1 < src.size() && src[i] == '*' &&
                src[i + 1] == '/') {
                inBlock = false;
                ++i;
                continue;
            }
            if (!inLine && !inBlock && src[i] == '/' &&
                i + 1 < src.size() && src[i + 1] == '/') {
                inLine = true;
                ++i;
                continue;
            }
            if (inLine && (src[i] == '\n' || src[i] == '\r')) {
                inLine = false;
            }
            if (!inLine && !inBlock) {
                code += src[i];
            }
        }
        EXPECT_MSG(countSub(code, "uniform vec3 uWind;") == 1,
                   "wind_uniform_passed: o uniform uWind saiu do VS");
        EXPECT_MSG(countSub(code, "(uWind * localTime)") == 1,
                   "wind_uniform_passed: o TERMO DO VENTO saiu da posição "
                   "do VS (a fórmula do prompt — M-Yg1)");
        // a SOBRECARGA pura replica o VS (a fonte única da F2)
        const f32 p0[3] = {1.0f, 2.0f, 3.0f};
        const f32 vel[3] = {0.0f, 3.0f, 0.0f};
        const f32 grav[3] = {0.0f, -4.0f, 0.0f};
        const f32 wind[3] = {2.0f, 0.0f, 0.0f};
        f32 p[3];
        fx::particlePos(p0, vel, grav, wind, 1.0f, p);
        EXPECT_MSG(nearEq(p[0], 3.0f) && nearEq(p[1], 3.0f),
                   "wind_uniform_passed: a pura COM vento não soma o "
                   "desvio wind·t (1+2·1, 2+3−2)");
        f32 q[3];
        fx::particlePos(p0, vel, grav, 1.0f, q);
        EXPECT_MSG(nearEq(q[0], 1.0f) && nearEq(q[1], 3.0f),
                   "wind_uniform_passed: a sobrecarga SEM vento mudou o "
                   "caminho de sempre (a F1 tem de ficar byte a byte)");
    }
    // (2) O DRAW REAL no stub — o uWind é o ÚLTIMO 3f do draw (o uTime
    // continua o ÚLTIMO 1f: o contrato do pin da F1 intacto)
    {
        glstub::fb::enabled = true;
        glstub::fb::resetState();
        glstub::reset();
        ParticleRenderer pr;
        ASSERT(pr.init());
        ParticleRenderer::Params pp;
        pp.maxParticles = 100;
        pp.time = 2.5f;
        pp.wind[0] = 0.5f;
        pp.wind[1] = 0.25f;
        pp.wind[2] = -0.75f;
        const DrawStats st = pr.draw(pp);
        EXPECT_MSG(st.drawCalls == 1,
                   "wind_uniform_passed: o draw deixou de ser 1 call");
        EXPECT_MSG(nearEq(glstub::stats.lastUniform3f[0], 0.5f, 1e-6f) &&
                       nearEq(glstub::stats.lastUniform3f[1], 0.25f,
                              1e-6f) &&
                       nearEq(glstub::stats.lastUniform3f[2], -0.75f,
                              1e-6f),
                   "wind_uniform_passed: o uWind não chega ao shader "
                   "(último 3f = %.2f,%.2f,%.2f — o M-Yg1 no render)",
                   (double)glstub::stats.lastUniform3f[0],
                   (double)glstub::stats.lastUniform3f[1],
                   (double)glstub::stats.lastUniform3f[2]);
        EXPECT_MSG(nearEq(glstub::stats.lastUniform1f, 2.5f, 1e-6f),
                   "wind_uniform_passed: o uTime deixou de ser o ÚLTIMO "
                   "1f do draw (%.2f — o contrato da casa)",
                   (double)glstub::stats.lastUniform1f);
        // SEM vento (o default do Params): uWind = (0,0,0) — a F1 byte a
        // byte (o caminho de sempre não muda)
        glstub::reset();
        ParticleRenderer::Params pz;
        pz.maxParticles = 100;
        pz.time = 1.0f;
        pr.draw(pz);
        EXPECT_MSG(glstub::stats.lastUniform3f[0] == 0.0f &&
                       glstub::stats.lastUniform3f[1] == 0.0f &&
                       glstub::stats.lastUniform3f[2] == 0.0f,
                   "wind_uniform_passed: sem WindTIC o uWind não é "
                   "(0,0,0) — a F1 não fica byte a byte");
        pr.destroy();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }
    // (3) O CAMINHO NO MAIN — o passe procura o 1.º WindTIC ATIVO e o
    // uWind vive no Params (o contrato estrutural do prompt)
    {
        std::string mainSrc;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/platform/main.cpp",
                        mainSrc));
        EXPECT_MSG(countSub(mainSrc, "pp.wind[0] = windNow[0];") == 1,
                   "wind_uniform_passed: o main deixou de passar o vento "
                   "ao Params do draw");
        EXPECT_MSG(countSub(mainSrc, "windVector(wnds.at(i), windNow);") ==
                       1,
                   "wind_uniform_passed: o main deixou de usar a FONTE "
                   "ÚNICA windVector");
    }
}
