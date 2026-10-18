#include "ui/ui_internal.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "app/app.h"
#include "io/native_dialog.h"
#include "io/paths.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#ifndef IMRELIGHTER_VERSION
#define IMRELIGHTER_VERSION "0.2.1"
#endif

namespace ui {

namespace {

std::string modelPathLabel(const std::filesystem::path& path) {
    if (path.empty()) return {};
    std::error_code error;
    const auto relative = std::filesystem::relative(path, paths::exeDir(), error);
    if (!error && !relative.empty() &&
        relative.native().compare(0, 2, L"..") != 0) {
        return paths::toUtf8(relative);
    }
    return paths::toUtf8(path);
}

std::string formatMegabytes(std::uintmax_t bytes) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.1f MB",
                  static_cast<double>(bytes) / (1024.0 * 1024.0));
    return buffer;
}

} // namespace

void drawSettings(app::App& app) {
    if (!app.ui.settingsOpen) return;

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const ImVec2 dialogSize(
        std::min(scaled(680.0f), std::max(scaled(360.0f),
                                         display.x - scaled(32.0f))),
        std::min(scaled(520.0f), std::max(scaled(280.0f),
                                         display.y - scaled(32.0f))));
    Dialog settings("##settings_window", dialogSize, scaled(22.0f));
    if (settings) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
        {
            FontScale titleScale(1.12f);
            ImGui::TextUnformatted(app.tr("settings.title").c_str());
        }
        ImGui::SameLine(ImGui::GetWindowWidth() - 48.0f);
        if (closeButton("##settings_close", ImVec2(26.0f, 26.0f))) {
            app.ui.settingsOpen = false;
        }
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6.0f);
        ImGui::Separator();

        if (ImGui::BeginTabBar("##settings_tabs")) {
            if (ImGui::BeginTabItem(
                    (app.tr("settings.general") + "###general").c_str())) {
                sectionTitle(app.tr("settings.appearance").c_str());

                formLabel(app.tr("settings.theme").c_str(), scaled(kSettingsLabelWidth));
                if (segmentButton(app.tr("settings.theme.dark").c_str(),
                                  app.theme() == core::Theme::Dark,
                                  ImVec2(scaled(80.0f), 0.0f))) {
                    app.setTheme(core::Theme::Dark);
                }
                ImGui::SameLine();
                if (segmentButton(app.tr("settings.theme.light").c_str(),
                                  app.theme() == core::Theme::Light,
                                  ImVec2(scaled(80.0f), 0.0f))) {
                    app.setTheme(core::Theme::Light);
                }

                ImGui::Spacing();
                formLabel(app.tr("settings.language").c_str(), scaled(kSettingsLabelWidth));
                if (segmentButton("简体中文",
                                  app.language() == core::Language::ZhCN,
                                  ImVec2(scaled(110.0f), 0.0f))) {
                    app.setLanguage(core::Language::ZhCN);
                }
                ImGui::SameLine();
                if (segmentButton("English",
                                  app.language() == core::Language::EnUS,
                                  ImVec2(scaled(100.0f), 0.0f))) {
                    app.setLanguage(core::Language::EnUS);
                }

                ImGui::Spacing();
                formLabel(app.tr("settings.backend").c_str(), scaled(kSettingsLabelWidth));
                if (segmentButton(app.tr("settings.auto").c_str(),
                                  app.config.backend == core::Backend::Auto,
                                  ImVec2(scaled(80.0f), 0.0f))) {
                    app.setBackend(core::Backend::Auto);
                }
                ImGui::SameLine();
                if (segmentButton(app.tr("settings.cpu").c_str(),
                                  app.config.backend == core::Backend::Cpu,
                                  ImVec2(scaled(70.0f), 0.0f))) {
                    app.setBackend(core::Backend::Cpu);
                }
                ImGui::SameLine();
                const bool dmlReady = app.directmlAvailable();
                if (!dmlReady) ImGui::BeginDisabled();
                if (segmentButton(app.tr("settings.dml").c_str(),
                                  app.config.backend == core::Backend::Dml,
                                  ImVec2(scaled(100.0f), 0.0f)) &&
                     dmlReady) {
                    app.setBackend(core::Backend::Dml);
                }
                if (!dmlReady) ImGui::EndDisabled();
                if (!dmlReady) {
                    ImGui::TextDisabled("%s",
                                        app.tr("settings.dml_unavailable").c_str());
                }

                ImGui::Spacing();
                sectionTitle(app.tr("settings.resources").c_str());
                // ImGui sizes integer slider grabs from the value range, so a
                // 1..500 slider gets a much thinner knob than a 1..16 slider.
                // Use float sliders with integer formatting and one explicit
                // grab size to keep both controls visually identical.
                ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, scaled(18.0f));
                formLabel(app.tr("settings.max_tasks").c_str(),
                          scaled(kSettingsLabelWidth));
                float maxTasks = static_cast<float>(app.config.maxTasks);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::SliderFloat("##max_tasks", &maxTasks, 1.0f, 500.0f,
                                       "%.0f", ImGuiSliderFlags_AlwaysClamp)) {
                    const int value = std::clamp(
                        static_cast<int>(std::lround(maxTasks)), 1, 500);
                    maxTasks = static_cast<float>(value);
                    app.setResourceLimits(value, app.config.memoryLimitGb,
                                          false);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    app.setResourceLimits(app.config.maxTasks,
                                          app.config.memoryLimitGb);
                }
                formLabel(app.tr("settings.memory_limit").c_str(),
                          scaled(kSettingsLabelWidth));
                float memoryGb =
                    static_cast<float>(app.config.memoryLimitGb);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::SliderFloat("##memory_limit", &memoryGb, 1.0f, 16.0f,
                                       "%.0f", ImGuiSliderFlags_AlwaysClamp)) {
                    const int value = std::clamp(
                        static_cast<int>(std::lround(memoryGb)), 1, 16);
                    memoryGb = static_cast<float>(value);
                    app.setResourceLimits(app.config.maxTasks, value, false);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    app.setResourceLimits(app.config.maxTasks,
                                          app.config.memoryLimitGb);
                }
                formLabel(app.tr("settings.image_limit").c_str(),
                          scaled(kSettingsLabelWidth));
                float imageMegapixels =
                    static_cast<float>(app.config.maxImageMegapixels);
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::SliderFloat(
                        "##image_limit", &imageMegapixels, 4.0f, 100.0f,
                        "%.0f MP", ImGuiSliderFlags_AlwaysClamp)) {
                    const int value = std::clamp(
                        static_cast<int>(std::lround(imageMegapixels)), 4,
                        100);
                    imageMegapixels = static_cast<float>(value);
                    app.setImageMegapixelLimit(value, false);
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    app.setImageMegapixelLimit(
                        app.config.maxImageMegapixels);
                }
                ImGui::PopStyleVar();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(
                    (app.tr("settings.model") + "###model").c_str())) {
                sectionTitle(app.tr("settings.model").c_str());
                const bool modelReady = app.modelExists();
                {
                    Box info("##model_info", ImVec2(0.0f, 0.0f), 12.0f, true);
                    if (info) {
                    const bool noModel = app.modelPath().empty();
                    ImGui::TextColored(modelReady ? theme::ok : theme::danger,
                                       "%s",
                                       app.tr(noModel
                                                  ? "model.not_selected"
                                                  : modelReady ? "model.ready"
                                                               : "model.missing")
                                           .c_str());
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s",
                                        app.tr("settings.model_path").c_str());
                    const std::string modelPathText = modelPathLabel(app.modelPath());
                    ImGui::TextWrapped("%s", modelPathText.c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", paths::toUtf8(app.modelPath()).c_str());
                    }
                    if (!app.modelPath().empty()) {
                        app.inspectModelInfo();
                        ImGui::TextDisabled("%s",
                                            app.tr("settings.model_type").c_str());
                        if (app.modelInfoPending()) {
                            ImGui::TextDisabled(
                                "%s",
                                app.tr("settings.model_checking").c_str());
                        } else if (!app.modelInfoError().empty()) {
                            ImGui::PushStyleColor(ImGuiCol_Text, theme::danger);
                            ImGui::TextWrapped("%s",
                                               app.modelInfoError().c_str());
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::TextUnformatted(app.modelType().c_str());
                        }
                    }
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s",
                                        app.tr("settings.model_size").c_str());
                    ImGui::TextUnformatted(
                        noModel ? "—"
                                : formatMegabytes(app.modelSize()).c_str());
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s",
                                        app.tr("settings.model_sha").c_str());
                    app.refreshModelSha();
                    if (app.modelShaPending()) {
                        ImGui::TextDisabled("%s", app.tr("settings.model_sha_calculating").c_str());
                    } else {
                        const std::string sha = app.modelSha();
                        ImGui::TextWrapped("%s", sha.empty() ? "—" : sha.c_str());
                    }
                    ImGui::Spacing();

                    if (ImGui::Button(
                            app.tr("settings.choose_model_file").c_str(),
                            ImVec2(-1.0f, 0.0f))) {
                        app.chooseModelFile();
                    }
                    const float modelButtonWidth =
                        (ImGui::GetContentRegionAvail().x -
                         ImGui::GetStyle().ItemSpacing.x) * 0.5f;
                    if (ImGui::Button(
                            app.tr("settings.open_model_dir").c_str(),
                            ImVec2(modelButtonWidth, 0.0f))) {
                        native_dialog::openInExplorer(app.modelFolder());
                    }
                    ImGui::SameLine();
                    ImGui::BeginDisabled(noModel);
                    ImGui::PushStyleColor(ImGuiCol_Button, theme::danger);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::dangerHover);
                    if (ImGui::Button(
                            app.tr("settings.clear_model").c_str(),
                            ImVec2(modelButtonWidth, 0.0f))) {
                        app.clearModelSelection();
                    }
                    ImGui::PopStyleColor(2);
                    ImGui::EndDisabled();
                    }
                }
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(
                    (app.tr("settings.about") + "###about").c_str())) {
                sectionTitle(app.tr("settings.about").c_str());
                ImGui::Text("%s %s", app.tr("settings.version").c_str(),
                            IMRELIGHTER_VERSION);
                ImGui::TextDisabled("by pza");
                ImGui::Text("%s: %s", app.tr("settings.opengl").c_str(),
                            app.glVersion().c_str());
                ImGui::Text("%s: %s", app.tr("settings.gpu").c_str(),
                            app.glRenderer().c_str());
                ImGui::Spacing();
                ImGui::TextUnformatted(app.tr("settings.shortcuts").c_str());
                ImGui::TextDisabled("%s",
                                    app.tr("settings.shortcuts_text").c_str());
                ImGui::Spacing();
                ImGui::TextUnformatted(app.tr("settings.licenses").c_str());
                ImGui::TextDisabled(
                    "GLFW · Dear ImGui · GLAD · stb · nlohmann/json");
                ImGui::TextDisabled(
                    "ONNX Runtime (MIT) · DirectML (Microsoft) · libwebp (BSD-3)");
                ImGui::TextDisabled("Depth Anything V2 (Apache-2.0) · Source Han Sans (OFL-1.1)");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
}

} // namespace ui
