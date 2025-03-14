#pragma once

#include <string>
#include <vector>

#include "curve_renderer.hpp"
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
    GRID,
    DUAL_GRID
};

enum MergeStatus
{
    NA,
    SUCCESS,
    METRIC_ERROR,
    CYCLE
};
struct ComponentSelectOptions
{
    enum Type
    {
        None,
        Patch,
        Curve
    };
    bool on = true;
    Type type = Type::Patch;
    bool showPatchAABB = false;
    bool showMaxProductRegion = false;

    bool renderPatchAABB() const { return on && type == Type::Patch && showPatchAABB; }
    bool renderMaxProductRegion() const { return on && type == Type::Patch && showMaxProductRegion; }
};
enum class MergeProcess
{
    PreprocessSingleMerge,
    PreprocessProductRegions,
    LoadProductRegionsPreprocessing,
    MergeTPRs,
    MergeGreedyQuadError,
    MergeMotorcycle,
    DebugMergeGreedyQuadError,
    ViewEdgeMap,
    ViewConflictGraphStats,
    PreviewMerge,
    RandomTest,
    GridTest,
    DualGridTest,
    Merging
};

struct ConflictGraphStats
{
    int numOfNodes = 0;
    float avgDegree = 0.0f;
    float avgQuads = 0;
    float avgError = 0.0f;
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
        updateCurveRender();
    }

    // User modes
    RenderMode currentMode = {RENDER_PATCHES};
    MergeSelectMode mergeMode = {NONE};
    MergeProcess mergeProcess = MergeProcess::Merging;
    EdgeErrorDisplay edgeErrorDisplay = EdgeErrorDisplay::Binary;
    float mergeError;
    int preprocessSingleMergeProgress{-1};
    std::optional<std::chrono::high_resolution_clock::time_point> startTime;

    float preprocessProductRegionsProgress{-1.0f};
    std::vector<EdgeRegion> edgeRegions = {};
    int selectedTPRIdx = 0;
    int regionsMerged = 0;
    ConflictGraphStats conflictGraphStats;
    float quadErrorWeight = 0.75;
    std::string loadPreprocessingFilename;
    float oneStepQuadErrorProgress{-1.0f};

    // Metadata
    std::string filename = "../meshes/avocado.hemesh";
    std::string meshname;
    bool filenameChanged = false;
    bool loadSave = false;

    // Mesh and mesh rendering data
    GradMesh mesh;
    std::vector<GLfloat> originalGlPatches;
    std::vector<Patch> patches;
    PatchRenderer::PatchRenderParams patchRenderParams;
    CurveRenderer::CurveRenderParams curveRenderParams{CurveRenderer::Hermite, {}};

    // User rendering preferences
    int maxHWTessellation;
    bool isWireframeMode = false;
    bool renderControlPoints = true;
    bool renderHandles = false;
    bool renderCurves = true;
    bool renderPatches = true;
    bool showCurveAABB = false;
    float handleLineWidth = 2.0f;
    float curveLineWidth = 3.0f;
    ComponentSelectOptions componentSelectOptions;

    // Patch selection data
    bool manualEdgeSelect = false;
    CurveId userSelectedId{-1, -1};

    // Merging edge selection
    std::vector<DoubleHalfEdge> candidateMerges{};
    int selectedEdgeId = -1;
    int numOfMerges = 0;
    int attemptedMergesIdx = 0;

    // Single merge stats
    MergeStats mergeStats;
    int currentSave = 0;

    // Merging metrics - capturing pixels and error
    bool useError = true;
    MergeMetrics::MergeSettings mergeSettings;
    MergeStatus mergeStatus = {NA};
    float mergePreviewError;

    MergeMetrics::PatchRenderResources patchRenderResources;
    std::vector<int> selectedProductRegionIdxs;

    void setSelectedDhe(CurveId selectedCurve)
    {
        for (size_t i = 0; i < candidateMerges.size(); i++)
        {
            auto &dhe = candidateMerges[i];
            if (dhe.matches(selectedCurve))
            {
                selectedEdgeId = i;
                setPatchCurveColor(dhe.curveId1, blue);
                setPatchCurveColor(dhe.curveId2, blue);
                return;
            }
        }
        selectedEdgeId = candidateMerges.size() > 0 ? 0 : -1;
    }
    void resetSelectedDhe()
    {
        auto &dhe = candidateMerges[selectedEdgeId];
        selectedEdgeId = -1;
        setPatchCurveColor(dhe.curveId1, black);
        setPatchCurveColor(dhe.curveId2, black);
    }
    void updateMeshRender(const std::vector<Patch> &patchData = {}, const std::vector<GLfloat> &glPatchData = {})
    {
        patches = patchData.empty() ? mesh.generatePatches().value() : patchData;
        patchRenderParams.glPatches = glPatchData.empty() ? getAllPatchGLData(patches, &Patch::getControlMatrix) : glPatchData;
        patchRenderParams.glCurves = getAllPatchGLData(patches, &Patch::getCurveData);
        patchRenderParams.handles = mesh.getHandleBars();
        patchRenderParams.points = mesh.getControlPoints();
    }
    void updateCurveRender()
    {
        if (curveRenderParams.curveMode == CurveRenderer::Bezier)
        {
            curveRenderParams.curves = getRandomCurves(Curve::CurveType::Bezier);
        }
        else
        {
            curveRenderParams.curves = getRandomCurves(Curve::CurveType::Hermite);
        }
    }
    void resetMerges()
    {
        numOfMerges = 0;
        regionsMerged = 0;
        loadSave = false;
        filenameChanged = false;
        mergeStatus = NA;
        selectedEdgeId = -1;
        meshname = extractFileName(filename);
        loadPreprocessingFilename = std::string{TPR_PREPROCESSING_DIR} + "/" + meshname + ".txt";
    }
    void setPatchCurveColor(CurveId someCurve, glm::vec3 col)
    {
        patches[someCurve.patchId].setCurveSelected(someCurve.curveId, col);
    }
    void setUserCurveColor(glm::vec3 col)
    {
        if (!userSelectedId.isNull())
            patches[userSelectedId.patchId].setCurveSelected(userSelectedId.curveId, col);
    }
    void showAllCandidateEdges(glm::vec3 col)
    {
        for (auto &dhe : candidateMerges)
        {
            auto &c1 = dhe.curveId1;
            auto &c2 = dhe.curveId2;
            patches[c1.patchId].setCurveSelected(c1.curveId, col);
            patches[c2.patchId].setCurveSelected(c2.curveId, col);
        }
        patchRenderParams.glCurves = getAllPatchGLData(patches, &Patch::getCurveData);
    }
    void showProductRegion(std::pair<int, int> gridPair, std::pair<int, int> maxRegion)
    {
        auto edgeIdxs = mesh.getRegionBorderIdxs(gridPair, maxRegion);
        for (int idx : selectedProductRegionIdxs)
        {
            auto curve = getCurveIdFromEdgeIdx(patches, idx);
            patches[curve.patchId].setCurveSelected(curve.curveId, black);
        }
        selectedProductRegionIdxs.clear();
        for (int edgeIdx : edgeIdxs)
        {
            auto curve = getCurveIdFromEdgeIdx(patches, edgeIdx);
            patches[curve.patchId].setCurveSelected(curve.curveId, green);
            selectedProductRegionIdxs.push_back(edgeIdx);
        }
        patchRenderParams.glCurves = getAllPatchGLData(patches, &Patch::getCurveData);
    }
    void updateCurves(std::vector<int> drawLastIdxs)
    {
        std::vector<Patch> newPatches = patches;
        std::partition(newPatches.begin(), newPatches.end(), [&](const Patch &patch)
                       { return std::find(drawLastIdxs.begin(), drawLastIdxs.end(), patch.getFaceIdx()) == drawLastIdxs.end(); });
        patchRenderParams.glCurves = getAllPatchGLData(newPatches, &Patch::getCurveData);
    }
    bool isCompSelect(ComponentSelectOptions::Type type) const
    {
        return componentSelectOptions.type == type;
    }
    void resetUserSelectedCurve()
    {
        setUserCurveColor(black);
        patchRenderParams.glCurves = getAllPatchGLData(patches, &Patch::getCurveData);
    }
    void setUnmergedGlPatches()
    {
        originalGlPatches = patchRenderParams.glPatches;
    }
    bool maxProductRegionsDone()
    {
        return edgeRegions.empty();
    }
    std::optional<EdgeRegion> findSelectedRegion()
    {
        if (userSelectedId.patchId == -1)
            return std::nullopt;

        int faceIdx = patches[userSelectedId.patchId].getFaceIdx();
        auto it = std::ranges::find_if(edgeRegions, [faceIdx](const auto &edgeRegion)
                                       { return edgeRegion.faceIdx == faceIdx; });
        if (it != edgeRegions.end())
            return *it;

        return std::nullopt;
    }
    void setPatchId(int newId)
    {
        userSelectedId.patchId = newId;
        selectedTPRIdx = 0;
    }
};