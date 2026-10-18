#pragma once

#include <filesystem>
#include <string>

namespace io {

std::string sha256Model(const std::filesystem::path& modelPath,
                        std::string* error = nullptr);

} // namespace io
