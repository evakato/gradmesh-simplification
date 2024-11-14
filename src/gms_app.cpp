#include "gms_app.hpp"

GmsApp::GmsApp()
{
    setupDirectories();
    setupNewMesh();
}

void GmsApp::loadMesh()
{
    appState.resetMerges();
    appState.mesh = readHemeshFile(appState.filename);
    appState.updateMeshRender();
    appState.originalGlPatches = appState.patchRenderParams.glPatches;
    merger.startupMesh();
    appState.preprocessingProgress = -1;
}

void GmsApp::setupNewMesh()
{
    loadMesh();
    createDir(SAVES_DIR);
    writeHemeshFile(DEFAULT_FIRST_SAVE_DIR, appState.mesh);
}

void GmsApp::run()
{
    while (!gmsWindow.shouldClose())
    {
        glfwPollEvents();

        if (appState.loadSave)
            loadMesh();

        if (appState.filenameChanged)
            setupNewMesh();

        merger.run();

        switch (appState.currentMode)
        {
        case RENDER_CURVES:
            curveRenderer.render();
            break;
        case RENDER_PATCHES:
            patchRenderer.render(appState.patchRenderParams);
            break;
        }

        gui.render();
        gmsWindow.swapBuffers();
    }
}
