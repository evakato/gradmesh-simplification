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
    appState.setUnmergedGlPatches();
    merger.startupMesh();
    appState.preprocessProductRegionsProgress = -1;
    preprocessor.setEdgeRegions();
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

        switch (appState.mergeProcess)
        {
        case MergeProcess::PreprocessSingleMerge:
            preprocessor.preprocessSingleMergeError();
            break;
        case MergeProcess::PreprocessProductRegions:
            preprocessor.preprocessProductRegions();
            break;
        case MergeProcess::ViewEdgeMap:
            merger.metrics.generateEdgeErrorMap(appState.edgeErrorDisplay);
            break;
        case MergeProcess::Merging:
            merger.merge();
            break;
        }

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
