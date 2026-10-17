#include "app/app.h"

#include <windows.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <cstdio>
#include <fstream>
#include <chrono>
#include <future>
#include <sstream>
#include <thread>

#include <shellapi.h>

#include "io/image_io.h"
#include "io/native_dialog.h"
#include "io/paths.h"
#include "io/png_export.h"
#include "ml/model_metadata.h"
#include "nlohmann/json.hpp"
#include "ui/theme.h"
#include "ui/ui.h"

namespace app {

namespace {

constexpr size_t kMaxDocuments = 12;
constexpr uint64_t kMemoryBudget = 2ull * 1024ull * 1024ull * 1024ull;

void glfwErrorCallback(int code, const char* description) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "GLFW error %d: %s\n", code,
                  description ? description : "");
    OutputDebugStringA(buffer);
}

#ifndef NDEBUG
void APIENTRY glDebugCallback(GLenum, GLenum type, GLuint, GLenum severity,
                              GLsizei, const GLchar* message, const void*) {
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    char buffer[2048];
    std::snprintf(buffer, sizeof(buffer), "GL debug (0x%x): %s\n", type, message);
    OutputDebugStringA(buffer);
}
#endif

std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                                           static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring result(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                        result.data(), needed);
    return result;
}

std::filesystem::path resolveModelFile(const std::filesystem::path& directory) {
    const std::filesystem::path candidates[] = {
        directory / L"model.onnx",
        directory / L"depth-anything-v2-small" / L"model.onnx",
        directory / L"onnx-community" / L"depth-anything-v2-small" / L"onnx" /
            L"model.onnx",
    };
    std::error_code ec;
    for (const auto& candidate : candidates) {
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
    }
    return {};
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

} // namespace

App::~App() {
    shutdown();
}

int App::run() {
    if (!initializeWindow()) return 1;

    loadConfigAndLanguage();
    if (!initializeImGui()) return 1;
    loadFonts();
    theme::setMode(config.theme == core::Theme::Light ? theme::Mode::Light
                                                     : theme::Mode::Dark);
    renderer_ = std::make_unique<gpu::Renderer>();
    std::string rendererError;
    if (!renderer_->initialize(paths::assetPath("shaders"), &rendererError)) {
        ui.statusOverride = rendererError;
        renderer_.reset();
    }
    inference_.start(paths::exeDir() / L"onnxruntime.dll");
    loadIcon();
    openInitialFilesFromCommandLine();

    centerWindow();
    glfwShowWindow(window_);
    glfwSwapInterval(1);

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();
        drawFrame();
    }

    shutdown();
    glfwTerminate();
    glfwReady_ = false;
    return 0;
}

bool App::initializeWindow() {
    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit()) {
        showError(L"Failed to initialize GLFW.");
        return false;
    }
    glfwReady_ = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    int width = 1440;
    int height = 900;
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        int mx = 0;
        int my = 0;
        int mw = 0;
        int mh = 0;
        glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
        (void)mx;
        (void)my;
        if (mw > 0 && mh > 0) {
            width = std::min(width, mw - 80);
            height = std::min(height, mh - 80);
        }
    }
    width = std::max(width, 1100);
    height = std::max(height, 700);

    window_ = glfwCreateWindow(width, height, "ImRelighter", nullptr, nullptr);
    if (!window_) {
        showError(L"Failed to create an OpenGL 4.5 window.\n"
                  L"Please update your graphics driver.");
        glfwTerminate();
        glfwReady_ = false;
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSetWindowSizeLimits(window_, 1100, 700, GLFW_DONT_CARE, GLFW_DONT_CARE);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        showError(L"Failed to load OpenGL functions (GLAD).");
        return false;
    }

    if (!GLAD_GL_VERSION_4_5) {
        showError(L"ImRelighter requires OpenGL 4.5 Core.\n"
                  L"Your graphics driver does not expose it.");
        return false;
    }

    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    glVersion_ = version ? version : "unknown";
    glRenderer_ = renderer ? renderer : "unknown";

#ifndef NDEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugCallback, nullptr);
#endif

    return true;
}

