#include "gms_app.hpp"

static bool firstRandomIteration = true;

GmsApp::GmsApp()
{
    createDir(LOGS_DIR);
    createDir(IMAGE_DIR);
    createDir(SAVES_DIR);
    setupNewMesh();
    glGenTextures(1, &appState.unmergedTexture);
    glGenTextures(1, &appState.mergedTexture);
}

void GmsApp::setupNewMesh()
{
    appState.mesh = readHemeshFile(appState.filename);

    appState.updateMeshRender();
    appState.resetMerges();

    merger.findCandidateMerges();
    patchRenderer.render(appState.glPatches, appState.patches, appState.tangentHandles);
    appState.selectedEdgeId = -1;
    prevSelectedEdgeId = -1;

    writeHemeshFile("mesh_saves/save_0.hemesh", appState.mesh);
    firstRandomIteration = true;
}

void GmsApp::run()
{
    while (!gmsWindow.shouldClose())
    {
        static auto lastTime = std::chrono::steady_clock::now();
        auto currentTime = std::chrono::steady_clock::now();
        glfwPollEvents();

        if (appState.filenameChanged)
        {
            setupNewMesh();
        }

        if (appState.selectedEdgeId != prevSelectedEdgeId)
        {
            resetCurveColors();
        }

        switch (appState.mergeMode)
        {
        case MANUAL:
        {
            MergeStatus status = merger.mergeAtSelectedEdge();
            appState.mergeStatus = status;
            if (status == SUCCESS)
            {
                resetEdgeSelection();
                resetCurveColors();
            }
            appState.mergeMode = NONE;
            break;
        }
        case RANDOM:
        {
            if (appState.numOfCandidateMerges <= 0)
            {
                appState.mergeMode = NONE;
                break;
            }

            if (appState.selectedEdges.empty() && firstRandomIteration)
            {
                appState.selectedEdges = generateRandomNums(appState.numOfCandidateMerges - 1);
                appState.attemptedMergesIdx = 0;
                firstRandomIteration = false;
            }

            if (appState.selectedEdgeId == -1)
            {
                appState.selectedEdgeId = appState.selectedEdges[appState.attemptedMergesIdx++];
            }

            MergeStatus status = merger.mergeAtSelectedEdge();
            appState.mergeStatus = status;
            switch (status)
            {
            case SUCCESS:
            {
                firstRandomIteration = true;
                appState.selectedEdges.clear();
                appState.selectedEdgeId = -1;
                prevSelectedEdgeId = -1;
                break;
            }
            case CYCLE:
            case METRIC_ERROR:
            {
                if (appState.attemptedMergesIdx == appState.selectedEdges.size())
                {
                    appState.mergeMode = NONE;
                    appState.selectedEdgeId = -1;
                    prevSelectedEdgeId = -1;
                }
                else
                {
                    appState.selectedEdgeId = appState.selectedEdges[appState.attemptedMergesIdx++];
                }
                break;
            }
            case FAILURE:
            {
                appState.selectedEdgeId = -1;
                prevSelectedEdgeId = -1;
                appState.mergeMode = NONE;
                appState.selectedEdges.clear();
                break;
            }
            }
            resetCurveColors();
            lastTime = currentTime;

            break;
        }
        }

        switch (appState.currentMode)
        {
        case RENDER_CURVES:
            // curveRenderer.render();
            break;
        case RENDER_PATCHES:
            patchRenderer.render(appState.glPatches, appState.patches, appState.tangentHandles);
            break;
        }

        gui.render();
        gmsWindow.swapBuffers();
    }
}

void GmsApp::resetEdgeSelection()
{
    if (appState.numOfCandidateMerges > 0)
    {
        appState.selectedEdgeId = 0;
        prevSelectedEdgeId = 0;
    }
    else
    {
        appState.selectedEdgeId = -1;
        prevSelectedEdgeId = -1;
    }
}

void GmsApp::resetCurveColors()
{
    if (prevSelectedEdgeId != -1)
        setCurveColor(prevSelectedEdgeId, black);
    if (appState.selectedEdgeId != -1)
        setCurveColor(appState.selectedEdgeId, blue);
    prevSelectedEdgeId = appState.selectedEdgeId;
}

void GmsApp::setCurveColor(int edgeIdx, glm::vec3 color)
{
    DoubleHalfEdge dhe = merger.getDoubleHalfEdge(edgeIdx);
    appState.patches[dhe.curveId1.patchId].setCurveSelected(dhe.curveId1.curveId, color);
    appState.patches[dhe.curveId2.patchId].setCurveSelected(dhe.curveId2.curveId, color);
}
