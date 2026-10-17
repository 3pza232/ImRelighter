#include "core/i18n.h"

#include <fstream>
#include <windows.h>

#include "nlohmann/json.hpp"

namespace core {

namespace {

bool loadFile(const std::filesystem::path& path,
              std::unordered_map<std::string, std::string>& out,
              std::string* error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        if (error) *error = "Missing language file: " + path.string();
        return false;
    }

    try {
        nlohmann::json json;
        stream >> json;
        if (!json.is_object()) {
            if (error) *error = "Language file is not an object: " + path.string();
            return false;
        }
        for (auto it = json.begin(); it != json.end(); ++it) {
            if (it.value().is_string()) out[it.key()] = it.value().get<std::string>();
        }
        return true;
    } catch (const std::exception& e) {
        if (error) *error = std::string("Failed to parse ") + path.string() + ": " + e.what();
        return false;
    }
}

} // namespace

bool I18n::load(const std::filesystem::path& directory, std::string* error) {
    std::string firstError;
    const bool zhOk = loadFile(directory / "zh-CN.json", zh_, &firstError);
    const bool enOk = loadFile(directory / "en-US.json", en_, error ? error : &firstError);

    if (!zhOk && !enOk) {
        if (error && !firstError.empty()) *error = firstError;
        return false;
    }
    return true;
}

void I18n::setLanguage(Language language) {
    language_ = language;
}

Language I18n::systemDefault() {
    const LANGID lang = GetUserDefaultUILanguage();
    return PRIMARYLANGID(lang) == LANG_CHINESE ? Language::ZhCN : Language::EnUS;
}

const char* I18n::code(Language language) {
    return language == Language::ZhCN ? "zh-CN" : "en-US";
}

Language I18n::fromCode(std::string_view code) {
    return code == "zh-CN" ? Language::ZhCN : Language::EnUS;
}

const std::unordered_map<std::string, std::string>& I18n::active() const {
    if (language_ == Language::ZhCN && !zh_.empty()) return zh_;
    if (language_ == Language::EnUS && !en_.empty()) return en_;
    return !zh_.empty() ? zh_ : en_;
}

const std::unordered_map<std::string, std::string>& I18n::fallback() const {
    if (language_ == Language::ZhCN) return en_;
    return zh_;
}

std::string I18n::tr(std::string_view key) const {
    const std::string k(key);
    const auto& current = active();
    if (const auto it = current.find(k); it != current.end()) return it->second;

    const auto& other = fallback();
    if (const auto it = other.find(k); it != other.end()) return it->second;

    return k;
}

} // namespace core