void App::loadConfigAndLanguage() {
    if (!i18n.load(paths::assetDir() / L"i18n")) {
        // Keep running with keys as a last resort; the UI will still be usable.
        OutputDebugStringA("ImRelighter: language files are missing or invalid.\n");
    }

    core::UserConfig loaded;
    std::string error;
    if (!core::loadUserConfig(loaded, &error)) {
        OutputDebugStringA((error + "\n").c_str());
    }
    if (!loaded.languageSet) {
        loaded.language = core::I18n::systemDefault();
    }
    config = loaded;
    i18n.setLanguage(config.language);
}

bool App::initializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true)) {
        showError(L"Failed to initialize ImGui (GLFW backend).");
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 450")) {
        showError(L"Failed to initialize ImGui (OpenGL backend).");
        return false;
    }

    imguiReady_ = true;
    return true;
}

void App::loadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    const auto fontPath = paths::assetPath("fonts/SourceHanSansCN-Normal.otf");

    ImFontConfig fontConfig;
    fontConfig.OversampleH = 2;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = true;

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    glfwGetWindowContentScale(window_, &scaleX, &scaleY);
    const float scale = std::max(1.0f, (scaleX + scaleY) * 0.5f);

    ImFont* font = io.Fonts->AddFontFromFileTTF(
        paths::toUtf8(fontPath).c_str(),
        15.0f * scale,
        &fontConfig,
        io.Fonts->GetGlyphRangesChineseFull());

    if (!font) {
        OutputDebugStringA("ImRelighter: failed to load Source Han Sans; using default font.\n");
        io.Fonts->AddFontDefault();
    }
}

void App::loadIcon() {
    const auto iconPath = paths::assetPath("icons/ImRelight.png");
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load(paths::toUtf8(iconPath).c_str(), &width, &height,
                                &channels, 4);
    if (!pixels) return;

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = pixels;
    glfwSetWindowIcon(window_, 1, &image);
    stbi_image_free(pixels);
}

void App::pollExport() {
    if (!exportFuture_.valid()) return;
    if (exportFuture_.wait_for(std::chrono::seconds(0)) !=
        std::future_status::ready) {
        return;
    }
    std::string error = exportFuture_.get();
    ui.statusOverride = error.empty() ? tr("export.done") : error;
}

