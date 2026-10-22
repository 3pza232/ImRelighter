#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace imrelighter {

struct ImageData {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;

    bool valid() const {
        return width > 0 && height > 0 &&
               rgba.size() == static_cast<size_t>(width) * height * 4;
    }
};

struct ModelInfo {
    std::string id;
    std::string name;
    std::string description;
    std::string asset_path;
    std::string display_path;
    std::string type;
    bool available = false;
};

struct DepthResult {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> depth_rgba;
    std::vector<uint8_t> normal_rgba;
    std::vector<float> normalized_depth;
    float depth_min = 0.0f;
    float depth_max = 0.0f;
    double inference_ms = 0.0;
    std::string model_id;
    uint64_t generation = 0;
};

enum class ViewMode {
    Original = 0,
    Lit = 1,
    Normal = 2,
    Depth = 3,
    Shadow = 4,
    Lighting = 5,
};

enum class LightType {
    Point = 0,
    Spot = 1,
};

enum class LightingModel {
    Classic = 0,
    Physical = 1,
};

enum class ToneMap {
    Soft = 0,
    Aces = 1,
};

struct Light {
    int id = 1;
    float position[3] = {-0.6f, 0.65f, 1.6f};
    float color[3] = {1.0f, 0.941f, 0.847f};
    float intensity = 1.8f;
    float range = 3.0f;
    bool enabled = true;
    LightType type = LightType::Point;
    float spot_angle = 35.0f;
    float spot_softness = 0.35f;

    bool operator==(const Light&) const = default;
};

struct LightingSettings {
    float ambient = 0.40f;
    float ambient_color[3] = {0.752941f, 0.768627f, 0.8f};
    float wrap_light = 0.20f;

    bool shadow_enabled = true;
    float shadow_bias = 0.007f;
    float shadow_softness = 0.035f;

    float specular = 0.04f;
    float shininess = 60.0f;
    float rim = 0.06f;
    float roughness = 0.30f;
    float metalness = 0.0f;
    float sheen = 0.0f;
    float subsurface = 0.0f;

    float exposure = 0.0f;
    float height = 0.65f;
    bool invert_depth = false;
    float smooth = 1.0f;
    bool line_assist = true;
    float normal_strength = 1.0f;
    float detail = 0.15f;

    bool flip_x = false;
    bool flip_y = false;
    bool flip_z = false;

    float depth_min = 0.0f;
    float depth_max = 1.0f;
    bool auto_depth_range = true;

    int shadow_steps = 96;
    int shadow_samples = 4;
    int shadow_divisor = 2;
    int preview_long_edge = 1536;

    LightingModel lighting_model = LightingModel::Physical;
    ToneMap tone_map = ToneMap::Soft;
    bool base_mode = false;
    float mix = 1.0f;

    bool operator==(const LightingSettings&) const = default;
};

}  // namespace imrelighter
