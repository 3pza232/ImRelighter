#pragma once

#include <array>
#include <string>
#include <vector>

namespace core {

using Vec3 = std::array<float, 3>;

enum class View { Lit, Compare, Original, Normal, Depth, Lighting, Shadow };
enum class Language { ZhCN, EnUS };
enum class Backend { Auto, Cpu, Dml };
enum class Theme { Dark, Light };
enum class ExportKind { Lit, Normal, Depth, Lighting, Shadow };

struct Light {
    int id = 1;
    std::string name = "Light 1";
    Vec3 position{-0.6f, 0.65f, 1.6f};
    std::string color = "#fff0d8";
    float intensity = 1.8f;
    float range = 3.0f;
    bool enabled = true;
};

struct Settings {
    float comparison = 0.5f;

    bool baseMode = false;
    float mix = 1.0f;

    float ambient = 0.42f;
    std::string ambientColor = "#b4bddc";
    float wrap = 0.2f;

    bool shadows = true;
    float bias = 0.007f;
    float softness = 0.035f;

    float specular = 0.13f;
    float shininess = 60.0f;
    float rim = 0.18f;

    float exposure = 0.0f;
    float height = 0.65f;

    bool invert = false;
    float smooth = 1.0f;
    bool lineAssist = true;
    float normalStrength = 1.0f;

    bool flipX = false;
    bool flipY = false;
    bool flipZ = false;

    float depthMin = 0.0f;
    float depthMax = 1.0f;
    bool autoDepthRange = true;
};

inline Settings defaults() {
    return Settings{};
}

inline Light makeLight(int id, Vec3 position = {-0.6f, 0.65f, 1.6f}) {
    Light light;
    light.id = id;
    light.name = "Light " + std::to_string(id);
    light.position = position;
    return light;
}

inline const char* viewKey(View view) {
    switch (view) {
        case View::Lit: return "view.lit";
        case View::Compare: return "view.compare";
        case View::Original: return "view.original";
        case View::Normal: return "view.normal";
        case View::Depth: return "view.depth";
        case View::Lighting: return "view.lighting";
        case View::Shadow: return "view.shadow";
    }
    return "view.lit";
}

} // namespace core
