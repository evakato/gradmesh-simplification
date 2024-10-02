#pragma once

#include "curve_renderer.hpp"
#include "fileio.hpp"
#include "gradmesh.hpp"
#include "gui.hpp"
#include "merging.hpp"
#include "patch.hpp"
#include "patch_renderer.hpp"
#include "renderer.hpp"
#include "types.hpp"
#include "window.hpp"

#include <GLFW/glfw3.h>

enum RenderMode
{
    RENDER_PATCHES,
    RENDER_CURVES
};

class GmsAppState
{
public:
    RenderMode currentMode = {RENDER_PATCHES};

    std::string filename = "../meshes/order1.hemesh";
    bool isWireframeMode = false;
    bool renderControlPoints = true;
    bool renderHandles = true;
    bool renderCurves = true;
    bool renderPatches = true;
    float handleLineWidth = 2.0f;
    float curveLineWidth = 2.0f;
    int maxHWTessellation;

    bool filenameChanged = false;
    bool doMerge = false;

    int selectedPatchId = -1;
    std::vector<Vertex> currentPatchData = std::vector<Vertex>(16);

    int numOfCandidateMerges = 0;
    int selectedEdgeId = -1;

    bool debugMesh = false;
};

class GmsApp
{
public:
    GmsApp();
    ~GmsApp() {}
    void run();

private:
    void resetEdgeSelection();
    void resetCurveColors();
    void setCurveColor(int edgeIdx, glm::vec3 color);

    GmsAppState appState{};

    GmsWindow gmsWindow{SCR_WIDTH, SCR_HEIGHT, "gms"};
    GmsGui gui{gmsWindow, appState};
    PatchRenderer patchRenderer{gmsWindow, appState};

    GradMesh currMesh;
    std::vector<Patch> patches;
    std::vector<Vertex> tangentHandles;
    std::vector<Merging::DoubleHalfEdge> candidateMerges;

    int prevSelectedEdgeId = -1;
};