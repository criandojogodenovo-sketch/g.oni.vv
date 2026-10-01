# tests/CMakeLists.txt — suíte do core da G.One VV (CI Linux).
# Compila apenas o core (puro C++17, sem dependência de Android).
cmake_minimum_required(VERSION 3.16)
project(goni_vv_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp)

add_executable(test_core
        test_main.cpp
        test_logs_crash.cpp
        test_storageperm.cpp
        test_handshake.cpp
        test_saf_project.cpp
        test_assetpersist.cpp
        test_ui.cpp
        test_project.cpp
        test_import_obj.cpp
        test_resources.cpp
        test_import_gltf.cpp
        test_gltf_textures.cpp
        test_f51_roundtrip.cpp
        test_assets.cpp
        test_storage.cpp
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
        # 0.6.7: lifecycle GL — re-upload de recursos após TERM/INIT_WINDOW
        # (fix dos "cubinhos" do C33; contadores do stub GLES3)
        test_lifecycle.cpp
        # 0.6.8: play mode com janela própria (transição, sem painéis,
        # touchcontrols sem sobreposição, pose restaurada, orbit off)
        test_playui.cpp
        # 0.6.9: gizmos de transformação (hit-test/drag/snapping/desenho)
        test_gizmo.cpp
        # F6: wiring do seletor de textura (dispatch puro + sequência do
        # main + bind no stub GL + round-trip .goni — o fix do C33 0.6.9)
        test_assetpick.cpp
        # 0.7.0: UI criável — runtime (âncoras/hit-test/ações/serialização) e
        # editor dedicado (viewport 2D, teclado in-app, gestão de TICs, cor)
        test_uicanvas.cpp
        test_uieditor.cpp
        # 0.7.1: cenas múltiplas + transições (projeto e2e, SceneFx puro,
        # overlay CENAS, ações Scene.Load/Transition)
        test_scenes.cpp
        # 0.7.2: import robusto (navegador com galeria, caminho visível,
        # aplicar-após-import)
        test_browser.cpp
        # 0.7.3: joystick editável (layout derivado dos campos + input) e
        # compostos Menu/Card/Article (desenho + serialização)
        test_joystick.cpp
        # 0.7.4: paridade editor<->Play + texturas de fundo + Label sem
        # fundo + Menu configurável + containers VBox/HBox + serialização
        test_uilayout.cpp
        # 0.7.6: barra final de 5 grupos (layout dinâmico, segmented
        # exclusivos, G4 condicional, ícones dentro do rect, Theme central)
        test_toolbar.cpp
        # 0.7.7: TIC de câmara (frustum, seleção por toque, gizmos com
        # escalar=fov, handles far/fov, serialização, play usa a ativa)
        test_cameratic.cpp
        # 0.7.8: fronteira 3D↔UI (estado GL do pass de UI afirmado no stub;
        # Play com câmara ativa + UiCanvas → rects do resolver, textScale 1)
        test_passgl.cpp
        ${CORE_DIR}/core/Scene.cpp
        ${CORE_DIR}/core/Time.cpp
        ${CORE_DIR}/core/AnimationSystem.cpp
        ${CORE_DIR}/core/ComponentRegistry.cpp
        ${CORE_DIR}/core/ComponentStore.cpp
        ${CORE_DIR}/core/Presets.cpp
        ${CORE_DIR}/core/SceneSerializer.cpp
        ${CORE_DIR}/core/Project.cpp
        ${CORE_DIR}/core/ProjectStorage.cpp
        # F5.4-hotfix: Salvar materializa assets em-runtime (cubo → meshes/)
        ${CORE_DIR}/core/AssetPersist.cpp
        ${CORE_DIR}/core/FsStorage.cpp
        # F5.4: SafStorage — ProjectStorage sobre a pasta SAF escolhida no
        # Gestor (testado contra o FakeSafIo deste TU)
        ${CORE_DIR}/core/SafStorage.cpp
        # F5: assets (importadores GL-free)
        ${CORE_DIR}/assets/ObjImporter.cpp
        ${CORE_DIR}/assets/ObjExporter.cpp
        ${CORE_DIR}/assets/ResourceManager.cpp
        ${CORE_DIR}/assets/GltfImporter.cpp
        ${CORE_DIR}/assets/GltfTextures.cpp
        ${CORE_DIR}/assets/GltfInstantiate.cpp
        ${CORE_DIR}/assets/GltfAnim.cpp
        ${CORE_DIR}/assets/PngLoader.cpp
        ${CORE_DIR}/assets/MipGen.cpp
        ${CORE_DIR}/assets/TextureCache.cpp
        ${CORE_DIR}/assets/TexturePipeline.cpp
        ${CORE_DIR}/assets/TextureCompressor.cpp
        ${CORE_DIR}/assets/Etc2Compressor.cpp
        ${CORE_DIR}/assets/AstcCompressor.cpp
        # F5.1-A: vendors de compressão (licenças preservadas nas pastas)
        ${CORE_DIR}/vendor/etcpak/ProcessRGB.cpp
        ${CORE_DIR}/vendor/etcpak/Tables.cpp
        ${CORE_DIR}/vendor/etcpak/Dither.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_averages_and_directions.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_block_sizes.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_color_quantize.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_color_unquantize.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_compress_symbolic.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_compute_variance.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_decompress_symbolic.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_diagnostic_trace.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_entry.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_find_best_partitioning.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_ideal_endpoints_and_weights.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_image.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_integer_sequence.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_mathlib.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_mathlib_softfloat.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_partition_tables.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_percentile_tables.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_pick_best_endpoint_format.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_quantization.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_symbolic_physical.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_weight_align.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_weight_quant_xfer_tables.cpp
        ${CORE_DIR}/core/TransformSystem.cpp
        # GL-free (cubo e câmara são lógica pura — sem contexto OpenGL)
        ${CORE_DIR}/render/Cube.cpp
        ${CORE_DIR}/render/Primitives.cpp
        ${CORE_DIR}/render/Camera.cpp
        # F4: física pura (sem GL)
        ${CORE_DIR}/physics/Shapes.cpp
        ${CORE_DIR}/physics/PhysicsSystem.cpp
        ${CORE_DIR}/components/TouchControls.cpp
        ${CORE_DIR}/core/TransformSystem.cpp
        # F5.0-fix: UI REAL no hospedeiro — FontAtlas (stbtt), UiContext,
        # EditorUi e InputState compilam contra os stubs GLES3/android
        # (tests/stub) e os testes aferem os batches de CPU (sem GL real)
        ${CORE_DIR}/ui/UiContext.cpp
        ${CORE_DIR}/ui/EditorUi.cpp
        # 0.7.0: UI criável (componente + runtime + editor dedicado)
        ${CORE_DIR}/components/UiCanvas.cpp
        ${CORE_DIR}/components/AnimationPlayer.cpp
        ${CORE_DIR}/ui/UiRuntime.cpp
        ${CORE_DIR}/ui/UiEditor.cpp
        # 0.7.1: transições de cena (fade/slide — puro + draw em batch)
        ${CORE_DIR}/ui/SceneFx.cpp
        # 0.6.9: gizmos (hit-test/drag/snapping GL-free + desenho em batch)
        ${CORE_DIR}/ui/Gizmo.cpp
        # 0.7.6: toolbar final (5 grupos) + ícones vetoriais
        ${CORE_DIR}/ui/Toolbar.cpp
        # 0.8.0: animacao (player + timeline do editor)
        ${CORE_DIR}/ui/Timeline.cpp
        # 0.8.0: testes da campanha F7
        test_anim.cpp
        test_prims.cpp
        test_timeline.cpp
        # 0.8.1: import de clips glTF
        test_gltfanim.cpp
        ${CORE_DIR}/ui/Icons.cpp
        # 0.7.7: câmara de cena (componente/invariante + gizmo do frustum)
        ${CORE_DIR}/core/CameraUtil.cpp
        ${CORE_DIR}/ui/CamGizmo.cpp
        ${CORE_DIR}/ui/FontAtlas.cpp
        ${CORE_DIR}/platform/InputState.cpp
        ${CORE_DIR}/platform/EngineLog.cpp
        ${CORE_DIR}/platform/FileApi.cpp
        ${CORE_DIR}/platform/CrashHandler.cpp
        # F5.3: a PONTE JNI inteira compila na suíte com o fake JNI
        # (tests/stub/jni.h) — o handshake inverso é testado no hospedeiro
        # (o teste chama Java_vv_goni_VvActivity_nativeRegisterActivity
        # diretamente, o papel do "stub Java")
        ${CORE_DIR}/platform/StorageBridge.cpp
        ${CORE_DIR}/render/Renderer.cpp
        ${CORE_DIR}/render/Mesh.cpp
        ${CORE_DIR}/render/Material.cpp
        ${CORE_DIR}/render/Texture.cpp
        # 0.6.7: GpuAssets na suíte — releaseAll/re-upload após TERM_WINDOW
        ${CORE_DIR}/render/GpuAssets.cpp)


