#include "ml/model_config.h"

#include <algorithm>
#include <fstream>

#include "nlohmann/json.hpp"

namespace ml {
namespace {

bool readJson(const std::filesystem::path& path, nlohmann::json& json) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    try {
        stream >> json;
        return json.is_object();
    } catch (const std::exception&) {
        return false;
    }
}

bool readPreprocess(const nlohmann::json& value, ModelConfig& config) {
    if (!value.is_object()) return false;

    const auto readSize = [&](const nlohmann::json& size) {
        if (size.is_number_integer()) {
            config.inputSize = size.get<int>();
        } else if (size.is_object()) {
            if (size.contains("height") && size["height"].is_number_integer())
                config.inputSize = size["height"].get<int>();
            else if (size.contains("width") && size["width"].is_number_integer())
                config.inputSize = size["width"].get<int>();
        }
    };

    if (value.contains("size")) readSize(value["size"]);
    if (value.contains("ensure_multiple_of") &&
        value["ensure_multiple_of"].is_number_integer()) {
        config.sizeMultiple = value["ensure_multiple_of"].get<int>();
    }
    if (value.contains("do_rescale") && value["do_rescale"].is_boolean())
        config.rescale = value["do_rescale"].get<bool>();
    if (value.contains("do_normalize") && value["do_normalize"].is_boolean())
        config.normalize = value["do_normalize"].get<bool>();
    const auto readArray = [&](const char* key, float* destination) {
        if (!value.contains(key) || !value[key].is_array() ||
            value[key].size() != 3) {
            return;
        }
        for (size_t i = 0; i < 3; ++i)
            destination[i] = value[key][i].get<float>();
    };
    readArray("image_mean", config.mean);
    readArray("mean", config.mean);
    readArray("image_std", config.stddev);
    readArray("std", config.stddev);
    return true;
}

} // namespace

ModelConfig defaults() {
    return ModelConfig{};
}

ModelConfig loadModelConfig(const std::filesystem::path& modelPath) {
    ModelConfig config;
    if (modelPath.empty()) return config;

    const auto directory = modelPath.parent_path();
    const std::filesystem::path candidates[] = {
        directory / L"preprocessor_config.json",
        directory / L"manifest.json",
        directory.parent_path() / L"preprocessor_config.json",
        directory.parent_path() / L"manifest.json",
    };
    for (const auto& candidate : candidates) {
        nlohmann::json json;
        if (!readJson(candidate, json)) continue;

        if (json.contains("preprocess"))
            readPreprocess(json["preprocess"], config);
        else
            readPreprocess(json, config);

        if (json.contains("output") && json["output"].is_object() &&
            json["output"].contains("invert") &&
            json["output"]["invert"].is_boolean()) {
            config.invertDepth = json["output"]["invert"].get<bool>();
        }
        config.loaded = true;
        break;
    }

    if (config.inputSize <= 0) config.inputSize = 518;
    if (config.sizeMultiple <= 0) config.sizeMultiple = 14;
    for (int i = 0; i < 3; ++i) {
        if (config.stddev[i] <= 0.0f) config.stddev[i] = 0.225f;
    }
    return config;
}

} // namespace ml
