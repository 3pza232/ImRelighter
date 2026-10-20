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
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <fstream>
#include <future>

#include <shellapi.h>

#include "io/image_limits.h"
#include "io/image_io.h"
#include "io/native_dialog.h"
#include "io/paths.h"
#include "io/hash.h"
#include "io/png_export.h"
#include "ml/model_metadata.h"
#include "ui/theme.h"
#include "ui/ui.h"

namespace app {

namespace {


void glfwErrorCallback(int code, const char* description) {
    char buffer[1024];
    std::snprintf(buffer, sizeof(buffer), "GLFW error %d: %s\n", code,
                  description ? description : "");
    OutputDebugStringA(buffer);
}

void dropCallback(GLFWwindow* window, int count, const char** dropped) {
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;
    std::vector<std::filesystem::path> files;
    files.reserve(static_cast<std::size_t>(std::max(count, 0)));
    for (int i = 0; i < count; ++i) {
        if (dropped[i] && dropped[i][0] != '\0') {
            files.push_back(paths::fromUtf8(dropped[i]));
        }
    }
    if (!files.empty()) app->openPaths(files);
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

std::vector<unsigned char> readFileBytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return {};
    const std::streampos size = stream.tellg();
    if (size <= 0) return {};
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    if (!stream) return {};
    return bytes;
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
        setStatus(rendererError, 10.0);
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

    int width = 1500;
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

    glfwSetWindowUserPointer(window_, this);
    glfwSetDropCallback(window_, dropCallback);
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
        setStatus(error, 10.0);
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
    fontConfig.FontDataOwnedByAtlas = false;

    float scaleX = 1.0f;
    float scaleY = 1.0f;
    glfwGetWindowContentScale(window_, &scaleX, &scaleY);
    const float scale = std::max(1.0f, (scaleX + scaleY) * 0.5f);
    ui::setUiScale(scale);

    // Keep the scaled minimum inside the work area; otherwise Windows cannot
    // shrink the window and the bottom or right edge lands off-screen.
    int minWidth = static_cast<int>(1100.0f * scale);
    int minHeight = static_cast<int>(700.0f * scale);
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
        int mx = 0;
        int my = 0;
        int mw = 0;
        int mh = 0;
        glfwGetMonitorWorkarea(monitor, &mx, &my, &mw, &mh);
        (void)mx;
        (void)my;
        if (mw > 0 && mh > 0) {
            minWidth = std::min(minWidth, mw - 40);
            minHeight = std::min(minHeight, mh - 40);
            int width = 0;
            int height = 0;
            glfwGetWindowSize(window_, &width, &height);
            if (width < minWidth || height < minHeight) {
                glfwSetWindowSize(window_, std::max(width, minWidth),
                                  std::max(height, minHeight));
            }
        }
    }
    glfwSetWindowSizeLimits(window_, std::max(minWidth, 640),
                            std::max(minHeight, 480), GLFW_DONT_CARE,
                            GLFW_DONT_CARE);

