#include "ml/model_config.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <android/log.h>
#include <nlohmann/json.hpp>

#include "io/image_loader.h"

namespace imrelighter::ml {
namespace {

constexpr const char* kTag = "ImRelighterAndroid";

bool readTextFile(const std::filesystem::path& path, std::string& text) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    text.assign(std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
    return !text.empty();
}

bool readAssetText(AAssetManager* assets, const std::string& path,
                   std::string& text) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!io::readAsset(assets, path, bytes, error) || bytes.empty()) {
        return false;
    }
    text.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

bool readJson(const std::string& text, nlohmann::json& json) {
    try {
        json = nlohmann::json::parse(text);
        return json.is_object();
    } catch (const std::exception&) {
        return false;
    }
}

bool readPreprocess(const nlohmann::json& value, ModelConfig& config) {
    if (!value.is_object()) {
        return false;
    }

    const auto readSize = [&](const nlohmann::json& size) {
        if (size.is_number_integer()) {
            config.inputSize = size.get<int>();
        } else if (size.is_object()) {
            if (size.contains("height") && size["height"].is_number_integer()) {
                config.inputSize = size["height"].get<int>();
            } else if (size.contains("width") && size["width"].is_number_integer()) {
                config.inputSize = size["width"].get<int>();
            }
        }
    };

    if (value.contains("size")) {
        readSize(value["size"]);
    }
    if (value.contains("ensure_multiple_of") &&
        value["ensure_multiple_of"].is_number_integer()) {
        config.sizeMultiple = value["ensure_multiple_of"].get<int>();
    }
    if (value.contains("do_rescale") && value["do_rescale"].is_boolean()) {
        config.rescale = value["do_rescale"].get<bool>();
    }
    if (value.contains("rescale_factor") && value["rescale_factor"].is_number()) {
        const float factor = value["rescale_factor"].get<float>();
        if (std::isfinite(factor) && factor > 0.0f) {
            config.rescaleFactor = factor;
        }
    }
    if (value.contains("do_normalize") && value["do_normalize"].is_boolean()) {
        config.normalize = value["do_normalize"].get<bool>();
    }
    if (value.contains("do_resize") && value["do_resize"].is_boolean()) {
        config.doResize = value["do_resize"].get<bool>();
    }
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
        for (size_t i = 0; i < 3; ++i) {
            if (value[key][i].is_number()) {
                destination[i] = value[key][i].get<float>();
            }
        }
    };
    readArray("image_mean", config.mean);
    readArray("mean", config.mean);
    readArray("image_std", config.stddev);
    readArray("std", config.stddev);
    return true;
}

void applyJson(const nlohmann::json& json, ModelConfig& config) {
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
}

}  // namespace

ModelConfig defaultModelConfig() {
    return ModelConfig{};
}

ModelConfig loadModelConfig(AAssetManager* assets, const std::string& modelPath) {
    ModelConfig config;
    if (modelPath.empty()) {
        return config;
    }

    std::string path = modelPath;
    if (path.rfind("file:", 0) == 0) {
        path.erase(0, 5);
    }
    const bool localFile = !path.empty() && path.front() == '/';
    const std::filesystem::path model(path);
    const std::filesystem::path directory = model.parent_path();
    const std::filesystem::path candidates[] = {
        directory / "preprocessor_config.json",
        directory.parent_path() / "preprocessor_config.json",
    };

    for (const auto& candidate : candidates) {
        std::string text;
        const std::string candidatePath = candidate.generic_string();
        const bool read = localFile ? readTextFile(candidate, text)
                                    : readAssetText(assets, candidatePath, text);
        if (!read) {
            continue;
        }
        nlohmann::json json;
        if (!readJson(text, json)) {
            continue;
        }
        applyJson(json, config);
        config.loaded = true;
        __android_log_print(ANDROID_LOG_INFO, kTag,
                            "preprocessor config loaded: %s", candidatePath.c_str());
        break;
    }

    if (config.inputSize <= 0) {
        config.inputSize = 518;
    }
    if (config.sizeMultiple <= 0) {
        config.sizeMultiple = 14;
    }
    for (int i = 0; i < 3; ++i) {
        if (!std::isfinite(config.stddev[i]) || config.stddev[i] <= 0.0f) {
            config.stddev[i] = 0.225f;
        }
        if (!std::isfinite(config.mean[i])) {
            config.mean[i] = 0.0f;
        }
    }
    return config;
}

}  // namespace imrelighter::ml