void App::drawFrame() {
    pollExport();
    const double now = glfwGetTime();
    if (lastFrameTime_ == 0.0) lastFrameTime_ = now;
    const double delta = std::min(0.05, std::max(0.0, now - lastFrameTime_));
    lastFrameTime_ = now;
    if (ui.animate && hasTask()) {
        for (core::Light& light : ui.lights) {
            const float radius = std::max(
                0.6f, std::sqrt(light.position[0] * light.position[0] +
                                light.position[1] * light.position[1]));
            const float angle = std::atan2(light.position[1], light.position[0]) +
                                static_cast<float>(delta) * 0.45f;
            light.position[0] = std::cos(angle) * radius;
            light.position[1] = std::sin(angle) * radius;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ml::InferenceResult inferenceResult;
    while (inference_.poll(inferenceResult)) {
        core::Document* target = nullptr;
        for (const auto& document : documents_) {
            if (document->id == inferenceResult.documentId) {
                target = document.get();
                break;
            }
        }
        if (!target || target->depthGeneration != inferenceResult.generation) {
            continue;
        }
        if (inferenceResult.cancelled) {
            ui.statusOverride = tr("depth.cancelled");
            continue;
        }
        if (!inferenceResult.error.empty()) {
            ui.statusOverride = inferenceResult.error;
            continue;
        }
        target->depth = std::move(inferenceResult.depth);
        target->hasDepth = true;
        ++target->depthRevision;
        if (target == activeDocument()) {
            ui.view = core::View::Lit;
            ui.statusOverride = tr("depth.ready");
        }
    }

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) requestOpenImage();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
            ui.exportPopupRequested = true;
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
            generateDepth();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Comma, false)) {
            ui.settingsOpen = !ui.settingsOpen;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_F, false) && hasTask()) {
            ui.fitPending = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            if (ui.settingsOpen) ui.settingsOpen = false;
        }
    }

    ui::draw(*this);
    saveUiToActive();

    if (hasTask()) {
        const core::Document* doc = activeDocument();
        if (doc && doc->hasDepth && renderer_) {
            std::string renderError;
            if (!renderer_->renderPreview(*doc, &renderError) &&
                !renderError.empty()) {
                ui.statusOverride = renderError;
            }
        }
    }

    ImGui::Render();

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    glViewport(0, 0, width, height);
    glClearColor(theme::bg.x, theme::bg.y, theme::bg.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

bool App::hasTask() const {
    return activeIndex_ >= 0 &&
           activeIndex_ < static_cast<int>(documents_.size());
}

const core::Document* App::document(size_t index) const {
    if (index >= documents_.size()) return nullptr;
    return documents_[index].get();
}

const core::Document* App::activeDocument() const {
    return hasTask() ? documents_[activeIndex_].get() : nullptr;
}

core::Document* App::activeDocument() {
    return hasTask() ? documents_[activeIndex_].get() : nullptr;
}

gpu::Texture* App::thumbnail(uint64_t id) {
    const auto it = thumbnails_.find(id);
    return it == thumbnails_.end() ? nullptr : it->second.get();
}

void App::requestOpenImage() {
    const auto paths =
        native_dialog::openImagesDialog(glfwGetWin32Window(window_),
                                      widen(tr("topbar.open")).c_str());
    if (paths.empty()) return;

    std::string error;
    uint64_t lastOpened = 0;
    for (const auto& path : paths) {
        const uint64_t id = addDocument(path, &error);
        if (id != 0) lastOpened = id;
    }

    if (lastOpened != 0) {
        ui.statusOverride.clear();
        activateDocument(lastOpened);
    } else if (!error.empty()) {
        ui.statusOverride = error;
    }
}

uint64_t App::addDocument(const std::filesystem::path& path, std::string* error) {
    if (documents_.size() >= kMaxDocuments) {
        if (error) *error = tr("document.limit");
        return 0;
    }

    auto image = io::loadImage(path, error);
    if (!image) return 0;

    uint64_t used = 0;
    for (const auto& document : documents_) {
        used += document->color.size() + document->depth.capacity() * sizeof(float) +
                document->thumbnail.size();
    }
    const uint64_t incoming =
        image->rgba.size() +
        static_cast<uint64_t>(image->width) * image->height * sizeof(float) +
        128ull * 1024ull;
    if (used + incoming > kMemoryBudget) {
        if (error) *error = tr("document.memory");
        return 0;
    }

    auto doc = std::make_unique<core::Document>();
    doc->id = nextDocumentId_++;
    doc->path = path;
    doc->name = paths::fileNameUtf8(path);
    doc->width = image->width;
    doc->height = image->height;

    if (!io::makeThumbnail(*image, 160, doc->thumbnail, doc->thumbWidth,
                           doc->thumbHeight)) {
        if (error) *error = "Failed to create thumbnail.";
        return 0;
    }

    doc->color = std::move(image->rgba);
    doc->settings = core::defaults();
    doc->lights = {core::makeLight(1)};
    doc->selectedLight = 1;
    doc->view = core::View::Original;
    doc->fitPending = true;

    const uint64_t id = doc->id;
    auto texture = std::make_unique<gpu::Texture>();
    texture->upload(doc->thumbnail.data(), doc->thumbWidth, doc->thumbHeight);
    thumbnails_[id] = std::move(texture);

    documents_.push_back(std::move(doc));
    return id;
}

void App::activateDocument(uint64_t id) {
    const auto it = std::find_if(documents_.begin(), documents_.end(),
                                 [&](const std::unique_ptr<core::Document>& doc) {
                                     return doc->id == id;
                                 });
    if (it == documents_.end()) return;

    const int index = static_cast<int>(std::distance(documents_.begin(), it));
    if (index == activeIndex_) return;

    saveUiToActive();
    activeIndex_ = index;
    ui.statusOverride.clear();
    loadUiFromActive();
    uploadActiveTexture();
}

void App::closeDocument(uint64_t id) {
    const auto it = std::find_if(documents_.begin(), documents_.end(),
                                 [&](const std::unique_ptr<core::Document>& doc) {
                                     return doc->id == id;
                                 });
    if (it == documents_.end()) return;

    const int index = static_cast<int>(std::distance(documents_.begin(), it));
    const bool wasActive = index == activeIndex_;
    saveUiToActive();

    thumbnails_.erase(id);
    documents_.erase(it);

    if (documents_.empty()) {
        activeIndex_ = -1;
        UiState fresh;
        fresh.sidebarOpen = ui.sidebarOpen;
        fresh.settingsOpen = ui.settingsOpen;
        fresh.exportPopupRequested = ui.exportPopupRequested;
        ui = std::move(fresh);
        activeTexture_.destroy();
        return;
    }

    if (index < activeIndex_) {
        --activeIndex_;
    }

    if (wasActive) {
        activeIndex_ = std::min(index, static_cast<int>(documents_.size()) - 1);
        loadUiFromActive();
        uploadActiveTexture();
    }
}

void App::loadUiFromActive() {
    const core::Document* doc = activeDocument();
    if (!doc) {
        UiState fresh;
        fresh.sidebarOpen = ui.sidebarOpen;
        fresh.settingsOpen = ui.settingsOpen;
        ui = std::move(fresh);
        return;
    }

    ui.settings = doc->settings;
    ui.lights = doc->lights;
    ui.selectedLight = doc->selectedLight;
    ui.animate = doc->animate;
    ui.view = doc->view;
    ui.zoom = doc->zoom;
    ui.panX = doc->panX;
    ui.panY = doc->panY;
    ui.fitPending = doc->fitPending;
}

void App::saveUiToActive() {
    core::Document* doc = activeDocument();
    if (!doc) return;

    doc->settings = ui.settings;
    doc->lights = ui.lights;
    doc->selectedLight = ui.selectedLight;
    doc->animate = ui.animate;
    doc->view = ui.view;
    doc->zoom = ui.zoom;
    doc->panX = ui.panX;
    doc->panY = ui.panY;
    doc->fitPending = ui.fitPending;
}

unsigned int App::displayTextureId() const {
    const core::Document* doc = activeDocument();
    if (doc && doc->hasDepth && ui.view != core::View::Original && renderer_ &&
        renderer_->ready() && renderer_->previewTexture() != 0) {
        return renderer_->previewTexture();
    }
    return activeTexture_.id();
}

bool App::displayTextureFlipped() const {
    const core::Document* doc = activeDocument();
    return doc && doc->hasDepth && ui.view != core::View::Original && renderer_ &&
           renderer_->ready() && renderer_->previewTexture() != 0;
}

void App::exportImage(core::ExportKind kind) {
    core::Document* doc = activeDocument();
    if (!doc || !doc->hasDepth) {
        ui.statusOverride = tr("depth.not_generated");
        return;
    }

    std::wstring suffix;
    core::View view = core::View::Lit;
    switch (kind) {
        case core::ExportKind::Lit: suffix = L"_relight.png"; break;
        case core::ExportKind::Normal: suffix = L"_normal.png"; view = core::View::Normal; break;
        case core::ExportKind::Depth: suffix = L"_depth16.png"; break;
        case core::ExportKind::Lighting: suffix = L"_lighting.png"; view = core::View::Lighting; break;
        case core::ExportKind::Shadow: suffix = L"_shadow.png"; view = core::View::Shadow; break;
    }

    if (exportFuture_.valid()) {
        ui.statusOverride = tr("export.running");
        return;
    }

    std::wstring defaultName = doc->path.stem().wstring() + suffix;
    const auto destination = native_dialog::savePngDialog(
        glfwGetWin32Window(window_), widen(tr("topbar.export")).c_str(),
        defaultName.c_str());
    if (!destination) return;

    if (kind == core::ExportKind::Depth) {
        std::vector<float> depth = doc->depth;
        const int width = doc->width;
        const int height = doc->height;
        const bool invert = doc->settings.invert;
        const auto path = *destination;
        exportFuture_ = std::async(
            std::launch::async,
            [path, depth = std::move(depth), width, height, invert]() {
                std::string error;
                const bool ok = io::writeDepth16Png(path, depth, width, height,
                                                    invert, &error);
                return ok ? std::string() : error;
            });
        ui.statusOverride = tr("export.running");
        return;
    }

    if (!renderer_) {
        ui.statusOverride = "Renderer is not available.";
        return;
    }

    std::vector<uint8_t> rgba;
    std::string error;
    if (!renderer_->renderExport(*doc, view, rgba, &error)) {
        ui.statusOverride = error;
        return;
    }

    const int width = doc->width;
    const int height = doc->height;
    const auto path = *destination;
    exportFuture_ = std::async(
        std::launch::async, [path, rgba = std::move(rgba), width, height]() {
            std::string encodeError;
            const bool ok =
                io::writeRgba8Png(path, rgba, width, height, &encodeError);
            return ok ? std::string() : encodeError;
        });
    ui.statusOverride = tr("export.running");
}
void App::cancelInference() {
    if (!inference_.busy()) return;
    inference_.cancel();
    ui.statusOverride = tr("depth.cancelling");
}

void App::generateDepth() {
    core::Document* doc = activeDocument();
    if (!doc) return;
    if (inference_.busy()) {
        ui.statusOverride = tr("depth.running");
        return;
    }
    if (!modelExists()) {
        ui.statusOverride = tr("model.missing");
        return;
    }

    ++doc->depthGeneration;
    ml::InferenceJob job;
    job.documentId = doc->id;
    job.generation = doc->depthGeneration;
    job.width = doc->width;
    job.height = doc->height;
    job.rgba = doc->color;
    job.modelPath = modelPath();
    job.useDml = config.backend == core::Backend::Dml ||
                 (config.backend == core::Backend::Auto && directmlAvailable());
    job.allowCpuFallback = config.backend == core::Backend::Auto;
    if (!inference_.submit(std::move(job))) {
        ui.statusOverride = tr("depth.running");
        return;
    }
    ui.statusOverride = tr("depth.running");
}

void App::uploadActiveTexture() {
    const core::Document* doc = activeDocument();
    if (!doc || doc->color.empty()) {
        activeTexture_.destroy();
        return;
    }
    activeTexture_.upload(doc->color.data(), doc->width, doc->height);
}

void App::releaseGpuResources() {
    activeTexture_.destroy();
    thumbnails_.clear();
}

void App::setLanguage(core::Language language) {
    config.language = language;
    config.languageSet = true;
    i18n.setLanguage(language);
    saveConfig();
}

void App::setTheme(core::Theme value) {
    config.theme = value;
    theme::setMode(value == core::Theme::Light ? theme::Mode::Light
                                               : theme::Mode::Dark);
    saveConfig();
}

void App::setBackend(core::Backend backend) {
    if (backend == core::Backend::Dml && !directmlAvailable()) {
        backend = core::Backend::Cpu;
    }
    config.backend = backend;
    saveConfig();
}

void App::resetTask() {
    ui.settings = core::defaults();
    ui.lights = {core::makeLight(1)};
    ui.selectedLight = 1;
    ui.animate = false;
    const core::Document* doc = activeDocument();
    ui.view = (doc && doc->hasDepth) ? core::View::Lit : core::View::Original;
}

void App::openInitialFilesFromCommandLine() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;

    std::string error;
    uint64_t lastOpened = 0;
    for (int i = 1; i < argc; ++i) {
        const std::filesystem::path path(argv[i]);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) continue;
        const uint64_t id = addDocument(path, &error);
        if (id != 0) lastOpened = id;
    }
    LocalFree(argv);

    if (lastOpened != 0) {
        activateDocument(lastOpened);
    } else if (!error.empty()) {
        ui.statusOverride = error;
    }
}

