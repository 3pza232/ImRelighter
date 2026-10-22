cmake_minimum_required(VERSION 3.22)
project(imrelighter_android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(IMGUI_DIR "${ROOT_DIR}/third_party/imgui")
set(STB_DIR "${ROOT_DIR}/third_party/stb")
set(JSON_DIR "${ROOT_DIR}/third_party/json")
set(ORT_DIR "${ROOT_DIR}/third_party/onnxruntime-android-1.20.0")

add_library(imrelighter SHARED
    app/android_app.cpp
    core/model_catalog.cpp
    io/image_loader.cpp
    ml/model_config.cpp
    ml/model_metadata.cpp
    ml/preprocess.cpp
    ml/depth_processing.cpp
    ml/inference_service.cpp
    ui/theme.cpp
    ui/widgets.cpp
    platform/jni_bridge.cpp
    stb_impl.cpp
    stb_resize_impl.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imrelighter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/app
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/io
    ${CMAKE_CURRENT_SOURCE_DIR}/ml
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${STB_DIR}
    ${JSON_DIR}
    ${ORT_DIR}/headers
)

target_compile_definitions(imrelighter PRIVATE IMGUI_IMPL_OPENGL_ES3)
target_compile_options(imrelighter PRIVATE -fexceptions)
target_link_options(imrelighter PRIVATE
    "-Wl,-z,max-page-size=16384"
    "-Wl,-z,common-page-size=16384"
)
target_link_libraries(imrelighter
    android
    log
    jnigraphics
    EGL
    GLESv3
    ${ORT_DIR}/jni/${ANDROID_ABI}/libonnxruntime.so
)
