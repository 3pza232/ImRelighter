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
    float dragged = static_cast<float>(value);
    const bool changed = ImGui::SliderFloat(
        "##value", &dragged, static_cast<float>(min), static_cast<float>(max),
        "%.0f", ImGuiSliderFlags_AlwaysClamp);
    if (changed) {
        const int rounded = std::clamp(
            static_cast<int>(std::lround(dragged)), min, max);
        if (rounded != value) {
            value = rounded;
            ImGui::PopID();
            return true;
        }
    }
    ImGui::PopID();
    return false;
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

void applyMaterialPreset(core::Document& doc, int preset) {
    core::Settings& settings = doc.settings;
    settings.materialRoughness = 0.30f;
    settings.metalness = 0.0f;
    settings.sheen = 0.0f;
    settings.subsurface = 0.0f;
    settings.specular = 0.04f;
    settings.shininess = 60.0f;

    if (preset == 1) { // skin
        settings.materialRoughness = 0.55f;
        settings.subsurface = 0.35f;
        settings.specular = 0.03f;
        settings.shininess = 35.0f;
    } else if (preset == 2) { // fabric
        settings.materialRoughness = 0.82f;
        settings.sheen = 0.35f;
        settings.specular = 0.02f;
        settings.shininess = 20.0f;
    } else if (preset == 3) { // hair
        settings.materialRoughness = 0.42f;
        settings.sheen = 0.18f;
        settings.subsurface = 0.05f;
        settings.specular = 0.05f;
        settings.shininess = 70.0f;
    } else if (preset == 4) { // metal
        settings.materialRoughness = 0.22f;
        settings.metalness = 0.85f;
        settings.specular = 0.08f;
        settings.shininess = 120.0f;
    }
}

