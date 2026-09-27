# app/src/main/cpp/CMakeLists.txt — G.One VV 0.1.0 (F1)
# Core C++17 + NativeActivity (native_app_glue do NDK).
cmake_minimum_required(VERSION 3.22.1)
project(goni_vv LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shim do sistema para NativeActivity (fonte oficial do NDK).
add_library(native_app_glue STATIC
        ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
        ${ANDROID_NDK}/sources/android/native_app_glue)

# Fontes do goni_vv (passos incrementais da F1: platform → core → render → ui → diagnóstico).
add_library(goni_vv SHARED
        core/Types.h
        core/Handle.h
        core/Tic.h
        core/Scene.h
        core/Scene.cpp
        core/Time.h
        core/Time.cpp
        math/Math.h
        render/QuadBatch.h
        render/Renderer.h
        render/Renderer.cpp
        ui/FontAtlas.h
        ui/FontAtlas.cpp
        ui/UiContext.h
        ui/UiContext.cpp
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

target_link_libraries(goni_vv
        native_app_glue
        android
        EGL
        GLESv3
        log)
