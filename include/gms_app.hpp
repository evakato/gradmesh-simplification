#pragma once

#include <chrono>

#include "curve_renderer.hpp"
#include "fileio.hpp"
#include "gms_appstate.hpp"
#include "gms_math.hpp"
#include "gradmesh.hpp"
#include "gui.hpp"
#include "merging.hpp"
#include "patch.hpp"
#include "patch_renderer.hpp"
#include "renderer.hpp"
#include "types.hpp"
#include "window.hpp"

#include <GLFW/glfw3.h>

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