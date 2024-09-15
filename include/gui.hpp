#pragma once

#include "types.hpp"
#include "window.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

class GmsGui
{
public:
    enum RenderMode
    {
        RENDER_PATCHES,
        RENDER_CURVES
    };

    GmsGui(GLFWwindow *window, std::string &filename);
    ~GmsGui();
    void renderGUI();
    bool isCurrentMode(RenderMode mode) { return currentMode == mode; }

    float lineWidth = 4.0f;
    bool isWireframeMode = false;
    int maxHWTessellation;
    glm::vec3 currentColor = glm::vec3{0.45f, 0.55f, 0.6f};
    bool currentColorChanged{false};

    bool drawControlPoints = true;
    bool drawHandles = true;
    float handleLineWidth = 2.0f;
    bool drawCurves = true;
    float curveLineWidth = 2.0f;
    bool drawPatches = true;

    bool patchColorChange{false};

    std::vector<Vertex> currentPatchData;

private:
    void createGUIFrame();
    void showHermiteMatrixTable();
    void showRenderSettings();
    void ShowWindowMenuBar();

    GLFWwindow *window;
    RenderMode currentMode = {RENDER_PATCHES};
    const char *selectedTab;
    bool open = true;

    static constexpr char *modeNames[2] = {(char *)"Patch", (char *)"Curve"};

    ImVec4 imCurrentColor;
    std::string &filename;
};

static constexpr std::array<const char *, 16> hermiteControlMatrixLabels{
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

static std::string extractFileName(const std::string &filepath);