#include "ui/ui_internal.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "app/app.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace ui {

namespace {

void sectionCardHeader(app::App& app, const char* key) {
    ImGui::TextColored(theme::accent, "%s", app.tr(key).c_str());
    ImGui::Separator();
}

bool sliderRow(app::App& app, const char* labelKey, float& value, float min,
               float max, const char* format) {
    ImGui::PushID(labelKey);
    formLabel(app.tr(labelKey).c_str(), scaled(kLabelWidth));
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::SliderFloat("##value", &value, min, max, format);
    ImGui::PopID();
    return changed;
}

bool sliderIntRow(app::App& app, const char* labelKey, int& value, int min,
                  int max) {
    ImGui::PushID(labelKey);
    formLabel(app.tr(labelKey).c_str(), scaled(kLabelWidth));
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::SliderInt("##value", &value, min, max);
    ImGui::PopID();
    return changed;
}

bool checkboxRow(app::App& app, const char* labelKey, bool& value) {
    ImGui::PushID(labelKey);
    const bool changed = ImGui::Checkbox(app.tr(labelKey).c_str(), &value);
    ImGui::PopID();
    return changed;
}

std::string vec3ToHex(const float rgb[3]) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02x%02x%02x",
                  static_cast<int>(std::lround(std::clamp(rgb[0], 0.0f, 1.0f) * 255.0f)),
                  static_cast<int>(std::lround(std::clamp(rgb[1], 0.0f, 1.0f) * 255.0f)),
                  static_cast<int>(std::lround(std::clamp(rgb[2], 0.0f, 1.0f) * 255.0f)));
    return buffer;
}

bool colorRow(app::App& app, const char* labelKey, std::string& color) {
    ImGui::PushID(labelKey);
    formLabel(app.tr(labelKey).c_str(), scaled(kLabelWidth));

    ImVec4 value = hexToVec4(color);
    float rgb[3] = {value.x, value.y, value.z};
    ImGui::SetNextItemWidth(-1.0f);
    const bool changed = ImGui::ColorEdit3("##color", rgb,
                                           ImGuiColorEditFlags_NoInputs |
                                               ImGuiColorEditFlags_NoLabel);
    if (changed) color = vec3ToHex(rgb);
    ImGui::PopID();
    return changed;
}

} // namespace

