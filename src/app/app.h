#pragma once

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/config.h"
#include "core/document.h"
#include "core/i18n.h"
#include "core/types.h"
#include "gpu/renderer.h"
#include "ml/inference.h"
#include "ml/model_metadata.h"
#include "gpu/texture.h"

struct GLFWwindow;

namespace app {

struct UiState {
    bool sidebarOpen = true;
    bool settingsOpen = false;
    bool exportPopupRequested = false;

    core::Settings settings = core::defaults();
    std::vector<core::Light> lights{core::makeLight(1)};
    int selectedLight = 1;
    core::View view = core::View::Original;
    bool animate = false;

    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    bool fitPending = true;

    std::string statusOverride;
};

class App {
public:
    App() = default;
    ~App();

    int run();

    void requestOpenImage();
    void generateDepth();
    void cancelInference();
    void exportImage(core::ExportKind kind);
    void setLanguage(core::Language language);
    void setBackend(core::Backend backend);
    void setTheme(core::Theme theme);
    void resetTask();

    bool hasTask() const;
    bool inferenceBusy() const { return inference_.busy(); }
    bool exportBusy() const { return exportFuture_.valid(); }
    size_t documentCount() const { return documents_.size(); }
    const core::Document* document(size_t index) const;
    const core::Document* activeDocument() const;
    core::Document* activeDocument();
    int activeIndex() const { return activeIndex_; }
    gpu::Texture* thumbnail(uint64_t id);
    unsigned int displayTextureId() const;
    bool displayTextureFlipped() const;

    void activateDocument(uint64_t id);
    void closeDocument(uint64_t id);

    bool modelExists() const;
    bool directmlAvailable() const;
    std::uintmax_t modelSize() const;
    std::string modelSha() const;
    std::filesystem::path modelPath() const;
    std::filesystem::path modelFolder() const;
    void chooseModelFile();
    void clearModelSelection();
    void inspectModelInfo();
    void refreshModelSha();
    bool modelShaPending() const;
    const ml::ModelInfo& modelInfo() const { return modelInfo_; }
    const std::string& modelInfoError() const { return modelInfoError_; }
    std::string modelType() const;

    std::string glVersion() const;
    std::string glRenderer() const;

    std::string tr(std::string_view key) const;
    std::string statusText() const;

    GLFWwindow* window() const { return window_; }
    core::Language language() const { return config.language; }
    core::Backend backend() const { return config.backend; }
    core::Theme theme() const { return config.theme; }

    core::I18n i18n;
    core::UserConfig config;
    UiState ui;

private:
    bool initializeWindow();
    bool initializeImGui();
    void loadFonts();
    void loadIcon();
    void loadConfigAndLanguage();
    void openInitialFilesFromCommandLine();
    void saveConfig();
    void drawFrame();
    void pollExport();
    void shutdown();
    void centerWindow();
    void showError(const std::wstring& message) const;

    uint64_t addDocument(const std::filesystem::path& path, std::string* error);
    void loadUiFromActive();
    void saveUiToActive();
    void uploadActiveTexture();
    void releaseGpuResources();

    GLFWwindow* window_ = nullptr;
    bool glfwReady_ = false;
    bool imguiReady_ = false;
    std::string glVersion_;
    std::string glRenderer_;

    std::vector<std::unique_ptr<core::Document>> documents_;
    int activeIndex_ = -1;
    uint64_t nextDocumentId_ = 1;
    gpu::Texture activeTexture_;
    std::unique_ptr<gpu::Renderer> renderer_;
    ml::InferenceService inference_;
    std::unordered_map<uint64_t, std::unique_ptr<gpu::Texture>> thumbnails_;
    std::future<std::string> exportFuture_;
    double lastFrameTime_ = 0.0;
    ml::ModelInfo modelInfo_;
    ml::WeightInfo modelWeights_;
    std::future<std::pair<std::filesystem::path, std::string>> modelShaFuture_;
    std::filesystem::path modelShaPath_;
    std::string modelShaValue_;
    std::filesystem::path modelInfoPath_;
    std::string modelInfoError_;
    bool modelInfoAttempted_ = false;
    mutable bool dmlChecked_ = false;
    mutable bool dmlAvailable_ = false;
};

} // namespace app
