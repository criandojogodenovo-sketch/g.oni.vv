#pragma once
// platform/SafBridge.h — ponte nativo ↔ Java do SAF (F5.1-C, device).
//
// O lado C++ é chamado do loop principal: openTreePicker/openImportPicker/
// openExportPicker lançam os intents Java (via JNI sobre a VvActivity) e o
// resultado chega DE VOLTA por Java_vv_goni_VvActivity_nativeOnActivityResult
// → handler injetado com setHandler() (main.cpp alimenta a máquina de
// estado + RoutingStorage).
//
// Este TU só entra na build ANDROID (usa JNI; o CI compila a suíte core
// sem ele — o mesmo se aplica a SafIoJni.cpp).
#include <string>
#include "platform/Saf.h"

namespace vv::saf {

// handler dos resultados — injetado por main.cpp ANTES de abrir pickers
void setHandler(ResultHandler fn, void* user);

// cache de env/classe/métodos — chamar 1× no android_main com o
// app->activity (o thread do NativeActivity já está anexado à VM)
void initJava(void* vm, void* activityObject);

bool openTreePicker(void* activityObject, i32 request);
bool openImportPicker(void* activityObject, i32 request);
bool openExportPicker(void* activityObject, i32 request,
                      const char* suggestedName);

} // namespace vv::saf
