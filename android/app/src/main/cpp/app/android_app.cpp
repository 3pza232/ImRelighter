#include "app/android_app.h"

#include <android/log.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "app/lighting_shader.h"
#include "imgui_impl_opengl3.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#define LOG_TAG "ImRelighterAndroid"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace imrelighter {
namespace {

constexpr const char* kFullscreenVertexShader = R"(#version 300 es
precision highp float;
out vec2 vUv;
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr const char* kLightingFragmentShader = R"(#version 300 es
precision highp float;
in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uAlbedo;
uniform sampler2D uNormal;
uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;
uniform float uAmbient;
uniform float uIntensity;
uniform float uExposure;
uniform float uNormalStrength;
uniform float uWrap;
uniform float uShadowStrength;
uniform float uShadowSoftness;
uniform float uRoughness;
uniform float uMetalness;
uniform float uSheen;
uniform int uMode;

void main() {
    vec4 albedo = texture(uAlbedo, vUv);
    vec3 albedo_linear = pow(max(albedo.rgb, vec3(0.0)), vec3(2.2));
    vec3 normal = normalize(texture(uNormal, vUv).rgb * 2.0 - 1.0);
    normal = normalize(mix(vec3(0.0, 0.0, 1.0), normal, uNormalStrength));
    vec3 light_dir = normalize(uLightDir);
    float ndl = max(dot(normal, light_dir), 0.0);
    float wrapped = clamp((dot(normal, light_dir) + uWrap) / (1.0 + uWrap), 0.0, 1.0);

    float contact = smoothstep(0.0, 1.0, ndl + uShadowSoftness * 0.35);
    float shadow = 1.0 - uShadowStrength * (1.0 - contact);
    shadow = clamp(shadow, 0.0, 1.0);

    vec3 half_dir = normalize(light_dir + vec3(0.0, 0.0, 1.0));
    float gloss = mix(96.0, 6.0, clamp(uRoughness, 0.0, 1.0));
    float specular = pow(max(dot(normal, half_dir), 0.0), gloss);
    specular *= mix(0.04, 1.0, clamp(uMetalness, 0.0, 1.0)) * (1.0 - uRoughness * 0.65);
    float rim = pow(1.0 - max(dot(normal, vec3(0.0, 0.0, 1.0)), 0.0), 2.0);

    vec3 ambient = pow(max(uAmbientColor, vec3(0.0)), vec3(2.2)) * uAmbient;
    vec3 light_color_linear = pow(max(uLightColor, vec3(0.0)), vec3(2.2));
    vec3 light = light_color_linear * uIntensity * wrapped * shadow;
    vec3 lit = albedo_linear * (ambient + light);
    lit += specular * light_color_linear * uIntensity * shadow;
    lit += rim * uSheen * light_color_linear * 0.22;
    lit *= exp2(uExposure);
    lit = lit / (lit + vec3(0.95));
    lit = pow(clamp(lit, 0.0, 1.0), vec3(1.0 / 2.2));

    if (uMode == 1) {
        float s = clamp(shadow, 0.0, 1.0);
        fragColor = vec4(vec3(s) * vec3(0.72, 0.80, 0.94), 1.0);
    } else if (uMode == 2) {
        vec3 only = ambient * 0.9 + light +
                    specular * light_color_linear * uIntensity * shadow;
        fragColor = vec4(pow(clamp(only, 0.0, 1.0), vec3(1.0 / 2.2)), 1.0);
    } else {
        fragColor = vec4(lit, 1.0);
    }
}
)";

GLuint compileShader(GLenum type, const char* source, std::string& error) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        error = "shader compile failed: " + log;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint linkProgram(const char* vertex_source, const char* fragment_source,
                   std::string& error) {
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertex_source, error);
    if (vertex == 0) {
        return 0;
    }
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragment_source, error);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::string log(static_cast<size_t>(std::max(length, 1)), '\0');
        glGetProgramInfoLog(program, length, nullptr, log.data());
        error = "program link failed: " + log;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

float scaleFactor(float density) {
    return std::max(density, 1.0f);
}

std::string localizedImportedName(const std::string& name) {
    if (ui::language() == ui::Language::English && name == "相册图片") {
        return "Gallery image";
    }
    return name;
}

std::string localizedStatus(const std::string& status) {
    if (ui::language() != ui::Language::English) {
        return status;
    }
    if (status == "请导入图片") return "Import an image";
    if (status == "深度与法线已生成") return "Depth and normals generated";
    if (status == "正在生成深度与法线…") return "Generating depth and normals…";
    if (status == "图片已导入，请点击“生成深度与法线”") {
        return "Image imported. Tap Generate Depth & Normals.";
    }
    if (status == "图片数据无效") return "Invalid image data";
    if (status == "所选模型不可用") return "Selected model is unavailable";
    if (status == "没有可导出的结果") return "Nothing to export";
    if (status == "导出失败：纹理不可用") return "Export failed: texture unavailable";
    if (status == "已导出") return "Exported";
    if (status == "自定义模型文件不可用") return "Custom model file is unavailable";
    if (status == "模型目录中没有可用的 ONNX 文件") {
        return "No usable ONNX file was found in the model folder";
    }
    if (status.rfind("已达到任务导入上限（", 0) == 0) {
        return "Task limit reached. Delete a task or raise the limit in Settings.";
    }
    if (status.rfind("已选择模型: ", 0) == 0) {
        return "Model selected. Generate depth to continue.";
    }
    const auto replace_prefix = [&](const char* chinese, const char* english) {
        const std::string prefix(chinese);
        if (status.rfind(prefix, 0) == 0) {
            return std::string(english) + status.substr(prefix.size());
        }
        return std::string();
    };
    for (const auto& pair : {
             std::pair<const char*, const char*>{"图片打开失败: ", "Image open failed: "},
             {"纹理上传失败: ", "Texture upload failed: "},
             {"原图纹理上传失败: ", "Original texture upload failed: "},
             {"推理失败: ", "Inference failed: "},
             {"未知模型: ", "Unknown model: "},
             {"模型未内置: ", "Model is not available: "},
             {"已选择模型: ", "Model selected: "}}) {
        const std::string replaced = replace_prefix(pair.first, pair.second);
        if (!replaced.empty()) {
            return replaced;
        }
    }
    return status;
}

std::string localizedModelType(const std::string& type) {
    if (type.empty() || type == "pending") {
        return ui::tr("检查中…", "Checking…");
    }
    if (type == "q4") {
        return ui::tr("Q4 · 4-bit 权重 · FP32 激活",
                      "Q4 · 4-bit weights · FP32 activations");
    }
    if (type == "q4f16") {
        return ui::tr("Q4F16 · 4-bit 权重 · FP16 激活",
                      "Q4F16 · 4-bit weights · FP16 activations");
    }
    if (type == "fp16") return "FP16";
    if (type == "fp32") return "FP32";
    if (type == "bf16") return "BF16";
    if (type == "int8") return "INT8";
    if (type == "bnb4") return "BNB4";
    return ui::tr("未知类型", "Unknown type");
}

std::string localizedModelDescription(const ModelInfo& model) {
    if (model.id == ModelCatalog::kDefaultId) {
        return ui::tr("4-bit 权重 · FP32 激活 · 推荐",
                      "4-bit weights · FP32 activations · recommended");
    }
    if (model.id.rfind("file:", 0) == 0) {
        const std::string filename =
            std::filesystem::path(model.display_path).filename().string();
        return localizedModelType(model.type) +
               (filename.empty() ? "" : " · " + filename);
    }
    return model.description;
}

ImU32 contrastTextFor(const float rgb[3], float alpha = 1.0f) {
    const float luma = 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2];
    if (luma > 0.58f) {
        return IM_COL32(24, 26, 30, static_cast<int>(alpha * 255.0f));
    }
    return IM_COL32(245, 245, 245, static_cast<int>(alpha * 255.0f));
}

}  // namespace

AndroidApp::AndroidApp(AAssetManager* assets, std::string files_dir, float density)
    : assets_(assets), files_dir_(std::move(files_dir)), density_(density),
      last_frame_time_(std::chrono::steady_clock::now()) {}

AndroidApp::~AndroidApp() {
    shutdown();
}

bool AndroidApp::initialize(std::string& error) {
    if (initialized_) {
        return true;
    }
    if (assets_ == nullptr) {
        error = "AssetManager is null";
        return false;
    }
    try {
        // Task documents are intentionally session-only. Remove the legacy
        // serialized task store so a process restart always starts clean.
        {
            std::error_code ignored;
            std::filesystem::remove(
                std::filesystem::path(files_dir_) / "tasks.bin", ignored);
        }
        catalog_ = std::make_unique<ModelCatalog>(assets_);
        selected_model_ = catalog_->load_selection(files_dir_);
        if (!catalog_->available(selected_model_)) {
            selected_model_ = ModelCatalog::kDefaultId;
        }
        inference_ = std::make_unique<ml::InferenceService>(assets_);
        inference_->start();
        lights_.clear();
        Light default_light;
        default_light.id = 1;
        lights_.push_back(default_light);
        if (!initialize_imgui(error)) {
            return false;
        }
        if (!initialize_lighting(error)) {
            return false;
        }
        {
            std::ifstream theme_file(std::filesystem::path(files_dir_) / "theme.txt");
            int theme = 0;
            if (theme_file >> theme && theme == 1) {
                ui::setThemeMode(ui::ThemeMode::Light);
            }
            std::ifstream language_file(std::filesystem::path(files_dir_) / "language.txt");
            int language = 0;
            if (language_file >> language && language == 1) {
                ui::setLanguage(ui::Language::English);
            }
            std::ifstream task_limit_file(std::filesystem::path(files_dir_) / "task_limit.txt");
            int task_limit = 8;
            if (task_limit_file >> task_limit) {
                max_tasks_ = std::clamp(task_limit, 2, 16);
            }
        }
        initialized_ = true;
        model_storage_label_ = ui::tr("应用内部存储", "App storage");
        set_status(ui::tr("请导入图片", "Import an image"));
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        LOGE("initialize failed: %s", error.c_str());
        return false;
    }
}

void AndroidApp::shutdown() {
    if (!initialized_) {
        return;
    }
    if (inference_) {
        inference_->stop();
        inference_.reset();
    }
    for (Task& task : tasks_) {
        delete_texture(task.thumbnail_texture);
    }
    tasks_.clear();
    active_task_ = -1;
    delete_texture(original_texture_);
    delete_texture(depth_texture_);
    delete_texture(normal_texture_);
    delete_texture(lit_texture_);
    if (lit_fbo_ != 0) {
        glDeleteFramebuffers(1, &lit_fbo_);
        lit_fbo_ = 0;
    }
    if (lit_program_ != 0) {
        glDeleteProgram(lit_program_);
        lit_program_ = 0;
    }
    if (lit_vao_ != 0) {
        glDeleteVertexArrays(1, &lit_vao_);
        lit_vao_ = 0;
    }
    if (ImGui::GetCurrentContext() != nullptr) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
    }
    catalog_.reset();
    initialized_ = false;
}

void AndroidApp::resize(int width, int height) {
    surface_width_ = std::max(width, 1);
    surface_height_ = std::max(height, 1);
}

void AndroidApp::render() {
    if (!initialized_) {
        return;
    }
    // Texture deletion must not happen while the previous frame's draw lists
    // still reference a task thumbnail. Consume this at the start of the next
    // frame, after the previous RenderDrawData has completed.
    if (pending_delete_task_ >= 0) {
        const int index = pending_delete_task_;
        pending_delete_task_ = -1;
        delete_task(index);
    }
    process_queues();
    poll_inference();

    ImGui_ImplOpenGL3_NewFrame();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(surface_width_),
                            static_cast<float>(surface_height_));
    const auto now = std::chrono::steady_clock::now();
    const double delta = std::chrono::duration<double>(now - last_frame_time_).count();
    last_frame_time_ = now;
    io.DeltaTime = static_cast<float>(std::clamp(delta, 0.0, 0.1));
    ImGui::NewFrame();
    draw_ui(scaleFactor(density_));
    ImGui::Render();

    glViewport(0, 0, surface_width_, surface_height_);
    glClearColor(ui::palette().bg.x, ui::palette().bg.y, ui::palette().bg.z, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void AndroidApp::queue_touch(int action, int pointer_count, float x0, float y0,
                             float x1, float y1) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    touch_events_.push_back({action, pointer_count, x0, y0, x1, y1});
}

void AndroidApp::queue_image(std::vector<uint8_t> bytes) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_image_ = std::move(bytes);
    has_pending_image_ = true;
}

void AndroidApp::queue_image_rgba(std::vector<uint8_t> rgba, int width, int height) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_image_rgba_ = std::move(rgba);
    pending_image_width_ = width;
    pending_image_height_ = height;
    has_pending_image_rgba_ = true;
    has_pending_image_ = false;
    pending_image_.clear();
}

void AndroidApp::queue_model(const std::string& id) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_model_ = id;
    pending_model_file_.clear();
    pending_model_files_.clear();
    pending_reset_ = false;
}

void AndroidApp::queue_model_file(const std::string& absolute_path) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_model_file_ = absolute_path;
    pending_model_files_.clear();
    pending_model_.clear();
    pending_reset_ = false;
}

void AndroidApp::queue_model_files(std::vector<std::string> absolute_paths) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_model_files_ = std::move(absolute_paths);
    pending_model_file_.clear();
    pending_model_.clear();
    pending_reset_ = false;
}