void App::saveConfig() {
    std::string error;
    if (!core::saveUserConfig(config, &error)) {
        OutputDebugStringA((error + "\n").c_str());
    }
}

bool App::modelExists() const {
    return paths::fileExists(modelPath());
}

bool App::directmlAvailable() const {
    if (dmlChecked_) return dmlAvailable_;
    dmlChecked_ = true;
    if (!paths::fileExists(paths::exeDir() / L"DirectML.dll")) return false;
    ml::OnnxRuntime runtime;
    std::string error;
    if (!runtime.load(paths::exeDir() / L"onnxruntime.dll", &error)) return false;
    dmlAvailable_ = runtime.hasDml();
    return dmlAvailable_;
}

std::uintmax_t App::modelSize() const {
    return paths::fileSize(modelPath());
}

std::string App::modelSha() const {
    if (modelPath().empty()) return {};
    auto manifest = modelPath().parent_path() / L"manifest.json";
    if (!paths::fileExists(manifest)) {
        if (!config.modelFile.empty() || !config.modelDir.empty()) return {};
        manifest = paths::modelManifest();
    }
    const std::string text = readFile(manifest);
    if (text.empty()) return {};
    try {
        const auto json = nlohmann::json::parse(text);
        if (json.contains("file") && json["file"].contains("sha256") &&
            json["file"]["sha256"].is_string()) {
            return json["file"]["sha256"].get<std::string>();
        }
    } catch (const std::exception&) {
    }
    return {};
}

