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
// 0=Transform3D, 1=MeshRenderer, 2=InputMap, 3=BodyComp, 4=TouchControls,
// 5=UiCanvas (0.7.0), 6=Camera (0.7.7 — sempre NO FIM: ids antigos intactos).
// 0.8.0: 7=AnimationPlayer (depois da câmara, mesma regra: sempre no fim).
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
    registry_.add("TouchControls", [](ComponentStore& s, Handle h) {
        return s.add<TouchControls>(h) != nullptr;
    });
    registry_.add("UiCanvas", [](ComponentStore& s, Handle h) {
        return s.add<UiCanvas>(h) != nullptr;
    });
    registry_.add("Camera", [](ComponentStore& s, Handle h) {   // 0.7.7
        return s.add<CameraComp>(h) != nullptr;
    });
    registry_.add("AnimationPlayer", [](ComponentStore& s, Handle h) {   // 0.8.0
        return s.add<AnimationPlayer>(h) != nullptr;
    });
}

void ComponentStore::removeAll(Handle h) {
    transforms_.remove(h);
    meshRenderers_.remove(h);
    inputMaps_.remove(h);
    bodies_.remove(h);
    touchControls_.remove(h);
    uiCanvases_.remove(h);
    cameras_.remove(h);   // 0.7.7
    animators_.remove(h);   // 0.8.0
}

bool ComponentStore::hasAny(Handle h) const {
    return transforms_.find(h) != nullptr ||
           meshRenderers_.find(h) != nullptr ||
           inputMaps_.find(h) != nullptr ||
           bodies_.find(h) != nullptr ||
           touchControls_.find(h) != nullptr ||
           uiCanvases_.find(h) != nullptr ||
           cameras_.find(h) != nullptr ||   // 0.7.7
           animators_.find(h) != nullptr;   // 0.8.0
}

} // namespace vv
