#pragma once

#include <algorithm>
#include <utility>
#include <vector>

#include "fileio.hpp"
#include "gms_appstate.hpp"
#include "gms_math.hpp"
#include "merge_metrics.hpp"
#include "patch.hpp"
#include "types.hpp"
#include "window.hpp"

class GradMesh;

class GradMeshMerger
{
public:
    GradMeshMerger(GmsAppState &appState);
    MergeStatus mergeAtSelectedEdge();
    void findCandidateMerges();
    void setDoubleHalfEdge() const
    {
        int selectedIdx = appState.selectedEdgeId;
        int prevIdx = appState.prevSelectedEdgeId;
        if (selectedIdx >= 0 && selectedIdx < candidateMerges.size())
            appState.selectedDhe = candidateMerges[selectedIdx];
        if (prevIdx >= 0 && prevIdx < candidateMerges.size())
            appState.prevSelectedDhe = candidateMerges[prevIdx];
    }

private:
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
    std::vector<DoubleHalfEdge> candidateMerges;
    MergeMetrics metrics;
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