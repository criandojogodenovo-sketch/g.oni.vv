#include "core/ComponentStore.h"
#include "core/Scene.h"

namespace vv {

Tic* resolveTic(Scene* scene, Handle h) {
    return scene ? scene->get(h) : nullptr;
}

ComponentStore& storeOf(Scene* scene) {
    return scene->components();
}

const ComponentStore& storeOf(const Scene* scene) {
    return scene->components();
}

// Ordem de registo FIXA — o serializer e os testes assumem estes ids:
// 0=Transform3D, 1=MeshRenderer, 2=InputMap, 3=BodyComp (4=TouchControls na F4-D).
ComponentStore::ComponentStore(Scene* owner) : scene_(owner) {
    registry_.add("Transform3D", [](ComponentStore& s, Handle h) {
        return s.add<Transform3D>(h) != nullptr;
    });
    registry_.add("MeshRenderer", [](ComponentStore& s, Handle h) {
        return s.add<MeshRenderer>(h) != nullptr;
    });
    registry_.add("InputMap", [](ComponentStore& s, Handle h) {
        return s.add<InputMap>(h) != nullptr;
    });
    registry_.add("BodyComp", [](ComponentStore& s, Handle h) {
        return s.add<BodyComp>(h) != nullptr;
    });
}

void ComponentStore::removeAll(Handle h) {
    transforms_.remove(h);
    meshRenderers_.remove(h);
    inputMaps_.remove(h);
    bodies_.remove(h);
}

bool ComponentStore::hasAny(Handle h) const {
    return transforms_.find(h) != nullptr ||
           meshRenderers_.find(h) != nullptr ||
           inputMaps_.find(h) != nullptr ||
           bodies_.find(h) != nullptr;
}

} // namespace vv
