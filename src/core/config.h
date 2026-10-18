#pragma once

#include <string>

#include "core/types.h"

namespace core {

struct UserConfig {
    Language language = Language::EnUS;
    bool languageSet = false;
    Backend backend = Backend::Auto;
    Theme theme = Theme::Dark;
    std::string modelFile; // UTF-8; empty = bundled model unless cleared
    std::string modelDir; // legacy folder selection
    bool modelSelectionCleared = false;
    int maxTasks = 100;
    int memoryLimitGb = 4;
};

bool loadUserConfig(UserConfig& config, std::string* error = nullptr);
bool saveUserConfig(const UserConfig& config, std::string* error = nullptr);

const char* backendKey(Backend backend);
const char* themeKey(Theme theme);
Backend backendFromCode(const std::string& code);
Theme themeFromCode(const std::string& code);

} // namespace core
