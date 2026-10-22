#include "core/model_catalog.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <android/log.h>

#include <filesystem>

namespace imrelighter {
namespace {

constexpr const char* kTag = "ImRelighter";
constexpr const char* kCustomPrefix = "file:";

bool assetExists(AAssetManager* assets, const std::string& path) {
    if (assets == nullptr || path.empty()) {
        return false;
    }
    AAsset* asset = AAssetManager_open(assets, path.c_str(), AASSET_MODE_BUFFER);
    if (asset == nullptr) {
        return false;
    }
    AAsset_close(asset);
    return true;
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return !path.empty() && std::filesystem::is_regular_file(path, ec);
}

std::string customId(const std::string& path) {
    return std::string(kCustomPrefix) + path;
}

std::string displayName(const std::filesystem::path& path) {
    std::string name = path.parent_path().filename().string();
    if (name.empty() || name == ".") {
        name = path.stem().string();
    }
    return name.empty() ? "自定义模型" : name;
}

void addCustomModel(std::vector<ModelInfo>& models,
                    const std::string& absolute_path) {
    const std::string id = customId(absolute_path);
    models.erase(std::remove_if(models.begin(), models.end(),
                                [&](const ModelInfo& model) {
                                    return model.id == id;
                                }),
                 models.end());
    const std::filesystem::path path(absolute_path);
    models.push_back(ModelInfo{
        id,
        displayName(path),
        "自定义 ONNX · 本地文件",
        absolute_path,
        true,
    });
}

void saveCustomModels(const std::string& files_dir,
                      const std::vector<ModelInfo>& models) {
    std::ofstream file(files_dir + "/custom_models.txt", std::ios::trunc);
    if (!file) {
        return;
    }
    for (const ModelInfo& model : models) {
        if (model.id.rfind(kCustomPrefix, 0) == 0) {
            file << model.asset_path << '\n';
        }
    }
}

}  // namespace

ModelCatalog::ModelCatalog(AAssetManager* assets) : assets_(assets) {
    models_.push_back(ModelInfo{
        "q4",
        "q4",
        "4-bit 权重 · FP32 激活 · 推荐",
        "models/depth-anything-v2-small/q4/model_q4.onnx",
        false,
    });
    for (ModelInfo& model : models_) {
        model.available = assetExists(assets_, model.asset_path);
        __android_log_print(ANDROID_LOG_INFO, kTag, "model %s available=%d",
                            model.id.c_str(), model.available ? 1 : 0);
    }
}

const ModelInfo* ModelCatalog::find(const std::string& id) const {
    for (const ModelInfo& model : models_) {
        if (model.id == id) {
            return &model;
        }
    }
    return nullptr;
}

const ModelInfo& ModelCatalog::default_model() const {
    return models_.front();
}

bool ModelCatalog::available(const std::string& id) const {
    const ModelInfo* model = find(id);
    return model != nullptr && model->available;
}

std::string ModelCatalog::register_custom_model(
    const std::string& files_dir, const std::string& absolute_path) {
    if (!fileExists(absolute_path)) {
        __android_log_print(ANDROID_LOG_WARN, kTag,
                            "custom model file is missing: %s", absolute_path.c_str());
        return {};
    }

    addCustomModel(models_, absolute_path);
    saveCustomModels(files_dir, models_);
    const std::string id = customId(absolute_path);
    __android_log_print(ANDROID_LOG_INFO, kTag,
                        "registered custom model: %s (%s)", absolute_path.c_str(),
                        displayName(std::filesystem::path(absolute_path)).c_str());
    return id;
}

bool ModelCatalog::remove_custom_model(const std::string& files_dir,
                                       const std::string& id) {
    if (id.rfind(kCustomPrefix, 0) != 0) {
        return false;
    }
    const auto before = models_.size();
    models_.erase(std::remove_if(models_.begin(), models_.end(),
                                 [&](const ModelInfo& model) {
                                     return model.id == id;
                                 }),
                  models_.end());
    if (models_.size() == before) {
        return false;
    }
    saveCustomModels(files_dir, models_);
    return true;
}

void ModelCatalog::load_custom_model(const std::string& files_dir) {
    // Keep q4 and any already registered models; load each persisted custom
    // path once. The old single-path file is still accepted for upgrades.
    std::ifstream file(files_dir + "/custom_models.txt");
    if (!file) {
        file.open(files_dir + "/custom_model.txt");
    }
    if (!file) {
        return;
    }
    std::string path;
    while (std::getline(file, path)) {
        if (!path.empty() && path.back() == '\r') {
            path.pop_back();
        }
        if (!path.empty() && fileExists(path)) {
            addCustomModel(models_, path);
        }
    }
    saveCustomModels(files_dir, models_);
}

std::string ModelCatalog::load_selection(const std::string& files_dir) const {
    const_cast<ModelCatalog*>(this)->load_custom_model(files_dir);
    std::ifstream input(files_dir + "/model.txt");
    std::string id;
    if (std::getline(input, id)) {
        if (!id.empty() && id.back() == '\r') {
            id.pop_back();
        }
        if (id == "custom") {
            // Migrate the pre-multi-model selection id to the first custom
            // model retained from custom_models.txt.
            for (const ModelInfo& model : models_) {
                if (model.id.rfind(kCustomPrefix, 0) == 0) {
                    return model.id;
                }
            }
        }
        if (!id.empty() && available(id)) {
            return id;
        }
    }
    return kDefaultId;
}

bool ModelCatalog::save_selection(const std::string& files_dir,
                                  const std::string& id) const {
    if (!available(id)) {
        return false;
    }
    std::ofstream output(files_dir + "/model.txt", std::ios::trunc);
    if (!output) {
        return false;
    }
    output << id << '\n';
    return output.good();
}

}  // namespace imrelighter
