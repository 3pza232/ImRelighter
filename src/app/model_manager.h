#pragma once

#include <cstdint>
#include <filesystem>
#include <future>
#include <string>

#include "core/config.h"
#include "ml/model_metadata.h"
#include "ml/onnx.h"

namespace app {

// Resolves the selected model, reports its metadata, and owns the background
// inspection task. File selection itself stays in App because it needs a
// native dialog owned by the window.
class ModelManager {
public:
    explicit ModelManager(core::UserConfig& config) : config_(&config) {}

    bool exists() const;
    bool directmlAvailable() const;
    std::uintmax_t size() const;
    std::filesystem::path path() const;
    std::filesystem::path folder() const;

    void selectFile(const std::filesystem::path& file);
    void clearSelection();

    void inspect();
    bool inspectPending() const { return inspectionFuture_.valid(); }
    const std::string& inspectError() const { return inspectError_; }
    std::string type() const;

    void refreshSha();
    void pollSha();
    bool shaPending() const;
    std::string sha() const { return shaValue_; }

private:
    struct Inspection {
        std::filesystem::path path;
        ml::ModelInfo info;
        ml::WeightInfo weights;
        std::string error;
    };

    core::UserConfig* config_;
    ml::ModelInfo info_;
    ml::WeightInfo weights_;
    std::filesystem::path inspectedPath_;
    std::string inspectError_;
    bool inspectAttempted_ = false;
    std::future<Inspection> inspectionFuture_;
    std::filesystem::path shaPath_;
    std::string shaValue_;
    std::future<std::pair<std::filesystem::path, std::string>> shaFuture_;
    mutable bool dmlChecked_ = false;
    mutable bool dmlAvailable_ = false;
};

} // namespace app
