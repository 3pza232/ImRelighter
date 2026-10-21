#include "core/model_catalog.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <android/log.h>

#include <filesystem>

namespace imrelighter {
namespace {

constexpr const char* kTag = "ImRelighter";

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

}  // namespace

ModelCatalog::ModelCatalog(AAssetManager* assets) : assets_(assets) {
    models_.push_back(ModelInfo{
        "q4",
        "Q4",
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

bool ModelCatalog::register_custom_model(const std::string& files_dir,
                                         const std::string& absolute_path) {
    if (!fileExists(absolute_path)) {
        __android_log_print(ANDROID_LOG_WARN, kTag,
                            "custom model file is missing: %s", absolute_path.c_str());
        return false;
    }

    models_.erase(std::remove_if(models_.begin(), models_.end(),
                                 [](const ModelInfo& model) {
                                     return model.id == "custom";
                                 }),
                  models_.end());

    const std::filesystem::path path(absolute_path);
    const std::string filename = path.filename().string();
    models_.push_back(ModelInfo{
        "custom",
        filename.empty() ? "自定义模型" : filename,
        "自定义 ONNX · 本地文件",
        absolute_path,
        true,
    });

    std::ofstream file(files_dir + "/custom_model.txt", std::ios::trunc);
    if (file) {
        file << absolute_path;
    }
    __android_log_print(ANDROID_LOG_INFO, kTag, "registered custom model: %s",
                        absolute_path.c_str());
    return true;
}

void ModelCatalog::load_custom_model(const std::string& files_dir) {
    std::ifstream file(files_dir + "/custom_model.txt");
    if (!file) {
        return;
    }
    std::string path;
    std::getline(file, path);
    if (!path.empty() && fileExists(path)) {
        register_custom_model(files_dir, path);
    }
}

std::string ModelCatalog::load_selection(const std::string& files_dir) const {
    const_cast<ModelCatalog*>(this)->load_custom_model(files_dir);
    std::ifstream input(files_dir + "/model.txt");
    std::string id;
    if (input >> id && available(id)) {
        return id;
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
