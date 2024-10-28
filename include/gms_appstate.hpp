#pragma once

#include <string>
#include <vector>

#include "fileio.hpp"
#include "gradmesh.hpp"
#include "merge_metrics.hpp"
#include "merge_select.hpp"
#include "patch.hpp"
#include "patch_renderer.hpp"
#include "types.hpp"

enum RenderMode
{
    RENDER_PATCHES,
    RENDER_CURVES
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

enum MergeSelectMode
{
    NONE = -1,
    MANUAL,
    RANDOM,
    GRID
};

enum MergeStatus
{
    NA,
    SUCCESS,
    METRIC_ERROR,
    CYCLE
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

    GmsAppState()
    {
        // make sure to gen textures after intializing opengl
        glGenTextures(1, &patchRenderResources.unmergedTexture);
        glGenTextures(1, &patchRenderResources.mergedTexture);
    }

    // User modes
    RenderMode currentMode = {RENDER_PATCHES};
    MergeSelectMode mergeMode = {NONE};
    MergeStatus mergeStatus = {NA};

    // Metadata
    std::string filename = "../meshes/global-refinement.hemesh";
    bool filenameChanged = false;
    bool loadSave = false;

    // Mesh and mesh rendering data
    GradMesh mesh;
    std::vector<Patch> patches;
    PatchRenderer::PatchRenderParams patchRenderParams;

    // User rendering preferences
    int maxHWTessellation;
    bool isWireframeMode = false;
    bool renderControlPoints = true;
    bool renderHandles = false;
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
    int prevSelectedEdgeId = -1;
    DoubleHalfEdge selectedDhe;
    DoubleHalfEdge prevSelectedDhe;
    int attemptedMergesIdx = 0;

    // Merging stats
    MergeStats mergeStats;
    int numOfMerges = 0;
    int currentSave = 0;

    // Merging metrics - capturing pixels and error
    bool useError = true;
    MergeMetrics::MergeSettings mergeSettings;
    MergeMetrics::PatchRenderResources patchRenderResources;

    void updateMeshRender(const std::vector<Patch> &patchData = {}, const std::vector<GLfloat> &glPatchData = {})
    {
        patches = patchData.empty() ? mesh.generatePatches().value() : patchData;
        patchRenderParams.glPatches = glPatchData.empty() ? getAllPatchGLData(patches, &Patch::getControlMatrix) : glPatchData;
        patchRenderParams.glCurves = getAllPatchGLData(patches, &Patch::getCurveData);
        patchRenderParams.handles = mesh.getHandleBars();
        patchRenderParams.points = mesh.getControlPoints();
    }
    void setSelectedAndPrev(int val)
    {
        selectedEdgeId = val;
        prevSelectedEdgeId = val;
    }
    void resetMerges()
    {
        numOfMerges = 0;
        loadSave = filenameChanged = false;
        mergeStatus = NA;
        setSelectedAndPrev(-1);
    }
    void resetCurveColors()
    {
        if (selectedEdgeId == prevSelectedEdgeId)
            return;
        if (prevSelectedEdgeId != -1)
        {
            patches[prevSelectedDhe.curveId1.patchId].setCurveSelected(prevSelectedDhe.curveId1.curveId, black);
            patches[prevSelectedDhe.curveId2.patchId].setCurveSelected(prevSelectedDhe.curveId2.curveId, black);
            patchRenderParams.glCurves = getAllPatchGLData(patches, &Patch::getCurveData);
        }
        if (selectedEdgeId != -1)
        {
            patches[selectedDhe.curveId1.patchId].setCurveSelected(selectedDhe.curveId1.curveId, blue);
            patches[selectedDhe.curveId2.patchId].setCurveSelected(selectedDhe.curveId2.curveId, blue);
            patchRenderParams.glCurves = getAllPatchGLData(patches, &Patch::getCurveData);
        }
        prevSelectedEdgeId = selectedEdgeId;
    }
};