    fontData_ = readFileBytes(fontPath);
    ImFont* font = nullptr;
    if (!fontData_.empty()) {
        font = io.Fonts->AddFontFromMemoryTTF(
            fontData_.data(), static_cast<int>(fontData_.size()), 15.0f * scale,
            &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
    }

    if (!font) {
        fontData_.clear();
        OutputDebugStringA("ImRelighter: failed to load Source Han Sans; using default font.\n");
        io.Fonts->AddFontDefault();
    }
}

void App::loadIcon() {
    const auto iconPath = paths::assetPath("icons/ImRelight.png");
    const auto iconData = readFileBytes(iconPath);
    if (iconData.empty()) return;
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        iconData.data(), static_cast<int>(iconData.size()), &width, &height,
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
    const ExportService::Result result = exports_.poll();
    if (!result.done) return;
    setStatus(result.error.empty() ? tr("export.done") : result.error,
              result.error.empty() ? 4.0 : 10.0);
}

void App::drawFrame() {
    pollExport();
    models_.pollSha();
    const double now = glfwGetTime();
    if (lastFrameTime_ == 0.0) lastFrameTime_ = now;
    if (!ui.statusOverride.empty() && !ui.statusBusy &&
        now >= ui.statusExpiresAt) {
        clearStatus();
    }
    const double delta = std::min(0.05, std::max(0.0, now - lastFrameTime_));
    lastFrameTime_ = now;
    core::Document* active = activeDocument();
    if (active && active->animate) {
        for (core::Light& light : active->lights) {
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
        for (std::size_t i = 0; i < documentCount(); ++i) {
            core::Document* candidate = documents_.document(i);
            if (candidate->id == inferenceResult.documentId) {
                target = candidate;
                break;
            }
        }
        if (!target || target->depthGeneration != inferenceResult.generation) {
            if (!inference_.busy()) clearStatusIfBusy();
            continue;
        }
        if (inferenceResult.cancelled) {
            setStatus(tr("depth.cancelled"));
            continue;
        }
        if (!inferenceResult.error.empty()) {
            setStatus(inferenceResult.error, 10.0);
            continue;
        }
        target->depth = std::move(inferenceResult.depth);
        target->hasDepth = true;
        if (models_.type() == "int8") {
            // Quantized int8 depth often carries strong per-pixel steps;
            // enable the normal median prefilter and a stronger edge-aware
            // smoothing pass without changing the other model profiles.
            target->settings.smooth = std::max(target->settings.smooth, 3.0f);
        }
        ++target->depthRevision;
        if (target == activeDocument()) {
            target->view = core::View::Lit;
        }
        setStatus(tr("depth.ready"));
    }

    ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ui.settingsOpen) {
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                ui.settingsOpen = false;
            }
        } else {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
                requestOpenImage();
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
                const core::Document* doc = activeDocument();
                if (doc && doc->hasDepth && !exportBusy()) {
                    ui.exportPopupRequested = true;
                } else {
                    setStatus(tr("depth.not_generated"));
                }
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
                generateDepth();
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Comma, false)) {
                ui.settingsOpen = true;
            }
            if (io.KeyCtrl && io.KeyShift &&
                ImGui::IsKeyPressed(ImGuiKey_C, false)) {
                copyLook();
            }
            if (io.KeyCtrl && io.KeyShift &&
                ImGui::IsKeyPressed(ImGuiKey_V, false)) {
                pasteLook();
            }
            if (io.KeyCtrl && !io.KeyShift &&
                ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                undo();
            }
            if (io.KeyCtrl &&
                (ImGui::IsKeyPressed(ImGuiKey_Y, false) ||
                 (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))) {
                redo();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
                core::Document* doc = activeDocument();
                if (doc) doc->fitPending = true;
            }
        }
    }

    ui::draw(*this);

    if (core::Document* doc = activeDocument()) {
        trackHistory(*doc, now);
    }

    if (hasTask()) {
        const core::Document* doc = activeDocument();
        if (doc && doc->hasDepth && renderer_) {
            std::string renderError;
            if (!renderer_->renderPreview(*doc, &renderError) &&
                !renderError.empty()) {
                setStatus(renderError, 10.0);
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
    return documents_.hasActive();
}

const core::Document* App::document(size_t index) const {
    return documents_.document(index);
}

const core::Document* App::activeDocument() const {
    return documents_.activeDocument();
}

core::Document* App::activeDocument() {
    return documents_.activeDocument();
}

gpu::Texture* App::thumbnail(uint64_t id) {
    return documents_.thumbnail(id);
}

void App::requestOpenImage() {
    const auto selected =
        native_dialog::openImagesDialog(glfwGetWin32Window(window_),
                                        paths::fromUtf8(tr("topbar.open"))
                                            .c_str());
    if (selected.empty()) return;
    openPaths(selected);
}

void App::openPaths(const std::vector<std::filesystem::path>& files) {
    std::string firstError;
    int failed = 0;
    uint64_t lastOpened = 0;
    for (const auto& path : files) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) continue;
        std::string error;
        const uint64_t id = addDocument(path, &error);
        if (id != 0) {
            lastOpened = id;
        } else {
            ++failed;
            if (firstError.empty()) firstError = std::move(error);
        }
    }

    if (lastOpened != 0) {
        activateDocument(lastOpened);
        if (failed > 0) {
            setStatus(tr("document.open_partial") + " (" +
                          std::to_string(failed) + "): " + firstError,
                      10.0);
        } else {
            clearStatus();
        }
    } else if (!firstError.empty()) {
        setStatus(firstError, 10.0);
    }
}

