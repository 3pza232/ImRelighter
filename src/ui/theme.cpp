#include "ui/theme.h"

namespace theme {
namespace {

struct Palette {
    ImVec4 bg, canvas, panel, panel2, border, text, muted, accent, accentCool;
    ImVec4 accentHover, danger, dangerHover, ok, ink, closeText, taskHover;
    ImVec4 glowCore, overlay, fieldHover, fieldActive, buttonHover, buttonActive;
    ImVec4 header, headerHover, headerActive, tabHover, tabDimSelected;
    ImVec4 sliderActive, plotHover, textSelected, modalDim;
    ImVec4 scrollbarBg, scrollbarGrab, scrollbarHover, scrollbarActive;
    ImVec4 axisX, axisY, axisZ;
};

Palette darkPalette() {
    Palette p;
    p.bg = {0.067f, 0.075f, 0.086f, 1.0f};
    p.canvas = {0.043f, 0.051f, 0.059f, 1.0f};
    p.panel = {0.094f, 0.106f, 0.122f, 1.0f};
    p.panel2 = {0.125f, 0.141f, 0.165f, 1.0f};
    p.border = {0.165f, 0.188f, 0.220f, 1.0f};
    p.text = {0.906f, 0.914f, 0.925f, 1.0f};
    p.muted = {0.541f, 0.569f, 0.612f, 1.0f};
    p.accent = {0.910f, 0.702f, 0.416f, 1.0f};
    p.accentCool = {0.416f, 0.663f, 0.910f, 1.0f};
    p.accentHover = {0.960f, 0.760f, 0.480f, 1.0f};
    p.danger = {0.878f, 0.416f, 0.416f, 1.0f};
    p.dangerHover = {0.960f, 0.500f, 0.500f, 1.0f};
    p.ok = {0.498f, 0.753f, 0.541f, 1.0f};
    p.ink = {0.070f, 0.070f, 0.070f, 1.0f};
    p.closeText = {1.0f, 1.0f, 1.0f, 1.0f};
    p.taskHover = {0.102f, 0.114f, 0.133f, 1.0f};
    p.glowCore = {1.0f, 0.933f, 0.824f, 1.0f};
    p.overlay = {0.016f, 0.020f, 0.024f, 0.590f};
    p.fieldHover = {0.170f, 0.190f, 0.220f, 1.0f};
    p.fieldActive = {0.200f, 0.220f, 0.250f, 1.0f};
    p.buttonHover = {0.200f, 0.230f, 0.270f, 1.0f};
    p.buttonActive = {0.240f, 0.270f, 0.310f, 1.0f};
    p.header = {0.160f, 0.180f, 0.210f, 1.0f};
    p.headerHover = {0.200f, 0.230f, 0.270f, 1.0f};
    p.headerActive = {0.240f, 0.270f, 0.310f, 1.0f};
    p.tabHover = {0.200f, 0.230f, 0.270f, 1.0f};
    p.tabDimSelected = {0.100f, 0.110f, 0.130f, 1.0f};
    p.sliderActive = {0.980f, 0.800f, 0.530f, 1.0f};
    p.plotHover = {0.980f, 0.800f, 0.530f, 1.0f};
    p.textSelected = {0.910f, 0.700f, 0.420f, 0.300f};
    p.modalDim = {0.020f, 0.020f, 0.030f, 0.650f};
    p.scrollbarBg = {0.0f, 0.0f, 0.0f, 0.0f};
    p.scrollbarGrab = p.border;
    p.scrollbarHover = p.muted;
    p.scrollbarActive = p.accent;
    p.axisX = {0.878f, 0.420f, 0.420f, 1.0f};
    p.axisY = {0.450f, 0.753f, 0.541f, 1.0f};
    p.axisZ = {0.416f, 0.663f, 0.910f, 1.0f};
    return p;
}

Palette lightPalette() {
    Palette p;
    p.bg = {0.949f, 0.953f, 0.961f, 1.0f};
    p.canvas = {0.898f, 0.906f, 0.918f, 1.0f};
    p.panel = {1.0f, 1.0f, 1.0f, 1.0f};
    p.panel2 = {0.965f, 0.969f, 0.976f, 1.0f};
    p.border = {0.827f, 0.843f, 0.867f, 1.0f};
    p.text = {0.118f, 0.133f, 0.153f, 1.0f};
    p.muted = {0.420f, 0.447f, 0.490f, 1.0f};
    p.accent = {0.690f, 0.435f, 0.176f, 1.0f};
    p.accentCool = {0.184f, 0.435f, 0.682f, 1.0f};
    p.accentHover = {0.600f, 0.350f, 0.118f, 1.0f};
    p.danger = {0.784f, 0.278f, 0.278f, 1.0f};
    p.dangerHover = {0.878f, 0.353f, 0.353f, 1.0f};
    p.ok = {0.176f, 0.541f, 0.294f, 1.0f};
    p.ink = {1.0f, 1.0f, 1.0f, 1.0f};
    p.closeText = {1.0f, 1.0f, 1.0f, 1.0f};
    p.taskHover = {0.933f, 0.941f, 0.953f, 1.0f};
    p.glowCore = {1.0f, 0.973f, 0.929f, 1.0f};
    p.overlay = {0.050f, 0.060f, 0.080f, 0.340f};
    p.fieldHover = {0.933f, 0.941f, 0.953f, 1.0f};
    p.fieldActive = {0.910f, 0.922f, 0.941f, 1.0f};
    p.buttonHover = {0.925f, 0.937f, 0.953f, 1.0f};
    p.buttonActive = {0.898f, 0.914f, 0.937f, 1.0f};
    p.header = {0.933f, 0.941f, 0.953f, 1.0f};
    p.headerHover = {0.898f, 0.914f, 0.937f, 1.0f};
    p.headerActive = {0.870f, 0.890f, 0.918f, 1.0f};
    p.tabHover = {0.925f, 0.937f, 0.953f, 1.0f};
    p.tabDimSelected = {0.965f, 0.969f, 0.976f, 1.0f};
    p.sliderActive = {0.600f, 0.350f, 0.118f, 1.0f};
    p.plotHover = {0.690f, 0.435f, 0.176f, 1.0f};
    p.textSelected = {0.690f, 0.435f, 0.176f, 0.200f};
    p.modalDim = {0.200f, 0.220f, 0.260f, 0.340f};
    p.scrollbarBg = {0.0f, 0.0f, 0.0f, 0.0f};
    p.scrollbarGrab = {0.780f, 0.800f, 0.830f, 1.0f};
    p.scrollbarHover = {0.600f, 0.630f, 0.670f, 1.0f};
    p.scrollbarActive = p.accent;
    p.axisX = {0.780f, 0.300f, 0.300f, 1.0f};
    p.axisY = {0.250f, 0.550f, 0.350f, 1.0f};
    p.axisZ = {0.250f, 0.450f, 0.720f, 1.0f};
    return p;
}

Mode currentMode = Mode::Dark;

void assign(const Palette& p) {
    bg = p.bg;
    canvas = p.canvas;
    panel = p.panel;
    panel2 = p.panel2;
    border = p.border;
    text = p.text;
    muted = p.muted;
    accent = p.accent;
    accentCool = p.accentCool;
    accentHover = p.accentHover;
    danger = p.danger;
    dangerHover = p.dangerHover;
    ok = p.ok;
    ink = p.ink;
    closeText = p.closeText;
    taskHover = p.taskHover;
    glowCore = p.glowCore;
    overlay = p.overlay;
    fieldHover = p.fieldHover;
    fieldActive = p.fieldActive;
    buttonHover = p.buttonHover;
    buttonActive = p.buttonActive;
    header = p.header;
    headerHover = p.headerHover;
    headerActive = p.headerActive;
    tabHover = p.tabHover;
    tabDimSelected = p.tabDimSelected;
    sliderActive = p.sliderActive;
    plotHover = p.plotHover;
    textSelected = p.textSelected;
    modalDim = p.modalDim;
    scrollbarBg = p.scrollbarBg;
    scrollbarGrab = p.scrollbarGrab;
    scrollbarHover = p.scrollbarHover;
    scrollbarActive = p.scrollbarActive;
    axisX = p.axisX;
    axisY = p.axisY;
    axisZ = p.axisZ;
}

} // namespace

Mode mode() { return currentMode; }

void setMode(Mode mode) {
    currentMode = mode;
    assign(mode == Mode::Light ? lightPalette() : darkPalette());
    apply();
}

void apply() {
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 0.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 8.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.TabRounding = 7.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.SeparatorTextBorderSize = 1.0f;

    style.WindowPadding = ImVec2(16.0f, 16.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 10.0f;
    style.IndentSpacing = 18.0f;
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.DisabledAlpha = 0.48f;
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    const ImVec4 transparent(0.0f, 0.0f, 0.0f, 0.0f);
    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = muted;
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_ChildBg] = panel;
    c[ImGuiCol_PopupBg] = panel2;
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_BorderShadow] = transparent;

