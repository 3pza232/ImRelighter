cmake_minimum_required(VERSION 3.26)
project(ImRelighter VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(EXTERN_DIR "${CMAKE_SOURCE_DIR}/extern")

# ---------------------------------------------------------------------------
# Third-party dependencies
# ---------------------------------------------------------------------------

add_library(glad STATIC "${EXTERN_DIR}/glad/src/glad.c")
target_include_directories(glad PUBLIC "${EXTERN_DIR}/glad/include")

add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${EXTERN_DIR}/glfw/lib-mingw-w64/libglfw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${EXTERN_DIR}/glfw/include")

add_library(imgui STATIC
    "${EXTERN_DIR}/imgui/imgui.cpp"
    "${EXTERN_DIR}/imgui/imgui_draw.cpp"
    "${EXTERN_DIR}/imgui/imgui_tables.cpp"
    "${EXTERN_DIR}/imgui/imgui_widgets.cpp"
    "${EXTERN_DIR}/imgui/backends/imgui_impl_glfw.cpp"
    "${EXTERN_DIR}/imgui/backends/imgui_impl_opengl3.cpp"
    "${EXTERN_DIR}/imgui/misc/cpp/imgui_stdlib.cpp")
target_include_directories(imgui PUBLIC
    "${EXTERN_DIR}/imgui"
    "${EXTERN_DIR}/imgui/backends"
    "${EXTERN_DIR}/imgui/misc/cpp")
target_link_libraries(imgui PUBLIC glfw)

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE "${EXTERN_DIR}/stb")

add_library(json INTERFACE)
target_include_directories(json INTERFACE "${EXTERN_DIR}/json")

# ---------------------------------------------------------------------------
# Core library (no GLFW / ImGui / GL)
# ---------------------------------------------------------------------------

add_library(imrelighter_core STATIC
    src/core/i18n.cpp
    src/core/config.cpp
    src/io/paths.cpp
    src/io/native_dialog.cpp
    src/io/image_io.cpp
    src/io/webp_decoder.cpp
    src/io/png_export.cpp
    src/io/stb_impl.cpp
    src/io/stb_write_impl.cpp)
