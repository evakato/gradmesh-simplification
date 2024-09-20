#include "gms_app.hpp"

GmsApp::GmsApp() : currMesh{readFile(appState.filename)},
                   patches{*(currMesh.generatePatchData())}
{
    initializeOpenGL();
    patchRenderer.bindBuffers();
}

void GmsApp::run()
{
    while (!gmsWindow.shouldClose())
    {
        glfwPollEvents();

        if (appState.filenameChanged)
        {
            currMesh = readFile(appState.filename);
            patches = *(currMesh.generatePatchData());
            appState.filenameChanged = false;
        }
        if (appState.doMerge)
        {
            Merging::merge(currMesh);
            patches = *(currMesh.generatePatchData());
            appState.doMerge = false;
        }

        if (appState.currentMode == RENDER_CURVES)
            ; // curveRenderer.render();
        else if (appState.currentMode == RENDER_PATCHES)
            patchRenderer.render(patches);

        gui.render();
        gmsWindow.swapBuffers();
    }
}