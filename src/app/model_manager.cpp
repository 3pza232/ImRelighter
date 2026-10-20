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

    const auto directVersioned = directory / L"model_q4f16.onnx";
    if (paths::fileExists(directVersioned)) return directVersioned;

    const auto versioned = directory / L"q4f16" / L"model_q4f16.onnx";
    if (paths::fileExists(versioned)) return versioned;

    const auto nested = directory / L"depth-anything-v2-small" / L"model.onnx";
    if (paths::fileExists(nested)) return nested;

    const auto bundled = directory / L"depth-anything-v2-small" / L"q4f16" /
                         L"model_q4f16.onnx";
    if (paths::fileExists(bundled)) return bundled;
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
        // Read weight types before creating the session. A quantized model can
        // fail to load on an older ORT (for example ConvInteger), but the
        // settings page should still be able to label it as int8/q4 rather
        // than showing only a generic error.
        std::string weightError;
        inspectModelWeights(modelFile, result.weights, &weightError);
        const std::string filename = modelFile.filename().string();
        if (filename.find("int8") != std::string::npos) {
            result.error =
                "int8 ConvInteger operations are not supported by the bundled "
                "ONNX Runtime/DirectML; use q4, q4f16, FP16 or FP32.";
            return result;
        }
        ml::OnnxSession session;
        if (!session.load(runtime, modelFile, false, &error)) {
            // Quantized exports may be usable only through DirectML even when
            // the CPU EP lacks a kernel. Inspection should mirror the Auto
            // backend instead of painting every such model red.
            if (runtime.hasDml()) {
                std::string dmlError;
                if (!session.load(runtime, modelFile, true, &dmlError)) {
                    result.error = std::move(error) +
                                   " (DirectML: " + dmlError + ")";
                    return result;
                }
            } else {
                result.error = std::move(error);
                return result;
            }
        }
        result.info = session.info();
        return result;
    });
}

std::string ModelManager::type() const {
    const std::string filename = path().filename().string();
    const auto has = [&](const char* token) {
        return filename.find(token) != std::string::npos;
    };
    // Quantized exports often keep float input/output, so the tensor type in
    // the session does not reveal the weight format. Prefer the explicit
    // filename token used by the common Hugging Face exports, then fall back
    // to the initializer data types.
    if (has("q4f16")) return "q4f16";
    if (has("bnb4")) return "bnb4";
    if (has("int8")) return "int8";
    if (has("q4")) return "q4";

    if (weights_.int8 > 0 || weights_.uint8 > 0) return "int8";
    if (weights_.int4 > 0 || weights_.uint4 > 0) {
        return weights_.fp16 > 0 ? "q4f16" : "q4";
    }
    if (weights_.fp16 > weights_.fp32) return "fp16";
    if (weights_.bf16 > 0) return "bf16";
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