target_include_directories(imrelighter_core PUBLIC "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(imrelighter_core PUBLIC json stb)
target_compile_features(imrelighter_core PUBLIC cxx_std_20)

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

add_executable(ImRelighter WIN32
    src/main.cpp
    src/app/app.cpp
    src/gpu/texture.cpp
    src/gpu/shader.cpp
    src/gpu/renderer.cpp
    src/ml/onnx.cpp
    src/ml/model_metadata.cpp
    src/ml/preprocess.cpp
    src/ml/stb_resize_impl.cpp
    src/ml/inference.cpp
    src/ui/theme.cpp
    src/ui/widgets.cpp
    src/ui/ui.cpp)

target_include_directories(ImRelighter PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${EXTERN_DIR}/onnxruntime/include")
target_link_libraries(ImRelighter PRIVATE
    imrelighter_core
    glad
    imgui
    glfw
    opengl32
    gdi32
    user32
    shell32
    comdlg32
    ole32
    uuid)

target_compile_definitions(ImRelighter PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    IMRELIGHTER_VERSION="${PROJECT_VERSION}")

target_compile_options(ImRelighter PRIVATE
    -Wall -Wextra
    $<$<CONFIG:Debug>:-O0 -g3>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>)

target_link_options(ImRelighter PRIVATE
    -mwindows
    -static-libgcc
    -static-libstdc++)

if(WIN32)
    enable_language(RC)
    set(APP_ICON "${CMAKE_SOURCE_DIR}/assets/icons/ImRelight.ico")
    configure_file(
        "${CMAKE_SOURCE_DIR}/src/app/resources.rc.in"
        "${CMAKE_BINARY_DIR}/resources.rc"
        @ONLY)
    target_sources(ImRelighter PRIVATE "${CMAKE_BINARY_DIR}/resources.rc")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

include(CTest)
if(BUILD_TESTING)
    add_executable(imrelighter_tests
        tests/core_tests.cpp
        src/ml/preprocess.cpp
        src/ml/stb_resize_impl.cpp)
    target_include_directories(imrelighter_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${EXTERN_DIR}/stb")
    target_link_libraries(imrelighter_tests PRIVATE imrelighter_core)
    set_target_properties(imrelighter_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    target_compile_definitions(imrelighter_tests PRIVATE
        "IMRELIGHTER_I18N_DIR=\"${CMAKE_SOURCE_DIR}/assets/i18n\""
        "IMRELIGHTER_TEST_WEBP=\"${CMAKE_SOURCE_DIR}/tests/data/test.webp\"")
    add_custom_command(TARGET imrelighter_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${EXTERN_DIR}/libwebp/libwebp.dll"
                "$<TARGET_FILE_DIR:imrelighter_tests>/libwebp.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${EXTERN_DIR}/libwebp/libsharpyuv.dll"
                "$<TARGET_FILE_DIR:imrelighter_tests>/libsharpyuv.dll")
    add_test(NAME imrelighter_core COMMAND imrelighter_tests)

    add_executable(imrelighter_gpu_tests
        tests/gpu_tests.cpp
        src/gpu/renderer.cpp
        src/gpu/shader.cpp
        src/gpu/texture.cpp)
    target_include_directories(imrelighter_gpu_tests PRIVATE
        "${CMAKE_SOURCE_DIR}/src")
    target_link_libraries(imrelighter_gpu_tests PRIVATE
        imrelighter_core glad glfw opengl32 gdi32 user32)
    target_compile_definitions(imrelighter_gpu_tests PRIVATE
        "IMRELIGHTER_SHADER_DIR=\"${CMAKE_SOURCE_DIR}/assets/shaders\""
        "IMRELIGHTER_GOLDEN_DIR=\"${CMAKE_SOURCE_DIR}/tests/golden\"")
    set_target_properties(imrelighter_gpu_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_test(NAME imrelighter_gpu COMMAND imrelighter_gpu_tests)

    add_executable(imrelighter_inference_bench
        tests/inference_bench.cpp
        src/ml/onnx.cpp
        src/ml/preprocess.cpp
        src/ml/stb_resize_impl.cpp)
    target_include_directories(imrelighter_inference_bench PRIVATE
        "${CMAKE_SOURCE_DIR}/src"
        "${EXTERN_DIR}/stb"
        "${EXTERN_DIR}/onnxruntime/include")
    target_compile_definitions(imrelighter_inference_bench PRIVATE
        "IMRELIGHTER_MODEL_PATH=\"${CMAKE_SOURCE_DIR}/models/depth-anything-v2-small/model.onnx\""
        "IMRELIGHTER_ORT_DLL=\"${CMAKE_SOURCE_DIR}/extern/onnxruntime/lib/onnxruntime.dll\"")
    set_target_properties(imrelighter_inference_bench PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")
    add_custom_command(TARGET imrelighter_inference_bench POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${EXTERN_DIR}/onnxruntime/lib/onnxruntime.dll"
                "$<TARGET_FILE_DIR:imrelighter_inference_bench>/onnxruntime.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${EXTERN_DIR}/directml/DirectML.dll"
                "$<TARGET_FILE_DIR:imrelighter_inference_bench>/DirectML.dll")
endif()

# ---------------------------------------------------------------------------
# Runtime assets
# ---------------------------------------------------------------------------

add_custom_command(TARGET ImRelighter POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:ImRelighter>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:ImRelighter>/models/depth-anything-v2-small"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/models/depth-anything-v2-small/model.onnx"
            "$<TARGET_FILE_DIR:ImRelighter>/models/depth-anything-v2-small/model.onnx"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/models/depth-anything-v2-small/manifest.json"
            "$<TARGET_FILE_DIR:ImRelighter>/models/depth-anything-v2-small/manifest.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/models/depth-anything-v2-small/LICENSE"
            "$<TARGET_FILE_DIR:ImRelighter>/models/depth-anything-v2-small/LICENSE"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/onnxruntime/lib/onnxruntime.dll"
            "$<TARGET_FILE_DIR:ImRelighter>/onnxruntime.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/onnxruntime/lib/onnxruntime_providers_shared.dll"
            "$<TARGET_FILE_DIR:ImRelighter>/onnxruntime_providers_shared.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/directml/DirectML.dll"
            "$<TARGET_FILE_DIR:ImRelighter>/DirectML.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/libwebp/libwebp.dll"
            "$<TARGET_FILE_DIR:ImRelighter>/libwebp.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/libwebp/libsharpyuv.dll"
            "$<TARGET_FILE_DIR:ImRelighter>/libsharpyuv.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
            "$<TARGET_FILE_DIR:ImRelighter>/THIRD_PARTY_NOTICES.md"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/onnxruntime/LICENSE"
            "$<TARGET_FILE_DIR:ImRelighter>/LICENSE-ONNXRUNTIME.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/directml/LICENSE.txt"
            "$<TARGET_FILE_DIR:ImRelighter>/LICENSE-DIRECTML.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/directml/LICENSE-CODE.txt"
            "$<TARGET_FILE_DIR:ImRelighter>/LICENSE-DIRECTML-CODE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/libwebp/COPYING"
            "$<TARGET_FILE_DIR:ImRelighter>/LICENSE-LIBWEBP.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${EXTERN_DIR}/libwebp/PATENTS"
            "$<TARGET_FILE_DIR:ImRelighter>/PATENTS-LIBWEBP.txt"
    COMMENT "Copying assets, model and runtime DLLs")
