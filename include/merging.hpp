#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "fileio.hpp"
#include "gms_appstate.hpp"
#include "gms_math.hpp"
#include "merge_metrics.hpp"
#include "merge_select.hpp"
#include "patch.hpp"
#include "types.hpp"
#include "window.hpp"

class GradMesh;

// Primary merge class with functions to modify the mesh for a merge
class GradMeshMerger
{
public:
    GradMeshMerger(GmsAppState &appState);
    void run();
    void startupMesh()
    {
        mesh.findULPoints();
        select.reset();
        select.findCandidateMerges();
        metrics.setAABB();
        metrics.captureGlobalImage(appState.patchRenderParams.glPatches, ORIG_IMG);
    }

private:
    void preprocessEdges();
    void merge();
    MergeStatus mergeAtSelectedEdge(int halfEdgeIdx);

    GmsAppState::MergeStats mergePatches(int halfEdgeIdx);
    float splittingFactor(HalfEdge &stem, HalfEdge &bar1, HalfEdge &bar2, int sign) const;
    float addTJunction(HalfEdge &edge1, HalfEdge &edge2, int twinOfParentIdx, float t);

    void leftTUpdateInterval(int parentIdx, float totalCurve);
    void rightTUpdateInterval(int parentIdx, float reparam1, float reparam2);
    void scaleDownChildrenByT(HalfEdge &parentEdge, float t);
    void scaleUpChildrenByT(HalfEdge &parentEdge, float t);

    void setChildrenNewParent(HalfEdge &parentEdge, int newParentIdx);
    void setParentChildrenTwin(HalfEdge &parentEdge, int newTwinIdx);
    void childBecomesItsParent(int childIdx);
    void setBarChildrensTwin(HalfEdge &parentEdge, int twinIdx);
    void setNextRightL(const HalfEdge &bar, int nextIdx);

    void transferChildTo(int oldChildIdx, int newChildIdx);
    void transferChildToWithoutGeometry(int oldChildIdx, int newChildIdx);
    void fixAndSetTwin(int barIdx);

    void copyEdgeTwin(int e1Idx, int e2Idx);
    void removeFace(int faceIdx);

    GradMesh &mesh;
    GmsAppState &appState;
    MergeMetrics metrics;
    MergeSelect select;
    bool firstIteration = true;
};

inline int getCornerTJunctions(bool topLeftL, bool topLeftT, bool topRightL, bool topRightT, bool isStem)
{
    if (isStem)
        return IsStem;

    int flags = None;
    if (topLeftL)
        flags |= LeftL;
    if (topLeftT)
        flags |= LeftT;
    if (topRightL)
        flags |= RightL;
    if (topRightT)
        flags |= RightT;
    return flags;
};