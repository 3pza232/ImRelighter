#include "ml/model_config.h"

#include <algorithm>
#include <cmath>
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
    if (value.contains("rescale_factor") &&
        value["rescale_factor"].is_number()) {
        const float factor = value["rescale_factor"].get<float>();
        if (std::isfinite(factor) && factor > 0.0f) {
            config.rescaleFactor = factor;
        }
    }
    if (value.contains("do_normalize") && value["do_normalize"].is_boolean())
        config.normalize = value["do_normalize"].get<bool>();
    if (value.contains("do_resize") && value["do_resize"].is_boolean())
        config.doResize = value["do_resize"].get<bool>();
    if (value.contains("resample") && value["resample"].is_number_integer()) {
        const int code = value["resample"].get<int>();
        if (code >= 0 && code <= 5) {
            config.resample = static_cast<Resample>(code);
        }
    }
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
        directory.parent_path() / L"preprocessor_config.json",
    };
    for (const auto& candidate : candidates) {
        nlohmann::json json;
        if (!readJson(candidate, json)) continue;

        // Some processors wrap the settings in a "preprocess" object while
        // others keep them at the top level.
        if (json.contains("preprocess") && json["preprocess"].is_object()) {
            readPreprocess(json["preprocess"], config);
        } else {
            readPreprocess(json, config);
        }

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
