#include "core/Presets.h"
#include "components/AmbientEnvironment.h"
#include "components/AudioPlayer.h"
#include "components/Environment.h"
#include "components/ParticleEmitter.h"
#include "components/WindTIC.h"
#include "components/BodyComp.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "platform/EngineLog.h"   // 0.12-W-1: o warn do alvo ausente
#include "render/Primitives.h"
#include <cstdio>

namespace vv {

const char* presetName(PresetKind kind) {
    switch (kind) {
        case PresetKind::PlayerBody3D:    return "PlayerBody3D";
        case PresetKind::CharacterBody3D: return "CharacterBody3D";
        case PresetKind::StaticBody3D:    return "StaticBody3D";
        case PresetKind::RigidBody3D:     return "RigidBody3D";
        case PresetKind::Mesh:            return "Mesh";   // 0.8.0 (F7)
        case PresetKind::Audio:           return "Audio";  // 0.8.11
        case PresetKind::WorldEnvironment:
            return "WorldEnvironment";   // 0.12-Y (3.Y-C)
        case PresetKind::ParticleEmitter:
            return "ParticleEmitter";    // 0.12-Y-F1
        case PresetKind::Wind:
            return "Wind";               // 0.12-Y-F2
        case PresetKind::Rain:
            return "Rain";               // 0.12-Y-F2
        case PresetKind::Snow:
            return "Snow";               // 0.12-Y-F2
        case PresetKind::Storm:
            return "Storm";              // 0.12-Y-F2
        case PresetKind::WeatherController:
            return "WeatherController";  // 0.12-W-1 (o CONTROLADOR — o
                                         // emissor é um TIC à parte)
        default:                          return "Tic";
    }
}

namespace {

// Primeiro nome livre: base, base.001, base.002… (limite prático: 999).
std::string uniqueTicName(Scene& scene, const char* base) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s", base);
    if (!scene.find(buf).valid()) {
        return std::string(buf);
    }
    for (u32 i = 1; i < 1000u; ++i) {
        std::snprintf(buf, sizeof(buf), "%s.%03u", base, i);
        if (!scene.find(buf).valid()) {
            return std::string(buf);
        }
    }
    std::snprintf(buf, sizeof(buf), "%s.999", base);
    return std::string(buf);
}

} // namespace

