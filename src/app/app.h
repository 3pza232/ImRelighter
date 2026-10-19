#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
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
    int focusLight = 0;
    uint64_t scrollToTask = 0;

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
    void openPaths(const std::vector<std::filesystem::path>& paths);
    void copyLook();
    void pasteLook();
    bool hasLookClipboard() const { return lookClipboardValid_; }
    void undo();
    void redo();
    void generateDepth();
    void cancelInference();
    void exportImage(core::ExportKind kind);
    void setLanguage(core::Language language);
    void setBackend(core::Backend backend);
    void setTheme(core::Theme theme);
    void setResourceLimits(int maxTasks, int memoryLimitGb, bool persist = true);
    void setImageMegapixelLimit(int megapixels, bool persist = true);
    void resetTask();

    bool hasTask() const;
    bool inferenceBusy() const { return inference_.busy(); }
    uint64_t inferenceDocumentId() const {
        return inference_.activeDocumentId();
    }
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
    struct LookSnapshot {
        core::Settings settings;
        std::vector<core::Light> lights;
        int selectedLight = 1;
        int soloLight = 0;
        bool animate = false;

        // Selecting a light is a navigation action, not a look edit. History
        // compares everything except the selection so clicking the light list
        // does not create an undo step.
        bool sameLook(const LookSnapshot& other) const {
            return settings == other.settings && lights == other.lights &&
                   soloLight == other.soloLight && animate == other.animate;
        }
    };

    struct DocumentHistory {
        std::vector<LookSnapshot> undo;
        std::vector<LookSnapshot> redo;
        LookSnapshot stable;
        LookSnapshot latest;
        bool initialized = false;
        bool burst = false;
        double lastChange = 0.0;
    };

    LookSnapshot captureLook(const core::Document& doc) const;
    void applyLook(core::Document& doc, const LookSnapshot& look) const;
    DocumentHistory& historyFor(std::uint64_t documentId);
    void finalizeHistory(core::Document& doc);
    void trackHistory(core::Document& doc, double now);
    void beginLookEdit(core::Document& doc);
    void commitLookEdit(core::Document& doc);

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

    std::unordered_map<std::uint64_t, DocumentHistory> histories_;
    LookSnapshot lookClipboard_;
    bool lookClipboardValid_ = false;
};

} // namespace app
