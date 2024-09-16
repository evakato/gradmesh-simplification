#include "gms_app.hpp"

GmsApp::GmsApp()
    : currMesh{readFile(appState.filename)},
      patches{*(currMesh.generatePatchData())},
      patchRenderer{gmsWindow, appState, patches}
{
    initializeOpenGL();
}

void GmsApp::run()
{
    patchRenderer.bindBuffers();

    while (!gmsWindow.shouldClose())
    {
        glfwPollEvents();

        if (appState.currentMode == RENDER_CURVES)
            ; // curveRenderer.render();
        else if (appState.currentMode == RENDER_PATCHES)
            patchRenderer.render();

        gui.render();
        gmsWindow.swapBuffers();
    }
}