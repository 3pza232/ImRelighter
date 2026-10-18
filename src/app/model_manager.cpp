#include "app/model_manager.h"

#include <chrono>

#include "io/hash.h"
#include "io/paths.h"
#include "ml/onnx.h"

namespace app {
namespace {

std::filesystem::path resolveModelFile(const std::filesystem::path& directory) {
    const auto direct = directory / L"model.onnx";
    if (paths::fileExists(direct)) return direct;

    const auto nested = directory / L"depth-anything-v2-small" / L"model.onnx";
    if (paths::fileExists(nested)) return nested;
    return {};
}

} // namespace

bool ModelManager::exists() const {
    return paths::fileExists(path());
}

bool ModelManager::directmlAvailable() const {
    if (dmlChecked_) return dmlAvailable_;
    dmlChecked_ = true;
    if (!paths::fileExists(paths::exeDir() / L"DirectML.dll")) return false;
    ml::OnnxRuntime runtime;
    std::string error;
    if (!runtime.load(paths::exeDir() / L"onnxruntime.dll", &error)) return false;
    dmlAvailable_ = runtime.hasDml();
    return dmlAvailable_;
}

std::uintmax_t ModelManager::size() const {
    const auto modelFile = path();
    if (modelFile.empty()) return 0;
    std::uintmax_t total = paths::fileSize(modelFile);
    auto external = modelFile;
    external += L"_data";
    total += paths::fileSize(external);
    return total;
}

std::filesystem::path ModelManager::path() const {
    if (config_->modelSelectionCleared) return {};
    if (!config_->modelFile.empty()) return paths::fromUtf8(config_->modelFile);
    if (!config_->modelDir.empty()) {
        const auto selected = paths::fromUtf8(config_->modelDir);
        const auto resolved = resolveModelFile(selected);
        if (!resolved.empty()) return resolved;
        return selected / L"model.onnx";
    }
    return paths::modelFile();
}

std::filesystem::path ModelManager::folder() const {
    const auto modelFile = path();
    return modelFile.empty() ? paths::modelDir() : modelFile.parent_path();
}

void ModelManager::selectFile(const std::filesystem::path& file) {
    config_->modelFile = paths::toUtf8(file);
    config_->modelDir.clear();
    config_->modelSelectionCleared = false;
    info_ = {};
    weights_ = {};
    inspectedPath_.clear();
    inspectError_.clear();
    inspectAttempted_ = false;
    shaPath_.clear();
    shaValue_.clear();
}

void ModelManager::clearSelection() {
    config_->modelFile.clear();
    config_->modelDir.clear();
    config_->modelSelectionCleared = true;
    info_ = {};
    weights_ = {};
    inspectedPath_.clear();
    inspectError_.clear();
    inspectAttempted_ = false;
    shaPath_.clear();
    shaValue_.clear();
}

void ModelManager::inspect() {
    const auto modelFile = path();

    if (inspectionFuture_.valid() &&
        inspectionFuture_.wait_for(std::chrono::seconds(0)) ==
            std::future_status::ready) {
        Inspection result = inspectionFuture_.get();
        if (!modelFile.empty() && result.path == modelFile) {
            info_ = std::move(result.info);
            weights_ = std::move(result.weights);
            inspectError_ = std::move(result.error);
            inspectedPath_ = modelFile;
            inspectAttempted_ = true;
        } else {
            inspectAttempted_ = false;
        }
    }

    if (modelFile.empty()) {
        info_ = {};
        weights_ = {};
        inspectError_.clear();
        inspectedPath_.clear();
        inspectAttempted_ = true;
        return;
    }
    if (inspectAttempted_ && inspectedPath_ == modelFile) return;
    if (inspectionFuture_.valid()) return;

    inspectedPath_ = modelFile;
    inspectAttempted_ = true;
    info_ = {};
    weights_ = {};
    inspectError_.clear();

    const auto dllPath = paths::exeDir() / L"onnxruntime.dll";
    inspectionFuture_ = std::async(std::launch::async, [modelFile, dllPath]() {
        Inspection result;
        result.path = modelFile;

        ml::OnnxRuntime runtime;
        std::string error;
        if (!runtime.load(dllPath, &error)) {
            result.error = std::move(error);
            return result;
        }
        ml::OnnxSession session;
        if (!session.load(runtime, modelFile, false, &error)) {
            result.error = std::move(error);
            return result;
        }
        result.info = session.info();
        std::string weightError;
        inspectModelWeights(modelFile, result.weights, &weightError);
        return result;
    });
}

std::string ModelManager::type() const {
    if (weights_.fp16 > weights_.fp32) return "fp16";
    if (weights_.fp32 > 0) return "fp32";
    if (info_.inputType == "FLOAT16") return "fp16";
    return "fp32";
}

void ModelManager::refreshSha() {
    const auto modelFile = path();
    if (modelFile.empty()) {
        if (!shaFuture_.valid()) {
            shaPath_.clear();
            shaValue_.clear();
        }
        return;
    }
    if (modelFile == shaPath_ || shaFuture_.valid()) return;
    shaPath_ = modelFile;
    shaValue_.clear();
    shaFuture_ = std::async(std::launch::async, [modelFile] {
        std::string error;
        return std::make_pair(modelFile, io::sha256Model(modelFile, &error));
    });
}

void ModelManager::pollSha() {
    if (!shaFuture_.valid() ||
        shaFuture_.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready) {
        return;
    }
    auto result = shaFuture_.get();
    if (result.first == path()) {
        shaValue_ = std::move(result.second);
    } else {
        shaPath_.clear();
    }
}

bool ModelManager::shaPending() const {
    return shaFuture_.valid() && shaPath_ == path();
}

} // namespace app
