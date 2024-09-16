#include "window.hpp"
#include "gui.hpp"
#include "patch.hpp"
#include "renderer.hpp"
#include "curve_renderer.hpp"
#include "patch_renderer.hpp"
#include "curve.hpp"
#include "types.hpp"
#include "fileio.hpp"
#include "gradmesh.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

glm::vec3 col1{0.0f, 1.0f, 0.0f};
glm::vec3 col2{1.0f, 1.0f, 0.0f};
glm::vec3 col3{0.5f, 0.2f, 0.7f};
glm::vec3 col4{1.0f, 0.2f, 0.2f};

Curve curve3{
    {glm::vec2(-0.8f, -0.8f), col1},
    {glm::vec2(-0.4f, 0.8f), col2},
    {glm::vec2(0.4f, 0.8f), col4},
    {glm::vec2(0.5f, 0.0f), col3},
};

std::vector<Curve> curves = {curve3};
std::vector<Patch> patches;

int main()
{
    // std::string filename = "../meshes/recoloring_meshes/local-refinement.hemesh";
    std::string filename = "../meshes/recoloring_meshes/tulips.hemesh";
    // std::string filename = "../meshes/exp_3.hemesh";
    GradMesh newMesh = readFile(filename);

    patches = *(newMesh.generatePatchData());

    GmsWindow gmsWindow{SCR_WIDTH, SCR_HEIGHT, "gms"};
    GLFWwindow *window = gmsWindow.getGLFWwindow();
    GmsGui gmsGui{window, filename};
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
