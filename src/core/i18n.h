#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/types.h"

namespace core {

class I18n {
public:
    bool load(const std::filesystem::path& directory, std::string* error = nullptr);

    void setLanguage(Language language);
    Language language() const { return language_; }

    std::string tr(std::string_view key) const;

    static Language systemDefault();
    static const char* code(Language language);
    static Language fromCode(std::string_view code);

private:
    const std::unordered_map<std::string, std::string>& active() const;
    const std::unordered_map<std::string, std::string>& fallback() const;

    std::unordered_map<std::string, std::string> zh_;
    std::unordered_map<std::string, std::string> en_;
    Language language_ = Language::EnUS;
};

} // namespace core
