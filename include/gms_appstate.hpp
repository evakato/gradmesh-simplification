#pragma once

#include <string>
#include <vector>

#include "types.hpp"

enum RenderMode
{
    RENDER_PATCHES,
    RENDER_CURVES
};

enum MergeMode
{
    NONE,
    MANUAL,
    RANDOM
};

class GmsAppState
{
public:
    RenderMode currentMode = {RENDER_PATCHES};
    MergeMode mergeMode = {NONE};

    std::string filename = "../meshes/global-refinement.hemesh";
    bool isWireframeMode = false;
    bool renderControlPoints = true;
    bool renderHandles = true;
    bool renderCurves = true;
    bool renderPatches = true;
    float handleLineWidth = 2.0f;
    float curveLineWidth = 2.0f;
    int maxHWTessellation;

    bool filenameChanged = false;

    int selectedPatchId = -1;
    std::vector<Vertex> currentPatchData = std::vector<Vertex>(16);

    int numOfCandidateMerges = 0;
    int selectedEdgeId = -1;

    bool debugMesh = false;

    float t = -1;
    int removedFaceId = -1;
    std::string topEdgeCase = "";
    std::string bottomEdgeCase = "";
    float topEdgeTJunction = 0;
    float bottomEdgeTJunction = 0;
    std::vector<int> merges{};
};