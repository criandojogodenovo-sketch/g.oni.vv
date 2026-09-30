// core/Project.cpp — manifesto do projeto .goni (F5-A).
#include "core/Project.h"
#include "core/Scene.h"

namespace vv {

namespace {

Json manifestToJson(const Project& p) {
    Json root = Json::makeObject();
    root.addMember("version", Json::makeNumber(p.version));
    root.addMember("name", Json::makeString(p.name));
    root.addMember("activeScene", Json::makeNumber(p.activeScene));
    Json sc = Json::makeArray();
    for (const std::string& s : p.scenes) {
        sc.addItem(Json::makeString(s));
    }
    root.addMember("scenes", std::move(sc));
    root.addMember("settings",
                   p.settings.type == Json::Type::Object ? p.settings : Json::makeObject());
    return root;
}

bool manifestFromJson(const Json& root, Project& out) {
    if (root.type != Json::Type::Object) {
        return false;
    }
    Project p;
    if (const Json* v = root.find("version"); v && v->type == Json::Type::Number) {
        p.version = static_cast<u32>(v->number);
    }
    if (const Json* n = root.find("name"); n && n->type == Json::Type::String) {
        p.name = n->string;
    }
    if (const Json* a = root.find("activeScene"); a && a->type == Json::Type::Number) {
        p.activeScene = static_cast<u32>(a->number);
    }
    if (const Json* sc = root.find("scenes"); sc && sc->type == Json::Type::Array) {
        for (const Json& s : sc->items) {
            if (s.type != Json::Type::String || !validRelPath(s.string)) {
                return false;   // ref inválido = manifesto corrompido
            }
            p.scenes.push_back(s.string);
        }
    }
    if (const Json* st = root.find("settings");
        st && st->type == Json::Type::Object) {
        p.settings = *st;
    } else {
        p.settings = Json::makeObject();
    }
    out = std::move(p);
    return true;
}

} // namespace

bool Project::createNew(ProjectStorage& st, const std::string& name, Project& out) {
    // F5.4-hotfix: só criar com AUSÊNCIA CONFIRMADA (probe). Com um bool,
    // "verificação falhou" e "não existe" eram a mesma resposta — o boot
    // criava o projeto POR CIMA de ficheiros que estavam lá (o SAF nunca
    // sobrescreve por nome → "project.goni (2)", "main.goni (1).json"…).
    // Unknown = existência indecidida → NUNCA criar (falha honesta).
    if (st.probe(kManifestFile) != Presence::Absent) {
        return false;   // existe OU indecidível — nunca destruir/duplicar
    }
    if (!st.makeDirs(kDirScenes) || !st.makeDirs(kDirMeshes) || !st.makeDirs(kDirTextures)) {
        return false;
    }
    Project p;
    p.name = name.empty() ? "projeto" : name;
    p.scenes.push_back(std::string(kDirScenes) + "/main.goni");
    p.activeScene = 0;
    p.settings = Json::makeObject();

    // cena inicial vazia, já no disco — o manifesto nunca refere fantasma
    Scene empty;
    if (!p.saveActiveScene(st, empty) || !p.saveManifest(st)) {
        return false;
    }
    out = std::move(p);
    return true;
}

bool Project::open(ProjectStorage& st, Project& out) {
    std::string text;
    if (!st.readText(kManifestFile, text)) {
        return false;
    }
    Json root;
    if (!Json::parse(text.data(), text.size(), root)) {
        return false;
    }
    // migração mínima: manifesto sem "version" (rascunho) → v1
    if (root.find("version") == nullptr) {
        root.addMember("version", Json::makeNumber(kVersion));
    }
    Project p;
    if (!manifestFromJson(root, p) || p.version > kVersion) {
        return false;   // corrompido ou escrito por versão futura
    }
    if (p.activeScene >= p.scenes.size()) {
        p.activeScene = 0;   // defensivo: clamp (scenes vazio → sem cena ativa)
    }
    out = std::move(p);
    return true;
}

bool Project::openOrCreate(ProjectStorage& st, const std::string& name, Project& out) {
    Project p;
    if (open(st, p)) {
        out = std::move(p);
        return true;
    }
    return createNew(st, name, out);
}

bool Project::saveManifest(ProjectStorage& st) const {
    const Json root = manifestToJson(*this);
    return st.writeText(kManifestFile, root.dump());
}

bool Project::addScene(const std::string& sceneName) {
    std::string clean;
    for (const char c : sceneName) {
        if (c == '/' || c == '\\' || c == ':' || c == '.') {
            return false;   // nome simples: só o stem, sem caminho
        }
        clean.push_back(c);
    }
    if (clean.empty()) {
        return false;
    }
    const std::string rel = std::string(kDirScenes) + "/" + clean + ".goni";
    for (const std::string& s : scenes) {
        if (s == rel) {
            activeScene = static_cast<u32>(&s - scenes.data());
            return true;   // já existe — só ativa
        }
    }
    scenes.push_back(rel);
    activeScene = static_cast<u32>(scenes.size() - 1);
    return true;
}

const std::string* Project::activeScenePath() const {
    if (activeScene >= scenes.size()) {
        return nullptr;
    }
    return &scenes[activeScene];
}

bool Project::saveActiveScene(ProjectStorage& st, const Scene& scene) const {
    const std::string* rel = activeScenePath();
    if (!rel) {
        return false;
    }
    return st.writeText(*rel, SceneSerializer::dump(scene));
}

bool Project::loadActiveScene(ProjectStorage& st, Scene& scene,
                              const SceneSerializer::LoadCtx& ctx) const {
    const std::string* rel = activeScenePath();
    if (!rel) {
        return false;
    }
    std::string text;
    if (!st.readText(*rel, text)) {
        return false;
    }
    return SceneSerializer::loadText(scene, text, ctx);
}

} // namespace vv