std::filesystem::path App::modelPath() const {
    if (config.modelSelectionCleared) return {};
    if (!config.modelFile.empty()) return paths::fromUtf8(config.modelFile);
    if (!config.modelDir.empty()) {
        const auto selected = paths::fromUtf8(config.modelDir);
        const auto resolved = resolveModelFile(selected);
        if (!resolved.empty()) return resolved;
        return selected / L"model.onnx";
    }
    return paths::modelFile();
}

std::filesystem::path App::modelFolder() const {
    const auto path = modelPath();
    return path.empty() ? paths::modelDir() : path.parent_path();
}

void App::chooseModelFile() {
    const auto file = native_dialog::pickOnnxFile(
        glfwGetWin32Window(window_), widen(tr("settings.choose_model_file")).c_str());
    if (!file) return;
    std::wstring extension = file->extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    if (!paths::fileExists(*file) || extension != L".onnx") {
        ui.statusOverride = tr("model.invalid_file");
        return;
    }

    config.modelFile = paths::toUtf8(*file);
    config.modelDir.clear();
    config.modelSelectionCleared = false;
    modelInfoPath_.clear();
    modelInfoAttempted_ = false;
    ui.statusOverride.clear();
    saveConfig();
}

void App::inspectModelInfo() {
    const auto path = modelPath();
    if (path.empty()) {
        modelInfo_ = {};
        modelInfoError_.clear();
        modelInfoPath_.clear();
        modelInfoAttempted_ = true;
        return;
    }
    if (modelInfoAttempted_ && modelInfoPath_ == path) return;
    modelInfoPath_ = path;
    modelInfoAttempted_ = true;
    modelInfo_ = {};
    modelInfoError_.clear();

    ml::OnnxRuntime runtime;
    std::string error;
    if (!runtime.load(paths::exeDir() / L"onnxruntime.dll", &error)) {
        modelInfoError_ = error;
        return;
    }
    ml::OnnxSession session;
    if (!session.load(runtime, path, false, &error)) {
        modelInfoError_ = error;
        return;
    }
    modelInfo_ = session.info();
    std::string weightError;
    if (!inspectModelWeights(path, modelWeights_, &weightError)) {
        modelWeights_ = {};
    }
}

