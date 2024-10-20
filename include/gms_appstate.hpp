#pragma once

#include <string>
#include <vector>

#include "fileio.hpp"
#include "gradmesh.hpp"
#include "patch.hpp"
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
    RANDOM,
    EDGELIST
};

class GradMesh;

class GmsAppState
{
public:
    struct MergeStats
    {
        int mergedHalfEdgeIdx = -1;
        float t = -1;
        int removedFaceId = -1;
        float topEdgeT = 0;
        float bottomEdgeT = 0;
        std::string topEdgeCase = "";
        std::string bottomEdgeCase = "";
    };

    // User modes
    RenderMode currentMode = {RENDER_PATCHES};
    MergeMode mergeMode = {NONE};

    // Metadata
    std::string filename = "../meshes/global-refinement.hemesh";
    bool filenameChanged = false;

    // Mesh
    GradMesh mesh;
    AABB meshAABB;

    // Mesh rendering data
    std::vector<Patch> patches;
    std::vector<Vertex> tangentHandles;
    std::vector<GLfloat> glPatches;

    // User rendering preferences
    int maxHWTessellation;
    bool isWireframeMode = false;
    bool renderControlPoints = true;
    bool renderHandles = true;
    bool renderCurves = true;
    bool renderPatches = true;
    float handleLineWidth = 2.0f;
    float curveLineWidth = 2.0f;

    // Patch selection data
    int selectedPatchId = -1;
    std::vector<Vertex> currentPatchData = std::vector<Vertex>(16);

    // Merging edge selection
    int numOfCandidateMerges = 0;
    int selectedEdgeId = -1;
    std::vector<int> selectedEdges;
    std::vector<int> edgeIds;

    // Merging gui stats
    MergeStats mergeStats;

    // Merging history
    bool saveMerges = false;
    std::vector<int> merges{};

    // Merging errror
    bool useError = true;
    float errorThreshold = ERROR_THRESHOLD;

    void updateMeshRender(const std::vector<Patch> &patchData = {}, const std::vector<GLfloat> &glPatchData = {})
    {
        const auto &data = patchData.empty() ? mesh.generatePatchData() : patchData;
        patches = data;
        const auto &glData = glPatchData.empty() ? getAllPatchGLData(patches, &Patch::getControlMatrix) : glPatchData;
        glPatches = glData;
        tangentHandles = mesh.getHandleBars();
    }
    void resetMerges()
    {
        merges.clear();
        filenameChanged = false;
        meshAABB = getMeshAABB(patches);
    }
    bool noMoreEdgeIds(int idx) const { return edgeIds.empty() || idx == edgeIds.size() - 1; }
};