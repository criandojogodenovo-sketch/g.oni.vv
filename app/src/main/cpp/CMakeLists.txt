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
        core/ProjectStorage.h
        core/ProjectStorage.cpp
        core/SafStorage.h
        core/SafStorage.cpp
        core/FsStorage.h
        core/FsStorage.cpp
        assets/Assets.h
        assets/ObjImporter.h
        assets/ObjImporter.cpp
        assets/ObjExporter.h
        assets/ObjExporter.cpp
        assets/ResourceManager.h
        assets/ResourceManager.cpp
        assets/GltfImporter.h
        assets/GltfImporter.cpp
        assets/GltfTextures.h
        assets/GltfTextures.cpp
        assets/GltfInstantiate.h
        assets/GltfInstantiate.cpp
        assets/PngLoader.h
        assets/PngLoader.cpp
        assets/TextureCompressor.h
        assets/TextureCompressor.cpp
        assets/MipGen.h
        assets/MipGen.cpp
        assets/TextureCache.h
        assets/TextureCache.cpp
        assets/TexturePipeline.h
        assets/TexturePipeline.cpp
        assets/Etc2Compressor.cpp
        assets/AstcCompressor.cpp
        assets/stb/stb_image.h
        # F5.1-A: vendors de compressão (licenças preservadas nas pastas)
        vendor/etcpak/ProcessRGB.cpp
        vendor/etcpak/Tables.cpp
        vendor/etcpak/Dither.cpp
        vendor/astc-encoder/astcenc_averages_and_directions.cpp
        vendor/astc-encoder/astcenc_block_sizes.cpp
        vendor/astc-encoder/astcenc_color_quantize.cpp
        vendor/astc-encoder/astcenc_color_unquantize.cpp
        vendor/astc-encoder/astcenc_compress_symbolic.cpp
        vendor/astc-encoder/astcenc_compute_variance.cpp
        vendor/astc-encoder/astcenc_decompress_symbolic.cpp
        vendor/astc-encoder/astcenc_diagnostic_trace.cpp
        vendor/astc-encoder/astcenc_entry.cpp
        vendor/astc-encoder/astcenc_find_best_partitioning.cpp
        vendor/astc-encoder/astcenc_ideal_endpoints_and_weights.cpp
        vendor/astc-encoder/astcenc_image.cpp
        vendor/astc-encoder/astcenc_integer_sequence.cpp
        vendor/astc-encoder/astcenc_mathlib.cpp
        vendor/astc-encoder/astcenc_mathlib_softfloat.cpp
        vendor/astc-encoder/astcenc_partition_tables.cpp
        vendor/astc-encoder/astcenc_percentile_tables.cpp
        vendor/astc-encoder/astcenc_pick_best_endpoint_format.cpp
        vendor/astc-encoder/astcenc_quantization.cpp
        vendor/astc-encoder/astcenc_symbolic_physical.cpp
        vendor/astc-encoder/astcenc_weight_align.cpp
        vendor/astc-encoder/astcenc_weight_quant_xfer_tables.cpp
        render/Texture.h
        render/Texture.cpp
        render/GpuAssets.h
        render/GpuAssets.cpp
        core/Project.h
        core/Project.cpp
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
        components/TouchControls.h
        components/TouchControls.cpp
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
        platform/EngineLog.h
        platform/EngineLog.cpp
        platform/CrashHandler.h
        platform/CrashHandler.cpp
        platform/EglContext.h
        platform/EglContext.cpp
        platform/InputState.h
        platform/InputState.cpp
        # F5.1-C: ponte SAF (device-only — a suíte core compila sem estes TUs)
        platform/SafBridge.cpp
        platform/SafIoJni.cpp
        platform/main.cpp)

target_include_directories(goni_vv PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/etcpak
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/astc-encoder)

# F5.1-A: flags por TU dos vendors.
#  • etcpak: usa SSE4.1 quando disponível (x86_64 — hospedeiro/CI; no
#    Android arm64 o caminho NEON liga sozinho via __ARM_NEON).
#  • astc-encoder: SIMD auto-detectado pelos headers (NEON no arm64);
#    -fsigned-char estabiliza a matemática de char em ARM.
if(NOT ANDROID)
        set_source_files_properties(
                vendor/etcpak/ProcessRGB.cpp
                vendor/etcpak/Tables.cpp
                vendor/etcpak/Dither.cpp
                PROPERTIES COMPILE_OPTIONS "-msse4.1;-w")
endif()
set_source_files_properties(
        vendor/astc-encoder/astcenc_averages_and_directions.cpp
        vendor/astc-encoder/astcenc_block_sizes.cpp
        vendor/astc-encoder/astcenc_color_quantize.cpp
        vendor/astc-encoder/astcenc_color_unquantize.cpp
        vendor/astc-encoder/astcenc_compress_symbolic.cpp
        vendor/astc-encoder/astcenc_compute_variance.cpp
        vendor/astc-encoder/astcenc_decompress_symbolic.cpp
        vendor/astc-encoder/astcenc_diagnostic_trace.cpp
        vendor/astc-encoder/astcenc_entry.cpp
        vendor/astc-encoder/astcenc_find_best_partitioning.cpp
        vendor/astc-encoder/astcenc_ideal_endpoints_and_weights.cpp
        vendor/astc-encoder/astcenc_image.cpp
        vendor/astc-encoder/astcenc_integer_sequence.cpp
        vendor/astc-encoder/astcenc_mathlib.cpp
        vendor/astc-encoder/astcenc_mathlib_softfloat.cpp
        vendor/astc-encoder/astcenc_partition_tables.cpp
        vendor/astc-encoder/astcenc_percentile_tables.cpp
        vendor/astc-encoder/astcenc_pick_best_endpoint_format.cpp
        vendor/astc-encoder/astcenc_quantization.cpp
        vendor/astc-encoder/astcenc_symbolic_physical.cpp
        vendor/astc-encoder/astcenc_weight_align.cpp
        vendor/astc-encoder/astcenc_weight_quant_xfer_tables.cpp
        PROPERTIES COMPILE_OPTIONS "-w;-fsigned-char")

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