void AndroidApp::set_model_storage_label(const std::string& label) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_model_storage_label_ = label;
}

void AndroidApp::set_system_insets(int top, int bottom) {
    system_inset_top_.store(std::max(0, top));
    system_inset_bottom_.store(std::max(0, bottom));
}

void AndroidApp::queue_reset_model() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    pending_reset_ = true;
    pending_model_.clear();
}

bool AndroidApp::handle_back() {
    if (show_settings_) {
        show_settings_ = false;
        return true;
    }
    if (task_drawer_open_) {
        task_drawer_open_ = false;
        return true;
    }
    return false;
}

void AndroidApp::set_open_image_callback(std::function<void()> callback) {
    open_image_callback_ = std::move(callback);
}

void AndroidApp::set_model_file_callback(std::function<void()> callback) {
    model_file_callback_ = std::move(callback);
}

void AndroidApp::set_model_folder_callback(std::function<void()> callback) {
    model_folder_callback_ = std::move(callback);
}

void AndroidApp::set_export_callback(
    std::function<void(const std::vector<uint8_t>&, int, int, const std::string&)>
        callback) {
    export_callback_ = std::move(callback);
}

void AndroidApp::process_queues() {
    std::vector<TouchEvent> touches;
    std::vector<uint8_t> image_bytes;
    std::vector<uint8_t> image_rgba;
    int image_width = 0;
    int image_height = 0;
    bool has_image_rgba = false;
    std::string model;
    std::string model_file;
    std::vector<std::string> model_files;
    std::string model_storage_label;
    bool reset = false;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        touches.swap(touch_events_);
        if (has_pending_image_) {
            image_bytes = std::move(pending_image_);
            pending_image_.clear();
            has_pending_image_ = false;
        }
        if (has_pending_image_rgba_) {
            image_rgba = std::move(pending_image_rgba_);
            image_width = pending_image_width_;
            image_height = pending_image_height_;
            has_image_rgba = true;
            pending_image_rgba_.clear();
            pending_image_width_ = 0;
            pending_image_height_ = 0;
            has_pending_image_rgba_ = false;
        }
        model = pending_model_;
        pending_model_.clear();
        model_file = pending_model_file_;
        pending_model_file_.clear();
        model_files = std::move(pending_model_files_);
        pending_model_files_.clear();
        model_storage_label = pending_model_storage_label_;
        pending_model_storage_label_.clear();
        reset = pending_reset_;
        pending_reset_ = false;
    }

    if (!model_storage_label.empty()) {
        model_storage_label_ = model_storage_label;
    }

    if (ImGui::GetCurrentContext() != nullptr) {
        ImGuiIO& io = ImGui::GetIO();
        if (pending_lower_release_) {
            io.AddMouseButtonEvent(0, false);
            pending_lower_release_ = false;
        }
        if (pending_lower_click_) {
            io.AddMousePosEvent(pending_click_x_, pending_click_y_);
            io.AddMouseButtonEvent(0, true);
            pending_lower_click_ = false;
            pending_lower_release_ = true;
        }
        if (pending_settings_release_) {
            io.AddMouseButtonEvent(0, false);
            pending_settings_release_ = false;
        }
        if (pending_settings_click_) {
            io.AddMousePosEvent(settings_click_x_, settings_click_y_);
            io.AddMouseButtonEvent(0, true);
            pending_settings_click_ = false;
            pending_settings_release_ = true;
        }
        const float touch_slop = 8.0f * scaleFactor(density_);
        for (const TouchEvent& touch : touches) {
            const float x = touch.x0;
            const float y = touch.y0;
            const auto touch_now = std::chrono::steady_clock::now();

            if (touch.pointer_count >= 2) {
                if (!touch_pinching_) {
                    io.AddMouseButtonEvent(0, false);
                    touch_pinching_ = true;
                    pinch_last_distance_ = 0.0f;
                }
                const float dx = touch.x1 - touch.x0;
                const float dy = touch.y1 - touch.y0;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (pinch_last_distance_ > 1.0f && distance > 1.0f) {
                    canvas_zoom_ = std::clamp(
                        canvas_zoom_ * (distance / pinch_last_distance_), 0.15f, 6.0f);
                    fit_pending_ = false;
                }
                pinch_last_distance_ = distance;
                if (touch.action == 2) {
                    touch_pinching_ = false;
                    pinch_last_distance_ = 0.0f;
                }
                continue;
            }

            if (touch_pinching_) {
                if (touch.action == 2) {
                    touch_pinching_ = false;
                    pinch_last_distance_ = 0.0f;
                }
                continue;
            }

            if (touch.action == 1 && model_press_candidate_ &&
                (std::fabs(x - model_press_pos_.x) > touch_slop ||
                 std::fabs(y - model_press_pos_.y) > touch_slop)) {
                model_press_candidate_ = false;
            }

            const bool in_drawer =
                task_drawer_open_ && !show_settings_ &&
                task_list_max_.y > task_list_min_.y &&
                x >= task_list_min_.x && x <= task_list_max_.x &&
                y >= task_list_min_.y && y <= task_list_max_.y;
            if (in_drawer) {
                if (touch.action == 0) {
                    ui::cancelActiveControl();
                    drawer_touch_started_ = true;
                    drawer_touch_scrolling_ = false;
                    drawer_scroll_velocity_ = 0.0f;
                    drawer_touch_start_y_ = y;
                    drawer_touch_start_scroll_ = drawer_scroll_;
                } else if (touch.action == 1 && drawer_touch_started_) {
                    if (!drawer_touch_scrolling_ &&
                        std::fabs(y - drawer_touch_start_y_) > touch_slop) {
                        drawer_touch_scrolling_ = true;
                        ui::cancelActiveControl();
                    }
                    if (drawer_touch_scrolling_) {
                        drawer_scroll_ = std::clamp(
                            drawer_touch_start_scroll_ - (y - drawer_touch_start_y_),
                            0.0f, std::max(0.0f, drawer_scroll_max_));
                    }
                } else if (touch.action == 2) {
                    if (!drawer_touch_scrolling_) {
                        pending_click_x_ = x;
                        pending_click_y_ = y;
                        pending_lower_click_ = true;
                    }
                    drawer_touch_started_ = false;
                    drawer_touch_scrolling_ = false;
                }
                continue;
            }
            drawer_touch_started_ = false;
            drawer_touch_scrolling_ = false;

            const bool in_settings =
                show_settings_ && settings_max_.y > settings_min_.y &&
                x >= settings_min_.x && x <= settings_max_.x &&
                y >= settings_min_.y && y <= settings_max_.y;
            const bool in_model_list =
                in_settings && settings_tab_ == 1 &&
                model_list_max_.y > model_list_min_.y &&
                x >= model_list_min_.x && x <= model_list_max_.x &&
                y >= model_list_min_.y && y <= model_list_max_.y;
            const bool in_lower_panel =
                !show_settings_ && lower_panel_max_.y > lower_panel_min_.y &&
                x >= lower_panel_min_.x && x <= lower_panel_max_.x &&
                y >= lower_panel_min_.y && y <= lower_panel_max_.y;

            // While a modal is open, all touch input belongs to the modal.
            // Touches on the dimmed backdrop must not scroll the page behind.
            if (show_settings_ && !in_settings) {
                if (touch.action == 0) {
                    settings_touch_started_ = false;
                    settings_touch_scrolling_ = false;
                    model_press_candidate_ = false;
                    touch_gesture_cancelled_ = false;
                } else if (touch.action == 1 && settings_touch_started_) {
                    settings_touch_scrolling_ = false;
                    settings_touch_started_ = false;
                    touch_gesture_cancelled_ = true;
                } else if (touch.action == 2) {
                    if (!touch_gesture_cancelled_) {
                        show_settings_ = false;
                    }
                    settings_touch_started_ = false;
                    settings_touch_scrolling_ = false;
                    touch_gesture_cancelled_ = false;
                }
                continue;
            }

            const bool in_canvas =
                image_.valid() && canvas_image_max_.x > canvas_image_min_.x &&
                x >= canvas_image_min_.x && x <= canvas_image_max_.x &&
                y >= canvas_image_min_.y && y <= canvas_image_max_.y;
            const bool direct_control =
                ui::touchHitsTarget(x, y) || (!show_settings_ && in_canvas);

            if (touch.action == 0) {
                model_press_candidate_ = false;
                model_long_press_triggered_ = false;
                if (show_settings_ && settings_tab_ == 1) {
                    for (const ModelRowHit& row : model_rows_) {
                        if (row.id == ModelCatalog::kDefaultId) {
                            continue;
                        }
                        if (x >= row.rect.x && x <= row.rect.z && y >= row.rect.y &&
                            y <= row.rect.w) {
                            model_press_candidate_ = true;
                            model_press_id_ = row.id;
                            model_press_pos_ = ImVec2(x, y);
                            model_press_time_ = touch_now;
                            break;
                        }
                    }
                }
                touch_direct_control_ = direct_control;
                if (!direct_control) {
                    ui::cancelActiveControl();
                }
                touch_gesture_cancelled_ = false;
                touch_scrolling_ = false;
                lower_scroll_velocity_ = 0.0f;
                settings_scroll_velocity_ = 0.0f;
                touch_start_time_ = touch_now;
                touch_last_move_time_ = touch_now;
                touch_started_in_lower_ = in_lower_panel;
                touch_start_y_ = y;
                touch_start_scroll_ = lower_scroll_;
                settings_touch_scrolling_ = false;
                settings_touch_started_ = in_settings;
                settings_touch_start_y_ = y;
                settings_touch_start_scroll_ = settings_scroll_;
                model_list_touch_started_ = in_model_list;
                model_list_touch_scrolling_ = false;
                model_list_touch_start_y_ = y;
                model_list_touch_start_scroll_ = model_list_scroll_;
                settings_touch_start_time_ = touch_now;
                settings_touch_last_move_time_ = touch_now;
                if (direct_control) {
                    io.AddMousePosEvent(x, y);
                    io.AddMouseButtonEvent(0, true);
                }
            } else if (touch.action == 1) {
                if (touch_gesture_cancelled_) {
                    continue;
                }
                if (touch_direct_control_) {
                    io.AddMousePosEvent(x, y);
                } else if (model_list_touch_started_) {
                    if (!model_list_touch_scrolling_ &&
                        std::fabs(y - model_list_touch_start_y_) > touch_slop) {
                        model_list_touch_scrolling_ = true;
                        ui::cancelActiveControl();
                    }
                    if (model_list_touch_scrolling_) {
                        model_list_scroll_ = std::clamp(
                            model_list_touch_start_scroll_ -
                                (y - model_list_touch_start_y_),
                            0.0f, std::max(0.0f, model_list_scroll_max_));
                        model_list_scroll_dirty_ = true;
                    }
                    continue;
                } else if (settings_touch_started_ && in_settings) {
                    if (!settings_touch_scrolling_ &&
                        std::fabs(y - settings_touch_start_y_) > touch_slop) {
                        settings_touch_scrolling_ = true;
                        ui::cancelActiveControl();
                    }
                    if (settings_touch_scrolling_) {
                        settings_scroll_ = std::clamp(
                            settings_touch_start_scroll_ -
                                (y - settings_touch_start_y_),
                            0.0f, std::max(0.0f, settings_scroll_max_));
                        settings_touch_last_move_time_ = touch_now;
                    }
                } else if (touch_started_in_lower_) {
                    // Keep ownership of a lower-panel drag after the finger
                    // crosses into the canvas. The gesture is released only
                    // on ACTION_UP, so a diagonal swipe remains continuous.
                    if (!touch_scrolling_ &&
                        std::fabs(y - touch_start_y_) > touch_slop) {
                        touch_scrolling_ = true;
                        ui::cancelActiveControl();
                    }
                    if (touch_scrolling_) {
                        lower_scroll_ = std::clamp(
                            touch_start_scroll_ - (y - touch_start_y_), 0.0f,
                            std::max(0.0f, lower_scroll_max_));
                        touch_last_move_time_ = touch_now;
                    }
                }
            } else if (touch.action == 2) {
                if (touch_gesture_cancelled_) {
                    touch_gesture_cancelled_ = false;
                    touch_direct_control_ = false;
                    touch_scrolling_ = false;
                    touch_started_in_lower_ = false;
                    settings_touch_started_ = false;
                    settings_touch_scrolling_ = false;
                    model_list_touch_started_ = false;
                    model_list_touch_scrolling_ = false;
                    lower_scroll_velocity_ = 0.0f;
                    settings_scroll_velocity_ = 0.0f;
                    continue;
                }
                if (model_long_press_triggered_) {
                    model_long_press_triggered_ = false;
                    model_press_candidate_ = false;
                    model_list_touch_started_ = false;
                    model_list_touch_scrolling_ = false;
                    settings_touch_started_ = false;
                    settings_touch_scrolling_ = false;
                    touch_direct_control_ = false;
                    continue;
                }
                model_press_candidate_ = false;
                if (model_list_touch_scrolling_) {
                    model_list_touch_started_ = false;
                    model_list_touch_scrolling_ = false;
                    settings_touch_started_ = false;
                    settings_touch_scrolling_ = false;
                    touch_direct_control_ = false;
                    continue;
                }
                model_list_touch_started_ = false;
                if (touch_direct_control_) {
                    io.AddMousePosEvent(x, y);
                    io.AddMouseButtonEvent(0, false);
                } else if (show_settings_ && !in_settings) {
                    show_settings_ = false;
                } else if (!touch_scrolling_ && !settings_touch_scrolling_ &&
                           (!touch_started_in_lower_ || in_lower_panel)) {
                    // ImGui needs a press and a release on consecutive frames
                    // for a clicked item. Defer the tap instead of injecting
                    // both events into the same frame.
                    pending_click_x_ = x;
                    pending_click_y_ = y;
                    pending_lower_click_ = true;
                }
                const auto release_now = std::chrono::steady_clock::now();
                const double idle_seconds =
                    std::chrono::duration<double>(release_now - touch_last_move_time_).count();
                if (touch_scrolling_ && idle_seconds < 0.12) {
                    const double elapsed = std::max(
                        0.008, std::chrono::duration<double>(
                                   release_now - touch_start_time_).count());
                    lower_scroll_velocity_ = std::clamp(
                        static_cast<float>((lower_scroll_ - touch_start_scroll_) / elapsed),
                        -1800.0f, 1800.0f);
                } else {
                    lower_scroll_velocity_ = 0.0f;
                }
                if (settings_touch_scrolling_ &&
                    std::chrono::duration<double>(release_now - settings_touch_last_move_time_)
                            .count() < 0.12) {
                    const double elapsed = std::max(
                        0.008, std::chrono::duration<double>(
                                   release_now - settings_touch_start_time_).count());
                    settings_scroll_velocity_ = std::clamp(
                        static_cast<float>(
                            (settings_scroll_ - settings_touch_start_scroll_) / elapsed),
                        -1800.0f, 1800.0f);
                } else {
                    settings_scroll_velocity_ = 0.0f;
                }
                touch_direct_control_ = false;
                touch_scrolling_ = false;
                touch_started_in_lower_ = false;
                settings_touch_started_ = false;
                settings_touch_scrolling_ = false;
            }
        }
    }

    if (!image_bytes.empty()) {
        ImageData image;
        std::string error;
        if (io::decodeImage(image_bytes.data(), image_bytes.size(), image, error)) {
            set_image(image, ui::tr("相册图片", "Gallery image"));
        } else {
            set_status(ui::tr("图片打开失败: ", "Image open failed: ") + error);
        }
    }
    if (has_image_rgba && image_width > 0 && image_height > 0 &&
        image_rgba.size() == static_cast<size_t>(image_width) * image_height * 4) {
        ImageData image;
        image.width = image_width;
        image.height = image_height;
        image.rgba = std::move(image_rgba);
        set_image(image, ui::tr("相册图片", "Gallery image"));
    }
    if (!model.empty()) {
        select_model(model);
    }
    if (reset) {
        reset_model();
    }
    if (!model_file.empty()) {
        const std::string custom_id =
            catalog_ ? catalog_->register_custom_model(files_dir_, model_file)
                     : std::string();
        if (!custom_id.empty()) {
            select_model(custom_id);
        } else {
            set_status(ui::tr("自定义模型文件不可用", "Custom model file is unavailable"));
        }
    }
    if (!model_files.empty()) {
        int imported = 0;
        for (const std::string& path : model_files) {
            if (!catalog_) {
                break;
            }
            if (!catalog_->register_custom_model(files_dir_, path).empty()) {
                ++imported;
            }
        }
        if (imported > 0) {
            // Importing a folder must not replace the user's current model.
            model_list_scroll_to_end_ = true;
            set_status(ui::tr("已导入 ", "Imported ") + std::to_string(imported) +
                       ui::tr(" 个模型，当前选择保持不变",
                              " model(s); current selection unchanged"));
        } else {
            set_status(ui::tr("模型目录中没有可用的 ONNX 文件",
                              "No usable ONNX file was found in the model folder"));
        }
    }
}

