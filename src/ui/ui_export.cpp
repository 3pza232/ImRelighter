#include "ui/ui_internal.h"

#include <imgui.h>

#include "app/app.h"

namespace ui {

void drawExportMenu(app::App& app) {
    if (app.ui.exportPopupRequested) {
        ImGui::OpenPopup("##export_menu");
        app.ui.exportPopupRequested = false;
    }

    if (ImGui::BeginPopup("##export_menu")) {
        const core::Document* doc = app.activeDocument();
        const bool enabled = doc && doc->hasDepth && !app.exportBusy();
        if (ImGui::MenuItem(app.tr("view.lit").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Lit);
        }
        if (ImGui::MenuItem(app.tr("view.normal").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Normal);
        }
        if (ImGui::MenuItem(app.tr("view.depth").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Depth);
        }
        if (ImGui::MenuItem(app.tr("view.lighting").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Lighting);
        }
        if (ImGui::MenuItem(app.tr("view.shadow").c_str(), nullptr, false, enabled)) {
            app.exportImage(core::ExportKind::Shadow);
        }
        ImGui::EndPopup();
    }
}

} // namespace ui