void drawImageSection(app::App& app) {
    Card section("##section_image", 14.0f, theme::panel2);
    if (!section) return;
    sectionCardHeader(app, "panel.image");

    core::Document fallback;
    core::Document* doc = app.activeDocument();
    const bool hasDocument = doc != nullptr;
    if (!doc) doc = &fallback;

    ImGui::SeparatorText(app.tr("panel.image.model").c_str());
    const bool modelReady = app.modelExists();
    ImGui::TextColored(modelReady ? theme::ok : theme::danger, "%s",
                       app.tr(modelReady ? "model.ready" : "model.missing").c_str());

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("panel.image.depth").c_str());
    ImGui::TextUnformatted(app.tr("panel.image.backend").c_str());
    ImGui::SameLine(scaled(kLabelWidth));
    ImGui::TextDisabled("%s", backendLabel(app, app.config.backend).c_str());

    ImGui::TextUnformatted(app.tr("inference.size").c_str());
    ImGui::SameLine(scaled(kLabelWidth));
    const std::string autoLabel = app.tr("inference.auto");
    const char* sizeLabels[] = {autoLabel.c_str(), "384", "504", "518", "672"};
    int sizeIndex = 0;
    if (doc->settings.inferenceSize == 384) sizeIndex = 1;
    else if (doc->settings.inferenceSize == 504) sizeIndex = 2;
    else if (doc->settings.inferenceSize == 518) sizeIndex = 3;
    else if (doc->settings.inferenceSize == 672) sizeIndex = 4;
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::BeginDisabled(!hasDocument);
    if (ImGui::Combo("##inference_size", &sizeIndex, sizeLabels,
                     IM_ARRAYSIZE(sizeLabels))) {
        const int sizes[] = {0, 384, 504, 518, 672};
        doc->settings.inferenceSize = sizes[sizeIndex];
    }
    ImGui::EndDisabled();

    ImGui::TextUnformatted(app.tr("panel.image.depth").c_str());
    ImGui::SameLine(scaled(kLabelWidth));
    const core::Document* active = app.activeDocument();
    const bool hasDepth = active && active->hasDepth;
    const bool running = app.inferenceBusy();
    ImGui::TextDisabled("%s",
                        app.tr(running ? "depth.running"
                                       : hasDepth ? "depth.ready"
                                                  : "depth.not_generated")
                            .c_str());

    ImGui::Spacing();
    const bool canGenerate = app.hasTask() && !running;
    if (canGenerate) {
        ImGui::PushStyleColor(ImGuiCol_Button, theme::accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::accentHover);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::ink);
    }
    ImGui::BeginDisabled(!canGenerate);
    if (ImGui::Button(app.tr("panel.image.generate").c_str(),
                      ImVec2(-1.0f, 34.0f))) {
        app.generateDepth();
    }
    ImGui::EndDisabled();
    if (canGenerate) ImGui::PopStyleColor(3);

    if (running) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, theme::danger);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::dangerHover);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::ink);
        if (ImGui::Button(app.tr("panel.image.cancel").c_str(),
                          ImVec2(-1.0f, 30.0f))) {
            app.cancelInference();
        }
        ImGui::PopStyleColor(3);
    }

    if (!app.hasTask()) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", app.tr("panel.image.no_task").c_str());
    }
}
void drawLightSection(app::App& app) {
    Card section("##section_light", 14.0f, theme::panel2);
    if (!section) return;
    sectionCardHeader(app, "panel.light");

    core::Document fallback;
    core::Document* doc = app.activeDocument();
    if (!doc) doc = &fallback;

    ImGui::SeparatorText(app.tr("section.light_list").c_str());
    ImGui::TextDisabled("%d / 8", static_cast<int>(doc->lights.size()));
    ImGui::Spacing();

    for (auto& light : doc->lights) {
        ImGui::PushID(light.id);
        ImGui::PushStyleColor(ImGuiCol_Text, hexToVec4(light.color));
        ImGui::TextUnformatted("●");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        const std::string label =
            app.tr("light.name") + " " + std::to_string(light.id);
        if (ImGui::Selectable(label.c_str(),
                              doc->selectedLight == light.id)) {
            doc->selectedLight = light.id;
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::BeginDisabled(doc->lights.size() >= 8);
    if (ImGui::Button(app.tr("light.add").c_str())) {
        int nextId = 1;
        for (const auto& light : doc->lights) {
            nextId = std::max(nextId, light.id + 1);
        }
        doc->lights.push_back(core::makeLight(nextId, {0.35f, 0.2f, 1.4f}));
        doc->selectedLight = nextId;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(doc->lights.size() <= 1);
    if (ImGui::Button(app.tr("light.delete").c_str())) {
        doc->lights.erase(
            std::remove_if(doc->lights.begin(), doc->lights.end(),
                           [&](const core::Light& light) {
                               return light.id == doc->selectedLight;
                           }),
            doc->lights.end());
        if (!doc->lights.empty()) {
            doc->selectedLight = doc->lights.front().id;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(app.tr("light.toggle").c_str())) {
        for (auto& light : doc->lights) {
            if (light.id == doc->selectedLight) {
                light.enabled = !light.enabled;
            }
        }
    }

    auto selected = std::find_if(doc->lights.begin(), doc->lights.end(),
                                 [&](const core::Light& light) {
                                     return light.id == doc->selectedLight;
                                 });
    if (selected == doc->lights.end() && !doc->lights.empty()) {
        selected = doc->lights.begin();
        doc->selectedLight = selected->id;
    }

    if (selected != doc->lights.end()) {
        ImGui::Spacing();
        ImGui::SeparatorText(app.tr("section.selected_light").c_str());
        ImGui::TextUnformatted(app.tr("light.position").c_str());
        const float positionMin[3] = {-2.0f, -2.0f, 0.1f};
        const float positionMax[3] = {2.0f, 2.0f, 4.0f};
        {
            Box positionBox("##position_box", ImVec2(0.0f, scaled(kPositionBoxHeight)),
                            10.0f, true);
            if (positionBox) {
                vector3Control("##position", selected->position.data(),
                               positionMin, positionMax);
            }
        }

        ImGui::Spacing();
        colorRow(app, "light.color", selected->color);
        sliderRow(app, "light.intensity", selected->intensity, 0.0f, 8.0f,
                  "%.2f");
        sliderRow(app, "light.range", selected->range, 0.2f, 8.0f, "%.2f");
    }

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.motion").c_str());
    checkboxRow(app, "light.orbit", doc->animate);
}
void drawShadingSection(app::App& app) {
    Card section("##section_shading", 14.0f, theme::panel2);
    if (!section) return;
    sectionCardHeader(app, "panel.shading");

    core::Document fallback;
    core::Document* doc = app.activeDocument();
    if (!doc) doc = &fallback;

    ImGui::SeparatorText(app.tr("section.environment").c_str());
    colorRow(app, "shading.ambient_color", doc->settings.ambientColor);
    sliderRow(app, "shading.ambient", doc->settings.ambient, 0.0f, 2.0f,
              "%.2f");
    sliderRow(app, "shading.wrap", doc->settings.wrap, 0.0f, 1.0f, "%.2f");

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.shadows").c_str());
    checkboxRow(app, "shading.shadows", doc->settings.shadows);
    ImGui::BeginDisabled(!doc->settings.shadows);
    sliderRow(app, "shading.bias", doc->settings.bias, 0.0001f, 0.1f,
              "%.4f");
    sliderRow(app, "shading.softness", doc->settings.softness, 0.0f, 0.25f,
              "%.3f");
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (ImGui::CollapsingHeader(
            (app.tr("shading.highlights") + "###highlights").c_str())) {
        sliderRow(app, "shading.specular", doc->settings.specular, 0.0f,
                  2.0f, "%.2f");
        sliderRow(app, "shading.shininess", doc->settings.shininess, 1.0f,
                  256.0f, "%.0f");
        sliderRow(app, "shading.rim", doc->settings.rim, 0.0f, 2.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader(
            (app.tr("quality.title") + "###quality").c_str())) {
        sliderIntRow(app, "quality.shadow_steps",
                     doc->settings.shadowSteps, 16, 192);
        sliderIntRow(app, "quality.shadow_samples",
                     doc->settings.shadowSamples, 1, 8);

        const std::string fullLabel = app.tr("quality.full");
        const char* resolutionLabels[] = {"1/2", "1/4", fullLabel.c_str()};
        int resolutionIndex = doc->settings.shadowDivisor == 4
                                  ? 1
                                  : doc->settings.shadowDivisor == 1 ? 2 : 0;
        ImGui::TextUnformatted(app.tr("quality.shadow_resolution").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##shadow_resolution", &resolutionIndex,
                         resolutionLabels,
                         IM_ARRAYSIZE(resolutionLabels))) {
            const int divisors[] = {2, 4, 1};
            doc->settings.shadowDivisor = divisors[resolutionIndex];
        }

        const char* previewLabels[] = {"1024", "1536", "2048"};
        int previewIndex = doc->settings.previewLongEdge == 1024
                               ? 0
                               : doc->settings.previewLongEdge == 2048 ? 2 : 1;
        ImGui::TextUnformatted(
            app.tr("quality.preview_resolution").c_str());
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::Combo("##preview_resolution", &previewIndex,
                         previewLabels,
                         IM_ARRAYSIZE(previewLabels))) {
            const int previewSizes[] = {1024, 1536, 2048};
            doc->settings.previewLongEdge = previewSizes[previewIndex];
        }
    }
}
void drawSurfaceSection(app::App& app) {
    Card section("##section_surface", 14.0f, theme::panel2);
    if (!section) return;
    sectionCardHeader(app, "panel.surface");

    core::Document fallback;
    core::Document* doc = app.activeDocument();
    if (!doc) doc = &fallback;

    ImGui::SeparatorText(app.tr("section.base").c_str());
    ImGui::TextUnformatted(app.tr("surface.base_mode").c_str());
    const float baseWidth = ImGui::GetContentRegionAvail().x;
    if (segmentButton(app.tr("surface.base_keep").c_str(),
                      !doc->settings.baseMode, ImVec2(baseWidth, 0.0f))) {
        doc->settings.baseMode = false;
    }
    if (segmentButton(app.tr("surface.base_clean").c_str(),
                      doc->settings.baseMode, ImVec2(baseWidth, 0.0f))) {
        doc->settings.baseMode = true;
    }

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.depth_surface").c_str());
    sliderRow(app, "surface.height", doc->settings.height, 0.01f, 2.0f,
              "%.2f");
    sliderRow(app, "surface.normal_strength", doc->settings.normalStrength,
              0.1f, 3.0f, "%.2f");
    sliderRow(app, "surface.smooth", doc->settings.smooth, 0.0f, 4.0f,
              "%.1f");
    checkboxRow(app, "surface.line_assist", doc->settings.lineAssist);
    checkboxRow(app, "surface.invert", doc->settings.invert);

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.composite").c_str());
    sliderRow(app, "surface.mix", doc->settings.mix, 0.0f, 1.0f, "%.2f");
    sliderRow(app, "surface.exposure", doc->settings.exposure, -3.0f, 3.0f,
              "%.2f");

    ImGui::Spacing();
    if (ImGui::CollapsingHeader(
            (app.tr("surface.advanced") + "###advanced").c_str())) {
        checkboxRow(app, "surface.flip_x", doc->settings.flipX);
        checkboxRow(app, "surface.flip_y", doc->settings.flipY);
        checkboxRow(app, "surface.flip_z", doc->settings.flipZ);
        checkboxRow(app, "surface.auto_depth", doc->settings.autoDepthRange);
        ImGui::BeginDisabled(doc->settings.autoDepthRange);
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputFloat(app.tr("surface.depth_min").c_str(),
                          &doc->settings.depthMin, 0.0f, 0.0f, "%.3f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::InputFloat(app.tr("surface.depth_max").c_str(),
                          &doc->settings.depthMax, 0.0f, 0.0f, "%.3f");
        ImGui::EndDisabled();
        if (doc->settings.depthMax <= doc->settings.depthMin) {
            doc->settings.depthMax = doc->settings.depthMin + 0.001f;
        }
    }
}
void drawRightPanel(app::App& app) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::panel);
    {
        Panel panel("##right_panel", ImVec2(scaled(kRightWidth), 0.0f), 0.0f, true);
        if (panel) {
            Panel content("##right_scroll", ImVec2(0.0f, 0.0f), 18.0f);
            if (content) {
                drawImageSection(app);
                ImGui::BeginDisabled(!app.hasTask());
                drawLightSection(app);
                drawShadingSection(app);
                drawSurfaceSection(app);
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
                ImGui::BeginDisabled(!app.hasTask());
                if (ImGui::Button(app.tr("panel.reset").c_str(),
                                  ImVec2(-1.0f, 0.0f))) {
                    app.resetTask();
                }
                ImGui::EndDisabled();
                ImGui::Dummy(ImVec2(0.0f, 4.0f));
            }
        }
    }
    ImGui::PopStyleColor();
}

} // namespace ui
