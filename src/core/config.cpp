#include "core/config.h"

#include <algorithm>
#include <fstream>

#include "core/i18n.h"
#include "io/paths.h"
#include "nlohmann/json.hpp"

namespace core {

const char* backendKey(Backend backend) {
    switch (backend) {
        case Backend::Cpu: return "cpu";
        case Backend::Dml: return "dml";
        case Backend::Auto: return "auto";
    }
    return "auto";
}

const char* themeKey(Theme theme) {
    switch (theme) {
        case Theme::Light: return "light";
        case Theme::Dark: return "dark";
    }
    return "dark";
}

Theme themeFromCode(const std::string& code) {
    if (code == "light") return Theme::Light;
    return Theme::Dark;
}

Backend backendFromCode(const std::string& code) {
    if (code == "cpu") return Backend::Cpu;
    if (code == "dml") return Backend::Dml;
    return Backend::Auto;
}

bool loadUserConfig(UserConfig& config, std::string* error) {
    return loadUserConfigFrom(paths::configFile(), config, error);
}

bool loadUserConfigFrom(const std::filesystem::path& path, UserConfig& config,
                        std::string* error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return true; // No config yet is not an error.

    try {
        nlohmann::json json;
        stream >> json;

        if (json.contains("language") && json["language"].is_string()) {
            config.language = I18n::fromCode(json["language"].get<std::string>());
            config.languageSet = true;
        }
        if (json.contains("backend") && json["backend"].is_string()) {
            config.backend = backendFromCode(json["backend"].get<std::string>());
        }
        if (json.contains("theme") && json["theme"].is_string()) {
            config.theme = themeFromCode(json["theme"].get<std::string>());
        }
        if (json.contains("modelFile") && json["modelFile"].is_string()) {
            config.modelFile = json["modelFile"].get<std::string>();
        }
        if (json.contains("modelDir") && json["modelDir"].is_string()) {
            config.modelDir = json["modelDir"].get<std::string>();
        }
        if (json.contains("modelSelectionCleared") &&
            json["modelSelectionCleared"].is_boolean()) {
            config.modelSelectionCleared =
                json["modelSelectionCleared"].get<bool>();
        }
        if (json.contains("maxTasks") && json["maxTasks"].is_number_integer()) {
            config.maxTasks = json["maxTasks"].get<int>();
        }
        if (json.contains("memoryLimitGb") &&
            json["memoryLimitGb"].is_number_integer()) {
            config.memoryLimitGb = json["memoryLimitGb"].get<int>();
        }
        if (json.contains("maxImageMegapixels") &&
            json["maxImageMegapixels"].is_number_integer()) {
            config.maxImageMegapixels =
                json["maxImageMegapixels"].get<int>();
        }
        config.maxTasks = std::clamp(config.maxTasks, 1, 500);
        config.memoryLimitGb = std::clamp(config.memoryLimitGb, 1, 16);
        config.maxImageMegapixels =
            std::clamp(config.maxImageMegapixels, 4, 200);
        config.rawJson = json.dump();
        return true;
    } catch (const std::exception& e) {
        if (error) *error = std::string("Failed to read config: ") + e.what();
        return false;
    }
}

bool saveUserConfig(const UserConfig& config, std::string* error) {
    return saveUserConfigTo(paths::configFile(), config, error);
}

bool saveUserConfigTo(const std::filesystem::path& path,
                      const UserConfig& config, std::string* error) {
    try {
        nlohmann::json json = nlohmann::json::object();
        if (!config.rawJson.empty()) {
            try {
                nlohmann::json preserved = nlohmann::json::parse(config.rawJson);
                if (preserved.is_object()) json = std::move(preserved);
            } catch (const std::exception&) {
                // A stale or hand-edited raw snapshot must not block saving
                // the current known fields.
            }
        }
        json["version"] = 1;
        json["language"] = I18n::code(config.language);
        json["backend"] = backendKey(config.backend);
        json["theme"] = themeKey(config.theme);
        if (!config.modelFile.empty()) json["modelFile"] = config.modelFile;
        if (!config.modelDir.empty()) json["modelDir"] = config.modelDir;
        if (config.modelSelectionCleared) json["modelSelectionCleared"] = true;
        json["maxTasks"] = std::clamp(config.maxTasks, 1, 500);
        json["memoryLimitGb"] = std::clamp(config.memoryLimitGb, 1, 16);
        json["maxImageMegapixels"] =
            std::clamp(config.maxImageMegapixels, 4, 200);

        auto temporary = path;
        temporary += L".tmp";
        {
            std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
            if (!stream) {
                if (error) *error = "Cannot write config file";
                return false;
            }
            stream << json.dump(2) << '\n';
            stream.flush();
            if (!stream) {
                stream.close();
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
                if (error) *error = "Failed to write config file";
                return false;
            }
        }

        std::error_code renameError;
        std::filesystem::rename(temporary, path, renameError);
        if (renameError) {
            // Windows does not replace an existing destination atomically.
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            std::filesystem::rename(temporary, path, renameError);
            if (renameError) {
                std::filesystem::remove(temporary, ignored);
                if (error) {
                    *error = "Cannot replace config file: " +
                             renameError.message();
                }
                return false;
            }
        }
        return true;
    } catch (const std::exception& e) {
        if (error) *error = std::string("Failed to save config: ") + e.what();
        return false;
    }
}

} // namespace core
