#include "gms_app.hpp"

GmsApp::GmsApp()
{
    initializeOpenGL();
    patchRenderer.bindBuffers();
    createDir(LOGS_DIR);
    createDir(IMAGE_DIR);
    createDir(SAVES_DIR);
    appState.mesh = &currMesh; // free this pointer later
    setupNewMesh();
}

void GmsApp::setupNewMesh()
{
    currMesh = readHemeshFile(appState.filename);
    patches = *(currMesh.generatePatchData());
    appState.merges.clear();
    appState.filenameChanged = false;
    merger.findCandidateMerges();
    resetEdgeSelection();
    tangentHandles = currMesh.getHandleBars();
    patchRenderer.render(patches, tangentHandles);

    writeHemeshFile("mesh_saves/save_0.hemesh", currMesh);
    renderFBO("img/currMesh.png", patchRenderer.getGlPatches(), patchRenderer.getPatchShaderId(), getMeshAABB(patches));
    appState.meshAABB = getMeshAABB(patches);
}

void GmsApp::run()
{
    while (!gmsWindow.shouldClose())
    {
        static auto lastTime = std::chrono::steady_clock::now();
        auto currentTime = std::chrono::steady_clock::now();
        glfwPollEvents();

        if (appState.debugMesh)
        {
            std::cout << currMesh;
            appState.debugMesh = false;
        }

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
            if (merger.mergeAtSelectedEdge())
            {
                patches = *(currMesh.generatePatchData());
                renderFBO("img/mergedMesh.png", patchRenderer.getGlPatches(), patchRenderer.getPatchShaderId(), getMeshAABB(patches));

                resetEdgeSelection();
                resetCurveColors();
            }
            appState.mergeMode = NONE;
            break;
        }
        case RANDOM:
        {
            std::chrono::duration<double> elapsedSeconds = currentTime - lastTime;
            if (appState.numOfCandidateMerges <= 0)
            {
                appState.mergeMode = NONE;
                break;
            }

            if (appState.selectedEdgeId == -1)
                appState.selectedEdgeId = getRandomInt(currentTime.time_since_epoch().count(), appState.numOfCandidateMerges - 1);

            if (elapsedSeconds.count() >= 0.1f)
            {
                if (merger.mergeAtSelectedEdge())
                {
                    patches = *(currMesh.generatePatchData());
                    appState.selectedEdgeId = -1;
                    prevSelectedEdgeId = -1;
                    resetCurveColors();
                }
                else
                {
                    appState.mergeMode = NONE;
                }
                lastTime = currentTime;
            }
            break;
        }
        case EDGELIST:
        {
            static int edgeListIdx = 0;

            if (appState.numOfCandidateMerges <= 0 || edgeIds.empty() || edgeListIdx == edgeIds.size() - 1)
            {
                appState.mergeMode = NONE;
                break;
            }

            if (appState.selectedEdgeId == -1)
            {
                appState.selectedEdgeId = edgeIds[edgeListIdx];
                resetCurveColors();
            }

            if (merger.mergeAtSelectedEdge())
            {
                patches = *(currMesh.generatePatchData());
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

        tangentHandles = currMesh.getHandleBars();

        switch (appState.currentMode)
        {
        case RENDER_CURVES:
            // curveRenderer.render();
            break;
        case RENDER_PATCHES:
            patchRenderer.render(patches, tangentHandles);
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
    patches[dhe.curveId1.patchId].setCurveSelected(dhe.curveId1.curveId, color);
    patches[dhe.curveId2.patchId].setCurveSelected(dhe.curveId2.curveId, color);
}