Handle createTicFromPreset(Scene& scene, PresetKind kind,
                           Mesh* mesh, Material* material) {
    const Handle h = scene.create(uniqueTicName(scene, presetName(kind)));
    Tic* tic = scene.get(h);
    if (!tic) {
        return Handle::invalid();
    }

    // corpos com cápsula assentam a base no y=0 (raio 0.3 + meia-altura 0.25);
    // 0.8.0: o TIC "Mesh" (esfera default r=0.5) assenta igual — r 0.5
    // 0.8.11: o TIC "Audio" assenta como os corpos (é UM objeto do mundo —
    // só sem mesh visível; o glifo marca o sítio)
    const f32 baseY = (kind == PresetKind::PlayerBody3D ||
                       kind == PresetKind::CharacterBody3D ||
                       kind == PresetKind::Mesh) ? 0.55f : 0.5f;
    Transform3D* tr = tic->addComponent<Transform3D>();
    if (tr) {
        tr->pos = Vec3{0.0f, baseY, 0.0f};
        tr->updateWorld();
    }

    // 0.8.11: o preset ÁUDIO é ESTRUTURA — Transform + AudioPlayer, SEM
    // MeshRenderer (o glifo de altifalante marca a posição no editor; o
    // clip atribui-se no Inspector; autoplay/loop/volume/pitch idem)
    if (kind == PresetKind::Audio) {
        tic->addComponent<AudioPlayer>();
        return h;
    }

    // 0.12-Y (3.Y-C): o preset WORLDENVIRONMENT é ESTRUTURA — Transform +
    // Environment (o céu/névoa/HDRI da cena; o 1.º ATIVO manda; SEM mesh,
    // SEM física). O TIC fica na origem (o "sítio" não importa — é um
    // singleton lógico da cena, como a câmara).
    if (kind == PresetKind::WorldEnvironment) {
        tr->pos = Vec3{0.0f, 0.0f, 0.0f};
        tr->updateWorld();
        tic->addComponent<Environment>();
        return h;
    }

    // 0.12-Y-F1: o preset PARTICLEEMITTER é ESTRUTURA — Transform +
    // ParticleEmitter (o jorro de fábrica: a física vive NO VERTEX SHADER;
    // SEM mesh, SEM física de corpos). Assenta no grid como os objetos.
    if (kind == PresetKind::ParticleEmitter) {
        tic->addComponent<ParticleEmitter>();
        return h;
    }

    // 0.12-Y-F2: o preset WIND é ESTRUTURA — Transform + WindTIC (o vento
    // GLOBAL da cena; o 1.º ATIVO manda — o «sítio» não importa, como o
    // WorldEnvironment). SEM mesh, SEM física, SEM partículas.
    if (kind == PresetKind::Wind) {
        tr->pos = Vec3{0.0f, 0.0f, 0.0f};
        tr->updateWorld();
        tic->addComponent<WindTIC>();
        return h;
    }

    // 0.12-Y-F2: os presets de CLIMA (Rain/Snow/Storm) são Transform +
    // ParticleEmitter + AmbientEnvironment NO MESMO TIC (a decisão do
    // dono: o clima CONFIGURA o emissor — applyWeatherPreset corre na
    // criação; assenta no grid como os objetos — a chuva CAI do sítio).
    if (kind == PresetKind::Rain || kind == PresetKind::Snow ||
        kind == PresetKind::Storm) {
        AmbientEnvironment* ae = tic->addComponent<AmbientEnvironment>();
        if (ae) {
            ae->weatherType = (kind == PresetKind::Rain)
                                  ? AmbientEnvironment::kWeatherRain
                                  : (kind == PresetKind::Snow)
                                        ? AmbientEnvironment::kWeatherSnow
                                        : AmbientEnvironment::kWeatherStorm;
            ae->intensity = (kind == PresetKind::Storm) ? 0.7f : 0.5f;
        }
        if (ParticleEmitter* pe = tic->addComponent<ParticleEmitter>()) {
            if (ae) {
                ae->applyWeatherPreset(pe);   // o clima de fábrica
            }
        }
        return h;
    }

    // 0.12-W-1: o preset WEATHERCONTROLLER é ESTRUTURA — Transform +
    // AmbientEnvironment SÓ (o CONTROLADOR do dono: o emissor é UM TIC À
    // PARTE — o "RainEmitter" com ParticleEmitter + Transform; o alvo liga
    // por NOME no targetEmitter). A criação APLICA o preset ao alvo SE ele
    // já existir na cena (o dono cria o RainEmitter primeiro); sem alvo o
    // WARN 1× diz o nome que falta — o utilizador cria/renomeia e o próximo
    // edit (ou a recriação) liga os dois.
    if (kind == PresetKind::WeatherController) {
        AmbientEnvironment* ae = tic->addComponent<AmbientEnvironment>();
        if (ae) {
            ae->weatherType = AmbientEnvironment::kWeatherRain;
            applyWeatherToTarget(scene, *tic, *ae);
        }
        return h;
    }

    MeshRenderer* mr = tic->addComponent<MeshRenderer>();
    if (mr) {
        mr->mesh = mesh;
        mr->material = material;
        // 0.8.0 (F7): o preset Mesh NASCE com a PRIMITIVA esfera default
        // (a assinatura serializa; o main pode rebindar outro tipo depois)
        if (kind == PresetKind::Mesh) {
            mr->primOn = true;
            mr->prim = primDefaults(PrimKind::Sphere);
        }
    }

    if (kind == PresetKind::PlayerBody3D) {
        tic->addComponent<InputMap>();   // SEM TouchControls — via Inspector (F4-D)
    }

    // F4: corpo de física do preset (0.8.0: o preset Mesh NÃO leva física —
    // prototipagem pura de cena/animação)
    if (kind == PresetKind::Mesh) {
        return h;
    }
    if (BodyComp* b = tic->addComponent<BodyComp>()) {
        switch (kind) {
            case PresetKind::PlayerBody3D:
            case PresetKind::CharacterBody3D:
                b->type = BodyType::Character;
                b->shape = phys::Capsule{Vec3{0.0f, 0.0f, 0.0f}, 0.3f, 0.25f};
                break;
            case PresetKind::StaticBody3D:
                b->type = BodyType::Static;
                b->shape = phys::OBB{Vec3{0.0f, 0.0f, 0.0f},
                                     Vec3{0.5f, 0.5f, 0.5f}, Quat::identity()};
                break;
            case PresetKind::RigidBody3D:
                b->type = BodyType::Rigid;
                b->shape = phys::Sphere{Vec3{0.0f, 0.0f, 0.0f}, 0.5f};
                break;
            default:
                break;
        }
    }
    return h;
}

// ---- 0.12-W-1: O ALVO DO CLIMA (a FONTE ÚNICA da resolução) -----------------
ParticleEmitter* findWeatherTarget(Scene& scene, Tic& aeTic,
                                   const AmbientEnvironment& ae) {
    // 1.º o PE do MESMO TIC (o contrato da Y-F2 byte a byte — os presets
    // Rain/Snow/Storm da casa trazem o emissor no próprio TIC)
    if (ParticleEmitter* own = aeTic.getComponent<ParticleEmitter>()) {
        return own;
    }
    // 2.º o TIC ATIVO com o NOME do alvo (o fluxo W-1 dos DOIS TICs). O
    // scene.find devolve o 1.º ATIVO com o nome (o inativo não desenha —
    // não é alvo honesto); o PE do TIC encontrado é o controlado.
    if (ae.targetEmitter.empty()) {
        return nullptr;
    }
    const Handle ht = scene.find(ae.targetEmitter);
    Tic* t = scene.get(ht);
    if (!t) {
        return nullptr;
    }
    return t->getComponent<ParticleEmitter>();
}

bool applyWeatherToTarget(Scene& scene, Tic& aeTic,
                          AmbientEnvironment& ae) {
    ParticleEmitter* pe = findWeatherTarget(scene, aeTic, ae);
    if (pe == nullptr) {
        // o WARN EDGE-TRIGGERED: 1× por AUSÊNCIA (o flag desarma quando o
        // alvo aparece — a próxima ausência volta a falar; o spam morre e
        // o dono fica a saber O NOME que falta)
        if (!ae.targetWarned) {
            ae.targetWarned = true;
            elog::warn("ambient: TIC '%s' tem clima sem emissor — o alvo "
                       "'%s' não existe na cena (cria/renomeia um TIC "
                       "ParticleEmitter com ESSE nome)",
                       aeTic.name.c_str(), ae.targetEmitter.c_str());
        }
        return false;
    }
    ae.targetWarned = false;   // o alvo voltou — a próxima ausência avisa
    return ae.applyWeatherPreset(pe);
}

} // namespace vv
