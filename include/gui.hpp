#pragma once

#include <filesystem>

#include "types.hpp"
#include "window.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
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
    void showHermiteMatrixTable();
    void showRenderSettings();
    void showWindowMenuBar();

    GmsWindow &gmsWindow;
    GmsAppState &appState;

    std::filesystem::path meshDir = "../meshes";
    ImGui::FileBrowser fileDialog{0, meshDir};
};

void createImguiContext(GLFWwindow *window);
void prepareImguiFrame(GLFWwindow *window);
void processKeyInput(GLFWwindow *window, const GmsAppState &appState, ImGui::FileBrowser &fileDialog);

constexpr char *modeNames[2] = {(char *)"Patch", (char *)"Curve"};
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

static std::string extractFileName(const std::string &filepath);