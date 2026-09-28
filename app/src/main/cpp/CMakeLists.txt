# app/src/main/cpp/CMakeLists.txt — G.One VV (F1→F4)
# Core C++17 + NativeActivity (native_app_glue do NDK).
cmake_minimum_required(VERSION 3.22.1)
project(goni_vv LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fontes do goni_vv (F3: componentes + registry/store; presets/serializer/ui entram nos próximos passos).
add_library(goni_vv SHARED
        core/Types.h
        core/Handle.h
        core/Tic.h
        core/Component.h
        core/ComponentStorage.h
        core/ComponentRegistry.h
        core/ComponentRegistry.cpp
        core/ComponentStore.h
        core/ComponentStore.cpp
        core/Presets.h
        core/Presets.cpp
        core/Json.h
        core/SceneSerializer.h
        core/SceneSerializer.cpp
        core/Tick.h
        core/TransformSystem.h
        core/TransformSystem.cpp
        core/Scene.h
        core/Scene.cpp
        core/Time.h
        core/Time.cpp
        components/Transform3D.h
        components/MeshRenderer.h
        components/BodyComp.h
        components/InputMap.h
        physics/Shapes.h
        physics/Shapes.cpp
        physics/InputSource.h
        physics/PhysicsSystem.h
        physics/PhysicsSystem.cpp
        math/Math.h
        render/Vertex.h
        render/Mesh.h
        render/Mesh.cpp
        render/Material.h
        render/Material.cpp
        render/Cube.h
        render/Cube.cpp
        render/Grid.h
        render/Grid.cpp
        render/Camera.h
        render/Camera.cpp
        render/DrawStats.h
        render/QuadBatch.h
        render/Renderer.h
        render/Renderer.cpp
        ui/FontAtlas.h
        ui/FontAtlas.cpp
        ui/UiContext.h
        ui/UiContext.cpp
        ui/EditorUi.h
        ui/EditorUi.cpp
        ui/stb/stb_truetype.h
        platform/Log.h
        platform/CrashHandler.h
        platform/CrashHandler.cpp
        platform/EglContext.h
        platform/EglContext.cpp
        platform/InputState.h
        platform/InputState.cpp
        platform/main.cpp)

target_include_directories(goni_vv PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Shim do sistema para NativeActivity (fonte oficial do NDK).
# FIX UnsatisfiedLinkError: o glue tem de ser compilado DENTRO de goni_vv.
# Antes era uma static library ligada por nome — como nenhum objeto de goni_vv
# referencia ANativeActivity_onCreate, o linker não extraía
# android_native_app_glue.o do .a e o símbolo de entrada nunca chegava ao
# libgoni_vv.so → UnsatisfiedLinkError no arranque do NativeActivity.
if(ANDROID)
        set(VV_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
        target_sources(goni_vv PRIVATE ${VV_GLUE_DIR}/android_native_app_glue.c)
        target_include_directories(goni_vv PRIVATE ${VV_GLUE_DIR})
        # Preserva os símbolos de entrada na ligação (defensivo: o -u força o
        # linker a resolver os símbolos, extraindo os objetos que os definem,
        # mesmo que o glue um dia volte a ser static library). Forma
        # -Wl,-u,SYMBOL: um token por símbolo (o CMake deduplica flags -u
        # repetidos, o que deixaria o segundo símbolo órfão).
        target_link_options(goni_vv PRIVATE
                -Wl,-u,ANativeActivity_onCreate
                -Wl,-u,android_main)
endif()

target_link_libraries(goni_vv
        android
        EGL
        GLESv3
        log)