uint64_t App::addDocument(const std::filesystem::path& path, std::string* error) {
    DocumentStore::Limits limits;
    limits.maxTasks = config.maxTasks;
    limits.memoryGb = config.memoryLimitGb;
    limits.maxImagePixels =
        io::megapixelsToPixels(config.maxImageMegapixels);
    limits.taskLimitMessage = tr("document.limit");
    limits.memoryLimitMessage = tr("document.memory");
    limits.thumbnailFailedMessage = tr("document.thumbnail_failed");
    const uint64_t id = documents_.add(path, limits, error);
    if (id != 0) ui.scrollToTask = id;
    return id;
}

void App::activateDocument(uint64_t id) {
    if (core::Document* previous = activeDocument()) {
        finalizeHistory(*previous);
    }
    if (!documents_.activate(id)) return;
    ui.draggingLight = 0;
    clearStatus();
}

void App::closeDocument(uint64_t id) {
    if (core::Document* active = activeDocument(); active && active->id == id) {
        finalizeHistory(*active);
    }
    const DocumentStore::CloseResult result = documents_.close(id);
    if (!result.closed) return;
    histories_.erase(id);
    if (result.activeChanged) {
        ui.draggingLight = 0;
    }
}

unsigned int App::displayTextureId() const {
    const core::Document* doc = activeDocument();
    if (doc && doc->hasDepth && doc->view != core::View::Original && renderer_ &&
        renderer_->ready() && renderer_->previewTexture() != 0) {
        return renderer_->previewTexture();
    }
    return documents_.activeTextureId();
}

bool App::displayTextureFlipped() const {
    const core::Document* doc = activeDocument();
    return doc && doc->hasDepth && doc->view != core::View::Original && renderer_ &&
           renderer_->ready() && renderer_->previewTexture() != 0;
}

