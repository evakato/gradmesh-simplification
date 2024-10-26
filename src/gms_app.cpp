#include "gms_app.hpp"

static bool firstRandomIteration = true;

GmsApp::GmsApp()
{
    setupDirectories();
    setupNewMesh();
}

void GmsApp::readMesh()
{
    appState.resetMerges();
    appState.mesh = readHemeshFile(appState.filename);
    appState.updateMeshRender();
    merger.findCandidateMerges();
    firstRandomIteration = true;
}

void GmsApp::setupNewMesh()
{
    readMesh();
    createDir(SAVES_DIR);
    writeHemeshFile(DEFAULT_FIRST_SAVE_DIR, appState.mesh);
}

void GmsApp::run()
{
    while (!gmsWindow.shouldClose())
    {
        glfwPollEvents();

        if (appState.loadSave)
        {
            readMesh();
            appState.loadSave = false;
        }

        if (appState.filenameChanged)
            setupNewMesh();

        switch (appState.mergeMode)
        {
        case MANUAL:
        {
            appState.mergeStatus = merger.mergeAtSelectedEdge();
            appState.resetEdgeSelection();
            appState.mergeMode = NONE;
            break;
        }
        case RANDOM:
        {
            assert(appState.numOfCandidateMerges > 0);
            appState.generateNewEdgePool(firstRandomIteration);

            appState.mergeStatus = merger.mergeAtSelectedEdge();
            switch (appState.mergeStatus)
            {
            case SUCCESS:
            {
                firstRandomIteration = true;
                appState.resetMergingIteration();
                if (appState.numOfCandidateMerges <= 0)
                    appState.mergeMode = NONE;
                break;
            }
            case CYCLE:
            case METRIC_ERROR:
            {
                if (appState.attemptedMergesIdx < appState.selectedEdgePool.size())
                {
                    appState.setNewEdgeFromPool();
                    break;
                }
                [[fallthrough]];
            }
            case FAILURE:
            {
                appState.resetMergingIteration();
                appState.mergeMode = NONE;
                break;
            }
            }
            break;
        }
        }

        merger.setDoubleHalfEdge();
        appState.resetCurveColors();

        switch (appState.currentMode)
        {
        case RENDER_CURVES:
            // curveRenderer.render();
            break;
        case RENDER_PATCHES:
            patchRenderer.render(appState.patchRenderParams);
            break;
        }

        gui.render();
        gmsWindow.swapBuffers();
    }
}
