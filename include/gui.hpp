#pragma once

#include <filesystem>
#include <sstream>
#include <string>

#include "curve_renderer.hpp"
#include "fileio.hpp"
#include "gms_appstate.hpp"
#include "gradmesh.hpp"
#include "merge_metrics.hpp"
#include "types.hpp"
#include "window.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "../assets/fonts/IconsFontAwesome6.h"
#include <GLFW/glfw3.h>

#include "imfilebrowser.h"

class GmsAppState;

class GmsGui
{
public:
    GmsGui(GmsWindow &window, GmsAppState &appState);
    ~GmsGui();
    void render();

private:
    void showRightBar();
    void showWindowMenuBar();

    GmsWindow &gmsWindow;
    GmsAppState &appState;

    std::filesystem::path meshDir = "../meshes";
    ImGui::FileBrowser fileDialog{0, meshDir};
};

enum class ButtonColor
{
    OriginPoint,
    Face,
    Handles,
    PrevNext,
    Twin,
    Parent,
    Children
};

void showRenderSettings(GmsAppState &appState);
void showCurvesTab(bool isRenderCurves, bool firstRender, GmsAppState &appState);
void showPatchesTab(bool isRenderCurves, bool firstRender, GmsAppState &appState);

void createImguiContext(GLFWwindow *window);
void prepareImguiFrame(GLFWwindow *window);
void processKeyInput(GLFWwindow *window, const GmsAppState &appState, ImGui::FileBrowser &fileDialog);
void showMergingMenu(GmsAppState &appState);
void showDebuggingMenu(GmsAppState &appState);
void showDCELInfo(GmsAppState &appState);

void showHermiteMatrixTable(GmsAppState &appState);
void showPreviousMergeInfo(GmsAppState::MergeStats &stats);
void showGradMeshInfo(const GradMesh &mesh);
void setHalfEdgeInfo(const auto &components, int &item_selected_idx, const auto &idxs);
void pushColor(ButtonColor color);

void createListItems(std::vector<int> &idxs, std::vector<std::string> &dynamicItems, std::vector<const char *> &items, std::string ctype);
void createListBox(std::vector<const char *> &items, int &item_selected_idx, int &item_highlighted_idx);
void setComponentText(const auto &components, int &item_selected_idx, const auto &idxs);
void compButton(int &item_selected_idx, const auto &idxs, const int findIdx, std::string text);

constexpr std::array<const char *, 16> hermiteControlMatrixLabels{
    "S(0,0)",
    "S_v(0,0)",
    "S_v(0,1)",
    "S(0,1)",
    "S_u(0,0)",
    "S_uv(0,0)",
    "S_uv(0,1)",
    "S_u(0,1)",
    "S_u(1,0)",
    "S_uv(1,0)",
    "S_uv(1,1)",
    "S_u(1,1)",
    "S(1,0)",
    "S_v(1,0)",
    "S_v(1,1)",
    "S(1,1)",
};

constexpr const char *toString(MergeStatus status)
{
    switch (status)
    {
    case SUCCESS:
        return "Success";
    case METRIC_ERROR:
        return "Metric error";
    case CYCLE:
        return "Cycle";
    default:
        return "N/A";
    }
}

constexpr const char *toString(int cornerFlags)
{
    switch (cornerFlags)
    {
    case IsStem:
        return "Is stem";
    case LeftT | RightT:
        return "LeftT and RightT";
    case LeftL | RightT:
    case RightT:
        return "RightT (w/ or w/o LeftL)";
    case LeftT | RightL:
    case LeftT:
        return "LeftT (w/ or w/o RightL)";
    case LeftL | RightL:
    case RightL:
        return "RightL (w/ or w/o LeftL)";
    case LeftL:
        return "LeftL";
    default:
        return "None";
    }
}

constexpr const char *renderModeStrings[] = {"Patch", "Curve"};
constexpr const char *metric_mode_items[] = {"SSIM", "FLIP"};
constexpr const char *edge_select_items[] = {"Manual", "Random", "Grid", "Dual Grid"};
static int edge_select_current = DUAL_GRID;

inline constexpr int GUI_IMAGE_SIZE{150};
inline constexpr float INPUT_WIDTH{170.0f};