void App::exportImage(core::ExportKind kind) {
    core::Document* doc = activeDocument();
    if (!doc || !doc->hasDepth) {
        setStatus(tr("depth.not_generated"));
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

    if (exports_.busy()) {
        setStatus(tr("export.running"), 0.0, true);
        return;
    }

    std::wstring defaultName = doc->path.stem().wstring() + suffix;
    const auto destination = native_dialog::savePngDialog(
        glfwGetWin32Window(window_), paths::fromUtf8(tr("topbar.export")).c_str(),
        defaultName.c_str());
    if (!destination) return;

    if (kind == core::ExportKind::Depth) {
        std::vector<float> depth = doc->depth;
        const int width = doc->width;
        const int height = doc->height;
        const bool invert = doc->settings.invert;
        exports_.startDepth(*destination, std::move(depth), width, height,
                            invert);
        setStatus(tr("export.running"), 0.0, true);
        return;
    }

    if (!renderer_) {
        setStatus(tr("export.renderer_unavailable"), 10.0);
        return;
    }

    std::vector<uint8_t> rgba;
    std::string error;
    if (!renderer_->renderExport(*doc, view, rgba, &error)) {
        setStatus(error, 10.0);
        return;
    }

    exports_.startRgba(*destination, std::move(rgba), doc->width, doc->height);
    setStatus(tr("export.running"), 0.0, true);
}
void App::cancelInference() {
    if (!inference_.busy()) return;
    inference_.cancel();
    setStatus(tr("depth.cancelling"), 0.0, true);
}

void App::generateDepth() {
    core::Document* doc = activeDocument();
    if (!doc) return;
    if (inference_.busy()) {
        setStatus(tr("depth.other_running"), 6.0);
        return;
    }
    if (modelPath().empty()) {
        setStatus(tr("model.not_selected"), 6.0);
        return;
    }
    if (!modelExists()) {
        setStatus(tr("model.missing"), 6.0);
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
    job.inputSize = doc->settings.inferenceSize;
    job.useDml = config.backend == core::Backend::Dml ||
                 (config.backend == core::Backend::Auto && directmlAvailable());
    job.allowCpuFallback = config.backend == core::Backend::Auto;
    if (!inference_.submit(std::move(job))) {
        setStatus(tr("depth.running"), 0.0, true);
        return;
    }
    setStatus(tr("depth.running"), 0.0, true);
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

void App::setResourceLimits(int maxTasks, int memoryLimitGb, bool persist) {
    config.maxTasks = std::clamp(maxTasks, 1, 500);
    config.memoryLimitGb = std::clamp(memoryLimitGb, 1, 16);
    if (persist) saveConfig();
}

void App::setImageMegapixelLimit(int megapixels, bool persist) {
    config.maxImageMegapixels =
        std::clamp(megapixels, io::kMinImageMegapixels,
                   io::kMaxImageMegapixels);
    if (persist) saveConfig();
}

void App::setBackend(core::Backend backend) {
    if (backend == core::Backend::Dml && !directmlAvailable()) {
        backend = core::Backend::Cpu;
    }
    config.backend = backend;
    saveConfig();
}

App::LookSnapshot App::captureLook(const core::Document& doc) const {
    LookSnapshot look;
    look.settings = doc.settings;
    look.lights = doc.lights;
    look.selectedLight = doc.selectedLight;
    look.soloLight = doc.soloLight;
    look.animate = doc.animate;
    return look;
}

void App::applyLook(core::Document& doc, const LookSnapshot& look) const {
    doc.settings = look.settings;
    doc.lights = look.lights;
    doc.soloLight = look.soloLight;
    doc.selectedLight = look.selectedLight;
    doc.animate = look.animate;
    if (doc.lights.empty()) {
        doc.lights.push_back(core::makeLight(1));
    }
    const bool hasSelection =
        std::any_of(doc.lights.begin(), doc.lights.end(),
                    [&](const core::Light& light) {
                        return light.id == doc.selectedLight;
                    });
    if (!hasSelection) doc.selectedLight = doc.lights.front().id;
    const bool hasSolo =
        doc.soloLight != 0 &&
        std::any_of(doc.lights.begin(), doc.lights.end(),
                    [&](const core::Light& light) {
                        return light.id == doc.soloLight;
                    });
    if (!hasSolo) doc.soloLight = 0;
}

App::DocumentHistory& App::historyFor(std::uint64_t documentId) {
    return histories_[documentId];
}

void App::finalizeHistory(core::Document& doc) {
    if (!doc.id) return;
    auto& history = historyFor(doc.id);
    if (!history.initialized) {
        history.stable = history.latest = captureLook(doc);
        history.initialized = true;
        history.burst = false;
        return;
    }
    if (history.burst) {
        history.stable = history.latest;
        history.burst = false;
    }
}

void App::trackHistory(core::Document& doc, double now) {
    if (!doc.id) return;
    auto& history = historyFor(doc.id);
    const LookSnapshot current = captureLook(doc);
    if (!history.initialized) {
        history.stable = history.latest = current;
        history.initialized = true;
        return;
    }

    // Orbiting lights change every frame by design; do not turn that motion
    // into an endless undo stack.
    if (doc.animate) {
        history.stable = history.latest = current;
        history.burst = false;
        return;
    }

    if (current.sameLook(history.latest)) {
        if (history.burst && now - history.lastChange > 0.5) {
            history.stable = history.latest;
            history.burst = false;
        }
        return;
    }

    if (!history.burst) {
        history.undo.push_back(history.stable);
        if (history.undo.size() > 60) history.undo.erase(history.undo.begin());
        history.redo.clear();
        history.burst = true;
    }
    history.latest = current;
    history.lastChange = now;
}

void App::beginLookEdit(core::Document& doc) {
    auto& history = historyFor(doc.id);
    const LookSnapshot before = captureLook(doc);
    if (!history.initialized) {
        history.stable = history.latest = before;
        history.initialized = true;
    }
    history.undo.push_back(before);
    if (history.undo.size() > 60) history.undo.erase(history.undo.begin());
    history.redo.clear();
    history.burst = false;
}

void App::commitLookEdit(core::Document& doc) {
    auto& history = historyFor(doc.id);
    history.stable = history.latest = captureLook(doc);
    history.initialized = true;
    history.burst = false;
}

void App::copyLook() {
    core::Document* doc = activeDocument();
    if (!doc) return;
    lookClipboard_ = captureLook(*doc);
    lookClipboard_.animate = false;
    lookClipboardValid_ = true;
    setStatus(tr("look.copied"));
}

void App::pasteLook() {
    core::Document* doc = activeDocument();
    if (!doc || !lookClipboardValid_) return;
    beginLookEdit(*doc);
    applyLook(*doc, lookClipboard_);
    commitLookEdit(*doc);
    setStatus(tr("look.pasted"));
}

void App::undo() {
    core::Document* doc = activeDocument();
    if (!doc) return;
    finalizeHistory(*doc);
    auto& history = historyFor(doc->id);
    if (history.undo.empty()) {
        setStatus(tr("history.empty"));
        return;
    }
    history.redo.push_back(captureLook(*doc));
    applyLook(*doc, history.undo.back());
    history.undo.pop_back();
    commitLookEdit(*doc);
    setStatus(tr("history.undo"));
}

void App::redo() {
    core::Document* doc = activeDocument();
    if (!doc) return;
    finalizeHistory(*doc);
    auto& history = historyFor(doc->id);
    if (history.redo.empty()) {
        setStatus(tr("history.empty"));
        return;
    }
    history.undo.push_back(captureLook(*doc));
    applyLook(*doc, history.redo.back());
    history.redo.pop_back();
    commitLookEdit(*doc);
    setStatus(tr("history.redo"));
}

void App::resetTask() {
    core::Document* doc = activeDocument();
    if (!doc) return;

    beginLookEdit(*doc);
    doc->settings = core::defaults();
    doc->lights = {core::makeLight(1)};
    doc->selectedLight = 1;
    doc->soloLight = 0;
    doc->animate = false;
    doc->view = doc->hasDepth ? core::View::Lit : core::View::Original;
    commitLookEdit(*doc);
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
        setStatus(error, 10.0);
    }
}

void App::saveConfig() {
    std::string error;
    if (!core::saveUserConfig(config, &error)) {
        setStatus(error, 10.0);
    }
}

bool App::modelExists() const {
    return models_.exists();
}

bool App::directmlAvailable() const {
    return models_.directmlAvailable();
}

std::uintmax_t App::modelSize() const {
    return models_.size();
}

std::string App::modelSha() const {
    return models_.sha();
}

void App::refreshModelSha() {
    models_.refreshSha();
}

bool App::modelShaPending() const {
    return models_.shaPending();
}

std::filesystem::path App::modelPath() const {
    return models_.path();
}

std::filesystem::path App::modelFolder() const {
    return models_.folder();
}

void App::chooseModelFile() {
    const auto file = native_dialog::pickOnnxFile(
        glfwGetWin32Window(window_),
        paths::fromUtf8(tr("settings.choose_model_file")).c_str());
    if (!file) return;
    std::wstring extension = file->extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    if (!paths::fileExists(*file) || extension != L".onnx") {
        setStatus(tr("model.invalid_file"), 6.0);
        return;
    }

    models_.selectFile(*file);
    clearStatus();
    saveConfig();
}

void App::inspectModelInfo() {
    models_.inspect();
}

std::string App::modelType() const {
    return models_.type();
}

void App::clearModelSelection() {
    models_.clearSelection();
    clearStatus();
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
    if (!ui.statusOverride.empty()) {
        if (ui.statusBusy || glfwGetTime() < ui.statusExpiresAt) {
            return ui.statusOverride;
        }
    }
    const core::Document* doc = activeDocument();
    if (!doc) return tr("status.no_task");
    return doc->name + "  ·  " + std::to_string(doc->width) + " × " +
           std::to_string(doc->height);
}

void App::setStatus(std::string text, double seconds, bool busy) {
    if (text.empty()) {
        clearStatus();
        return;
    }
    ui.statusOverride = std::move(text);
    ui.statusBusy = busy;
    ui.statusExpiresAt = busy ? 0.0 : glfwGetTime() + seconds;
}

void App::clearStatus() {
    ui.statusOverride.clear();
    ui.statusExpiresAt = 0.0;
    ui.statusBusy = false;
}

void App::clearStatusIfBusy() {
    if (ui.statusBusy) clearStatus();
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
    exports_.wait();
    documents_.releaseGpuResources();
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
