#include "window.hpp"
#include "gui.hpp"
#include "patch.hpp"
#include "renderer.hpp"
#include "curve_renderer.hpp"
#include "patch_renderer.hpp"
#include "curve.hpp"
#include "types.hpp"
#include "fileio.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

glm::vec3 col1{0.0f, 1.0f, 0.0f};
glm::vec3 col2{1.0f, 1.0f, 0.0f};
glm::vec3 col3{0.5f, 0.2f, 0.7f};
glm::vec3 col4{1.0f, 0.2f, 0.2f};
glm::vec3 green{0.0f, 1.0f, 0.0f};
glm::vec3 blue{0.0f, 0.0f, 1.0f};
glm::vec3 red{1.0f, 0.0f, 0.0f};
glm::vec3 magenta{1.0f, 0.0f, 1.0f};
glm::vec3 black{0.0f, 0.0, 0.0};

Curve curve3{
    {glm::vec2(-0.8f, -0.8f), col1},
    {glm::vec2(-0.4f, 0.8f), col2},
    {glm::vec2(0.4f, 0.8f), col4},
    {glm::vec2(0.5f, 0.0f), col3},
};

std::vector<Curve> curves = {curve3};

/*
std::vector<Vertex> corners {
    {glm::vec2(-0.5f, -0.5f), col1},
    {glm::vec2(-0.7f, 0.7f), col4},
    {glm::vec2(0.5f, -0.5f), col2},
    {glm::vec2(0.5f, 0.5f), col3},
};

std::vector<Vertex> handles{
    {glm::vec2(0.0f, 1.0f), black},
    {glm::vec2(0.0f, 1.0f), black},
    {glm::vec2(1.0f, 0.0f), black},
    {glm::vec2(1.0f, 0.0f), black},
    {glm::vec2(1.0f, 0.0f), black},
    {glm::vec2(1.0f, 0.0f), black},
    {glm::vec2(0.0f, 1.0f), black},
    {glm::vec2(0.0f, 1.0f), black},
};
*/

std::vector<Vertex> corners{
    {glm::vec2(0.5f, -0.5f), green},
    {glm::vec2(0.5f, 0.5f), blue},
    {glm::vec2(-0.5f, -0.5f), red},
    {glm::vec2(-0.5f, 0.5f), magenta},
};

std::vector<Vertex> corners2{
    {glm::vec2(0.0f, 0.0f), blue},
    {glm::vec2(0.0f, 0.5f), green},
    {glm::vec2(-0.5f, 0.0f), red},
    {glm::vec2(-0.5f, 0.5f), magenta},
};
std::vector<Vertex> corners3{
    {glm::vec2(0.5f, 0.0f), red},
    {glm::vec2(0.5f, 0.5f), green},
    {glm::vec2(0.0f, 0.0f), blue},
    {glm::vec2(0.0f, 0.5f), magenta},
};

std::vector<Vertex> handles{
    {glm::vec2(0.0f, 1.0f), black},
    {glm::vec2(0.0f, 1.0f), black},
    {glm::vec2(-1.0f, 0.0f), black},
    {glm::vec2(-1.0f, 0.0f), black},
    {glm::vec2(-1.0f, 0.0f), black},
    {glm::vec2(-1.0f, 0.0f), black},
    {glm::vec2(0.0f, 1.0f), black},
    {glm::vec2(0.0f, 1.0f), black},
};

std::vector<Vertex> twists{
    {glm::vec2(0.0f, 0.0f), black},
    {glm::vec2(0.0f, 0.0f), black},
    {glm::vec2(0.0f, 0.0f), black},
    {glm::vec2(0.0f, 0.0f), black},
};

Patch patch1{corners2, handles, twists};
Patch patch2{corners3, handles, twists};
std::vector<Patch> patches;

int main()
{
    patches = readFile("../meshes/global_duck.hemesh");
    // patches = {patches[0]};
    GmsWindow gmsWindow{SCR_WIDTH, SCR_HEIGHT, "gms"};
    GLFWwindow *window = gmsWindow.getGLFWwindow();
    GmsGui gmsGui{window};
    initializeOpenGL();

    CurveRenderer curveRenderer{gmsWindow, gmsGui, curves};
    PatchRenderer patchRenderer{gmsWindow, gmsGui, patches};

    while (!gmsWindow.shouldClose())
    {
        glfwPollEvents();

        if (gmsGui.isCurrentMode(GmsGui::RENDER_CURVES))
            curveRenderer.render();
        else if (gmsGui.isCurrentMode(GmsGui::RENDER_PATCHES))
            patchRenderer.render();

        gmsGui.renderGUI();
        gmsWindow.swapBuffers();
    }

    return 0;
}
