# tests/CMakeLists.txt — suíte do core da G.One VV (CI Linux).
# Compila apenas o core (puro C++17, sem dependência de Android).
cmake_minimum_required(VERSION 3.16)
project(goni_vv_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp)

add_executable(test_core
        test_main.cpp
        test_project.cpp
        test_safearea.cpp
        test_textfit.cpp
        test_play.cpp
        test_scroll.cpp
        test_scene.cpp
        test_time.cpp
        test_math.cpp
        test_cube.cpp
        test_camera.cpp
        test_shapes.cpp
        test_physics.cpp
        test_grid.cpp
        test_components.cpp
        test_transform.cpp
        test_presets.cpp
        test_serializer.cpp
        test_systems.cpp
        ${CORE_DIR}/core/Scene.cpp
        ${CORE_DIR}/core/Time.cpp
        ${CORE_DIR}/core/ComponentRegistry.cpp
        ${CORE_DIR}/core/ComponentStore.cpp
        ${CORE_DIR}/core/Presets.cpp
        ${CORE_DIR}/core/SceneSerializer.cpp
        ${CORE_DIR}/core/Project.cpp
        ${CORE_DIR}/core/ProjectStorage.cpp
        ${CORE_DIR}/core/TransformSystem.cpp
        # GL-free (cubo e câmara são lógica pura — sem contexto OpenGL)
        ${CORE_DIR}/render/Cube.cpp
        ${CORE_DIR}/render/Camera.cpp
        # F4: física pura (sem GL)
        ${CORE_DIR}/physics/Shapes.cpp
        ${CORE_DIR}/physics/PhysicsSystem.cpp
        ${CORE_DIR}/components/TouchControls.cpp
        ${CORE_DIR}/core/TransformSystem.cpp)


target_include_directories(test_core PRIVATE ${CORE_DIR})

enable_testing()
add_test(NAME core COMMAND test_core)