void AndroidApp::poll_inference() {
    if (!inference_) {
        return;
    }
    DepthResult result;
    if (inference_->take_result(result)) {
        for (int i = 0; i < static_cast<int>(tasks_.size()); ++i) {
            Task& task = tasks_[static_cast<size_t>(i)];
            if (task.generation != result.generation) {
                continue;
            }
            task.depth = std::move(result);
            task.depth_ready = true;
            task.busy = false;
            if (i == active_task_) {
                depth_ = task.depth;
                depth_ready_ = true;
                std::string error;
                bool uploaded = upload_texture(depth_texture_, depth_.width,
                                                depth_.height, depth_.depth_rgba,
                                                false, error);
                if (uploaded) {
                    uploaded = upload_texture(normal_texture_, depth_.width,
                                              depth_.height, depth_.normal_rgba,
                                              false, error);
                }
                busy_ = false;
                if (!uploaded) {
                    set_status(ui::tr("纹理上传失败: ", "Texture upload failed: ") + error);
                } else {
                    if (view_ == ViewMode::Original) {
                        // The first generated result is shown immediately in
                        // the relit view; an explicit other view is kept.
                        view_ = ViewMode::Lit;
                        task.view = ViewMode::Lit;
                    }
                    set_status(ui::tr("深度与法线已生成", "Depth and normals generated"));
                }
            }
            break;
        }
    }
    std::string error;
    uint64_t error_generation = 0;
    if (inference_->take_error(error, error_generation)) {
        bool matched = false;
        for (Task& task : tasks_) {
            if (task.generation == error_generation) {
                task.busy = false;
                matched = true;
                if (active_task_ >= 0 &&
                    &task == &tasks_[static_cast<size_t>(active_task_)]) {
                    busy_ = false;
                    set_status(ui::tr("推理失败: ", "Inference failed: ") + error);
                }
                break;
            }
        }
        if (!matched && error_generation == generation_) {
            busy_ = false;
            set_status(ui::tr("推理失败: ", "Inference failed: ") + error);
        }
    }
}

void AndroidApp::apply_light_preset(int preset) {
    light_preset_ = std::clamp(preset, 0, 2);
    lights_.clear();
    auto add = [this](float x, float y, float z, float r, float g, float b,
                      float intensity, LightType type = LightType::Point,
                      float angle = 35.0f, float softness = 0.35f) {
        Light light;
        light.id = static_cast<int>(lights_.size()) + 1;
        light.position[0] = x;
        light.position[1] = y;
        light.position[2] = z;
        light.color[0] = r;
        light.color[1] = g;
        light.color[2] = b;
        light.intensity = intensity;
        light.range = 4.0f;
        light.type = type;
        light.spot_angle = angle;
        light.spot_softness = softness;
        lights_.push_back(light);
    };
    if (light_preset_ == 0) {
        add(-1.15f, 0.85f, 1.9f, 1.00f, 0.72f, 0.42f, 1.55f);
        add(0.95f, 0.20f, 2.3f, 1.00f, 0.48f, 0.28f, 1.05f);
        add(0.10f, 1.10f, 2.6f, 1.00f, 0.88f, 0.62f, 0.75f);
        lighting_.ambient = 0.24f;
        lighting_.ambient_color[0] = 0.34f;
        lighting_.ambient_color[1] = 0.40f;
        lighting_.ambient_color[2] = 0.54f;
    } else if (light_preset_ == 1) {
        add(-1.10f, 0.65f, 1.8f, 0.46f, 0.62f, 1.00f, 1.05f);
        add(0.85f, 0.55f, 2.2f, 0.30f, 0.48f, 1.00f, 0.80f);
        add(0.00f, 1.00f, 2.5f, 0.72f, 0.80f, 1.00f, 0.55f);
        lighting_.ambient = 0.16f;
        lighting_.ambient_color[0] = 0.18f;
        lighting_.ambient_color[1] = 0.26f;
        lighting_.ambient_color[2] = 0.46f;
    } else {
        add(-1.20f, 1.05f, 2.1f, 1.00f, 0.70f, 0.38f, 1.35f,
            LightType::Spot, 58.0f, 0.45f);
        add(1.20f, 0.95f, 2.1f, 0.38f, 0.68f, 1.00f, 1.15f,
            LightType::Spot, 52.0f, 0.50f);
        add(-0.65f, 0.20f, 2.6f, 1.00f, 0.42f, 0.28f, 0.70f);
        add(0.65f, 0.20f, 2.6f, 0.42f, 0.72f, 1.00f, 0.70f);
        lighting_.ambient = 0.13f;
        lighting_.ambient_color[0] = 0.22f;
        lighting_.ambient_color[1] = 0.25f;
        lighting_.ambient_color[2] = 0.34f;
    }
    selected_light_ = 0;
}

void AndroidApp::apply_material_preset(int preset) {
    material_preset_ = std::clamp(preset, 0, 4);
    switch (material_preset_) {
        case 0:
            lighting_.roughness = 0.30f;
            lighting_.metalness = 0.0f;
            lighting_.sheen = 0.0f;
            break;
        case 1:
            lighting_.roughness = 0.55f;
            lighting_.metalness = 0.0f;
            lighting_.sheen = 0.06f;
            break;
        case 2:
            lighting_.roughness = 0.82f;
            lighting_.metalness = 0.0f;
            lighting_.sheen = 0.35f;
            break;
        case 3:
            lighting_.roughness = 0.42f;
            lighting_.metalness = 0.18f;
            lighting_.sheen = 0.12f;
            break;
        default:
            lighting_.roughness = 0.22f;
            lighting_.metalness = 0.85f;
            lighting_.sheen = 0.04f;
            break;
    }
}

void AndroidApp::update_model_long_press() {
    if (!model_press_candidate_ || !show_settings_ || settings_tab_ != 1 ||
        touch_scrolling_ || settings_touch_scrolling_ ||
        model_list_touch_scrolling_) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration<double>(now - model_press_time_).count() < 0.55) {
        return;
    }
    model_press_candidate_ = false;
    model_long_press_triggered_ = true;
    model_delete_target_ = model_press_id_;
    model_delete_popup_requested_ = true;
}

