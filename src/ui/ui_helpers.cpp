#include "ui/ui_internal.h"

#include "app/app.h"
#include "core/color.h"
#include "ui/theme.h"

namespace ui {

ImVec4 hexToVec4(const std::string& hex) {
    const auto color = core::parseHexColor(hex);
    if (!color) return theme::text;
    return ImVec4((*color)[0], (*color)[1], (*color)[2], 1.0f);
}

std::string backendLabel(app::App& app, core::Backend backend) {
    switch (backend) {
        case core::Backend::Cpu: return app.tr("settings.cpu");
        case core::Backend::Dml: return app.tr("settings.dml");
        case core::Backend::Auto: return app.tr("settings.auto");
    }
    return app.tr("settings.auto");
}

} // namespace ui
