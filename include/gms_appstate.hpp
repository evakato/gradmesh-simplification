#pragma once

#include <string>
#include <vector>

#include "fileio.hpp"
#include "gradmesh.hpp"
#include "merge_metrics.hpp"
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

enum MergeStatus
{
    NA,
    FAILURE,
    SUCCESS,
    METRIC_ERROR,
    CYCLE
};

enum CornerTJunctions
{
    None = 0,
    RightT = 1 << 0, // 0001
    LeftT = 1 << 1,  // 0010
    RightL = 1 << 2, // 0100
    LeftL = 1 << 3,  // 1000
    IsStem = 1 << 4
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
        int topEdgeCase = 0;
        int bottomEdgeCase = 0;
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
    int attemptedMergesIdx = 0;
    std::vector<int> edgeIds; // for reading a list of edges from file
    MergeMetrics::MetricMode metricMode = MergeMetrics::MetricMode::FLIP;

    // Merging gui stats
    MergeStats mergeStats;

    // Merging history
    MergeStatus mergeStatus = NA;
    bool saveMerges = false;
    std::vector<int> merges{};

    // Merging metrics - capturing pixels and error
    bool useError = true;
    float errorThreshold = ERROR_THRESHOLD;
    GLuint unmergedTexture;
    GLuint mergedTexture;
    AABB mergeAABB;

    void updateMeshRender(const std::vector<Patch> &patchData = {}, const std::vector<GLfloat> &glPatchData = {})
    {
        const auto &data = patchData.empty() ? mesh.generatePatches().value() : patchData;
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
        mergeStatus = NA;
    }
    bool noMoreEdgeIds(int idx) const { return edgeIds.empty() || idx == edgeIds.size() - 1; }
};