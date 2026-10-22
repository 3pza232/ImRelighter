#pragma once

#include <android/asset_manager.h>
#include <GLES3/gl3.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "core/model_catalog.h"
#include "core/types.h"
#include "imgui.h"
#include "io/image_loader.h"
#include "ml/inference_service.h"

namespace imrelighter {

class AndroidApp {
public:
    AndroidApp(AAssetManager* assets, std::string files_dir, float density);
    ~AndroidApp();

    AndroidApp(const AndroidApp&) = delete;
    AndroidApp& operator=(const AndroidApp&) = delete;

    bool initialize(std::string& error);
    void shutdown();
    void resize(int width, int height);
    void render();

    void queue_touch(int action, int pointer_count, float x0, float y0,
                     float x1, float y1);
    void queue_image(std::vector<uint8_t> bytes);
    void queue_image_rgba(std::vector<uint8_t> rgba, int width, int height);
    void queue_model(const std::string& id);
    void queue_model_file(const std::string& absolute_path);
    void queue_model_files(std::vector<std::string> absolute_paths);
    void set_model_storage_label(const std::string& label);
    void set_system_insets(int top, int bottom);
    void queue_reset_model();
    bool handle_back();

    void set_open_image_callback(std::function<void()> callback);
    void set_model_file_callback(std::function<void()> callback);
    void set_model_folder_callback(std::function<void()> callback);
    void set_export_callback(
        std::function<void(const std::vector<uint8_t>&, int, int, const std::string&)>
            callback);

private:
    struct TouchEvent {
        int action = 0;
        int pointer_count = 0;
        float x0 = 0.0f;
        float y0 = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
    };

    struct Task {
        uint64_t id = 0;
        std::string name;
        ImageData image;
        DepthResult depth;
        bool depth_ready = false;
        bool busy = false;
        uint64_t generation = 0;
        std::vector<Light> lights;
        int selected_light = 0;
        LightingSettings lighting;
        ViewMode view = ViewMode::Original;
        float canvas_zoom = 1.0f;
        float canvas_pan_x = 0.0f;
        float canvas_pan_y = 0.0f;
        int material_preset = 0;
        int light_preset = 0;
        GLuint thumbnail_texture = 0;
    };

    struct ModelRowHit {
        std::string id;
        ImVec4 rect;  // min.x, min.y, max.x, max.y
    };

    void process_queues();
    void poll_inference();
    void draw_ui(float scale);

    void draw_top_bar(float scale);
    void draw_task_rail(float scale);
    void draw_task_drawer(float scale);
    void draw_workspace(float scale);
    void draw_view_tabs(float scale);
    void draw_canvas(float scale);
    void draw_bottom_bar(float scale);
    void draw_lower_panel(float scale);
    void draw_settings(float scale);
    void draw_export_menu(float scale);
    void update_model_long_press();
    void draw_empty_canvas(float scale);
    void draw_light_markers(float scale);
    void draw_image_fit(GLuint texture, const ImVec2& uv0, const ImVec2& uv1,
                        float scale);

    bool initialize_imgui(std::string& error);
    bool initialize_lighting(std::string& error);
    bool initialize_normal_pass(std::string& error);
    bool ensure_normal_target(int width, int height, std::string& error);
    bool initialize_depth_pass(std::string& error);
    bool render_depth_pass(std::string& error);
    bool render_normal_pass(std::string& error);
    bool initialize_shadow_pass(std::string& error);
    bool ensure_shadow_targets(int width, int height, std::string& error);
    bool render_shadow_pass(std::string& error);
    void ensure_lighting_target(int width, int height);
    void render_lighting(bool for_export = false);
    bool upload_texture(GLuint& texture, int width, int height,
                        const std::vector<uint8_t>& rgba, bool mipmaps,
                        std::string& error);
    void delete_texture(GLuint& texture);
    void set_image(const ImageData& image, const std::string& name);
    void store_active_task();
    void activate_task(int index);
    void delete_task(int index);
    bool make_thumbnail(const ImageData& image, GLuint& texture,
                        std::string& error);
    void submit_inference();
    void select_model(const std::string& id);
    void reset_model();
    void apply_light_preset(int preset);
    void apply_material_preset(int preset);
    void export_image(ViewMode mode);
    void set_status(const std::string& text);
    void request_open_image();