target_include_directories(test_core PRIVATE
        # stubs PRIMEIRO: <GLES3/gl3.h> e <android/input.h> resolvem para os
        # no-ops do hospedeiro (a build Android usa o NDK, sem estes caminhos)
        ${CMAKE_CURRENT_LIST_DIR}/stub
        ${CORE_DIR}
        ${CORE_DIR}/vendor/etcpak
        ${CORE_DIR}/vendor/astc-encoder)

# F5.1-A: flags por TU dos vendors (idênticas à build do APK; ver lá a
# nota — SSE4.1 no hospedeiro x86_64, NEON auto no arm64, -fsigned-char
# para a matemática de char do astc-encoder).
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
        set_source_files_properties(
                ${CORE_DIR}/vendor/etcpak/ProcessRGB.cpp
                ${CORE_DIR}/vendor/etcpak/Tables.cpp
                ${CORE_DIR}/vendor/etcpak/Dither.cpp
                PROPERTIES COMPILE_OPTIONS "-msse4.1;-w")
endif()
set_source_files_properties(
        ${CORE_DIR}/vendor/astc-encoder/astcenc_averages_and_directions.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_block_sizes.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_color_quantize.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_color_unquantize.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_compress_symbolic.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_compute_variance.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_decompress_symbolic.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_diagnostic_trace.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_entry.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_find_best_partitioning.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_ideal_endpoints_and_weights.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_image.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_integer_sequence.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_mathlib.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_mathlib_softfloat.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_partition_tables.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_percentile_tables.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_pick_best_endpoint_format.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_quantization.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_symbolic_physical.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_weight_align.cpp
        ${CORE_DIR}/vendor/astc-encoder/astcenc_weight_quant_xfer_tables.cpp
        PROPERTIES COMPILE_OPTIONS "-w;-fsigned-char")

# fixtures PNG p/ test_assets (caminho absoluto injetado — ctest roda no
# diretório de build, o caminho relativo não serviria) + fonte real p/ os
# testes de UI (Liberation Sans, OFL — só CI, não vai no APK)
target_compile_definitions(test_core PRIVATE
        FIXTURE_DIR="${CMAKE_CURRENT_LIST_DIR}/fixtures"
        FONT_FIXTURE="${CMAKE_CURRENT_LIST_DIR}/fixtures/fonts/LiberationSans-Regular.ttf")

enable_testing()
add_test(NAME core COMMAND test_core)