void AndroidApp::draw_ui(float s) {
    ui::clearTouchTargets();
    update_model_long_press();
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("##imrelighter_root", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(2);

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float inset_top = static_cast<float>(system_inset_top_.load());
    const float inset_bottom = static_cast<float>(system_inset_bottom_.load());
    const float top_height = 58.0f * s;
    const float bottom_height = 52.0f * s;
    const float body_height = std::max(
        0.0f, display.y - inset_top - top_height - inset_bottom - bottom_height);

    ImGui::SetCursorPos(ImVec2(0.0f, inset_top));
    draw_top_bar(s);

    ImGui::SetCursorPos(ImVec2(0.0f, inset_top + top_height));
    ImGui::BeginChild("##body", ImVec2(0.0f, body_height), 0,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    draw_task_rail(s);
    ImGui::SameLine(0.0f, 0.0f);
    draw_workspace(s);
    if (task_drawer_open_) {
        ImGui::SetCursorPos(ImVec2(44.0f * s, 0.0f));
        draw_task_drawer(s);
        if (!show_settings_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            if (mouse.x > 280.0f * s) {
                task_drawer_open_ = false;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SetCursorPos(ImVec2(0.0f, display.y - inset_bottom - bottom_height));
    draw_bottom_bar(s);

    draw_export_menu(s);
    if (show_settings_) {
        // The inspector is still submitted behind the modal; do not let its
        // slider hit targets capture touches intended for the settings list.
        ui::clearTouchTargets();
    }
    draw_settings(s);

    if (busy_) {
        ImGui::GetForegroundDrawList()->AddRectFilled(
            ImVec2(display.x * 0.5f - 118.0f * s,
                   display.y - inset_bottom - bottom_height - 66.0f * s),
            ImVec2(display.x * 0.5f + 118.0f * s,
                   display.y - inset_bottom - bottom_height - 26.0f * s),
            IM_COL32(22, 26, 34, 232), 18.0f * s);
        const char* text = ui::tr("正在生成深度与法线…", "Generating depth and normals…");
        const ImVec2 text_size = ImGui::CalcTextSize(text);
        ImGui::GetForegroundDrawList()->AddText(
            ImVec2(display.x * 0.5f - text_size.x * 0.5f,
                   display.y - inset_bottom - bottom_height - 46.0f * s -
                       text_size.y * 0.5f),
            IM_COL32(232, 179, 106, 255), text);
    }

    ImGui::End();
}

void AndroidApp::draw_top_bar(float s) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().panel);
    ImGui::BeginChild("##topbar", ImVec2(0.0f, 58.0f * s), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const float text_y = (58.0f * s - ImGui::GetTextLineHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(16.0f * s, text_y));
    ImGui::TextColored(ui::palette().accent, "IMRELIGHTER");

    const float buttons_height = 40.0f * s;
    const float export_width = 88.0f * s;
    const float settings_width = 64.0f * s;
    const float button_y = (58.0f * s - buttons_height) * 0.5f;
    const float right = ImGui::GetWindowWidth() - 12.0f * s;
    ImGui::SetCursorPos(ImVec2(right - export_width - settings_width - 8.0f * s, button_y));
    {
        ui::DisabledScope disabled(!depth_ready_ || export_busy_);
        if (ui::centeredButton(ui::tr("导出图片", "Export"), ImVec2(export_width, buttons_height))) {
            export_popup_open_ = true;
        }
    }
    ImGui::SameLine(0.0f, 8.0f * s);
    if (ui::centeredButton(ui::tr("设置", "Settings"), ImVec2(settings_width, buttons_height))) {
        show_settings_ = true;
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AndroidApp::draw_task_rail(float s) {
    constexpr float kRailWidth = 44.0f;
    constexpr float kRailButton = 38.0f;
    const float button_x = (kRailWidth - kRailButton) * 0.5f * s;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().panel);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##task_rail", ImVec2(kRailWidth * s, 0.0f),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(button_x, 8.0f * s));
    if (ui::iconButton("##drawer_toggle", task_drawer_open_ ? "«" : "»",
                       ImVec2(kRailButton * s, kRailButton * s))) {
        task_drawer_open_ = !task_drawer_open_;
    }
    const float center_x =
        (ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f;
    {
        ui::FontScale small(0.78f);
        const ImVec2 origin = ImGui::GetWindowPos();
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 muted = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        const char* label = ui::tr("任\n务", "Tasks");
        const float label_width = ImGui::CalcTextSize(label).x;
        draw->AddText(ImVec2(center_x - label_width * 0.5f,
                             origin.y + 58.0f * s),
                      muted, label);
        const std::string count_text = std::to_string(tasks_.size());
        const char* count = count_text.c_str();
        const float count_width = ImGui::CalcTextSize(count).x;
        draw->AddText(ImVec2(center_x - count_width * 0.5f,
                             origin.y + 112.0f * s),
                      muted, count);
    }

    const float bottom = ImGui::GetWindowHeight() - 52.0f * s;
    ImGui::SetCursorPos(ImVec2(button_x, bottom));
    if (ui::iconButton("##rail_import", "+",
                       ImVec2(kRailButton * s, kRailButton * s))) {
        request_open_image();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

void AndroidApp::draw_task_drawer(float s) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().panel);
    ImGui::BeginChild("##task_drawer", ImVec2(236.0f * s, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::SetCursorPos(ImVec2(14.0f * s, 14.0f * s));
    ImGui::TextUnformatted(ui::tr("任务", "Tasks"));
    ImGui::SameLine();
    ImGui::TextDisabled("%d / %d", static_cast<int>(tasks_.size()), max_tasks_);
    ImGui::Separator();

    const float footer = 56.0f * s;
    ImGui::BeginChild("##task_list", ImVec2(0.0f, -footer), ImGuiChildFlags_None,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    task_list_min_ = ImGui::GetWindowPos();
    task_list_max_ = ImVec2(task_list_min_.x + ImGui::GetWindowWidth(),
                            task_list_min_.y + ImGui::GetWindowHeight());
    const float drawer_dt = std::max(0.001f, ImGui::GetIO().DeltaTime);
    if (!drawer_touch_scrolling_ && std::fabs(drawer_scroll_velocity_) > 1.0f) {
        const float max_scroll = ImGui::GetScrollMaxY();
        drawer_scroll_ = std::clamp(drawer_scroll_ + drawer_scroll_velocity_ * drawer_dt,
                                    0.0f, max_scroll);
        drawer_scroll_velocity_ *= std::pow(0.90f, drawer_dt * 60.0f);
        if (drawer_scroll_ <= 0.0f || drawer_scroll_ >= max_scroll ||
            std::fabs(drawer_scroll_velocity_) < 8.0f) {
            drawer_scroll_velocity_ = 0.0f;
        }
    }
    if (drawer_touch_scrolling_ || std::fabs(drawer_scroll_velocity_) > 1.0f) {
        ImGui::SetScrollY(std::clamp(drawer_scroll_, 0.0f, ImGui::GetScrollMaxY()));
    }

    if (!tasks_.empty()) {
        const float row_height = 64.0f * s;
        const float row_gap = 4.0f * s;
        const float thumb = 48.0f * s;
        const float close_width = 30.0f * s;
        const float close_gap = 4.0f * s;
        for (int i = 0; i < static_cast<int>(tasks_.size()); ++i) {
            Task& task = tasks_[static_cast<size_t>(i)];
            ImGui::PushID(i);
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            const float row_width = std::max(24.0f * s, width - close_width - close_gap);
            ImGui::InvisibleButton("##task_row", ImVec2(row_width, row_height));
            const bool clicked = ImGui::IsItemClicked();
            const bool hovered = ImGui::IsItemHovered();
            const bool active = i == active_task_;
            ImDrawList* draw = ImGui::GetWindowDrawList();
            if (active || hovered) {
                draw->AddRectFilled(start,
                                    ImVec2(start.x + width, start.y + row_height),
                                    active ? ImGui::GetColorU32(ui::palette().panel2)
                                           : ImGui::GetColorU32(ui::palette().border),
                                    7.0f * s);
            }
            if (active) {
                draw->AddRectFilled(start,
                                    ImVec2(start.x + 2.0f * s, start.y + row_height),
                                    ImGui::GetColorU32(ui::palette().accent), 2.0f * s);
            }
            const ImVec2 thumb_min(start.x + 7.0f * s, start.y + 8.0f * s);
            draw->AddRectFilled(thumb_min,
                                ImVec2(thumb_min.x + thumb, thumb_min.y + thumb),
                                ImGui::GetColorU32(ui::palette().canvas), 4.0f * s);
            if (task.thumbnail_texture != 0) {
                draw->AddImage(static_cast<ImTextureID>(task.thumbnail_texture),
                               thumb_min,
                               ImVec2(thumb_min.x + thumb, thumb_min.y + thumb));
            }
            ImGui::PushClipRect(ImVec2(thumb_min.x + thumb + 7.0f * s, start.y),
                                ImVec2(start.x + row_width, start.y + row_height), true);
            draw->AddText(ImVec2(thumb_min.x + thumb + 7.0f * s,
                                 start.y + 10.0f * s),
                          ImGui::GetColorU32(active ? ui::palette().accent
                                                    : ui::palette().text),
                          localizedImportedName(task.name).c_str());
            draw->AddText(ImVec2(thumb_min.x + thumb + 7.0f * s,
                                 start.y + 32.0f * s),
                          ImGui::GetColorU32(ui::palette().muted),
                          (std::to_string(task.image.width) + " × " +
                           std::to_string(task.image.height)).c_str());
            ImGui::PopClipRect();

            ImGui::SetCursorScreenPos(
                ImVec2(start.x + row_width + close_gap,
                       start.y + (row_height - 26.0f * s) * 0.5f));
            const bool remove = ui::closeButton("##task_close",
                                                ImVec2(26.0f * s, 26.0f * s));
            ImGui::SetCursorScreenPos(
                ImVec2(start.x, start.y + row_height + row_gap));
            ImGui::PopID();
            if (remove) {
                pending_delete_task_ = i;
                break;
            }
            if (clicked) {
                activate_task(i);
            }
        }
    } else {
        ImGui::Dummy(ImVec2(0.0f, 32.0f * s));
        ui::centeredText(ui::tr("暂无任务", "No tasks"));
        ImGui::Spacing();
        ui::centeredText(ui::tr("导入图片后开始", "Import an image to begin"));
    }
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
    drawer_scroll_max_ = ImGui::GetScrollMaxY();
    if (!drawer_touch_scrolling_ && std::fabs(drawer_scroll_velocity_) < 1.0f) {
        drawer_scroll_ = std::clamp(ImGui::GetScrollY(), 0.0f, drawer_scroll_max_);
    }
    ImGui::EndChild();

    if (ui::centeredButton(ui::tr("导入图片", "Import image"),
                           ImVec2(ImGui::GetContentRegionAvail().x, 40.0f * s))) {
        request_open_image();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AndroidApp::draw_workspace(float s) {
    ImGui::BeginChild("##workspace", ImVec2(0.0f, 0.0f), 0,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    draw_view_tabs(s);
    draw_canvas(s);
    draw_lower_panel(s);
    ImGui::EndChild();
}

void AndroidApp::draw_view_tabs(float s) {
    ImGui::BeginChild("##view_tabs", ImVec2(0.0f, 48.0f * s), 0,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const char* labels[] = {
        ui::tr("原图", "Original"), ui::tr("光照", "Lit"),
        ui::tr("法线", "Normal"), ui::tr("深度", "Depth"),
        ui::tr("阴影", "Shadow"), ui::tr("仅光照", "Lighting")};
    const ViewMode modes[] = {ViewMode::Original, ViewMode::Lit, ViewMode::Normal,
                              ViewMode::Depth, ViewMode::Shadow, ViewMode::Lighting};
    const float gap = 4.0f * s;
    const float available = ImGui::GetContentRegionAvail().x - 12.0f * s;
    const float width = std::max(42.0f * s, (available - gap * 5.0f) / 6.0f);
    const bool has_image = image_.valid();
    const bool has_depth = depth_ready_;
    if (!has_image || (!has_depth && view_ != ViewMode::Original)) {
        view_ = ViewMode::Original;
    }
    ImGui::SetCursorPos(ImVec2(6.0f * s, 5.0f * s));
    {
        ui::FontScale small(0.84f);
        for (int i = 0; i < 6; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, gap);
            }
            const bool enabled = i == 0 || (has_image && has_depth);
            ImGui::BeginDisabled(!enabled);
            if (ui::segmentButton(labels[i], view_ == modes[i],
                                  ImVec2(width, 36.0f * s)) &&
                enabled) {
                view_ = modes[i];
            }
            ImGui::EndDisabled();
        }
    }
    ImGui::EndChild();
}

void AndroidApp::draw_canvas(float s) {
    const float canvas_height = ImGui::GetContentRegionAvail().y * 0.47f;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().canvas);
    ImGui::BeginChild("##canvas_area", ImVec2(0.0f, canvas_height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (!image_.valid()) {
        draw_empty_canvas(s);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        return;
    }

    GLuint texture = original_texture_;
    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);
    if (view_ == ViewMode::Depth && depth_ready_) {
        texture = depth_texture_;
    } else if (view_ == ViewMode::Normal && depth_ready_) {
        texture = normal_texture_;
    } else if ((view_ == ViewMode::Lit || view_ == ViewMode::Shadow ||
                view_ == ViewMode::Lighting) && depth_ready_) {
        render_lighting();
        texture = lit_texture_;
    }

    if (texture == 0) {
        draw_empty_canvas(s);
    } else {
        const ImVec2 saved_cursor = ImGui::GetCursorPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
        ImGui::InvisibleButton("##canvas_navigation", avail,
                               ImGuiButtonFlags_MouseButtonLeft);
        if (!dragging_light_ && ImGui::IsItemActive() &&
            ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            canvas_pan_.x += ImGui::GetIO().MouseDelta.x;
            canvas_pan_.y += ImGui::GetIO().MouseDelta.y;
            fit_pending_ = false;
        }
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseWheel != 0.0f) {
            canvas_zoom_ = std::clamp(canvas_zoom_ *
                                          std::pow(1.12f, ImGui::GetIO().MouseWheel),
                                      0.15f, 6.0f);
        }
        ImGui::SetCursorPos(saved_cursor);
        draw_image_fit(texture, uv0, uv1, s);
        draw_light_markers(s);
        if (view_ != ViewMode::Original && !depth_ready_) {
            ImGui::SetCursorPos(ImVec2(12.0f * s, 12.0f * s));
            ImGui::TextDisabled("%s", ui::tr("等待深度结果…", "Waiting for depth…"));
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AndroidApp::draw_light_markers(float s) {
    if (canvas_image_max_.x <= canvas_image_min_.x ||
        canvas_image_max_.y <= canvas_image_min_.y || lights_.empty()) {
        return;
    }
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 size(canvas_image_max_.x - canvas_image_min_.x,
                      canvas_image_max_.y - canvas_image_min_.y);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    for (size_t i = 0; i < lights_.size(); ++i) {
        Light& light = lights_[i];
        const float u = std::clamp(0.5f + light.position[0] / 6.0f, 0.0f, 1.0f);
        const float v = std::clamp(0.5f - light.position[1] / 6.0f, 0.0f, 1.0f);
        const ImVec2 marker(canvas_image_min_.x + u * size.x,
                            canvas_image_min_.y + v * size.y);
        const float radius = 10.0f * s;
        const float dx = mouse.x - marker.x;
        const float dy = mouse.y - marker.y;
        const bool hovered = dx * dx + dy * dy <= radius * radius * 2.25f;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered) {
            selected_light_ = static_cast<int>(i);
            dragging_light_ = true;
        }
        if (dragging_light_ && selected_light_ == static_cast<int>(i) &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const float u = std::clamp((mouse.x - canvas_image_min_.x) / size.x, 0.0f, 1.0f);
            const float v = std::clamp((mouse.y - canvas_image_min_.y) / size.y, 0.0f, 1.0f);
            light.position[0] = std::clamp((u - 0.5f) * 6.0f, -3.0f, 3.0f);
            light.position[1] = std::clamp((0.5f - v) * 6.0f, -3.0f, 3.0f);
        }
        const ImU32 fill = ImGui::ColorConvertFloat4ToU32(
            ImVec4(light.color[0], light.color[1], light.color[2],
                   light.enabled ? 0.95f : 0.38f));
        draw->AddCircleFilled(marker, radius, fill);
        draw->AddCircle(marker, radius + 2.0f * s,
                        selected_light_ == static_cast<int>(i)
                            ? ImGui::GetColorU32(ui::palette().accent)
                            : ImGui::GetColorU32(ui::palette().border),
                        0, 2.0f);
        const std::string label = std::to_string(i + 1);
        const ImVec2 label_size = ImGui::CalcTextSize(label.c_str());
        draw->AddText(ImVec2(marker.x - label_size.x * 0.5f,
                             marker.y - label_size.y * 0.5f),
                      contrastTextFor(light.color, 0.95f), label.c_str());
    }
    if (dragging_light_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragging_light_ = false;
    }
}

void AndroidApp::draw_empty_canvas(float s) {
    const ImVec2 min = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 center(min.x + size.x * 0.5f, min.y + size.y * 0.34f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (int i = 4; i >= 1; --i) {
        draw->AddCircleFilled(center, (24.0f + i * 15.0f) * s,
                              IM_COL32(232, 179, 106, 4 + i * 3), 48);
    }
    draw->AddCircle(center, 23.0f * s, IM_COL32(232, 179, 106, 110), 48, 1.0f);
    draw->AddCircleFilled(center, 10.0f * s, IM_COL32(255, 238, 210, 190), 32);

    const float button_height = 38.0f * s;
    const float button_y = size.y - button_height - 14.0f * s;
    const float width = 156.0f * s;
    ImGui::SetCursorPos(ImVec2(0.0f, button_y - 72.0f * s));
    {
        ui::FontScale title(1.05f);
        ui::centeredText(ui::tr("导入图片开始重打光", "Import an image to relight"));
    }
    ImGui::SetCursorPos(ImVec2(0.0f, button_y - 38.0f * s));
    {
        ui::FontScale body(0.82f);
        ui::centeredText(ui::tr("从手机相册选择一张照片",
                                "Choose a photo from your gallery"));
    }
    ImGui::SetCursorPos(ImVec2((size.x - width) * 0.5f, button_y));
    if (ui::centeredButton(ui::tr("导入图片", "Import image"),
                           ImVec2(width, button_height))) {
        request_open_image();
    }
}

void AndroidApp::draw_image_fit(GLuint texture, const ImVec2& uv0, const ImVec2& uv1, float s) {
    if (texture == 0 || !image_.valid()) {
        return;
    }
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float padding = 8.0f * s;
    const float fit = std::min((avail.x - padding * 2.0f) / static_cast<float>(image_.width),
                               (avail.y - padding * 2.0f) / static_cast<float>(image_.height));
    const float scale = std::max(fit, 0.001f) * canvas_zoom_;
    const ImVec2 image_size(static_cast<float>(image_.width) * scale,
                            static_cast<float>(image_.height) * scale);
    ImGui::SetCursorPos(ImVec2((avail.x - image_size.x) * 0.5f + canvas_pan_.x,
                               (avail.y - image_size.y) * 0.5f + canvas_pan_.y));
    ImGui::Image(static_cast<ImTextureID>(texture), image_size, uv0, uv1);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    canvas_image_min_ = min;
    canvas_image_max_ = max;
    ImGui::GetWindowDrawList()->AddRect(min, max, IM_COL32(74, 84, 100, 170), 3.0f * s,
                                        0, 1.0f);
}

void AndroidApp::draw_lower_panel(float s) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().panel);
    ImGui::BeginChild("##lower_panel", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    lower_panel_min_ = ImGui::GetWindowPos();
    lower_panel_max_ = ImVec2(lower_panel_min_.x + ImGui::GetWindowWidth(),
                              lower_panel_min_.y + ImGui::GetWindowHeight());
    const float frame_dt = std::max(0.001f, ImGui::GetIO().DeltaTime);
    if (!touch_scrolling_ && !show_settings_ &&
        std::fabs(lower_scroll_velocity_) > 1.0f) {
        const float max_scroll = ImGui::GetScrollMaxY();
        lower_scroll_ = std::clamp(lower_scroll_ + lower_scroll_velocity_ * frame_dt,
                                   0.0f, max_scroll);
        lower_scroll_velocity_ *= std::pow(0.90f, frame_dt * 60.0f);
        if (lower_scroll_ <= 0.0f || lower_scroll_ >= max_scroll ||
            std::fabs(lower_scroll_velocity_) < 8.0f) {
            lower_scroll_velocity_ = 0.0f;
        }
    }
    if ((touch_scrolling_ || std::fabs(lower_scroll_velocity_) > 1.0f) &&
        !show_settings_) {
        ImGui::SetScrollY(std::clamp(lower_scroll_, 0.0f, ImGui::GetScrollMaxY()));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f * s, 10.0f * s));
    const float label_width = 108.0f * s;
    const ModelInfo* selected_info =
        catalog_ ? catalog_->find(selected_model_) : nullptr;
    const char* selected_model_name =
        selected_info != nullptr ? selected_info->name.c_str() : selected_model_.c_str();
    if (selected_light_ < 0 ||
        selected_light_ >= static_cast<int>(lights_.size())) {
        selected_light_ = lights_.empty() ? -1 : 0;
    }

    {
        ui::Card image_card("##image_card", 12.0f * s, ui::palette().panel2);
        ui::sectionTitle(ui::tr("图像与深度", "Image & Depth"));
        ui::labelValueRow(ui::tr("当前模型", "Model"), selected_model_name, label_width);
        ui::labelValueRow(ui::tr("推理后端", "Backend"), "CPU", label_width);
        ui::labelValueRow(ui::tr("深度状态", "Depth Status"),
                          depth_ready_ ? ui::tr("已生成", "Ready") : (busy_ ? ui::tr("生成中…", "Generating…") : ui::tr("未生成", "Not generated")),
                          label_width);
        ImGui::Spacing();
        ui::DisabledScope disabled(busy_ || !image_.valid());
        if (ui::primaryButton(ui::tr("生成深度与法线", "Generate Depth & Normals"),
                              ImVec2(ImGui::GetContentRegionAvail().x, 40.0f * s))) {
            submit_inference();
        }
    }

    {
        ui::Card light_card("##light_card", 12.0f * s, ui::palette().panel2);
        ui::sectionTitle(ui::tr("光源", "Lights"));
        const char* presets[] = {ui::tr("阳光", "Sunset"), ui::tr("月光", "Moonlight"), ui::tr("舞台", "Stage")};
        const float preset_width = (ImGui::GetContentRegionAvail().x - 8.0f * s) / 3.0f;
        for (int i = 0; i < 3; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, 4.0f * s);
            }
            if (ui::segmentButton(presets[i], light_preset_ == i,
                                  ImVec2(preset_width, 34.0f * s))) {
                apply_light_preset(i);
            }
        }
        ImGui::Spacing();
        for (size_t i = 0; i < lights_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const Light& light = lights_[i];
            const ImVec2 row_min = ImGui::GetCursorScreenPos();
            const float row_width = ImGui::GetContentRegionAvail().x;
            ImGui::InvisibleButton("##light_row", ImVec2(row_width, 36.0f * s));
            const bool active = selected_light_ == static_cast<int>(i);
            const ImU32 fill = ImGui::ColorConvertFloat4ToU32(
                ImVec4(light.color[0], light.color[1], light.color[2],
                       light.enabled ? 0.92f : 0.38f));
            ImDrawList* draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(row_min,
                                ImVec2(row_min.x + row_width, row_min.y + 36.0f * s),
                                fill, 7.0f * s);
            draw->AddRect(row_min,
                          ImVec2(row_min.x + row_width, row_min.y + 36.0f * s),
                          ImGui::GetColorU32(active ? ui::palette().accent
                                                    : ui::palette().border),
                          7.0f * s, 0, active ? 2.0f * s : 1.0f);
            if (active) {
                draw->AddRectFilled(row_min,
                                    ImVec2(row_min.x + 3.0f * s, row_min.y + 36.0f * s),
                                    ImGui::GetColorU32(ui::palette().accent), 2.0f * s);
            }
            const std::string label =
                std::string(ui::tr("灯光 ", "Light ")) + std::to_string(i + 1) +
                (light.enabled ? "" : " · " + std::string(ui::tr("关闭", "Off")));
            const ImVec2 text_size = ImGui::CalcTextSize(label.c_str());
            draw->AddText(ImVec2(row_min.x + 10.0f * s,
                                 row_min.y + (36.0f * s - text_size.y) * 0.5f),
                          contrastTextFor(light.color, light.enabled ? 1.0f : 0.7f),
                          label.c_str());
            if (ImGui::IsItemClicked()) {
                selected_light_ = static_cast<int>(i);
            }
            ImGui::PopID();
            ImGui::Spacing();
        }
        if (ui::centeredButton(ui::tr("添加灯光", "Add Light"),
                               ImVec2(ImGui::GetContentRegionAvail().x, 34.0f * s))) {
            if (lights_.size() < 8) {
                Light light;
                light.id = static_cast<int>(lights_.size()) + 1;
                light.position[0] = 0.0f;
                light.position[1] = 0.4f;
                light.position[2] = 2.0f;
                lights_.push_back(light);
                selected_light_ = static_cast<int>(lights_.size()) - 1;
            }
        }
        if (lights_.size() > 1) {
            if (ui::centeredButton(ui::tr("删除当前灯光", "Delete Light"),
                                   ImVec2(ImGui::GetContentRegionAvail().x, 30.0f * s))) {
                lights_.erase(lights_.begin() + selected_light_);
                selected_light_ = std::min(selected_light_,
                                           static_cast<int>(lights_.size()) - 1);
            }
        }
        if (selected_light_ >= 0 && selected_light_ < static_cast<int>(lights_.size())) {
            Light& light = lights_[static_cast<size_t>(selected_light_)];
            ImGui::Spacing();
            ui::sectionTitle(ui::tr("当前灯光", "Selected Light"));
            ui::formCheckbox("##light_enabled", ui::tr("启用", "Enabled"), light.enabled);
            ui::formSliderFloat("##light_x", ui::tr("位置 X", "Position X"), light.position[0], -3.0f, 3.0f,
                                "%.2f", label_width);
            ui::formSliderFloat("##light_y", ui::tr("位置 Y", "Position Y"), light.position[1], -3.0f, 3.0f,
                                "%.2f", label_width);
            ui::formSliderFloat("##light_z", ui::tr("位置 Z", "Position Z"), light.position[2], 0.1f, 4.0f,
                                "%.2f", label_width);
            ui::formColor("##light_color", ui::tr("颜色", "Color"), light.color, label_width);
            ui::formSliderFloat("##light_intensity", ui::tr("强度", "Intensity"), light.intensity, 0.0f,
                                8.0f, "%.2f", label_width);
            ui::formSliderFloat("##light_range", ui::tr("范围", "Range"), light.range, 0.2f, 12.0f,
                                "%.2f", label_width);
            const char* types[] = {ui::tr("点光", "Point"), ui::tr("聚光", "Spot")};
            const float type_width = (ImGui::GetContentRegionAvail().x - 4.0f * s) / 2.0f;
            for (int i = 0; i < 2; ++i) {
                if (i > 0) {
                    ImGui::SameLine(0.0f, 4.0f * s);
                }
                if (ui::segmentButton(types[i], light.type == static_cast<LightType>(i),
                                      ImVec2(type_width, 32.0f * s))) {
                    light.type = static_cast<LightType>(i);
                }
            }
            if (light.type == LightType::Spot) {
                ui::formSliderFloat("##spot_angle", ui::tr("聚光角度", "Spot Angle"), light.spot_angle,
                                    5.0f, 120.0f, "%.0f°", label_width);
                ui::formSliderFloat("##spot_soft", ui::tr("聚光柔和", "Spot Softness"), light.spot_softness,
                                    0.0f, 1.0f, "%.2f", label_width);
            }
        }
    }

    {
        ui::Card shading_card("##shading_card", 12.0f * s, ui::palette().panel2);
        ui::sectionTitle(ui::tr("光照与阴影", "Lighting & Shadows"));
        ui::formColor("##ambient_color", ui::tr("环境色", "Ambient Color"), lighting_.ambient_color, label_width);
        ui::formSliderFloat("##ambient", ui::tr("环境光", "Ambient"), lighting_.ambient, 0.0f, 3.0f,
                            "%.2f", label_width);
        ui::formSliderFloat("##wrap", ui::tr("环绕光", "Wrap"), lighting_.wrap_light, 0.0f, 1.5f,
                            "%.2f", label_width);
        ui::formCheckbox("##shadow_enabled", ui::tr("启用阴影", "Enable Shadows"), lighting_.shadow_enabled);
        ui::formSliderFloat("##shadow_bias", ui::tr("阴影偏移", "Shadow Bias"), lighting_.shadow_bias,
                            0.0001f, 0.2f, "%.4f", label_width);
        ui::formSliderFloat("##shadow_soft", ui::tr("阴影柔和", "Shadow Softness"), lighting_.shadow_softness,
                            0.0f, 0.5f, "%.3f", label_width);
        ui::formSliderInt("##shadow_steps", ui::tr("阴影步数", "Shadow Steps"), lighting_.shadow_steps,
                          16, 192, label_width);
        ui::formSliderInt("##shadow_samples", ui::tr("采样数", "Samples"), lighting_.shadow_samples,
                          1, 8, label_width);
        ui::formSliderFloat("##specular", ui::tr("高光", "Specular"), lighting_.specular, 0.0f, 3.0f,
                            "%.2f", label_width);
        ui::formSliderFloat("##shininess", ui::tr("光泽硬度", "Shininess"), lighting_.shininess,
                            1.0f, 512.0f, "%.0f", label_width);
        ui::formSliderFloat("##rim", ui::tr("轮廓光", "Rim"), lighting_.rim, 0.0f, 3.0f,
                            "%.2f", label_width);
        const char* models[] = {ui::tr("经典", "Classic"), ui::tr("物理", "Physical")};
        const char* tonemaps[] = {ui::tr("柔和", "Soft"), "ACES"};
        const float half = (ImGui::GetContentRegionAvail().x - 4.0f * s) / 2.0f;
        for (int i = 0; i < 2; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, 4.0f * s);
            }
            if (ui::segmentButton(models[i],
                                  lighting_.lighting_model == static_cast<LightingModel>(i),
                                  ImVec2(half, 32.0f * s))) {
                lighting_.lighting_model = static_cast<LightingModel>(i);
            }
        }
        for (int i = 0; i < 2; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, 4.0f * s);
            }
            if (ui::segmentButton(tonemaps[i],
                                  lighting_.tone_map == static_cast<ToneMap>(i),
                                  ImVec2(half, 32.0f * s))) {
                lighting_.tone_map = static_cast<ToneMap>(i);
            }
        }
        ui::formCheckbox("##base_mode", ui::tr("仅使用重打光结果", "Use relit result only"), lighting_.base_mode);
        ui::formSliderFloat("##mix", ui::tr("效果混合", "Effect Mix"), lighting_.mix, 0.0f, 1.0f,
                            "%.2f", label_width);
        ui::formSliderFloat("##exposure", ui::tr("曝光 EV", "Exposure EV"), lighting_.exposure, -5.0f, 5.0f,
                            "%.2f", label_width);
    }

    {
        ui::Card surface_card("##surface_card", 12.0f * s, ui::palette().panel2);
        ui::sectionTitle(ui::tr("表面与合成", "Surface & Composite"));
        ui::formSliderFloat("##normal_strength", ui::tr("法线强度", "Normal Strength"), lighting_.normal_strength,
                            0.1f, 4.0f, "%.2f", label_width);
        ui::formSliderFloat("##surface_detail", ui::tr("表面细节", "Surface Detail"), lighting_.detail, 0.0f,
                            1.0f, "%.2f", label_width);
        ui::formSliderFloat("##smooth", ui::tr("边缘平滑", "Edge Smooth"), lighting_.smooth, 0.0f, 6.0f,
                            "%.1f", label_width);
        ui::formCheckbox("##line_assist", ui::tr("线条辅助", "Line Assist"), lighting_.line_assist);
        ui::formCheckbox("##invert_depth", ui::tr("反转深度", "Invert Depth"), lighting_.invert_depth);
        const char* materials[] = {ui::tr("默认", "Default"), ui::tr("皮肤", "Skin"), ui::tr("织物", "Fabric"), ui::tr("头发", "Hair"), ui::tr("金属", "Metal")};
        const float material_width = (ImGui::GetContentRegionAvail().x - 8.0f * s) / 5.0f;
        for (int i = 0; i < 5; ++i) {
            if (i > 0) {
                ImGui::SameLine(0.0f, 2.0f * s);
            }
            if (ui::segmentButton(materials[i], material_preset_ == i,
                                  ImVec2(material_width, 32.0f * s))) {
                apply_material_preset(i);
            }
        }
        ui::formSliderFloat("##roughness", ui::tr("粗糙度", "Roughness"), lighting_.roughness, 0.05f,
                            1.0f, "%.2f", label_width);
        ui::formSliderFloat("##metalness", ui::tr("金属度", "Metalness"), lighting_.metalness, 0.0f,
                            1.0f, "%.2f", label_width);
        ui::formSliderFloat("##sheen", ui::tr("光泽", "Sheen"), lighting_.sheen, 0.0f, 1.0f,
                            "%.2f", label_width);
        ui::formSliderFloat("##subsurface", ui::tr("次表面", "Subsurface"), lighting_.subsurface, 0.0f,
                            1.0f, "%.2f", label_width);
        ui::formSliderFloat("##height", ui::tr("深度幅度", "Depth Height"), lighting_.height, 0.01f, 3.0f,
                            "%.2f", label_width);
        ui::formCheckbox("##flip_x", ui::tr("翻转 X", "Flip X"), lighting_.flip_x);
        ui::formCheckbox("##flip_y", ui::tr("翻转 Y", "Flip Y"), lighting_.flip_y);
        ui::formCheckbox("##flip_z", ui::tr("翻转 Z", "Flip Z"), lighting_.flip_z);
        ui::formCheckbox("##auto_range", ui::tr("自动深度范围", "Auto Depth Range"), lighting_.auto_depth_range);
        if (!lighting_.auto_depth_range) {
            ui::formSliderFloat("##depth_min", ui::tr("深度最小", "Depth Min"), lighting_.depth_min,
                                -1.0f, 3.0f, "%.3f", label_width);
            ui::formSliderFloat("##depth_max", ui::tr("深度最大", "Depth Max"), lighting_.depth_max,
                                -1.0f, 5.0f, "%.3f", label_width);
        }
        ui::formSliderInt("##preview_long", ui::tr("预览长边", "Preview Edge"), lighting_.preview_long_edge, 512,
                          3072, label_width);
    }

    {
        ui::Card model_card("##model_card", 12.0f * s, ui::palette().panel2);
        ui::sectionTitle(ui::tr("模型与状态", "Model & Status"));
        ui::labelValueRow(ui::tr("当前模型", "Model"), selected_model_name, label_width);
        const ModelInfo* model = catalog_ ? catalog_->find(selected_model_) : nullptr;
        if (model != nullptr) {
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::TextDisabled("%s", localizedModelDescription(*model).c_str());
            ImGui::PopTextWrapPos();
        }
        if (ui::centeredButton(ui::tr("恢复默认 Q4", "Restore Default Q4"),
                               ImVec2(ImGui::GetContentRegionAvail().x, 36.0f * s))) {
            reset_model();
        }
        if (depth_ready_) {
            ImGui::TextDisabled(ui::tr("推理 %.1f ms · %d × %d", "Inference %.1f ms · %d × %d"), depth_.inference_ms,
                                depth_.width, depth_.height);
        }
        ImGui::TextDisabled(ui::tr("状态：%s", "Status: %s"),
                            localizedStatus(status_).c_str());
    }

    ImGui::Spacing();
    if (ui::centeredButton(ui::tr("重置当前设置", "Reset Settings"),
                           ImVec2(ImGui::GetContentRegionAvail().x, 38.0f * s))) {
        lighting_ = LightingSettings{};
        light_preset_ = 0;
        material_preset_ = 0;
        view_ = ViewMode::Original;
        canvas_zoom_ = 1.0f;
        canvas_pan_ = ImVec2(0.0f, 0.0f);
        lights_.clear();
        Light light;
        light.id = 1;
        lights_.push_back(light);
        selected_light_ = 0;
    }

    lower_scroll_max_ = ImGui::GetScrollMaxY();
    if (!touch_scrolling_ && std::fabs(lower_scroll_velocity_) < 1.0f) {
        lower_scroll_ = std::clamp(ImGui::GetScrollY(), 0.0f, lower_scroll_max_);
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AndroidApp::draw_bottom_bar(float s) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::palette().panel);
    ImGui::BeginChild("##bottombar", ImVec2(0.0f, 52.0f * s), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ui::FontScale micro(0.80f);
        ImGui::SetCursorPos(ImVec2(12.0f * s, 2.0f * s));
        ImGui::PushTextWrapPos(ImGui::GetWindowWidth() - 12.0f * s);
        const std::string line =
            (image_.valid() ? localizedImportedName(image_name_)
                            : ui::tr("未导入图片", "No image")) +
            " · " + localizedStatus(status_);
        ImGui::TextDisabled("%s", line.c_str());
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void AndroidApp::draw_export_menu(float s) {
    if (export_popup_open_) {
        ImGui::OpenPopup("##export_menu");
        export_popup_open_ = false;
    }
    if (!ImGui::BeginPopup("##export_menu")) {
        return;
    }
    ImGui::TextDisabled("%s", ui::tr("导出当前结果", "Export current result"));
    ImGui::Separator();
    const struct {
        const char* label;
        ViewMode mode;
    } items[] = {
        {ui::tr("光照", "Lit"), ViewMode::Lit},
        {ui::tr("法线", "Normal"), ViewMode::Normal},
        {ui::tr("深度", "Depth"), ViewMode::Depth},
        {ui::tr("阴影", "Shadow"), ViewMode::Shadow},
        {ui::tr("仅光照", "Lighting"), ViewMode::Lighting},
    };
    for (const auto& item : items) {
        if (ImGui::MenuItem(item.label)) {
            export_image(item.mode);
        }
    }
    ImGui::EndPopup();
}

void AndroidApp::export_image(ViewMode mode) {
    if (!depth_ready_ || !image_.valid() || export_callback_ == nullptr) {
        set_status(ui::tr("没有可导出的结果", "Nothing to export"));
        return;
    }
    export_busy_ = true;
    const ViewMode previous_view = view_;
    GLuint source_texture = 0;
    int width = image_.width;
    int height = image_.height;
    std::string kind;

    if (mode == ViewMode::Normal) {
        source_texture = normal_texture_;
        width = depth_.width;
        height = depth_.height;
        kind = "normal";
    } else if (mode == ViewMode::Depth) {
        source_texture = depth_texture_;
        width = depth_.width;
        height = depth_.height;
        kind = "depth";
    } else {
        view_ = mode;
        render_lighting();
        view_ = previous_view;
        source_texture = lit_texture_;
        kind = mode == ViewMode::Shadow ? "shadow"
               : mode == ViewMode::Lighting ? "lighting"
                                            : "lit";
    }
    if (source_texture == 0 || width <= 0 || height <= 0) {
        export_busy_ = false;
        set_status(ui::tr("导出失败：纹理不可用", "Export failed: texture unavailable"));
        return;
    }

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           source_texture, 0);
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);

    export_callback_(pixels, width, height, kind);
    export_busy_ = false;
    set_status(ui::tr("已导出", "Exported"));
}

void AndroidApp::draw_settings(float s) {
    if (catalog_) {
        catalog_->poll_inspections();
    }
    if (show_settings_ && !ImGui::IsPopupOpen("##settings", ImGuiPopupFlags_AnyPopupId)) {
        ImGui::OpenPopup("##settings");
    }
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float inset_top = static_cast<float>(system_inset_top_.load());
    const float settings_width = std::min(680.0f * s, display.x - 24.0f * s);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(settings_width, 0.0f),
        ImVec2(settings_width, display.y - inset_top - 24.0f * s));
    ImGui::SetNextWindowPos(
        ImVec2(display.x * 0.5f, inset_top + (display.y - inset_top) * 0.5f),
        ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f * s);
    ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 12.0f * s);
    bool open = show_settings_;
    if (!ImGui::BeginPopupModal("##settings", &open,
                                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoScrollbar |
                                    ImGuiWindowFlags_NoScrollWithMouse)) {
        show_settings_ = open;
        ImGui::PopStyleVar(2);
        return;
    }
    show_settings_ = open;
    settings_min_ = ImGui::GetWindowPos();
    settings_max_ = ImVec2(settings_min_.x + ImGui::GetWindowWidth(),
                           settings_min_.y + ImGui::GetWindowHeight());
    const float settings_dt = std::max(0.001f, ImGui::GetIO().DeltaTime);
    if (!settings_touch_scrolling_ &&
        std::fabs(settings_scroll_velocity_) > 1.0f) {
        const float max_scroll = ImGui::GetScrollMaxY();
        settings_scroll_ = std::clamp(
            settings_scroll_ + settings_scroll_velocity_ * settings_dt, 0.0f,
            max_scroll);
        settings_scroll_velocity_ *= std::pow(0.90f, settings_dt * 60.0f);
        if (settings_scroll_ <= 0.0f || settings_scroll_ >= max_scroll ||
            std::fabs(settings_scroll_velocity_) < 8.0f) {
            settings_scroll_velocity_ = 0.0f;
        }
    }
    if (settings_touch_scrolling_ ||
        std::fabs(settings_scroll_velocity_) > 1.0f) {
        ImGui::SetScrollY(std::clamp(settings_scroll_, 0.0f, ImGui::GetScrollMaxY()));
    }

    ImGui::TextUnformatted(ui::tr("设置", "Settings"));
    ImGui::SameLine(ImGui::GetWindowWidth() - 46.0f * s);
    if (ui::closeButton("##settings_close", ImVec2(28.0f * s, 28.0f * s))) {
        show_settings_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::Separator();

    const char* tabs[] = {ui::tr("通用", "General"), ui::tr("模型", "Model"), ui::tr("关于", "About")};
    const float tab_width = (ImGui::GetContentRegionAvail().x - 8.0f * s) / 3.0f;
    for (int i = 0; i < 3; ++i) {
        if (i > 0) {
            ImGui::SameLine(0.0f, 4.0f * s);
        }
        const ImVec2 tab_size(tab_width, 36.0f * s);
        const ImVec2 tab_min = ImGui::GetCursorScreenPos();
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##settings_tab", tab_size)) {
            settings_tab_ = i;
        }
        const bool hovered = ImGui::IsItemHovered();
        const ImVec2 tab_max(tab_min.x + tab_size.x, tab_min.y + tab_size.y);
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec4 fill = settings_tab_ == i
                                ? ui::palette().accent
                                : (hovered ? ui::palette().panel2 : ui::palette().panel);
        draw->AddRectFilled(tab_min, tab_max, ImGui::GetColorU32(fill),
                            ImGui::GetStyle().FrameRounding);
        draw->AddRect(tab_min, tab_max, ImGui::GetColorU32(ui::palette().border),
                      ImGui::GetStyle().FrameRounding);
        const ImVec2 text_size = ImGui::CalcTextSize(tabs[i]);
        draw->AddText(ImVec2(tab_min.x + (tab_size.x - text_size.x) * 0.5f,
                             tab_min.y + (tab_size.y - text_size.y) * 0.5f),
                      ImGui::GetColorU32(settings_tab_ == i ? ImVec4(0.07f, 0.07f, 0.07f, 1.0f)
                                                            : ui::palette().text),
                      tabs[i]);
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (settings_tab_ == 0) {
        ui::sectionTitle(ui::tr("外观", "Appearance"));
        ui::formLabel(ui::tr("主题", "Theme"), 110.0f * s);
        if (ui::segmentButton(ui::tr("深色", "Dark"), ui::themeMode() == ui::ThemeMode::Dark,
                              ImVec2(90.0f * s, 34.0f * s))) {
            ui::setThemeMode(ui::ThemeMode::Dark);
            std::ofstream(std::filesystem::path(files_dir_) / "theme.txt") << 0;
        }
        ImGui::SameLine();
        if (ui::segmentButton(ui::tr("浅色", "Light"), ui::themeMode() == ui::ThemeMode::Light,
                              ImVec2(90.0f * s, 34.0f * s))) {
            ui::setThemeMode(ui::ThemeMode::Light);
            std::ofstream(std::filesystem::path(files_dir_) / "theme.txt") << 1;
        }
        ui::formLabel(ui::tr("语言", "Language"), 110.0f * s);
        if (ui::segmentButton("简体中文", ui::language() == ui::Language::Chinese,
                              ImVec2(96.0f * s, 34.0f * s))) {
            ui::setLanguage(ui::Language::Chinese);
            std::ofstream(std::filesystem::path(files_dir_) / "language.txt") << 0;
        }
        ImGui::SameLine();
        if (ui::segmentButton("English", ui::language() == ui::Language::English,
                              ImVec2(96.0f * s, 34.0f * s))) {
            ui::setLanguage(ui::Language::English);
            std::ofstream(std::filesystem::path(files_dir_) / "language.txt") << 1;
        }
        ui::labelValueRow(ui::tr(ui::tr("推理后端", "Backend"), "Backend"), "CPU", 110.0f * s);
        ui::labelValueRow(ui::tr("字体", "Font"), ui::tr("内置思源黑体", "Bundled Source Han Sans"), 110.0f * s);
        ImGui::Spacing();
        ui::sectionTitle(ui::tr("任务", "Tasks"));
        if (ui::formSliderInt("##task_limit", ui::tr("任务上限", "Task Limit"), max_tasks_,
                              2, 16, 110.0f * s)) {
            std::ofstream(std::filesystem::path(files_dir_) / "task_limit.txt")
                << max_tasks_;
        }
        ImGui::TextDisabled("%s", ui::tr("达到上限后将阻止继续导入，直到删除任务。",
                                        "Imports are blocked at the limit until a task is deleted."));
        ImGui::Spacing();
    } else if (settings_tab_ == 1) {
        ui::sectionTitle(ui::tr("深度模型", "Depth Model"));
        ui::labelValueRow(ui::tr("模型存储位置", "Model Storage"),
                          model_storage_label_.c_str(), 110.0f * s);
        if (ui::centeredButton(ui::tr("选择模型文件夹", "Choose Model Folder"),
                               ImVec2(ImGui::GetContentRegionAvail().x, 36.0f * s))) {
            if (model_folder_callback_) {
                model_folder_callback_();
            }
        }
        if (catalog_ != nullptr) {
            const auto& models = catalog_->models();
            const ImGuiStyle& style = ImGui::GetStyle();
            const float row_stride = 52.0f * s + style.ItemSpacing.y;
            const float content_height =
                row_stride * static_cast<float>(models.size()) +
                style.WindowPadding.y * 2.0f;
            const float max_list_height = std::min(240.0f * s, display.y * 0.45f);
            const float list_height = std::min(content_height, max_list_height);
            ImGui::BeginChild("##model_list", ImVec2(0.0f, list_height),
                              ImGuiChildFlags_Borders |
                                  ImGuiChildFlags_AlwaysUseWindowPadding);
            model_list_min_ = ImGui::GetWindowPos();
            model_list_max_ = ImVec2(model_list_min_.x + ImGui::GetWindowWidth(),
                                     model_list_min_.y + ImGui::GetWindowHeight());
            bool scroll_applied = false;
            if (model_list_touch_scrolling_ || model_list_scroll_dirty_) {
                ImGui::SetScrollY(
                    std::clamp(model_list_scroll_, 0.0f, ImGui::GetScrollMaxY()));
                model_list_scroll_dirty_ = false;
                scroll_applied = true;
            }
            model_rows_.clear();
            for (const ModelInfo& model : models) {
                ImGui::PushID(model.id.c_str());
                if (ImGui::Selectable(("##model_row_" + model.id).c_str(),
                                      selected_model_ == model.id, 0,
                                      ImVec2(0.0f, 52.0f * s))) {
                    select_model(model.id);
                }
                const ImVec2 row_min = ImGui::GetItemRectMin();
                const ImVec2 row_max = ImGui::GetItemRectMax();
                model_rows_.push_back(
                    {model.id, ImVec4(row_min.x, row_min.y, row_max.x, row_max.y)});
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextColored(selected_model_ == model.id ? ui::palette().accent
                                                               : ui::palette().text,
                                   "%s", model.name.c_str());
                {
                    ui::FontScale small(0.72f);
                    const std::string description = localizedModelDescription(model);
                    ImGui::TextDisabled("%s%s", description.c_str(),
                                        model.available
                                            ? ""
                                            : ui::tr("（未打包）", " (not bundled)"));
                }
                ImGui::EndGroup();
                ImGui::PopID();
            }
            model_list_scroll_max_ = ImGui::GetScrollMaxY();
            if (model_list_scroll_to_end_) {
                model_list_scroll_ = model_list_scroll_max_;
                model_list_scroll_dirty_ = true;
                model_list_scroll_to_end_ = false;
                scroll_applied = true;
            }
            if (!model_list_touch_scrolling_ && !scroll_applied) {
                model_list_scroll_ =
                    std::clamp(ImGui::GetScrollY(), 0.0f, model_list_scroll_max_);
            }
            ImGui::EndChild();
        }
        if (model_delete_popup_requested_) {
            ImGui::SetNextWindowPos(model_press_pos_, ImGuiCond_Appearing);
            ImGui::OpenPopup("##model_delete");
            model_delete_popup_requested_ = false;
        }
        if (ImGui::BeginPopup("##model_delete")) {
            const ModelInfo* target =
                catalog_ ? catalog_->find(model_delete_target_) : nullptr;
            ImGui::TextUnformatted(
                ui::tr("删除模型记录？", "Delete model record?"));
            if (target != nullptr) {
                ImGui::TextDisabled("%s", target->name.c_str());
            }
            ImGui::TextDisabled(
                "%s", ui::tr("不会删除磁盘上的模型文件。",
                             "The model file on disk is not deleted."));
            ImGui::Separator();
            if (ImGui::Button(ui::tr("删除记录", "Delete Record"),
                              ImVec2(132.0f * s, 34.0f * s))) {
                if (catalog_ &&
                    catalog_->remove_custom_model(files_dir_, model_delete_target_)) {
                    if (selected_model_ == model_delete_target_) {
                        selected_model_ = ModelCatalog::kDefaultId;
                        catalog_->save_selection(files_dir_, selected_model_);
                        set_status(ui::tr("已删除模型记录，已恢复默认 Q4",
                                          "Model record removed; default Q4 restored"));
                    } else {
                        set_status(ui::tr("已删除模型记录", "Model record removed"));
                    }
                }
                model_delete_target_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(ui::tr("取消", "Cancel"),
                              ImVec2(92.0f * s, 34.0f * s))) {
                model_delete_target_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        const ModelInfo* active = catalog_ ? catalog_->find(selected_model_) : nullptr;
        if (active != nullptr) {
            ui::labelValueRow(ui::tr("模型文件", "Model File"),
                              active->display_path.c_str(), 110.0f * s);
            ui::labelValueRow(ui::tr("模型类型", "Model Type"),
                              localizedModelType(active->type).c_str(),
                              110.0f * s);
            ui::labelValueRow(ui::tr("状态", "Status"),
                              active->available ? ui::tr("可用", "Available")
                                                : ui::tr("未打包", "Not bundled"),
                              110.0f * s);
        }
        ImGui::Spacing();
        if (ui::centeredButton(ui::tr("恢复默认 Q4", "Restore Default Q4"),
                               ImVec2(ImGui::GetContentRegionAvail().x, 38.0f * s))) {
            reset_model();
        }
    } else {
        ui::sectionTitle(ui::tr("关于", "About"));
        ui::labelValueRow(ui::tr("版本", "Version"), "0.1.0", 100.0f * s);
        ui::labelValueRow(ui::tr("作者", "Author"), "pza", 100.0f * s);
        ui::labelValueRow(ui::tr("渲染", "Renderer"), "OpenGL ES 3.1 + ImGui",
                          100.0f * s);
        ui::labelValueRow(ui::tr("深度模型", "Depth Model"),
                          "Depth Anything V2 Small · Q4", 100.0f * s);
        ui::labelValueRow(ui::tr("推理运行时", "Runtime"),
                          "ONNX Runtime 1.20.0 · CPU", 100.0f * s);
        ImGui::Spacing();
    }

    settings_scroll_max_ = ImGui::GetScrollMaxY();
    if (!settings_touch_scrolling_ && std::fabs(settings_scroll_velocity_) < 1.0f) {
        settings_scroll_ = std::clamp(ImGui::GetScrollY(), 0.0f, settings_scroll_max_);
    }
    ImGui::EndPopup();
    ImGui::PopStyleVar(2);
}

bool AndroidApp::initialize_imgui(std::string& error) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    ui::applyTheme(density_);
    std::string font_error;
    if (!ui::loadFont(io, assets_, density_, font_error)) {
        LOGE("CJK font unavailable: %s", font_error.c_str());
    }
    if (!ImGui_ImplOpenGL3_Init("#version 300 es")) {
        error = "ImGui OpenGL ES backend initialization failed";
        ImGui::DestroyContext();
        return false;
    }
    return true;
}

bool AndroidApp::initialize_lighting(std::string& error) {
    lit_program_ = linkProgram(kFullscreenVertexShader, kLightingFragmentShaderEs, error);
    if (lit_program_ == 0) {
        return false;
    }
    glGenVertexArrays(1, &lit_vao_);
    return true;
}

void AndroidApp::ensure_lighting_target(int width, int height) {
    if (lit_texture_ != 0 && lit_fbo_ != 0 && lit_width_ == width &&
        lit_height_ == height) {
        return;
    }
    if (lit_texture_ != 0) {
        glDeleteTextures(1, &lit_texture_);
        lit_texture_ = 0;
    }
    if (lit_fbo_ != 0) {
        glDeleteFramebuffers(1, &lit_fbo_);
        lit_fbo_ = 0;
    }
    glGenTextures(1, &lit_texture_);
    glBindTexture(GL_TEXTURE_2D, lit_texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    glGenFramebuffers(1, &lit_fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, lit_fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           lit_texture_, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        LOGE("lighting framebuffer incomplete");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    lit_width_ = width;
    lit_height_ = height;
}

void AndroidApp::render_lighting() {
    if (!depth_ready_ || original_texture_ == 0 || normal_texture_ == 0 ||
        lit_program_ == 0) {
        return;
    }
    ensure_lighting_target(image_.width, image_.height);
    if (lit_texture_ == 0 || lit_fbo_ == 0) {
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, lit_fbo_);
    glViewport(0, 0, image_.width, image_.height);
    glUseProgram(lit_program_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, original_texture_);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, normal_texture_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, depth_texture_);
    glUniform1i(glGetUniformLocation(lit_program_, "uAlbedo"), 0);
    glUniform1i(glGetUniformLocation(lit_program_, "uNormal"), 1);
    glUniform1i(glGetUniformLocation(lit_program_, "uHeight"), 2);
    glUniform2f(glGetUniformLocation(lit_program_, "uImageSize"),
                static_cast<float>(image_.width), static_cast<float>(image_.height));
    glUniform1f(glGetUniformLocation(lit_program_, "uAmplitude"),
                std::max(lighting_.height, 0.001f));
    glUniform3f(glGetUniformLocation(lit_program_, "uAmbientColor"),
                lighting_.ambient_color[0], lighting_.ambient_color[1],
                lighting_.ambient_color[2]);
    glUniform1f(glGetUniformLocation(lit_program_, "uAmbient"), lighting_.ambient);
    glUniform1f(glGetUniformLocation(lit_program_, "uWrap"), lighting_.wrap_light);
    glUniform1f(glGetUniformLocation(lit_program_, "uSpecular"), lighting_.specular);
    glUniform1f(glGetUniformLocation(lit_program_, "uShininess"), lighting_.shininess);
    glUniform1f(glGetUniformLocation(lit_program_, "uRim"), lighting_.rim);
    glUniform1f(glGetUniformLocation(lit_program_, "uRoughness"), lighting_.roughness);
    glUniform1f(glGetUniformLocation(lit_program_, "uMetalness"), lighting_.metalness);
    glUniform1f(glGetUniformLocation(lit_program_, "uSheen"), lighting_.sheen);
    glUniform1f(glGetUniformLocation(lit_program_, "uSubsurface"),
                lighting_.subsurface);
    glUniform1f(glGetUniformLocation(lit_program_, "uExposure"), lighting_.exposure);
    glUniform1f(glGetUniformLocation(lit_program_, "uNormalStrength"),
                lighting_.normal_strength);
    glUniform1f(glGetUniformLocation(lit_program_, "uMix"), lighting_.mix);
    glUniform1i(glGetUniformLocation(lit_program_, "uLightingModel"),
                static_cast<int>(lighting_.lighting_model));
    glUniform1i(glGetUniformLocation(lit_program_, "uToneMap"),
                static_cast<int>(lighting_.tone_map));
    glUniform1i(glGetUniformLocation(lit_program_, "uShadowsEnabled"),
                lighting_.shadow_enabled ? 1 : 0);

    const int light_count =
        static_cast<int>(std::min<size_t>(lights_.size(), 8));
    std::vector<float> positions(static_cast<size_t>(light_count) * 3, 0.0f);
    std::vector<float> colors(static_cast<size_t>(light_count) * 3, 1.0f);
    std::vector<float> intensities(static_cast<size_t>(light_count), 0.0f);
    std::vector<float> ranges(static_cast<size_t>(light_count), 1.0f);
    std::vector<int> enabled(static_cast<size_t>(light_count), 0);
    std::vector<int> types(static_cast<size_t>(light_count), 0);
    std::vector<float> spot_cos(static_cast<size_t>(light_count), -1.0f);
    std::vector<float> spot_soft(static_cast<size_t>(light_count), 0.0f);
    for (int i = 0; i < light_count; ++i) {
        const Light& light = lights_[static_cast<size_t>(i)];
        for (int axis = 0; axis < 3; ++axis) {
            positions[static_cast<size_t>(i * 3 + axis)] = light.position[axis];
            colors[static_cast<size_t>(i * 3 + axis)] = light.color[axis];
        }
        intensities[static_cast<size_t>(i)] = light.intensity;
        ranges[static_cast<size_t>(i)] = light.range;
        enabled[static_cast<size_t>(i)] = light.enabled ? 1 : 0;
        types[static_cast<size_t>(i)] =
            light.type == LightType::Spot ? 1 : 0;
        const float half_angle =
            std::clamp(light.spot_angle, 1.0f, 179.0f) * 0.5f;
        spot_cos[static_cast<size_t>(i)] =
            std::cos(half_angle * 3.14159265358979323846f / 180.0f);
        spot_soft[static_cast<size_t>(i)] =
            std::clamp(light.spot_softness, 0.0f, 1.0f);
    }
    glUniform1i(glGetUniformLocation(lit_program_, "uLightCount"), light_count);
    if (light_count > 0) {
        glUniform3fv(glGetUniformLocation(lit_program_, "uLightPos"), light_count,
                     positions.data());
        glUniform3fv(glGetUniformLocation(lit_program_, "uLightColor"), light_count,
                     colors.data());
        glUniform1fv(glGetUniformLocation(lit_program_, "uLightIntensity"),
                     light_count, intensities.data());
        glUniform1fv(glGetUniformLocation(lit_program_, "uLightRange"), light_count,
                     ranges.data());
        glUniform1iv(glGetUniformLocation(lit_program_, "uLightEnabled"),
                     light_count, enabled.data());
        glUniform1iv(glGetUniformLocation(lit_program_, "uLightType"), light_count,
                     types.data());
        glUniform1fv(glGetUniformLocation(lit_program_, "uLightSpotCos"),
                     light_count, spot_cos.data());
        glUniform1fv(glGetUniformLocation(lit_program_, "uLightSpotSoft"),
                     light_count, spot_soft.data());
    }

    int mode = 0;
    if (view_ == ViewMode::Shadow) {
        mode = 1;
    } else if (view_ == ViewMode::Lighting) {
        mode = 2;
    }
    glUniform1i(glGetUniformLocation(lit_program_, "uMode"), mode);

    glBindVertexArray(lit_vao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, surface_width_, surface_height_);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool AndroidApp::upload_texture(GLuint& texture, int width, int height,
                                const std::vector<uint8_t>& rgba, bool mipmap,
                                std::string& error) {
    if (width <= 0 || height <= 0 ||
        rgba.size() != static_cast<size_t>(width) * height * 4) {
        error = "invalid RGBA texture data";
        return false;
    }
    if (texture == 0) {
        glGenTextures(1, &texture);
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgba.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (mipmap) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    const GLenum gl_error = glGetError();
    if (gl_error != GL_NO_ERROR) {
        error = "OpenGL texture upload error";
        return false;
    }
    return true;
}

void AndroidApp::delete_texture(GLuint& texture) {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
        texture = 0;
    }
}

bool AndroidApp::make_thumbnail(const ImageData& image, GLuint& texture,
                                std::string& error) {
    if (!image.valid()) {
        error = "invalid image";
        return false;
    }
    constexpr int kThumbSize = 64;
    std::vector<uint8_t> thumbnail(kThumbSize * kThumbSize * 4);
    for (int y = 0; y < kThumbSize; ++y) {
        const int source_y = std::min(image.height - 1, y * image.height / kThumbSize);
        for (int x = 0; x < kThumbSize; ++x) {
            const int source_x = std::min(image.width - 1, x * image.width / kThumbSize);
            const size_t source = (static_cast<size_t>(source_y) * image.width +
                                   static_cast<size_t>(source_x)) * 4;
            const size_t target = (static_cast<size_t>(y) * kThumbSize +
                                   static_cast<size_t>(x)) * 4;
            thumbnail[target + 0] = image.rgba[source + 0];
            thumbnail[target + 1] = image.rgba[source + 1];
            thumbnail[target + 2] = image.rgba[source + 2];
            thumbnail[target + 3] = image.rgba[source + 3];
        }
    }
    return upload_texture(texture, kThumbSize, kThumbSize, thumbnail, false, error);
}

void AndroidApp::store_active_task() {
    if (active_task_ < 0 || active_task_ >= static_cast<int>(tasks_.size())) {
        return;
    }
    Task& task = tasks_[static_cast<size_t>(active_task_)];
    task.name = image_name_;
    // The inference result may have arrived through the active-result
    // fallback path. Always mirror the active depth into the task so a
    // surface recreation can restore it.
    task.depth = depth_;
    task.depth_ready = depth_ready_;
    task.busy = busy_;
    task.generation = generation_;
    task.lights = lights_;
    task.selected_light = selected_light_;
    task.lighting = lighting_;
    task.view = view_;
    task.canvas_zoom = canvas_zoom_;
    task.canvas_pan_x = canvas_pan_.x;
    task.canvas_pan_y = canvas_pan_.y;
    task.material_preset = material_preset_;
    task.light_preset = light_preset_;
}

void AndroidApp::activate_task(int index) {
    if (index < 0 || index >= static_cast<int>(tasks_.size())) {
        return;
    }
    // Callers that have just appended or restored a task set active_task_ to
    // the new index before calling this. Do not overwrite that task with the
    // previous active state; only save when actually switching documents.
    if (active_task_ >= 0 && active_task_ != index) {
        store_active_task();
    }
    active_task_ = index;
    Task& task = tasks_[static_cast<size_t>(index)];
    image_ = task.image;
    image_name_ = task.name;
    depth_ = task.depth;
    depth_ready_ = task.depth_ready && depth_.width > 0 && depth_.height > 0 &&
                   !depth_.depth_rgba.empty();
    // A snapshot may carry the payload even if an older task flag was not
    // updated; the payload is authoritative for restoring a generated task.
    if (!depth_ready_ && depth_.width > 0 && depth_.height > 0 &&
        !depth_.depth_rgba.empty()) {
        depth_ready_ = true;
    }
    generation_ = task.generation;
    busy_ = task.busy;
    lights_ = task.lights;
    selected_light_ = task.selected_light;
    lighting_ = task.lighting;
    view_ = task.view;
    canvas_zoom_ = task.canvas_zoom;
    canvas_pan_ = ImVec2(task.canvas_pan_x, task.canvas_pan_y);
    material_preset_ = task.material_preset;
    light_preset_ = task.light_preset;
    fit_pending_ = true;

    std::string error;
    if (!upload_texture(original_texture_, image_.width, image_.height, image_.rgba,
                        true, error)) {
        set_status(ui::tr("原图纹理上传失败: ", "Original texture upload failed: ") + error);
        return;
    }
    if (depth_ready_ && depth_.width > 0 && depth_.height > 0) {
        upload_texture(depth_texture_, depth_.width, depth_.height, depth_.depth_rgba,
                       false, error);
        upload_texture(normal_texture_, depth_.width, depth_.height,
                       depth_.normal_rgba, false, error);
    }
    if (!depth_ready_ && view_ != ViewMode::Original) {
        view_ = ViewMode::Original;
    }
    if (depth_ready_) {
        set_status(ui::tr("深度与法线已生成", "Depth and normals generated"));
    } else {
        set_status(ui::tr("图片已导入，请点击“生成深度与法线”", "Image imported. Tap Generate Depth & Normals."));
    }
}

void AndroidApp::delete_task(int index) {
    if (index < 0 || index >= static_cast<int>(tasks_.size())) {
        return;
    }
    store_active_task();
    Task& task = tasks_[static_cast<size_t>(index)];
    delete_texture(task.thumbnail_texture);
    tasks_.erase(tasks_.begin() + index);
    if (tasks_.empty()) {
        active_task_ = -1;
        image_ = ImageData{};
        image_name_.clear();
        depth_ = DepthResult{};
        depth_ready_ = false;
        generation_ = 0;
        busy_ = false;
        view_ = ViewMode::Original;
        delete_texture(original_texture_);
        delete_texture(depth_texture_);
        delete_texture(normal_texture_);
        set_status(ui::tr("请导入图片", "Import an image"));
        return;
    }
    if (index < active_task_) {
        --active_task_;
    } else if (index == active_task_) {
        active_task_ = std::min(index, static_cast<int>(tasks_.size()) - 1);
        activate_task(active_task_);
    }
}

void AndroidApp::set_image(const ImageData& image, const std::string& name) {
    if (!image.valid()) {
        set_status(ui::tr("图片数据无效", "Invalid image data"));
        return;
    }
    if (static_cast<int>(tasks_.size()) >= max_tasks_) {
        set_status(ui::tr("已达到任务导入上限（", "Task limit reached (") +
                   std::to_string(max_tasks_) +
                   ui::tr(" 个），请先删除任务或在设置中调整上限",
                          " tasks). Delete a task or raise the limit in Settings."));
        return;
    }
    store_active_task();
    Task task;
    task.id = next_task_id_++;
    task.name = name;
    task.image = image;
    task.depth_ready = false;
    task.busy = false;
    task.generation = 0;
    task.lights = lights_;
    task.selected_light = 0;
    task.lighting = lighting_;
    task.view = ViewMode::Original;
    task.canvas_zoom = 1.0f;
    task.canvas_pan_x = 0.0f;
    task.canvas_pan_y = 0.0f;
    task.material_preset = material_preset_;
    task.light_preset = light_preset_;
    std::string error;
    if (!make_thumbnail(image, task.thumbnail_texture, error)) {
        LOGE("thumbnail upload failed: %s", error.c_str());
    }
    tasks_.push_back(std::move(task));
    active_task_ = static_cast<int>(tasks_.size()) - 1;
    activate_task(active_task_);
}

void AndroidApp::submit_inference() {
    if (!catalog_ || !inference_ || !image_.valid()) {
        return;
    }
    const ModelInfo* info = catalog_->find(selected_model_);
    if (info == nullptr || !info->available) {
        set_status(ui::tr("所选模型不可用", "Selected model is unavailable"));
        return;
    }
    generation_ = next_generation_++;
    busy_ = true;
    depth_ready_ = false;
    depth_ = DepthResult{};
    if (active_task_ >= 0 && active_task_ < static_cast<int>(tasks_.size())) {
        Task& task = tasks_[static_cast<size_t>(active_task_)];
        task.generation = generation_;
        task.busy = true;
        task.depth_ready = false;
        task.depth = DepthResult{};
    }
    set_status(ui::tr("正在生成深度与法线…", "Generating depth and normals…"));
    inference_->submit(image_, info->id, info->asset_path, generation_);
}

void AndroidApp::select_model(const std::string& id) {
    if (!catalog_) {
        return;
    }
    const ModelInfo* info = catalog_->find(id);
    if (info == nullptr) {
        set_status(ui::tr("未知模型: ", "Unknown model: ") + id);
        return;
    }
    if (!info->available) {
        set_status(ui::tr("模型未内置: ", "Model is not available: ") + id);
        return;
    }
    selected_model_ = id;
    catalog_->save_selection(files_dir_, id);
    set_status(ui::tr("已选择模型: ", "Model selected: ") + info->name +
               ui::tr("，请生成深度与法线", "; generate depth to continue"));
}

void AndroidApp::reset_model() {
    select_model(ModelCatalog::kDefaultId);
}

void AndroidApp::set_status(const std::string& status) {
    status_ = status;
    LOGI("%s", status.c_str());
}

void AndroidApp::request_open_image() {
    if (static_cast<int>(tasks_.size()) >= max_tasks_) {
        set_status(ui::tr("已达到任务导入上限（", "Task limit reached (") +
                   std::to_string(max_tasks_) +
                   ui::tr(" 个），请先删除任务或在设置中调整上限",
                          " tasks). Delete a task or raise the limit in Settings."));
        return;
    }
    if (open_image_callback_) {
        open_image_callback_();
    }
}

}  // namespace imrelighter