    AAssetManager* assets_ = nullptr;
    std::string files_dir_;
    float density_ = 1.0f;
    int surface_width_ = 0;
    int surface_height_ = 0;
    std::atomic<int> system_inset_top_{0};
    std::atomic<int> system_inset_bottom_{0};
    bool initialized_ = false;

    std::unique_ptr<ModelCatalog> catalog_;
    std::unique_ptr<ml::InferenceService> inference_;
    std::string selected_model_ = "q4";

    ImageData image_;
    std::string image_name_;
    DepthResult depth_;
    bool depth_ready_ = false;
    uint64_t generation_ = 0;
    bool busy_ = false;
    std::string status_ = "Preparing…";

    std::vector<Task> tasks_;
    int active_task_ = -1;
    int pending_delete_task_ = -1;
    int max_tasks_ = 8;
    uint64_t next_task_id_ = 1;
    uint64_t next_generation_ = 1;

    GLuint original_texture_ = 0;
    GLuint depth_texture_ = 0;
    GLuint normal_texture_ = 0;
    GLuint height_texture_ = 0;
    GLuint height_fbo_ = 0;
    GLuint depth_source_texture_ = 0;
    GLuint depth_program_ = 0;
    GLuint depth_vao_ = 0;
    GLuint normal_fbo_ = 0;
    GLuint normal_program_ = 0;
    GLuint normal_vao_ = 0;
    int normal_width_ = 0;
    int normal_height_ = 0;
    bool normal_pass_ready_ = false;
    bool height_dirty_ = true;
    bool normal_dirty_ = true;
    bool shadow_dirty_ = true;
    bool interaction_active_ = false;
    GLuint shadow_texture_[2] = {0, 0};
    GLuint shadow_fbo_[2] = {0, 0};
    GLuint blur_texture_[2] = {0, 0};
    GLuint blur_fbo_[2] = {0, 0};
    GLuint shadow_program_ = 0;
    GLuint shadow_blur_program_ = 0;
    GLuint shadow_vao_ = 0;
    int shadow_width_ = 0;
    int shadow_height_ = 0;
    bool shadow_pass_ready_ = false;
    GLuint lit_texture_ = 0;
    GLuint lit_fbo_ = 0;
    GLuint lit_program_ = 0;
    GLuint lit_vao_ = 0;
    int lit_width_ = 0;
    int lit_height_ = 0;

    ViewMode view_ = ViewMode::Original;
    LightingSettings lighting_;
    std::vector<Light> lights_;
    int selected_light_ = 0;
    bool render_dirty_ = true;
    LightingSettings last_lighting_{};
    std::vector<Light> last_lights_;
    bool last_render_state_valid_ = false;
    bool task_drawer_open_ = false;
    bool show_settings_ = false;
    int settings_tab_ = 0;
    int light_preset_ = 0;
    int material_preset_ = 0;
    float canvas_zoom_ = 1.0f;
    ImVec2 canvas_pan_ = ImVec2(0.0f, 0.0f);
    ImVec2 canvas_image_min_ = ImVec2(0.0f, 0.0f);
    ImVec2 canvas_image_max_ = ImVec2(0.0f, 0.0f);
    bool fit_pending_ = true;

    ImVec2 task_list_min_ = ImVec2(0.0f, 0.0f);
    ImVec2 task_list_max_ = ImVec2(0.0f, 0.0f);
    float drawer_scroll_ = 0.0f;
    float drawer_scroll_max_ = 0.0f;
    float drawer_scroll_velocity_ = 0.0f;
    bool drawer_touch_scrolling_ = false;
    bool drawer_touch_started_ = false;
    float drawer_touch_start_y_ = 0.0f;
    float drawer_touch_start_scroll_ = 0.0f;
    std::chrono::steady_clock::time_point touch_start_time_{};
    std::chrono::steady_clock::time_point touch_last_move_time_{};

