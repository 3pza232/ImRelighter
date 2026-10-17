#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace io {

bool webpAvailable();
bool decodeWebp(const uint8_t* bytes, size_t size, int& width, int& height,
                std::vector<uint8_t>& rgba, std::string* error);

} // namespace io
