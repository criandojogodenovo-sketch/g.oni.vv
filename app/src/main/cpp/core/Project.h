#pragma once
// core/Project.h — formato do projeto .goni (F5-A).
//
// ESTRUTURA NA RAIZ DO STORAGE (no device: getExternalFilesDir):
//   project.goni      — manifesto (nome/versão/cenas/settings)
//   scenes/*.goni     — cenas (formato SceneSerializer v1)
//   meshes/           — assets de malha (.obj/.gltf/.glb) — F5-B/C
//   textures/         — assets de textura (.png) — F5-D
//
// MANIFESTO (project.goni, JSON):
//   {
//     "version": 1,
//     "name": "meu projeto",
//     "activeScene": 0,
//     "scenes": ["scenes/main.goni"],   // refs RELATIVOS — intactos no reopen
//     "settings": { ... }               // objeto aberto: persistido verbatim
//   }
//
// "settings" é transportado sem interpretação (fases futuras leem as chaves
// que lhes couberem) — o round-trip verbatim é garantido por teste.
//
// "Persistir projeto aberto": activeScene vive no manifesto; o boot abre o
// projeto da raiz do storage e recarrega a cena ativa (F5-A/3). Não há
// estado de projeto fora do manifesto — trocar de device preserva tudo.
#include <string>
#include <vector>
#include "core/Json.h"
#include "core/ProjectStorage.h"
#include "core/SceneSerializer.h"

namespace vv {

class Scene;

class Project {
public:
    static constexpr const char* kManifestFile = "project.goni";
    static constexpr const char* kDirScenes    = "scenes";
    static constexpr const char* kDirMeshes    = "meshes";
    static constexpr const char* kDirTextures  = "textures";
    static constexpr u32 kVersion = 1;

    std::string name = "projeto";
    u32 version = kVersion;
    std::vector<std::string> scenes;   // caminhos relativos ("scenes/main.goni")
    u32 activeScene = 0;               // índice em scenes
    Json settings;                     // objeto aberto (verbatim; tipo Object)

    // Cria a estrutura completa: scenes/ meshes/ textures/ + manifesto com
    // uma cena inicial "scenes/main.goni" (vazia, já escrita).
    // FALHA sem tocar em nada se já existir manifesto (nunca destrói projeto).
    static bool createNew(ProjectStorage& st, const std::string& name, Project& out);

    // Lê o manifesto existente (migra versão antiga → atual).
    // false se ausente, corrompido ou com refs inválidos.
    static bool open(ProjectStorage& st, Project& out);

    // Escreve o manifesto (createNew e qualquer mudança de estado chamam isto).
    bool saveManifest(ProjectStorage& st) const;

    // Adiciona "scenes/<sceneName>.goni" (sem duplicar) e torna-a ativa.
    // NÃO grava o manifesto nem a cena — o chamador persiste o que precisar.
    bool addScene(const std::string& sceneName);

    // Caminho relativo da cena ativa; nullptr se o estado não tem cena válida.
    const std::string* activeScenePath() const;

    // Cena ativa ↔ storage (texto via SceneSerializer::dump/loadText — o
    // formato viaja DENTRO da interface de storage, pronto p/ SAF na F5.2).
    bool saveActiveScene(ProjectStorage& st, const Scene& scene) const;
    bool loadActiveScene(ProjectStorage& st, Scene& scene,
                         const SceneSerializer::LoadCtx& ctx) const;
};

} // namespace vv
