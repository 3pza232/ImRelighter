#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/types.h"

namespace core {

struct Document {
    uint64_t id = 0;
    std::filesystem::path path;
    std::string name;

    int width = 0;
    int height = 0;
    std::vector<uint8_t> color; // RGBA8, top-down
    std::vector<float> depth; // normalized 0..1, bottom-up (GL upload convention)
    uint64_t depthRevision = 0;
    uint64_t depthGeneration = 0;

    std::vector<uint8_t> thumbnail;
    int thumbWidth = 0;
    int thumbHeight = 0;

    Settings settings;
    std::vector<Light> lights{makeLight(1)};
    int selectedLight = 1;
    bool animate = false;
    View view = View::Original;

    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    bool fitPending = true;
    bool hasDepth = false;
};

} // namespace core
