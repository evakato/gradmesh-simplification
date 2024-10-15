#include "gms_app.hpp"

GmsApp::GmsApp() : currMesh{readFile(appState.filename)},
                   patches{*(currMesh.generatePatchData())}
{
    initializeOpenGL();
    patchRenderer.bindBuffers();
    candidateMerges = Merging::candidateEdges(currMesh);
    appState.numOfCandidateMerges = candidateMerges.size();
    createDir(LOGS_DIR);
    createDir(IMAGE_DIR);
    appState.mesh = &currMesh; // free this pointer later
}

void GmsApp::run()
{
    while (!gmsWindow.shouldClose())
    {
        glfwPollEvents();

        if (appState.debugMesh)
        {
            std::cout << currMesh;
            appState.debugMesh = false;
        }

        if (appState.filenameChanged)
        {
            currMesh = readFile(appState.filename);
            patches = *(currMesh.generatePatchData());
            appState.filenameChanged = false;
            candidateMerges = Merging::candidateEdges(currMesh);
            appState.numOfCandidateMerges = candidateMerges.size();
            resetEdgeSelection();
            // resetCurveColors();
        }

        switch (appState.mergeMode)
        {
        case MANUAL:
            if (Merging::mergeAtSelectedEdge(currMesh, candidateMerges, appState))
            {
                patches = *(currMesh.generatePatchData());
                appState.numOfCandidateMerges = candidateMerges.size();
                resetEdgeSelection();
                resetCurveColors();
            }
            appState.mergeMode = NONE;
            break;
        case RANDOM:
            static auto lastTime = std::chrono::steady_clock::now();
            auto currentTime = std::chrono::steady_clock::now();

            std::chrono::duration<double> elapsedSeconds = currentTime - lastTime;
            if (candidateMerges.size() <= 0)
                break;
            if (appState.selectedEdgeId == -1)
                appState.selectedEdgeId = getRandomInt(appState.numOfCandidateMerges - 1);

            if (elapsedSeconds.count() >= 0.5f)
            {
                appState.merges.push_back(appState.selectedEdgeId);
                if (Merging::mergeAtSelectedEdge(currMesh, candidateMerges, appState))
                {
                    patches = *(currMesh.generatePatchData());
                    appState.numOfCandidateMerges = candidateMerges.size();
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

        if (appState.selectedEdgeId != prevSelectedEdgeId)
        {
            resetCurveColors();
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
    if (candidateMerges.size() > 0)
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
    Merging::DoubleHalfEdge dhe = candidateMerges[edgeIdx];
    patches[dhe.curveId1.patchId].setCurveSelected(dhe.curveId1.curveId, color);
    patches[dhe.curveId2.patchId].setCurveSelected(dhe.curveId2.curveId, color);
}
