#include "gms_app.hpp"

GmsApp::GmsApp() : currMesh{readFile(appState.filename)},
                   patches{*(currMesh.generatePatchData())}
{
    initializeOpenGL();
    patchRenderer.bindBuffers();
    candidateMerges = Merging::candidateEdges(currMesh);
    appState.numOfCandidateMerges = candidateMerges.size();
    // filename = "../meshes/recoloring_meshes/local-refinement.hemesh";
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
            std::cout << currMesh;
        }
        if (appState.doMerge)
        {
            if (Merging::merge(currMesh, candidateMerges, appState.selectedEdgeId))
            {
                patches = *(currMesh.generatePatchData());
                appState.numOfCandidateMerges = candidateMerges.size();
                resetEdgeSelection();
                resetCurveColors();
            }
            appState.doMerge = false;
        }
        if (appState.selectedEdgeId != prevSelectedEdgeId)
        {
            resetCurveColors();
        }

        tangentHandles = currMesh.getHandles();

        if (appState.currentMode == RENDER_CURVES)
            ; // curveRenderer.render();
        else if (appState.currentMode == RENDER_PATCHES)
            patchRenderer.render(patches, tangentHandles);

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