void applyRenderPreset(app::App& app, int preset) {
    core::Document* doc = app.activeDocument();
    if (!doc) return;
    core::Settings& settings = doc->settings;
    settings.lightingModel = core::LightingModel::Physical;
    settings.toneMap = core::ToneMap::Soft;
    settings.ambient = 0.40f;
    settings.ambientColor = "#c0c4cc";
    settings.specular = 0.04f;
    settings.shininess = 60.0f;
    settings.rim = 0.06f;
    settings.detail = 0.15f;
    settings.wrap = 0.20f;
    settings.exposure = 0.0f;
    settings.bias = 0.007f;
    settings.softness = 0.035f;

    if (preset == 1) {
        settings.ambient = 0.50f;
        settings.ambientColor = "#c8ccd4";
        settings.specular = 0.03f;
        settings.shininess = 40.0f;
        settings.rim = 0.04f;
        settings.detail = 0.18f;
        settings.wrap = 0.30f;
        settings.softness = 0.05f;
        settings.exposure = 0.10f;
    } else if (preset == 2) {
        settings.toneMap = core::ToneMap::Aces;
        settings.ambient = 0.30f;
        settings.ambientColor = "#b8c0cc";
        settings.specular = 0.08f;
        settings.shininess = 90.0f;
        settings.rim = 0.10f;
        settings.detail = 0.12f;
        settings.wrap = 0.15f;
        settings.exposure = 0.20f;
    } else if (preset == 3) {
        settings.lightingModel = core::LightingModel::Classic;
        settings.ambient = 0.42f;
        settings.ambientColor = "#b4bddc";
        settings.specular = 0.08f;
        settings.shininess = 60.0f;
        settings.rim = 0.10f;
        settings.detail = 0.15f;
        settings.wrap = 0.20f;
        settings.exposure = 0.0f;
    }
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
    const bool noModel = app.modelPath().empty();
    const bool modelReady = app.modelExists();
    ImGui::TextColored(
        noModel ? theme::muted : (modelReady ? theme::ok : theme::danger), "%s",
        app.tr(noModel ? "model.not_selected"
                       : (modelReady ? "model.ready" : "model.missing"))
            .c_str());

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
    const bool inferenceBusy = app.inferenceBusy();
    const bool runningThis =
        inferenceBusy && active && app.inferenceDocumentId() == active->id;
    const bool runningOther = inferenceBusy && !runningThis;
    ImGui::TextDisabled(
        "%s", app.tr(runningThis ? "depth.running"
                                 : runningOther ? "depth.other_running"
                                                : hasDepth ? "depth.ready"
                                                           : "depth.not_generated")
                  .c_str());

    ImGui::Spacing();
    const bool canGenerate = app.hasTask() && !inferenceBusy;
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

    if (runningThis) {
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
    if (doc->soloLight != 0) {
        ImGui::SameLine();
        ImGui::TextColored(theme::accent, "%s %d",
                           app.tr("light.solo").c_str(), doc->soloLight);
    }
    ImGui::Spacing();

    for (auto& light : doc->lights) {
        ImGui::PushID(light.id);
        if (!light.enabled) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.45f);
        ImGui::PushStyleColor(ImGuiCol_Text, hexToVec4(light.color));
        ImGui::TextUnformatted("●");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        std::string label =
            app.tr("light.name") + " " + std::to_string(light.id);
        if (!light.enabled) label += " · " + app.tr("light.off");
        if (doc->soloLight == light.id) label += " · " + app.tr("light.solo");
        if (ImGui::Selectable(label.c_str(),
                              doc->selectedLight == light.id)) {
            doc->selectedLight = light.id;
        }
        if (!light.enabled) ImGui::PopStyleVar();
        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted(app.tr("light.preset").c_str());
    const float lightPresetGap = scaled(4.0f);
    const float lightPresetWidth =
        (ImGui::GetContentRegionAvail().x - 2.0f * lightPresetGap) / 3.0f;
    if (ImGui::Button(app.tr("light.preset_three").c_str(),
                      ImVec2(lightPresetWidth, 0.0f))) {
        doc->lights = {
            core::makeLight(1, {-1.35f, 1.15f, 1.8f}),
            core::makeLight(2, {1.45f, 0.25f, 1.5f}),
            core::makeLight(3, {0.25f, 0.85f, 1.9f}),
        };
        doc->lights[0].color = "#ffe3c4";
        doc->lights[0].intensity = 2.2f;
        doc->lights[1].color = "#c8d8ff";
        doc->lights[1].intensity = 1.1f;
        doc->lights[2].color = "#ffffff";
        doc->lights[2].type = core::LightType::Spot;
        doc->lights[2].spotAngle = 48.0f;
        doc->lights[2].intensity = 1.8f;
        doc->selectedLight = 1;
        doc->soloLight = 0;
    }
    ImGui::SameLine(0.0f, lightPresetGap);
    if (ImGui::Button(app.tr("light.preset_window").c_str(),
                      ImVec2(lightPresetWidth, 0.0f))) {
        doc->lights = {core::makeLight(1, {-0.8f, 0.7f, 2.2f})};
        doc->lights[0].color = "#e8f0ff";
        doc->lights[0].intensity = 2.4f;
        doc->lights[0].range = 6.0f;
        doc->selectedLight = 1;
        doc->soloLight = 0;
    }
    ImGui::SameLine(0.0f, lightPresetGap);
    if (ImGui::Button(app.tr("light.preset_rim").c_str(),
                      ImVec2(lightPresetWidth, 0.0f))) {
        doc->lights = {
            core::makeLight(1, {-0.45f, 0.45f, 2.0f}),
            core::makeLight(2, {0.5f, 0.8f, -1.6f}),
        };
        doc->lights[0].color = "#ffe6c8";
        doc->lights[0].intensity = 1.8f;
        doc->lights[1].color = "#cfe4ff";
        doc->lights[1].intensity = 2.6f;
        doc->lights[1].range = 4.5f;
        doc->selectedLight = 1;
        doc->soloLight = 0;
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
        const int removedId = doc->selectedLight;
        doc->lights.erase(
            std::remove_if(doc->lights.begin(), doc->lights.end(),
                           [&](const core::Light& light) {
                               return light.id == removedId;
                           }),
            doc->lights.end());
        if (doc->soloLight == removedId) doc->soloLight = 0;
        if (!doc->lights.empty()) {
            doc->selectedLight = doc->lights.front().id;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    bool selectedEnabled = true;
    for (const auto& light : doc->lights) {
        if (light.id == doc->selectedLight) selectedEnabled = light.enabled;
    }
    if (ImGui::Button(
            app.tr(selectedEnabled ? "light.disable" : "light.enable").c_str())) {
        for (auto& light : doc->lights) {
            if (light.id == doc->selectedLight) {
                light.enabled = !light.enabled;
            }
        }
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(doc->lights.size() >= 8);
    if (ImGui::Button(app.tr("light.duplicate").c_str())) {
        auto source = std::find_if(
            doc->lights.begin(), doc->lights.end(),
            [&](const core::Light& light) {
                return light.id == doc->selectedLight;
            });
        if (source != doc->lights.end()) {
            int nextId = 1;
            for (const auto& light : doc->lights) {
                nextId = std::max(nextId, light.id + 1);
            }
            core::Light copy = *source;
            copy.id = nextId;
            copy.position[0] += 0.25f;
            copy.position[1] += 0.15f;
            doc->lights.push_back(copy);
            doc->selectedLight = nextId;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(app.tr("light.solo").c_str())) {
        doc->soloLight =
            doc->soloLight == doc->selectedLight ? 0 : doc->selectedLight;
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
        const float positionMin[3] = {-3.0f, -3.0f, 0.1f};
        const float positionMax[3] = {3.0f, 3.0f, 4.0f};
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
        sliderRow(app, "light.intensity", selected->intensity, 0.0f, 12.0f,
                  "%.2f");
        sliderRow(app, "light.range", selected->range, 0.2f, 12.0f, "%.2f");

        ImGui::TextUnformatted(app.tr("light.type").c_str());
        const float typeWidth =
            (ImGui::GetContentRegionAvail().x - scaled(4.0f)) / 2.0f;
        if (segmentButton(app.tr("light.point").c_str(),
                          selected->type == core::LightType::Point,
                          ImVec2(typeWidth, 0.0f))) {
            selected->type = core::LightType::Point;
        }
        ImGui::SameLine(0.0f, scaled(4.0f));
        if (segmentButton(app.tr("light.spot").c_str(),
                          selected->type == core::LightType::Spot,
                          ImVec2(typeWidth, 0.0f))) {
            selected->type = core::LightType::Spot;
        }
        if (selected->type == core::LightType::Spot) {
            sliderRow(app, "light.spot_angle", selected->spotAngle, 5.0f,
                      120.0f, "%.0f°");
            sliderRow(app, "light.spot_softness", selected->spotSoftness, 0.0f,
                      1.0f, "%.2f");
        }
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
    sliderRow(app, "shading.ambient", doc->settings.ambient, 0.0f, 3.0f,
              "%.2f");
    sliderRow(app, "shading.wrap", doc->settings.wrap, 0.0f, 1.5f, "%.2f");

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.shadows").c_str());
    checkboxRow(app, "shading.shadows", doc->settings.shadows);
    ImGui::BeginDisabled(!doc->settings.shadows);
    sliderRow(app, "shading.bias", doc->settings.bias, 0.0001f, 0.2f,
              "%.4f");
    sliderRow(app, "shading.softness", doc->settings.softness, 0.0f, 0.5f,
              "%.3f");
    ImGui::EndDisabled();

    ImGui::Spacing();
    if (ImGui::CollapsingHeader(
            (app.tr("shading.highlights") + "###highlights").c_str())) {
        sliderRow(app, "shading.specular", doc->settings.specular, 0.0f,
                  3.0f, "%.2f");
        sliderRow(app, "shading.shininess", doc->settings.shininess, 1.0f,
                  512.0f, "%.0f");
        sliderRow(app, "shading.rim", doc->settings.rim, 0.0f, 3.0f, "%.2f");
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

    const float lookGap = scaled(4.0f);
    const float lookWidth =
        (ImGui::GetContentRegionAvail().x - lookGap) / 2.0f;
    ImGui::PushID("look_clipboard");
    if (ImGui::Button(app.tr("look.copy").c_str(),
                      ImVec2(lookWidth, 0.0f))) {
        app.copyLook();
    }
    ImGui::SameLine(0.0f, lookGap);
    ImGui::BeginDisabled(!app.hasLookClipboard());
    if (ImGui::Button(app.tr("look.paste").c_str(),
                      ImVec2(lookWidth, 0.0f))) {
        app.pasteLook();
    }
    ImGui::EndDisabled();
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("surface.render_preset").c_str());
    // "电影" and "经典" also appear in the tone-map and lighting-model rows;
    // scope each group so the shared labels get distinct ImGui IDs.
    ImGui::PushID("render_preset");
    const float presetGap = scaled(4.0f);
    const float presetWidth =
        (ImGui::GetContentRegionAvail().x - 3.0f * presetGap) / 4.0f;
    const char* presetKeys[] = {"preset.natural", "preset.portrait",
                                "preset.cinema", "preset.classic"};
    for (int i = 0; i < 4; ++i) {
        if (i > 0) ImGui::SameLine(0.0f, presetGap);
        if (segmentButton(app.tr(presetKeys[i]).c_str(), false,
                          ImVec2(presetWidth, 0.0f))) {
            applyRenderPreset(app, i);
        }
    }
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::TextUnformatted(app.tr("surface.lighting_model").c_str());
    ImGui::PushID("lighting_model");
    const float modelWidth =
        (ImGui::GetContentRegionAvail().x - presetGap) / 2.0f;
    if (segmentButton(app.tr("lighting.classic").c_str(),
                      doc->settings.lightingModel ==
                          core::LightingModel::Classic,
                      ImVec2(modelWidth, 0.0f))) {
        doc->settings.lightingModel = core::LightingModel::Classic;
    }
    ImGui::SameLine(0.0f, presetGap);
    if (segmentButton(app.tr("lighting.physical").c_str(),
                      doc->settings.lightingModel ==
                          core::LightingModel::Physical,
                      ImVec2(modelWidth, 0.0f))) {
        doc->settings.lightingModel = core::LightingModel::Physical;
    }
    ImGui::PopID();

    ImGui::TextUnformatted(app.tr("surface.tone_map").c_str());
    ImGui::PushID("tone_map");
    if (segmentButton(app.tr("tone.soft").c_str(),
                      doc->settings.toneMap == core::ToneMap::Soft,
                      ImVec2(modelWidth, 0.0f))) {
        doc->settings.toneMap = core::ToneMap::Soft;
    }
    ImGui::SameLine(0.0f, presetGap);
    if (segmentButton(app.tr("tone.aces").c_str(),
                      doc->settings.toneMap == core::ToneMap::Aces,
                      ImVec2(modelWidth, 0.0f))) {
        doc->settings.toneMap = core::ToneMap::Aces;
    }
    ImGui::PopID();

    ImGui::Spacing();
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
    sliderRow(app, "surface.height", doc->settings.height, 0.01f, 3.0f,
              "%.2f");
    sliderRow(app, "surface.normal_strength", doc->settings.normalStrength,
              0.1f, 4.0f, "%.2f");
    sliderRow(app, "surface.smooth", doc->settings.smooth, 0.0f, 6.0f,
              "%.1f");
    sliderRow(app, "surface.detail", doc->settings.detail, 0.0f, 1.0f,
              "%.2f");
    checkboxRow(app, "surface.line_assist", doc->settings.lineAssist);
    checkboxRow(app, "surface.invert", doc->settings.invert);

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("material.title").c_str());
    ImGui::TextUnformatted(app.tr("material.preset").c_str());
    ImGui::PushID("material_preset");
    const float materialGap = scaled(3.0f);
    const float materialWidth =
        (ImGui::GetContentRegionAvail().x - 4.0f * materialGap) / 5.0f;
    const char* materialKeys[] = {"material.default", "material.skin",
                                  "material.fabric", "material.hair",
                                  "material.metal"};
    for (int i = 0; i < 5; ++i) {
        if (i > 0) ImGui::SameLine(0.0f, materialGap);
        if (segmentButton(app.tr(materialKeys[i]).c_str(), false,
                          ImVec2(materialWidth, 0.0f))) {
            applyMaterialPreset(*doc, i);
        }
    }
    ImGui::PopID();
    sliderRow(app, "material.roughness", doc->settings.materialRoughness, 0.05f,
              1.0f, "%.2f");
    sliderRow(app, "material.metalness", doc->settings.metalness, 0.0f, 1.0f,
              "%.2f");
    sliderRow(app, "material.sheen", doc->settings.sheen, 0.0f, 1.0f, "%.2f");
    sliderRow(app, "material.subsurface", doc->settings.subsurface, 0.0f, 1.0f,
              "%.2f");

    ImGui::Spacing();
    ImGui::SeparatorText(app.tr("section.composite").c_str());
    sliderRow(app, "surface.mix", doc->settings.mix, 0.0f, 1.0f, "%.2f");
    sliderRow(app, "surface.exposure", doc->settings.exposure, -5.0f, 5.0f,
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
        // Use a single scrollable child. Nesting a zero-height child inside
        // another zero-height child makes Dear ImGui auto-fit both windows, which
        // silently removes the scroll range and hides every section below the
        // first screenful.
        const float panelHeight =
            std::max(1.0f, ImGui::GetContentRegionAvail().y);
        const float panelWidth = scaled(kRightWidth);
        Panel content("##right_panel", ImVec2(panelWidth, panelHeight), 18.0f,
                      true,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
        if (content) {
            // Cards are child windows, so ImGui routes the wheel to the hovered
            // card instead of this panel. Test the panel's real window rect and
            // scroll it explicitly; the cursor is not always where the layout
            // cursor was before BeginChild().
            const ImVec2 panelMin = ImGui::GetWindowPos();
            const ImVec2 panelSize = ImGui::GetWindowSize();
            const ImVec2 panelMax(panelMin.x + panelSize.x,
                                  panelMin.y + panelSize.y);
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool overPanel =
                !app.ui.settingsOpen && mouse.x >= panelMin.x &&
                mouse.x < panelMax.x && mouse.y >= panelMin.y &&
                mouse.y < panelMax.y;
            const float wheel =
                overPanel ? ImGui::GetIO().MouseWheel : 0.0f;
            if (wheel != 0.0f) {
                const float step =
                    ImGui::GetTextLineHeightWithSpacing() * 4.0f;
                ImGui::SetScrollY(ImGui::GetScrollY() - wheel * step);
            }

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
    ImGui::PopStyleColor();
}

} // namespace ui