    ImVec2 lower_panel_min_ = ImVec2(0.0f, 0.0f);
    ImVec2 lower_panel_max_ = ImVec2(0.0f, 0.0f);
    float lower_scroll_ = 0.0f;
    float lower_scroll_max_ = 0.0f;
    bool touch_scrolling_ = false;
    bool touch_direct_control_ = false;
    bool touch_pinching_ = false;
    bool touch_started_in_lower_ = false;
    bool touch_gesture_cancelled_ = false;
    float lower_scroll_velocity_ = 0.0f;
    float settings_scroll_velocity_ = 0.0f;
    bool pending_lower_click_ = false;
    bool pending_lower_release_ = false;
    float pending_click_x_ = 0.0f;
    float pending_click_y_ = 0.0f;

    ImVec2 settings_min_ = ImVec2(0.0f, 0.0f);
    ImVec2 settings_max_ = ImVec2(0.0f, 0.0f);
    float settings_scroll_ = 0.0f;
    float settings_scroll_max_ = 0.0f;
    bool settings_touch_scrolling_ = false;
    bool settings_touch_started_ = false;
    ImVec2 model_list_min_ = ImVec2(0.0f, 0.0f);
    ImVec2 model_list_max_ = ImVec2(0.0f, 0.0f);
    float model_list_scroll_ = 0.0f;
    float model_list_scroll_max_ = 0.0f;
    bool model_list_touch_scrolling_ = false;
    bool model_list_scroll_dirty_ = false;
    bool model_list_scroll_to_end_ = false;
    bool model_list_touch_started_ = false;
    float model_list_touch_start_y_ = 0.0f;
    float model_list_touch_start_scroll_ = 0.0f;
    std::vector<ModelRowHit> model_rows_;
    std::string model_press_id_;
    ImVec2 model_press_pos_ = ImVec2(0.0f, 0.0f);
    std::chrono::steady_clock::time_point model_press_time_{};
    bool model_press_candidate_ = false;
    bool model_long_press_triggered_ = false;
    bool model_delete_popup_requested_ = false;
    std::string model_delete_target_;
    bool pending_settings_click_ = false;
    bool pending_settings_release_ = false;
    float settings_click_x_ = 0.0f;
    float settings_click_y_ = 0.0f;
    float settings_touch_start_y_ = 0.0f;
    float settings_touch_start_scroll_ = 0.0f;
    std::chrono::steady_clock::time_point settings_touch_start_time_{};
    std::chrono::steady_clock::time_point settings_touch_last_move_time_{};
    bool dragging_light_ = false;
    float touch_start_y_ = 0.0f;
    float touch_start_scroll_ = 0.0f;
    float pinch_last_distance_ = 0.0f;

    bool export_popup_open_ = false;
    bool export_busy_ = false;
    std::string export_status_;

    std::mutex queue_mutex_;
    std::vector<TouchEvent> touch_events_;
    std::vector<uint8_t> pending_image_;
    bool has_pending_image_ = false;
    std::vector<uint8_t> pending_image_rgba_;
    int pending_image_width_ = 0;
    int pending_image_height_ = 0;
    bool has_pending_image_rgba_ = false;
    std::string pending_model_;
    std::string pending_model_file_;
    std::vector<std::string> pending_model_files_;
    std::string pending_model_storage_label_;
    bool pending_reset_ = false;

    std::function<void()> open_image_callback_;
    std::function<void()> model_file_callback_;
    std::function<void()> model_folder_callback_;
    std::string model_storage_label_ = "应用内部存储";
    std::function<void(const std::vector<uint8_t>&, int, int, const std::string&)>
        export_callback_;
    std::chrono::steady_clock::time_point last_frame_time_;
};

}  // namespace imrelighter
