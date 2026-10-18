#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "app/document_store.h"
#include "app/export_service.h"
#include "app/model_manager.h"
#include "core/config.h"
#include "core/document.h"
#include "core/i18n.h"
#include "core/types.h"
#include "gpu/renderer.h"
#include "ml/inference.h"

struct GLFWwindow;

namespace app {

struct UiState {
    bool sidebarOpen = true;
    bool settingsOpen = false;
    bool exportPopupRequested = false;
    int draggingLight = 0;

    std::string statusOverride;
    double statusExpiresAt = 0.0;
    bool statusBusy = false;
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
    void setResourceLimits(int maxTasks, int memoryLimitGb, bool persist = true);
    void resetTask();

    bool hasTask() const;
    bool inferenceBusy() const { return inference_.busy(); }
    bool exportBusy() const { return exports_.busy(); }
    size_t documentCount() const { return documents_.size(); }
    const core::Document* document(size_t index) const;
    const core::Document* activeDocument() const;
    core::Document* activeDocument();
    int activeIndex() const { return documents_.activeIndex(); }
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
    bool modelInfoPending() const { return models_.inspectPending(); }
    const std::string& modelInfoError() const { return models_.inspectError(); }
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
    void setStatus(std::string text, double seconds = 4.0, bool busy = false);
    void clearStatus();
    void clearStatusIfBusy();
    void drawFrame();
    void pollExport();
    void shutdown();
    void centerWindow();
    void showError(const std::wstring& message) const;

    uint64_t addDocument(const std::filesystem::path& path, std::string* error);

    GLFWwindow* window_ = nullptr;
    bool glfwReady_ = false;
    bool imguiReady_ = false;
    std::string glVersion_;
    std::string glRenderer_;
    std::vector<unsigned char> fontData_;

    DocumentStore documents_;
    std::unique_ptr<gpu::Renderer> renderer_;
    ml::InferenceService inference_;
    ModelManager models_{config};
    ExportService exports_;
    double lastFrameTime_ = 0.0;
};

} // namespace app