    c[ImGuiCol_FrameBg] = panel2;
    c[ImGuiCol_FrameBgHovered] = fieldHover;
    c[ImGuiCol_FrameBgActive] = fieldActive;

    c[ImGuiCol_TitleBg] = panel;
    c[ImGuiCol_TitleBgActive] = panel;
    c[ImGuiCol_TitleBgCollapsed] = panel;
    c[ImGuiCol_MenuBarBg] = panel;

    c[ImGuiCol_ScrollbarBg] = scrollbarBg;
    c[ImGuiCol_ScrollbarGrab] = scrollbarGrab;
    c[ImGuiCol_ScrollbarGrabHovered] = scrollbarHover;
    c[ImGuiCol_ScrollbarGrabActive] = scrollbarActive;

    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = sliderActive;

    c[ImGuiCol_Button] = panel2;
    c[ImGuiCol_ButtonHovered] = buttonHover;
    c[ImGuiCol_ButtonActive] = buttonActive;

    c[ImGuiCol_Header] = header;
    c[ImGuiCol_HeaderHovered] = headerHover;
    c[ImGuiCol_HeaderActive] = headerActive;

    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_SeparatorHovered] = accent;
    c[ImGuiCol_SeparatorActive] = accent;

    c[ImGuiCol_ResizeGrip] = transparent;
    c[ImGuiCol_ResizeGripHovered] = accent;
    c[ImGuiCol_ResizeGripActive] = accent;

    c[ImGuiCol_Tab] = panel;
    c[ImGuiCol_TabHovered] = tabHover;
    c[ImGuiCol_TabSelected] = bg;
    c[ImGuiCol_TabSelectedOverline] = accent;
    c[ImGuiCol_TabDimmed] = panel;
    c[ImGuiCol_TabDimmedSelected] = tabDimSelected;

    c[ImGuiCol_PlotLines] = accentCool;
    c[ImGuiCol_PlotLinesHovered] = accent;
    c[ImGuiCol_PlotHistogram] = accent;
    c[ImGuiCol_PlotHistogramHovered] = plotHover;

    c[ImGuiCol_TextSelectedBg] = textSelected;
    c[ImGuiCol_DragDropTarget] = accent;
    c[ImGuiCol_NavCursor] = accent;
    c[ImGuiCol_ModalWindowDimBg] = modalDim;

    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
}

} // namespace theme