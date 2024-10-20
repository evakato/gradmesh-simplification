#include "gms_app.hpp"

static bool firstRandomIteration = true;

GmsApp::GmsApp()
{
    appState.edgeIds = readEdgeIdsFromFile("../build/logs/mergelist.txt");
    createDir(LOGS_DIR);
    createDir(IMAGE_DIR);
    createDir(SAVES_DIR);
    setupNewMesh();
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
    renderFBO(ORIG_METRIC_IMG, appState.glPatches, patchRenderer.getPatchShaderId(), appState.meshAABB);
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
            // resetCurveColors();
        }

        switch (appState.mergeMode)
        {
        case MANUAL:
        {
            if (merger.mergeAtSelectedEdge())
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
                firstRandomIteration = false;
            }

            if (appState.selectedEdgeId == -1)
            {
                appState.selectedEdgeId = pop(appState.selectedEdges);
            }

            switch (merger.mergeAtSelectedEdge())
            {
            case SUCCESS:
            {
                firstRandomIteration = true;
                appState.selectedEdges.clear();
                appState.selectedEdgeId = -1;
                prevSelectedEdgeId = -1;
                break;
            }
            case METRIC_ERROR:
            {
                if (appState.selectedEdges.empty())
                {
                    appState.mergeMode = NONE;
                    appState.selectedEdgeId = -1;
                    prevSelectedEdgeId = -1;
                }
                else
                {
                    appState.selectedEdgeId = pop(appState.selectedEdges);
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
        case EDGELIST:
        {
            static int edgeListIdx = 0;

            if (appState.numOfCandidateMerges <= 0 || appState.noMoreEdgeIds(edgeListIdx))
            {
                appState.mergeMode = NONE;
                break;
            }

            if (appState.selectedEdgeId == -1)
            {
                appState.selectedEdgeId = appState.edgeIds[edgeListIdx];
                resetCurveColors();
            }

            if (merger.mergeAtSelectedEdge())
            {
                appState.selectedEdgeId = -1;
                ++edgeListIdx;
            }
            else
            {
                appState.mergeMode = NONE;
            }
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
