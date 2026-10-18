// core/CameraUtil.cpp — o invariante "uma câmara ativa por cena" (0.7.7).
#include "core/CameraUtil.h"
#include "components/CameraComp.h"
#include "components/Transform3D.h"
#include "core/Scene.h"

namespace vv {

Tic* findActiveCameraTic(Scene& scene) {
    Tic* found = nullptr;
    scene.forEachActive([&](Tic& t) {
        if (found) {
            return;
        }
        if (CameraComp* c = t.getComponent<CameraComp>()) {
            if (c->active && t.getComponent<Transform3D>()) {
                found = &t;
            }
        }
    });
    return found;
}

CameraComp* findActiveCamera(Scene& scene) {
    Tic* t = findActiveCameraTic(scene);
    return t ? t->getComponent<CameraComp>() : nullptr;
}

bool setOnlyActiveCamera(Scene& scene, Handle h) {
    Tic* target = scene.get(h);
    if (!target) {
        return false;
    }
    CameraComp* cam = target->getComponent<CameraComp>();
    if (!cam) {
        return false;
    }
    scene.forEachActive([&](Tic& t) {
        if (CameraComp* c = t.getComponent<CameraComp>()) {
            c->active = (t.handle == h);
        }
    });
    return true;
}

bool clearActiveCamera(Scene& scene, Handle h) {
    Tic* target = scene.get(h);
    if (!target) {
        return false;
    }
    if (CameraComp* cam = target->getComponent<CameraComp>()) {
        cam->active = false;
        return true;
    }
    return false;
}

void enforceSingleActiveCamera(Scene& scene) {
    bool seen = false;
    scene.forEachActive([&](Tic& t) {
        if (CameraComp* c = t.getComponent<CameraComp>()) {
            if (c->active && !seen) {
                seen = true;   // a PRIMEIRA (ordem do manifesto) fica
            } else {
                c->active = false;
            }
        }
    });
}

} // namespace vv
