# tests/CMakeLists.txt — suíte do core da G.One VV (CI Linux).
# Compila apenas o core (puro C++17, sem dependência de Android).
cmake_minimum_required(VERSION 3.16)
project(goni_vv_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR ${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp)

add_executable(test_core
        ../app/src/main/cpp/ui/ScriptEditor.cpp   # 0.9.2: editor de script
        ../app/src/main/cpp/ui/DocsScreen.cpp   # 0.9.2: Docs (Settings)
        ../app/src/main/cpp/ui/ViewportChrome.cpp   # (o main precisa)
        ../app/src/main/cpp/ui/BottomPanel.cpp
        ../app/src/main/cpp/ui/SettingsPage.cpp
        ../app/src/main/cpp/voni/VoniGrammar.cpp   # 0.9.2: V.ONI
        ../app/src/main/cpp/voni/VoniCompile.cpp
        ../app/src/main/cpp/voni/VoniVm.cpp
        ../app/src/main/cpp/voni/VoniHighlight.cpp
        ../app/src/main/cpp/voni/VoniDocs.cpp
        ../app/src/main/cpp/voni/VoniEngineHost.cpp
        ../app/src/main/cpp/voni/VoniRegistry.cpp   # 0.9.5: registo central
        ../app/src/main/cpp/voni/VoniTykers.cpp   # 0.9.5: runtime linkers/tykers
        ../app/src/main/cpp/core/VoniSystem.cpp
        test_main.cpp
        test_logs_crash.cpp
        test_storageperm.cpp
        test_handshake.cpp
        test_saf_project.cpp
        test_assetpersist.cpp
        test_bench.cpp   # 0.9.6 (G6 · R-017): benchmarks reais (puras)
        test_editor_buffer.cpp   # 0.9.6.2 (R-019): o buffer e o cursor do editor
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
        ${CORE_DIR}/core/Bench.cpp
        # F5: assets (importadores GL-free)
        ${CORE_DIR}/assets/ObjImporter.cpp
        # 0.8.10: formatos próprios (.gmesh/.gtext/.gm) + conversor streaming
        ${CORE_DIR}/assets/GOwnFormats.cpp
        ${CORE_DIR}/assets/AssetConverter.cpp
        # 0.8.10: archives — parser ZIP próprio + inflate zlib
        ${CORE_DIR}/assets/ZipExtract.cpp
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
        ${CORE_DIR}/render/ThumbPng.cpp   # 0.9.0: miniatura do projeto
        ${CORE_DIR}/ui/ViewportChrome.cpp   # 0.9.0: chrome do viewport
        ${CORE_DIR}/ui/BottomPanel.cpp   # 0.9.0: painel de baixo
        ${CORE_DIR}/ui/SettingsPage.cpp   # 0.9.0: página de Settings
        ${CORE_DIR}/ui/TextWindow.cpp   # 0.9.1: janela de texto pesado (IME)
        ${CORE_DIR}/core/UndoStack.cpp   # 0.9.0: undo/redo por snapshot
        ${CORE_DIR}/platform/ImeQueue.cpp   # 0.9.1: fila do IME + orientação
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
        ${CORE_DIR}/ui/LayoutDump.cpp   # 0.9.6.5 (GRUPO B): layout exportado + validador
        ${CORE_DIR}/ui/EditorUi.cpp
        # 0.7.0: UI criável (componente + runtime + editor dedicado)
        ${CORE_DIR}/components/UiCanvas.cpp
        ${CORE_DIR}/components/AnimationPlayer.cpp
        ${CORE_DIR}/components/SkeletonComp.cpp
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
        # 0.8.2: skinning (parse skins, esqueleto, pose altera vertices)
        test_skin.cpp
        # 0.8.3: blending (peso + crossfade)
        test_blend.cpp
        # 0.8.4: wiring de estabilização (resolver cap/sentinela, caps de
        # submissão, storm add/remove, play/stop ×20, steady-state GL,
        # header da timeline fluido)
        test_wiring084.cpp
        # 0.8.5: wiring de funcionalidade (addTrack no clip ativo, anim e2e
        # com opções e save/load, troca de primitiva ×3 no render, fallback
        # multi-mesh #0 no GpuAssets, browser com não suportados)
        test_wiring085.cpp
        # 0.8.6: wiring de UX (hex, tipografia no batch, gizmos de UI,
        # round-trip de rot/fscale/tstyle, zero sobreposição em paisagem)
        test_wiring086.cpp
        # 0.8.7: wiring do HOTFIX cirúrgico — IMPORT (botão abre o
        # navegador: as DUAS flags) + TROCA DE MESH (cache com cap/evicção,
        # negative-cache anti-retry-storm, stress anti-freeze com guarda de
        # tempo, geometria por primitiva, lifecycle destroy/recreate,
        # gesto órfão) — INCLUI platform/main.cpp: o caminho real do device
        test_wiring087.cpp
        # 0.8.9: wiring CRASH-PROOF (parte PURA — placeAt guard de ciclos,
        # Json::dumpTo teto, grelha adaptativa, far dinâmico, normalização
        # uniforme, campo numérico sem teto, auditoria de geradores com
        # extremos, Mesh AABB/NaN, cura de self-parent ao carregar). Os
        # casos do CAMINHO DO DEVICE estão no TU do test_wiring087 (único
        # que inclui platform/main.cpp) — secção 0.8.9 lá.
        test_wiring089.cpp
        test_wiring090.cpp   # 0.9.0: design system + editor polish (spec A-M)
        test_wiring091.cpp   # 0.9.1: orientação (portrait) + IME do sistema
        test_voni.cpp       # 0.9.2: linguagem V.ONI (spec §§2-12)
        test_wiring092.cpp  # 0.9.2: central VoniSystem + EngineHost + UI
        # 0.8.10: formatos próprios + 500 MB + archives + migração + identidade
        test_wiring010.cpp
        # 0.10-M (PASSO 1c): medições reais por fase (cópia/parse/fusão/
        # escrita) + RAM pico dos 3 perfis do dono — o relatório cita-as
        test_010m_medicoes.cpp
        # 0.8.11: ÁUDIO — .gi (ADPCM round-trip/WAV/OGG/MP3 erros),
        # misturador (vozes/loop/pitch/posicional/master), PROBE harness
        # (fake backends: falha→fallback, ok→AAudio), workspace (fake host:
        # import/gravar/preview/apagar/atribuir), serializer/preset/store,
        # FileApi kinds, toolbar exclusiva. Os casos do CAMINHO DO DEVICE
        # (import e2e, gravação sintética, boot do backend) estão no TU do
        # test_wiring087 (único que inclui platform/main.cpp) — secção lá.
        test_wiring011.cpp
        ${CORE_DIR}/ui/Icons.cpp
        ${CORE_DIR}/ui/Brand.cpp
        ${CORE_DIR}/ui/Strings.cpp
        # 0.7.7: câmara de cena (componente/invariante + gizmo do frustum)
        ${CORE_DIR}/core/CameraUtil.cpp
        ${CORE_DIR}/ui/CamGizmo.cpp
        ${CORE_DIR}/ui/FontAtlas.cpp
        ${CORE_DIR}/platform/InputState.cpp
        ${CORE_DIR}/platform/EngineLog.cpp
        ${CORE_DIR}/platform/FileApi.cpp
        ${CORE_DIR}/platform/CrashHandler.cpp
        ${CORE_DIR}/platform/BuildInfo.cpp
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
        ${CORE_DIR}/render/GpuAssets.cpp
        # 0.8.7: o CAMINHO REAL DO DEVICE na suíte — test_wiring087
        # #include platform/main.cpp (primMesh/attemptImport/rebind/
        # browserImportFile/applyImported/boot INIT_WINDOW/frame); estes
        # TUs deixa de ser device-only e passam a ser cobertos no hospedeiro
        ${CORE_DIR}/platform/EglContext.cpp
        ${CORE_DIR}/render/Grid.cpp
        # 0.8.11: ÁUDIO — o misturador/contentor .gi/codecs + o backend
        # COMUM (AudioOut.cpp: probe harness + currentMixFn; as fábricas
        # stub existem SÓ no host — o TU do device não compila aqui) + o
        # workspace de desenho puro
        ${CORE_DIR}/assets/GiFormat.cpp
        ${CORE_DIR}/assets/AudioCodecs.cpp
        ${CORE_DIR}/core/AudioEngine.cpp
        ${CORE_DIR}/platform/AudioOut.cpp
        # 0.9.3 (REG-002): o OboeBackend de PRODUÇÃO compila na suíte contra
        # o stub tests/stub/oboe/Oboe.h (tests/stub vem primeiro no include
        # path) — os sentinelas regress_audio_lifecycle afervam o portão,
        # os Result verificados e a degradação sem crash
        ${CORE_DIR}/platform/OboeBackend.cpp
        ${CORE_DIR}/ui/AudioWorkspace.cpp
        # 0.8.12 — SENTINELAS PERMANENTES DE REGRESSÃO (docs/REGRESSOES.md):
        # correm em TODAS as runs de CI, em todas as releases futuras. Os
        # nomes regress_* são o CONTRATO (remover/saltar exige aprovação
        # escrita do autor — ver docs/REGRESSOES.md).
        test_sentinels.cpp)


# 0.8.12 — o DISPOSITIVO VIRTUAL EM CI ("C33 virtual"): o executável que
# reproduz o telefone (1536x720 + insets, /tmp read-only, cache dir via
# JNI fake, content:// SAF, lifecycle EGL, taps replayáveis) e corre o
# REPLAY da sessão real do dono pelo frame() REAL do platform/main.cpp.
# O CI corre-o e grepa o output contra ci/forbidden_log_patterns.txt —
# qualquer padrão proibido = CI VERMELHO = release bloqueada.
add_executable(c33_virtual
        ${CORE_DIR}/core/Scene.cpp
        ${CORE_DIR}/core/Time.cpp
        ${CORE_DIR}/core/Bench.cpp
        ${CORE_DIR}/core/AnimationSystem.cpp
        ${CORE_DIR}/core/ComponentRegistry.cpp
        ${CORE_DIR}/core/ComponentStore.cpp
        ${CORE_DIR}/core/Presets.cpp
        ${CORE_DIR}/core/SceneSerializer.cpp
        ${CORE_DIR}/core/Project.cpp
        ${CORE_DIR}/core/ProjectStorage.cpp
        ${CORE_DIR}/core/AssetPersist.cpp
        ${CORE_DIR}/core/FsStorage.cpp
        ${CORE_DIR}/core/SafStorage.cpp
        ${CORE_DIR}/assets/ObjImporter.cpp
        ${CORE_DIR}/assets/GOwnFormats.cpp
        ${CORE_DIR}/assets/AssetConverter.cpp
        ${CORE_DIR}/assets/ZipExtract.cpp
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
        ${CORE_DIR}/render/Cube.cpp
        ${CORE_DIR}/render/Primitives.cpp
        ${CORE_DIR}/render/ThumbPng.cpp   # 0.9.0: miniatura do projeto (paridade com o test_core — o harness captura a thumb)
        ${CORE_DIR}/ui/ViewportChrome.cpp   # 0.9.0: chrome do viewport
        ${CORE_DIR}/ui/BottomPanel.cpp   # 0.9.0: painel de baixo
        ${CORE_DIR}/ui/SettingsPage.cpp   # 0.9.0: página de Settings
        ${CORE_DIR}/core/UndoStack.cpp   # 0.9.0: undo/redo por snapshot
        ${CORE_DIR}/ui/ScriptEditor.cpp   # 0.9.2: editor de script V.ONI
        ${CORE_DIR}/ui/DocsScreen.cpp   # 0.9.2: Settings → Docs
        ${CORE_DIR}/voni/VoniGrammar.cpp   # 0.9.2: linguagem
        ${CORE_DIR}/voni/VoniCompile.cpp
        ${CORE_DIR}/voni/VoniVm.cpp
        ${CORE_DIR}/voni/VoniHighlight.cpp
        ${CORE_DIR}/voni/VoniDocs.cpp
        ${CORE_DIR}/voni/VoniEngineHost.cpp
        ${CORE_DIR}/voni/VoniRegistry.cpp   # 0.9.5: registo central
        ${CORE_DIR}/voni/VoniTykers.cpp   # 0.9.5: runtime linkers/tykers
        ${CORE_DIR}/core/VoniSystem.cpp   # a "central"
        ${CORE_DIR}/render/Camera.cpp
        ${CORE_DIR}/physics/Shapes.cpp
        ${CORE_DIR}/physics/PhysicsSystem.cpp
        ${CORE_DIR}/components/TouchControls.cpp
        ${CORE_DIR}/ui/UiContext.cpp
        ${CORE_DIR}/ui/LayoutDump.cpp   # 0.9.6.5 (GRUPO B): layout exportado + validador
        ${CORE_DIR}/ui/EditorUi.cpp
        ${CORE_DIR}/components/UiCanvas.cpp
        ${CORE_DIR}/components/AnimationPlayer.cpp
        ${CORE_DIR}/components/SkeletonComp.cpp
        ${CORE_DIR}/ui/UiRuntime.cpp
        ${CORE_DIR}/ui/UiEditor.cpp
        ${CORE_DIR}/ui/SceneFx.cpp
        ${CORE_DIR}/ui/Gizmo.cpp
        ${CORE_DIR}/ui/Toolbar.cpp
        ${CORE_DIR}/ui/Timeline.cpp
        ${CORE_DIR}/ui/Icons.cpp
        ${CORE_DIR}/ui/Brand.cpp
        ${CORE_DIR}/ui/Strings.cpp
        ${CORE_DIR}/core/CameraUtil.cpp
        ${CORE_DIR}/ui/CamGizmo.cpp
        ${CORE_DIR}/ui/FontAtlas.cpp
        ${CORE_DIR}/platform/InputState.cpp
        ${CORE_DIR}/platform/EngineLog.cpp
        ${CORE_DIR}/platform/FileApi.cpp
        ${CORE_DIR}/platform/CrashHandler.cpp
        ${CORE_DIR}/platform/BuildInfo.cpp
        ${CORE_DIR}/platform/StorageBridge.cpp
        ${CORE_DIR}/render/Renderer.cpp
        ${CORE_DIR}/render/Mesh.cpp
        ${CORE_DIR}/render/Material.cpp
        ${CORE_DIR}/render/Texture.cpp
        ${CORE_DIR}/render/GpuAssets.cpp
        ${CORE_DIR}/platform/EglContext.cpp
        ${CORE_DIR}/render/Grid.cpp
        ${CORE_DIR}/assets/GiFormat.cpp
        ${CORE_DIR}/assets/AudioCodecs.cpp
        ${CORE_DIR}/core/AudioEngine.cpp
        ${CORE_DIR}/platform/AudioOut.cpp
        # 0.9.3 (REG-002): o OboeBackend REAL no dispositivo virtual (o
        # replay do lifecycle FASE 8 usa o caminho de produção contra o
        # stub do oboe)
        ${CORE_DIR}/platform/OboeBackend.cpp
        ${CORE_DIR}/ui/AudioWorkspace.cpp
        ${CORE_DIR}/ui/TextWindow.cpp   # 0.9.1: janela de texto (replay da rotação)
        ${CORE_DIR}/platform/ImeQueue.cpp   # 0.9.1: fila do IME + orientação
        c33_virtual.cpp)

target_include_directories(test_core PRIVATE
        # stubs PRIMEIRO: <GLES3/gl3.h> e <android/input.h> resolvem para os
        # no-ops do hospedeiro (a build Android usa o NDK, sem estes caminhos)
        ${CMAKE_CURRENT_LIST_DIR}/stub
        ${CORE_DIR}
        ${CORE_DIR}/vendor
        ${CORE_DIR}/vendor/etcpak
        ${CORE_DIR}/vendor/astc-encoder)
target_include_directories(c33_virtual PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/stub
        ${CORE_DIR}
        ${CORE_DIR}/vendor
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

set_source_files_properties(
        ${CORE_DIR}/assets/AudioCodecs.cpp
        PROPERTIES COMPILE_OPTIONS "-w")   # 0.8.11: vendors (stb_vorbis/minimp3) sem warnings

# 0.9.2: o TU que inclui o peglib (vendor) compila sem warnings do header
set_source_files_properties(
        ${CORE_DIR}/voni/VoniCompile.cpp
        PROPERTIES COMPILE_OPTIONS "-w")

# fixtures PNG p/ test_assets (caminho absoluto injetado — ctest roda no
# diretório de build, o caminho relativo não serviria) + fonte real p/ os
# testes de UI (Liberation Sans, OFL — só CI, não vai no APK)
target_compile_definitions(test_core PRIVATE
        FIXTURE_DIR="${CMAKE_CURRENT_LIST_DIR}/fixtures"
        FONT_FIXTURE="${CMAKE_CURRENT_LIST_DIR}/fixtures/fonts/LiberationSans-Regular.ttf"
        # 0.9.5 (R-013): a raiz do repo — o teste afere que os ficheiros da
        # referência pública commitados estão SINCRONIZADOS com o registo
        REPO_ROOT="${CMAKE_CURRENT_LIST_DIR}/..")

enable_testing()
add_test(NAME core COMMAND test_core)
# 0.8.12: o C33 virtual como TESTE (o CI corre-o em TODAS as runs; o output
# passo-a-passo vai para o log do ctest — o gate de padrões grepa o engine.log)
add_test(NAME c33_virtual_replay COMMAND c33_virtual)

# 0.9.5 · METADE 2 — O GERADOR DA REFERÊNCIA PÚBLICA: escreve no stdout
# a referência V.ONI COMPLETA gerada do REGISTO CENTRAL. Os ficheiros da
# raiz (VONI_referencia.md / llms-full.txt) são a SAÍDA dele; o teste R-013
# afere a sincronia byte a byte (ficheiro desatualizado = CI vermelho).
add_executable(voni_refgen
        ${CORE_DIR}/voni/VoniGrammar.cpp
        ${CORE_DIR}/voni/VoniCompile.cpp
        ${CORE_DIR}/voni/VoniVm.cpp
        ${CORE_DIR}/voni/VoniRegistry.cpp
        ${CORE_DIR}/voni/VoniTykers.cpp
        voni_refgen.cpp)
target_include_directories(voni_refgen PRIVATE
        ${CORE_DIR}
        ${CORE_DIR}/vendor)

# 0.8.10: zlib (inflate do ZIP)
target_link_libraries(test_core z)
target_link_libraries(c33_virtual z)

# fixtures (o harness carrega a fonte da fixture quando não há fonte de sistema)
target_compile_definitions(c33_virtual PRIVATE
        FIXTURE_DIR="${CMAKE_CURRENT_LIST_DIR}/fixtures"
        FONT_FIXTURE="${CMAKE_CURRENT_LIST_DIR}/fixtures/fonts/LiberationSans-Regular.ttf")
target_link_libraries(c33_virtual pthread)