std::string App::modelType() const {
    if (modelWeights_.fp16 > modelWeights_.fp32) return "fp16";
    if (modelWeights_.fp32 > 0) return "fp32";
    if (modelInfo_.inputType == "FLOAT16") return "fp16";
    return "fp32";
}

void App::clearModelSelection() {
    config.modelFile.clear();
    config.modelDir.clear();
    config.modelSelectionCleared = true;
    modelInfo_ = {};
    modelWeights_ = {};
    modelInfoPath_.clear();
    modelInfoError_.clear();
    modelInfoAttempted_ = false;
    ui.statusOverride.clear();
    saveConfig();
}

std::string App::glVersion() const {
    return glVersion_;
}

std::string App::glRenderer() const {
    return glRenderer_;
}

std::string App::tr(std::string_view key) const {
    return i18n.tr(key);
}

std::string App::statusText() const {
    if (!ui.statusOverride.empty()) return ui.statusOverride;
    const core::Document* doc = activeDocument();
    if (!doc) return tr("status.no_task");
    return doc->name + "  ·  " + std::to_string(doc->width) + " × " +
           std::to_string(doc->height);
}

void App::centerWindow() {
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    if (!monitor) return;

    int mx = 0;
    int my = 0;
    int mw = 0;
    int mh = 0;
    glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
    int ww = 0;
    int wh = 0;
    glfwGetWindowSize(window_, &ww, &wh);
    const int x = std::max(mx, mx + (mw - ww) / 2);
    const int y = std::max(my, my + (mh - wh) / 2);
    glfwSetWindowPos(window_, x, y);
}

void App::shutdown() {
    inference_.stop();
    if (renderer_) {
        renderer_->shutdown();
        renderer_.reset();
    }
    releaseGpuResources();
    if (imguiReady_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        imguiReady_ = false;
    }
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
}

void App::showError(const std::wstring& message) const {
    MessageBoxW(nullptr, message.c_str(), L"ImRelighter", MB_OK | MB_ICONERROR);
}

} // namespace app
