# tests/CMakeLists.txt — suíte do core da G.One VV (CI Linux).
# Compila apenas o core (puro C++17, sem dependência de Android).
cmake_minimum_required(VERSION 3.16)
project(goni_vv_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp)

add_executable(test_core
        test_main.cpp
        test_scene.cpp
        test_time.cpp
        ${CORE_DIR}/core/Scene.cpp
        ${CORE_DIR}/core/Time.cpp)

target_include_directories(test_core PRIVATE ${CORE_DIR})

enable_testing()
add_test(NAME core COMMAND test_core)
